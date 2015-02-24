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


#ifndef ___KISSDB_H
#define ___KISSDB_H

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include "../hashtable/qlibc.h"
#include "../inc/protected/persComDbAccess.h"

#ifdef __cplusplus
extern "C" {
#endif

/* #define __showTimeMeasurements */

#define DATA_BLOCK_A_START_DELIMITER 0x2AAAAAAA
#define DATA_BLOCK_A_END_DELIMITER   0x55555555

#define DATA_BLOCK_B_START_DELIMITER 0xE38E38E3
#define DATA_BLOCK_B_END_DELIMITER   0xAAAAAAA8

#define DATA_BLOCK_A_DELETED_START_DELIMITER 0xAAAAAAAA
#define DATA_BLOCK_A_DELETED_END_DELIMITER   0xD5555555

#define DATA_BLOCK_B_DELETED_START_DELIMITER 0x7E07E07E
#define DATA_BLOCK_B_DELETED_END_DELIMITER   0x81F81F81

#define HASHTABLE_START_DELIMITER 0x33333333
#define HASHTABLE_END_DELIMITER   0xCCCCCCCC

#define HASHTABLE_SLOT_COUNT 510

#ifdef __showTimeMeasurements
#define SECONDS2NANO 1000000000L
#define NANO2MIL        1000000L
#define MIL2SEC            1000L
/*  define for the used clock: "CLOCK_MONOTONIC" or "CLOCK_REALTIME" */
#define CLOCK_ID  CLOCK_MONOTONIC

#endif


/**
 * Version: 2.3
 *
 * This is the file format identifier, and changes any time the file
 * format changes.
 */
#define KISSDB_MAJOR_VERSION 2
#define KISSDB_MINOR_VERSION 3

typedef int16_t Kdb_bool;
static const int16_t Kdb_true  = -1;
static const int16_t Kdb_false =  0;

typedef struct
{
      uint64_t htShmSize; /* shared info about current size of hashtable shared memory */
      /*uint64_t cacheSize;*/
      /*uint16_t cacheCount;*/
      uint16_t htNum;
      uint16_t refCount;
      uint16_t openMode;
      uint16_t writeMode;
      Kdb_bool cacheCreated; /* flag to indicate if the shared cache was created */
      pthread_rwlock_t rwlock;
      uint64_t mappedDbSize; /* shared information about current mapped size of database file */
} Shared_Data_s;


/**
 * Header of the database file ->
 */
typedef struct
{
      char KdbV[8];
      uint64_t checksum; /* checksum over database file */
      uint64_t closeFailed;
      uint64_t closeOk;
      uint64_t htSize;
      uint64_t keySize;
      uint64_t valSize;
      char delimiter[8];
      char padding[4032]; /* TODO remove padding*/
} Header_s;

typedef struct
{
   int64_t  delimStart;
   uint64_t crc;
   char     key[PERS_DB_MAX_LENGTH_KEY_NAME];
   uint32_t valSize;
   char     value[PERS_DB_MAX_SIZE_KEY_DATA]; /* 8028, 12124, 16220 -- > PERS_DB_MAX_SIZE_KEY_DATA = (pagesize * n) - (PERS_DB_MAX_LENGTH_KEY_NAME + 36) */
   uint64_t htNum; /*index which hashtable stores the offset for this data block */
   int64_t  delimEnd;
} DataBlock_s;


/**
 * Hashtable slot entry -for usage with mmap -> 24 byte --> use 510 + 1 slots
 */
typedef struct
{
      int64_t offsetA;
      int64_t offsetB;
      uint64_t current; //flag which offset points to the current data -> (if 0x00 offsetA points to current data, if 0x01 offsetB)
} Hashtable_slot_s;


//hashtable structure (size is multiple of 4096 byte for usage in shared memory)
typedef struct
{
   int64_t delimStart;
   uint64_t crc;
   Hashtable_slot_s slots[HASHTABLE_SLOT_COUNT + 1]; //510 + 1 item (link to next hashtable) -> 12264 byte
   int64_t delimEnd;
} Hashtable_s; //12288 byte -> 3 pages of 4096 byte



/**
 * KISSDB database
 *
 * These fields should never be changed.
 */
typedef struct {
        uint16_t htSize;
        //uint16_t cacheReferenced;
        uint64_t keySize;
        uint64_t valSize;
        uint64_t htSizeBytes;
        uint64_t htMappedSize; //local info about currently mapped hashtable size for this process
        uint64_t dbMappedSize; //local info about currently mapped database  size for this process
        Kdb_bool shmCreator;   //local information if this instance is the creator of the shared memory
        Kdb_bool alreadyOpen;
        Hashtable_s* hashTables; //local pointer to hashtables in shared memory
        char* mappedDb; // local mapping of database file for every process
        void* sharedCache; //shared: memory for key-value pair caching
        int sharedFd;
        int htFd;
        int sharedCacheFd;
        char* semName;
        char* sharedName;
        char* cacheName;
        char* htName;
        Shared_Data_s* shared;
        qhasharr_t *tbl[1];   //reference to cache
        sem_t* kdbSem;
        int fd; //local fd
} KISSDB;


/**
 * I/O error or file not found
 */
#define KISSDB_ERROR_IO -1

/**
 * Out of memory
 */
#define KISSDB_ERROR_MALLOC -2

/**
 * Invalid paramters (e.g. missing _size paramters on init to create database)
 */
#define KISSDB_ERROR_INVALID_PARAMETERS -3

/**
 * Database file appears corrupt
 */
#define KISSDB_ERROR_CORRUPT_DBFILE -4

/**
 * Database file appears corrupt
 */
#define KISSDB_ERROR_ACCESS_VIOLATION -5

/**
 * Unable to open shared memory
 */
#define KISSDB_ERROR_OPEN_SHM -7

/**
 * Unable to remap shared memory
 */
#define KISSDB_ERROR_REMAP_SHM -8

/**
 * Unable to map shared memory
 */
#define KISSDB_ERROR_MAP_SHM -9

/**
 * Unable to resize shared memory
 */
#define KISSDB_ERROR_RESIZE_SHM -10

/**
 * Unable to close shared memory
 */
#define KISSDB_ERROR_CLOSE_SHM -11

/**
 * try to open database with wrong version
 */
#define KISSDB_ERROR_WRONG_DATABASE_VERSION -12

/**
 * buffer where data should be returned is too small
 */
#define KISSDB_ERROR_WRONG_BUFSIZE -13

/**
 * Open mode: read only
 */
#define KISSDB_OPEN_MODE_RDONLY 1

/**
 * Open mode: read/write
 */
#define KISSDB_OPEN_MODE_RDWR 2

/**
 * Open mode: read/write, create if doesn't exist
 */
#define KISSDB_OPEN_MODE_RWCREAT 3

/**
 * Open mode: truncate database, open for reading and writing
 */
#define KISSDB_OPEN_MODE_RWREPLACE 4

/**
 * Write mode: cache is not used, data is directly written (writeThrough mode)
 */
#define KISSDB_WRITE_MODE_WT 5

/**
 * Write mode: cache is used for database access
 */
#define KISSDB_WRITE_MODE_WC 6



/**
 * Open database
 *
 * The three _size parameters must be specified if the database could
 * be created or re-created. Otherwise an error will occur. If the
 * database already exists, these parameters are ignored and are read
 * from the database. You can check the struture afterwords to see what
 * they were.
 *
 * @param db Database struct
 * @param path Path to file
 * @param openMode One of the KISSDB_OPEN_MODE constants
 * @param writeMode One of the KISSDB_WRITE constants
 * @param hash_table_size Size of hash table in entries (must be >0)
 * @param key_size Size of keys in bytes
 * @param value_size Size of values in bytes
 * @return 0 on success, nonzero on error (see kissdb.h for error codes)
 */
extern int KISSDB_open(
	KISSDB *db,
	const char *path,
	int openMode,
	int writeMode,
        uint16_t hash_table_size,
	uint64_t key_size,
        uint64_t value_size);

/**
 * Close database
 *
 * @param db Database struct
 * @return negative on error (see kissdb.h for error codes), 0 on success
 */
extern int KISSDB_close(KISSDB *db);

/**
 * Get an entry
 *
 * @param db Database struct
 * @param key Key (key_size bytes)
 * @param vbuf Value buffer (value_size bytes capacity)
 * @return negative on error (see kissdb.h for error codes), 0 on success, 1 if key not found
 */
extern int KISSDB_get(KISSDB *db,const void *key,void *vbuf, uint32_t bufsize, uint32_t* vsize);



/**
 * delete an entry (offset in hashtable is set to 0 and record content is set to 0
 *
 * @param db Database struct
 * @param key Key (key_size bytes)
 * @return negative on error (see kissdb.h for error codes), 0 on success, 1 if key not found
 */
extern int KISSDB_delete(KISSDB *db,const void *key, int32_t* bytesDeleted);

/**
 * Put an entry (overwriting it if it already exists)
 *
 * In the already-exists case the size of the database file does not
 * change.
 *
 * @param db Database struct
 * @param key Key (key_size bytes)
 * @param value Value (value_size bytes)
 * @return negative on error (see kissdb.h for error codes) error, 0 on success
 */
extern int KISSDB_put(KISSDB *db,const void *key,const void *value, int valueSize, int32_t* bytesWritten);

/**
 * Cursor used for iterating over all entries in database
 */
typedef struct {
	KISSDB *db;
	unsigned long h_no;
	unsigned long h_idx;
} KISSDB_Iterator;

/**
 * Initialize an iterator
 *
 * @param db Database struct
 * @param i Iterator to initialize
 */
extern void KISSDB_Iterator_init(KISSDB *db,KISSDB_Iterator *dbi);

/**
 * Get the next entry
 *
 * The order of entries returned by iterator is undefined. It depends on
 * how keys hash.
 *
 * @param Database iterator
 * @param kbuf Buffer to fill with next key (key_size bytes)
 * @param vbuf Buffer to fill with next value (value_size bytes)
 * @return 0 if there are no more entries, negative on error, positive if kbuf/vbuf have been filled
 */
extern int KISSDB_Iterator_next(KISSDB_Iterator *dbi,void *kbuf,void *vbuf);
extern Kdb_bool freeKdbShmemPtr(void * shmem_ptr, size_t length);
extern void * getKdbShmemPtr(int shmem, size_t length);
extern Kdb_bool kdbShmemClose(int shmem, const char * shmName);
extern int kdbShmemOpen(const char * name, size_t length, Kdb_bool* shmCreator);
extern char * kdbGetShmName(const char * format, const char * path);
extern void Kdb_wrlock(pthread_rwlock_t * wrlock);
extern void Kdb_rdlock(pthread_rwlock_t * rdlock);
extern void Kdb_unlock(pthread_rwlock_t * lock);
extern int readHeader(KISSDB* db, uint16_t* htSize, uint64_t* keySize, uint64_t* valSize);
extern int writeHeader(KISSDB* db, uint16_t* htSize, uint64_t* keySize, uint64_t* valSize);
extern int writeDualDataBlock(KISSDB* db, int64_t offset, int htNumber, const void* key, unsigned long klen, const void* value, int valueSize);
extern int checkErrorFlags(KISSDB* db);
extern int verifyHashtableCS(KISSDB* db);
extern int rebuildHashtables(KISSDB* db);
extern int greatestCommonFactor(int x, int y);
extern void invalidateBlocks(DataBlock_s* dataA, DataBlock_s* dataB, KISSDB* db);
extern void invertBlockOffsets(DataBlock_s* data, KISSDB* db, int64_t offsetA, int64_t offsetB);
extern void rebuildWithBlockB(DataBlock_s* data, KISSDB* db, int64_t offsetA, int64_t offsetB);
extern void rebuildWithBlockA(DataBlock_s* data, KISSDB* db, int64_t offsetA, int64_t offsetB);
extern int recoverDataBlocks(KISSDB* db);
extern int checkIsLink(const char* path, char* linkBuffer);
extern void cleanKdbStruct(KISSDB* db);

#if 0
extern void printSharedHashtable(KISSDB *db);
#endif

#ifdef __cplusplus
}
#endif

#endif //___KISSDB_H

