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
#include "dlt.h"
#include <errno.h>
#include <sys/time.h>
/* L&T context */
#define LT_HDR                          "[persComLLDB]"

DLT_DECLARE_CONTEXT(persComLldbDLTCtx);

/* ---------------------- local definition  ---------------------------- */
/* max number of open handlers per process */
#define PERS_LLDB_NO_OF_STATIC_HANDLES 16
#define PERS_LLDB_MAX_STATIC_HANDLES (PERS_LLDB_NO_OF_STATIC_HANDLES-1)

#define PERS_STATUS_KEY_NOT_IN_CACHE             -10        //!< key not in cache

typedef struct
{
   char m_data[PERS_DB_MAX_SIZE_KEY_DATA];
   uint32_t m_dataSize;
} Data_LocalDB_s;

typedef enum pers_lldb_cache_flag_e
{
   CachedDataDelete = 0, /* Resource-Configuration-Table */
   CachedDataWrite, /* Local/Shared DB */
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
   struct lldb_handles_list_el_s_ * pNext;
} lldb_handles_list_el_s;

typedef struct
{
   lldb_handler_s asStaticHandles[PERS_LLDB_NO_OF_STATIC_HANDLES]; /* static area should be enough for most of the processes*/
   lldb_handles_list_el_s* pListHead; /* for the processes with a large number of threads which use Persistency */
} lldb_handlers_s;

/* ---------------------- local variables  --------------------------------- */
static const char ListItemsSeparator = '\0';

/* shared by all the threads within a process */
static lldb_handlers_s g_sHandlers = { { { 0 } } };
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
static sint_t SetDataInKissRCT(sint_t dbHandler, pconststr_t key, PersistenceConfigurationKey_s const * pConfig);
static sint_t writeBackKissDB(KISSDB* db, lldb_handler_s* pLldbHandler);
static sint_t writeBackKissRCT(KISSDB* db, lldb_handler_s* pLldbHandler);
static sint_t getListandSize(KISSDB* db, pstr_t buffer, sint_t size, bool_t bOnlySizeNeeded, pers_lldb_purpose_e purpose);
static sint_t putToCache(KISSDB* db, sint_t dataSize, char* tmp_key, void* insert_cached_data);
static sint_t getFromCache(KISSDB* db, void* tmp_key, void* readBuffer, sint_t bufsize, bool_t sizeOnly);
static sint_t getFromDatabaseFile(KISSDB* db, void* tmp_key, void* readBuffer, pers_lldb_purpose_e purpose, sint_t bufsize, bool_t sizeOnly);

/* access to resources shared by the threads within a process */
static bool_t lldb_handles_Lock(void);
static bool_t lldb_handles_Unlock(void);
static lldb_handler_s* lldb_handles_FindInUseHandle(sint_t dbHandler);
static lldb_handler_s* lldb_handles_FindAvailableHandle(void);
static void lldb_handles_InitHandle(lldb_handler_s* psHandle_inout, pers_lldb_purpose_e ePurpose, str_t const * dbPathname);
static bool_t lldb_handles_DeinitHandle(sint_t dbHandler);

static int createCache(KISSDB* db);
static int openCache(KISSDB* db);
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
sint_t pers_lldb_open(str_t const * dbPathname, pers_lldb_purpose_e ePurpose, bool_t bForceCreationIfNotPresent)
{
   sint_t returnValue = PERS_COM_FAILURE;
   bool_t bCanContinue = true;
   lldb_handler_s* pLldbHandler = NIL;
   bool_t bLocked = false;
   int mode = KISSDB_OPEN_MODE_RDWR;
   static bool_t bFirstCall = true;

   if (bFirstCall)
   {
      pid_t pid = getpid();
      str_t dltContextID[16]; /* should be at most 4 characters string, but colissions occure */

      /* set an error handler - the default one will cause the termination of the calling process */
      bFirstCall = false;
      /* init DLT */
      (void) snprintf(dltContextID, sizeof(dltContextID), "Pers_%04d", pid);
      DLT_REGISTER_CONTEXT(persComLldbDLTCtx, dltContextID, "PersCommonLLDB");
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
            DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("register context PersCommonLLDB ContextID="); DLT_STRING(dltContextID));
   }

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING("Begin opening:"); DLT_STRING("<<"); DLT_STRING(dbPathname); DLT_STRING(">>, "); ((PersLldbPurpose_RCT == ePurpose) ? DLT_STRING("RCT, ") : DLT_STRING("DB, ")); ((true == bForceCreationIfNotPresent) ? DLT_STRING("forced, ") : DLT_STRING("unforced, ")); DLT_STRING(" ... "));

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
      bCanContinue = false;
   if (bCanContinue)
   {
      int kissdb_state = 0;
      size_t datasize =
            (PersLldbPurpose_RCT == ePurpose) ? sizeof(PersistenceConfigurationKey_s) : sizeof(Data_LocalDB_s);
      size_t keysize =
            (PersLldbPurpose_RCT == ePurpose) ? PERS_RCT_MAX_LENGTH_RESOURCE_ID : PERS_DB_MAX_LENGTH_KEY_NAME;

      if (bForceCreationIfNotPresent  & (1 << 0) ) //check bit 0
         mode = KISSDB_OPEN_MODE_RWCREAT;

#ifdef __writeThrough
      if(bForceCreationIfNotPresent & (1 << 1))   //check bit 1
         printf("cached \n");
      else
         printf("uncached \n");
#endif

      kissdb_state = KISSDB_open(&pLldbHandler->kissDb, dbPathname, mode, 256, keysize, datasize);
      if (kissdb_state != 0)
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN,
               DLT_STRING("KISSDB_open: "); DLT_STRING("<<"); DLT_STRING(dbPathname); DLT_STRING(">>, "); DLT_STRING(" retval=<"); DLT_INT(kissdb_state); DLT_STRING(">"), DLT_STRING(strerror(errno)));
         bCanContinue = false;
      }
      if (bCanContinue)
      {
         lldb_handles_InitHandle(pLldbHandler, ePurpose, dbPathname);
         returnValue = pLldbHandler->dbHandler;
      }
      else
      {
         /* clean up */
         returnValue = PERS_COM_FAILURE;
         (void) lldb_handles_DeinitHandle(pLldbHandler->dbHandler);
      }
   }

   if (bLocked)
      (void) lldb_handles_Unlock();
  DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
        DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING("End of open for:"); DLT_STRING("<<"); DLT_STRING(dbPathname); DLT_STRING(">>, "); ((PersLldbPurpose_RCT == ePurpose) ? DLT_STRING("RCT, ") : DLT_STRING("DB, ")); ((true == bForceCreationIfNotPresent) ? DLT_STRING("forced, ") : DLT_STRING("unforced, ")); DLT_STRING("retval=<"); DLT_INT(returnValue); DLT_STRING(">"));

   return returnValue;
}




/**
 * \close a key-value database
 * \note : DB type is identified from dbPathname (based on extension)
 *
 * \param handlerDB     [in] handler obtained with pers_lldb_open
 *
 * \return 0 for success, negative value otherway (see pers_error_codes.h)
 */
sint_t pers_lldb_close(sint_t handlerDB)
{
   int kissdb_state = 0;
   sint_t returnValue = PERS_COM_SUCCESS;
   lldb_handler_s* pLldbHandler = NIL;
   bool_t bLocked = false;

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
           DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(handlerDB); DLT_STRING("..."));

#ifdef __showTimeMeasurements
   long long duration = 0;
   long long KdbDuration = 0;
   long long writeDuration = 0;
   struct timespec writeStart, writeEnd, kdbStart, kdbEnd, writebackStart, writebackEnd;
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
            returnValue = PERS_COM_FAILURE;
      }
   }
   else
      returnValue = PERS_COM_ERR_INVALID_PARAM;

   if (PERS_COM_SUCCESS == returnValue)
   {
      //persist cached data to flash memory
      KISSDB* db = &pLldbHandler->kissDb;

      DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
                DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("Closing database  =<<"); DLT_STRING(pLldbHandler->dbPathname); DLT_STRING(">>, "));


      Kdb_wrlock(&db->shmem_info->cache_rwlock);

      if (db->shmem_info->cache_initialised == Kdb_true)
      {
         if (db->shmem_creator == Kdb_true)
         {
            //open existing cache in existing shared memory
            if (db->shmem_cached_fd <= 0)
            {
               if (openCache(db) != 0)
               {
                  Kdb_unlock(&db->shmem_info->cache_rwlock);
                  return PERS_COM_FAILURE;
               }
            }
#ifdef __showTimeMeasurements
            clock_gettime(CLOCK_ID, &writebackStart);
#endif
            if (pLldbHandler->ePurpose == PersLldbPurpose_DB)  //write back to local database
               writeBackKissDB(&pLldbHandler->kissDb, pLldbHandler);
            else if (pLldbHandler->ePurpose == PersLldbPurpose_RCT) //write back to RCT database
            {
               writeBackKissRCT(&pLldbHandler->kissDb, pLldbHandler);
            }
#ifdef __showTimeMeasurements
            clock_gettime(CLOCK_ID, &writebackEnd);
#endif
            if (db->shmem_info->cache_initialised)
            {
               db->tbl->free(db->tbl);
               if (closeCache(db) != 0)
               {
                  Kdb_unlock(&db->shmem_info->cache_rwlock);
                  return PERS_COM_FAILURE;
               }
            }
         }
      }
      Kdb_unlock(&db->shmem_info->cache_rwlock);

#ifdef __showTimeMeasurements
      clock_gettime(CLOCK_ID, &kdbStart);
#endif

      kissdb_state = KISSDB_close(&pLldbHandler->kissDb);

#ifdef __showTimeMeasurements
      clock_gettime(CLOCK_ID, &kdbEnd);
#endif

      if (kissdb_state == 0)
      {
         if (!lldb_handles_DeinitHandle(pLldbHandler->dbHandler))
            returnValue = PERS_COM_FAILURE;
      }
      else
      {
         switch (kissdb_state)
         {
            case KISSDB_ERROR_UNMAP_SHM:
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                     DLT_STRING("KISSDB_close: "); DLT_STRING("Could not unmap shared memory object, retval=<"); DLT_INT(kissdb_state); DLT_STRING(">"));
               break;
            }
            case KISSDB_ERROR_CLOSE_SHM:
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                     DLT_STRING("KISSDB_close: "); DLT_STRING("Could not close shared memory object, retval=<"); DLT_INT(kissdb_state); DLT_STRING(">"));
               break;
            }
            default:
               break;
         }
         returnValue = PERS_COM_FAILURE;
      }
   }
   if (bLocked)
      (void) lldb_handles_Unlock();

  DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("handlerDB="); DLT_INT(handlerDB); DLT_STRING(" retval=<"); DLT_INT(returnValue); DLT_STRING(">"));

#ifdef __showTimeMeasurements
   clock_gettime(CLOCK_ID, &writeEnd);
   writeDuration += getNsDuration(&writebackStart, &writebackEnd);
   printf("Writeback to flash duration for %s => %f ms\n", pLldbHandler->dbPathname, (double)((double)writeDuration/NANO2MIL));
   KdbDuration += getNsDuration(&kdbStart, &kdbEnd);
   printf("KISSDB_close duration for %s => %f ms\n", pLldbHandler->dbPathname, (double)((double)KdbDuration/NANO2MIL));
   duration += getNsDuration(&writeStart, &writeEnd);
   printf("Overall Close duration for %s => %f ms\n", pLldbHandler->dbPathname, (double)((double)duration/NANO2MIL));
#endif
   return returnValue;
}




/**
 * \writeback cache of RCT key-value database
 * \return 0 for success, negative value otherway (see pers_error_codes.h)
 */
static sint_t writeBackKissRCT(KISSDB* db, lldb_handler_s* pLldbHandler)
{
   int kissdb_state = 0;
   int idx = 0;
   sint_t returnValue = PERS_COM_SUCCESS;
   //lldb_handler_s* pLldbHandler = NIL;
   pers_lldb_cache_flag_e eFlag;
   char* ptr;
   qnobj_t obj;
   char tmp_key[PERS_RCT_MAX_LENGTH_RESOURCE_ID];

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
            DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("START writeback for RCT: "), DLT_STRING(db->shmem_ht_name) );

   while (db->tbl->getnext(db->tbl, &obj, &idx) == true)
   {
      ptr = obj.data;
      eFlag = (pers_lldb_cache_flag_e) *(int*) ptr;
      ptr += 2 * (sizeof(int));
      (void) strncpy(tmp_key, obj.name, PERS_RCT_MAX_LENGTH_RESOURCE_ID);

      //check how data should be persisted
      switch (eFlag)
      {
         case CachedDataDelete:  //data must be deleted from file
         {
            kissdb_state = KISSDB_delete(&pLldbHandler->kissDb, tmp_key);
            if (kissdb_state != 0)
            {
               if (kissdb_state == 1)
                  DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN,
                        DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_delete: RCT key=<"); DLT_STRING(tmp_key); DLT_STRING(">, "); DLT_STRING("not found in database file, retval=<"); DLT_INT(kissdb_state); DLT_STRING(">"));
               else
                  DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                        DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_delete: RCT key=<");DLT_STRING(tmp_key); DLT_STRING(">, "); DLT_STRING("Error with retval=<"); DLT_INT(kissdb_state); DLT_STRING(">"));
            }
            break;
         }
         case CachedDataWrite:   //data must be written to file
         {
            kissdb_state = KISSDB_put(&pLldbHandler->kissDb, tmp_key, ptr);
            if (kissdb_state != 0)
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                     DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_put: RCT key=<");DLT_STRING(tmp_key); DLT_STRING(">, "); DLT_STRING("Error: Writing back to file failed with retval=<"); DLT_INT(kissdb_state); DLT_STRING(">"));
            break;
         }
         default:
            break;
      }
      free(obj.name);
      free(obj.data);
   }

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
            DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("END writeback for RCT: "), DLT_STRING(db->shmem_ht_name) );
   return returnValue;
}







/**
 * \writeback cache of local DB key-value database
 * \return 0 for success, negative value otherway (see pers_error_codes.h)
 */
static sint_t writeBackKissDB(KISSDB* db, lldb_handler_s* pLldbHandler)
{
   int kissdb_state = 0;
   int idx = 0;
   sint_t returnValue = PERS_COM_SUCCESS;
   //lldb_handler_s* pLldbHandler = NIL;
   pers_lldb_cache_flag_e eFlag;
   char* ptr;
   qnobj_t obj;

   char tmp_key[PERS_DB_MAX_LENGTH_KEY_NAME];
   Data_LocalDB_s insert = { { 0 } };
   int datasize = 0;

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
            DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("START writeback for DB: "), DLT_STRING(db->shmem_ht_name) );

   while (db->tbl->getnext(db->tbl, &obj, &idx) == true)
   {
      //get flag and datasize
      ptr = obj.data;
      eFlag = (pers_lldb_cache_flag_e) *(int*) ptr;  //pointer in obj.data to eflag
      ptr += sizeof(int);
      datasize = *(int*) ptr; //pointer in obj.data to datasize
      ptr += sizeof(int);     //pointer in obj.data to data
      (void) strncpy(tmp_key, obj.name, PERS_DB_MAX_LENGTH_KEY_NAME);

      //check how data should be persisted
      switch (eFlag)
      {
         case CachedDataDelete:  //data must be deleted from file
         {
            //delete key-value pair from database file
            kissdb_state = KISSDB_delete(&pLldbHandler->kissDb, tmp_key);
            if (kissdb_state != 0)
            {
               if (kissdb_state == 1)
                  DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN,
                        DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_delete: key=<"); DLT_STRING(tmp_key); DLT_STRING(">, "); DLT_STRING("not found in database file, retval=<"); DLT_INT(kissdb_state); DLT_STRING(">"));
               else
                  DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                        DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_delete: key=<");DLT_STRING(tmp_key); DLT_STRING(">, "); DLT_STRING("Error with retval=<"); DLT_INT(kissdb_state); DLT_STRING(">"); DLT_STRING("Error Message: ");DLT_STRING(strerror(errno)));
            }
            break;
         }
         case CachedDataWrite:  //data must be written to file
         {
            (void) memcpy(insert.m_data, ptr, datasize);
            insert.m_dataSize = datasize;

            kissdb_state = KISSDB_put(&pLldbHandler->kissDb, tmp_key, &insert); //store data followed by datasize
            if (kissdb_state != 0)
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                     DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_put: key=<");DLT_STRING(tmp_key); DLT_STRING(">, "); DLT_STRING("Error: Writing back to file failed with retval=<"); DLT_INT(kissdb_state); DLT_STRING(">"); DLT_STRING("Error Message: ");DLT_STRING(strerror(errno)));
            break;
         }
         default:
            break;
      }
      free(obj.name);
      free(obj.data);
   }
   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
            DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("END writeback for DB: "), DLT_STRING(db->shmem_ht_name) );
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
sint_t pers_lldb_write_key(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const * key, str_t const * data,
      sint_t dataSize)
{
   sint_t eErrorCode = PERS_COM_SUCCESS;

   //int i =0;
   //DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
   //         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("Datatest="); DLT_RAW(data,dataSize); DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>"); DLT_STRING(" Data Size=<<"); DLT_INT(dataSize); DLT_STRING(">>..."));

   switch (ePurpose)
   {
      case PersLldbPurpose_DB:
      {
         eErrorCode = SetDataInKissLocalDB(handlerDB, key, data, dataSize);
         break;
      }
      case PersLldbPurpose_RCT:
      {
         eErrorCode = SetDataInKissRCT(handlerDB, key, (PersistenceConfigurationKey_s const *) data);
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
sint_t pers_lldb_read_key(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const * key, pstr_t dataBuffer_out,
      sint_t bufSize)
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
sint_t pers_lldb_get_key_size(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const * key)
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
sint_t pers_lldb_delete_key(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const * key)
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

//TODO add write through compatibility
static sint_t DeleteDataFromKissDB(sint_t dbHandler, pconststr_t key)
{
   bool_t bCanContinue = true;
   sint_t delete_size = PERS_COM_FAILURE;
   lldb_handler_s* pLldbHandler = NIL;
   bool_t bLocked = false;
   char m_data[sizeof(Data_LocalDB_s)] = {0};
   pers_lldb_cache_flag_e eFlag;
   void *val;
   char *ptr;
   int status = PERS_COM_FAILURE;
   int datasize = 0;
   Kdb_bool not_found = Kdb_false;
   size_t size = 0;
   Data_Cached_s data_cached = { 0 };

  DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("handlerDB="); DLT_INT(dbHandler); DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>..."));

   if ((dbHandler >= 0) && (NIL != key))
   {
      if (lldb_handles_Lock())
      {
         bLocked = true;
         pLldbHandler = lldb_handles_FindInUseHandle(dbHandler);
         if (NIL == pLldbHandler)
            bCanContinue = false;
      }
   }
   else
      bCanContinue = false;

   if (bCanContinue)
   {
      KISSDB* db = &pLldbHandler->kissDb;

      //DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
      //         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("Working on DB: "), DLT_STRING(db->shmem_ht_name) );

      Kdb_wrlock(&db->shmem_info->cache_rwlock);

      char tmp_key[PERS_DB_MAX_LENGTH_KEY_NAME];
      (void) strncpy(tmp_key, key, PERS_DB_MAX_LENGTH_KEY_NAME);
      data_cached.eFlag = CachedDataDelete;
      data_cached.m_dataSize = 0;

      //if cache not already created
      if (db->shmem_info->cache_initialised == Kdb_false)
      {
         if (createCache(db) != 0)
         {
            Kdb_unlock(&db->shmem_info->cache_rwlock);
            return PERS_COM_FAILURE;
         }
      }
      else //open cache
      {
         if (openCache(db) != 0)
         {
            Kdb_unlock(&db->shmem_info->cache_rwlock);
            return PERS_COM_FAILURE;
         }
      }
      val = db->tbl->get(db->tbl, tmp_key, &size);
      if (NULL != val) //check if key to be deleted is in Cache
      {
         ptr = val;
         eFlag = (pers_lldb_cache_flag_e) *(int*) ptr;
         ptr += sizeof(int);
         datasize = *(int*) ptr;

         //Mark data in cache as deleted
         if (eFlag != CachedDataDelete)
         {
            if (db->tbl->put(db->tbl, tmp_key, &data_cached, sizeof(pers_lldb_cache_flag_e) + sizeof(int)) == false) //do not store any data
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                     DLT_STRING(__FUNCTION__); DLT_STRING(":");DLT_STRING("Failed to mark data in cache as deleted"));
               delete_size = PERS_COM_ERR_NOT_FOUND;
               not_found = Kdb_true;
            }
            else
               delete_size = datasize;
         }
      }
      else //check if key to be deleted is in database file
      {
         //get dataSize
         status = KISSDB_get(&pLldbHandler->kissDb, tmp_key, m_data);
         if (status == 0)
         {
            ptr = m_data;
            ptr += PERS_DB_MAX_SIZE_KEY_DATA;
            datasize = *(int*) ptr;
            //put information about the key to be deleted in cache (deletion in file happens at system shutdown)
            if (db->tbl->put(db->tbl, tmp_key, &data_cached, sizeof(pers_lldb_cache_flag_e) + sizeof(int)) == false)
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                     DLT_STRING(__FUNCTION__); DLT_STRING(":");DLT_STRING("Failed to mark existing data as deleted"));
               delete_size = PERS_COM_ERR_NOT_FOUND;
            }
            else
               delete_size = datasize;
         }
         else
         {
            if (status == 1)
            {
               not_found = Kdb_true;
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN,
                     DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_get: key=<"); DLT_STRING(key); DLT_STRING(">, "); DLT_STRING("not found, retval=<"); DLT_INT(status); DLT_STRING(">"));
            }
            else
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                     DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_get:  key=<"); DLT_STRING(tmp_key); DLT_STRING(">, "); DLT_STRING("Error with retval=<"); DLT_INT(status); DLT_STRING(">"));
            }
         }
      }

      if (not_found == Kdb_true) //key not found,
         delete_size = PERS_COM_ERR_NOT_FOUND;
      Kdb_unlock(&db->shmem_info->cache_rwlock);
   }

   if (bLocked)
      (void) lldb_handles_Unlock();

  DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("handlerDB="); DLT_INT(dbHandler); DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>, "); DLT_STRING("retval=<"); DLT_INT(delete_size); DLT_STRING(">"));

   return delete_size;
}



//TODO add write through compatibility
static sint_t DeleteDataFromKissRCT(sint_t dbHandler, pconststr_t key)
{
   bool_t bCanContinue = true;
   sint_t delete_size = PERS_COM_FAILURE;
   lldb_handler_s* pLldbHandler = NIL;
   bool_t bLocked = false;
   char m_data[sizeof(Data_LocalDB_s)] = {0};
   pers_lldb_cache_flag_e eFlag;
   void *val;
   char *ptr;
   int status = PERS_COM_FAILURE;
   int datasize = 0;
   Kdb_bool not_found = Kdb_false;
   size_t size = 0;
   Data_Cached_s data_cached = { 0 };

  DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("handlerDB="); DLT_INT(dbHandler); DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>..."));

   if ((dbHandler >= 0) && (NIL != key))
   {
      if (lldb_handles_Lock())
      {
         bLocked = true;
         pLldbHandler = lldb_handles_FindInUseHandle(dbHandler);
         if (NIL == pLldbHandler)
            bCanContinue = false;
      }
   }
   else
      bCanContinue = false;

   if (bCanContinue)
   {
      KISSDB* db = &pLldbHandler->kissDb;

      //DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
      //         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("Working on DB: "), DLT_STRING(db->shmem_ht_name) );

      Kdb_wrlock(&db->shmem_info->cache_rwlock);

      char tmp_key[PERS_RCT_MAX_LENGTH_RESOURCE_ID];
      (void) strncpy(tmp_key, key, PERS_RCT_MAX_LENGTH_RESOURCE_ID);
      data_cached.eFlag = CachedDataDelete;
      data_cached.m_dataSize = 0;

      //if cache not already created
      if (db->shmem_info->cache_initialised == Kdb_false)
      {
         if (createCache(db) != 0)
         {
            Kdb_unlock(&db->shmem_info->cache_rwlock);
            return PERS_COM_FAILURE;
         }
      }
      else //open cache
      {
         if (openCache(db) != 0)
         {
            Kdb_unlock(&db->shmem_info->cache_rwlock);
            return PERS_COM_FAILURE;
         }
      }
      //get dataSize
      val = db->tbl->get(db->tbl, tmp_key, &size);
      if (NULL != val) //check if key to be deleted is in Cache
      {
         ptr = val;
         eFlag = (pers_lldb_cache_flag_e) *(int*) ptr;
         ptr += sizeof(int);
         datasize = *(int*) ptr;

         //Mark data in cache as deleted
         if (eFlag != CachedDataDelete)
         {
            if (db->tbl->put(db->tbl, tmp_key, &data_cached, sizeof(pers_lldb_cache_flag_e) + sizeof(int)) == false)
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                     DLT_STRING(__FUNCTION__); DLT_STRING(":");DLT_STRING("Failed to mark RCT data in cache as deleted"));
               delete_size = PERS_COM_ERR_NOT_FOUND;
               not_found = Kdb_true;
            }
            else
               delete_size = datasize;
         }
      }
      else //check if key to be deleted is in database file
      {
         status = KISSDB_get(&pLldbHandler->kissDb, tmp_key, m_data);
         if (status == 0)
         {
            //Data to be deleted is not in cache, but was found in local database
            //put information about the key to be deleted in cache (deletion in file happens at system shutdown)
            if (db->tbl->put(db->tbl, tmp_key, &data_cached, sizeof(pers_lldb_cache_flag_e) + sizeof(int)) == false)
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                     DLT_STRING(__FUNCTION__); DLT_STRING(":");DLT_STRING("Failed to mark existing RCT data as deleted"));
               delete_size = PERS_COM_ERR_NOT_FOUND;
            }
            else
               delete_size = sizeof(PersistenceConfigurationKey_s);
         }
         else
         {
            if (status == 1)
            {
               not_found = Kdb_true;
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN,
                     DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_get: key=<"); DLT_STRING(key); DLT_STRING(">, "); DLT_STRING("not found, retval=<"); DLT_INT(status); DLT_STRING(">"));
            }
            else
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                     DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_get:  key=<"); DLT_STRING(key); DLT_STRING(">, "); DLT_STRING("Error with retval=<"); DLT_INT(status); DLT_STRING(">"));
         }
      }
      if (not_found == Kdb_true)
         delete_size = PERS_COM_ERR_NOT_FOUND;

      Kdb_unlock(&db->shmem_info->cache_rwlock);
   }

   if (bLocked)
      (void) lldb_handles_Unlock();

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
   DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("handlerDB="); DLT_INT(dbHandler); DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>, "); DLT_STRING("retval=<"); DLT_INT(delete_size); DLT_STRING(">"));

   return delete_size;
}



//TODO add write through compatibility
static sint_t GetAllKeysFromKissLocalDB(sint_t dbHandler, pstr_t buffer, sint_t size)
{
   bool_t bCanContinue = true;
   sint_t result = 0;
   bool_t bOnlySizeNeeded = (NIL == buffer);
   lldb_handler_s* pLldbHandler = NIL;
   bool_t bLocked = false;

  //DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         //DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("buffer="); DLT_UINT((uint_t)buffer); DLT_STRING("size="); DLT_INT(size); DLT_STRING("..."));

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
            {/* this would be very bad */
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
         (void) memset(buffer, 0, (size_t) size);

      Kdb_wrlock(&pLldbHandler->kissDb.shmem_info->cache_rwlock);
      result = getListandSize(&pLldbHandler->kissDb, buffer, size, bOnlySizeNeeded, PersLldbPurpose_DB);
      Kdb_unlock(&pLldbHandler->kissDb.shmem_info->cache_rwlock);
      if (result < 0)
         result = PERS_COM_FAILURE;
   }
   if (bLocked)
      (void) lldb_handles_Unlock();

  //DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         //DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("retval=<"); DLT_INT(result); DLT_STRING(">"));
   return result;
}


//TODO add write through compatibility
static sint_t GetAllKeysFromKissRCT(sint_t dbHandler, pstr_t buffer, sint_t size)
{
   bool_t bCanContinue = true;
   sint_t result = 0;
   bool_t bOnlySizeNeeded = (NIL == buffer);
   lldb_handler_s* pLldbHandler = NIL;
   bool_t bLocked = false;

  //DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         //DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("buffer="); DLT_UINT((uint_t)buffer); DLT_STRING("size="); DLT_INT(size); DLT_STRING("..."));

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
            {/* this would be very bad */
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
         (void) memset(buffer, 0, (size_t) size);
      Kdb_wrlock(&pLldbHandler->kissDb.shmem_info->cache_rwlock);
      result = getListandSize(&pLldbHandler->kissDb, buffer, size, bOnlySizeNeeded, PersLldbPurpose_RCT);
      Kdb_unlock(&pLldbHandler->kissDb.shmem_info->cache_rwlock);
      if (result < 0)
         result = PERS_COM_FAILURE;
   }
   if (bLocked)
      (void) lldb_handles_Unlock();

  //DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         //DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("retval=<"); DLT_INT(result); DLT_STRING(">"));

   return result;
}


//TODO add write through compatibility
static sint_t SetDataInKissLocalDB(sint_t dbHandler, pconststr_t key, pconststr_t data, sint_t dataSize)
{
   bool_t bCanContinue = true;
   sint_t size_written = PERS_COM_FAILURE;
   lldb_handler_s* pLldbHandler = NIL;
   bool_t bLocked = false;
   Data_Cached_s data_cached = { 0 };

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>, "); DLT_STRING("size<<"); DLT_INT(dataSize); DLT_STRING(">> ..."));

   if ((dbHandler >= 0) && (NIL != key) && (NIL != data) && (dataSize > 0))
   {
      if (lldb_handles_Lock())
      {
         bLocked = true;
         pLldbHandler = lldb_handles_FindInUseHandle(dbHandler);
         if (NIL == pLldbHandler)
         {
            bCanContinue = false;
            size_written = PERS_COM_ERR_INVALID_PARAM;
         }
         else
         {
            if (PersLldbPurpose_DB != pLldbHandler->ePurpose)
            {/* this would be very bad */
               bCanContinue = false;
               size_written = PERS_COM_FAILURE;
            }
         }
      }
   }
   else
   {
      bCanContinue = false;
      size_written = PERS_COM_ERR_INVALID_PARAM;
   }

   if (bCanContinue)
   {
      //DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
      //         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("Working on DB: "), DLT_STRING(pLldbHandler->dbPathname) );

      //TODO add write through (call KissDB_put)
      char tmp_key[PERS_DB_MAX_LENGTH_KEY_NAME];
      (void) strncpy(tmp_key, key, PERS_DB_MAX_LENGTH_KEY_NAME);
      data_cached.eFlag = CachedDataWrite;
      data_cached.m_dataSize = dataSize;
      (void) memcpy(data_cached.m_data, data, (size_t) dataSize);
      size_written = putToCache(&pLldbHandler->kissDb, dataSize, (char*) &tmp_key, &data_cached);
   }

   if (bLocked)
      (void) lldb_handles_Unlock();

   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>, "); DLT_STRING("size<<"); DLT_INT(dataSize); DLT_STRING(">>, "); DLT_STRING("retval=<"); DLT_INT(size_written); DLT_STRING(">"));

   return size_written;
}

//TODO add write through compatibility
static sint_t SetDataInKissRCT(sint_t dbHandler, pconststr_t key, PersistenceConfigurationKey_s const * pConfig)
{
   bool_t bCanContinue = true;
   sint_t size_written = PERS_COM_FAILURE;
   lldb_handler_s* pLldbHandler = NIL;
   bool_t bLocked = false;
   Data_Cached_RCT_s data_cached = { 0 };

  DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>..."));

   if ((dbHandler >= 0) && (NIL != key) && (NIL != pConfig))
   {
      if (lldb_handles_Lock())
      {
         bLocked = true;
         pLldbHandler = lldb_handles_FindInUseHandle(dbHandler);
         if (NIL == pLldbHandler)
         {
            bCanContinue = false;
            size_written = PERS_COM_ERR_INVALID_PARAM;
         }
         else
         {
            if (PersLldbPurpose_RCT != pLldbHandler->ePurpose)
            {/* this would be very bad */
               bCanContinue = false;
               size_written = PERS_COM_FAILURE;
            }
            /* to not use DLT while mutex locked */
         }
      }
   }
   else
   {
      bCanContinue = false;
      size_written = PERS_COM_ERR_INVALID_PARAM;
   }
   if (bCanContinue)
   {
      //DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
      //         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("Working on DB: "), DLT_STRING(pLldbHandler->dbPathname) );

      //TODO add RCT write through (call KissDB_put)
      int dataSize = sizeof(PersistenceConfigurationKey_s);
      char tmp_key[PERS_RCT_MAX_LENGTH_RESOURCE_ID];
      (void) strncpy(tmp_key, key, PERS_RCT_MAX_LENGTH_RESOURCE_ID);
      data_cached.eFlag = CachedDataWrite;
      data_cached.m_dataSize = dataSize;
      (void) memcpy(data_cached.m_data, pConfig, (size_t) dataSize);
      size_written = putToCache(&pLldbHandler->kissDb, dataSize, (char*) &tmp_key, &data_cached);
   }
   if (bLocked)
      (void) lldb_handles_Unlock();

  DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>, "); DLT_STRING("retval=<"); DLT_INT(size_written); DLT_STRING(">"));

   return size_written;
}


//TODO add write through compatibility
static sint_t GetKeySizeFromKissLocalDB(sint_t dbHandler, pconststr_t key)
{
   bool_t bCanContinue = true;
   sint_t size_read = PERS_COM_FAILURE;
   lldb_handler_s* pLldbHandler = NIL;
   bool_t bLocked = false;

  DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">> ..."));

   if ((dbHandler >= 0) && (NIL != key))
   {
      if (lldb_handles_Lock())
      {
         bLocked = true;
         pLldbHandler = lldb_handles_FindInUseHandle(dbHandler);
         if (NIL == pLldbHandler)
         {
            bCanContinue = false;
            size_read = PERS_COM_ERR_INVALID_PARAM;
         }
         else
         {
            if (PersLldbPurpose_DB != pLldbHandler->ePurpose)
            {/* this would be very bad */
               bCanContinue = false;
               size_read = PERS_COM_FAILURE;
            }
         }
      }
   }
   else
   {
      bCanContinue = false;
      size_read = PERS_COM_ERR_INVALID_PARAM;
   }
   if (bCanContinue)
   {
      char tmp_key[PERS_DB_MAX_LENGTH_KEY_NAME];
      (void) strncpy(tmp_key, key, PERS_DB_MAX_LENGTH_KEY_NAME);
      size_read = getFromCache(&pLldbHandler->kissDb, &tmp_key, NULL, 0, true);
      if (size_read == PERS_STATUS_KEY_NOT_IN_CACHE)
         size_read = getFromDatabaseFile(&pLldbHandler->kissDb, &tmp_key, NULL, PersLldbPurpose_DB, 0, true);
   }
   if (bLocked)
      (void) lldb_handles_Unlock();
   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>, "); DLT_STRING("retval=<"); DLT_INT(size_read); DLT_STRING(">"));

   return size_read;
}




//TODO add write through compatibility
/* return no of bytes read, or negative value in case of error */
static sint_t GetDataFromKissLocalDB(sint_t dbHandler, pconststr_t key, pstr_t buffer_out, sint_t bufSize)
{
   bool_t bCanContinue = true;
   sint_t size_read = PERS_COM_FAILURE;
   lldb_handler_s* pLldbHandler = NIL;
   bool_t bLocked = false;

  DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>, "); DLT_STRING("bufsize=<<"); DLT_INT(bufSize); DLT_STRING(">> ... "));

   if ((dbHandler >= 0) && (NIL != key) && (NIL != buffer_out) && (bufSize > 0))
   {
      if (lldb_handles_Lock())
      {
         bLocked = true;
         pLldbHandler = lldb_handles_FindInUseHandle(dbHandler);
         if (NIL == pLldbHandler)
         {
            bCanContinue = false;
            size_read = PERS_COM_ERR_INVALID_PARAM;
         }
         else
         {
            if (PersLldbPurpose_DB != pLldbHandler->ePurpose)
            {/* this would be very bad */
               bCanContinue = false;
               size_read = PERS_COM_FAILURE;
            }
            /* to not use DLT while mutex locked */
         }
      }
   }
   else
   {
      bCanContinue = false;
      size_read = PERS_COM_ERR_INVALID_PARAM;
   }

   if (bCanContinue)
   {

      //DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
      //         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("Working on DB: "), DLT_STRING(pLldbHandler->dbPathname) );

      char tmp_key[PERS_DB_MAX_LENGTH_KEY_NAME];
      (void) strncpy(tmp_key, key, PERS_DB_MAX_LENGTH_KEY_NAME);
      size_read = getFromCache(&pLldbHandler->kissDb, &tmp_key, buffer_out, bufSize, false);
      //if key is not already in cache
      if (size_read == PERS_STATUS_KEY_NOT_IN_CACHE)
         size_read = getFromDatabaseFile(&pLldbHandler->kissDb, &tmp_key, buffer_out, PersLldbPurpose_DB, bufSize,
               false);
   }
   if (bLocked)
      (void) lldb_handles_Unlock();

  DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>, "); DLT_STRING("bufsize=<<"); DLT_INT(bufSize); DLT_STRING(">>, "); DLT_STRING("retval=<"); DLT_INT(size_read); DLT_STRING(">"));
   return size_read;
}


//TODO add write through compatibility
static sint_t GetDataFromKissRCT(sint_t dbHandler, pconststr_t key, PersistenceConfigurationKey_s* pConfig)
{
   bool_t bCanContinue = true;
   sint_t size_read = PERS_COM_FAILURE;
   lldb_handler_s* pLldbHandler = NIL;
   bool_t bLocked = false;

  DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">> ..."));

   if ((dbHandler >= 0) && (NIL != key) && (NIL != pConfig))
   {
      if (lldb_handles_Lock())
      {
         bLocked = true;
         pLldbHandler = lldb_handles_FindInUseHandle(dbHandler);
         if (NIL == pLldbHandler)
         {
            bCanContinue = false;
            size_read = PERS_COM_ERR_INVALID_PARAM;
         }
         else
         {
            if (PersLldbPurpose_RCT != pLldbHandler->ePurpose)
            {/* this would be very bad */
               bCanContinue = false;
               size_read = PERS_COM_FAILURE;
            }
            /* to not use DLT while mutex locked */
         }
      }
   }
   else
   {
      bCanContinue = false;
      size_read = PERS_COM_ERR_INVALID_PARAM;
   }

   //read RCT
   if (bCanContinue)
   {
      //DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
      //         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("Working on DB: "), DLT_STRING(pLldbHandler->dbPathname) );

      char tmp_key[PERS_RCT_MAX_LENGTH_RESOURCE_ID];
      (void) strncpy(tmp_key, key, PERS_RCT_MAX_LENGTH_RESOURCE_ID);

      size_read = getFromCache(&pLldbHandler->kissDb, &tmp_key, pConfig, sizeof(PersistenceConfigurationKey_s), false);
      if (size_read == PERS_STATUS_KEY_NOT_IN_CACHE)
         size_read = getFromDatabaseFile(&pLldbHandler->kissDb, &tmp_key, pConfig, PersLldbPurpose_RCT,
               sizeof(PersistenceConfigurationKey_s), false);
   }
   if (bLocked)
      (void) lldb_handles_Unlock();

  DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler="); DLT_INT(dbHandler); DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>, "); DLT_STRING("retval=<"); DLT_INT(size_read); DLT_STRING(">"));

   return size_read;
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

  DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING((NIL!=pHandler) ? "Found handler <" : "ERROR can't find handler <"); DLT_INT(dbHandler); DLT_STRING(">"); DLT_STRING((NIL!=pHandler) ? (dbHandler <= PERS_LLDB_MAX_STATIC_HANDLES ? "in static area" : "in dynamic list") : ""));

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
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
               DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("malloc failed"));
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

  DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING((NIL!=pHandler) ? "Found availble handler <" : "ERROR can't find available handler <"); DLT_INT((NIL!=pHandler) ? pHandler->dbHandler : (-1)); DLT_STRING(">"); DLT_STRING((NIL!=pHandler) ? (pHandler->dbHandler <= PERS_LLDB_MAX_STATIC_HANDLES ? "in static area" : "in dynamic list") : ""));

   return pHandler;
}

static void lldb_handles_InitHandle(lldb_handler_s* psHandle_inout, pers_lldb_purpose_e ePurpose,
      str_t const * dbPathname)
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

  DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO,
         DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler=<"); DLT_INT(dbHandler); DLT_STRING("> "); DLT_STRING(bEverythingOK ? (dbHandler <= PERS_LLDB_MAX_STATIC_HANDLES ? "deinit handler in static area" : "deinit handler in dynamic list") : "ERROR - handler not found"));

   return bEverythingOK;
}



sint_t getFromCache(KISSDB* db, void* tmp_key, void* readBuffer, sint_t bufsize, bool_t sizeOnly)
{
   Kdb_bool cache_empty, key_deleted, key_not_found;
   key_deleted = cache_empty = key_not_found = Kdb_false;
   sint_t size_read = 0;
   size_t size = 0;
   int datasize = 0;
   char* ptr;
   pers_lldb_cache_flag_e eFlag;

   //if cache not already created
   Kdb_wrlock(&db->shmem_info->cache_rwlock);

   if (db->shmem_info->cache_initialised == Kdb_true)
   {
      //open existing cache in existing shared memory
      if (db->shmem_cached_fd <= 0)
      {
         if (openCache(db) != 0)
         {
            Kdb_unlock(&db->shmem_info->cache_rwlock);
            return PERS_COM_FAILURE;
         }
      }
      void* val = db->tbl->get(db->tbl, tmp_key, &size);
      if (val == NULL)
      {
         size_read = PERS_COM_ERR_NOT_FOUND;
         key_not_found = Kdb_true;
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
            size_read = datasize;

            //get data if needed
            if (!sizeOnly)
            {
               if (bufsize < datasize)
               {
                  Kdb_unlock(&db->shmem_info->cache_rwlock);
                  return PERS_COM_FAILURE;
               }
               else
                  (void) memcpy(readBuffer, ptr, datasize);
            }
         }
         else
         {
            size_read = PERS_COM_ERR_NOT_FOUND;
            key_deleted = Kdb_true;
         }
         free(val);
      }
   }
   else
      cache_empty = Kdb_true;

   //only read from file if key was not found in cache and if key was not marked as deleted in cache
   if ((cache_empty == Kdb_true && key_deleted == Kdb_false) || key_not_found == Kdb_true)
   {
      Kdb_unlock(&db->shmem_info->cache_rwlock);
      return PERS_STATUS_KEY_NOT_IN_CACHE; //key not found in cache
   }
   else
   {
      Kdb_unlock(&db->shmem_info->cache_rwlock);
      return size_read;
   }
}



sint_t getFromDatabaseFile(KISSDB* db, void* tmp_key, void* readBuffer, pers_lldb_purpose_e purpose, sint_t bufsize,
      bool_t sizeOnly)
{
   sint_t size_read = 0;
   int datasize = 0;
   char* ptr;
   char m_data[sizeof(Data_LocalDB_s)] = { 0 }; //temporary buffer that gets filled with read in KISSDB_get

   int kissdb_status = KISSDB_get(db, tmp_key, m_data);
   if (kissdb_status == 0)
   {
      if (purpose == PersLldbPurpose_DB)
      {
         ptr = m_data;
         ptr += PERS_DB_MAX_SIZE_KEY_DATA;
         datasize = *(int*) ptr;
         if (!sizeOnly)
         {
            if (bufsize < datasize)
               return PERS_COM_FAILURE;
            else
               (void) memcpy(readBuffer, m_data, datasize);
         }
         size_read = datasize;
      }
      else
      {
         if (!sizeOnly)
         {
            if (bufsize < datasize)
               return PERS_COM_FAILURE;
            else
               (void) memcpy(readBuffer, m_data, sizeof(PersistenceConfigurationKey_s));
         }
         size_read = sizeof(PersistenceConfigurationKey_s);
      }
   }
   else
   {
      if (kissdb_status == 1)
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_WARN,
               DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_get: key=<"); DLT_STRING(tmp_key); DLT_STRING(">, "); DLT_STRING("not found, retval=<"); DLT_INT(kissdb_status); DLT_STRING(">"));
         size_read = PERS_COM_ERR_NOT_FOUND;
      }
      else
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
               DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("KISSDB_get:  key=<"); DLT_STRING(tmp_key); DLT_STRING(">, "); DLT_STRING("Error with retval=<"); DLT_INT(kissdb_status); DLT_STRING(">"));
      }
      size_read = PERS_COM_ERR_NOT_FOUND;
   }
   return size_read;
}

sint_t putToCache(KISSDB* db, sint_t dataSize, char* tmp_key, void* insert_cached_data)
{
   sint_t size_written = 0;
   Kdb_wrlock(&db->shmem_info->cache_rwlock);

   //if cache not already created
   if (db->shmem_info->cache_initialised == Kdb_false)
   {
      if (createCache(db) != 0)
      {
         Kdb_unlock(&db->shmem_info->cache_rwlock);
         return PERS_COM_FAILURE;
      }
   }
   else //open cache
   {
      if (openCache(db) != 0)
      {
         Kdb_unlock(&db->shmem_info->cache_rwlock);
         return PERS_COM_FAILURE;
      }
   }

   //put in cache
   if (db->tbl->put(db->tbl, tmp_key, insert_cached_data,
         sizeof(pers_lldb_cache_flag_e) + sizeof(int) + (size_t) dataSize) == false) //store flag , datasize and data as value in cache
   {
      size_written = PERS_COM_FAILURE;
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
                    DLT_STRING(__FUNCTION__); DLT_STRING(":");DLT_STRING("Error: Failed to put data into cache"));
   }
   else
      size_written = dataSize;

   Kdb_unlock(&db->shmem_info->cache_rwlock);
   return size_written;
}





sint_t getListandSize(KISSDB* db, pstr_t buffer, sint_t size, bool_t bOnlySizeNeeded, pers_lldb_purpose_e purpose)
{
   KISSDB_Iterator dbi;
   int keycount_file = 0, keycount_cache = 0, result = 0, x = 0, idx = 0, max = 0, used = 0, obj_count, keylength = 0;
   char** tmplist = NULL;
   qnobj_t obj;
   sint_t availableSize = size;
   char* ptr;
   char* memory = NULL;
   void* pt;
   pers_lldb_cache_flag_e eFlag;

   if (purpose == PersLldbPurpose_RCT)
      keylength = PERS_RCT_MAX_LENGTH_RESOURCE_ID;
   else
      keylength = PERS_DB_MAX_LENGTH_KEY_NAME;

   //open existing cache if present and look for keys
   if (db->shmem_info->cache_initialised == Kdb_true)
   {
      if (openCache(db) != 0)
      {
         Kdb_unlock(&db->shmem_info->cache_rwlock);
         return PERS_COM_FAILURE;
      }
      else
      {
         obj_count = db->tbl->size(db->tbl, &max, &used);
         if (obj_count > 0)
         {
            tmplist = malloc(sizeof(char*) * obj_count);
            if (tmplist != NULL)
            {
               while (db->tbl->getnext(db->tbl, &obj, &idx) == true)
               {
                  pt = obj.data;
                  eFlag = (pers_lldb_cache_flag_e) *(int*) pt;
                  if (eFlag != CachedDataDelete)
                  {
                     tmplist[keycount_cache] = (char*) malloc(strlen(obj.name) + 1);
                     (void) strncpy(tmplist[keycount_cache], obj.name, strlen(obj.name));
                     ptr = tmplist[keycount_cache];
                     ptr[strlen(obj.name)] = '\0';
                     keycount_cache++;
                  }
               }
            }
            else
               return PERS_COM_ERR_MALLOC;
         }
      }
   }

   //look for keys in database file
   KISSDB_Iterator_init(db, &dbi);
   char kbuf[keylength];

   //get number of keys, stored in database file
   while (KISSDB_Iterator_next(&dbi, &kbuf, NULL) > 0)
      keycount_file++;

   if ((keycount_cache + keycount_file) > 0)
   {
      int memsize = qhasharr_calculate_memsize(keycount_cache + keycount_file);
      //create hashtable that stores the list of keys without duplicates
      memory = malloc(memsize);
      if (memory != NULL)
      {
         memset(memory, 0, memsize);
         qhasharr_t *tbl = qhasharr(memory, memsize);
         if (tbl == NULL)
            return PERS_COM_ERR_MALLOC;

         //put keys in cache to a hashtable
         for (x = 0; x < keycount_cache; x++)
         {
            if (tbl->put(tbl, tmplist[x], "0", 1) == true)
            {
               if (tmplist[x] != NULL)
                  free(tmplist[x]);
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
               tbl->put(tbl, kbuf, "0", 1);
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
         return PERS_COM_ERR_MALLOC;
   }
   return result;
}




int createCache(KISSDB* db)
{
   Kdb_bool shmem_creator;
   int status = -1;

   db->shmem_cached_fd = kdbShmemOpen(db->shmem_cached_name, PERS_CACHE_MEMSIZE, &shmem_creator);
   if (db->shmem_cached_fd != -1)
   {
      db->shmem_cached = (void*) getKdbShmemPtr(db->shmem_cached_fd, PERS_CACHE_MEMSIZE);
      if (db->shmem_cached != ((void *) -1))
      {
         db->tbl = qhasharr(db->shmem_cached, PERS_CACHE_MEMSIZE);
         if (db->tbl != NULL)
         {
            status = 0;
            db->shmem_info->cache_initialised = Kdb_true;
         }
      }
   }
   if (status != 0)
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
            DLT_STRING(__FUNCTION__); DLT_STRING(":");DLT_STRING("Error: Failed to create cache"));
   return status;
}



int openCache(KISSDB* db)
{
   Kdb_bool shmem_creator;
   int status = -1;

   //only open shared memory again if filedescriptor is not initialised yet
   if (db->shmem_cached_fd <= 0) //not shared filedescriptor
   {
      db->shmem_cached_fd = kdbShmemOpen(db->shmem_cached_name, PERS_CACHE_MEMSIZE, &shmem_creator);
      if (db->shmem_cached_fd != -1)
      {
         db->shmem_cached = (void*) getKdbShmemPtr(db->shmem_cached_fd, PERS_CACHE_MEMSIZE);
         if (db->shmem_cached != ((void *) -1))
         {
            // use existent hash-table
            db->tbl = qhasharr(db->shmem_cached, 0);
            if (db->tbl != NULL)
               status = 0;
         }
      }
   }
   else
      status = 0;
   if (status != 0)
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
            DLT_STRING(__FUNCTION__); DLT_STRING(":");DLT_STRING("Error: Failed to open cache"));
   return status;
}


int closeCache(KISSDB* db)
{
   int status = -1;
   if (kdbShmemClose(db->shmem_cached_fd, db->shmem_cached_name) != Kdb_false)
   {
      free(db->shmem_cached_name); //free memory for name  obtained by kdbGetShmName() function
      if (freeKdbShmemPtr(db->shmem_cached, PERS_CACHE_MEMSIZE) != Kdb_false)
         status = 0;
   }
   if (status != 0)
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR,
            DLT_STRING(__FUNCTION__); DLT_STRING(":");DLT_STRING("Error: Failed to close cache"));
   return status;
}
