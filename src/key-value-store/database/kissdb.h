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
#include "../hashtable/qlibc.h"

#ifdef __cplusplus
extern "C" {
#endif

//#define __showTimeMeasurements

#ifdef __showTimeMeasurements
#define SECONDS2NANO 1000000000L
#define NANO2MIL        1000000L
#define MIL2SEC            1000L
// define for the used clock: "CLOCK_MONOTONIC" or "CLOCK_REALTIME"
#define CLOCK_ID  CLOCK_MONOTONIC

#endif


/**
 * Version: 2
 *
 * This is the file format identifier, and changes any time the file
 * format changes. The code version will be this dot something, and can
 * be seen in tags in the git repository.
 */
#define KISSDB_VERSION 2

//boolean type
typedef int16_t Kdb_bool;
static const int16_t Kdb_true  = -1;
static const int16_t Kdb_false =  0;

typedef struct
{
      uint64_t shmem_size;
      uint16_t num_hash_tables;
      Kdb_bool cache_initialised;
      Kdb_bool crc_invalid;
      pthread_rwlock_t rwlock;
      pthread_rwlock_t cache_rwlock;
} Shared_Data_s;


/**
 * Header of the database file ->
 */
typedef struct
{
      char KdbV[8];
      uint64_t checksum; // checksum over database file
      uint64_t closeFailed;
      uint64_t closeOk;
      uint64_t hash_table_size;
      uint64_t key_size;
      uint64_t value_size;
      char delimiter[8];
} Header_s;



/**
 * Hashtable slot entry -> same size for all struct members because of alignment problems on target system!!
 */
typedef struct
{
      int64_t offsetA;
      uint64_t checksumA;
      int64_t offsetB;
      uint64_t checksumB;
      uint64_t current; //flag which offset points to the current data -> (if 0x00 offsetA points to current data, if 0x01 offsetB)
} Hashtable_slot_s;



/**
 * KISSDB database
 *
 * These fields should never be changed.
 */
typedef struct {
        uint16_t hash_table_size;
        uint64_t key_size;
        uint64_t value_size;
        uint64_t hash_table_size_bytes;
        uint64_t old_mapped_size;
        Kdb_bool shmem_creator;
        Kdb_bool already_open;
        Hashtable_slot_s *hash_tables; //shared: stores the hashtables
        void* shmem_cached; //shared: memory for key-value pair caching
        int shmem_info_fd;
        int shmem_ht_fd;
        int shmem_cached_fd;
        char* shmem_info_name;
        char* shmem_cached_name;
        char* shmem_ht_name;
        Shared_Data_s* shmem_info;
        qhasharr_t *tbl;   //reference to cached datastructure
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
 * Unable to unmap shared memory
 */
#define KISSDB_ERROR_UNMAP_SHM -6

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
 * @param mode One of the KISSDB_OPEN_MODE constants
 * @param hash_table_size Size of hash table in entries (must be >0)
 * @param key_size Size of keys in bytes
 * @param value_size Size of values in bytes
 * @return 0 on success, nonzero on error (see kissdb.h for error codes)
 */
extern int KISSDB_open(
	KISSDB *db,
	const char *path,
	int mode,
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
extern int KISSDB_get(KISSDB *db,const void *key,void *vbuf);



/**
 * delete an entry (offset in hashtable is set to 0 and record content is set to 0
 *
 * @param db Database struct
 * @param key Key (key_size bytes)
 * @return negative on error (see kissdb.h for error codes), 0 on success, 1 if key not found
 */
extern int KISSDB_delete(KISSDB *db,const void *key);

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
extern int KISSDB_put(KISSDB *db,const void *key,const void *value);

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
extern int readHeader(KISSDB* db, uint16_t* hash_table_size, uint64_t* key_size, uint64_t* value_size);
extern int writeHeader(KISSDB* db, uint16_t* hash_table_size, uint64_t* key_size, uint64_t* value_size);
extern int checkErrorFlags(KISSDB* db);

#if 0
extern void printSharedHashtable(KISSDB *db);
#endif

#ifdef __cplusplus
}
#endif

#endif

