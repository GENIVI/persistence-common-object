 /******************************************************************************
 * Project         Persistency
 * (c) copyright   2014
 * Company         XS Embedded GmbH
 *****************************************************************************/
/* (Keep It) Simple Stupid Database
*
* Written by Adam Ierymenko <adam.ierymenko@zerotier.com>
* Modified by Simon Disch <simon.disch@xse.de>
*
* KISSDB is in the public domain and is distributed with NO WARRANTY.
*
* http://creativecommons.org/publicdomain/zero/1.0/ */

/* Compile with KISSDB_TEST to build as a test program. */

/* Note: big-endian systems will need changes to implement byte swapping
* on hash table file I/O. Or you could just use it as-is if you don't care
* that your database files will be unreadable on little-endian systems. */

#define _FILE_OFFSET_BITS 64
#define TMP_BUFFER_LENGTH 128
#define KISSDB_HEADER_SIZE sizeof(Header_s)
#define __useBackups
//#define __useFileMapping
//#define __writeThrough
#define __checkerror

#include "./kissdb.h"
#include "../crc32.h"
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>

#include "dlt.h"

DLT_DECLARE_CONTEXT(persComLldbDLTCtx);

#ifdef __showTimeMeasurements
inline long long getNsDuration(struct timespec* start, struct timespec* end)
{
   return ((end->tv_sec * SECONDS2NANO) + end->tv_nsec) - ((start->tv_sec * SECONDS2NANO) + start->tv_nsec);
}
#endif

/* djb2 hash function */
static uint64_t KISSDB_hash(const void *b, unsigned long len)
{
   unsigned long i;
   uint64_t hash = 5381;
   for (i = 0; i < len; ++i)
      hash = ((hash << 5) + hash) + (uint64_t) (((const uint8_t *) b)[i]);
   return hash;
}

//returns a name for shared memory objects beginning with a slash followed by "path" (non alphanumeric chars are replaced with '_')  appended with "tailing"
char * kdbGetShmName(const char *tailing, const char * path)
{
   char * result = (char *) malloc(1 +  strlen(path) + strlen(tailing) + 1); //free happens at lifecycle shutdown
   int i =0;
   int x = 1;

   if (result != NULL)
   {
      result[0] = '/';
      for (i = 0; i < strlen(path); i++)
      {
         if (!isalnum(path[i]))
            result[i + 1] = '_';
         else
            result[i + 1] = path[i];
      }
      for (x = 0; x < strlen(tailing); x++)
      {
         result[i + x + 1] = tailing[x];
      }
      result[i + x + 1] = '\0';
   }
   return result;
}

//returns -1 on error and positive value for success
int kdbShmemOpen(const char * name, size_t length, Kdb_bool* shmCreator)
{
   int result;
   result = shm_open(name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
   if (result < 0)
   {
      if (errno == EEXIST)
      {
         *shmCreator = Kdb_false;
         result = shm_open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
         if (result < 0)
            return -1;
      }
   }
   else
   {
      *shmCreator = Kdb_true;
      if (ftruncate(result, length) < 0)
         return -1;
   }
   return result;
}

void Kdb_wrlock(pthread_rwlock_t * wrlock)
{
   pthread_rwlock_wrlock(wrlock);
}

void Kdb_rdlock(pthread_rwlock_t * rdlock)
{
   pthread_rwlock_rdlock(rdlock);
}

void Kdb_unlock(pthread_rwlock_t * lock)
{
   pthread_rwlock_unlock(lock);
}

Kdb_bool kdbShmemClose(int shmem, const char * shmName)
{
   if( close(shmem) == -1)
      return Kdb_false;
   if( shm_unlink(shmName) < 0)
      return Kdb_false;
   return Kdb_true;
}

void * getKdbShmemPtr(int shmem, size_t length)
{
   void* result = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, shmem, 0);
   if (result == MAP_FAILED)
      return ((void *) -1);
   return result;
}

Kdb_bool freeKdbShmemPtr(void * shmem_ptr, size_t length)
{
   if(munmap(shmem_ptr, length) == 0)
      return Kdb_true;
   else
      return Kdb_false;
}

Kdb_bool resizeKdbShmem(int shmem, Hashtable_slot_s** shmem_ptr, size_t oldLength, size_t newLength)
{
   //unmap shm with old size
   if( freeKdbShmemPtr(*shmem_ptr, oldLength) == Kdb_false)
      return Kdb_false;

   if (ftruncate(shmem, newLength) < 0)
      return Kdb_false;

   //get pointer to resized shm with new Length
   *shmem_ptr = getKdbShmemPtr(shmem, newLength);
   if(*shmem_ptr == ((void *) -1))
      return Kdb_false;
   return Kdb_true;
}

#ifdef __writeThrough
Kdb_bool remapKdbShmem(int shmem, uint64_t** shmem_ptr, size_t oldLength, size_t newLength)
{
   //unmap shm with old size
   if( freeKdbShmemPtr(*shmem_ptr, oldLength) == Kdb_false )
      return Kdb_false;
   //get pointer to resized shm with new Length
   *shmem_ptr = getKdbShmemPtr(shmem, newLength);
   if(*shmem_ptr == ((void *) -1))
      return Kdb_false;
   return Kdb_true;
}
#endif


int KISSDB_open(KISSDB *db, const char *path, int mode, uint16_t hash_table_size, uint64_t key_size,
      uint64_t value_size)
{
   Hashtable_slot_s *httmp;
   Kdb_bool tmp_creator;
   int ret = 0;

   //TODO check if usage of O_SYNC O_DIRECT flags is needed. If O_SYNC and O_DIrect is specified, no additional fsync calls are needed after fflush
   if(mode == KISSDB_OPEN_MODE_RWCREAT)
      db->fd = open(path, O_CREAT | O_RDWR , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH  ); //gets closed when db->f is closed
   else
      db->fd = open(path, O_RDWR , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH  ); //gets closed when db->f is closed

   if(db->fd == -1)
      return KISSDB_ERROR_IO;

   if (lseek(db->fd, 0, SEEK_END) == -1)
   {
      close(db->fd);
      return KISSDB_ERROR_IO;
   }
   if (lseek(db->fd, 0, SEEK_CUR) < KISSDB_HEADER_SIZE)
   {
      /* write header if not already present */
      if ((hash_table_size) && (key_size) && (value_size))
      {
         ret = writeHeader(db, &hash_table_size, &key_size, &value_size);
         if(0 != ret)
         {
            close(db->fd);
            return ret;
         }
         //Seek behind header
         if (lseek(db->fd, KISSDB_HEADER_SIZE, SEEK_SET) == -1)
         {
            close(db->fd);
            return KISSDB_ERROR_IO;
         }
      }
      else
      {
         close(db->fd);
         return KISSDB_ERROR_INVALID_PARAMETERS;
      }
   }
   else
   {
      //read existing header
      ret = readHeader(db, &hash_table_size, &key_size, &value_size);
      if( 0 != ret)
         return ret;

      if (lseek(db->fd, KISSDB_HEADER_SIZE, SEEK_SET) == -1)
      {
         close(db->fd);
         return KISSDB_ERROR_IO;
      } //Seek behind header
   }
   //store non shared db information
   db->hash_table_size = hash_table_size;
   db->key_size = key_size;
   db->value_size = value_size;
   db->hash_table_size_bytes = sizeof(Hashtable_slot_s) * (hash_table_size + 1); /* [hash_table_size] == next table */

   //DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("Hashtable size in bytes: "), DLT_UINT64(db->hash_table_size_bytes));

   if (db->already_open == Kdb_false) //check if this instance has already opened the db before
   {
      db->shmem_cached_name = kdbGetShmName("-cache", path);
      if(db->shmem_cached_name == NULL)
         return KISSDB_ERROR_MALLOC;
      db->shmem_info_name = kdbGetShmName("-shm-info", path);
      if(db->shmem_info_name == NULL)
         return KISSDB_ERROR_MALLOC;
      db->shmem_info_fd = kdbShmemOpen(db->shmem_info_name, sizeof(Shared_Data_s), &db->shmem_creator);
      if(db->shmem_info_fd < 0)
         return KISSDB_ERROR_OPEN_SHM;
      db->shmem_info = (Shared_Data_s *) getKdbShmemPtr(db->shmem_info_fd, sizeof(Shared_Data_s));
      if(db->shmem_info == ((void *) -1))
         return KISSDB_ERROR_MAP_SHM;

      size_t first_mapping;
      if(db->shmem_info->shmem_size > db->hash_table_size_bytes )
         first_mapping = db->shmem_info->shmem_size;
      else
         first_mapping = db->hash_table_size_bytes;

      //open / create shared memory for first hashtable
      db->shmem_ht_name = kdbGetShmName("-ht", path);
      if(db->shmem_ht_name == NULL)
         return KISSDB_ERROR_MALLOC;
      db->shmem_ht_fd = kdbShmemOpen(db->shmem_ht_name,  first_mapping, &tmp_creator);
      if(db->shmem_ht_fd < 0)
         return KISSDB_ERROR_OPEN_SHM;
      db->hash_tables = (Hashtable_slot_s *) getKdbShmemPtr(db->shmem_ht_fd, first_mapping);
      if(db->hash_tables == ((void *) -1))
         return KISSDB_ERROR_MAP_SHM;
      db->old_mapped_size = first_mapping; //local information

      //if shared memory for rwlock was opened (created) with this call to KISSDB_open for the first time -> init rwlock
      if (db->shmem_creator == Kdb_true)
      {
         //[Initialize rwlock attributes]
         pthread_rwlockattr_t rwlattr, cache_rwlattr;
         pthread_rwlockattr_init(&rwlattr);
         pthread_rwlockattr_init(&cache_rwlattr);
         pthread_rwlockattr_setpshared(&rwlattr, PTHREAD_PROCESS_SHARED);
         pthread_rwlockattr_setpshared(&cache_rwlattr, PTHREAD_PROCESS_SHARED);
         pthread_rwlock_init(&db->shmem_info->rwlock, &rwlattr);
         pthread_rwlock_init(&db->shmem_info->cache_rwlock, &cache_rwlattr);
         Kdb_wrlock(&db->shmem_info->rwlock);

#ifdef __checkerror
         //CHECK POWERLOSS FLAGS
         ret = checkErrorFlags(db);
         if (0 != ret)
         {
            close(db->fd);
            Kdb_unlock(&db->shmem_info->rwlock);
            return ret;
         }
#endif
         db->shmem_info->num_hash_tables = 0;
      }
      else // already initialized
         Kdb_wrlock(&db->shmem_info->rwlock);

      db->already_open = Kdb_true;
   }
   else
      Kdb_wrlock(&db->shmem_info->rwlock);

   //only read header from file into memory for first caller of KISSDB_open
   if (db->shmem_creator == Kdb_true)
   {
      httmp = (Hashtable_slot_s*) malloc(db->hash_table_size_bytes);   //read hashtable from file
      if (!httmp)
      {
         close(db->fd);
         Kdb_unlock(&db->shmem_info->rwlock);
         return KISSDB_ERROR_MALLOC;
      }
      while (read(db->fd, httmp, db->hash_table_size_bytes) == db->hash_table_size_bytes)
      {
         Kdb_bool result = Kdb_false;
         //if new size would exceed old shared memory size-> allocate additional memory page to shared memory
         if (db->hash_table_size_bytes * (db->shmem_info->num_hash_tables + 1) > db->old_mapped_size)
         {
            Kdb_bool temp;
            if (db->shmem_ht_fd <= 0)
            {
               db->shmem_ht_fd = kdbShmemOpen(db->shmem_ht_name,  db->old_mapped_size, &temp);
               if(db->shmem_ht_fd < 0)
               {
                  free(httmp);
                  Kdb_unlock(&db->shmem_info->rwlock);
                  return KISSDB_ERROR_OPEN_SHM;
               }
            }
            result = resizeKdbShmem(db->shmem_ht_fd, &db->hash_tables, db->old_mapped_size, db->old_mapped_size + db->hash_table_size_bytes);
            if (result == Kdb_false)
            {
               free(httmp);
               Kdb_unlock(&db->shmem_info->rwlock);
               return KISSDB_ERROR_RESIZE_SHM;
            }
            else
            {
               db->shmem_info->shmem_size = db->old_mapped_size + db->hash_table_size_bytes;
               db->old_mapped_size = db->old_mapped_size + db->hash_table_size_bytes;
            }
         }
         // copy the current hashtable read from file to (htadress + (htsize  * htcount)) in memory
         memcpy(((uint8_t *) db->hash_tables) + (db->hash_table_size_bytes * db->shmem_info->num_hash_tables), httmp, db->hash_table_size_bytes);
         ++db->shmem_info->num_hash_tables;

         //read until all hash tables have been read
         if (httmp[db->hash_table_size].offsetA) //if httable[hash_table_size] contains a offset to a further hashtable
         {
            //ONE MORE HASHTABLE FOUND
            if (lseek(db->fd, httmp[db->hash_table_size].offsetA, SEEK_SET) == -1)
            { //move the filepointer to the next hashtable in the file
               KISSDB_close(db);
               free(httmp);
               Kdb_unlock(&db->shmem_info->rwlock);
               return KISSDB_ERROR_IO;
            }
         }
         else
            break; // no further hashtables exist
      }
      free(httmp);
   }

   //printSharedHashtable(db);

   Kdb_unlock(&db->shmem_info->rwlock);
   return 0;
}





int KISSDB_close(KISSDB *db)
{
   Kdb_wrlock(&db->shmem_info->rwlock);

   uint64_t crc = 0;
   Header_s* ptr = 0;
#ifdef __showTimeMeasurements
   long long KdbDuration = 0;
   struct timespec mmapStart, mmapEnd;
   KdbDuration = 0;
#endif

   //printSharedHashtable(db);
   if (db->shmem_creator == Kdb_true)
   {
      //free shared hashtable
      if( freeKdbShmemPtr(db->hash_tables, db->old_mapped_size) == Kdb_false)
      {
         Kdb_unlock(&db->shmem_info->rwlock);
         return KISSDB_ERROR_UNMAP_SHM;
      }
      if( kdbShmemClose(db->shmem_ht_fd, db->shmem_ht_name) == Kdb_false)
         return KISSDB_ERROR_CLOSE_SHM;

      free(db->shmem_ht_name);
      Kdb_unlock(&db->shmem_info->rwlock);
      pthread_rwlock_destroy(&db->shmem_info->rwlock);
      pthread_rwlock_destroy(&db->shmem_info->cache_rwlock);

      // free shared information
      if (freeKdbShmemPtr(db->shmem_info, sizeof(Kdb_bool)) == Kdb_false)
         return KISSDB_ERROR_UNMAP_SHM;
      if (kdbShmemClose(db->shmem_info_fd, db->shmem_info_name) == Kdb_false)
         return KISSDB_ERROR_CLOSE_SHM;
      free(db->shmem_info_name);

#ifdef __showTimeMeasurements
      clock_gettime(CLOCK_ID, &mmapStart);
#endif

      //update header (checksum and flags)
      int mapFlag = PROT_WRITE | PROT_READ;
      ptr = (Header_s*) mmap(NULL,KISSDB_HEADER_SIZE, mapFlag, MAP_SHARED, db->fd, 0);
      if (ptr == MAP_FAILED)
      {
         close(db->fd);
         return KISSDB_ERROR_IO;
      }
#ifdef __checkerror
      // generate checksum over database file (beginning at file offset [sizeof(ptr->KdbV) + sizeof(ptr->checksum)] up to EOF)
      if( db->fd )
      {
         crc = 0;
         crc = (uint64_t) pcoCalcCrc32Csum(db->fd, sizeof(Header_s) );
         ptr->checksum =  crc;
         //printf("CLOSING ------ DB: %s, WITH CHECKSUM CALCULATED: %" PRIu64 "  \n", db->shmem_ht_name, ptr->checksum);
      }
#endif
      ptr->closeFailed = 0x00; //remove closeFailed flag
      ptr->closeOk = 0x01;     //set closeOk flag

      //sync changes with file
      if( 0 != msync(ptr, KISSDB_HEADER_SIZE, MS_SYNC  | MS_INVALIDATE))
      {
         close(db->fd);
         return KISSDB_ERROR_IO;
      }
      //unmap memory
      if( 0 != munmap(ptr, KISSDB_HEADER_SIZE))
      {
         close(db->fd);
         return KISSDB_ERROR_IO;
      }
#ifdef __showTimeMeasurements
      clock_gettime(CLOCK_ID, &mmapEnd);
      KdbDuration += getNsDuration(&mmapStart, &mmapEnd);
      printf("mmap duration for => %f ms\n", (double)((double)KdbDuration/NANO2MIL));
#endif
      fsync(db->fd);

      if( db->fd)
         close(db->fd);

      db->already_open = Kdb_false;
      //memset(db, 0, sizeof(KISSDB)); //todo check if necessary
   }
   else
      //if caller is not the creator of the lock
      Kdb_unlock(&db->shmem_info->rwlock);
   return 0;
}


int KISSDB_get(KISSDB *db, const void *key, void *vbuf)
{
   Kdb_rdlock(&db->shmem_info->rwlock);

   uint8_t tmp[TMP_BUFFER_LENGTH];
   uint64_t current;
   const uint8_t *kptr;
   unsigned long klen, i;
   long n = 0;
   uint64_t checksum, backupChecksum, crc;
   uint64_t hash = KISSDB_hash(key, db->key_size) % (uint64_t) db->hash_table_size;
   int64_t offset, backupOffset, htoffset, checksumOffset, flagOffset; //lasthtoffset
   Hashtable_slot_s *cur_hash_table;

#ifdef __writeThrough
   //if new one or more hashtables were appended, remap shared memory block to adress space
   if (db->old_mapped_size < db->shmem_info->shmem_size)
   {
      Kdb_bool temp;
      db->shmem_ht_fd = kdbShmemOpen(db->shmem_ht_name, db->old_mapped_size, &temp);
      if(db->shmem_ht_fd < 0)
         return KISSDB_ERROR_OPEN_SHM;
      res = remapKdbShmem(db->shmem_ht_fd, &db->hash_tables, db->old_mapped_size, db->shmem_info->shmem_size);
      if (res == Kdb_false)
         return KISSDB_ERROR_REMAP_SHM;
      db->old_mapped_size = db->shmem_info->shmem_size;
   }
#endif

   htoffset = KISSDB_HEADER_SIZE; //lasthtoffset
   cur_hash_table = db->hash_tables;//pointer to current hashtable in memory
   for (i = 0; i < db->shmem_info->num_hash_tables; ++i)
   {
      offset = cur_hash_table[hash].offsetA;//get fileoffset where the data can be found in the file
#ifdef __useBackups
      //get information about current valid offset to latest written data
      if(cur_hash_table[hash].current == 0x00) //valid is offsetA
      {
         offset  = cur_hash_table[hash].offsetA;
         checksum = cur_hash_table[hash].checksumA;
      }
      else
      {
         offset = cur_hash_table[hash].offsetB;
         checksum = cur_hash_table[hash].checksumB;
      }
#endif

      if (offset >= KISSDB_HEADER_SIZE)       //if a valid offset is available in the slot
      {
         if (lseek(db->fd, offset, SEEK_SET) == -1) //move filepointer to this offset
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }
         kptr = (const uint8_t *) key;
         klen = db->key_size;
         while (klen)
         {
            n = (long) read(db->fd, tmp, (klen > sizeof(tmp)) ? sizeof(tmp) : klen);
            if (n > 0)
            {
               if (memcmp(kptr, tmp, n))//if key does not match -> search in next hashtable
                  goto get_no_match_next_hash_table;
               kptr += n;
               klen -= (unsigned long) n;
            }
            else
            {
               Kdb_unlock(&db->shmem_info->rwlock);
               return 1; /* not found */
            }
         }
         if (read(db->fd, vbuf, db->value_size) == db->value_size) //if key matches at the fileoffset -> read the value
         {
            //crc check for file content
#ifdef __useBackups
            //only validate checksums at read if checksum of file is invalid
            if (db->shmem_info->crc_invalid == Kdb_true)
            {
               //verify checksum of current key/value pair
               crc = 0;
               crc = (uint64_t) pcoCrc32(crc, (unsigned char*) vbuf, db->value_size);
               if (checksum != crc)
               {
                  //printf("KISSDB_get: WARNING: checksum invalid -> try to read from valid data block \n");
                  //try to read valid data from backup
                  Hashtable_slot_s slot = cur_hash_table[hash];
                  if (cur_hash_table[hash].current == 0x00) //current is offsetA, but Data there is corrupt--> so use offsetB as backupOffset
                  {
                     backupOffset = cur_hash_table[hash].offsetB;
                     backupChecksum = cur_hash_table[hash].checksumB;
                     checksumOffset = htoffset + (sizeof(Hashtable_slot_s) * hash + sizeof(slot.offsetA)); //offset that points to checksumA
                     current = 0x01; //current is offsetB
                  }
                  else
                  {
                     backupOffset = cur_hash_table[hash].offsetA;
                     backupChecksum = cur_hash_table[hash].checksumA;
                     checksumOffset = htoffset
                           + (sizeof(Hashtable_slot_s) * hash + sizeof(slot.offsetA) + sizeof(slot.checksumA)
                                 + sizeof(slot.offsetB)); //offset that points to checksumB
                     current = 0x00;
                  }
                  flagOffset = htoffset
                        + (sizeof(Hashtable_slot_s) * hash + (sizeof(Hashtable_slot_s) - sizeof(slot.current))); //offset that points to currentflag

                  //seek to backup data
                  if (lseek(db->fd, backupOffset + db->key_size, SEEK_SET) == -1) //move filepointer to data of key-value pair //TODO make checksum over key AND data ??
                  {
                     Kdb_unlock(&db->shmem_info->rwlock);
                     return KISSDB_ERROR_IO;
                  }

                  //verify checksum of backup key/value pair
                  //read from backup data
                  if (read(db->fd, vbuf, db->value_size) == db->value_size) //read value of backup Data block
                  {
                     //generate checksum of backup
                     crc = 0;
                     crc = (uint64_t) pcoCrc32(crc, (unsigned char*) vbuf, db->value_size);
                     if (crc == backupChecksum)  //if checksum ok
                     {
                        //printf("KISSDB_get: WARNING: OVERWRITING CORRUPT DATA \n");
                        //seek to corrupt data
                        if (lseek(db->fd, offset + db->key_size, SEEK_SET) == -1) //move filepointer to data of corrupt key-value pair
                        {
                           Kdb_unlock(&db->shmem_info->rwlock);
                           return KISSDB_ERROR_IO;
                        }
                        //overwrite corrupt data
                        if (write( db->fd, vbuf, db->value_size) != db->value_size )  //write value
                        {
                           Kdb_unlock(&db->shmem_info->rwlock);
                           return KISSDB_ERROR_IO;
                        }
                        //seek to header slot and update checksum of corrupt data (do not modify offsets)
                        if (lseek(db->fd, checksumOffset, SEEK_SET) == -1) //move to checksumX in file
                        {
                           Kdb_unlock(&db->shmem_info->rwlock);
                           return KISSDB_ERROR_IO;
                        }
                        if (write( db->fd, &crc, sizeof(uint64_t)) != sizeof(uint64_t) )  //write checksumX to hashtbale slot
                        {
                           Kdb_unlock(&db->shmem_info->rwlock);
                           return KISSDB_ERROR_IO;
                        }
                        //update checksumX in memory
                        if (cur_hash_table[hash].current == 0x00) //current is offsetA, but Data there is corrupt--> so update checksumA with new checksum
                           cur_hash_table[hash].checksumA = crc;
                        else
                           cur_hash_table[hash].checksumB = crc;
                        //switch current valid to backup

                        if (lseek(db->fd, flagOffset, SEEK_SET) == -1) //move to current flag in file
                        {
                           Kdb_unlock(&db->shmem_info->rwlock);
                           return KISSDB_ERROR_IO;
                        }
                        if (write( db->fd, &current, sizeof(uint64_t)) != sizeof(uint64_t) )     //write current hashtable slot in file
                        {
                           Kdb_unlock(&db->shmem_info->rwlock);
                           return KISSDB_ERROR_IO;
                        }
                        //update current valid in memory
                        cur_hash_table[hash].current = current;
                        //fsync(db->fd)
                        Kdb_unlock(&db->shmem_info->rwlock);
                        return 0; /* success */
                     }
                     else //if checksum not valid, return NOT FOUND
                     {
                        Kdb_unlock(&db->shmem_info->rwlock);
                        return 1; /* not found */
                     }
                  }
                  else
                  {
                     Kdb_unlock(&db->shmem_info->rwlock);
                     return KISSDB_ERROR_IO;
                  }
               }
            }
#endif
            Kdb_unlock(&db->shmem_info->rwlock);
            return 0; /* success */
         }
         else
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }
      }
      else
      {
         Kdb_unlock(&db->shmem_info->rwlock);
         return 1; /* not found */
      }
      //get_no_match_next_hash_table: cur_hash_table += db->hash_table_size + 1;
      get_no_match_next_hash_table:  //update lastht offset //lasthtoffset = htoffset
      htoffset = cur_hash_table[db->hash_table_size].offsetA; // fileoffset to the next file-hashtable
      cur_hash_table += (db->hash_table_size + 1); //pointer to the next memory-hashtable
   }
   Kdb_unlock(&db->shmem_info->rwlock);
   return 1; /* not found */
}


//TODO check current valid data to be deleted ?
int KISSDB_delete(KISSDB *db, const void *key)
{
   Kdb_wrlock(&db->shmem_info->rwlock);

   uint8_t tmp[TMP_BUFFER_LENGTH];
   //uint64_t current = 0x00;
   const uint8_t *kptr;
   long n;
   unsigned long klen, i;
   //uint64_t crc = 0x00;
   uint64_t hash = KISSDB_hash(key, db->key_size) % (uint64_t) db->hash_table_size;
   //int64_t empty_offset = 0;
   int64_t empty_offsetB = 0;
   int64_t offset = 0;
   int64_t htoffset = 0;
   Hashtable_slot_s *cur_hash_table;

#ifdef __writeThrough
   //if new hashtable was appended, remap shared memory block to adress space
   if (db->old_mapped_size < db->shmem_info->shmem_size)
   {
      Kdb_bool temp;
      db->shmem_ht_fd = kdbShmemOpen(db->shmem_ht_name, db->old_mapped_size, &temp);
      if(db->shmem_ht_fd < 0)
         return KISSDB_ERROR_OPEN_SHM;
      result = remapKdbShmem(db->shmem_ht_fd, &db->hash_tables, db->old_mapped_size, db->shmem_info->shmem_size);
      if (result == Kdb_false)
         return KISSDB_ERROR_REMAP_SHM;
      db->old_mapped_size = db->shmem_info->shmem_size;
   }
#endif

   htoffset = KISSDB_HEADER_SIZE;
   cur_hash_table = db->hash_tables; //pointer to current hashtable in memory

   for (i = 0; i < db->shmem_info->num_hash_tables; ++i)
   {
      offset = cur_hash_table[hash].offsetA; //get fileoffset where the data can be found in the file
      if (offset >= KISSDB_HEADER_SIZE)
      {
         if (lseek(db->fd, offset, SEEK_SET) == -1)
         {
            //set filepointer to Key value offset in file
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }
         kptr = (const uint8_t *) key;
         klen = db->key_size;
         while (klen)
         {
            n = (long) read(db->fd, tmp, (klen > sizeof(tmp)) ? sizeof(tmp) : klen);
            if (n > 0)
            {
               if (memcmp(kptr, tmp, n))//if key does not match, search in next hashtable
                  goto get_no_match_next_hash_table;
               kptr += n;
               klen -= (unsigned long) n;
            }
            else
            {
               Kdb_unlock(&db->shmem_info->rwlock);
               return 1; /* not found */
            }
         }
         //TODO: mmap Hashtable slot structure to avoid seeking -> align hashtables at a multiple of a pagesize
#ifdef __useFileMapping
         empty_offsetB = -(offset + (db->key_size + db->value_size)); //todo check if offset is rewritten in put function !
         cur_hash_table[hash].offsetB = empty_offsetB;
         cur_hash_table[hash].checksumA = 0x00;
         cur_hash_table[hash].checksumB = 0x00;
         cur_hash_table[hash].current =   0x00;
         int testoffset= lseek(fd, 0, SEEK_CUR); //filepointer position
         int myoffset = htoffset + (sizeof(Hashtable_slot_s) * hash);

         printf("Endoffset in file: %d , Offset for mmap: %d , size for mmap: %d \n", testoffset, myoffset, sizeof(Hashtable_slot_s));

         //mmap the current hashtable slot
         int mapFlag = PROT_WRITE | PROT_READ;
         printf("In Delete: filedes: %d\n", db->fd);
         htSlot = (Hashtable_slot_s*) mmap(NULL, sizeof(Hashtable_slot_s), mapFlag, MAP_SHARED, db->fd, htoffset + (sizeof(Hashtable_slot_s) * hash) ); //TODO offset must be a multiple of pagesize
         if (htSlot == MAP_FAILED)
         {
            printf("MMAP ERROR !\n");
            close(db->fd);
            return KISSDB_ERROR_IO;
         }
         //do changes to slot in file
         htSlot->offsetA = empty_offset;
         htSlot->checksumA = 0x00;
         htSlot->offsetB = empty_offsetB;
         htSlot->checksumB = 0x00;
         htSlot->current = 0x00;

         //sync changes with file
         if (0 != msync(htSlot,  sizeof(Hashtable_slot_s), MS_SYNC | MS_INVALIDATE))
         {
            close(db->fd);
            return KISSDB_ERROR_IO;
         }
         //unmap memory
         if (0 != munmap(htSlot,  sizeof(Hashtable_slot_s)))
         {
            close(db->fd);
            return KISSDB_ERROR_IO;
         }
#endif
         if (lseek(db->fd, htoffset + (sizeof(Hashtable_slot_s) * hash), SEEK_SET) == -1) //move Filepointer to used slot in file-hashtable.
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }

#ifndef __useBackups
         cur_hash_table[hash].offsetA = -offset; //negate offset in hashtable that points to the data
         empty_offset = -offset;
         //update hashtable slot in file header (delete existing offset information)
         if (write( db->fd, &empty_offset, sizeof(int64_t)) != sizeof(int64_t) )  //mark slot in file-hashtable as deleted
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }
#endif

#ifdef __useBackups
         //negate offsetB, delete checksums and current flag in memory
         cur_hash_table[hash].offsetA = -offset; //negate offset in hashtable that points to the data
         empty_offsetB = -(offset + (db->key_size + db->value_size));
         cur_hash_table[hash].checksumA = 0x00;
         cur_hash_table[hash].offsetB = empty_offsetB;
         cur_hash_table[hash].checksumB = 0x00;
         cur_hash_table[hash].current =   0x00;
         if (write( db->fd, &cur_hash_table[hash], sizeof(Hashtable_slot_s)) != sizeof(Hashtable_slot_s) )  //write updated data in the file-hashtable slot
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }
#endif
         //TODO currently, no synchronus Filedescriptor is used!!!! fsync after fflush is needed to do synchronus writes
         //fsync(db->fd) // associating a file stream with a synchronous file descriptor means that an fsync() call is not needed on the file descriptor after the fflush()
         Kdb_unlock(&db->shmem_info->rwlock);
         return 0; /* success */
      }
      else
      {
         Kdb_unlock(&db->shmem_info->rwlock);
         return 1; /* not found */ //if no offset is found at hashed position in ht
      }
      get_no_match_next_hash_table: htoffset = cur_hash_table[db->hash_table_size].offsetA; // fileoffset to next ht in file
      cur_hash_table += (db->hash_table_size + 1); //pointer to next hashtable in memory
   }
   Kdb_unlock(&db->shmem_info->rwlock);
   return 1; /* not found */
}

int KISSDB_put(KISSDB *db, const void *key, const void *value)
{
   Kdb_wrlock(&db->shmem_info->rwlock);

   uint8_t tmp[TMP_BUFFER_LENGTH];
   uint64_t current = 0x00;
   const uint8_t *kptr;
   unsigned long klen, i;
   uint64_t hash = KISSDB_hash(key, db->key_size) % (uint64_t) db->hash_table_size;
   int64_t offset, endoffset, htoffset, lasthtoffset;
   Hashtable_slot_s *cur_hash_table;
   Kdb_bool result = Kdb_false;
   Kdb_bool temp = Kdb_false;
   uint64_t crc = 0x00;
   long n;
   char delimiter[8] = "||||||||";

#ifdef __writeThrough
   //if new hashtable was appended, remap shared memory block to adress space
   if(db->old_mapped_size < db->shmem_info->shmem_size)
   {
      db->shmem_ht_fd = kdbShmemOpen(db->shmem_ht_name,  db->old_mapped_size, &temp);
      if(db->shmem_ht_fd < 0)
         return KISSDB_ERROR_OPEN_SHM;
      res = remapKdbShmem(db->shmem_ht_fd, &db->hash_tables, db->old_mapped_size,db->shmem_info->shmem_size);
      if (res == Kdb_false)
         return KISSDB_ERROR_REMAP_SHM;
      db->old_mapped_size = db->shmem_info->shmem_size;
   }
#endif
   lasthtoffset = htoffset = KISSDB_HEADER_SIZE;
   cur_hash_table = db->hash_tables; //pointer to current hashtable in memory

   for (i = 0; i < db->shmem_info->num_hash_tables; ++i)
   {
      offset = cur_hash_table[hash].offsetA;   //fileoffset to data in file
      if (offset >= KISSDB_HEADER_SIZE || offset < 0) //if a key with same hash is already in this slot or the same key must be overwritten
      {
         // if slot is marked as deleted, use this slot and negate the offset in order to reuse the existing data block
         if(offset < 0)
         {
            offset = -offset; //get original offset where data was deleted
            //printf("Overwriting slot for key: [%s] which was deleted before, offsetA: %d \n",key, offset);
            if (lseek(db->fd, offset, SEEK_SET) == -1) //move filepointer to fileoffset where the key can be found
            {
               Kdb_unlock(&db->shmem_info->rwlock);
               return KISSDB_ERROR_IO;
            }
            if (write( db->fd, key, db->key_size) != db->key_size )  //write key
            {
               Kdb_unlock(&db->shmem_info->rwlock);
               return KISSDB_ERROR_IO;
            }
            if (write( db->fd, value, db->value_size) != db->value_size )  //write value
            {
               Kdb_unlock(&db->shmem_info->rwlock);
               return KISSDB_ERROR_IO;
            }

            // write same key and value again here because slot was deleted an can be reused like an initial write
#ifdef __useBackups
            if (write( db->fd, key, db->key_size) != db->key_size )  //write key
            {
               Kdb_unlock(&db->shmem_info->rwlock);
               return KISSDB_ERROR_IO;
            }
            if (write( db->fd, value, db->value_size) != db->value_size )  //write value
            {
               Kdb_unlock(&db->shmem_info->rwlock);
               return KISSDB_ERROR_IO;
            }
#endif
            //seek back to hashtbale slot
            if (lseek(db->fd, htoffset + (sizeof(Hashtable_slot_s) * hash), SEEK_SET) == -1) //move  to beginning of hashtable slot in file
            {
               Kdb_unlock(&db->shmem_info->rwlock);
               return KISSDB_ERROR_IO;
            }

#ifndef __useBackups
            cur_hash_table[hash].offsetA = offset; //write the offset to the data in the memory-hashtable slot
            if (write( db->fd, &offset, sizeof(int64_t)) != sizeof(int64_t) )  //write the offsetA to the data in the file-hashtable slot
            {
               Kdb_unlock(&db->shmem_info->rwlock);
               return KISSDB_ERROR_IO;
            }
#endif

#ifdef __useBackups
            crc = 0x00;
            crc = (uint32_t) pcoCrc32(crc, (unsigned char*)value, db->value_size);
            cur_hash_table[hash].offsetA = offset; //write the offset to the data in the memory-hashtable slot
            cur_hash_table[hash].checksumA = crc;
            offset += (db->key_size + db->value_size);
            cur_hash_table[hash].offsetB = offset;          //write the offset to the data in the memory-hashtable slot
            cur_hash_table[hash].checksumB = crc;
            cur_hash_table[hash].current = 0x00;

            if (write( db->fd, &cur_hash_table[hash], sizeof(Hashtable_slot_s)) != sizeof(Hashtable_slot_s) )  //write updated data in the file-hashtable slot
            {
               Kdb_unlock(&db->shmem_info->rwlock);
               return KISSDB_ERROR_IO;
            }
#endif
            //fsync(db->fd) //associating a file stream with a synchronous file descriptor means that an fsync() call is not needed on the file descriptor after the fflush()
            Kdb_unlock(&db->shmem_info->rwlock);
            return 0; /* success */
         }

         //overwrite existing if key matches
         // if cur_hash_table[hash].current == 0x00 -> offsetA is latest so write to offsetB else offsetB is latest and write to offsetA
#ifdef __useBackups
         if( cur_hash_table[hash].current == 0x00 )
            offset = cur_hash_table[hash].offsetA; //0x00 -> offsetA is latest
         else
            offset = cur_hash_table[hash].offsetB; //else offsetB is latest
#endif
         if (lseek(db->fd, offset, SEEK_SET) == -1) //move filepointer to fileoffset where valid data can be found
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }

         kptr = (const uint8_t *) key; //pointer to search key
         klen = db->key_size;
         while (klen)
         {
            n = (long) read(db->fd, tmp, (klen > sizeof(tmp)) ? sizeof(tmp) : klen);
            if (n > 0)
            {
               if (memcmp(kptr, tmp, n)) //if search key does not match with key in file
                  goto put_no_match_next_hash_table;
               kptr += n;
               klen -= (unsigned long) n;
            }
         }

         //if key matches -> seek to currently non valid data block for this key
#ifdef __useBackups
         if( cur_hash_table[hash].current == 0x00 )
            offset = cur_hash_table[hash].offsetB; // 0x00 -> offsetA is latest so write new data to offsetB which holds old data
         else
            offset = cur_hash_table[hash].offsetA; // offsetB is latest so write new data to offsetA which holds old data

         if (lseek(db->fd, offset, SEEK_SET) == -1)//move filepointer to fileoffset where backup data can be found
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }
         if (write( db->fd, key, db->key_size) != db->key_size )  //write key
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }
#endif
         if (write( db->fd, value, db->value_size) != db->value_size )
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }
         // seek back to slot in header for update of checksum and flag
         if (lseek(db->fd, htoffset + (sizeof(Hashtable_slot_s) * hash), SEEK_SET) == -1) //move  to beginning of hashtable slot in file
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }

         //generate crc for value
         crc = 0x00;
         crc = (uint64_t) pcoCrc32(crc, (unsigned char*)value, db->value_size);
         current = 0x00;
         Hashtable_slot_s slot = cur_hash_table[hash];

         // check current flag and decide what parts of hashtable slot in file must be updated
         if( cur_hash_table[hash].current == 0x00 ) //offsetA is latest -> modify settings of B
         {
            int seek = sizeof(slot.offsetA) + sizeof(slot.checksumA) + sizeof(slot.offsetB);
            lseek(db->fd, seek , SEEK_CUR);                //move to checksumB in file
            if( write( db->fd, &crc, sizeof(uint64_t)) != sizeof(uint64_t))      //write checksumB to file
            {
               Kdb_unlock(&db->shmem_info->rwlock);
               return KISSDB_ERROR_IO;
            }
            current = 0x01;
            if( write( db->fd, &current, sizeof(uint64_t)) != sizeof(uint64_t))   //write current  to hashtbale slot
            {
               Kdb_unlock(&db->shmem_info->rwlock);
               return KISSDB_ERROR_IO;
            }
            cur_hash_table[hash].checksumB = crc;
            cur_hash_table[hash].current = current;
         }
         else //offsetB is latest -> modify settings of A
         {

            int seek = sizeof(slot.offsetA);
            lseek(db->fd, seek , SEEK_CUR); //move to checksumA in file
            if( write( db->fd, &crc, sizeof(uint64_t)) != sizeof(uint64_t))   //write checksumA to file
            {
               Kdb_unlock(&db->shmem_info->rwlock);
               return KISSDB_ERROR_IO;
            }
            seek = sizeof(slot.offsetB) + sizeof(slot.checksumB);;
            lseek(db->fd, seek , SEEK_CUR); //move to checksumA in file
            current = 0x00;
            if( write( db->fd, &current, sizeof(uint64_t)) != sizeof(uint64_t))//write current  to hashtbale slot
            {
               Kdb_unlock(&db->shmem_info->rwlock);
               return KISSDB_ERROR_IO;
            }
            cur_hash_table[hash].checksumA = crc;
            cur_hash_table[hash].current = current;
         }
         Kdb_unlock(&db->shmem_info->rwlock);
         return 0; //success
      }
      else //if key is not already inserted
      {
         /* add new data if an empty hash table slot is discovered */
         if (lseek(db->fd, 0, SEEK_END) == -1) //filepointer to the end of the file
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }
         endoffset = lseek(db->fd, 0, SEEK_CUR); //filepointer position
         if (write( db->fd, key, db->key_size) != db->key_size )
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }
         if (write( db->fd, value, db->value_size) != db->value_size )
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }

         // write same key and value again here --> initial write
#ifdef __useBackups
         if (write( db->fd, key, db->key_size) != db->key_size )
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }
         if (write( db->fd, value, db->value_size) != db->value_size )
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }
#endif
         if (lseek(db->fd, htoffset + (sizeof(Hashtable_slot_s) * hash), SEEK_SET) == -1) //move filepointer to file-hashtable slot in file (offsetA)
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }
#ifndef __useBackups
         if (write( db->fd, &endoffset, sizeof(int64_t)) != sizeof(int64_t) )  //write the offsetA to the data in the file-hashtable slot
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }
         cur_hash_table[hash].offsetA = endoffset; //write the offsetA to the data in the memory-hashtable slot
#endif

#ifdef __useBackups
         crc = 0x00;
         crc = (uint64_t) pcoCrc32(crc, (unsigned char*) value, db->value_size);
         offset = endoffset + (db->key_size + db->value_size);
         cur_hash_table[hash].offsetA = endoffset; //write the offsetA to the data in the memory-hashtable slot
         cur_hash_table[hash].checksumA = crc;
         cur_hash_table[hash].offsetB = offset;          //write the offset to the data in the memory-hashtable slot
         cur_hash_table[hash].checksumB = crc;
         cur_hash_table[hash].current = 0x00;
         current = 0x00; //current

         if (write( db->fd, &cur_hash_table[hash], sizeof(Hashtable_slot_s)) != sizeof(Hashtable_slot_s) )  //write updated data in the file-hashtable slot
         {
            Kdb_unlock(&db->shmem_info->rwlock);
            return KISSDB_ERROR_IO;
         }
#endif
         //fsync(db->fd) // associating a file stream with a synchronous file descriptor means that an fsync() call is not needed on the file descriptor after the fflush()
         Kdb_unlock(&db->shmem_info->rwlock);
         return 0; /* success */
      }
      put_no_match_next_hash_table: lasthtoffset = htoffset;
      htoffset = cur_hash_table[db->hash_table_size].offsetA; // fileoffset to the next file-hashtable
      cur_hash_table += (db->hash_table_size + 1); //pointer to the next memory-hashtable
   }

   /* if no existing slots, add a new page of hash table entries */
   if (lseek(db->fd, 0, SEEK_END) == -1) //Filepointer to the end of file
   {
      Kdb_unlock(&db->shmem_info->rwlock);
      return KISSDB_ERROR_IO;
   }
   if(db->shmem_info->num_hash_tables > 0) //only write delimiter if first hashtable has been written (first delimiter is written by open call)
   {
      if (write( db->fd, &delimiter, sizeof(delimiter)) != sizeof(delimiter) )  //write delimiter
      {
         Kdb_unlock(&db->shmem_info->rwlock);
         return KISSDB_ERROR_IO;
      }
   }
   endoffset = lseek(db->fd, 0, SEEK_CUR);

   //if new size would exceed old shared memory size-> allocate additional memory to shared memory (+ db->hash_table_size_bytes)
   if( (db->hash_table_size_bytes * (db->shmem_info->num_hash_tables + 1)) > db->shmem_info->shmem_size)
   {
      if (db->shmem_ht_fd <= 0)
      {
         db->shmem_ht_fd = kdbShmemOpen(db->shmem_ht_name,  db->old_mapped_size, &temp);
         if(db->shmem_ht_fd < 0)
            return KISSDB_ERROR_OPEN_SHM;
      }
      result = resizeKdbShmem(db->shmem_ht_fd, &db->hash_tables, db->old_mapped_size, db->old_mapped_size + db->hash_table_size_bytes);
      if (result == Kdb_false)
      {
         return KISSDB_ERROR_RESIZE_SHM;
      }
      else
      {
         db->shmem_info->shmem_size = db->old_mapped_size + db->hash_table_size_bytes;
         db->old_mapped_size = db->shmem_info->shmem_size;
      }
   }

   //if( currentHtOffset <= db->old_mapped_size / sizeof(Hashtable_slot_s) )
   cur_hash_table = &(db->hash_tables[(db->hash_table_size + 1) * db->shmem_info->num_hash_tables]);
   //else
   //   return KISSDB_ERROR_ACCESS_VIOLATION;
   memset(cur_hash_table, 0, db->hash_table_size_bytes); //hashtable init
   cur_hash_table[hash].offsetA = endoffset + db->hash_table_size_bytes; /* where new entry will go (behind the new Ht that gets written)*/

#ifdef __useBackups
   crc = 0x00;
   crc = (uint64_t) pcoCrc32(crc, (unsigned char*)value, db->value_size);
   cur_hash_table[hash].checksumA = crc;
   cur_hash_table[hash].checksumB = crc;
   cur_hash_table[hash].offsetB = cur_hash_table[hash].offsetA + (db->key_size + db->value_size);//write the offset to the data in the memory-hashtable slot
   cur_hash_table[hash].current = 0x00;
#endif

   // write new hashtable at the end of the file
   if (write( db->fd, cur_hash_table, db->hash_table_size_bytes) != db->hash_table_size_bytes )
   {
      Kdb_unlock(&db->shmem_info->rwlock);
      return KISSDB_ERROR_IO;
   }
   // write key behind new hashtable
   if (write( db->fd, key, db->key_size) != db->key_size )
   {
      Kdb_unlock(&db->shmem_info->rwlock);
      return KISSDB_ERROR_IO;
   }
   // write value behind key
   if (write( db->fd, value, db->value_size) != db->value_size )
   {
      Kdb_unlock(&db->shmem_info->rwlock);
      return KISSDB_ERROR_IO;
   }
   // write same key and value again here --> initial write
#ifdef __useBackups
   if (write( db->fd, key, db->key_size) != db->key_size )
   {
      Kdb_unlock(&db->shmem_info->rwlock);
      return KISSDB_ERROR_IO;
   }
   if (write( db->fd, value, db->value_size) != db->value_size )
   {
      Kdb_unlock(&db->shmem_info->rwlock);
      return KISSDB_ERROR_IO;
   }
#endif

   //if a hashtable exists, update link to new hashtable
   if (db->shmem_info->num_hash_tables)
   {
      if (lseek(db->fd, lasthtoffset + (sizeof(Hashtable_slot_s) * db->hash_table_size), SEEK_SET) == -1)
      {
         Kdb_unlock(&db->shmem_info->rwlock);
         return KISSDB_ERROR_IO;
      }
      if (write( db->fd, &endoffset, sizeof(int64_t)) != sizeof(int64_t) )
      {
         Kdb_unlock(&db->shmem_info->rwlock);
         return KISSDB_ERROR_IO;
      }
      db->hash_tables[((db->hash_table_size + 1) * (db->shmem_info->num_hash_tables - 1)) + db->hash_table_size].offsetA = endoffset; //update link to new hashtable in old hashtable
   }
   ++db->shmem_info->num_hash_tables;
   //fsync(db->fd)
   Kdb_unlock(&db->shmem_info->rwlock);
   return 0; /* success */
}



#if 0
/*
 * prints the offsets stored in the shared Hashtable
 */
void printSharedHashtable(KISSDB *db)
{
   Hashtable_slot_s *cur_hash_table;
   cur_hash_table = db->hash_tables;
   unsigned long k;
   unsigned long x = (db->hash_table_size * db->shmem_info->num_hash_tables);
   //printf("Address of SHARED HT_NUMBER: %p \n", &db->shmem_info->num_hash_tables);
   printf("Address of SHARED HEADER: %p \n", &cur_hash_table);
   Header_s* ptr;
   printf("HT Struct sizes: %d, %d, %d, %d,%d, %d, %d, %d\n", sizeof(ptr->KdbV), sizeof(ptr->checksum), sizeof(ptr->closeFailed), sizeof(ptr->closeOk), sizeof(ptr->hash_table_size),sizeof(ptr->key_size),sizeof(ptr->value_size),sizeof(ptr->delimiter));
   printf("HEADER SIZE: %d \n", sizeof(Header_s));
   printf("Hashtable_slot_s SIZE: %d \n", sizeof(Hashtable_slot_s));
   for (k = 0; k < x; k++)
   {
      if (db->hash_tables[k].offsetA != 0)
      {
         printf("offsetA  [%lu]: %" PRId64 " \n", k, db->hash_tables[k].offsetA);
         printf("checksumA[%lu]: %" PRIu64 " \n", k, db->hash_tables[k].checksumA);
         printf("offsetB  [%lu]: %" PRId64 " \n", k, db->hash_tables[k].offsetB);
         printf("checksumB[%lu]: %" PRIu64 " \n", k, db->hash_tables[k].checksumB);
         printf("current  [%lu]: %" PRIu64 " \n", k, db->hash_tables[k].current);
      }
   }
}
#endif


void KISSDB_Iterator_init(KISSDB *db, KISSDB_Iterator *dbi)
{
   dbi->db = db;
   dbi->h_no = 0;  // number of read hashtables
   dbi->h_idx = 0; // index in current hashtable
}


int KISSDB_Iterator_next(KISSDB_Iterator *dbi, void *kbuf, void *vbuf)
{
   int64_t offset;
   Kdb_rdlock(&dbi->db->shmem_info->rwlock);

   if ((dbi->h_no < (dbi->db->shmem_info->num_hash_tables)) && (dbi->h_idx < dbi->db->hash_table_size))
   {
      //TODO check for currently valid data block flag and use this offset instead of offsetA
      while (!(offset = dbi->db->hash_tables[((dbi->db->hash_table_size + 1) * dbi->h_no) + dbi->h_idx].offsetA))
      {
         if (++dbi->h_idx >= dbi->db->hash_table_size)
         {
            dbi->h_idx = 0;
            if (++dbi->h_no >= (dbi->db->shmem_info->num_hash_tables))
            {
               Kdb_unlock(&dbi->db->shmem_info->rwlock);
               return 0;
            }
         }
      }

      if (lseek(dbi->db->fd, offset, SEEK_SET) == -1)
         return KISSDB_ERROR_IO;
      if (read(dbi->db->fd, kbuf, dbi->db->key_size) != dbi->db->key_size)
         return KISSDB_ERROR_IO;
      if (vbuf != NULL)
      {
         if (read(dbi->db->fd, vbuf, dbi->db->value_size) != dbi->db->value_size)
            return KISSDB_ERROR_IO;
      }
      else
      {
         if (lseek(dbi->db->fd, dbi->db->value_size, SEEK_CUR) == -1)
            return KISSDB_ERROR_IO;
      }

      if (++dbi->h_idx >= dbi->db->hash_table_size)
      {
         dbi->h_idx = 0;
         ++dbi->h_no;
      }
      Kdb_unlock(&dbi->db->shmem_info->rwlock);
      return 1;
   }
   Kdb_unlock(&dbi->db->shmem_info->rwlock);
   return 0;
}



int readHeader(KISSDB* db, uint16_t* hash_table_size, uint64_t* key_size, uint64_t* value_size)
{
   //set Filepointer to the beginning of the file
   if (lseek(db->fd, 0, SEEK_SET) == -1)
      return KISSDB_ERROR_IO;
   //mmap header from beginning of file
   int mapFlag = PROT_WRITE | PROT_READ;
   Header_s* ptr = 0;
   ptr = (Header_s*) mmap(NULL, KISSDB_HEADER_SIZE, mapFlag, MAP_SHARED, db->fd, 0);
   if (ptr == MAP_FAILED)
      return KISSDB_ERROR_IO;

   if ((ptr->KdbV[0] != 'K') || (ptr->KdbV[1] != 'd') || (ptr->KdbV[2] != 'B') || (ptr->KdbV[3] != KISSDB_VERSION))
      return KISSDB_ERROR_CORRUPT_DBFILE;

   if (!ptr->hash_table_size)
      return KISSDB_ERROR_CORRUPT_DBFILE;
   (*hash_table_size) = (uint16_t) ptr->hash_table_size;

   if (!ptr->key_size)
      return KISSDB_ERROR_CORRUPT_DBFILE;
   (*key_size) = (uint64_t) ptr->key_size;

   if (!ptr->value_size)
      return KISSDB_ERROR_CORRUPT_DBFILE;
   (*value_size) = (uint64_t) ptr->value_size;

   //sync changes with file
   if (0 != msync(ptr, KISSDB_HEADER_SIZE, MS_SYNC | MS_INVALIDATE))
      return KISSDB_ERROR_IO;

   //unmap memory
   if (0 != munmap(ptr, KISSDB_HEADER_SIZE))
      return KISSDB_ERROR_IO;
   return 0;
}




int writeHeader(KISSDB* db, uint16_t* hash_table_size, uint64_t* key_size, uint64_t* value_size)
{
   Header_s* ptr = 0;
   int ret= 0;

   //Seek to beginning of file
   if (lseek(db->fd, 0, SEEK_SET) == -1)
      return KISSDB_ERROR_IO;

   //ftruncate file to needed size for header
   ret = ftruncate(db->fd, KISSDB_HEADER_SIZE);
   if (ret < 0)
      return KISSDB_ERROR_IO;

   //mmap header from beginning of file
   int mapFlag = PROT_WRITE | PROT_READ;
   ptr = (Header_s*) mmap(NULL, KISSDB_HEADER_SIZE, mapFlag, MAP_SHARED, db->fd, 0);
   if (ptr == MAP_FAILED)
      return KISSDB_ERROR_IO;

   ptr->KdbV[0] = 'K';
   ptr->KdbV[1] = 'd';
   ptr->KdbV[2] = 'B';
   ptr->KdbV[3] = KISSDB_VERSION;
   ptr->KdbV[4] = '-';
   ptr->KdbV[5] = '-';
   ptr->KdbV[6] = '-';
   ptr->KdbV[7] = '-';
   ptr->checksum = 0x00;
   ptr->closeFailed = 0x00; //remove closeFailed flag
   ptr->closeOk = 0x01;     //set closeOk flag
   ptr->hash_table_size = (uint64_t)(*hash_table_size);
   ptr->key_size = (uint64_t)(*key_size);
   ptr->value_size = (uint64_t)(*value_size);
   memcpy(ptr->delimiter,"||||||||", 8);

   //sync changes with file
   if (0 != msync(ptr, KISSDB_HEADER_SIZE, MS_SYNC | MS_INVALIDATE))
      return KISSDB_ERROR_IO;

   //unmap memory
   if (0 != munmap(ptr, KISSDB_HEADER_SIZE))
      return KISSDB_ERROR_IO;
   return 0;
}


int checkErrorFlags(KISSDB* db)
{
   //mmap header from beginning of file
   int mapFlag = PROT_WRITE | PROT_READ;
   Header_s* ptr = 0;
   ptr = (Header_s*) mmap(NULL, KISSDB_HEADER_SIZE, mapFlag, MAP_SHARED, db->fd, 0);
   if (ptr == MAP_FAILED)
      return KISSDB_ERROR_IO;
   //uint64_t crc = 0;

#ifdef __checkerror
   //check if closeFailed flag is set
   if(ptr->closeFailed == 0x01)
   {
      //TODO implement verifyHashtableCS

      //if closeFailed flag is set, something went wrong at last close -> so check crc
      db->shmem_info->crc_invalid = Kdb_true; //check crc for further reads

#if 0
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING("OPENING DB -> closeFailed flag is set:  "), DLT_UINT64(ptr->closeFailed));
      crc = (uint64_t) pcoCalcCrc32Csum(db->fd, sizeof(Header_s));
      if(ptr->checksum != 0) //do not check if database is currently in creation
      {
         if (crc != ptr->checksum)
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING("OPENING DB: "), DLT_STRING(db->shmem_ht_name), DLT_STRING(" CHECKSUM IN HEADER : "), DLT_UINT64(ptr->checksum), DLT_STRING(" != CHECKSUM CALCULATED: "), DLT_UINT64(crc));
            //db->shmem_info->crc_invalid = Kdb_true; //check datablocks at further reads
            //return KISSDB_ERROR_CORRUPT_DBFILE; //previous close failed and checksum invalid -> error state -> return error
         }
         else
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("OPENING DB: "), DLT_STRING(db->shmem_ht_name), DLT_STRING(" CECHKSUM IN HEADER: "), DLT_UINT64(ptr->checksum), DLT_STRING(" == CHECKSUM CALCULATED: "), DLT_UINT64(crc));
            //db->shmem_info->crc_invalid = Kdb_false; //do not check datablocks at further reads
         }
      }
      else
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("Do not check checksum, database in creation: "), DLT_STRING(db->shmem_ht_name));
#endif
   }
   else
   {
      //DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("OPENING DB: closeFailed flag is not set:  "), DLT_UINT64(ptr->closeFailed));
      ptr->closeFailed = 0x01; //NO: create close failed flag
   }


   //check if closeOk flag is set
   if(ptr->closeOk == 0x01)
   {
      //DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("OPENING DB -> closeOk flag is set:  "), DLT_UINT64(ptr->closeOk));
      ptr->closeOk = 0x00;
   }
   else
   {
      //if closeOK is not set , something went wrong at last close
      db->shmem_info->crc_invalid = Kdb_true; //do crc check at read

#if 0
      crc = (uint64_t) pcoCalcCrc32Csum(db->fd, sizeof(Header_s));
      if(ptr->checksum != 0) //do not check if database is currently in creation
      {
         if (crc != ptr->checksum)
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING("OPENING DB: "), DLT_STRING(db->shmem_ht_name), DLT_STRING(" CHECKSUM IN HEADER : "), DLT_UINT64(ptr->checksum), DLT_STRING(" != CHECKSUM CALCULATED: "), DLT_UINT64(crc));
            //db->shmem_info->crc_invalid = Kdb_true;
            //return KISSDB_ERROR_CORRUPT_DBFILE; //previous close failed and checksum invalid -> error state -> return error
         }
         else
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("OPENING DB: "), DLT_STRING(db->shmem_ht_name), DLT_STRING(" CECHKSUM IN HEADER: "), DLT_UINT64(ptr->checksum), DLT_STRING(" == CHECKSUM CALCULATED: "), DLT_UINT64(crc));
            //db->shmem_info->crc_invalid = Kdb_false;
         }
      }
      else
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("Do not check checksum, database in creation: "), DLT_STRING(db->shmem_ht_name));
      }

      DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING("OPENING DB -> closeOk flag is not set:  "), DLT_UINT64(ptr->closeOk));
#endif


   }
#endif
   //sync changes with file
   if (0 != msync(ptr, KISSDB_HEADER_SIZE, MS_SYNC | MS_INVALIDATE))
      return KISSDB_ERROR_IO;

   //unmap memory
   if (0 != munmap(ptr, KISSDB_HEADER_SIZE))
      return KISSDB_ERROR_IO;

   return 0;
}
