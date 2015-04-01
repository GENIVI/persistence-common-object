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
#define KISSDB_HEADER_SIZE sizeof(Header_s)

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
#include <sys/stat.h>
#include <sys/time.h>
#include <semaphore.h>
#include <dlt.h>
#include "persComErrors.h"

//
//#ifdef COND_GCOV
//extern void __gcov_flush(void);
//#endif

//#define PFS_TEST
//extern DltContext persComLldbDLTCtx;
DLT_IMPORT_CONTEXT (persComLldbDLTCtx)

#ifdef __showTimeMeasurements
inline long long getNsDuration(struct timespec* start, struct timespec* end)
{
   return ((end->tv_sec * SECONDS2NANO) + end->tv_nsec) - ((start->tv_sec * SECONDS2NANO) + start->tv_nsec);
}
#endif

/* djb2 hash function */
static uint64_t KISSDB_hash(const void* b, unsigned long len)
{
   unsigned long i;
   uint64_t hash = 5381;
   for (i = 0; i < len; ++i)
   {
      hash = ((hash << 5) + hash) + (uint64_t) (((const uint8_t*) b)[i]);
   }
   return hash;
}

#if 1
//returns a name for shared memory objects beginning with a slash followed by "path" (non alphanumeric chars are replaced with '_')  appended with "tailing"
char* kdbGetShmName(const char* tailing, const char* path)
{
   int pathLen = strlen(path);
   int tailLen = strlen(tailing);
   char* result = (char*) malloc(1 + pathLen + tailLen + 1);   //free happens at lifecycle shutdown
   int i =0;
   int x = 1;

   if (result != NULL)
   {
      result[0] = '/';
      for (i = 0; i < pathLen; i++)
      {
         if (!isalnum(path[i]))
         {
            result[i + 1] = '_';
         }
         else
         {
            result[i + 1] = path[i];
         }
      }
      for (x = 0; x < tailLen; x++)
      {
         result[i + x + 1] = tailing[x];
      }
      result[i + x + 1] = '\0';
   }
   else
   {
      result = NULL;
   }
   return result;
}
#endif



//returns -1 on error and positive value for success
int kdbShmemOpen(const char* name, size_t length, Kdb_bool* shmCreator)
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
         {
            return -1;
         }
      }
   }
   else
   {
      *shmCreator = Kdb_true;
      if (ftruncate(result, length) < 0)
      {
         return -1;
      }
   }
   return result;
}

void Kdb_wrlock(pthread_rwlock_t* wrlock)
{
   pthread_rwlock_wrlock(wrlock);
}

//void Kdb_rdlock(pthread_rwlock_t* rdlock)
//{
//   pthread_rwlock_rdlock(rdlock);
//}

void Kdb_unlock(pthread_rwlock_t* lock)
{
   pthread_rwlock_unlock(lock);
}

Kdb_bool kdbShmemClose(int shmem, const char* shmName)
{
   if( close(shmem) == -1)
   {
      return Kdb_false;
   }
   if( shm_unlink(shmName) < 0)
   {
      return Kdb_false;
   }
   return Kdb_true;
}

void* getKdbShmemPtr(int shmem, size_t length)
{
   void* result = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, shmem, 0);
   if (result == MAP_FAILED)
   {
      return ((void*) -1);
   }
   return result;
}


Kdb_bool freeKdbShmemPtr(void* shmem_ptr, size_t length)
{
   if(munmap(shmem_ptr, length) == 0)
   {
      return Kdb_true;
   }
   else
   {
      return Kdb_false;
   }
}

Kdb_bool resizeKdbShmem(int shmem, Hashtable_s** shmem_ptr, size_t oldLength, size_t newLength)
{
   //unmap shm with old size
   if( freeKdbShmemPtr(*shmem_ptr, oldLength) == Kdb_false)
   {
      return Kdb_false;
   }

   if (ftruncate(shmem, newLength) < 0)
   {
      return Kdb_false;
   }

   //get pointer to resized shm with new Length
   *shmem_ptr = getKdbShmemPtr(shmem, newLength);
   if(*shmem_ptr == ((void*) -1))
   {
      return Kdb_false;
   }
   return Kdb_true;
}


Kdb_bool remapSharedHashtable(int shmem, Hashtable_s** shmem_ptr, size_t oldLength, size_t newLength )
{
   //unmap hashtable with old size
   if( freeKdbShmemPtr(*shmem_ptr, oldLength) == Kdb_false)
   {
      return Kdb_false;
   }
   //get pointer to resized shm with new Length
   *shmem_ptr = getKdbShmemPtr(shmem, newLength);
   if(*shmem_ptr == ((void*) -1))
   {
      return Kdb_false;
   }
   return Kdb_true;
}

#if 0
void printKdb(KISSDB* db)
{
   printf("START ############################### \n");
   printf("db->htSize:  %d \n", db->htSize);
   printf("db->cacheReferenced: %d \n", db->cacheReferenced);
   printf("db->keySize:  %" PRId64 " \n", db->keySize);
   printf("db->valSize:  %" PRId64 " \n", db->valSize);
   printf("db->htSizeBytes:  %" PRId64 " \n", db->htSizeBytes);
   printf("db->htMappedSize:  %" PRId64 " \n", db->htMappedSize);
   printf("db->dbMappedSize:  %" PRId64 " \n", db->dbMappedSize);
   printf("db->shmCreator:  %d \n", db->shmCreator);
   printf("db->alreadyOpen:  %d \n", db->alreadyOpen);
   printf("db->hashTables:  %p \n", db->hashTables);
   printf("db->mappedDb:  %p \n", db->mappedDb);
   printf("db->sharedCache:  %p \n", db->sharedCache);
   printf("db->sharedFd:  %d \n", db->sharedFd);
   printf("db->htFd:  %d \n", db->htFd);
   printf("db->sharedCacheFd:  %d \n", db->sharedCacheFd);
   printf("db->semName:  %s \n", db->semName);
   printf("db->sharedName:  %s \n", db->sharedName);
   printf("db->cacheName:  %s \n", db->cacheName);
   printf("db->htName:  %s \n", db->htName);
   printf("db->shared:  %p \n", db->shared);
   printf("db->tbl:  %p \n", db->tbl[0]);
   printf("db->kdbSem:  %p \n", db->kdbSem);
   printf("db->fd:  %d \n", db->fd);
   printf("END ############################### \n");
}
#endif


int KISSDB_open(KISSDB* db, const char* path, int openMode, int writeMode, uint16_t hash_table_size, uint64_t key_size, uint64_t value_size)
{
   Hashtable_s* htptr;
   int ret = 0;
   Kdb_bool htFound;
   Kdb_bool tmpCreator;
   off_t offset = 0;
   size_t firstMappSize;
   struct stat sb;

   if (db->alreadyOpen == Kdb_false) //check if this instance has already opened the db before
   {
      db->sharedName = kdbGetShmName("-shm-info", path);
      if (db->sharedName == NULL)
      {
         return KISSDB_ERROR_MALLOC;
      }
      db->sharedFd = kdbShmemOpen(db->sharedName, sizeof(Shared_Data_s), &db->shmCreator);
      if (db->sharedFd < 0)
      {
         return KISSDB_ERROR_OPEN_SHM;
      }
      db->shared = (Shared_Data_s*) getKdbShmemPtr(db->sharedFd, sizeof(Shared_Data_s));
      if (db->shared == ((void*) -1))
      {
         return KISSDB_ERROR_MAP_SHM;
      }

      db->sharedCacheFd = -1;
      db->mappedDb = NULL;

      if (db->shmCreator == Kdb_true)
      {
         //[Initialize rwlock attributes]
         pthread_rwlockattr_t rwlattr;
         pthread_rwlockattr_init(&rwlattr);
         pthread_rwlockattr_setpshared(&rwlattr, PTHREAD_PROCESS_SHARED);
         pthread_rwlock_init(&db->shared->rwlock, &rwlattr);

         Kdb_wrlock(&db->shared->rwlock);

         //init cache filedescriptor, reference counter and hashtable number
         db->sharedCacheFd = -1;
         db->shared->refCount = 0;
         db->shared->htNum = 0;
         db->shared->mappedDbSize = 0;
         db->shared->writeMode = writeMode;
         db->shared->openMode = openMode;
      }
      else
      {
         Kdb_wrlock(&db->shared->rwlock);
      }
   }
   else
   {
      Kdb_wrlock(&db->shared->rwlock);
   }

   switch (db->shared->openMode)
   {
      case KISSDB_OPEN_MODE_RWCREAT:
      {
         //create database
         db->fd = open(path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
         break;
      }
      case KISSDB_OPEN_MODE_RDWR:
      {
         //read / write mode
         db->fd = open(path, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
         break;
      }
      case KISSDB_OPEN_MODE_RDONLY:
      {
         db->fd = open(path, O_RDONLY);
         break;
      }
      default:
      {
         break;
      }
   }
   if(db->fd == -1)
   {
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__); DLT_STRING(": Opening database file: <"); DLT_STRING(path); DLT_STRING("> failed: "); DLT_STRING(strerror(errno)));
      return KISSDB_ERROR_IO;
   }

   if( 0 != fstat(db->fd, &sb))
   {
      return KISSDB_ERROR_IO;
   }


   /* mmap whole database file if it already exists (else the file is mapped in writeheader()) */
   if (sb.st_size > 0)
   {
      if(db->shared->openMode != KISSDB_OPEN_MODE_RDONLY )
      {
         db->mappedDb = (void*) mmap(NULL, sb.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, db->fd, 0);
      }
      else
      {
         db->mappedDb = (void*) mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, db->fd, 0);
      }
      if (db->mappedDb == MAP_FAILED)
      {
         return KISSDB_ERROR_IO;
      }
      else
      {
         //update mapped size
         db->shared->mappedDbSize = (uint64_t)sb.st_size;
         db->dbMappedSize = db->shared->mappedDbSize;
      }
   }

   offset = sb.st_size;
   if (offset == -1)
   {
      return KISSDB_ERROR_IO;
   }
   if ( offset < KISSDB_HEADER_SIZE)
   {
      /* write header if not already present */
      if ((hash_table_size) && (key_size) && (value_size))
      {
         ret = writeHeader(db, &hash_table_size, &key_size, &value_size);
         if(0 != ret)
         {
            return ret;
         }
      }
      else
      {
         return KISSDB_ERROR_INVALID_PARAMETERS;
      }
   }
   else
   {
      /* read existing header to verify database version */
      ret = readHeader(db, &hash_table_size, &key_size, &value_size);
      if( 0 != ret)
      {
         return ret;
      }
   }
   //store non shared db information
   db->htSize = hash_table_size;
   db->keySize = key_size;
   db->valSize = value_size;
   db->htSizeBytes = sizeof(Hashtable_s);

   if (db->alreadyOpen == Kdb_false) //check if this instance has already opened the db before
   {
      db->cacheName = kdbGetShmName("-cache", path);
      if(db->cacheName == NULL)
      {
         return KISSDB_ERROR_MALLOC;
      }
      //check if more than one hashtable is already in shared memory
      if(db->shared->htShmSize > db->htSizeBytes )
      {
         firstMappSize = db->shared->htShmSize;
      }
      else
      {
         firstMappSize = db->htSizeBytes;
      }

      //open / create shared memory for first hashtable
      db->htName = kdbGetShmName("-ht", path);
      if(db->htName == NULL)
      {
         return KISSDB_ERROR_MALLOC;
      }
      db->htFd = kdbShmemOpen(db->htName,  firstMappSize, &tmpCreator);
      if(db->htFd < 0)
      {
         return KISSDB_ERROR_OPEN_SHM;
      }
      db->hashTables = (Hashtable_s*) getKdbShmemPtr(db->htFd, firstMappSize);
      if(db->hashTables == ((void*) -1))
      {
         return KISSDB_ERROR_MAP_SHM;
      }
      db->htMappedSize = firstMappSize;
      db->alreadyOpen = Kdb_true;
   }

   /*
    *  Read hashtables from file into memory ONLY for first caller of KISSDB_open
    *  Determine number of existing hashtables (db->shared->htNum)    *
    */
   if (db->shmCreator == Kdb_true )
   {
      uint64_t offset = KISSDB_HEADER_SIZE;
      //only read hashtables from file if file is larger than header + hashtable size
      if(db->shared->mappedDbSize >= ( KISSDB_HEADER_SIZE + db->htSizeBytes) )
      {
         htptr = (Hashtable_s*) ( db->mappedDb + KISSDB_HEADER_SIZE);
         htFound = Kdb_true;
         while (htFound && offset < db->dbMappedSize )
         {
            //check for existing start OR end delimiter of hashtable
            if (htptr->delimStart == HASHTABLE_START_DELIMITER || htptr->delimEnd == HASHTABLE_END_DELIMITER)
            {
               //if new size would exceed old shared memory size-> allocate additional memory page to shared memory
               Kdb_bool result = Kdb_false;
               if ( (db->htSizeBytes * (db->shared->htNum + 1)) > db->htMappedSize)
               {
                  Kdb_bool temp;
                  if (db->htFd <= 0)
                  {
                     db->htFd = kdbShmemOpen(db->htName,  db->htMappedSize, &temp);
                     if(db->htFd < 0)
                     {
                        return KISSDB_ERROR_OPEN_SHM;
                     }
                  }
                  result = resizeKdbShmem(db->htFd, &db->hashTables, db->htMappedSize, db->htMappedSize + db->htSizeBytes);
                  if (result == Kdb_false)
                  {
                     return KISSDB_ERROR_RESIZE_SHM;
                  }
                  else
                  {
                     db->shared->htShmSize = db->htMappedSize + db->htSizeBytes;
                     db->htMappedSize = db->shared->htShmSize;
                  }
               }
               // copy the current hashtable read from file to (htadress + (htsize  * htcount)) in memory
               memcpy(((uint8_t*) db->hashTables) + (db->htSizeBytes * db->shared->htNum), htptr, db->htSizeBytes);
               ++db->shared->htNum;

               //read until all linked hashtables have been read
               if (htptr->slots[db->htSize].offsetA ) //if a offset to a further hashtable exists
               {
                  htptr = (Hashtable_s*) (db->mappedDb + htptr->slots[db->htSize].offsetA); //follow link to next hashtable
                  offset = htptr->slots[db->htSize].offsetA;
               }
               else //no link to next hashtable or link is invalid
               {
                  htFound = Kdb_false;
               }
            }
            else //delimiters of first hashtable or linked hashtable are invalid
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__); DLT_STRING(": hashtable delimiters are invalid -> rebuild not possible!"));
               htFound = Kdb_false;
            }
         }
      }
      /*
       *  CHECK POWERLOSS FLAGS AND REBUILD DATABASE IF NECESSARY
       */
      if (db->shared->openMode != KISSDB_OPEN_MODE_RDONLY)
      {
         if (checkErrorFlags(db) != 0)
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING(__FUNCTION__); DLT_STRING(": database was not closed correctly in last lifecycle!"));
            if (verifyHashtableCS(db) != 0)
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING(__FUNCTION__); DLT_STRING(": A hashtable is invalid -> Start rebuild of hashtables!"));
               if (rebuildHashtables(db) != 0) //hashtables are corrupt, walk through the database and search for data blocks -> then rebuild the hashtables
               {
                  DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__); DLT_STRING(": hashtable rebuild failed!"));
               }
               else
               {
                  DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG, DLT_STRING(__FUNCTION__); DLT_STRING(": hashtable rebuild successful!"));
               }
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING(__FUNCTION__); DLT_STRING(":Start datablock check / recovery!"));
               recoverDataBlocks(db);
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING(__FUNCTION__); DLT_STRING(":End datablock check / recovery!"));
            }
            else
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING(__FUNCTION__); DLT_STRING(":Start datablock check / recovery!"));
               recoverDataBlocks(db);
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING(__FUNCTION__); DLT_STRING(":End datablock check / recovery!"));
            }
         }
      }
   }
   Kdb_unlock(&db->shared->rwlock);
   return 0;
}


int KISSDB_close(KISSDB* db)
{
#ifdef PFS_TEST
   printf("  START: KISSDB_CLOSE \n");
#endif

   Hashtable_s* htptr = NULL;
   Header_s* ptr = 0;
   uint64_t  crc = 0;

   Kdb_wrlock(&db->shared->rwlock);

   //if no other instance has opened the database
   if( db->shared->refCount == 0)
   {
      if (db->shared->openMode != KISSDB_OPEN_MODE_RDONLY)
      {
         if(db->htMappedSize < db->shared->htShmSize)
         {
            if ( Kdb_false == remapSharedHashtable(db->htFd, &db->hashTables, db->htMappedSize, db->shared->htShmSize))
            {
               return KISSDB_ERROR_RESIZE_SHM;
            }
            else
            {
               db->htMappedSize = db->shared->htShmSize;
            }
         }
         //remap database file if in the meanwhile another process added new data (key value pairs / hashtables) to the file (only happens if writethrough is used)
         if (db->dbMappedSize < db->shared->mappedDbSize)
         {
            db->mappedDb = mremap(db->mappedDb, db->dbMappedSize, db->shared->mappedDbSize, MREMAP_MAYMOVE);
            if (db->mappedDb == MAP_FAILED)
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING(__FUNCTION__); DLT_STRING(":mremap error: !"), DLT_STRING(strerror(errno)));
               return KISSDB_ERROR_IO;
            }
            else
            {
               db->dbMappedSize = db->shared->mappedDbSize;
            }
         }

         // generate checksum for every hashtable and write crc to file
         if (db->fd)
         {
            int i = 0;
            int offset = sizeof(Header_s); //offset in file to first hashtable
            if (db->shared->htNum > 0) //if hashtables exist
            {
               //write hashtables and crc to file
               for (i = 0; i < db->shared->htNum; i++)
               {
                  crc = 0;
                  crc = (uint64_t) pcoCrc32(crc, (unsigned char*) db->hashTables[i].slots, sizeof(db->hashTables[i].slots));
                  db->hashTables[i].crc = crc;
                  htptr = (Hashtable_s*) (db->mappedDb +  offset);
                  //copy hashtable and generated crc from shared memory to mapped hashtable in file
                  memcpy(htptr, &db->hashTables[i], db->htSizeBytes);
                  offset = db->hashTables[i].slots[db->htSize].offsetA;
               }
            }
         }
         //update header (close flags)
         ptr = (Header_s*) db->mappedDb;
         ptr->closeFailed = 0x00; //remove closeFailed flag
         ptr->closeOk = 0x01;     //set closeOk flag
         msync(db->mappedDb, KISSDB_HEADER_SIZE, MS_SYNC);
      }

      //unmap whole database file
      munmap(db->mappedDb, db->dbMappedSize);
      db->mappedDb = NULL;

      //unmap shared hashtables
      munmap(db->hashTables, db->htMappedSize);
      db->hashTables = NULL;
      //close shared memory for hashtables
      if( kdbShmemClose(db->htFd, db->htName) == Kdb_false)
      {
         close(db->fd);
         Kdb_unlock(&db->shared->rwlock);
         return KISSDB_ERROR_CLOSE_SHM;
      }
      db->htFd = 0;

      if(db->htName != NULL)
      {
         free(db->htName);
         db->htName = NULL;
      }

      //free rwlocks
      Kdb_unlock(&db->shared->rwlock);
      pthread_rwlock_destroy(&db->shared->rwlock);

      // unmap shared information
      munmap(db->shared, sizeof(Shared_Data_s));
      db->shared = NULL;

      if (kdbShmemClose(db->sharedFd, db->sharedName) == Kdb_false)
      {
         close(db->fd);
         return KISSDB_ERROR_CLOSE_SHM;
      }
      db->sharedFd =0;
      if(db->sharedName != NULL)
      {
         free(db->sharedName);
         db->sharedName = NULL;
      }
      if(db->cacheName != NULL)
      {
        free(db->cacheName); //free memory for name  obtained by kdbGetShmName() function
        db->cacheName = NULL;
      }
      if( db->fd)
      {
         close(db->fd);
         db->fd = 0;
      }

      db->alreadyOpen = Kdb_false;
      db->htSize = 0;
      //db->cacheReferenced = 0;
      db->keySize = 0;
      db->valSize = 0;
      db->htSizeBytes = 0;
      db->htMappedSize = 0;
      db->dbMappedSize = 0;
      db->shmCreator = 0;
      db->alreadyOpen = 0;

      //destroy named semaphore
      if (-1 == sem_post(db->kdbSem)) //release semaphore
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                 DLT_STRING(__FUNCTION__); DLT_STRING(": sem_post() failed: "),
                 DLT_STRING(strerror(errno)));
      }
      if (-1 == sem_close(db->kdbSem))
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                 DLT_STRING(__FUNCTION__); DLT_STRING(": sem_close() failed: "),
                 DLT_STRING(strerror(errno)));
      }
      if (-1 == sem_unlink(db->semName))
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                 DLT_STRING(__FUNCTION__); DLT_STRING(": sem_unlink() failed: "),
                 DLT_STRING(strerror(errno)));
      }
      db->kdbSem = NULL;
      if(db->semName != NULL)
      {
         free(db->semName);
         db->semName = NULL;
      }

   }
   else
   {
      //if caller of close is not the last instance using the database
      //unmap whole database file
      munmap(db->mappedDb, db->dbMappedSize);
      db->mappedDb = NULL;

      //unmap shared hashtables
      munmap(db->hashTables, db->htMappedSize);
      db->hashTables = NULL;

      if( db->fd)
      {
         close(db->fd);
         db->fd = 0;
      }
      if(db->htFd)
      {
         close(db->htFd);
         db->htFd = 0;
      }

      db->alreadyOpen = Kdb_false;
      db->htSize = 0;
      //db->cacheReferenced = 0;
      db->keySize = 0;
      db->valSize = 0;
      db->htSizeBytes = 0;
      db->htMappedSize = 0;
      db->dbMappedSize = 0;
      db->shmCreator = 0;
      db->alreadyOpen = 0;

      Kdb_unlock(&db->shared->rwlock);

      // unmap shared information
      munmap(db->shared, sizeof(Shared_Data_s));
      db->shared = NULL;

      if(db->sharedFd)
      {
         close(db->sharedFd);
         db->sharedFd = 0;
      }
      if(db->htName != NULL)
      {
         free(db->htName);
         db->htName = NULL;
      }
      if(db->sharedName != NULL)
      {
         free(db->sharedName);
         db->sharedName = NULL;
      }
      if(db->cacheName != NULL)
      {
        free(db->cacheName); //free memory for name  obtained by kdbGetShmName() function
        db->cacheName = NULL;
      }
      //clean struct
      if (-1 == sem_post(db->kdbSem))
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                 DLT_STRING(__FUNCTION__); DLT_STRING(": sem_post() in close failed: "),
                 DLT_STRING(strerror(errno)));
      }
      if (-1 == sem_close(db->kdbSem))
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                 DLT_STRING(__FUNCTION__); DLT_STRING(": sem_close() in close failed: "),
                 DLT_STRING(strerror(errno)));
      }
      db->kdbSem = NULL;
      if(db->semName != NULL)
      {
         free(db->semName);
         db->semName = NULL;
      }
   }
#ifdef PFS_TEST
   printf("  END: KISSDB_CLOSE \n");
#endif
   return 0;
}


int KISSDB_get(KISSDB* db, const void* key, void* vbuf, uint32_t bufsize, uint32_t* vsize)
{
   const uint8_t* kptr;
   DataBlock_s* block;
   Hashtable_slot_s* hashTable;
   int64_t offset;
   Kdb_bool bCanContinue = Kdb_true;
   Kdb_bool bKeyFound = Kdb_false;
   uint64_t hash = 0;
   unsigned long klen, i;

   klen = strlen(key);
   hash = KISSDB_hash(key, klen) % (uint64_t) db->htSize;

   if(db->htMappedSize < db->shared->htShmSize)
   {
      if ( Kdb_false == remapSharedHashtable(db->htFd, &db->hashTables, db->htMappedSize, db->shared->htShmSize))
      {
         return KISSDB_ERROR_RESIZE_SHM;
      }
      else
      {
         db->htMappedSize = db->shared->htShmSize;
      }
   }

   hashTable = db->hashTables[0].slots; //pointer to first hashtable in memory at first slot

   if (db->dbMappedSize < db->shared->mappedDbSize)
   {
      db->mappedDb = mremap(db->mappedDb, db->dbMappedSize, db->shared->mappedDbSize, MREMAP_MAYMOVE);
      if (db->mappedDb == MAP_FAILED)
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING(__FUNCTION__); DLT_STRING(":mremap error: !"), DLT_STRING(strerror(errno)));
         return KISSDB_ERROR_IO;
      }
      else
      {
         db->dbMappedSize = db->shared->mappedDbSize;
      }
   }


   for (i = 0; i < db->shared->htNum; ++i)
   {
      //get information about current valid offset to latest written data
      offset = (hashTable[hash].current == 0x00) ? hashTable[hash].offsetA : hashTable[hash].offsetB; // if 0x00 -> offsetA is latest else offsetB is latest
      if(offset < 0) //deleted or invalidated data but search in next hashtable
      {
         bCanContinue = Kdb_false; //deleted datablock -> do not compare the key
      }
      else
      {
         bCanContinue = Kdb_true; //possible match -> compare the key
      }

      if(Kdb_true == bCanContinue)
      {
         if( abs(offset) > db->dbMappedSize )
         {
            return KISSDB_ERROR_IO;
         }
         if (offset >= KISSDB_HEADER_SIZE) //if a valid offset is available in the slot
         {
            block = (DataBlock_s*) (db->mappedDb +  offset);
            kptr = (const uint8_t*) key;
            if (klen > 0)
            {
               if (memcmp(kptr, block->key, klen)
                     || strlen(block->key) != klen) //if search key does not match with key in file
               {
                  bKeyFound = Kdb_false;
               }
               else
               {
                  bKeyFound = Kdb_true;
               }
            }
            if(Kdb_true == bKeyFound)
            {
               //copy found value if buffer is big enough
               if(bufsize >= block->valSize)
               {
                  memcpy(vbuf, block->value, block->valSize);
               }
               *(vsize) = block->valSize;
               return 0; /* success */
            }
         }
         else
         {
            return 1; /* not found */
         }
      }
      hashTable = (Hashtable_slot_s*) ((char*) hashTable + sizeof(Hashtable_s));  //pointer to the next memory-hashtable
   }
   return 1; /* not found */
}


int KISSDB_delete(KISSDB* db, const void* key, int32_t* bytesDeleted)
{
   const uint8_t* kptr;
   DataBlock_s* backupBlock;
   DataBlock_s* block;
   Hashtable_slot_s* hashTable;
   int64_t backupOffset = 0;
   int64_t offset = 0;
   Kdb_bool bCanContinue = Kdb_true;
   Kdb_bool bKeyFound = Kdb_false;
   uint64_t hash = 0;
   uint64_t crc = 0x00;
   unsigned long klen, i;

   klen = strlen(key);
   hash = KISSDB_hash(key, klen) % (uint64_t) db->htSize;
   *(bytesDeleted) = PERS_COM_ERR_NOT_FOUND;

   if(db->htMappedSize < db->shared->htShmSize)
   {
      if ( Kdb_false == remapSharedHashtable(db->htFd, &db->hashTables, db->htMappedSize, db->shared->htShmSize))
      {
         return KISSDB_ERROR_RESIZE_SHM;
      }
      else
      {
         db->htMappedSize = db->shared->htShmSize;
      }
   }

   hashTable = db->hashTables->slots; //pointer to current hashtable in memory

   //remap database file if in the meanwhile another process added new data (key value pairs / hashtables) to the file
   if (db->dbMappedSize < db->shared->mappedDbSize)
   {
      db->mappedDb = mremap(db->mappedDb, db->dbMappedSize, db->shared->mappedDbSize, MREMAP_MAYMOVE);
      if (db->mappedDb == MAP_FAILED)
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING(__FUNCTION__); DLT_STRING(":mremap error: !"), DLT_STRING(strerror(errno)));
         return KISSDB_ERROR_IO;
      }
      db->dbMappedSize = db->shared->mappedDbSize;
   }

   for (i = 0; i < db->shared->htNum; ++i)
   {
      //get information about current valid offset to latest written data
      if (hashTable[hash].current == 0x00) //valid is offsetA
      {
         offset = hashTable[hash].offsetA;
         backupOffset = hashTable[hash].offsetB;
      }
      else
      {
         offset = hashTable[hash].offsetB;
         backupOffset = hashTable[hash].offsetA;
      }
      if (offset < 0) //deleted or invalidated data but search in next hashtable
      {
         bCanContinue = Kdb_false;
      }
      else
      {
         bCanContinue = Kdb_true;
      }
      if (Kdb_true == bCanContinue)
      {
         if (offset >= KISSDB_HEADER_SIZE)
         {
            if( abs(offset) > db->dbMappedSize || abs(backupOffset) >  db->dbMappedSize)
            {
               return KISSDB_ERROR_IO;
            }

            block = (DataBlock_s*) (db->mappedDb +  offset);
            kptr = (const uint8_t*) key;  //pointer to search key
            if (klen > 0)
            {
               if (memcmp(kptr, block->key, klen) || strlen(block->key) != klen) //if search key does not match with key in file
               {
                  bKeyFound = Kdb_false;
               }
               else
               {
                  bKeyFound = Kdb_true;
               }
            }
            /* data to be deleted was found
             write "deleted block delimiters" for both blocks and delete key / value */
            if (Kdb_true == bKeyFound)
            {
               block->delimStart = (offset < backupOffset) ? DATA_BLOCK_A_DELETED_START_DELIMITER : DATA_BLOCK_B_DELETED_START_DELIMITER;
               //memset(block->key,   0, db->keySize); //do not delete key -> used in hashtable rebuild
               memset(block->value, 0, db->valSize);
               block->valSize = 0;
               crc = 0x00;
               crc = (uint32_t) pcoCrc32(crc, (unsigned char*)block->key, db->keySize + sizeof(uint32_t) + db->valSize + sizeof(uint64_t) );
               block->crc = crc;
               block->delimEnd = (offset < backupOffset) ? DATA_BLOCK_A_DELETED_END_DELIMITER : DATA_BLOCK_B_DELETED_END_DELIMITER;

               backupBlock = (DataBlock_s*) (db->mappedDb +  backupOffset);  //map data and backup block

               backupBlock->delimStart = (backupOffset < offset) ? DATA_BLOCK_A_DELETED_START_DELIMITER : DATA_BLOCK_B_DELETED_START_DELIMITER;
               //memset(backupBlock->key,   0, db->keySize);
               memset(backupBlock->value, 0, db->valSize);
               backupBlock->valSize = 0;
               crc = 0x00;
               crc = (uint32_t) pcoCrc32(crc, (unsigned char*)backupBlock->key, db->keySize + sizeof(uint32_t) + db->valSize + sizeof(uint64_t) );
               backupBlock->crc = crc;
               backupBlock->delimEnd = (backupOffset < offset) ? DATA_BLOCK_A_DELETED_END_DELIMITER : DATA_BLOCK_B_DELETED_END_DELIMITER;


               //negate offsetB, delete checksums and current flag in memory
               hashTable[hash].offsetA = -hashTable[hash].offsetA; //negate offset in hashtable that points to the data
               hashTable[hash].offsetB = -hashTable[hash].offsetB;
               hashTable[hash].current = 0x00;

               *(bytesDeleted) = block->valSize;
               return 0; /* success */
            }
         }
         else
         {
            return 1; /* not found */ //if no offset is found at hashed position in slots
         }
      }
      hashTable = (Hashtable_slot_s*) ((char*) hashTable + sizeof(Hashtable_s));  //pointer to the next memory-hashtable
   }
   return 1; /* not found */
}

// To improve write amplifiction: sort the keys at writeback for sequential write
//return offset where key would be written if Kissdb_put with same key is called
//int determineKeyOffset(KISSDB* db, const void* key)
//{
//   /*
//    * - hash the key,
//    * - go through hashtables and get corresponding offset to hash
//    * - if offset is negative return the inverse offset
//    * - if key matches at file offset return this offset
//    */
//   return 0;
//}




int KISSDB_put(KISSDB* db, const void* key, const void* value, int valueSize, int32_t* bytesWritten)
{
   const uint8_t* kptr;
   DataBlock_s* backupBlock;
   DataBlock_s* block;
   Hashtable_s* hashtable;
   Hashtable_s* htptr;
   Hashtable_slot_s* hashTable;
   int64_t offset, backupOffset, endoffset;
   Kdb_bool bKeyFound = Kdb_false;
   Kdb_bool result = Kdb_false;
   Kdb_bool temp = Kdb_false;
   uint64_t crc = 0x00;
   uint64_t hash = 0;
   unsigned long klen, i;

   klen = strlen(key);
   hash = KISSDB_hash(key, klen) % (uint64_t) db->htSize;
   *(bytesWritten) = 0;

   if(db->htMappedSize < db->shared->htShmSize)
   {
      if ( Kdb_false == remapSharedHashtable(db->htFd, &db->hashTables, db->htMappedSize, db->shared->htShmSize))
      {
         return KISSDB_ERROR_RESIZE_SHM;
      }
      else
      {
         db->htMappedSize = db->shared->htShmSize;
      }
   }

   hashTable = db->hashTables->slots; //pointer to current hashtable in memory

   //remap database file (only necessary here in writethrough mode) if in the meanwhile another process added new data (key value pairs / hashtables) to the file
   if (db->dbMappedSize < db->shared->mappedDbSize)
   {
      db->mappedDb = mremap(db->mappedDb, db->dbMappedSize, db->shared->mappedDbSize, MREMAP_MAYMOVE);
      if (db->mappedDb == MAP_FAILED)
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING(__FUNCTION__); DLT_STRING(":mremap error: !"), DLT_STRING(strerror(errno)));
         return KISSDB_ERROR_IO;
      }
      db->dbMappedSize = db->shared->mappedDbSize;
   }


   for (i = 0; i < db->shared->htNum; ++i)
   {
      offset = hashTable[hash].offsetA;   //fileoffset to data in file
      if (offset >= KISSDB_HEADER_SIZE || offset < 0) //if a key with same hash is already in this slot or the same key must be overwritten
      {
         if( abs(offset) > db->dbMappedSize )
         {
            return KISSDB_ERROR_IO;
         }

         // if slot is marked as deleted, use this slot and negate the offset in order to reuse the existing data block
         if(offset < 0)
         {
            offset = -offset; //get original offset where data was deleted
            //printf("Overwriting slot for key: [%s] which was deleted before, offsetA: %d \n",key, offset);
            writeDualDataBlock(db, offset, i, key, klen, value, valueSize);
            hashTable[hash].offsetA = offset; //write the offset to the data in the memory-hashtable slot
            offset += sizeof(DataBlock_s);
            hashTable[hash].offsetB = offset; //write the offset to the second databloxk in the memory-hashtable slot
            hashTable[hash].current = 0x00;
            *(bytesWritten) = valueSize;

            return 0; /* success */
         }
         //overwrite existing if key matches
         offset = (hashTable[hash].current == 0x00) ? hashTable[hash].offsetA : hashTable[hash].offsetB; // if 0x00 -> offsetA is latest else offsetB is latest
         block = (DataBlock_s*) (db->mappedDb +  offset);
         kptr = (const uint8_t*) key;  //pointer to search key
         if (klen > 0)
         {
            if (memcmp(kptr, block->key, klen)
                  || strlen(block->key) != klen) //if search key does not match with key in file
            {
               //if key does not match -> search in next hashtable
               bKeyFound = Kdb_false;
            }
            else
            {
               bKeyFound = Kdb_true;
            }
         }
         if(Kdb_true == bKeyFound)
         {
            backupOffset = (hashTable[hash].current == 0x00) ? hashTable[hash].offsetB : hashTable[hash].offsetA; // if 0x00 -> offsetB is latest backup  else offsetA is latest

            //ALSO OVERWRITE LATEST VALID BLOCK to improve write amplification factor
            block->delimStart = (offset < backupOffset) ? DATA_BLOCK_A_START_DELIMITER : DATA_BLOCK_B_START_DELIMITER;
            block->valSize = valueSize;
            memcpy(block->value,value, block->valSize);
            block->htNum = i;
            crc = 0x00;
            crc = (uint32_t) pcoCrc32(crc, (unsigned char*)block->key, db->keySize + sizeof(uint32_t) + db->valSize + sizeof(uint64_t) );
            block->crc = crc;
            block->delimEnd = (offset < backupOffset) ? DATA_BLOCK_A_END_DELIMITER : DATA_BLOCK_B_END_DELIMITER;

            //if key matches -> seek to currently non valid data block for this key
            backupOffset = (hashTable[hash].current == 0x00) ? hashTable[hash].offsetB : hashTable[hash].offsetA; // if 0x00 -> offsetB is latest backup  else offsetA is latest
            backupBlock = (DataBlock_s*) (db->mappedDb +  backupOffset);
            //backupBlock->delimStart = DATA_BLOCK_START_DELIMITER;
            backupBlock->delimStart = (backupOffset < offset) ? DATA_BLOCK_A_START_DELIMITER : DATA_BLOCK_B_START_DELIMITER;
            backupBlock->valSize = valueSize;
            memcpy(backupBlock->value,value, backupBlock->valSize);
            backupBlock->htNum = i;
            crc = 0x00;
            crc = (uint32_t) pcoCrc32(crc, (unsigned char*)backupBlock->key, db->keySize + sizeof(uint32_t) + db->valSize + sizeof(uint64_t) );
            backupBlock->crc = crc;
            //backupBlock->delimEnd = DATA_BLOCK_END_DELIMITER;
            backupBlock->delimEnd = (backupOffset < offset) ? DATA_BLOCK_A_END_DELIMITER : DATA_BLOCK_B_END_DELIMITER;
            // check current flag and decide what parts of hashtable slot in file must be updated
            hashTable[hash].current = (hashTable[hash].current == 0x00) ? 0x01 : 0x00; // if 0x00 -> offsetA is latest -> set to 0x01 else /offsetB is latest -> modify settings of A set 0x00
            *(bytesWritten) = valueSize;

            return 0; //success
         }
      }
      else //if key is not already inserted
      {
         /* add new data if an empty hash table slot is discovered */
         endoffset = db->shared->mappedDbSize;
         if ( -1 == endoffset) //filepointer to the end of the file
         {
            return KISSDB_ERROR_IO;
         }

         //truncate file -> data + backup block
         if( ftruncate(db->fd, endoffset + ( sizeof(DataBlock_s) * 2) ) < 0)
         {
            return KISSDB_ERROR_IO;
         }

         db->mappedDb = mremap(db->mappedDb, db->dbMappedSize, db->shared->mappedDbSize + (sizeof(DataBlock_s) * 2), MREMAP_MAYMOVE);
         if (db->mappedDb == MAP_FAILED)
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING(__FUNCTION__); DLT_STRING(":mremap error: !"),DLT_STRING(strerror(errno)));
            return KISSDB_ERROR_IO;
         }

         db->shared->mappedDbSize = db->shared->mappedDbSize + (sizeof(DataBlock_s) * 2); //shared info about database file size
         db->dbMappedSize = db->shared->mappedDbSize; //local info about mapped size of file

         writeDualDataBlock(db, endoffset, i, key, klen, value, valueSize);

         //update hashtable entry
         offset = endoffset + sizeof(DataBlock_s);
         hashTable[hash].offsetA = endoffset; //write the offsetA to the data in the memory-hashtable slot
         hashTable[hash].offsetB = offset;    //write the offset to the data in the memory-hashtable slot
         hashTable[hash].current = 0x00;

         *(bytesWritten) = valueSize;
         return 0; /* success */
      }
      hashTable = (Hashtable_slot_s*) ((char*) hashTable + sizeof(Hashtable_s));  //pointer to the next memory-hashtable
   }

   //if new size would exceed old shared memory size for hashtables-> allocate additional memory to shared memory (+ db->htSizeBytes)
   if( (db->htSizeBytes * (db->shared->htNum + 1)) > db->shared->htShmSize)
   {
      //munlockall();
      if (db->htFd <= 0)
      {
         db->htFd = kdbShmemOpen(db->htName,  db->htMappedSize, &temp);
         if(db->htFd < 0)
         {
            return KISSDB_ERROR_OPEN_SHM;
         }
      }
      result = resizeKdbShmem(db->htFd, &db->hashTables, db->htMappedSize, db->htMappedSize + db->htSizeBytes);
      if (result == Kdb_false)
      {
         return KISSDB_ERROR_RESIZE_SHM;
      }
      else
      {
         db->shared->htShmSize = db->htMappedSize + db->htSizeBytes;
         db->htMappedSize = db->shared->htShmSize;
      }
      //mlockall(MCL_FUTURE);
   }

   /* if no existing slots, add a new page of hash table entries */
   endoffset = db->shared->mappedDbSize;
   if ( -1 == endoffset) //filepointer to the end of the file
   {
      return KISSDB_ERROR_IO;
   }
   //truncate file in order to save new hashtable + two Datablocks (this does not modify filedescriptor)
   if (ftruncate(db->fd, endoffset + db->htSizeBytes + (sizeof(DataBlock_s) * 2)) < 0)
   {
      return KISSDB_ERROR_IO;
   }

   db->mappedDb = mremap(db->mappedDb, db->dbMappedSize, db->shared->mappedDbSize + (db->htSizeBytes + (sizeof(DataBlock_s) * 2)), MREMAP_MAYMOVE);
   if (db->mappedDb == MAP_FAILED)
   {
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING(__FUNCTION__); DLT_STRING(":mremap error: !"),DLT_STRING(strerror(errno)));
      return KISSDB_ERROR_IO;
   }
   db->shared->mappedDbSize = db->shared->mappedDbSize + (db->htSizeBytes + (sizeof(DataBlock_s) * 2));
   db->dbMappedSize = db->shared->mappedDbSize;

   //prepare new hashtable in shared memory
   hashtable = &(db->hashTables[db->shared->htNum]);
   memset(hashtable, 0, db->htSizeBytes); //hashtable init
   hashtable->delimStart = HASHTABLE_START_DELIMITER;
   hashtable->delimEnd = HASHTABLE_END_DELIMITER;
   hashtable->crc = 0x00;
   hashTable = hashtable->slots; //pointer to the next memory-hashtable
   hashTable[hash].offsetA = endoffset + db->htSizeBytes; /* where new entry will go (behind the new Ht that gets written)*/
   hashTable[hash].offsetB = hashTable[hash].offsetA + sizeof(DataBlock_s);//write the offset to the data in the memory-hashtable slot
   hashTable[hash].current = 0x00;

   htptr = (Hashtable_s*) (db->mappedDb + endoffset);
   //copy hashtable in shared memory to mapped hashtable in file
   memcpy(htptr, hashtable, db->htSizeBytes);
   //write data behind new hashtable
   writeDualDataBlock(db, endoffset + db->htSizeBytes, db->shared->htNum, key, klen, value, valueSize);
   //if a hashtable exists, update link to new hashtable in previous hashtable
   if (db->shared->htNum)
   {
      db->hashTables[db->shared->htNum -1].slots[HASHTABLE_SLOT_COUNT].offsetA = endoffset; //update link to new hashtable in previous hashtable
   }
   ++db->shared->htNum;

   *(bytesWritten) = valueSize;

   return 0; /* success */
}



#if 0
/*
 * prints the offsets stored in the shared Hashtable
 */
void printSharedHashtable(KISSDB* db)
{
   unsigned long k=0;
   int i = 0;

   for(i =0 ; i< db->shared->htNum; i++)
   {
      for (k = 0; k <= db->htSize; k++)
      {
         printf("ht[%d] offsetA  [%lu]: %" PRId64 " \n",i, k, db->hashTables[i].slots[k].offsetA);
         printf("ht[%d] offsetB  [%lu]: %" PRId64 " \n",i, k, db->hashTables[i].slots[k].offsetB);
         printf("ht[%d] current  [%lu]: %" PRIu64 " \n",i, k, db->hashTables[i].slots[k].current);
      }
   }
}
#endif


void KISSDB_Iterator_init(KISSDB* db, KISSDB_Iterator* dbi)
{
   dbi->db = db;
   dbi->h_no = 0;  // number of read hashtables
   dbi->h_idx = 0; // index in current hashtable
}


int KISSDB_Iterator_next(KISSDB_Iterator* dbi, void* kbuf, void* vbuf)
{
   DataBlock_s* block;
   Hashtable_slot_s* ht;
   int64_t offset;

   if(dbi->db->htMappedSize < dbi->db->shared->htShmSize)
   {
      if ( Kdb_false == remapSharedHashtable(dbi->db->htFd, &dbi->db->hashTables, dbi->db->htMappedSize, dbi->db->shared->htShmSize))
      {
         return KISSDB_ERROR_RESIZE_SHM;
      }
      else
      {
         dbi->db->htMappedSize = dbi->db->shared->htShmSize;
      }
   }

   //remap database file if in the meanwhile another process added new data (key value pairs / hashtables) to the file
   if (dbi->db->dbMappedSize < dbi->db->shared->mappedDbSize)
   {
      dbi->db->mappedDb = mremap(dbi->db->mappedDb, dbi->db->dbMappedSize, dbi->db->shared->mappedDbSize, MREMAP_MAYMOVE);
      if (dbi->db->mappedDb == MAP_FAILED)
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING(__FUNCTION__); DLT_STRING(":mremap error: !"), DLT_STRING(strerror(errno)));
         return KISSDB_ERROR_IO;
      }
      dbi->db->dbMappedSize = dbi->db->shared->mappedDbSize;
   }

   if ((dbi->h_no < (dbi->db->shared->htNum)) && (dbi->h_idx < dbi->db->htSize))
   {
      ht = dbi->db->hashTables[dbi->h_no].slots; //pointer to first hashtable

      while ( !(ht[dbi->h_idx].offsetA || ht[dbi->h_idx].offsetB) ) //until a offset was found
      {
         if (++dbi->h_idx >= dbi->db->htSize)
         {
            dbi->h_idx = 0;
            if (++dbi->h_no >= (dbi->db->shared->htNum))
            {
               return 0;
            }
            else
            {
               ht = dbi->db->hashTables[dbi->h_no].slots;   //next hashtable
            }
         }
      }
      if(ht[dbi->h_idx].current == 0x00)
      {
         offset = ht[dbi->h_idx].offsetA;
      }
      else
      {
         offset = ht[dbi->h_idx].offsetB;
      }

      if( abs(offset) > dbi->db->dbMappedSize )
      {
         return KISSDB_ERROR_IO;
      }

      block = (DataBlock_s*) (dbi->db->mappedDb + offset);
      memcpy(kbuf,block->key, dbi->db->keySize);
      if (vbuf != NULL)
      {
         memcpy(vbuf, block->value, dbi->db->valSize);
      }
      if (++dbi->h_idx >= dbi->db->htSize)
      {
         dbi->h_idx = 0;
         ++dbi->h_no;
      }
      return 1;
   }
   return 0;
}




int readHeader(KISSDB* db, uint16_t* htSize, uint64_t* keySize, uint64_t* valSize)
{
   Header_s* ptr = 0;
   ptr = (Header_s*) db->mappedDb;

   if ((ptr->KdbV[0] != 'K') || (ptr->KdbV[1] != 'd') || (ptr->KdbV[2] != 'B') )
   {
      return KISSDB_ERROR_CORRUPT_DBFILE;
   }
   if( (ptr->KdbV[3] != KISSDB_MAJOR_VERSION) || (ptr->KdbV[4] != '.') || (ptr->KdbV[5] != KISSDB_MINOR_VERSION))
   {
      return KISSDB_ERROR_WRONG_DATABASE_VERSION;
   }
   if (!ptr->htSize)
   {
      return KISSDB_ERROR_CORRUPT_DBFILE;
   }
   (*htSize) = (uint16_t) ptr->htSize;
   if (!ptr->keySize)
   {
      return KISSDB_ERROR_CORRUPT_DBFILE;
   }
   (*keySize) = (uint64_t) ptr->keySize;

   if (!ptr->valSize)
   {
      return KISSDB_ERROR_CORRUPT_DBFILE;
   }
   (*valSize) = (uint64_t) ptr->valSize;
   return 0;
}




int writeHeader(KISSDB* db, uint16_t* htSize, uint64_t* keySize, uint64_t* valSize)
{
   Header_s* ptr = 0;
   int ret= 0;

   //truncate file to needed size for header
   ret = ftruncate(db->fd, KISSDB_HEADER_SIZE);
   if (ret < 0)
   {
      return KISSDB_ERROR_IO;
   }
   //mmap whole file for the first time
   db->mappedDb = (void*) mmap(NULL, KISSDB_HEADER_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, db->fd, 0);
   if (db->mappedDb == MAP_FAILED)
   {
      return KISSDB_ERROR_IO;
   }
   db->shared->mappedDbSize = KISSDB_HEADER_SIZE;
   db->dbMappedSize = KISSDB_HEADER_SIZE;

   ptr = (Header_s*) db->mappedDb;
   ptr->KdbV[0] = 'K';
   ptr->KdbV[1] = 'd';
   ptr->KdbV[2] = 'B';
   ptr->KdbV[3] = KISSDB_MAJOR_VERSION;
   ptr->KdbV[4] = '.';
   ptr->KdbV[5] = KISSDB_MINOR_VERSION;
   ptr->KdbV[6] = '0';
   ptr->KdbV[7] = '0';
   ptr->checksum = 0x00;
   ptr->closeFailed = 0x00; //remove closeFailed flag
   ptr->closeOk = 0x01;     //set closeOk flag
   ptr->htSize = (uint64_t)(*htSize);
   ptr->keySize = (uint64_t)(*keySize);
   ptr->valSize = (uint64_t)(*valSize);
   msync(db->mappedDb, KISSDB_HEADER_SIZE, MS_SYNC);

   return 0;
}


int checkErrorFlags(KISSDB* db)
{
   Header_s* ptr;
   ptr = (Header_s*) db->mappedDb;

   //check if closeFailed flag is set or closeOk is not set
   if(ptr->closeFailed == 0x01 || ptr->closeOk == 0x00 )
   {
#ifdef PFS_TEST
      printf("CHECK ERROR FLAGS: CLOSE FAILED!\n");
#endif
      ptr->closeFailed = 0x01; //create close failed flags
      ptr->closeOk = 0x00;
      return KISSDB_ERROR_CORRUPT_DBFILE;
   }
   else
   {
#ifdef PFS_TEST
      printf("CHECK ERROR FLAGS: CLOSE OK!\n");
#endif
      ptr->closeFailed = 0x01; //NO: create close failed flag
      ptr->closeOk = 0x00;
   }
   msync(db->mappedDb, KISSDB_HEADER_SIZE, MS_SYNC);

   return 0;
}


int verifyHashtableCS(KISSDB* db)
{
   char* ptr;
   Hashtable_s* hashtable;
   int i = 0;
   int ptrOffset=1;
   int64_t offset = 0;
   struct stat statBuf;
   uint64_t crc = 0;
   void* memory;

   if (db->fd)
   {
      //map entire file into memory
      fstat(db->fd, &statBuf);
      memory = (void*) mmap(NULL, statBuf.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, db->fd, 0);
      if (memory == MAP_FAILED)
      {
         return KISSDB_ERROR_IO;
      }
      ptr = (char*) memory;
      db->shared->htNum = 0;
      //unmap previously allocated and maybe corrupted hashtables
      munmap(db->hashTables, db->htMappedSize);
      db->hashTables = (Hashtable_s*) getKdbShmemPtr(db->htFd, db->htSizeBytes);
      if(db->hashTables == ((void*) -1))
      {
         return KISSDB_ERROR_MAP_SHM;
      }
      db->htMappedSize = db->htSizeBytes; //size for first hashtable

      //determine greatest common factor of hashtable and datablock used for pointer incrementation
      ptrOffset = greatestCommonFactor(sizeof(DataBlock_s), sizeof(Hashtable_s) );

      //offsets in mapped area to first hashtable
      offset = sizeof(Header_s);
      ptr += offset;

      //get number of hashtables in file (search for hashtable delimiters) and copy the hashtables to memory
      while (offset <= ((int64_t)statBuf.st_size - (int64_t)db->htSizeBytes)) //begin with offset for first hashtable
      {
         hashtable = (Hashtable_s*) ptr;
         //if at least one of two hashtable delimiters are found
         if (hashtable->delimStart == HASHTABLE_START_DELIMITER
               || hashtable->delimEnd == HASHTABLE_END_DELIMITER)
         {
               //next hashtable to use
               //rewrite delimiters to make sure that both exist
               hashtable->delimStart = HASHTABLE_START_DELIMITER;
               hashtable->delimEnd   = HASHTABLE_END_DELIMITER;
               Kdb_bool result = Kdb_false;
               //if new size would exceed old shared memory size-> allocate additional memory page to shared memory
               if (db->htSizeBytes * (db->shared->htNum + 1) > db->htMappedSize)
               {
                  result = resizeKdbShmem(db->htFd, &db->hashTables, db->htMappedSize, db->htMappedSize + db->htSizeBytes);
                  if (result == Kdb_false)
                  {
                     return KISSDB_ERROR_RESIZE_SHM;
                  }
                  else
                  {
                     db->shared->htShmSize = db->htMappedSize + db->htSizeBytes;
                     db->htMappedSize = db->shared->htShmSize;
                  }
               }
               // copy the current hashtable read from file to (htadress + (htsize  * htcount)) in memory
               memcpy(((uint8_t*) db->hashTables) + (db->htSizeBytes * db->shared->htNum), ptr, db->htSizeBytes);
               ++db->shared->htNum;

               //jump to next data block after hashtable
               offset += sizeof(Hashtable_s);
               ptr += sizeof(Hashtable_s);
         }
         else
         {
            offset += ptrOffset;
            ptr += ptrOffset;
         }
      }
      munmap(memory, statBuf.st_size);

      //check CRC of all found hashtables
      if (db->shared->htNum > 0)
      {
         for (i = 0; i < db->shared->htNum; i++)
         {
            crc = 0;
            crc = (uint64_t) pcoCrc32(crc, (unsigned char*) db->hashTables[i].slots, sizeof(db->hashTables[i].slots));
            if (db->hashTables[i].crc != crc)
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING(__FUNCTION__); DLT_STRING(": Checksum of hashtable number: <"); DLT_INT(i); DLT_STRING("> is invalid"));
#ifdef PFS_TEST
               printf("VERIFY HASHTABLE: hashtable #%d: CHECKSUM INVALID! \n",i);
#endif
               return -1; //start rebuild of hashtables
            }
            else
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG, DLT_STRING(__FUNCTION__); DLT_STRING(": Checksum of hashtable number: <"); DLT_INT(i); DLT_STRING("> is OK"));

#ifdef PFS_TEST
               printf("VERIFY HASHTABLE: hashtable #%d: CHECKSUM OK! \n",i);
#endif
            }
         }
      }
   }
   return 0;
}

int rebuildHashtables(KISSDB* db)
{
   char* ptr;
   DataBlock_s* data;
   DataBlock_s* dataA;
   DataBlock_s* dataB;
   Hashtable_s* hashtable;
   int current = 0;
   int ptrOffset = 1;
   int64_t offset=0;
   int64_t offsetA = 0;
   struct stat statBuf;
   uint64_t crc = 0;
   uint64_t calcCrcA, calcCrcB, readCrcA, readCrcB;
   void* memory;

   fstat(db->fd, &statBuf);
   memory = (char*) mmap(NULL, statBuf.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, db->fd, 0);
   if (memory == MAP_FAILED)
   {
      return KISSDB_ERROR_IO;
   }
   ptr = (char*) memory;

   //recover all hashtables of database
   if (db->shared->htNum > 0) //htNum was determined in verifyhashtables() -> no reallocation is needed
   {
      ptr = (void*) memory;
      memset(db->hashTables, 0, db->shared->htNum * sizeof(Hashtable_s));
      db->hashTables[0].delimStart = HASHTABLE_START_DELIMITER;
      db->hashTables[0].delimEnd = HASHTABLE_END_DELIMITER;

      ptrOffset = greatestCommonFactor(sizeof(DataBlock_s), sizeof(Hashtable_s) );

      //begin searching after first hashtable
      offset = sizeof(Header_s) + sizeof(Hashtable_s);
      ptr += offset;

      //go through  database file until offset + Datablock size reaches end of file mapping
      while (offset <= (statBuf.st_size - sizeof(DataBlock_s)))
      {
         data = (DataBlock_s*) ptr;

         //if block A start or end delimiters were found
         if (data->delimStart == DATA_BLOCK_A_START_DELIMITER
               || data->delimEnd == DATA_BLOCK_A_END_DELIMITER)
         {
            //calculate checksum of Block A
            calcCrcA = 0;
            calcCrcA = (uint64_t) pcoCrc32(calcCrcA, (unsigned char*) data->key, db->keySize + sizeof(uint32_t) + db->valSize + sizeof(uint64_t));
            readCrcA = data->crc;

            //search for block B start delimiter
            offset += sizeof(DataBlock_s);
            ptr += sizeof(DataBlock_s);
            dataB = (DataBlock_s*) ptr;
            if (dataB->delimStart == DATA_BLOCK_B_START_DELIMITER
                  || dataB->delimEnd == DATA_BLOCK_B_END_DELIMITER)
            {
               //verify checksum of Block B
               calcCrcB = 0;
               calcCrcB = (uint64_t) pcoCrc32(calcCrcB, (unsigned char*) dataB->key, db->keySize + sizeof(uint32_t) + db->valSize + sizeof(uint64_t));
               readCrcB = dataB->crc;
               if (readCrcB == calcCrcB) //checksum of block B matches
               {
                  if (readCrcA == calcCrcA) //checksum of block A matches
                  {
                     if (1) //decide which datablock has latest written data - still statically using Block B for recovery because both blocks are written in kissdb_put
                     {
                        offsetA = offset - sizeof(DataBlock_s);
                        rebuildWithBlockB(dataB, db, offsetA, offset);
                     }
                     else
                     {
                        // use block A for rebuild
                        //write offsets for block a and block B
                        offsetA = offset - sizeof(DataBlock_s);
                        rebuildWithBlockA(data, db, offsetA, offset);
                     }
                  }
                  else //checksum of block A does not match, but checksum of block B was valid
                  {
                     // use block B for rebuild
                     offsetA = offset - sizeof(DataBlock_s);
                     rebuildWithBlockB(dataB, db, offsetA, offset);
                  }
               }
               else
               {
                  if (readCrcA == calcCrcA)
                  {
                     // use block A for rebuild
                     //write offsets for block a and block B
                     offsetA = offset - sizeof(DataBlock_s);
                     rebuildWithBlockA(data, db, offsetA, offset);
                  }
                  else //checksum of block A and of Block B do not match ---> worst case scenario
                  {
                     invalidateBlocks(data, dataB, db);
                  }
               }
            }
            else
            {
               //if checksum A matches and block B was not found
               if (readCrcA == calcCrcA)
               {
                  // use block A for rebuild
                  //write offsets for block a and block B
                  offsetA = offset - sizeof(DataBlock_s);
                  rebuildWithBlockA(data, db, offsetA, offset);
               }
               else //checksum of block A does not match and block B was not found
               {
                  invalidateBlocks(data, dataB, db);
               }
            }
            //jump behind datablock B
            offset += sizeof(DataBlock_s);
            ptr += sizeof(DataBlock_s);
         }
         //If a Bock B start or end delimiters were found: this only can happen if previous Block A was not found
         else if (data->delimStart == DATA_BLOCK_B_START_DELIMITER
                    || data->delimEnd == DATA_BLOCK_B_END_DELIMITER)
         {
            dataA = (DataBlock_s*) (ptr - sizeof(DataBlock_s)) ;
            //verify checksum of Block B
            crc = 0;
            crc = (uint64_t) pcoCrc32(crc, (unsigned char*) data->key, db->keySize + sizeof(uint32_t) + db->valSize + sizeof(uint64_t));
            if (data->crc == crc)
            {
               //use block B for rebuild
               //write offsets for block A and block B
               offsetA = offset - sizeof(DataBlock_s);
               rebuildWithBlockB(data, db, offsetA, offset);
            }
            else
            {
               invalidateBlocks(dataA, data, db);
            }
            //jump behind datablock B
            offset += sizeof(DataBlock_s);
            ptr += sizeof(DataBlock_s);
         }
         else if (data->delimStart == DATA_BLOCK_A_DELETED_START_DELIMITER
                    || data->delimEnd == DATA_BLOCK_A_DELETED_END_DELIMITER)
         {
            //calculate checksum of Block A
            calcCrcA = 0;
            calcCrcA = (uint64_t) pcoCrc32(calcCrcA, (unsigned char*) data->key, db->keySize + sizeof(uint32_t) + db->valSize + sizeof(uint64_t));
            readCrcA = data->crc;

            //search for block B start delimiter
            offset += sizeof(DataBlock_s);
            ptr += sizeof(DataBlock_s);
            dataB = (DataBlock_s*) ptr;

            if (dataB->delimStart == DATA_BLOCK_B_DELETED_START_DELIMITER
                  || dataB->delimEnd == DATA_BLOCK_B_DELETED_END_DELIMITER)
            {
               //calculate checksum of Block B
               calcCrcB = 0;
               calcCrcB = (uint64_t) pcoCrc32(calcCrcB, (unsigned char*) dataB->key, db->keySize + sizeof(uint32_t) + db->valSize + sizeof(uint64_t));
               readCrcB = dataB->crc;
               if (readCrcB == calcCrcB) //checksum of block B matches
               {
                  offsetA = offset - sizeof(DataBlock_s);
                  invertBlockOffsets(data, db, offsetA, offset);
               }
               else
               {
                  if (readCrcA == calcCrcA)
                  {
                     offsetA = offset - sizeof(DataBlock_s);
                     invertBlockOffsets(data, db, offsetA, offset);
                  }
                  else
                  {
                     invalidateBlocks(data, dataB, db);
                  }
               }
            }
            else //NO BLOCK B Found
            {
               if (readCrcA == calcCrcA)
               {

                  offsetA = offset - sizeof(DataBlock_s);
                  invertBlockOffsets(data, db, offsetA, offset);
               }
               else
               {
                  invalidateBlocks(data, dataB, db);
               }
            }
            //jump behind datablock B
            offset += sizeof(DataBlock_s);
            ptr += sizeof(DataBlock_s);
         }
         else if (data->delimStart == DATA_BLOCK_B_DELETED_START_DELIMITER
                    || data->delimEnd == DATA_BLOCK_B_DELETED_END_DELIMITER)
         {
            crc = 0;
            crc = (uint64_t) pcoCrc32(crc, (unsigned char*) data->key, db->keySize + sizeof(uint32_t) + db->valSize + sizeof(uint64_t));
            if (crc == data->crc)
            {
               offsetA = offset - sizeof(DataBlock_s);
               invertBlockOffsets(data, db, offsetA, offset);
            }
            else
            {
               dataA = (DataBlock_s*) (ptr - sizeof(DataBlock_s)) ;
               invalidateBlocks(dataA, data, db);
            }
            //jump behind datablock B
            offset += sizeof(DataBlock_s);
            ptr += sizeof(DataBlock_s);
         }
         else if( offset <= (statBuf.st_size - sizeof(Hashtable_s)) ) //check if ptr range for hashtable is within mapping of file
         {
            hashtable = (Hashtable_s*) ptr;

            if (hashtable->delimStart == HASHTABLE_START_DELIMITER
                  || hashtable->delimEnd == HASHTABLE_END_DELIMITER)
            {
               //next hashtable to use
               db->hashTables[current].slots[db->htSize].offsetA = offset; //update link to next hashtable in current hashtable
               current++;
               if (current < db->shared->htNum)
               {
                  db->hashTables[current].delimStart = HASHTABLE_START_DELIMITER;
                  db->hashTables[current].delimEnd   = HASHTABLE_END_DELIMITER;
               }
               else
               {
                  return -1;
               }
               offset += sizeof(Hashtable_s);
               ptr += sizeof(Hashtable_s);
            }
            else //if no hashtable is found
            {
               offset += ptrOffset;
               ptr += ptrOffset;
            }
         }
         else //if nothing is found for offsets in -> (filesize - hashtablesize)   area
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING(__FUNCTION__); DLT_STRING(": No Datablock or hashtable area found!"));
            //increment pointer by greatest common factor of hashtable size and datablock size
            offset += ptrOffset;
            ptr += ptrOffset;
         }
      }
   }
   msync(memory, statBuf.st_size, MS_SYNC | MS_INVALIDATE);
   munmap(memory, statBuf.st_size);
   return 0;
}


//invalidate block content for A and B
//this block can never be found / overwritten again
//new insertions can reuse hashtable entry but block is added at EOF
void invalidateBlocks(DataBlock_s* dataA, DataBlock_s* dataB, KISSDB* db)
{
   DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__); DLT_STRING(": Datablock recovery for key: <"); DLT_STRING(dataA->key); DLT_STRING("> impossible: both datablocks are invalid!"));

   memset(dataA->key, 0, db->keySize);
   memset(dataA->value, 0, db->valSize);
   dataA->crc=0;
   dataA->htNum = 0;
   dataA->valSize = 0;

   memset(dataB->key, 0, db->keySize);
   memset(dataB->value, 0, db->valSize);
   dataB->crc=0;
   dataB->htNum = 0;
   dataB->valSize = 0;
}


void invertBlockOffsets(DataBlock_s* data, KISSDB* db, int64_t offsetA, int64_t offsetB)
{
   uint64_t hash = 0;
   hash = KISSDB_hash(data->key, strlen(data->key)) % (uint64_t) db->htSize;
   //invert offsets for deleted block A
   db->hashTables[data->htNum].slots[hash].offsetA = - offsetA;
   //invert offsets for deleted block B
   db->hashTables[data->htNum].slots[hash].offsetB = - offsetB;
   //reset current flag
   db->hashTables[data->htNum].slots[hash].current = 0x00;
}


void rebuildWithBlockB(DataBlock_s* data, KISSDB* db, int64_t offsetA, int64_t offsetB)
{
   uint64_t hash = KISSDB_hash(data->key, strlen(data->key)) % (uint64_t) db->htSize;
   //write offsets for block A and block B
   db->hashTables[data->htNum].slots[hash].offsetA = offsetA;
   db->hashTables[data->htNum].slots[hash].offsetB = offsetB;
   //set block B as current
   db->hashTables[data->htNum].slots[hash].current = 0x01;

   /*
   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG, DLT_STRING(__FUNCTION__); DLT_STRING(": Rebuild in hashtable No. <"); DLT_INT(data->htNum);
         DLT_STRING("> with Datablock B for key: <"); DLT_STRING(data->key); DLT_STRING("- hash: <"); DLT_INT(hash); DLT_STRING("> - OffsetA: <"); DLT_INT(offsetA);
         DLT_STRING("> - OffsetB: <"); DLT_INT(offsetB));
   */
}


void rebuildWithBlockA(DataBlock_s* data, KISSDB* db, int64_t offsetA, int64_t offsetB)
{
   uint64_t hash = KISSDB_hash(data->key, strlen(data->key)) % (uint64_t) db->htSize;
   //write offsets for block A and block B
   db->hashTables[data->htNum].slots[hash].offsetA = offsetA;
   db->hashTables[data->htNum].slots[hash].offsetB = offsetB;
   //set block B as current
   db->hashTables[data->htNum].slots[hash].current = 0x00;

   /*
   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG, DLT_STRING(__FUNCTION__); DLT_STRING(": Rebuild in hashtable No. <"); DLT_INT(data->htNum);
         DLT_STRING("> with Datablock A for key: <"); DLT_STRING(data->key); DLT_STRING("- hash: <"); DLT_INT(hash); DLT_STRING("> - OffsetA: <"); DLT_INT(offsetA);
         DLT_STRING("> - OffsetB: <"); DLT_INT(offsetB));
   */
}




int recoverDataBlocks(KISSDB* db)
{

#ifdef PFS_TEST
                  printf("DATABLOCK RECOVERY: START! \n");
#endif

   char* ptr;
   DataBlock_s* data;
   int i = 0;
   int k = 0;
   int64_t offset = 0;
   struct stat statBuf;
   uint64_t crc = 0;
   void* memory;

   fstat(db->fd, &statBuf);
   memory = (char*) mmap(NULL, statBuf.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, db->fd, 0);
   if (memory == MAP_FAILED)
   {
      return KISSDB_ERROR_IO;
   }
   ptr = (char*) memory;

   //go through all hashtables and jump to data blocks for crc validation
   if (db->shared->htNum > 0)
   {
      ptr = (void*) memory;
      for (i = 0; i < db->shared->htNum; i++)
      {
         for (k = 0; k < HASHTABLE_SLOT_COUNT; k++)
         {
            if (db->hashTables[i].slots[k].offsetA > 0) //ignore deleted or unused slots
            {
               ptr = (void*) memory;  //reset pointer
               //current valid data is offset A or offset B?
               offset = (db->hashTables[i].slots[k].current == 0x00) ? db->hashTables[i].slots[k].offsetA : db->hashTables[i].slots[k].offsetB;
               ptr += offset; //set pointer to current valid datablock
               data = (DataBlock_s*) ptr;
               //check crc of data block marked as current in hashtable
               crc = 0;
               crc = (uint64_t) pcoCrc32(crc, (unsigned char*) data->key, db->keySize + sizeof(uint32_t) + db->valSize + sizeof(uint64_t));
               if (data->crc != crc)
               {
                  DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN, DLT_STRING(__FUNCTION__); DLT_STRING(": Invalid datablock found at file offset: "); DLT_INT(offset));
#ifdef PFS_TEST
                  printf("DATABLOCK RECOVERY: INVALID CRC FOR CURRENT DATABLOCK DETECTED! \n");
#endif
                  ptr = (void*) memory;
                  //get offset to other data block and check crc
                  offset = (db->hashTables[i].slots[k].current == 0x00) ? db->hashTables[i].slots[k].offsetB : db->hashTables[i].slots[k].offsetA;
                  ptr += offset;
                  data = (DataBlock_s*) ptr;
                  crc = 0;
                  crc = (uint64_t) pcoCrc32(crc, (unsigned char*) data->key, db->keySize + sizeof(uint32_t) + db->valSize + sizeof(uint64_t));
                  if (data->crc == crc)
                  {
                     //switch current flag if valid backup is available
                     db->hashTables[i].slots[k].current = (db->hashTables[i].slots[k].current == 0x00) ? 0x01 : 0x00;
#ifdef PFS_TEST
                     printf("DATABLOCK RECOVERY: REPAIR OF INVALID DATA SUCCESSFUL! \n");
#endif
                     DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG, DLT_STRING(__FUNCTION__); DLT_STRING(": Invalid datablock for key: <"); DLT_STRING(data->key); DLT_STRING("> successfully recovered!"));
                  }
                  else
                  {
                     //invalidate data blocks if recovery fails
                     db->hashTables[i].slots[k].offsetA = - db->hashTables[i].slots[k].offsetA;
                     db->hashTables[i].slots[k].offsetB = - db->hashTables[i].slots[k].offsetB;
                     db->hashTables[i].slots[k].current = 0x00;
                     DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__); DLT_STRING(": Datablock recovery for key: <"); DLT_STRING(data->key); DLT_STRING("> impossible: both datablocks are invalid!"));
#ifdef PFS_TEST
                     printf("DATABLOCK RECOVERY: ERROR -> BOTH BLOCKS ARE INVALID! \n");
#endif
                  }
               }
            }
         }
      }
   }
   msync(memory, statBuf.st_size, MS_SYNC | MS_INVALIDATE);
   munmap(memory, statBuf.st_size);

#ifdef PFS_TEST
                  printf("DATABLOCK RECOVERY: END! \n");
#endif

   return 0;
}


int checkIsLink(const char* path, char* linkBuffer)
{
   char fileName[64] = { 0 };
   char truncPath[128] = { 0 };
   int isLink = 0;
   int len = 0;
   size_t strLen = 0;
   struct stat statBuf;
   uint16_t i = 0;

   memset(&statBuf, 0, sizeof(statBuf));
   strLen = strlen(path);
   for (i = 0; i < strLen; i++)
   {
      if (path[i] == '/')
      {
         len = i; // remember the position of the last '/'
      }
   }
   strncpy(truncPath, path, len);
   truncPath[len + 1] = '\0'; // set end of string
   strncpy(fileName, (const char*) path + len, 64);

   if (lstat(truncPath, &statBuf) != -1)
   {
      if (S_ISLNK(statBuf.st_mode))
      {
         if (readlink(truncPath, linkBuffer, 256) != -1)
         {
            strncat(linkBuffer, fileName, 256);
            isLink = 1;
         }
         else
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
                    DLT_STRING(__FUNCTION__); DLT_STRING(": readlink failed: "); DLT_STRING(strerror(errno)));
            isLink = -1;
         }
      }
      else
      {
         isLink = -1;
      }
   }
   else
   {
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN,
              DLT_STRING(__FUNCTION__); DLT_STRING(": lstat failed: "); DLT_STRING(strerror(errno)));
      isLink = -1;
   }
   return isLink;
}



void cleanKdbStruct(KISSDB* db)
{
   if (db->shared != NULL)
   {
      //Clean for every instance
      if (db->mappedDb != NULL)
      {
         munmap(db->mappedDb, db->dbMappedSize);
         db->mappedDb = NULL;
      }
      if (db->hashTables != NULL)
      {
         munmap(db->hashTables, db->htMappedSize);
         db->hashTables = NULL;
      }
      if(db->cacheName != NULL)
      {
         free(db->cacheName);
         db->cacheName = NULL;
      }
      if (db->fd)
      {
         close(db->fd);
         db->fd = 0;
      }
      if(db->sharedCacheFd)
      {
         close(db->sharedCacheFd);
         db->sharedCacheFd = -1;
      }
      db->alreadyOpen = Kdb_false;
      db->htSize = 0;
      //db->cacheReferenced = 0;
      db->keySize = 0;
      db->valSize = 0;
      db->htSizeBytes = 0;
      db->htMappedSize = 0;
      db->dbMappedSize = 0;
      db->shmCreator = 0;

      Kdb_unlock(&db->shared->rwlock);

      //Clean up for last instance referencing the database
      if (db->shared->refCount == 0)
      {
         //close shared hashtable memory
         if (db->htFd)
         {
            kdbShmemClose(db->htFd, db->htName);
            db->htFd = 0;
         }
         if(db->htName != NULL)
         {
            free(db->htName);
            db->htName = NULL;
         }
         //free rwlocks
         pthread_rwlock_destroy(&db->shared->rwlock);
         if (db->shared != NULL)
         {
            munmap(db->shared, sizeof(Shared_Data_s));
            db->shared = NULL;
         }
         if (db->sharedFd)
         {
            kdbShmemClose(db->sharedFd, db->sharedName);
            db->sharedFd = 0;
         }
         if(db->sharedName != NULL)
         {
            free(db->sharedName);
            db->sharedName = NULL;
         }

         //destroy and unlock named semaphore only if ref counter is zero
         if (-1 == sem_post(db->kdbSem)) //release semaphore
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__); DLT_STRING(": sem_post() in cleanup failed: "),
                  DLT_STRING(strerror(errno)));
         }
         if (-1 == sem_close(db->kdbSem))
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__); DLT_STRING(": sem_close() in cleanup failed: "),
                  DLT_STRING(strerror(errno)));
         }
         if (-1 == sem_unlink(db->semName))
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__); DLT_STRING(": sem_unlink() in cleanup failed: "),
                  DLT_STRING(strerror(errno)));
         }
         db->kdbSem = NULL;
      }
      else  //Clean up if other instances have reference to the database
      {
         if (db->sharedFd)
         {
            close(db->sharedFd);
            db->sharedFd = 0;
         }
         if (db->htFd)
         {
            close(db->htFd);
            db->htFd = 0;
         }
         if (db->shared != NULL)
         {
            munmap(db->shared, sizeof(Shared_Data_s));
            db->shared = NULL;
         }
         if(db->htName != NULL)
         {
            free(db->htName);
            db->htName = NULL;
         }
         if (-1 == sem_post(db->kdbSem))
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__); DLT_STRING(": sem_post() in cleanup with refcounter > 0 failed: "),
                  DLT_STRING(strerror(errno)));
         }
         if (-1 == sem_close(db->kdbSem))
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__); DLT_STRING(": sem_close() in cleanup with refcounter > 0 failed: "),
                    DLT_STRING(strerror(errno)));
         }
         db->kdbSem = NULL;
      }
   }
}


int writeDualDataBlock(KISSDB* db, int64_t offset, int htNumber, const void* key, unsigned long klen, const void* value, int valueSize)
{
   DataBlock_s* backupBlock;
   DataBlock_s* block;
   uint64_t crc = 0x00;

   block = (DataBlock_s*) (db->mappedDb + offset);
   block->delimStart = DATA_BLOCK_A_START_DELIMITER;
   memcpy(block->key,key, klen);
   block->valSize = valueSize;
   memcpy(block->value,value, block->valSize);
   block->htNum = htNumber;
   crc = 0x00;
   crc = (uint32_t) pcoCrc32(crc, (unsigned char*)block->key, db->keySize + sizeof(uint32_t) + db->valSize + sizeof(uint64_t)); //crc over key, datasize, data and htnum
   block->crc = crc;
   block->delimEnd = DATA_BLOCK_A_END_DELIMITER;

   // write same key and value again
   backupBlock = (DataBlock_s*) ((char*) block + sizeof(DataBlock_s));
   backupBlock->delimStart = DATA_BLOCK_B_START_DELIMITER;
   backupBlock->crc = crc;
   memcpy(backupBlock->key,key, klen);
   backupBlock->valSize = valueSize;
   memcpy(backupBlock->value,value, backupBlock->valSize);
   backupBlock->htNum = htNumber;
   backupBlock->delimEnd = DATA_BLOCK_B_END_DELIMITER;

   return 0;
}


int greatestCommonFactor(int x, int y)
{
   while (y != 0)
   {
      if (x > y)
      {
         x = x - y;
      }
      else
      {
         y = y - x;
      }
   }
   return x;
}
