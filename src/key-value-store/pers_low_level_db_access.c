/******************************************************************************
 * Project         Persistency
 * (c) copyright   2014
 * Company         XS Embedded GmbH
 *****************************************************************************/
/******************************************************************************
 * This Source Code Form is subject to the terms of the
 * Mozilla Public License, v. 2.0. If a  copy of the MPL was not distributed
 * with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 ******************************************************************************/
/**
 * @file           pers_low_level_db_access.c
 * @author         Simon Disch
 * @brief          Implementation of persComDbAccess.h
 * @see
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include "./database/kissdb.h"
#include "./hashtable/qlibc.h"
#include <inttypes.h>
#include "persComTypes.h"
#include "persComErrors.h"
#include "persComDataOrg.h"
#include "persComDbAccess.h"
#include "persComRct.h"
#include "pers_low_level_db_access_if.h"
#include <dlt.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/shm.h>


/* #define PFS_TEST */

/* L&T context */
#define LT_HDR                          "[persComLLDB]"
DltContext persComLldbDLTCtx;
DLT_DECLARE_CONTEXT   (persComLldbDLTCtx)



///// library constructor
//void pco_library_init(void) __attribute__((constructor));
//void pco_library_init() {
//    printf ("\n      pco_library_init() constructor \n");
//}
//
///// library destructor
//void pco_library_destroy(void) __attribute__((destructor));
//void pco_library_destroy() {
//    printf ("\n      pco_library_destroy() destructor \n");
//}


/* ---------------------- local definition  ---------------------------- */
/* max number of open handlers per process */
#define PERS_LLDB_NO_OF_STATIC_HANDLES 16
#define PERS_LLDB_MAX_STATIC_HANDLES (PERS_LLDB_NO_OF_STATIC_HANDLES-1)

#define PERS_STATUS_KEY_NOT_IN_CACHE             -10        /* /!< key not in cache */

typedef struct
{
   char m_data[PERS_DB_MAX_SIZE_KEY_DATA];
   uint32_t m_dataSize;
} Data_LocalDB_s;

typedef enum pers_lldb_cache_flag_e
{
   CachedDataDelete = 0, /* Resource-Configuration-Table */
   CachedDataWrite /* Local/Shared DB */
} pers_lldb_cache_flag_e;

typedef struct
{
   pers_lldb_cache_flag_e eFlag;
   int m_dataSize;
   char m_data[PERS_DB_MAX_SIZE_KEY_DATA];
} Data_Cached_s;

typedef struct
{
   pers_lldb_cache_flag_e eFlag;
   uint32_t m_dataSize;
   char m_data[sizeof(PersistenceConfigurationKey_s)];
} Data_Cached_RCT_s;

typedef struct
{
   bool_t bIsAssigned;
   sint_t dbHandler;
   pers_lldb_purpose_e ePurpose;
   KISSDB kissDb;
   str_t dbPathname[PERS_ORG_MAX_LENGTH_PATH_FILENAME];
} lldb_handler_s;

typedef struct lldb_handles_list_el_s_
{
   lldb_handler_s sHandle;
   struct lldb_handles_list_el_s_* pNext;
} lldb_handles_list_el_s;

typedef struct
{
   lldb_handler_s asStaticHandles[PERS_LLDB_NO_OF_STATIC_HANDLES]; /* static area should be enough for most of the processes*/
   lldb_handles_list_el_s* pListHead; /* for the processes with a large number of threads which use Persistency */
} lldb_handlers_s;

/* ---------------------- local variables  --------------------------------- */
static const char ListItemsSeparator = '\0';

/* shared by all the threads within a process */
static lldb_handlers_s g_sHandlers; // initialize to 0 and NULL
//static lldb_handlers_s g_sHandlers = { { { 0 } } };
static pthread_mutex_t g_mutexLldb = PTHREAD_MUTEX_INITIALIZER;

/* ---------------------- local macros  --------------------------------- */

/* ---------------------- local functions  --------------------------------- */
static sint_t DeleteDataFromKissDB(sint_t dbHandler, pconststr_t key);
static sint_t DeleteDataFromKissRCT(sint_t dbHandler, pconststr_t key);
static sint_t GetAllKeysFromKissLocalDB(sint_t dbHandler, pstr_t buffer, sint_t size);
static sint_t GetAllKeysFromKissRCT(sint_t dbHandler, pstr_t buffer, sint_t size);
static sint_t GetKeySizeFromKissLocalDB(sint_t dbHandler, pconststr_t key);
static sint_t GetDataFromKissLocalDB(sint_t dbHandler, pconststr_t key, pstr_t buffer_out, sint_t bufSize);
static sint_t GetDataFromKissRCT(sint_t dbHandler, pconststr_t key, PersistenceConfigurationKey_s* pConfig);
static sint_t SetDataInKissLocalDB(sint_t dbHandler, pconststr_t key, pconststr_t data, sint_t dataSize);
static sint_t SetDataInKissRCT(sint_t dbHandler, pconststr_t key, PersistenceConfigurationKey_s const* pConfig);
static sint_t writeBackKissDB(KISSDB* db, lldb_handler_s* pLldbHandler);
static sint_t writeBackKissRCT(KISSDB* db, lldb_handler_s* pLldbHandler);
static sint_t getListandSize(KISSDB* db, pstr_t buffer, sint_t size, bool_t bOnlySizeNeeded, pers_lldb_purpose_e purpose);
static sint_t putToCache(KISSDB* db, sint_t dataSize, char* metaKey, void* cachedData);
static sint_t deleteFromCache(KISSDB* db, char* metaKey);
static sint_t getFromCache(KISSDB* db, void* metaKey, void* readBuffer, sint_t bufsize, bool_t sizeOnly);
static sint_t getFromDatabaseFile(KISSDB* db, void* metaKey, void* readBuffer, sint_t bufsize);

/* access to resources shared by the threads within a process */
static bool_t lldb_handles_Lock(void);
static bool_t lldb_handles_Unlock(void);
static lldb_handler_s* lldb_handles_FindInUseHandle(sint_t dbHandler);
static lldb_handler_s* lldb_handles_FindAvailableHandle(void);
static void lldb_handles_InitHandle(lldb_handler_s* psHandle_inout, pers_lldb_purpose_e ePurpose, str_t const* dbPathname);
static bool_t lldb_handles_DeinitHandle(sint_t dbHandler);

static int createCache(KISSDB* db);
static int openCache(KISSDB* db);
//static int addCache(KISSDB* db);
static int closeCache(KISSDB* db);

/**
 * \open or create a key-value database
 * \note : DB type is identified from dbPathname (based on extension)
 *
 * \param dbPathname                    [in] absolute path to DB
 * \param ePurpose                      [in] see pers_lldb_purpose_e
 * \param bForceCreationIfNotPresent    [in] if true, the DB is created if it does not exist
 *
 * \return >=0 for success, negative value otherway (see pers_error_codes.h)
 */
sint_t pers_lldb_open(str_t const* dbPathname, pers_lldb_purpose_e ePurpose, bool_t bForceCreationIfNotPresent)
{
   bool_t bCanContinue = true;
   bool_t bLocked = false;
   char linkBuffer[256] = { 0 };
   const char* path;
   int error = 0;
   int kdbState = 0;
   int openMode  = KISSDB_OPEN_MODE_RDWR; //default is open existing in RDWR
   int writeMode = KISSDB_WRITE_MODE_WC;  //default is write cached
   lldb_handler_s* pLldbHandler = NIL;
   sint_t returnValue = PERS_COM_FAILURE;
   static bool_t bFirstCall = true;

   path = dbPathname;

   if (bFirstCall)
   {
      pid_t pid = getpid();
      str_t dltContextID[16]; /* should be at most 4 characters string, but colissions occure */

      /* set an error handler - the default one will cause the termination of the calling process */
      bFirstCall = false;
      /* init DLT */
      (void) snprintf(dltContextID, sizeof(dltContextID), "Pers_%04d", pid);
      DLT_REGISTER_CONTEXT(persComLldbDLTCtx, dltContextID, "PersCommonLLDB");
      //DLT_SET_APPLICATION_LL_TS_LIMIT(DLT_LOG_DEBUG, DLT_TRACE_STATUS_OFF);
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
              DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("register context PersCommonLLDB ContextID="); DLT_STRING(dltContextID));
   }

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING("Begin opening:"); DLT_STRING("<"); DLT_STRING(dbPathname); DLT_STRING(">, ");
           ((PersLldbPurpose_RCT == ePurpose) ? DLT_STRING("RCT, ") : DLT_STRING("DB, ")); ((true == bForceCreationIfNotPresent) ? DLT_STRING("forced, ") : DLT_STRING("unforced, ")));

   if (lldb_handles_Lock())
   {
      bLocked = true;
      pLldbHandler = lldb_handles_FindAvailableHandle();
      if (NIL == pLldbHandler)
      {
         bCanContinue = false;
         returnValue = PERS_COM_ERR_OUT_OF_MEMORY;
      }
   }
   else
   {
      bCanContinue = false;
   }
   if (bCanContinue)
   {
      size_t datasize = (PersLldbPurpose_RCT == ePurpose) ? sizeof(PersistenceConfigurationKey_s) :
                        PERS_DB_MAX_SIZE_KEY_DATA;
      size_t keysize = (PersLldbPurpose_RCT == ePurpose) ? PERS_RCT_MAX_LENGTH_RESOURCE_ID :
                       PERS_DB_MAX_LENGTH_KEY_NAME;

      if (bForceCreationIfNotPresent & (1 << 0)) //check bit 0 0x0 (open)  0x1 (create)
      {
         openMode = KISSDB_OPEN_MODE_RWCREAT; //bit 0 is set
      }
      if(bForceCreationIfNotPresent & (1 << 1)) //check bit 1
      {
         //bit 1 is set -> writeThrough mode 0x2 (open) 0x3 (create)
         writeMode = KISSDB_WRITE_MODE_WT;
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG, DLT_STRING(LT_HDR), DLT_STRING(__FUNCTION__), DLT_STRING("Opening in write through mode:"), DLT_STRING("<"),
                 DLT_STRING(dbPathname), DLT_STRING(">, "));
      }
      if( bForceCreationIfNotPresent & (1 << 2)) //check bit 2
      {
         openMode = KISSDB_OPEN_MODE_RDONLY; //bit 2 is set 0x4
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG, DLT_STRING(LT_HDR), DLT_STRING(__FUNCTION__), DLT_STRING("Opening in read only mode:"), DLT_STRING("<"),
                 DLT_STRING(dbPathname), DLT_STRING(">, "));
      }

      if (1 == checkIsLink(dbPathname, linkBuffer))
      {
         path = linkBuffer;
      }
      else
      {
         path = dbPathname;
      }

      //printKdb(&pLldbHandler->kissDb);

      if (pLldbHandler->kissDb.alreadyOpen == Kdb_false) //check if this instance has already opened the db before
      {
         pLldbHandler->kissDb.semName = kdbGetShmName("-sem", path);
         if (NULL == pLldbHandler->kissDb.semName)
         {
            return -1;
         }
         pLldbHandler->kissDb.kdbSem = sem_open(pLldbHandler->kissDb.semName, O_CREAT | O_EXCL, 0644, 1);
         error = errno; //store errno -> (errno could be modified by following DLT_LOG)
         if (pLldbHandler->kissDb.kdbSem == SEM_FAILED) //open failed
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
                    DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(": first sem_open() with mode O_CREAT | O_EXCL failed with: "); DLT_STRING(strerror(error)));
            if (error == EEXIST)
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
                       DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(": semaphore already exists: "); DLT_STRING(strerror(error)));
               //try to open existing semaphore
               pLldbHandler->kissDb.kdbSem = sem_open(pLldbHandler->kissDb.semName, O_CREAT, 0644, 0);
               error = errno;
               if (pLldbHandler->kissDb.kdbSem == SEM_FAILED) //open failed
               {
                  DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                          DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(": sem_open() for existing semaphore failed with error: "); DLT_STRING(strerror(error)));
                  return -1;
               }
            }
            else
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                       DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":sem_open() failed:"); DLT_STRING(strerror(error)));
               return -1;
            }
         }
      }
      if (-1 == sem_wait(pLldbHandler->kissDb.kdbSem))
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__); DLT_STRING(": sem_wait() in open failed: "),
                 DLT_STRING(strerror(errno)));
      }
      kdbState = KISSDB_open(&pLldbHandler->kissDb, path, openMode, writeMode, HASHTABLE_SLOT_COUNT, keysize, datasize);
      if (kdbState != 0)
      {
         if (kdbState == KISSDB_ERROR_WRONG_DATABASE_VERSION)
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                    DLT_STRING("KISSDB_open: "); DLT_STRING("<"); DLT_STRING(path); DLT_STRING(">, "); DLT_STRING("database to be opened has wrong version! retval=<"); DLT_INT(kdbState); DLT_STRING(">"));
         }
         else
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN,
                    DLT_STRING("KISSDB_open: "); DLT_STRING("<"); DLT_STRING(path); DLT_STRING(">, "); DLT_STRING("retval=<"); DLT_INT(kdbState); DLT_STRING(">"),
                    DLT_STRING(strerror(errno)));
         }
         bCanContinue = false;
      }
   }
   if (kdbState == 0)
   {
      pLldbHandler->kissDb.shared->refCount++; //increment reference to opened databases
      if (-1 == sem_post(pLldbHandler->kissDb.kdbSem)) //release semaphore
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__); DLT_STRING(": End of open -> sem_post() failed: "),DLT_STRING(strerror(errno)));
      }
   }
   else
   {
      cleanKdbStruct(&pLldbHandler->kissDb);

      //in case of cleanup failure for semaphores, release semaphore
      if (pLldbHandler->kissDb.kdbSem != NULL)
      {
         if (-1 == sem_post(pLldbHandler->kissDb.kdbSem)) //release semaphore
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__); DLT_STRING(": End of open -> sem_post() in cleanup failed: "),
                  DLT_STRING(strerror(errno)));
         }
      }

      if(pLldbHandler->kissDb.semName != NULL)
      {
         free(pLldbHandler->kissDb.semName);
         pLldbHandler->kissDb.semName = NULL;
      }
   }

   if (bCanContinue)
   {
      lldb_handles_InitHandle(pLldbHandler, ePurpose, path);
      returnValue = pLldbHandler->dbHandler;
   }
   else
   {
      /* clean up */
      returnValue = PERS_COM_FAILURE;
      (void) lldb_handles_DeinitHandle(pLldbHandler->dbHandler);
   }
   if (bLocked)
   {
      (void) lldb_handles_Unlock();
   }


   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG, DLT_STRING(LT_HDR), DLT_STRING(__FUNCTION__), DLT_STRING("End of open for:"), DLT_STRING("<"),
           DLT_STRING(dbPathname), DLT_STRING(">, "), ((PersLldbPurpose_RCT == ePurpose) ? DLT_STRING("RCT, ") : DLT_STRING("DB, ")),
           ((true == bForceCreationIfNotPresent) ? DLT_STRING("forced, ") : DLT_STRING("unforced, ")); DLT_STRING("retval=<"), DLT_INT(returnValue),
           DLT_STRING(">"));

   return returnValue;
}

/**
 * \brief close a key-value database
 * \note : DB type is identified from dbPathname (based on extension)
 *
 * \param handlerDB     [in] handler obtained with pers_lldb_open
 *
 * \return 0 for success, negative value otherway (see pers_error_codes.h)
 */
sint_t pers_lldb_close(sint_t handlerDB)
{
#ifdef PFS_TEST
   printf("START: pers_lldb_close for PID: %d \n", getpid());
#endif

   bool_t bLocked = false;
   int kdbState = 0;
   lldb_handler_s* pLldbHandler = NIL;
   sint_t returnValue = PERS_COM_SUCCESS;

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(handlerDB));

#ifdef __showTimeMeasurements
   long long duration = 0;
   long long KdbDuration = 0;
   long long writeDuration = 0;
   struct timespec writeStart, writeEnd, kdbStart, kdbEnd, writebackStart,
             writebackEnd;
   duration = 0;
   KdbDuration = 0;
   writeDuration = 0;
   clock_gettime(CLOCK_ID, &writeStart);
#endif

   if (handlerDB >= 0)
   {
      if (lldb_handles_Lock())
      {
         bLocked = true;
         pLldbHandler = lldb_handles_FindInUseHandle(handlerDB);

         if (NIL == pLldbHandler)
         {
            returnValue = PERS_COM_FAILURE;
         }
      }
   }
   else
   {
      returnValue = PERS_COM_ERR_INVALID_PARAM;
   }

   if (PERS_COM_SUCCESS == returnValue)
   {
      KISSDB* db = &pLldbHandler->kissDb;
      if (-1 == sem_wait(db->kdbSem))
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__); DLT_STRING(": sem_wait() in close failed: "),
                 DLT_STRING(strerror(errno)));
      }

      DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
              DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("Closing database <"); DLT_STRING(pLldbHandler->dbPathname); DLT_STRING(">"));

      Kdb_wrlock(&db->shared->rwlock);         //lock acces to shared status information

      if (db->shared->refCount > 0)
      {
         db->shared->refCount--;
      }
      if (db->shared->cacheCreated == Kdb_true)
      {
         if (db->shared->refCount == 0)
         {
            if (openCache(db) != 0)
            {
               Kdb_unlock(&db->shared->rwlock);
               return PERS_COM_FAILURE;
            }
#ifdef __showTimeMeasurements
            clock_gettime(CLOCK_ID, &writebackStart);
#endif

            if (db->shared->openMode != KISSDB_OPEN_MODE_RDONLY)
            {
#ifdef PFS_TEST
               printf("  START: writeback of %d slots\n", pLldbHandler->kissDb.tbl->data->usedslots);
#endif
               if (pLldbHandler->ePurpose == PersLldbPurpose_DB)  //write back to local database
               {
                  writeBackKissDB(&pLldbHandler->kissDb, pLldbHandler);
               }
               else
               {
                  if (pLldbHandler->ePurpose == PersLldbPurpose_RCT) //write back to RCT database
                  {
                     writeBackKissRCT(&pLldbHandler->kissDb, pLldbHandler);
                  }
               }
#ifdef PFS_TEST
               printf("  END: writeback \n");
#endif
            }

#ifdef __showTimeMeasurements
            clock_gettime(CLOCK_ID, &writebackEnd);
#endif
            //release reference object
            db->tbl[0]->free(db->tbl[0]);
            db->tbl[0] = NULL;
            if (closeCache(db) != 0)
            {
               Kdb_unlock(&db->shared->rwlock);
               return PERS_COM_FAILURE;
            }
            db->sharedCache = NULL;
            if(db->sharedCacheFd)
            {
               close(db->sharedCacheFd);
               db->sharedCacheFd = -1;
            }
         }
         else //not the last instance, just unmap shared cache and free the name
         {
            //release reference object
            db->tbl[0]->free(db->tbl[0]);
            db->tbl[0] = NULL;
            freeKdbShmemPtr(db->sharedCache, PERS_CACHE_MEMSIZE);
            db->sharedCache = NULL;
            if(db->sharedCacheFd)
            {
               close(db->sharedCacheFd);
               db->sharedCacheFd = -1;
            }
         }
      }
      //no cache exists
      Kdb_unlock(&db->shared->rwlock);

#ifdef __showTimeMeasurements
      clock_gettime(CLOCK_ID, &kdbStart);
#endif

      kdbState = KISSDB_close(&pLldbHandler->kissDb);

#ifdef __showTimeMeasurements
      clock_gettime(CLOCK_ID, &kdbEnd);
#endif

      if (kdbState == 0)
      {
         if (!lldb_handles_DeinitHandle(pLldbHandler->dbHandler))
         {
            returnValue = PERS_COM_FAILURE;
         }
      }
      else
      {
         switch (kdbState)
         {
            case KISSDB_ERROR_CLOSE_SHM:
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                       DLT_STRING("KISSDB_close: "); DLT_STRING("Could not close shared memory object, retval=<"); DLT_INT(kdbState); DLT_STRING(">"));
               break;
            }
            default:
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                       DLT_STRING("KISSDB_close: "); DLT_STRING("Could not close database, retval=<"); DLT_INT(kdbState); DLT_STRING(">"));
               break;
            }
         }
         returnValue = PERS_COM_FAILURE;
      }
   }

   if (bLocked)
   {
      (void) lldb_handles_Unlock();
   }

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("handlerDB="); DLT_INT(handlerDB); DLT_STRING(" retval=<"); DLT_INT(returnValue); DLT_STRING(">"));

#ifdef __showTimeMeasurements
   clock_gettime(CLOCK_ID, &writeEnd);
   writeDuration += getNsDuration(&writebackStart, &writebackEnd);
   printf("Writeback to flash duration for %s => %f ms\n",
          pLldbHandler->dbPathname, (double)((double)writeDuration/NANO2MIL));
   KdbDuration += getNsDuration(&kdbStart, &kdbEnd);
   printf("KISSDB_close duration for %s => %f ms\n", pLldbHandler->dbPathname,
          (double)((double)KdbDuration/NANO2MIL));
   duration += getNsDuration(&writeStart, &writeEnd);
   printf("Overall Close duration for %s => %f ms\n", pLldbHandler->dbPathname,
          (double)((double)duration/NANO2MIL));
#endif

#ifdef PFS_TEST
   printf("END: pers_lldb_close for PID: %d \n", getpid());
#endif

   return returnValue;
}

/**
 * \writeback cache of RCT key-value database
 * \return 0 for success, negative value otherway (see pers_error_codes.h)
 */
static sint_t writeBackKissRCT(KISSDB* db, lldb_handler_s* pLldbHandler)
{
   char* metaKey;
   char* ptr;
   int idx = 0;
   int kdbState = 0;
   int32_t bytesDeleted = 0;
   int32_t bytesWritten = 0;
   pers_lldb_cache_flag_e eFlag;
   qnobj_t obj;
   sint_t returnValue = PERS_COM_SUCCESS;

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("START writeback for RCT: "),
           DLT_STRING(pLldbHandler->dbPathname));

   setMemoryAddress(db->sharedCache, db->tbl[0]);

   while (db->tbl[0]->getnext(db->tbl[0], &obj, &idx) == true)
   {
      ptr = obj.data;
      eFlag = (pers_lldb_cache_flag_e) *(int*) ptr;
      ptr += 2 * (sizeof(int));
      metaKey = obj.name;

      //check how data should be persisted
      switch (eFlag)
      {
         case CachedDataDelete:  //data must be deleted from file
         {
            kdbState = KISSDB_delete(&pLldbHandler->kissDb, metaKey, &bytesDeleted);
            if (kdbState != 0)
            {
               if (kdbState == 1)
               {
                  DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN,
                          DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_delete: RCT key=<"); DLT_STRING(metaKey); DLT_STRING(">, "); DLT_STRING("not found in database file, retval=<"); DLT_INT(kdbState);
                          DLT_STRING(">"));
               }
               else
               {
                  DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                          DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_delete: RCT key=<"); DLT_STRING(metaKey); DLT_STRING(">, "); DLT_STRING("Error with retval=<"); DLT_INT(kdbState); DLT_STRING(">"));
               }
            }
            break;
         }
         case CachedDataWrite:   //data must be written to file
         {
            kdbState = KISSDB_put(&pLldbHandler->kissDb, metaKey, ptr, sizeof(PersistenceConfigurationKey_s), &bytesWritten);
            if (kdbState != 0)
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                       DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_put: RCT key=<"); DLT_STRING(metaKey); DLT_STRING(">, "); DLT_STRING("Writing back to file failed with retval=<");
                       DLT_INT(kdbState); DLT_STRING(">"));
            }
            break;
         }
         default:
            break;
      }
      free(obj.name);
      free(obj.data);
   }

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("END writeback for RCT: "),
           DLT_STRING(pLldbHandler->dbPathname));
   return returnValue;
}




/**
 * Write back the data in cache to database file
 * @param db
 * @param pLldbHandler
 * @return 0 for success, negative value otherway (see pers_error_codes.h)
 */
static sint_t writeBackKissDB(KISSDB* db, lldb_handler_s* pLldbHandler)
{
   char* metaKey;
   char* ptr;
   Data_LocalDB_s insert = {{0},0};
   int datasize = 0;
   int idx = 0;
   int kdbState = 0;
   int32_t bytesDeleted = 0;
   int32_t bytesWritten = 0;
   pers_lldb_cache_flag_e eFlag;
   qnobj_t obj;
   sint_t returnValue = PERS_COM_SUCCESS;

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("START writeback for DB: "),
           DLT_STRING(pLldbHandler->dbPathname));

   setMemoryAddress(db->sharedCache, db->tbl[0]);

   while (db->tbl[0]->getnext(db->tbl[0], &obj, &idx) == true)
   {
      //get flag and datasize
      ptr = obj.data;
      eFlag = (pers_lldb_cache_flag_e) *(int*) ptr;  //pointer in obj.data to eflag
      ptr += sizeof(int);
      datasize = *(int*) ptr; //pointer in obj.data to datasize
      ptr += sizeof(int);     //pointer in obj.data to data
      metaKey = obj.name;

      //check how data should be persisted
      switch (eFlag)
      {
         case CachedDataDelete:  //data must be deleted from file
         {
            //delete key-value pair from database file
            kdbState = KISSDB_delete(&pLldbHandler->kissDb, metaKey, &bytesDeleted);
            if (kdbState != 0)
            {
               if (kdbState == 1)
               {
                  DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN,
                          DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_delete: key=<"); DLT_STRING(metaKey); DLT_STRING(">, "); DLT_STRING("not found in database file, retval=<"); DLT_INT(kdbState);
                          DLT_STRING(">"));
               }
               else
               {
                  DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                          DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_delete: key=<"); DLT_STRING(metaKey); DLT_STRING(">, "); DLT_STRING("Error with retval=<"); DLT_INT(kdbState); DLT_STRING(">");
                          DLT_STRING("Error Message: "); DLT_STRING(strerror(errno)));
               }
            }
            break;
         }
         case CachedDataWrite:  //data must be written to file
         {
            (void) memcpy(insert.m_data, ptr, datasize);
            insert.m_dataSize = datasize;
            kdbState = KISSDB_put(&pLldbHandler->kissDb, metaKey, &insert, insert.m_dataSize, &bytesWritten); //store data followed by datasize
            if (kdbState != 0)
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                       DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_put: key=<"); DLT_STRING(metaKey); DLT_STRING(">, "); DLT_STRING("Writing back to file failed with retval=<");
                       DLT_INT(kdbState); DLT_STRING(">"); DLT_STRING("Error Message: "); DLT_STRING(strerror(errno)));
            }
            break;
         }
         default:
            break;
      }
      free(obj.name);
      free(obj.data);
   }

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("END writeback for DB: "),
           DLT_STRING(pLldbHandler->dbPathname));
   return returnValue;
}

/**
 * \brief write a key-value pair into database
 * \note : DB type is identified from dbPathname (based on extension)
 * \note : DB is created if it does not exist
 *
 * \param handlerDB     [in] handler obtained with pers_lldb_open
 * \param ePurpose      [in] see pers_lldb_purpose_e
 * \param key           [in] key's name
 * \param data          [in] buffer with key's data
 * \param dataSize      [in] size of key's data
 *
 * \return 0 for success, negative value otherway (see pers_error_codes.h)
 */
sint_t pers_lldb_write_key(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const* key, str_t const* data, sint_t dataSize)
{
   sint_t eErrorCode = PERS_COM_SUCCESS;

   switch (ePurpose)
   {
      case PersLldbPurpose_DB:
      {
         eErrorCode = SetDataInKissLocalDB(handlerDB, key, data, dataSize);
         break;
      }
      case PersLldbPurpose_RCT:
      {
         eErrorCode = SetDataInKissRCT(handlerDB, key, (PersistenceConfigurationKey_s const*) data);
         break;
      }
      default:
      {
         eErrorCode = PERS_COM_ERR_INVALID_PARAM;
         break;
      }
   }
   return eErrorCode;
}

/**
 * \brief read a key's value from database
 * \note : DB type is identified from dbPathname (based on extension)
 *
 * \param handlerDB         [in] handler obtained with pers_lldb_open
 * \param ePurpose          [in] see pers_lldb_purpose_e
 * \param key               [in] key's name
 * \param dataBuffer_out    [out]buffer where to return the read data
 * \param bufSize           [in] size of dataBuffer_out
 *
 * \return read size, or negative value in case of error (see pers_error_codes.h)
 */
sint_t pers_lldb_read_key(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const* key, pstr_t dataBuffer_out, sint_t bufSize)
{
   sint_t eErrorCode = PERS_COM_SUCCESS;

   switch (ePurpose)
   {
      case PersLldbPurpose_DB:
      {
         eErrorCode = GetDataFromKissLocalDB(handlerDB, key, dataBuffer_out, bufSize);
         break;
      }
      case PersLldbPurpose_RCT:
      {
         eErrorCode = GetDataFromKissRCT(handlerDB, key, (PersistenceConfigurationKey_s*) dataBuffer_out);
         break;
      }
      default:
      {
         eErrorCode = PERS_COM_ERR_INVALID_PARAM;
         break;
      }
   }
   return eErrorCode;
}

/**
 * \brief reads the size of a value that corresponds to a key
 * \note : DB type is identified from dbPathname (based on extension)
 *
 * \param handlerDB         [in] handler obtained with pers_lldb_open
 * \param ePurpose          [in] see pers_lldb_purpose_e
 * \param key               [in] key's name
 * \return size of the value corresponding to the key, or negative value in case of error (see pers_error_codes.h)
 */
sint_t pers_lldb_get_key_size(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const* key)
{
   sint_t eErrorCode = PERS_COM_SUCCESS;

   switch (ePurpose)
   {
      case PersLldbPurpose_DB:
      {
         eErrorCode = GetKeySizeFromKissLocalDB(handlerDB, key);
         break;
      }
      default:
      {
         eErrorCode = PERS_COM_ERR_INVALID_PARAM;
         break;
      }
   }

   return eErrorCode;
}

/**
 * \brief delete key from database
 * \note : DB type is identified from dbPathname (based on extension)
 *
 * \param handlerDB         [in] handler obtained with pers_lldb_open
 * \param ePurpose          [in] see pers_lldb_purpose_e
 * \param key               [in] key's name
 *
 * \return 0 for success, negative value otherway (see pers_error_codes.h)
 */
sint_t pers_lldb_delete_key(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const* key)
{
   sint_t eErrorCode = PERS_COM_SUCCESS;

   switch (ePurpose)
   {
      case PersLldbPurpose_DB:
      {
         eErrorCode = DeleteDataFromKissDB(handlerDB, key);
         break;
      }
      case PersLldbPurpose_RCT:
      {
         eErrorCode = DeleteDataFromKissRCT(handlerDB, key);
         break;
      }
      default:
      {
         eErrorCode = PERS_COM_ERR_INVALID_PARAM;
         break;
      }
   }
   return eErrorCode;
}

/**
 * \brief Find the buffer's size needed to accomodate the listing of keys' names in database
 * \note : DB type is identified from dbPathname (based on extension)
 *
 * \param handlerDB         [in] handler obtained with pers_lldb_open
 * \param ePurpose          [in] see pers_lldb_purpose_e
 *
 * \return needed size, or negative value in case of error (see pers_error_codes.h)
 */
sint_t pers_lldb_get_size_keys_list(sint_t handlerDB, pers_lldb_purpose_e ePurpose)
{
   sint_t eErrorCode = PERS_COM_SUCCESS;

   switch (ePurpose)
   {
      case PersLldbPurpose_DB:
      {
         eErrorCode = GetAllKeysFromKissLocalDB(handlerDB, NIL, 0);
         break;
      }
      case PersLldbPurpose_RCT:
      {
         eErrorCode = GetAllKeysFromKissRCT(handlerDB, NIL, 0);
         break;
      }
      default:
      {
         eErrorCode = PERS_COM_ERR_INVALID_PARAM;
         break;
      }
   }
   return eErrorCode;
}

/**
 * \brief List the keys' names in database
 * \note : DB type is identified from dbPathname (based on extension)
 * \note : keys are separated by '\0'
 *
 * \param handlerDB         [in] handler obtained with pers_lldb_open
 * \param ePurpose          [in] see pers_lldb_purpose_e
 * \param listingBuffer_out [out]buffer where to return the listing
 * \param bufSize           [in] size of listingBuffer_out
 *
 * \return listing size, or negative value in case of error (see pers_error_codes.h)
 */
sint_t pers_lldb_get_keys_list(sint_t handlerDB, pers_lldb_purpose_e ePurpose, pstr_t listingBuffer_out, sint_t bufSize)
{
   sint_t eErrorCode = PERS_COM_SUCCESS;

   switch (ePurpose)
   {
      case PersLldbPurpose_DB:
      {
         eErrorCode = GetAllKeysFromKissLocalDB(handlerDB, listingBuffer_out, bufSize);
         break;
      }
      case PersLldbPurpose_RCT:
      {
         eErrorCode = GetAllKeysFromKissRCT(handlerDB, listingBuffer_out, bufSize);
         break;
      }
      default:
      {
         eErrorCode = PERS_COM_ERR_INVALID_PARAM;
         break;
      }
   }
   return eErrorCode;
}

static sint_t DeleteDataFromKissDB(sint_t dbHandler, pconststr_t key)
{
   bool_t bCanContinue = true;
   bool_t bLocked = false;
   int kdbState = 0;
   lldb_handler_s* pLldbHandler = NIL;
   sint_t bytesDeleted = PERS_COM_FAILURE;

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("handlerDB="); DLT_INT(dbHandler); DLT_STRING("key=<"); DLT_STRING(key); DLT_STRING(">"));

   if ((dbHandler >= 0) && (NIL != key))
   {
      if (lldb_handles_Lock())
      {
         bLocked = true;
         pLldbHandler = lldb_handles_FindInUseHandle(dbHandler);
         if (NIL == pLldbHandler)
         {
            bCanContinue = false;
         }
      }
   }
   else
   {
      bCanContinue = false;
   }


   if (bCanContinue)
   {
      Kdb_wrlock(&pLldbHandler->kissDb.shared->rwlock);
      if ( KISSDB_WRITE_MODE_WC == pLldbHandler->kissDb.shared->writeMode)
      {
         bytesDeleted = deleteFromCache(&pLldbHandler->kissDb, (char*) key);
      }
      else //write through
      {

         kdbState = KISSDB_delete(&pLldbHandler->kissDb, key, &bytesDeleted);
         if (kdbState != 0)
         {
            if (kdbState == 1)
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN,
                       DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_delete: key=<"); DLT_STRING(key); DLT_STRING(">, "); DLT_STRING("not found in database file, retval=<"); DLT_INT(kdbState);
                       DLT_STRING(">"));
            }
            else
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                       DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_delete: key=<"); DLT_STRING(key); DLT_STRING(">, "); DLT_STRING("Error with retval=<"); DLT_INT(kdbState); DLT_STRING(">");
                       DLT_STRING("Error Message: "); DLT_STRING(strerror(errno)));
            }
         }
      }
      Kdb_unlock(&pLldbHandler->kissDb.shared->rwlock);
   }

   if (bLocked)
   {
      (void) lldb_handles_Unlock();
   }

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("handlerDB="); DLT_INT(dbHandler); DLT_STRING("key=<"); DLT_STRING(key); DLT_STRING(">, "); DLT_STRING("retval=<");
           DLT_INT(bytesDeleted); DLT_STRING(">"));

   return bytesDeleted;
}

static sint_t DeleteDataFromKissRCT(sint_t dbHandler, pconststr_t key)
{
   bool_t bCanContinue = true;
   bool_t bLocked = false;
   int kdbState = 0;
   lldb_handler_s* pLldbHandler = NIL;
   sint_t bytesDeleted = PERS_COM_FAILURE;

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("handlerDB="); DLT_INT(dbHandler); DLT_STRING("key=<"); DLT_STRING(key); DLT_STRING(">"));

   if ((dbHandler >= 0) && (NIL != key))
   {
      if (lldb_handles_Lock())
      {
         bLocked = true;
         pLldbHandler = lldb_handles_FindInUseHandle(dbHandler);
         if (NIL == pLldbHandler)
         {
            bCanContinue = false;
         }
      }
   }
   else
   {
      bCanContinue = false;
   }

   if (bCanContinue)
   {
      Kdb_wrlock(&pLldbHandler->kissDb.shared->rwlock);
      if ( KISSDB_WRITE_MODE_WC == pLldbHandler->kissDb.shared->writeMode)
      {
         bytesDeleted = deleteFromCache(&pLldbHandler->kissDb, (char*) key);
      }
      else //write through
      {
         kdbState = KISSDB_delete(&pLldbHandler->kissDb, key, &bytesDeleted);
         if (kdbState != 0)
         {
            if (kdbState == 1)
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN,
                       DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_delete: key=<"); DLT_STRING(key); DLT_STRING(">, "); DLT_STRING("not found in database file, retval=<"); DLT_INT(kdbState);
                       DLT_STRING(">"));
            }
            else
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                       DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_delete: key=<"); DLT_STRING(key); DLT_STRING(">, "); DLT_STRING("Error with retval=<"); DLT_INT(kdbState); DLT_STRING(">");
                       DLT_STRING("Error Message: "); DLT_STRING(strerror(errno)));
            }
         }
      }
      Kdb_unlock(&pLldbHandler->kissDb.shared->rwlock);
   }

   if (bLocked)
   {
      (void) lldb_handles_Unlock();
   }

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("handlerDB="); DLT_INT(dbHandler); DLT_STRING("key=<"); DLT_STRING(key); DLT_STRING(">, "); DLT_STRING("retval=<");
           DLT_INT(bytesDeleted); DLT_STRING(">"));

   return bytesDeleted;
}

static sint_t GetAllKeysFromKissLocalDB(sint_t dbHandler, pstr_t buffer, sint_t size)
{
   bool_t bCanContinue = true;
   bool_t bLocked = false;
   bool_t bOnlySizeNeeded = (NIL == buffer);
   lldb_handler_s* pLldbHandler = NIL;
   sint_t result = 0;

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
   DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("buffer="); DLT_UINT((uint_t)buffer); DLT_STRING("size="); DLT_INT(size));

   if (dbHandler >= 0)
   {
      if (lldb_handles_Lock())
      {
         bLocked = true;
         pLldbHandler = lldb_handles_FindInUseHandle(dbHandler);
         if (NIL == pLldbHandler)
         {
            bCanContinue = false;
            result = PERS_COM_ERR_INVALID_PARAM;
         }
         else
         {
            if (PersLldbPurpose_DB != pLldbHandler->ePurpose)
            {
               /* this would be very bad */
               bCanContinue = false;
               result = PERS_COM_FAILURE;
            }
            /* to not use DLT while mutex locked */
         }
      }
   }
   else
   {
      bCanContinue = false;
      result = PERS_COM_ERR_INVALID_PARAM;
   }

   if (bCanContinue)
   {
      if ((buffer != NIL) && (size > 0))
      {
         (void) memset(buffer, 0, (size_t) size);
      }

      Kdb_wrlock(&pLldbHandler->kissDb.shared->rwlock);
      result = getListandSize(&pLldbHandler->kissDb, buffer, size, bOnlySizeNeeded, PersLldbPurpose_DB);
      Kdb_unlock(&pLldbHandler->kissDb.shared->rwlock);
      if (result < 0)
      {
         result = PERS_COM_FAILURE;
      }
   }
   if (bLocked)
   {
      (void) lldb_handles_Unlock();
   }

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
   DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("retval=<"); DLT_INT(result); DLT_STRING(">"));
   return result;
}

static sint_t GetAllKeysFromKissRCT(sint_t dbHandler, pstr_t buffer, sint_t size)
{
   bool_t bCanContinue = true;
   bool_t bLocked = false;
   bool_t bOnlySizeNeeded = (NIL == buffer);
   lldb_handler_s* pLldbHandler = NIL;
   sint_t result = 0;

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
   DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("buffer="); DLT_UINT((uint_t)buffer); DLT_STRING("size="); DLT_INT(size));

   if (dbHandler >= 0)
   {
      if (lldb_handles_Lock())
      {
         bLocked = true;
         pLldbHandler = lldb_handles_FindInUseHandle(dbHandler);
         if (NIL == pLldbHandler)
         {
            bCanContinue = false;
            result = PERS_COM_ERR_INVALID_PARAM;
         }
         else
         {
            if (PersLldbPurpose_RCT != pLldbHandler->ePurpose)
            {
               /* this would be very bad */
               bCanContinue = false;
               result = PERS_COM_FAILURE;
            }
            /* to not use DLT while mutex locked */
         }
      }
   }
   else
   {
      bCanContinue = false;
      result = PERS_COM_ERR_INVALID_PARAM;
   }

   if (bCanContinue)
   {
      if ((buffer != NIL) && (size > 0))
      {
         (void) memset(buffer, 0, (size_t) size);
      }
      Kdb_wrlock(&pLldbHandler->kissDb.shared->rwlock);
      result = getListandSize(&pLldbHandler->kissDb, buffer, size, bOnlySizeNeeded, PersLldbPurpose_RCT);
      Kdb_unlock(&pLldbHandler->kissDb.shared->rwlock);
      if (result < 0)
      {
         result = PERS_COM_FAILURE;
      }
   }
   if (bLocked)
   {
      (void) lldb_handles_Unlock();
   }

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
   DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("retval=<"); DLT_INT(result); DLT_STRING(">"));

   return result;
}

static sint_t SetDataInKissLocalDB(sint_t dbHandler, pconststr_t key, pconststr_t data, sint_t dataSize)
{
   bool_t bCanContinue = true;
   bool_t bLocked = false;
   Data_Cached_s dataCached = { 0 };
   int kdbState = 0;
   lldb_handler_s* pLldbHandler = NIL;
   sint_t bytesWritten = PERS_COM_FAILURE;


   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<"); DLT_STRING(key); DLT_STRING(">, "); DLT_STRING("size<");
           DLT_INT(dataSize); DLT_STRING(">"));

   if ((dbHandler >= 0) && (NIL != key) && (NIL != data) && (dataSize > 0))
   {
      if (lldb_handles_Lock())
      {
         bLocked = true;
         pLldbHandler = lldb_handles_FindInUseHandle(dbHandler);
         if (NIL == pLldbHandler)
         {
            bCanContinue = false;
            bytesWritten = PERS_COM_ERR_INVALID_PARAM;
         }
         else
         {
            if (PersLldbPurpose_DB != pLldbHandler->ePurpose)
            {
               /* this would be very bad */
               bCanContinue = false;
               bytesWritten = PERS_COM_FAILURE;
            }
         }
      }
   }
   else
   {
      bCanContinue = false;
      bytesWritten = PERS_COM_ERR_INVALID_PARAM;
   }

   if (bCanContinue)
   {
      char* metaKey = (char*) key;
      dataCached.eFlag = CachedDataWrite;
      dataCached.m_dataSize = dataSize;
      (void) memcpy(dataCached.m_data, data, (size_t) dataSize);

      Kdb_wrlock(&pLldbHandler->kissDb.shared->rwlock);
      if ( KISSDB_WRITE_MODE_WC == pLldbHandler->kissDb.shared->writeMode)
      {
         bytesWritten = putToCache(&pLldbHandler->kissDb, dataSize, (char*) metaKey, &dataCached);
      }
      else
      {
         if (KISSDB_OPEN_MODE_RDONLY != pLldbHandler->kissDb.shared->openMode)
         {
            kdbState = KISSDB_put(&pLldbHandler->kissDb, metaKey, dataCached.m_data, dataCached.m_dataSize, &bytesWritten);
            if (kdbState != 0)
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                     DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_put: key=<"); DLT_STRING(metaKey); DLT_STRING(">, "); DLT_STRING("WriteThrough to file failed with retval=<"); DLT_INT(bytesWritten); DLT_STRING(">"));
            }
         }
      }
      Kdb_unlock(&pLldbHandler->kissDb.shared->rwlock);
   }

   if (bLocked)
   {
      (void) lldb_handles_Unlock();
   }

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<"); DLT_STRING(key); DLT_STRING(">, "); DLT_STRING("size<");
           DLT_INT(dataSize); DLT_STRING(">, "); DLT_STRING("retval=<"); DLT_INT(bytesWritten); DLT_STRING(">"));

   return bytesWritten;
}

static sint_t SetDataInKissRCT(sint_t dbHandler, pconststr_t key, PersistenceConfigurationKey_s const* pConfig)
{
   bool_t bCanContinue = true;
   bool_t bLocked = false;
   Data_Cached_RCT_s dataCached = { 0 };
   int kdbState = 0;
   lldb_handler_s* pLldbHandler = NIL;
   sint_t bytesWritten = PERS_COM_FAILURE;

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<"); DLT_STRING(key); DLT_STRING(">"));

   if ((dbHandler >= 0) && (NIL != key) && (NIL != pConfig))
   {
      if (lldb_handles_Lock())
      {
         bLocked = true;
         pLldbHandler = lldb_handles_FindInUseHandle(dbHandler);
         if (NIL == pLldbHandler)
         {
            bCanContinue = false;
            bytesWritten = PERS_COM_ERR_INVALID_PARAM;
         }
         else
         {
            if (PersLldbPurpose_RCT != pLldbHandler->ePurpose)
            {
               /* this would be very bad */
               bCanContinue = false;
               bytesWritten = PERS_COM_FAILURE;
            }
            /* to not use DLT while mutex locked */
         }
      }
   }
   else
   {
      bCanContinue = false;
      bytesWritten = PERS_COM_ERR_INVALID_PARAM;
   }
   if (bCanContinue)
   {
      int dataSize = sizeof(PersistenceConfigurationKey_s);
      char* metaKey = (char*) key;
      dataCached.eFlag = CachedDataWrite;
      dataCached.m_dataSize = dataSize;
      (void) memcpy(dataCached.m_data, pConfig, (size_t) dataSize);


      Kdb_wrlock(&pLldbHandler->kissDb.shared->rwlock);
      if ( KISSDB_WRITE_MODE_WC == pLldbHandler->kissDb.shared->writeMode)
      {
         bytesWritten = putToCache(&pLldbHandler->kissDb, dataSize, (char*) metaKey, &dataCached);
      }
      else
      {

         if (KISSDB_OPEN_MODE_RDONLY != pLldbHandler->kissDb.shared->openMode)
         {
            kdbState = KISSDB_put(&pLldbHandler->kissDb, metaKey, dataCached.m_data, dataCached.m_dataSize, &bytesWritten);
            if (kdbState != 0)
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                     DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_put: RCT key=<"); DLT_STRING(metaKey); DLT_STRING(">, "); DLT_STRING("WriteThrough to file failed with retval=<"); DLT_INT(bytesWritten); DLT_STRING(">"));
            }
         }
      }
      Kdb_unlock(&pLldbHandler->kissDb.shared->rwlock);

   }
   if (bLocked)
   {
      (void) lldb_handles_Unlock();
   }

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<"); DLT_STRING(key); DLT_STRING(">, "); DLT_STRING("retval=<");
           DLT_INT(bytesWritten); DLT_STRING(">"));

   return bytesWritten;
}

static sint_t GetKeySizeFromKissLocalDB(sint_t dbHandler, pconststr_t key)
{
   bool_t bCanContinue = true;
   bool_t bLocked = false;
   lldb_handler_s* pLldbHandler = NIL;
   sint_t bytesRead = PERS_COM_FAILURE;

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<"); DLT_STRING(key); DLT_STRING(">"));

   if ((dbHandler >= 0) && (NIL != key))
   {
      if (lldb_handles_Lock())
      {
         bLocked = true;
         pLldbHandler = lldb_handles_FindInUseHandle(dbHandler);
         if (NIL == pLldbHandler)
         {
            bCanContinue = false;
            bytesRead = PERS_COM_ERR_INVALID_PARAM;
         }
         else
         {
            if (PersLldbPurpose_DB != pLldbHandler->ePurpose)
            {
               /* this would be very bad */
               bCanContinue = false;
               bytesRead = PERS_COM_FAILURE;
            }
         }
      }
   }
   else
   {
      bCanContinue = false;
      bytesRead = PERS_COM_ERR_INVALID_PARAM;
   }
   if (bCanContinue)
   {
      Kdb_wrlock(&pLldbHandler->kissDb.shared->rwlock);
      if ( KISSDB_WRITE_MODE_WC == pLldbHandler->kissDb.shared->writeMode)
      {
         bytesRead = getFromCache(&pLldbHandler->kissDb, (char*) key, NULL, 0, true);
         if (bytesRead == PERS_STATUS_KEY_NOT_IN_CACHE)
         {
            bytesRead = getFromDatabaseFile(&pLldbHandler->kissDb, (char*) key, NULL, 0);
         }
      }
      else
      {
         bytesRead = getFromDatabaseFile(&pLldbHandler->kissDb, (char*) key, NULL, 0);
      }
      Kdb_unlock(&pLldbHandler->kissDb.shared->rwlock);
   }
   if (bLocked)
   {
      (void) lldb_handles_Unlock();
   }
   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<"); DLT_STRING(key); DLT_STRING(">, "); DLT_STRING("retval=<");
           DLT_INT(bytesRead); DLT_STRING(">"));

   return bytesRead;
}

/* return no of bytes read, or negative value in case of error */
static sint_t GetDataFromKissLocalDB(sint_t dbHandler, pconststr_t key, pstr_t buffer_out, sint_t bufSize)
{
   bool_t bCanContinue = true;
   bool_t bLocked = false;
   lldb_handler_s* pLldbHandler = NIL;
   sint_t bytesRead = PERS_COM_FAILURE;

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<"); DLT_STRING(key); DLT_STRING(">, "); DLT_STRING("bufsize=<");
           DLT_INT(bufSize); DLT_STRING(">"));

   if ((dbHandler >= 0) && (NIL != key) && (NIL != buffer_out) && (bufSize > 0))
   {
      if (lldb_handles_Lock())
      {
         bLocked = true;
         pLldbHandler = lldb_handles_FindInUseHandle(dbHandler);
         if (NIL == pLldbHandler)
         {
            bCanContinue = false;
            bytesRead = PERS_COM_ERR_INVALID_PARAM;
         }
         else
         {
            if (PersLldbPurpose_DB != pLldbHandler->ePurpose)
            {
               /* this would be very bad */
               bCanContinue = false;
               bytesRead = PERS_COM_FAILURE;
            }
            /* to not use DLT while mutex locked */
         }
      }
   }
   else
   {
      bCanContinue = false;
      bytesRead = PERS_COM_ERR_INVALID_PARAM;
   }

   if (bCanContinue)
   {
      Kdb_wrlock(&pLldbHandler->kissDb.shared->rwlock);
      if ( KISSDB_WRITE_MODE_WC == pLldbHandler->kissDb.shared->writeMode)
      {
         bytesRead = getFromCache(&pLldbHandler->kissDb, (char*) key, buffer_out, bufSize, false);
         //if key is not already in cache
         if (bytesRead == PERS_STATUS_KEY_NOT_IN_CACHE)
         {
            bytesRead = getFromDatabaseFile(&pLldbHandler->kissDb, (char*) key, buffer_out, bufSize);
         }
      }
      else //write through mode -> only read from file
      {
         bytesRead = getFromDatabaseFile(&pLldbHandler->kissDb, (char*) key, buffer_out, bufSize);
      }
      Kdb_unlock(&pLldbHandler->kissDb.shared->rwlock);
   }
   if (bLocked)
   {
      (void) lldb_handles_Unlock();
   }

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<"); DLT_STRING(key); DLT_STRING(">, "); DLT_STRING("bufsize=<");
           DLT_INT(bufSize); DLT_STRING(">, "); DLT_STRING("retval=<"); DLT_INT(bytesRead); DLT_STRING(">"));
   return bytesRead;
}

static sint_t GetDataFromKissRCT(sint_t dbHandler, pconststr_t key, PersistenceConfigurationKey_s* pConfig)
{
   bool_t bCanContinue = true;
   bool_t bLocked = false;
   lldb_handler_s* pLldbHandler = NIL;
   sint_t bytesRead = PERS_COM_FAILURE;

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<"); DLT_STRING(key); DLT_STRING(">"));

   if ((dbHandler >= 0) && (NIL != key) && (NIL != pConfig))
   {
      if (lldb_handles_Lock())
      {
         bLocked = true;
         pLldbHandler = lldb_handles_FindInUseHandle(dbHandler);
         if (NIL == pLldbHandler)
         {
            bCanContinue = false;
            bytesRead = PERS_COM_ERR_INVALID_PARAM;
         }
         else
         {
            if (PersLldbPurpose_RCT != pLldbHandler->ePurpose)
            {
               /* this would be very bad */
               bCanContinue = false;
               bytesRead = PERS_COM_FAILURE;
            }
            /* to not use DLT while mutex locked */
         }
      }
   }
   else
   {
      bCanContinue = false;
      bytesRead = PERS_COM_ERR_INVALID_PARAM;
   }

   //read RCT
   if (bCanContinue)
   {
      Kdb_wrlock(&pLldbHandler->kissDb.shared->rwlock);
      if ( KISSDB_WRITE_MODE_WC == pLldbHandler->kissDb.shared->writeMode)
      {
         bytesRead = getFromCache(&pLldbHandler->kissDb, (char*) key, pConfig, sizeof(PersistenceConfigurationKey_s), false);
         if (bytesRead == PERS_STATUS_KEY_NOT_IN_CACHE)
         {
            bytesRead = getFromDatabaseFile(&pLldbHandler->kissDb, (char*) key, pConfig, sizeof(PersistenceConfigurationKey_s));
         }
      }
      else
      {
         bytesRead = getFromDatabaseFile(&pLldbHandler->kissDb, (char*) key, pConfig, sizeof(PersistenceConfigurationKey_s));
      }
      Kdb_unlock(&pLldbHandler->kissDb.shared->rwlock);
   }
   if (bLocked)
   {
      (void) lldb_handles_Unlock();
   }

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<"); DLT_STRING(key); DLT_STRING(">, "); DLT_STRING("retval=<");
           DLT_INT(bytesRead); DLT_STRING(">"));

   return bytesRead;
}

static bool_t lldb_handles_Lock(void)
{
   bool_t bEverythingOK = true;
   sint_t siErr = pthread_mutex_lock(&g_mutexLldb);
   if (0 != siErr)
   {
      bEverythingOK = false;
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
              DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("pthread_mutex_lock failed with error=<"); DLT_INT(siErr); DLT_STRING(">"));
   }
   return bEverythingOK;
}

static bool_t lldb_handles_Unlock(void)
{
   bool_t bEverythingOK = true;

   sint_t siErr = pthread_mutex_unlock(&g_mutexLldb);
   if (0 != siErr)
   {
      bEverythingOK = false;
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
              DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("pthread_mutex_unlock failed with error=<"); DLT_INT(siErr); DLT_STRING(">"));
   }
   return bEverythingOK;
}

/* it is assumed dbHandler is checked by the caller */
static lldb_handler_s* lldb_handles_FindInUseHandle(sint_t dbHandler)
{
   lldb_handler_s* pHandler = NIL;

   if (dbHandler <= PERS_LLDB_MAX_STATIC_HANDLES)
   {
      if (g_sHandlers.asStaticHandles[dbHandler].bIsAssigned)
      {
         pHandler = &g_sHandlers.asStaticHandles[dbHandler];
      }
   }
   else
   {
      lldb_handles_list_el_s* pListElemCurrent = g_sHandlers.pListHead;
      while (NIL != pListElemCurrent)
      {
         if (dbHandler == pListElemCurrent->sHandle.dbHandler)
         {
            pHandler = &pListElemCurrent->sHandle;
            break;
         }
         pListElemCurrent = pListElemCurrent->pNext;
      }
   }

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING((NIL!=pHandler) ? "Found handler <" : "ERROR can't find handler <"); DLT_INT(dbHandler); DLT_STRING(">");
           DLT_STRING((NIL!=pHandler) ? (dbHandler <= PERS_LLDB_MAX_STATIC_HANDLES ? "in static area" : "in dynamic list") : ""));

   return pHandler;
}

static lldb_handler_s* lldb_handles_FindAvailableHandle(void)
{
   bool_t bCanContinue = true;
   lldb_handler_s* pHandler = NIL;
   lldb_handles_list_el_s* psListElemNew = NIL;

   /* first try to find an available handle in the static area */
   sint_t siIndex = 0;
   for (siIndex = 0; siIndex <= PERS_LLDB_MAX_STATIC_HANDLES; siIndex++)
   {
      if (!g_sHandlers.asStaticHandles[siIndex].bIsAssigned)
      {
         //INITIALIZE KISSDB struct
         /* index setting should be done only once at the initialization of the static array
          * Anyway, doing it here is more consistent  */
         g_sHandlers.asStaticHandles[siIndex].dbHandler = siIndex;
         pHandler = &g_sHandlers.asStaticHandles[siIndex];
         break;
      }
   }

   if (NIL == pHandler)
   {
      /* no position available in the static array => we have to use the list
       * allocate memory for the new element and process the situation when the list is headless */

      psListElemNew = (lldb_handles_list_el_s*) malloc(sizeof(lldb_handles_list_el_s));
      memset(psListElemNew, 0, sizeof(lldb_handles_list_el_s));
      if (NIL == psListElemNew)
      {
         bCanContinue = false;
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("malloc failed"));
      }
      else
      {
         if (NIL == g_sHandlers.pListHead)
         {
            /* the list not yet used/created, so use the new created element as the head */
            g_sHandlers.pListHead = psListElemNew;
            g_sHandlers.pListHead->pNext = NIL;
            g_sHandlers.pListHead->sHandle.dbHandler = PERS_LLDB_MAX_STATIC_HANDLES + 1;
            /* the rest of the members will be set by lldb_handles_InitHandle */
            pHandler = &psListElemNew->sHandle;
         }
      }
   }
   if ((NIL == pHandler) && bCanContinue)
   {
      /* no position available in the static array => we have to use the list
       * the memory for psListElemNew has been allocated and the list has a head
       * The new element has to get the smallest index
       * Now lets consider the situation when the head of the list has an index higher than (PERS_LLDB_MAX_STATIC_HANDLES + 1)
       * => the list will have a new head !!! */
      if (g_sHandlers.pListHead->sHandle.dbHandler > (PERS_LLDB_MAX_STATIC_HANDLES + 1))
      {
         psListElemNew->pNext = g_sHandlers.pListHead;
         psListElemNew->sHandle.dbHandler = PERS_LLDB_MAX_STATIC_HANDLES + 1;
         /* the rest of the members will be set by lldb_handles_InitHandle */
         g_sHandlers.pListHead = psListElemNew;
         pHandler = &psListElemNew->sHandle;
      }
   }

   if ((NIL == pHandler) && bCanContinue)
   {
      /* no position available in the static array => we have to use the list
       * the memory for psListElemNew has been allocated and the list has a head (with the smallest index)
       * The new element has to get the smallest available index
       * So will search for the first gap between two consecutive elements of the list and will introduce the new element between */
      lldb_handles_list_el_s* pListElemCurrent1 = g_sHandlers.pListHead;
      lldb_handles_list_el_s* pListElemCurrent2 = pListElemCurrent1->pNext;
      while (NIL != pListElemCurrent2)
      {
         if (pListElemCurrent2->sHandle.dbHandler - pListElemCurrent1->sHandle.dbHandler > 1)
         {
            /* found a gap => insert the element between and use the index next to pListElemCurrent1's */
            psListElemNew->pNext = pListElemCurrent2;
            psListElemNew->sHandle.dbHandler = pListElemCurrent1->sHandle.dbHandler + 1;
            pListElemCurrent1->pNext = psListElemNew;
            pHandler = &psListElemNew->sHandle;
            break;
         }
         else
         {
            pListElemCurrent1 = pListElemCurrent2;
            pListElemCurrent2 = pListElemCurrent2->pNext;
         }
      }
      if (NIL == pListElemCurrent2)
      {
         /* reached the end of the list => the list will have a new end */
         psListElemNew->pNext = NIL;
         psListElemNew->sHandle.dbHandler = pListElemCurrent1->sHandle.dbHandler + 1;
         pListElemCurrent1->pNext = psListElemNew;
         pHandler = &psListElemNew->sHandle;
      }
   }

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING((NIL!=pHandler) ? "Found availble handler <" : "ERROR can't find available handler <");
           DLT_INT((NIL!=pHandler) ? pHandler->dbHandler : (-1)); DLT_STRING(">");
           DLT_STRING((NIL!=pHandler) ? (pHandler->dbHandler <= PERS_LLDB_MAX_STATIC_HANDLES ? "in static area" : "in dynamic list") : ""));

   return pHandler;
}

static void lldb_handles_InitHandle(lldb_handler_s* psHandle_inout, pers_lldb_purpose_e ePurpose, str_t const* dbPathname)
{
   psHandle_inout->bIsAssigned = true;
   psHandle_inout->ePurpose = ePurpose;
   (void) strncpy(psHandle_inout->dbPathname, dbPathname, sizeof(psHandle_inout->dbPathname));
}

static bool_t lldb_handles_DeinitHandle(sint_t dbHandler)
{
   bool_t bEverythingOK = true;
   bool_t bHandlerFound = false;

   if (dbHandler <= PERS_LLDB_MAX_STATIC_HANDLES)
   {
      bHandlerFound = true;
      g_sHandlers.asStaticHandles[dbHandler].bIsAssigned = false;
   }
   else
   {
      /* consider the situation when the handle is the head of the list */
      if (NIL != g_sHandlers.pListHead)
      {
         if (dbHandler == g_sHandlers.pListHead->sHandle.dbHandler)
         {
            lldb_handles_list_el_s* pListElemTemp = NIL;
            bHandlerFound = true;
            pListElemTemp = g_sHandlers.pListHead;
            g_sHandlers.pListHead = g_sHandlers.pListHead->pNext;
            free(pListElemTemp);
         }
      }
      else
      {
         bEverythingOK = false;
      }
   }

   if (bEverythingOK && (!bHandlerFound))
   {
      /* consider the situation when the handle is in the list (but not the head) */
      lldb_handles_list_el_s* pListElemCurrent1 = g_sHandlers.pListHead;
      lldb_handles_list_el_s* pListElemCurrent2 = pListElemCurrent1->pNext;
      while (NIL != pListElemCurrent2)
      {
         if (dbHandler == pListElemCurrent2->sHandle.dbHandler)
         {
            /* found the handle */
            bHandlerFound = true;
            pListElemCurrent1->pNext = pListElemCurrent2->pNext;
            free(pListElemCurrent2);
            break;
         }
         else
         {
            pListElemCurrent1 = pListElemCurrent2;
            pListElemCurrent2 = pListElemCurrent2->pNext;
         }
      }
      if (NIL == pListElemCurrent2)
      {
         /* reached the end of the list without finding the handle */
         bEverythingOK = false;
      }
   }

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler=<"); DLT_INT(dbHandler); DLT_STRING("> ");
           DLT_STRING(bEverythingOK ? (dbHandler <= PERS_LLDB_MAX_STATIC_HANDLES ? "deinit handler in static area" : "deinit handler in dynamic list") : "ERROR - handler not found"));

   return bEverythingOK;
}

sint_t getFromCache(KISSDB* db, void* metaKey, void* readBuffer, sint_t bufsize, bool_t sizeOnly)
{
   char* ptr;
   int datasize = 0;
   Kdb_bool cacheEmpty, keyDeleted, keyNotFound;
   pers_lldb_cache_flag_e eFlag;
   sint_t bytesRead = 0;
   size_t size = 0;
   void* val;

   keyDeleted = cacheEmpty = keyNotFound = Kdb_false;

   //if cache already created
   if (db->shared->cacheCreated == Kdb_true)
   {
      if (openCache(db) != 0)
      {
         return PERS_COM_FAILURE;
      }

      setMemoryAddress(db->sharedCache, db->tbl[0]);

      val = db->tbl[0]->get(db->tbl[0], metaKey, &size);
      if (val == NULL)
      {
         bytesRead = PERS_COM_ERR_NOT_FOUND;
         keyNotFound = Kdb_true;
      }
      else
      {
         ptr = val;
         eFlag = (pers_lldb_cache_flag_e) *(int*) ptr;

         //check if this key has already been marked as deleted
         if (eFlag != CachedDataDelete)
         {
            //get datasize
            ptr = ptr + sizeof(pers_lldb_cache_flag_e);
            datasize = *(int*) ptr;
            ptr = ptr + sizeof(int); //move pointer to beginning of data
            bytesRead = datasize;

            //get data if needed
            if (!sizeOnly)
            {
               if (bufsize < datasize)
               {
                  return PERS_COM_FAILURE;
               }
               else
               {
                  (void) memcpy(readBuffer, ptr, datasize);
               }
            }
         }
         else
         {
            bytesRead = PERS_COM_ERR_NOT_FOUND;
            keyDeleted = Kdb_true;
         }
         free(val);
      }
   }
   else
   {
      cacheEmpty = Kdb_true;
   }

   //only read from file if key was not found in cache and if key was not marked as deleted in cache
   if ((cacheEmpty == Kdb_true && keyDeleted == Kdb_false) || keyNotFound == Kdb_true)
   {
      return PERS_STATUS_KEY_NOT_IN_CACHE; //key not found in cache
   }
   else
   {
      return bytesRead;
   }
}

sint_t getFromDatabaseFile(KISSDB* db, void* metaKey, void* readBuffer, sint_t bufsize)
{
   int kdbState = 0;
   sint_t bytesRead = 0;
   uint32_t size = 0;

   kdbState  = KISSDB_get(db, metaKey, readBuffer, bufsize, &size);
   if (kdbState == 0)
   {
      bytesRead = size;
   }
   else
   {
      if (kdbState == 1)
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN,
                 DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_get: key=<"); DLT_STRING(metaKey); DLT_STRING(">, "); DLT_STRING("not found, retval=<"); DLT_INT(kdbState); DLT_STRING(">"));
         bytesRead = PERS_COM_ERR_NOT_FOUND;
      }
      else
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                 DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_get: key=<"); DLT_STRING(metaKey); DLT_STRING(">, "); DLT_STRING("Error with retval=<"); DLT_INT(kdbState); DLT_STRING(">"));
      }
      bytesRead = PERS_COM_ERR_NOT_FOUND;
   }
   return bytesRead;
}

sint_t putToCache(KISSDB* db, sint_t dataSize, char* metaKey, void* cachedData)
{
   sint_t bytesWritten = 0;

   //DO NOT ALLOW WRITING TO CACHE IF DATABASE IS OPENED IN READONLY MODE
   if(KISSDB_OPEN_MODE_RDONLY == db->shared->openMode )
   {
      return PERS_COM_ERR_READONLY;
   }

   //if cache not already created
   if (db->shared->cacheCreated == Kdb_false)
   {
      if (createCache(db) != 0)
      {
         return PERS_COM_FAILURE;
      }
   }
   else //open cache
   {
      if (openCache(db) != 0)
      {
         return PERS_COM_FAILURE;
      }
   }
   // update db->sharedCache pointer (process adress range mapping can be different after remap) (use
   //printf("setMemoryAddress(db->sharedCache, db->tbl[0] = %p \n", db->sharedCache );
   setMemoryAddress(db->sharedCache, db->tbl[0]); //address to first hashtable
   //put in cache
   if (db->tbl[0]->put(db->tbl[0], metaKey, cachedData, sizeof(pers_lldb_cache_flag_e) + sizeof(int) + (size_t) dataSize) ==
         false) //store flag , datasize and data as value in cache
   {
      bytesWritten = PERS_COM_FAILURE;
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("Failed to put data into cache: "); DLT_STRING(strerror(errno)));

#if 0
      int k = 0;
      //if additional caches were created by another process -> get referemnce to them and init the local tbl struct
      if(db->cacheReferenced < db->shared->cacheCount)
      {
         printf("additional caches found! \n");
         //init local tbl2 array (internally a malloc occurs)
         for(k=1; k < db->shared->cacheCount; k++ )
         {
            printf("qhasharr init cache no.= %d with zero \n",k);
            db->tbl[k] = qhasharr(db->sharedCache + (k * PERS_CACHE_MEMSIZE) , 0);
            db->cacheReferenced++;
         }
      }

      //reset addresses for all caches mapped to this process space
      for(k=0; k <  db->shared->cacheCount; k++ )
      {
         printf("setMemoryAddress  k= %d --  putToCache ptr: %p \n", k, db->sharedCache + (k * PERS_CACHE_MEMSIZE));
         setMemoryAddress(db->sharedCache + (k * PERS_CACHE_MEMSIZE ), db->tbl[k]);
      }

      int putOk = 0;
      k=0;
      //try to insert data until empty slot or same key is found in all of the caches
      while (k < db->shared->cacheCount) //store flag , datasize and data as value in cache
      {
         printf("start while k = %d, cacheCount= %d\n", k, db->shared->cacheCount);
         if( db->tbl[k]->put(db->tbl[k], metaKey, cachedData, sizeof(pers_lldb_cache_flag_e) + sizeof(int) + (size_t) dataSize) == true)
         {
            putOk = 1;
            printf("INSERT OK \n");
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
                                          DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("INSERT INTO Cache Nr: "); DLT_INT(k); DLT_STRING("worked : "));
            break;
         }
         else
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                              DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("INSERT INTO Cache Nr: "); DLT_INT(k); DLT_STRING("failed : ");  DLT_STRING(strerror(errno)));
         }
         printf("end while k = %d \n", k);
         k++;
      }



      //if all caches are full -> add new cache and insert data here
      if (putOk == 0)
      {
         printf("data->maxslots 1 = %d \n", db->tbl[0]->data->maxslots);
         printf("start adding new cache \n");
         addCache(db); //add additional cache
         printf("end  adding new cache \n");
         printf("data->maxslots 2 = %d \n", db->tbl[0]->data->maxslots);

         printf("Put not ok -> try to use added cache no. = %d \n", db->shared->cacheCount -1);

         if (db->tbl[db->shared->cacheCount - 1]->put(db->tbl[db->shared->cacheCount - 1], metaKey, cachedData,
               sizeof(pers_lldb_cache_flag_e) + sizeof(int) + (size_t) dataSize) ==  false) //store flag , datasize and data as value in cache
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                  DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("INSERT NEW DATA INTO RESIZE OF CACHE FAILED :  "); DLT_STRING(strerror(errno)));

         }
         else
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
                  DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("INSERT NEW DATA INTO RESIZE OF CACHE WORKS : "));
      }
      printf("END ----------- \n\n");
#endif
   }
   else
   {
      bytesWritten = dataSize; // return only size of data that has to be stored
   }
   return bytesWritten;
}



sint_t deleteFromCache(KISSDB* db, char* metaKey)
{
   char* ptr;
   Data_Cached_s dataCached = { 0 };
   int datasize = 0;
   int status = PERS_COM_FAILURE;
   Kdb_bool found = Kdb_true;
   pers_lldb_cache_flag_e eFlag;
   sint_t bytesDeleted = 0;
   size_t size = 0;
   void* val;

   //DO NOT ALLOW WRITING TO CACHE IF DATABASE IS OPENED IN READONLY MODE
   if (KISSDB_OPEN_MODE_RDONLY != db->shared->openMode)
   {
      dataCached.eFlag = CachedDataDelete;
      dataCached.m_dataSize = 0;

      //if cache not already created
      if (db->shared->cacheCreated == Kdb_false)
      {
         if (createCache(db) != 0)
         {
            return PERS_COM_FAILURE;
         }
      }
      else //open cache
      {
         if (openCache(db) != 0)
         {
            return PERS_COM_FAILURE;
         }
      }

      setMemoryAddress(db->sharedCache, db->tbl[0]);

      val = db->tbl[0]->get(db->tbl[0], metaKey, &size);
      if (NULL != val) //check if key to be deleted is in Cache
      {
         ptr = val;
         eFlag = (pers_lldb_cache_flag_e) *(int*) ptr;
         ptr += sizeof(int);
         datasize = *(int*) ptr;

         //Mark data in cache as deleted
         if (eFlag != CachedDataDelete)
         {
            if (db->tbl[0]->put(db->tbl[0], metaKey, &dataCached, sizeof(pers_lldb_cache_flag_e) + sizeof(int)) == false) //do not store any data
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                     DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("Failed to mark data in cache as deleted"));
               bytesDeleted = PERS_COM_ERR_NOT_FOUND;
               found = Kdb_false;
            }
            else
            {
               bytesDeleted = datasize;
            }
         }
      }
      else //check if key to be deleted is in database file
      {
         //get dataSize
         uint32_t size;
         status = KISSDB_get(db, metaKey, NULL, 0, &size);
         if (status == 0)
         {
            if (db->tbl[0]->put(db->tbl[0], metaKey, &dataCached, sizeof(pers_lldb_cache_flag_e) + sizeof(int)) == false)
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                     DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("Failed to mark existing data as deleted"));
               bytesDeleted = PERS_COM_ERR_NOT_FOUND;
            }
            else
            {
               bytesDeleted = size;
            }
         }
         else
         {
            if (status == 1)
            {
               found = Kdb_false;
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN,
                     DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_get: key=<"); DLT_STRING(metaKey); DLT_STRING(">, "); DLT_STRING("not found, retval=<"); DLT_INT(status); DLT_STRING(">"));
            }
            else
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                     DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_get: key=<"); DLT_STRING(metaKey); DLT_STRING(">, "); DLT_STRING("failed with retval=<"); DLT_INT(status); DLT_STRING(">"));
            }
         }
      }

      if (found == Kdb_false)
      {
         bytesDeleted = PERS_COM_ERR_NOT_FOUND;
      }
   }
   else
   {
      bytesDeleted = PERS_COM_ERR_READONLY;
   }
   return bytesDeleted;
}



sint_t getListandSize(KISSDB* db, pstr_t buffer, sint_t size, bool_t bOnlySizeNeeded, pers_lldb_purpose_e purpose)
{
   char* memory = NULL;
   char* ptr;
   char** tmplist = NULL;
   int keyCountFile = 0, keyCountCache = 0, result = 0, x = 0, idx = 0, max = 0, used = 0, objCount = 0;
   KISSDB_Iterator dbi;
   pers_lldb_cache_flag_e eFlag;
   qhasharr_t* tbl;
   qnobj_t obj;
   sint_t availableSize = size;
   void* pt;

   int keylength = (purpose == PersLldbPurpose_RCT) ? PERS_RCT_MAX_LENGTH_RESOURCE_ID : PERS_DB_MAX_LENGTH_KEY_NAME;
   char kbuf[PERS_DB_MAX_LENGTH_KEY_NAME];

   //open existing cache if present and look for keys
   if (db->shared->cacheCreated == Kdb_true)
   {
      if (openCache(db) != 0)
      {
         return PERS_COM_FAILURE;
      }
      else
      {
         setMemoryAddress(db->sharedCache, db->tbl[0]);

         objCount = db->tbl[0]->size(db->tbl[0], &max, &used);
         if (objCount > 0)
         {
            tmplist = malloc(sizeof(char*) * objCount);
            if (tmplist != NULL)
            {
               while (db->tbl[0]->getnext(db->tbl[0], &obj, &idx) == true)
               {
                  size_t keyLen = strlen(obj.name);
                  pt = obj.data;
                  eFlag = (pers_lldb_cache_flag_e) *(int*) pt;
                  if (eFlag != CachedDataDelete)
                  {
                     tmplist[keyCountCache] = (char*) malloc(keyLen + 1);
                     (void) strncpy(tmplist[keyCountCache], obj.name, keyLen);
                     ptr = tmplist[keyCountCache];
                     ptr[keyLen] = '\0';
                     keyCountCache++;
                  }
               }
            }
            else
            {
               return PERS_COM_ERR_MALLOC;
            }
         }
      }
   }

   //look for keys in database file
   //Initialise database iterator
   KISSDB_Iterator_init(db, &dbi);
   //get number of keys, stored in database file
   while (KISSDB_Iterator_next(&dbi, &kbuf, NULL) > 0)
   {
      keyCountFile++;
   }

   if ((keyCountCache + keyCountFile) > 0)
   {
      int memsize = qhasharr_calculate_memsize(keyCountCache + keyCountFile);
      //create hashtable that stores the list of keys without duplicates
      memory = malloc(memsize);
      if (memory != NULL)
      {
         memset(memory, 0, memsize);
         tbl = qhasharr(memory, memsize);
         if (tbl == NULL)
         {
            return PERS_COM_ERR_MALLOC;
         }
         //put keys in cache to a hashtable
         for (x = 0; x < keyCountCache; x++)
         {
            if (tbl->put(tbl, tmplist[x], "0", 1) == true)
            {
               if (tmplist[x] != NULL)
               {
                  free(tmplist[x]);
               }
            }
         }
         free(tmplist);

         //put keys in database file to hashtable (existing key gets overwritten -> no duplicate keys)
         KISSDB_Iterator_init(db, &dbi);
         memset(kbuf, 0, keylength);
         while (KISSDB_Iterator_next(&dbi, &kbuf, NULL) > 0)
         {
            size_t keyLen = strnlen(kbuf, sizeof(kbuf));
            if (keyLen > 0)
            {
               tbl->put(tbl, kbuf, "0", 1);
            }
         }

         //count needed size for buffer / copy keys to buffer
         idx = 0;
         while (tbl->getnext(tbl, &obj, &idx) == true)
         {
            size_t keyLen = strlen(obj.name);
            if (keyLen > 0)
            {
               if ((!bOnlySizeNeeded) && ((sint_t) keyLen < availableSize))
               {
                  (void) strncpy(buffer, obj.name, keyLen);
                  *(buffer + keyLen) = ListItemsSeparator;
                  buffer += (keyLen + sizeof(ListItemsSeparator));
                  availableSize -= (sint_t) (keyLen + sizeof(ListItemsSeparator));
               }
            }
            result += (sint_t) (keyLen + sizeof(ListItemsSeparator));
         }
         //release hashtable and allocated memory
         tbl->free(tbl);
         free(memory);
      }
      else
      {
         return PERS_COM_ERR_MALLOC;
      }
   }
   return result;
}

int createCache(KISSDB* db)
{
   Kdb_bool shmCreator;
   int status = -1;

   db->sharedCacheFd = kdbShmemOpen(db->cacheName, PERS_CACHE_MEMSIZE, &shmCreator);
   if (db->sharedCacheFd != -1)
   {
      db->sharedCache = (void*) getKdbShmemPtr(db->sharedCacheFd, PERS_CACHE_MEMSIZE);
      if (db->sharedCache != ((void*) -1))
      {
         // for dynamic cache -> create reference into array db->tbl[0] = qhasharr(db->sharedCache, PERS_CACHE_MEMSIZE);
         /*
          * Add a function called addCache that resizes shared memory with the size of PERS_CACHE_MEMSIZE
          * Then init the additional cache -> db->tbl[n] = qhasharr(db->sharedCache + (n * PERS_CACHE_MEMSIZE) , PERS_CACHE_MEMSIZE)
          * Other processes must recognize additional created caches and reopen them with -> db->tbl[0 - n] = qhasharr(db->sharedCache + (n * PERS_CACHE_MEMSIZE) , 0);
          * Store the count of created caches in shared information
          */
         db->tbl[0] = qhasharr(db->sharedCache, PERS_CACHE_MEMSIZE);
         if (db->tbl[0] != NULL)
         {
            status = 0;
            db->shared->cacheCreated = Kdb_true;
         }
      }
   }
   if (status != 0)
   {
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("Failed to create cache"); DLT_STRING(strerror(errno)));
   }
   return status;
}


int openCache(KISSDB* db)
{
   Kdb_bool shmCreator;
   int status = -1;

   //only open shared memory again if filedescriptor is not initialised yet
   if (db->sharedCacheFd <= 0) //not shared filedescriptor
   {
      db->sharedCacheFd = kdbShmemOpen(db->cacheName, PERS_CACHE_MEMSIZE, &shmCreator);
      if (db->sharedCacheFd != -1)
      {
         db->sharedCache = (void*) getKdbShmemPtr(db->sharedCacheFd, PERS_CACHE_MEMSIZE);
         if (db->sharedCache != ((void*) -1))
         {
            // use existent hash-table
            db->tbl[0] = qhasharr(db->sharedCache, 0);
            if (db->tbl[0] != NULL)
            {
               status = 0;
            }
         }
      }
   }
   else
   {
      status = 0;
   }
   if (status != 0)
   {
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__), DLT_STRING(":"), DLT_STRING("Failed to open cache"));
   }
   return status;
}


#if 0
int addCache(KISSDB* db)
{
   //ftruncate db->sharedCacheFd (oldsize + additionalspace)

   //printf("start ftruncate new cache db->tbl[0]->data->maxslots = %d\n", db->tbl[0]->data->maxslots);
   if( ftruncate(db->sharedCacheFd, db->shared->cacheSize + PERS_CACHE_MEMSIZE) < 0)
   {
      printf("Cache resize failed: %s \n", strerror(errno));
   }
   //printf("end  ftruncate new cache db->tbl[0]->data->maxslots = %d\n", db->tbl[0]->data->maxslots);

   //mremap oldsize , newsize
   //store new cache pointer for this process in db->sharedCache
   db->sharedCache = mremap(db->sharedCache, db->shared->cacheSize , db->shared->cacheSize + PERS_CACHE_MEMSIZE, MREMAP_MAYMOVE );
   if (db->sharedCache == MAP_FAILED)
   {
      printf("cacheresize MAP_FAILED \n");
   }
   //printf("end  mremap new cache db->tbl[0]->data->maxslots = %d\n", db->tbl[0]->data->maxslots);
   //printf("adding cache into db->tbl[%d],----- ptr: %p \n",db->shared->cacheCount, db->sharedCache + (db->shared->cacheCount * PERS_CACHE_MEMSIZE));

   db->tbl[db->shared->cacheCount] = qhasharr(db->sharedCache + (db->shared->cacheCount * PERS_CACHE_MEMSIZE) , PERS_CACHE_MEMSIZE);
   db->cacheReferenced++;
   //printf("here 1 \n ");

   //printf("end  qhasharr new cache db->tbl[0]->data->maxslots = %d\n", db->tbl[0]->data->maxslots);

   //store new size in shared memory
   db->shared->cacheSize += PERS_CACHE_MEMSIZE;
   db->shared->cacheCount++;
   // add check if cache was resized (check local mapped size versus shared mapped size) for all processes when cache is accessed and do a remap for new size without truncation
   return 0;
}
#endif


int closeCache(KISSDB* db)
{
   int status = -1;
   if (kdbShmemClose(db->sharedCacheFd, db->cacheName) != Kdb_false)
   {
      if (freeKdbShmemPtr(db->sharedCache, PERS_CACHE_MEMSIZE) != Kdb_false)
      {
         status = 0;
      }
   }
   if (status != 0)
   {
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(__FUNCTION__), DLT_STRING(":"), DLT_STRING("Failed to close cache"));
   }
   return status;
}
