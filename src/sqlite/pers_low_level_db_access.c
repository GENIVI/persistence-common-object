/******************************************************************************
 * Project         Persistency
 * (c) copyright   2017
 * Company         Mentor Graphics
 *****************************************************************************/
/******************************************************************************
 * This Source Code Form is subject to the terms of the
 * Mozilla Public License, v. 2.0. If a  copy of the MPL was not distributed
 * with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 ******************************************************************************/
/**
 * @file           pers_low_level_db_access.c
 * @author         Ingo Huerner
 * @brief          Implementation of pers_low_level_db_access.c; SQLite backend
 * @see
 */


#include "persComTypes.h"
#include "persComErrors.h"
#include "persComDataOrg.h"
#include "persComDbAccess.h"
#include "persComRct.h"
#include "pers_low_level_db_access_if.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <dlt.h>
#include <errno.h>
#include <sys/stat.h>


/* ---------------------- local definition  ---------------------------- */
/* max number of open handlers per process */
#define PERS_LLDB_NO_OF_STATIC_HANDLES 16
#define PERS_LLDB_MAX_STATIC_HANDLES (PERS_LLDB_NO_OF_STATIC_HANDLES-1)

//#define DB_DEBUG        // enable some debug output
//#define SHOW_COMP_OPT      // show compiler options

/* L&T context */
#define LT_HDR                          "[persComLLDB]"
DltContext persComLldbDLTCtx;
DLT_DECLARE_CONTEXT   (persComLldbDLTCtx)


typedef struct
{
   bool_t bIsAssigned;
   bool_t bIsCached;
   sint_t dbHandler;
   pers_lldb_purpose_e ePurpose;
   sqlite3 *sqlDb;

   // SQLite select statements
   sqlite3_stmt *stmtSelect;
   sqlite3_stmt *stmtSelectCache;
   // SQLite select all statements
   sqlite3_stmt *stmtSelectAll;
   // SQLite statement to merge cached and non cached table and filter duplicates
   sqlite3_stmt *stmtSelectCacheWt;
   // SQLite update statements
   sqlite3_stmt *stmtUpdate;
   sqlite3_stmt *stmtUpdateCache;
   // SQLite delete statements
   sqlite3_stmt *stmtDelete;
   sqlite3_stmt *stmtDeleteCache;
   // SQLite satetments to find modified and deleted items in cached table
   sqlite3_stmt *stmtSelectModCache;
   sqlite3_stmt *stmtSelectDelCache;

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
static lldb_handlers_s g_sHandlers;
static const char gListEntrySeperator  = '\0';


/* SQL statements */
static const char *gSqlCreateTable        = "CREATE TABLE KeyValueDB(PersKey varchar(128) UNIQUE PRIMARY KEY NOT NULL, PersValue BLOB, PersStatus INT NOT NULL DEFAULT 0)";
static const char *gSqlCreateTableCache   = "CREATE TEMP TABLE KeyValueDBTmp(PersKey varchar(128) UNIQUE PRIMARY KEY NOT NULL, PersValue BLOB, PersStatus INT NOT NULL DEFAULT 0)";

static const char *gSqlInsertReplace      = "INSERT OR REPLACE INTO KeyValueDB(PersKey, PersValue) VALUES(?, ?)";
static const char *gSqlInsertReplaceCache = "INSERT OR REPLACE INTO KeyValueDBTmp(PersKey, PersValue) VALUES(?, ?)";

static const char *gSqlSelect             = "SELECT PersValue FROM KeyValueDB WHERE PersKey = ?";
static const char *gSqlSelectCache        = "SELECT PersValue, PersStatus FROM KeyValueDBTmp WHERE PersKey = ?";

static const char *gSqlSelectAll          = "SELECT PersKey FROM KeyValueDB";

// merge two tables and remove duplicates
static const char *gSqlSelectCacheWt      = "SELECT inter.PersKey, IfNull(KeyValueDB.PersStatus, 0) as KeyValueDB, IfNull(KeyValueDBTmp.PersStatus, 0) as KeyValueDBTmp "
                                            "FROM  (SELECT KeyValueDB.PersKey FROM KeyValueDB  UNION  SELECT KeyValueDBTmp.PersKey FROM KeyValueDBTmp) as inter "
                                            "LEFT JOIN KeyValueDB on inter.PersKey = KeyValueDB.PersKey "
                                            "LEFT JOIN KeyValueDBTmp on inter.PersKey = KeyValueDBTmp.PersKey";

static const char *gSqlDelete             = "DELETE FROM KeyValueDB WHERE PersKey = ?";
static const char *gSqlDeleteCache        = "INSERT OR REPLACE INTO KeyValueDBTmp(PersKey, PersStatus) VALUES(?, 1)";

static const char *gSqlSelectModCacheAll  = "SELECT PersKey, PersValue FROM KeyValueDBTmp WHERE PersStatus = 0";
static const char *gSqlSelectDelCache     = "SELECT PersKey, PersValue FROM KeyValueDBTmp WHERE PersStatus = 1";


static const char *gSqlPragJour           = "PRAGMA main.journal_mode = MEMORY";
static const char *gSqlPragTmpSt          = "PRAGMA temp_store = MEMORY";
static const char *gSqlPragEncoding       = "PRAGMA encoding = \"UTF-8\"";
static const char *gSqlPragSynchroOff     = "PRAGMA synchronous = OFF";

#ifdef SHOW_COMP_OPT
   static const char *gSqlCompileOptions     = "PRAGMA compile_options";
#endif



/* ---------------------- local functions  --------------------------------- */
__attribute__((constructor))
static void pco_library_init()
{
   pid_t pid = getpid();
   str_t dltContextID[16]; /* should be at most 4 characters string, but colissions occure */

   /* init DLT */
   (void) snprintf(dltContextID, sizeof(dltContextID), "Pers_%04d", pid);
   DLT_REGISTER_CONTEXT(persComLldbDLTCtx, dltContextID, "PersCommonLLDB");
   //DLT_SET_APPLICATION_LL_TS_LIMIT(DLT_LOG_DEBUG, DLT_TRACE_STATUS_OFF);
   DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("register context PersCommonLLDB ContextID="); DLT_STRING(dltContextID));
}

__attribute__((destructor))
static void pco_library_destroy()
{
   DLT_UNREGISTER_CONTEXT(persComLldbDLTCtx);
}


/* access to resources shared by the threads within a process */
static void lldb_handles_InitHandle(lldb_handler_s* psHandle_inout, pers_lldb_purpose_e ePurpose, str_t const* dbPathname);
static bool_t lldb_handles_DeinitHandle(sint_t dbHandler);
static lldb_handler_s* lldb_handles_FindAvailableHandle(void);
static lldb_handler_s* lldb_handles_FindInUseHandle(sint_t dbHandler);

static int  dbHelper_checkIsLink(const char* path);

static void dbHelper_doDbSetup(lldb_handler_s* dbHandler);
static void dbHelper_update_database_from_cache(lldb_handler_s* pLldbHandler);
static void dbHelper_compileSQLiteStmt(lldb_handler_s* dbHandler);
static void dbHelper_compileSQLiteStmtCache(lldb_handler_s* dbHandler);
static void dbHelper_finalizeSQLiteStmt(lldb_handler_s* dbHandler);
static void dbHelper_finalizeSQLiteStmtCache(lldb_handler_s* dbHandler);


#ifdef SHOW_COMP_OPT
   static int callbackCompileOpt(void *NotUsed, int argc, char **argv, char **azColName);
#endif


#ifdef DB_DEBUG
   static int callback(void *NotUsed, int argc, char **argv, char **azColName);
   static void dbHelper_printDatabaseContent(lldb_handler_s* dbHandler);
#endif



sint_t pers_lldb_open(str_t const * dbPathname, pers_lldb_purpose_e ePurpose, bool_t bForceCreationIfNotPresent)
{
   int openMode = SQLITE_OPEN_READWRITE;   // default mode
   sint_t rval = PERS_COM_SUCCESS;
   lldb_handler_s* pLldbHandler = NIL;

#ifdef DB_DEBUG
   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("pers_lldb_open -> dbPathname:" ), DLT_STRING(dbPathname), DLT_STRING("Mode:"), DLT_INT(bForceCreationIfNotPresent));
   printf("\npers_lldb_open -> dbPathname: %s - Mode: %d\n", dbPathname, bForceCreationIfNotPresent );
#endif

   pLldbHandler = lldb_handles_FindAvailableHandle();
   if (NIL != pLldbHandler)
   {
      int exists = access(dbPathname, F_OK);       // check if database already exists

      pLldbHandler->bIsCached = true;              // by default database will be cached;

      if (bForceCreationIfNotPresent & (1 << 0))   //check bit 0 -> 0x0 (just open)  0x1 (open and create)
      {
         openMode |= SQLITE_OPEN_CREATE;
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("pers_lldb_open ->" ),DLT_STRING("SQLITE_OPEN_CREATE"));
      }
      if(bForceCreationIfNotPresent & (1 << 1))    //check bit 1 -> 0x2 (writeThrough) otherwise cached mode
      {
        pLldbHandler->bIsCached = false;
        DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("pers_lldb_open ->" ),DLT_STRING("WriteModeWt"));
      }
      if( bForceCreationIfNotPresent & (1 << 2))   //check bit 2 -> 0x4 (read only mode) otherwise read/write mode
      {
        openMode = SQLITE_OPEN_READONLY;
        pLldbHandler->bIsCached = false;
        DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("pers_lldb_open ->" ),DLT_STRING("SQLITE_OPEN_READONLY"));
      }

      if(sqlite3_open_v2(dbPathname, &pLldbHandler->sqlDb, openMode, NULL)  == SQLITE_OK)
      {
         if(exists)  // if database file does not exists, create table
         {
            char *errMsg = 0;

            if(sqlite3_exec(pLldbHandler->sqlDb, gSqlCreateTable, NULL, 0, &errMsg) != SQLITE_OK )
            {
               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("sqlite3_exec"), DLT_STRING(errMsg));
               //printf("sqlite3_exec \"gSqlCreateTable\": %s\n", errMsg);
               sqlite3_free(errMsg);
            }
         }

         // modify operation of SQLite library
         dbHelper_doDbSetup(pLldbHandler);

         // compile SQLite statements
         dbHelper_compileSQLiteStmt(pLldbHandler);

         lldb_handles_InitHandle(pLldbHandler, ePurpose, dbPathname);
         rval = pLldbHandler->dbHandler;

         // Cached  A N D  shared databases are currently not supported.
         // It is not possible under SQLite for multiple processes to access the same temporary table
         if((pLldbHandler->bIsCached == true) && (-1 == dbHelper_checkIsLink(dbPathname)) && rval != PERS_COM_FAILURE )
         {
            char *errMsg = 0;

            if(sqlite3_exec(pLldbHandler->sqlDb, gSqlCreateTableCache, NULL, 0, &errMsg) == SQLITE_OK )
            {
               dbHelper_compileSQLiteStmtCache(pLldbHandler);
            }
            else
            {
               (void) lldb_handles_DeinitHandle(pLldbHandler->dbHandler);

               DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("sqlite3_exec"), DLT_STRING(errMsg));
               sqlite3_free(errMsg);

               rval = PERS_COM_FAILURE;
            }
         }
         else
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("pers_lldb_open ->"),DLT_STRING("DO NOT CACHING"));
            pLldbHandler->bIsCached = false; // caching in this case not possible, reset caching flag.
         }
      }
      else
      {
         rval = PERS_COM_FAILURE;
      }
   }
   else
   {
      rval = PERS_COM_ERR_OUT_OF_MEMORY;
   }

   return rval;
}



sint_t pers_lldb_close(sint_t handlerDB)
{
   sint_t rval = PERS_COM_SUCCESS;

#ifdef DB_DEBUG
   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("pers_lldb_close"), DLT_INT(handlerDB));
#endif

   if (handlerDB >= 0)
   {
      lldb_handler_s* pLldbHandler = lldb_handles_FindInUseHandle(handlerDB);

      if (NIL != pLldbHandler)
      {
         // write back modified data from cache database
         if(pLldbHandler->bIsCached == true)
         {
            // update table with temp table content
            dbHelper_update_database_from_cache(pLldbHandler);

            dbHelper_finalizeSQLiteStmtCache(pLldbHandler);
         }

         // finalize statements
         dbHelper_finalizeSQLiteStmt(pLldbHandler);

         if(sqlite3_close(pLldbHandler->sqlDb) == SQLITE_OK)
         {
            if (!lldb_handles_DeinitHandle(pLldbHandler->dbHandler))
            {
               rval = PERS_COM_FAILURE;
            }
         }
         else
         {
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("pers_lldb_close ->" ), DLT_STRING(sqlite3_errmsg(pLldbHandler->sqlDb)));
            rval = PERS_COM_FAILURE;
         }
      }
      else
      {
         rval = PERS_COM_FAILURE;
      }
   }
   else
   {
      rval = PERS_COM_ERR_INVALID_PARAM;
   }
   return rval;
}



sint_t pers_lldb_write_key(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const * key, str_t const * data, sint_t dataSize)
{
   sint_t rval = PERS_COM_SUCCESS;
   lldb_handler_s* pLldbHandler = NIL;

#ifdef DB_DEBUG
   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("pers_lldb_write_key -> key:" ), DLT_STRING(key));
   printf("%s - key: \"%s\" \n", __FUNCTION__, key);
#endif

   if((handlerDB >= 0) && (NIL != key) && (NIL != data) && (dataSize > 0))
   {
      pLldbHandler = lldb_handles_FindInUseHandle(handlerDB);
      if (NIL != pLldbHandler)
      {
         int rc = 0;
         int len = 0;

         if(ePurpose == PersLldbPurpose_RCT)
         {
            len = sizeof(PersistenceConfigurationKey_s);
         }
         else if(ePurpose ==  PersLldbPurpose_DB)
         {
            len = (int)strlen(data);
         }

         if(pLldbHandler->bIsCached == true)
         {
            sqlite3_bind_text(pLldbHandler->stmtUpdateCache, 1, key, (int)strlen(key), SQLITE_STATIC);
            sqlite3_bind_blob(pLldbHandler->stmtUpdateCache, 2, (const void*)data, len, SQLITE_STATIC );
            rc = sqlite3_step(pLldbHandler->stmtUpdateCache);

            if(rc == SQLITE_DONE)
            {
               rval = len;
            }
            else if(rc < SQLITE_ROW)
            {
             DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("pers_lldb_write_key - SQL -" ),
                                                       DLT_STRING(sqlite3_errmsg(pLldbHandler->sqlDb)));

             if(rval == SQLITE_READONLY)
                rval = PERS_COM_FAILURE; //   PERS_COM_ERR_READONLY;
             else
                rval = PERS_COM_FAILURE;
            }
            sqlite3_reset(pLldbHandler->stmtUpdateCache);
         }
         else
         {
            sqlite3_bind_text(pLldbHandler->stmtUpdate, 1, key, (int)strlen(key), SQLITE_STATIC);
            sqlite3_bind_blob(pLldbHandler->stmtUpdate, 2, (const void*)data, len, SQLITE_TRANSIENT );
            rc = sqlite3_step(pLldbHandler->stmtUpdate);

            if(rc == SQLITE_DONE)
            {
              rval = len;
            }
            else if(rc < SQLITE_ROW)
            {
              DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("pers_lldb_write_key - SQL -" ), DLT_STRING(sqlite3_errmsg(pLldbHandler->sqlDb)));

              if(rval == SQLITE_READONLY)
                 rval = PERS_COM_FAILURE; //   PERS_COM_ERR_READONLY;
              else
                 rval = PERS_COM_FAILURE;
            }
            sqlite3_reset(pLldbHandler->stmtUpdate);
         }
      }
      else
      {
         rval = PERS_COM_ERR_INVALID_PARAM;
      }
   }
   else
   {
     rval = PERS_COM_ERR_INVALID_PARAM;
   }
   return rval;
}



sint_t pers_lldb_read_key(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const * key, pstr_t dataBuffer_out, sint_t bufSize)
{
   sint_t rval = PERS_COM_FAILURE;
   lldb_handler_s* pLldbHandler = NIL;

#ifdef DB_DEBUG
   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("pers_lldb_read_key -> key:" ), DLT_STRING(key));
   printf("%s [%d] - key: \"%s\" \n", __FUNCTION__, handlerDB, key);
#endif

   if ((handlerDB >= 0) && (NIL != key) && (NIL != dataBuffer_out) && (bufSize > 0))
   {
      pLldbHandler = lldb_handles_FindInUseHandle(handlerDB);
      if (NIL != pLldbHandler)
      {
         int sizeBlob = 0;
         int rc = 0;
         const void* blobData = NULL;
         rval = PERS_COM_FAILURE;

         if(pLldbHandler->bIsCached ==  true)   // if cached, first try to read from temp table
         {
#ifdef DB_DEBUG
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("pers_lldb_read_key -> key:" ), DLT_STRING(key), DLT_STRING("read cache"));
#endif
            do
            {
               rc = sqlite3_bind_text(pLldbHandler->stmtSelectCache, 1, key, (int)strlen(key), SQLITE_STATIC);
               rc = sqlite3_step(pLldbHandler->stmtSelectCache);

               if(rc == SQLITE_ROW)
               {
                  if(sqlite3_column_int(pLldbHandler->stmtSelectCache, 1) != 1)  // check if found item is not marked as deleted
                  {
                     sizeBlob = sqlite3_column_bytes(pLldbHandler->stmtSelectCache, 0);
                     blobData = sqlite3_column_blob(pLldbHandler->stmtSelectCache, 0);

                     if(blobData != NULL && ( sizeBlob >= 0 && sizeBlob <= bufSize))
                     {
                        if(ePurpose == PersLldbPurpose_RCT)  // if RCT tableitem, adjust size
                        {
                           sizeBlob = sizeof(PersistenceConfigurationKey_s);
                        }
                        memcpy(dataBuffer_out, blobData, (size_t)sizeBlob);
                        rval = sizeBlob;
                     }
                     else
                     {
                        rval = PERS_COM_ERR_BUFFER_TOO_SMALL;
                        DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("pers_lldb_read_key -> key:" ), DLT_STRING(key), DLT_STRING("Buffer to small"));
                     }
                  }
                  else
                  {
                     rval = PERS_COM_ERR_NOT_FOUND;   // if marked as deleted, return not found
                  }
               }
            } while(rc != SQLITE_DONE);

            sqlite3_reset(pLldbHandler->stmtSelectCache);
         }

         if(rval == PERS_COM_FAILURE)  // if not found in temp table, search in normal table
         {
#ifdef DB_DEBUG
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("pers_lldb_read_key -> key:" ), DLT_STRING(key), DLT_STRING("read non cache"), DLT_INT(rval));
#endif
            rval = PERS_COM_ERR_NOT_FOUND;

            do
            {
              rc = sqlite3_bind_text(pLldbHandler->stmtSelect, 1, key, (int)strlen(key), SQLITE_STATIC);
              rc = sqlite3_step(pLldbHandler->stmtSelect);

              if(rc == SQLITE_ROW)
              {
                 sizeBlob = sqlite3_column_bytes(pLldbHandler->stmtSelect, 0);
                 blobData = sqlite3_column_blob(pLldbHandler->stmtSelect, 0);
#ifdef DB_DEBUG
                 DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("pers_lldb_read_key -> key:" ), DLT_STRING(key), DLT_STRING("read non cache F O U N D ->"), DLT_INT(rval));
                 //printf("   *** data[%d]: %s\n", sizeBlob, (const char*)blobData);
#endif
                 if(blobData != NULL && sizeBlob >= 0)
                 {
                    if(ePurpose == PersLldbPurpose_RCT)  // if RCT table item, adjust size
                    {
                       sizeBlob = sizeof(PersistenceConfigurationKey_s);
                    }
                    memcpy(dataBuffer_out, blobData, (size_t)sizeBlob);
                    rval = sizeBlob;
                 }
              }
            } while(rc != SQLITE_DONE);
            sqlite3_reset(pLldbHandler->stmtSelect);
         }
      }
      else
      {
         rval = PERS_COM_ERR_INVALID_PARAM;
      }
   }
   else
   {
       rval = PERS_COM_ERR_INVALID_PARAM;
   }
   return rval;
}



sint_t pers_lldb_get_key_size(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const * key)
{
   sint_t rval = PERS_COM_ERR_INVALID_PARAM;
   lldb_handler_s* pLldbHandler = NIL;

#ifdef DB_DEBUG
   printf("%s - Key: %s\n", __FUNCTION__, key);
   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("pers_lldb_get_key_size -> key:" ), DLT_STRING(key), DLT_STRING("Purpose:"), DLT_INT(ePurpose));
#endif

   if ((handlerDB >= 0) && (NIL != key))
   {
      pLldbHandler = lldb_handles_FindInUseHandle(handlerDB);
      if (NIL != pLldbHandler)
      {
         int rc = -1;
         int len  = (int)strlen(key);
         rval = PERS_COM_ERR_NOT_FOUND;

         if(pLldbHandler->bIsCached == true) // if cached, first search temp table
         {
           do
           {
              sqlite3_bind_text(pLldbHandler->stmtSelectModCache, 1, key, len, SQLITE_STATIC);
              rc = sqlite3_step(pLldbHandler->stmtSelectModCache);

              if(rc == SQLITE_ROW)
              {
                 rval = sqlite3_column_bytes(pLldbHandler->stmtSelectModCache, 1);
              }
           } while(rc != SQLITE_DONE);
           sqlite3_reset(pLldbHandler->stmtSelectModCache);
         }

         if(rval == PERS_COM_ERR_NOT_FOUND)  // if nothing found, search in regular table
         {
           do
           {
              sqlite3_bind_text(pLldbHandler->stmtSelect, 1, key, len, SQLITE_STATIC);
              rc = sqlite3_step(pLldbHandler->stmtSelect);

              if(rc == SQLITE_ROW)
              {
                 rval = sqlite3_column_bytes(pLldbHandler->stmtSelect, 0);
              }
           } while(rc != SQLITE_DONE);
           sqlite3_reset(pLldbHandler->stmtSelect);
         }
      }
   }
   return rval;
}



sint_t pers_lldb_delete_key(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const * key)
{
   sint_t rval = PERS_COM_ERR_INVALID_PARAM;
   lldb_handler_s* pLldbHandler = NIL;

#ifdef DB_DEBUG
   printf("%s - key: \"%s\" \n", __FUNCTION__, key);
   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("pers_lldb_delete_key -> key:" ), DLT_STRING(key), DLT_STRING("Purpose:"), DLT_INT(ePurpose));
#endif

   if ((handlerDB >= 0) && (NIL != key))
   {
      pLldbHandler = lldb_handles_FindInUseHandle(handlerDB);
      if (NIL != pLldbHandler)
      {
         int rc = -1;
         int len = (int)strlen(key);

         if(pLldbHandler->bIsCached ==  true)
         {
           do   // first check if key is available in non cached db
           {
              rc = sqlite3_bind_text(pLldbHandler->stmtSelect, 1, key, len, SQLITE_STATIC);
              rc = sqlite3_step(pLldbHandler->stmtSelect);

              if(rc == SQLITE_ROW)
              {
                 rval = PERS_COM_SUCCESS;   // key found => remember to delete key
              }
           } while(rc != SQLITE_DONE);
           sqlite3_reset(pLldbHandler->stmtSelect);

           if(rval != PERS_COM_SUCCESS)   // key not found in temp table, check if available in regular table
           {
              do
              {
                 rc = sqlite3_bind_text(pLldbHandler->stmtSelectCache, 1, key, len, SQLITE_STATIC);
                 rc = sqlite3_step(pLldbHandler->stmtSelectCache);

                 if(rc == SQLITE_ROW)
                 {
                    rval = PERS_COM_SUCCESS;   // key found => remember to delete key

                 }
              } while(rc != SQLITE_DONE);
              sqlite3_reset(pLldbHandler->stmtSelectCache);
           }

           if(rval == PERS_COM_SUCCESS)   // now finally mark the key as deleted
           {
              do
              {
                 sqlite3_bind_text(pLldbHandler->stmtDeleteCache, 1, key, len, SQLITE_STATIC);
                 rc = sqlite3_step(pLldbHandler->stmtDeleteCache);

                 if (rc == SQLITE_ROW || rc == SQLITE_DONE)
                 {
                    rval = PERS_COM_SUCCESS;
                 }
              } while(rc != SQLITE_DONE);
              sqlite3_reset(pLldbHandler->stmtDeleteCache);
           }
         }
         else  // non cached, just delete item
         {
           do
           {
              sqlite3_bind_text(pLldbHandler->stmtDelete, 1, key, len, SQLITE_STATIC);
              rc = sqlite3_step(pLldbHandler->stmtDelete);

              if (rc == SQLITE_ROW)
              {
                 rval = PERS_COM_SUCCESS;
              }
           } while(rc != SQLITE_DONE);
           sqlite3_reset(pLldbHandler->stmtDelete);
         }
      }
   }

   return rval;
}



sint_t pers_lldb_get_size_keys_list(sint_t handlerDB, pers_lldb_purpose_e ePurpose)
{
   sint_t rval = PERS_COM_SUCCESS;
   lldb_handler_s* pLldbHandler = NIL;

#ifdef DB_DEBUG
   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("pers_lldb_get_size_keys_list - handlerDB" ), DLT_INT(handlerDB));
#endif

   if (handlerDB >= 0)
   {
      pLldbHandler = lldb_handles_FindInUseHandle(handlerDB);
      if(NIL != pLldbHandler)
      {
         int rc = -1;
         if(pLldbHandler->bIsCached ==  true)
         {
           do
           {
              rc = sqlite3_step(pLldbHandler->stmtSelectCacheWt);

              if(rc == SQLITE_ROW)
              {
                 if(sqlite3_column_int(pLldbHandler->stmtSelectCacheWt, 2) != 1)
                 {
                    rval += sqlite3_column_bytes(pLldbHandler->stmtSelectCacheWt, 0);
                 }
                 rval++;
              }

           } while(rc != SQLITE_DONE);
           sqlite3_reset(pLldbHandler->stmtSelectCacheWt);
         }
         else
         {
           rval = -1;  // yes, initialize with -1
           do
           {
              rc = sqlite3_step(pLldbHandler->stmtSelectAll);

              if(rc == SQLITE_ROW)
              {
                 rval += sqlite3_column_bytes(pLldbHandler->stmtSelectAll, 0);
              }

              rval++;

           } while(rc != SQLITE_DONE);
           sqlite3_reset(pLldbHandler->stmtSelectAll);
         }
      }
      else
      {
        rval = PERS_COM_ERR_INVALID_PARAM;
      }
   }
   else
   {
      rval = PERS_COM_ERR_INVALID_PARAM;
   }

   return rval;
}



sint_t pers_lldb_get_keys_list(sint_t handlerDB, pers_lldb_purpose_e ePurpose, pstr_t listingBuffer_out, sint_t bufSize)
{
   sint_t rval = PERS_COM_SUCCESS;
   lldb_handler_s* pLldbHandler = NIL;

#ifdef DB_DEBUG
   DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING("pers_lldb_get_keys_list - handlerDB" ), DLT_INT(handlerDB));
#endif

   if ((handlerDB >= 0) && listingBuffer_out != NULL && bufSize > 0 )
   {
      pLldbHandler = lldb_handles_FindInUseHandle(handlerDB);
      if (NIL != pLldbHandler)
      {
         int rc = -1;
         int availableSize = bufSize;

         if(pLldbHandler->bIsCached ==  true)
         {
#ifdef DB_DEBUG
           printf("\n**************************************************************************\n");
           printf("*** ***                   All key in both batabases               *** ***\n");
           printf("**************************************************************************\n");
#endif
           do
           {
              rc = sqlite3_step(pLldbHandler->stmtSelectCacheWt);

              if(rc == SQLITE_ROW)
              {
#ifdef DB_DEBUG
                 printf("Item name: \"%s\" - size [%d] - Status: %d",
                                       sqlite3_column_text(pLldbHandler->stmtSelectCacheWt, 0),
                                       sqlite3_column_bytes(pLldbHandler->stmtSelectCacheWt, 0),
                                       sqlite3_column_int(pLldbHandler->stmtSelectCacheWt, 2));
#endif
                 if(sqlite3_column_int(pLldbHandler->stmtSelectCacheWt, 2) != 1)
                 {
                    int keyLen = sqlite3_column_bytes(pLldbHandler->stmtSelectCacheWt, 0);

                    if(availableSize > 0)
                    {
                       (void) strncpy(listingBuffer_out, (const char*)sqlite3_column_text(pLldbHandler->stmtSelectCacheWt, 0), (size_t)keyLen);
                       *(listingBuffer_out + keyLen) = gListEntrySeperator;
                       listingBuffer_out += (keyLen + (int)sizeof(gListEntrySeperator));
                       availableSize -= (sint_t) (keyLen + (int)sizeof(gListEntrySeperator));
                    }
                    else
                    {
                       rval = PERS_COM_FAILURE;
                    }
#ifdef DB_DEBUG
                    printf("\n");
#endif
                 }
#ifdef DB_DEBUG
                 else
                 {
                    printf(" -- D E L E T E D item\n");
                 }
#endif
              }

           } while(rc != SQLITE_DONE);
           sqlite3_reset(pLldbHandler->stmtSelectCacheWt);

#ifdef DB_DEBUG
           printf("**************************************************************************\n");
#endif
         }
         else  // not cached
         {
           do
           {
             rc = sqlite3_step(pLldbHandler->stmtSelectAll);

             if(rc == SQLITE_ROW)
             {
                int keyLen = sqlite3_column_bytes(pLldbHandler->stmtSelectAll, 0);

                if(availableSize > 0)
                {
                   (void) strncpy(listingBuffer_out, (const char*)sqlite3_column_blob(pLldbHandler->stmtSelectAll, 0), (size_t)keyLen);
                   *(listingBuffer_out + keyLen) = gListEntrySeperator;
                   listingBuffer_out += (keyLen + (int)sizeof(gListEntrySeperator));
                   availableSize -= (sint_t) (keyLen + (int)sizeof(gListEntrySeperator));
                }
             }

           } while(rc != SQLITE_DONE);
           sqlite3_reset(pLldbHandler->stmtSelectAll);
         }
      }
      else
      {
         rval = PERS_COM_ERR_INVALID_PARAM;
      }
   }
   else
   {
     rval = PERS_COM_ERR_INVALID_PARAM;
   }

   return rval;
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

#ifdef DB_DEBUG
    DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
            DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING((NIL!=pHandler) ? "Found availble handler <" : "ERROR can't find available handler <");
            DLT_INT((NIL!=pHandler) ? pHandler->dbHandler : (-1)); DLT_STRING(">");
            DLT_STRING((NIL!=pHandler) ? (pHandler->dbHandler <= PERS_LLDB_MAX_STATIC_HANDLES ? "in static area" : "in dynamic list") : ""));
#endif

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

#ifdef DB_DEBUG
    DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
            DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING("dbHandler=<"); DLT_INT(dbHandler); DLT_STRING("> ");
            DLT_STRING(bEverythingOK ? (dbHandler <= PERS_LLDB_MAX_STATIC_HANDLES ? "deinit handler in static area" : "deinit handler in dynamic list") : "ERROR - handler not found"));
#endif

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

#ifdef DB_DEBUG
    DLT_LOG(persComLldbDLTCtx, DLT_LOG_DEBUG,
            DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":"); DLT_STRING((NIL!=pHandler) ? "Found handler <" : "ERROR can't find handler <"); DLT_INT(dbHandler); DLT_STRING(">");
            DLT_STRING((NIL!=pHandler) ? (dbHandler <= PERS_LLDB_MAX_STATIC_HANDLES ? "in static area" : "in dynamic list") : ""));
#endif

    return pHandler;
}



 void dbHelper_update_database_from_cache(lldb_handler_s* pLldbHandler)
 {
    int rc = 0;
    const char* key = NULL;
    const char* value = NULL;

 #ifdef DB_DEBUG
    dbHelper_printDatabaseContent(pLldbHandler);

    printf("\n**************************************************************************\n");
    printf("*** ***                  C L O S E  database                       *** ***\n");
    printf("**************************************************************************\n");

    printf(" -- U P D A T E  Section --\n");
 #endif

    // search for modified keys and update database
    do
    {
       rc = sqlite3_step(pLldbHandler->stmtSelectModCache);
       if(rc == SQLITE_ROW)
       {
          int size = sqlite3_column_bytes(pLldbHandler->stmtSelectModCache, 1);
          key   = (const char*)sqlite3_column_text(pLldbHandler->stmtSelectModCache, 0);
          value = sqlite3_column_blob(pLldbHandler->stmtSelectModCache, 1);

 #ifdef DB_DEBUG
          printf("   **       - UPDATE - Key[%d]: %s - \n", size, key);
 #endif

          // insert modified key into non cached database
          sqlite3_bind_text(pLldbHandler->stmtUpdate, 1, key, (int)strlen(key), SQLITE_STATIC);
          sqlite3_bind_blob(pLldbHandler->stmtUpdate, 2, value, size, SQLITE_STATIC);
          sqlite3_step(pLldbHandler->stmtUpdate);

          sqlite3_reset(pLldbHandler->stmtUpdate);
       }

    } while(rc != SQLITE_DONE);
    sqlite3_reset(pLldbHandler->stmtSelectModCache);

 #ifdef DB_DEBUG
    printf(" -- D E L E T E  Section --\n");
 #endif

    // search for deleted key and delete in
    do
    {
      rc = sqlite3_step(pLldbHandler->stmtSelectDelCache);

      if(rc == SQLITE_ROW)
      {
         key = (const char*)sqlite3_column_text(pLldbHandler->stmtSelectDelCache, 0);
 #ifdef DB_DEBUG
         printf("   **       - DELETE - Key: %s \n", key);
 #endif

         // insert modified key into non cached database
         sqlite3_bind_text(pLldbHandler->stmtDelete, 1, key, (int)strlen(key), SQLITE_STATIC);
         sqlite3_step(pLldbHandler->stmtDelete);

         sqlite3_reset(pLldbHandler->stmtDelete);
      }

    } while(rc != SQLITE_DONE);
    sqlite3_reset(pLldbHandler->stmtSelectDelCache);

 #ifdef DB_DEBUG
    printf("**************************************************************************\n");
 #endif
}



int dbHelper_checkIsLink(const char* path)
{
    char fileName[64] = { 0 };
    char truncPath[128] = { 0 };
    int isLink = 0;
    size_t len = 0;
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
          isLink = 1;
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


void dbHelper_doDbSetup(lldb_handler_s* dbHandler)
{
   if(dbHandler != NULL)
   {
      char *errMsg = 0;
      if(sqlite3_exec(dbHandler->sqlDb, gSqlPragJour, NULL, 0, &errMsg) != SQLITE_OK )
      {
        DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("Pragma Journal:"), DLT_STRING(errMsg));
        printf("\n** ** ** ** Pragma Journal: %s\n\n", errMsg);
        sqlite3_free(errMsg);
      }
      if(sqlite3_exec(dbHandler->sqlDb, gSqlPragTmpSt, NULL, 0, &errMsg) != SQLITE_OK )
      {
        DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("Pragma Tmp store:"), DLT_STRING(errMsg));
        printf("** ** ** ** Pragma Tmp store: %s\n\n", errMsg);
        sqlite3_free(errMsg);
      }
      if(sqlite3_exec(dbHandler->sqlDb, gSqlPragEncoding, NULL, 0, &errMsg) != SQLITE_OK )
      {
        DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("Pragma encoding:"), DLT_STRING(errMsg));
        printf("** ** ** ** Pragma Encoding: %s\n\n", errMsg);
        sqlite3_free(errMsg);
      }
      if(sqlite3_exec(dbHandler->sqlDb, gSqlPragSynchroOff, NULL, 0, &errMsg) != SQLITE_OK )
      {
        DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("Pragma Synchronous off:"), DLT_STRING(errMsg));
        printf("** ** ** ** Pragma Synchronous off: %s\n\n", errMsg);
        sqlite3_free(errMsg);
      }

 #ifdef SHOW_COMP_OPT
      if(sqlite3_exec(dbHandler->sqlDb, gSqlCompileOptions, callbackCompileOpt, 0, &errMsg) != SQLITE_OK )
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("Pragma Compile Options:"), DLT_STRING(errMsg));
         printf("** ** ** ** Pragma Compile Options: %s\n\n", errMsg);
         sqlite3_free(errMsg);
      }
#endif
   }
}


void dbHelper_compileSQLiteStmt(lldb_handler_s* dbHandler)
{
   if(dbHandler != NULL)
   {
      // insert replace statement
      if(sqlite3_prepare_v2(dbHandler->sqlDb, gSqlInsertReplace, (int)strlen(gSqlInsertReplace), &dbHandler->stmtUpdate, 0) != SQLITE_OK)
      {
        DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("Prepare stmtUpdate failed:"), DLT_STRING(sqlite3_errmsg(dbHandler->sqlDb)));
        printf("Prepare stmtUpdate failed: %s\n", sqlite3_errmsg(dbHandler->sqlDb));
      }

      // select key statement
      if(sqlite3_prepare_v2(dbHandler->sqlDb, gSqlSelect, (int)strlen(gSqlSelect), &dbHandler->stmtSelect, 0) != SQLITE_OK)
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("Prepare stmtSelect failed:" ), DLT_STRING(sqlite3_errmsg(dbHandler->sqlDb)));
         printf("Prepare stmtSelect failed: %s\n", sqlite3_errmsg(dbHandler->sqlDb));
      }

      // select all statement
      if(sqlite3_prepare_v2(dbHandler->sqlDb, gSqlSelectAll, (int)strlen(gSqlSelectAll), &dbHandler->stmtSelectAll, 0) != SQLITE_OK)
      {
        DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("Prepare stmtSelectAll failed:" ), DLT_STRING(sqlite3_errmsg(dbHandler->sqlDb)));
        printf("Prepare stmtSelectAll failed: %s\n", sqlite3_errmsg(dbHandler->sqlDb));
      }

      // delete statement
      if(sqlite3_prepare_v2(dbHandler->sqlDb, gSqlDelete, (int)strlen(gSqlDelete), &dbHandler->stmtDelete, 0) != SQLITE_OK)
      {
        DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("Prepare stmtDelete failed:" ), DLT_STRING(sqlite3_errmsg(dbHandler->sqlDb)));
        printf("Prepare stmtDelete failed: %s\n", sqlite3_errmsg(dbHandler->sqlDb));
      }
   }
   else
   {
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("dbHelper_compileSQLiteStmtfailed" ));
   }
}



void dbHelper_compileSQLiteStmtCache(lldb_handler_s* dbHandler)
{
   if(dbHandler != NULL)
   {
      // insert replace statement
      if(sqlite3_prepare_v2(dbHandler->sqlDb, gSqlInsertReplaceCache, (int)strlen(gSqlInsertReplaceCache), &dbHandler->stmtUpdateCache, 0) != SQLITE_OK)
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("Prepare stmtUpdateCache failed:" ), DLT_STRING(sqlite3_errmsg(dbHandler->sqlDb)));
         printf("Prepare stmtUpdateCache failed: %s\n", sqlite3_errmsg(dbHandler->sqlDb));
      }

      // select key statement
      if(sqlite3_prepare_v2(dbHandler->sqlDb, gSqlSelectCache, (int)strlen(gSqlSelectCache), &dbHandler->stmtSelectCache, 0) != SQLITE_OK)
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("Prepare stmtSelectCache failed:" ), DLT_STRING(sqlite3_errmsg(dbHandler->sqlDb)));
         printf("Prepare stmtSelectCache failed: %s\n", sqlite3_errmsg(dbHandler->sqlDb));
      }
      // delete statement
      if(sqlite3_prepare_v2(dbHandler->sqlDb, gSqlDeleteCache, (int)strlen(gSqlDeleteCache), &dbHandler->stmtDeleteCache, 0) != SQLITE_OK)
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("Prepare stmtDeleteCache failed:" ), DLT_STRING(sqlite3_errmsg(dbHandler->sqlDb)));
         printf("Prepare stmtDeleteCache failed: %s\n", sqlite3_errmsg(dbHandler->sqlDb));
      }


      // select deleted cache
      if(sqlite3_prepare_v2(dbHandler->sqlDb, gSqlSelectDelCache, (int)strlen(gSqlSelectDelCache), &dbHandler->stmtSelectDelCache, 0) != SQLITE_OK)
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("Prepare stmtSelectDelCache failed:" ), DLT_STRING(sqlite3_errmsg(dbHandler->sqlDb)));
         printf("Prepare stmtSelectDelCache failed: %s\n", sqlite3_errmsg(dbHandler->sqlDb));
      }

      // select modified cache
      if(sqlite3_prepare_v2(dbHandler->sqlDb, gSqlSelectModCacheAll, (int)strlen(gSqlSelectModCacheAll), &dbHandler->stmtSelectModCache, 0) != SQLITE_OK)
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("Prepare stmtSelectModCache failed:" ), DLT_STRING(sqlite3_errmsg(dbHandler->sqlDb)));
         printf("Prepare stmtSelectModCache failed: %s\n", sqlite3_errmsg(dbHandler->sqlDb));
      }

      // select from cache and non cached table
      if(sqlite3_prepare_v2(dbHandler->sqlDb, gSqlSelectCacheWt, (int)strlen(gSqlSelectCacheWt), &dbHandler->stmtSelectCacheWt, 0) != SQLITE_OK)
      {
         DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("Prepare stmtSelectModCache failed:" ), DLT_STRING(sqlite3_errmsg(dbHandler->sqlDb)));
         printf("Prepare stmtSelectModCache failed: %s\n", sqlite3_errmsg(dbHandler->sqlDb));
      }
   }
   else
   {
      DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING("dbHelper_compileSQLiteStmtfailedCache" ));
   }
}



void dbHelper_finalizeSQLiteStmt(lldb_handler_s* dbHandler)
{
   if(dbHandler != NULL)
   {
      sqlite3_finalize(dbHandler->stmtUpdate);

      sqlite3_finalize(dbHandler->stmtSelect);

      sqlite3_finalize(dbHandler->stmtSelectAll);

      sqlite3_finalize(dbHandler->stmtDelete);
   }
}


void dbHelper_finalizeSQLiteStmtCache(lldb_handler_s* dbHandler)
{
   if(dbHandler != NULL)
   {
      sqlite3_finalize(dbHandler->stmtUpdateCache);

      sqlite3_finalize(dbHandler->stmtSelectCache);

      sqlite3_finalize(dbHandler->stmtDeleteCache);

      sqlite3_finalize(dbHandler->stmtSelectDelCache);

      sqlite3_finalize(dbHandler->stmtSelectModCache);

      sqlite3_finalize(dbHandler->stmtSelectCacheWt);
   }
}


#ifdef SHOW_COMP_OPT
static int callbackCompileOpt(void *NotUsed, int argc, char **argv, char **azColName)
{
   int i;
   for(i=0; i<argc; i++)
   {
      printf("%s = %s\t\t", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");

   return 0;
}
#endif



#ifdef DB_DEBUG
static int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
   int i;
   for(i=0; i<argc; i++)
   {
      //printf("%s = %s\t\t", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");

   return 0;
}


static void dbHelper_printDatabaseContent(lldb_handler_s* dbHandler)
{
   char *sql = "SELECT * FROM KeyValueDB";
   char *sql2 = "SELECT * FROM KeyValueDBTmp";
   char *errMsg = 0;

   printf("\n**************************************************************************\n");
   printf("*** ***                   Database content                         *** ***\n");
   printf("**************************************************************************\n");
   printf("Database content -> non cached:\n -----------\n");
   if(sqlite3_exec(dbHandler->sqlDb, sql, callback, NULL, &errMsg) != SQLITE_OK )
   {
      printf("dbHelper_printDatabaseContent - non cache - => SQL error: %s\n", errMsg);
      sqlite3_free(errMsg);
   }
   printf(" -----------\n");

   if(dbHandler->bIsCached == true)
   {

      printf("\nDatabase content -> cached:\n -----------\n");
      if(sqlite3_exec(dbHandler->sqlDb, sql2, callback, NULL, &errMsg) != SQLITE_OK )
      {
        printf("dbHelper_printDatabaseContent - cached - => SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
      }
      printf(" -----------\n");
   }

#if 0
   printf("\nDatabase content -> temporary:\n -----------\n");
   if(sqlite3_exec(dbHandler->sqlDb, sql2, callback, NULL, &errMsg) != SQLITE_OK )
   {
     printf("dbHelper_printDatabaseContent - cached - => SQL error: %s\n", errMsg);
     sqlite3_free(errMsg);
   }
   printf(" -----------\n");
#endif

   printf("**************************************************************************\n");
}
#endif

