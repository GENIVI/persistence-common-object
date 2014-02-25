/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*
* Author: Ionut.Ieremie@continental-corporation.com
*
* Implementation of persComDbAccess.h
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date             Author              Reason
* 2013.09.14       uidl9757            CSP_WZ#4872:  Improvements
*                                                       - synchronization between threads of the same process
*                                                       - number of maximum simultan open handles by a process is no longer limited
* 2013.08.30       uidl9757            CSP_WZ#5356:  persistency common library uses too much stack size 
* 2013.07.10       uidl9757            CSP_WZ#4586:  Add instrumentation for debug purposes
* 2013.03.21       uidl9757            CSP_WZ#3774:  Default error handler causes the termination of the calling process
* 2013.03.21       uidl9757            CSP_WZ#2798:  Workaround - reading from an emptied itzam db returns error
* 2013.02.05       uidl9757            CSP_WZ#2220:  Adaptation for open source
* 2013.01.03       uidl9757            CSP_WZ#2060:  Remove "cursor" interface
* 2012.12.17       uidl9757            CSP_WZ#2060:  Changes to allow optimized access to DB
* 2012.12.10       uidl9757            CSP_WZ#2060:  Created
*
**********************************************************************************************************************/

#include <pthread.h>
#include <stdio.h> /*DG C7MR2R-MISRA-C:2004 Rule 20.9-SSW_PersCommon_1003*/
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/types.h>
#include <unistd.h>

#include "persComTypes.h"

#include "itzam.h"

#include "persComErrors.h"
#include "persComDataOrg.h"
#include "persComDbAccess.h"
#include "persComRct.h"

#include "pers_low_level_db_access_if.h"

#include "dlt.h"
/* L&T context */
#define LT_HDR                          "[persComLLDB]"
DLT_DECLARE_CONTEXT                      (persComLldbDLTCtx);

/* ---------------------- local definition  ---------------------------- */
/* max number of open handlers per process */
#define PERS_LLDB_NO_OF_STATIC_HANDLES 16
#define PERS_LLDB_MAX_STATIC_HANDLES (PERS_LLDB_NO_OF_STATIC_HANDLES-1)

/* ---------------------- local types  --------------------------------- */
typedef enum {
  dbType_unknown = 0,
  dbType_itzam
  /* TODO: Add here IDs for supported DB engines */
} dbType_e;

typedef struct
{
    char m_key[PERS_DB_MAX_LENGTH_KEY_NAME];
    char m_data[PERS_DB_MAX_SIZE_KEY_DATA];
    int  m_dataSize ;
}
KeyValuePair_LocalDB_s;

typedef struct
{
    char m_key[PERS_RCT_MAX_LENGTH_RESOURCE_ID];
    char m_data[sizeof(PersistenceConfigurationKey_s)];
}
KeyValuePair_RCT_s;

typedef struct
{
    bool_t              bIsAssigned ;
    sint_t              dbHandler ;
    pers_lldb_purpose_e ePurpose ;
    itzam_btree         btree;
    str_t               dbPathname[PERS_ORG_MAX_LENGTH_PATH_FILENAME] ;
}lldb_handler_s ;

typedef struct lldb_handles_list_el_s_
{
    lldb_handler_s                   sHandle ;
    struct lldb_handles_list_el_s_ * pNext ;
}lldb_handles_list_el_s ;

typedef struct
{
    lldb_handler_s          asStaticHandles[PERS_LLDB_NO_OF_STATIC_HANDLES] ; /* static area should be enough for most of the processes*/
    lldb_handles_list_el_s* pListHead ; /* for the processes with a large number of threads which use Persistency */
}lldb_handlers_s ;

/* ---------------------- local variables  --------------------------------- */
static const char ListItemsSeparator = '\0';

/* shared by all the threads within a process */
static lldb_handlers_s g_sHandlers = {{{0}}} ;
static pthread_mutex_t g_mutexLldb = PTHREAD_MUTEX_INITIALIZER; /*DG C7MR2R-MISRA-C:2004 Rule 18.4-SSW_PersCommon_1013*/

/* ---------------------- local macros  --------------------------------- */



/* ---------------------- local functions  --------------------------------- */
static sint_t   DeleteDataFromItzamDB(        sint_t      dbHandler,      pconststr_t key );
static void     ErrorHandler(                 pconststr_t function_name,  itzam_error error);
static sint_t   GetAllKeysFromItzamLocalDB(   sint_t      dbHandler,      pstr_t      buffer,  sint_t   size );
static sint_t   GetAllKeysFromItzamRCT(       sint_t      dbHandler,      pstr_t      buffer,  sint_t   size );
static sint_t   GetKeySizeFromItzamLocalDB(   sint_t      dbHandler,      pconststr_t key) ;
static sint_t   GetDataFromItzamLocalDB(      sint_t      dbHandler,      pconststr_t key,     pstr_t   buffer_out,   sint_t  bufSize );
static sint_t   GetDataFromItzamRCT(          sint_t      dbHandler,      pconststr_t key,     PersistenceConfigurationKey_s* pConfig);
static sint_t   SetDataInItzamLocalDB(        sint_t      dbHandler,      pconststr_t key,     pconststr_t data,         sint_t  dataSize );
static sint_t   SetDataInItzamRCT(            sint_t      dbHandler,      pconststr_t key,     PersistenceConfigurationKey_s const * pConfig);
/* access to resources shared by the threads within a process */
static bool_t           lldb_handles_Lock(void);
static bool_t           lldb_handles_Unlock(void);
static lldb_handler_s*  lldb_handles_FindInUseHandle(sint_t dbHandler) ;
static lldb_handler_s*  lldb_handles_FindAvailableHandle(void) ;
static void             lldb_handles_InitHandle(lldb_handler_s* psHandle_inout, pers_lldb_purpose_e ePurpose, str_t const * dbPathname);
static bool_t           lldb_handles_DeinitHandle(sint_t dbHandler);


/**
 * \brief write a key-value pair into database
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
    sint_t returnValue = PERS_COM_FAILURE ;
    bool_t bCanContinue = true ;
    lldb_handler_s* pLldbHandler = NIL ;
    bool_t bLocked = false ;
    
    static bool_t bFirstCall = true ;

    if(bFirstCall)
    {
        pid_t pid = getpid() ;
        str_t dltContextID[16] ; /* should be at most 4 characters string, but colissions occure */
        
        /* set an error handler - the default one will cause the termination of the calling process */
        bFirstCall = false ;
        itzam_set_default_error_handler(&ErrorHandler) ;
        /* init DLT */
        (void)snprintf(dltContextID, sizeof(dltContextID), "Pers_%04d", pid) ;
        DLT_REGISTER_CONTEXT(persComLldbDLTCtx, dltContextID, "PersCommonLLDB");
        DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("register context PersCommonLLDB ContextID="); DLT_STRING(dltContextID));
    }

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("<<"); DLT_STRING(dbPathname); DLT_STRING(">>, ");
            ((PersLldbPurpose_RCT == ePurpose) ? DLT_STRING("RCT, ") : DLT_STRING("DB, "));
            ((true == bForceCreationIfNotPresent) ? DLT_STRING("forced, ") : DLT_STRING("unforced, "));
            DLT_STRING(" ... ")) ;

    if(lldb_handles_Lock())
    {
        bLocked = true ;
        pLldbHandler = lldb_handles_FindAvailableHandle() ;
        if(NIL == pLldbHandler)
        {
            bCanContinue = false ;
            returnValue = PERS_COM_ERR_OUT_OF_MEMORY ;
        }
    }
    else
    {
        bCanContinue = false ;
    }

    if(bCanContinue)
    {
        itzam_state state = ITZAM_NOT_FOUND;
        size_t treeEntrySize = (PersLldbPurpose_RCT == ePurpose) ? sizeof(KeyValuePair_RCT_s) : sizeof(KeyValuePair_LocalDB_s) ;
        
        state = itzam_btree_open( & pLldbHandler->btree, dbPathname, &itzam_comparator_string, &ErrorHandler, 0/*recover*/, 0/*read_only*/);
        if( state != ITZAM_OKAY )
        {
            if(bForceCreationIfNotPresent)
            {
                state = itzam_btree_create( & pLldbHandler->btree, dbPathname, ITZAM_BTREE_ORDER_DEFAULT, (itzam_int)treeEntrySize, &itzam_comparator_string, &ErrorHandler );
                if(ITZAM_OKAY != state)
                {
                    bCanContinue = false ;
                    returnValue = PERS_COM_FAILURE ;
                }
            }
            else
            {
                bCanContinue = false ;
                returnValue = PERS_COM_ERR_NOT_FOUND ;
            }
        }

        if(bCanContinue)
        {
            lldb_handles_InitHandle(pLldbHandler, ePurpose, dbPathname) ;            
            returnValue = pLldbHandler->dbHandler;
        }
        else
        {
            /* clean up */            
            returnValue = PERS_COM_FAILURE ;
            (void)lldb_handles_DeinitHandle(pLldbHandler->dbHandler) ;
        }
    }

    if(bLocked)
    {
        (void)lldb_handles_Unlock() ;
    }

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("<<"); DLT_STRING(dbPathname); DLT_STRING(">>, ");
            ((PersLldbPurpose_RCT == ePurpose) ? DLT_STRING("RCT, ") : DLT_STRING("DB, "));
            ((true == bForceCreationIfNotPresent) ? DLT_STRING("forced, ") : DLT_STRING("unforced, "));
            DLT_STRING("retval=<"); DLT_INT(returnValue); DLT_STRING(">")) ;
    
    return returnValue ;
}/*DG C7MR2R-ISQP Metric 10-SSW_PersCommon_0001*/ /*DG C7MR2R-ISQP Metric 6-SSW_PersCommon_1005*/


/**
 * \brief write a key-value pair into database
 * \note : DB type is identified from dbPathname (based on extension)
 *
 * \param handlerDB     [in] handler obtained with pers_lldb_open
 *
 * \return 0 for success, negative value otherway (see pers_error_codes.h)
 */
sint_t pers_lldb_close(sint_t handlerDB)
{
    sint_t returnValue = PERS_COM_SUCCESS ;
    lldb_handler_s* pLldbHandler = NIL ;
    bool_t bLocked = false ;
    str_t dbPathnameTemp[PERS_ORG_MAX_LENGTH_PATH_FILENAME] = "invalid path" ;

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("handlerDB="); DLT_INT(handlerDB); DLT_STRING("...")) ;

    if(handlerDB >= 0)
    {
        if(lldb_handles_Lock())
        {
            bLocked = true ;
            pLldbHandler = lldb_handles_FindInUseHandle(handlerDB) ;
            if(NIL == pLldbHandler)
            {
                returnValue = PERS_COM_FAILURE ;
            }
            else
            {
                /* to not use DLT while mutex locked */
                (void)strncpy(dbPathnameTemp, pLldbHandler->dbPathname, sizeof(dbPathnameTemp)) ;
            }
        }        
    }
    else
    {
        returnValue = PERS_COM_ERR_INVALID_PARAM ;
    }

    if(PERS_COM_SUCCESS == returnValue)
    {
        if(ITZAM_OKAY == itzam_btree_close( & pLldbHandler->btree))
        {
            if( ! lldb_handles_DeinitHandle(pLldbHandler->dbHandler))
            {
                returnValue = PERS_COM_FAILURE ;
            }
        }
        else
        {
            returnValue = PERS_COM_FAILURE ;
        }
    }

    if(bLocked)
    {
        (void)lldb_handles_Unlock() ;
    }

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("handlerDB="); DLT_INT(handlerDB);
            DLT_STRING("<<"); DLT_STRING(dbPathnameTemp); DLT_STRING(">>, ");
            DLT_STRING("retval=<"); DLT_INT(returnValue); DLT_STRING(">")) ;

    return returnValue ;
}/*DG C7MR2R-ISQP Metric 10-SSW_PersCommon_0001*/

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
sint_t pers_lldb_write_key(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const * key, str_t const * data, sint_t dataSize)
{
    sint_t eErrorCode = PERS_COM_SUCCESS ;

    switch(ePurpose)
    {
        case PersLldbPurpose_DB:
        {
            eErrorCode = SetDataInItzamLocalDB(handlerDB, key, data, dataSize) ;
            break ;
        }
        case PersLldbPurpose_RCT:
        {
            eErrorCode = SetDataInItzamRCT(handlerDB, key, (PersistenceConfigurationKey_s const *)data) ;
            break ;
        }
        default:
        {
            eErrorCode = PERS_COM_ERR_INVALID_PARAM ;
            break ;
        }
    }

    return eErrorCode ;
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
sint_t pers_lldb_read_key(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const * key, pstr_t dataBuffer_out, sint_t bufSize)
{
    sint_t eErrorCode = PERS_COM_SUCCESS ;

    switch(ePurpose)
    {
        case PersLldbPurpose_DB:
        {
            eErrorCode = GetDataFromItzamLocalDB(handlerDB, key, dataBuffer_out, bufSize) ;
            break ;
        }
        case PersLldbPurpose_RCT:
        {
            eErrorCode = GetDataFromItzamRCT(handlerDB, key, (PersistenceConfigurationKey_s*)dataBuffer_out) ;
            break ;
        }
        default:
        {
            eErrorCode = PERS_COM_ERR_INVALID_PARAM ;
            break ;
        }
    }

    return eErrorCode ;
}

/**
 * \brief read a key's value from database
 * \note : DB type is identified from dbPathname (based on extension)
 *
 * \param handlerDB         [in] handler obtained with pers_lldb_open
 * \param ePurpose          [in] see pers_lldb_purpose_e
 * \param key               [in] key's name
 * \return key's size, or negative value in case of error (see pers_error_codes.h)
 */
sint_t pers_lldb_get_key_size(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const * key)
{
    sint_t eErrorCode = PERS_COM_SUCCESS ;

    switch(ePurpose)
    {
        case PersLldbPurpose_DB:
        {
            eErrorCode = GetKeySizeFromItzamLocalDB(handlerDB, key) ;
            break ;
        }
        default:
        {
            eErrorCode = PERS_COM_ERR_INVALID_PARAM ;
            break ;
        }
    }

    return eErrorCode ;
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
    sint_t eErrorCode = PERS_COM_SUCCESS ;

    switch(ePurpose)
    {
        case PersLldbPurpose_DB:
        case PersLldbPurpose_RCT:
        {
            eErrorCode = DeleteDataFromItzamDB(handlerDB, key) ;
            break ;
        }
        default:
        {
            eErrorCode = PERS_COM_ERR_INVALID_PARAM ;
            break ;
        }
    }

    return eErrorCode ;
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
    sint_t eErrorCode = PERS_COM_SUCCESS ;

    switch(ePurpose)
    {
        case PersLldbPurpose_DB:
        {
            eErrorCode = GetAllKeysFromItzamLocalDB(handlerDB, NIL, 0) ;
            break ;
        }
        case PersLldbPurpose_RCT:
        {
            eErrorCode = GetAllKeysFromItzamRCT(handlerDB, NIL, 0) ;
            break ;
        }
        default:
        {
            eErrorCode = PERS_COM_ERR_INVALID_PARAM ;
            break ;
        }
    }

    return eErrorCode ;
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
    sint_t eErrorCode = PERS_COM_SUCCESS ;

    switch(ePurpose)
    {
        case PersLldbPurpose_DB:
        {
            eErrorCode = GetAllKeysFromItzamLocalDB(handlerDB, listingBuffer_out, bufSize) ;
            break ;
        }
        case PersLldbPurpose_RCT:
        {
            eErrorCode = GetAllKeysFromItzamRCT(handlerDB, listingBuffer_out, bufSize) ;
            break ;
        }
        default:
        {
            eErrorCode = PERS_COM_ERR_INVALID_PARAM ;
            break ;
        }
    }

    return eErrorCode ;
}

static sint_t DeleteDataFromItzamDB( sint_t dbHandler, pconststr_t key ) 
{
    bool_t bCanContinue = true ;
    sint_t delete_size = PERS_COM_FAILURE ;
    lldb_handler_s* pLldbHandler = NIL ;
    bool_t bLocked = false ;
    str_t dbPathnameTemp[PERS_ORG_MAX_LENGTH_PATH_FILENAME] = "invalid path" ;

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("handlerDB="); DLT_INT(dbHandler);
            DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>...")) ;

    if( (dbHandler >= 0) && (NIL != key))
    {
        if(lldb_handles_Lock())
        {
            bLocked = true ;
            pLldbHandler = lldb_handles_FindInUseHandle(dbHandler) ;
            if(NIL == pLldbHandler)
            {
                bCanContinue = false ;
            }
            else
            {
                /* to not use DLT while mutex locked */
                (void)strncpy(dbPathnameTemp, pLldbHandler->dbPathname, sizeof(dbPathnameTemp)) ;
            }
        }        
    }
    else
    {
        bCanContinue = false ;
    }


    if(bCanContinue)
    {
        switch(pLldbHandler->ePurpose)
        {
            case PersLldbPurpose_DB:
            {
                KeyValuePair_LocalDB_s search;
                if( itzam_true == itzam_btree_find( &pLldbHandler->btree, key, & search ) )
                {
                    if(ITZAM_OKAY == itzam_btree_remove( &pLldbHandler->btree, key ))
                    {
                        delete_size = search.m_dataSize ;
                    }
                    else
                    {
                        delete_size = PERS_COM_FAILURE ;
                    }
                }
                else
                {
                    delete_size = PERS_COM_ERR_NOT_FOUND ;
                }
                break ;
            }
            case PersLldbPurpose_RCT:
            {
                KeyValuePair_RCT_s search;
                if( itzam_true == itzam_btree_find( &pLldbHandler->btree, key, & search ) )
                {
                    if(ITZAM_OKAY == itzam_btree_remove( &pLldbHandler->btree, key ))
                    {
                        delete_size = sizeof(PersistenceConfigurationKey_s) ;
                    }
                    else
                    {
                        delete_size = PERS_COM_FAILURE ;
                    }
                }
                else
                {
                    delete_size = PERS_COM_ERR_NOT_FOUND ;
                }
                break ;
            }
            default:
            {
                break ;
            }
        }
    }

    if(bLocked)
    {
        (void)lldb_handles_Unlock() ;
    }

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("handlerDB="); DLT_INT(dbHandler);
            DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>, ");
            DLT_STRING("<<"); DLT_STRING(dbPathnameTemp); DLT_STRING(">>, ");
            DLT_STRING("retval=<"); DLT_INT(delete_size); DLT_STRING(">")) ;

    return delete_size;
}/*DG C7MR2R-ISQP Metric 10-SSW_PersCommon_0001*/

static void ErrorHandler(pconststr_t function_name, itzam_error error ) 
{
    (void)fprintf( stderr, "pers_lldb:ErrorHandler:Itzam error in %s: %d\n", function_name, (sint_t)error );
    DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
                DLT_STRING("ErrorHandler:Itzam error in "); DLT_STRING(function_name);
                DLT_STRING("error=<"); DLT_INT((sint_t)error); DLT_STRING(">") ) ;
}

static sint_t GetAllKeysFromItzamLocalDB( sint_t dbHandler, pstr_t buffer, sint_t size )
{
    bool_t bCanContinue = true ;
    itzam_btree_cursor cursor;
    sint_t availableSize = size;
    sint_t result = 0 ;
    KeyValuePair_LocalDB_s crtKey;
    bool_t bOnlySizeNeeded = (NIL == buffer) ;
    itzam_state itzState ;
    lldb_handler_s* pLldbHandler = NIL ;
    bool_t bLocked = false ;
    str_t dbPathnameTemp[PERS_ORG_MAX_LENGTH_PATH_FILENAME] = "invalid path" ;

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("dbHandler="); DLT_INT(dbHandler);
            DLT_STRING("buffer="); DLT_UINT((uint_t)buffer);
            DLT_STRING("size="); DLT_INT(size); DLT_STRING("...")) ;

    if(dbHandler >= 0)
    {
        if(lldb_handles_Lock())
        {
            bLocked = true ;
            pLldbHandler = lldb_handles_FindInUseHandle(dbHandler) ;
            if(NIL == pLldbHandler)
            {
                bCanContinue = false ;
                result = PERS_COM_ERR_INVALID_PARAM ;
            }
            else
            {
                if(PersLldbPurpose_DB != pLldbHandler->ePurpose)
                {/* this would be very bad */
                    bCanContinue = false ;
                    result = PERS_COM_FAILURE ;
                }
                /* to not use DLT while mutex locked */
                (void)strncpy(dbPathnameTemp, pLldbHandler->dbPathname, sizeof(dbPathnameTemp)) ;
            }
        }        
    }
    else
    {
        bCanContinue = false ;
        result = PERS_COM_ERR_INVALID_PARAM ;
    }

    if(bCanContinue)
    {
        if( ( buffer != NIL ) && ( size > 0 ) ) 
        {
            (void)memset( buffer, 0, (size_t)size );
        }

        itzState = itzam_btree_cursor_create( & cursor, & pLldbHandler->btree) ;
        if(ITZAM_OKAY == itzState)
        {
            (void)memset( & crtKey, 0, sizeof( crtKey ) );
            itzState = itzam_btree_cursor_read( & cursor, & crtKey ) ;
            if(ITZAM_OKAY == itzState)
            {
                /* Add the length of the key separator to the count */
                size_t keyLen = strnlen( crtKey.m_key, sizeof( crtKey.m_key ) ) ;
                if(keyLen > 0)
                {
                    if( (! bOnlySizeNeeded) && ( (sint_t)keyLen < availableSize ) )
                    {
                        (void)strncpy( buffer, crtKey.m_key, keyLen);
                        *(buffer+keyLen) = ListItemsSeparator; /*DG C7MR2R-MISRA-C:2004 Rule 17.4-SSW_PersCommon_1004*/
                        buffer += (keyLen + sizeof(ListItemsSeparator)) ; /*DG C7MR2R-MISRA-C:2004 Rule 17.4-SSW_PersCommon_1004*/
                        availableSize -= (sint_t)(keyLen + sizeof(ListItemsSeparator));
                    }
                    result += (sint_t)(keyLen + sizeof(ListItemsSeparator));
                }

                while ( itzam_btree_cursor_next( & cursor ) == itzam_true ) 
                {
                    (void)memset( & crtKey, 0, sizeof( crtKey ) );
                    if( ITZAM_OKAY == itzam_btree_cursor_read( & cursor, & crtKey ) ) 
                    {
                        /* Add the length of the key separator to the count */
                        keyLen = strnlen( crtKey.m_key, sizeof( crtKey.m_key ) ) ; /* + sizeof( ListItemsSeparator ); */
                        if(keyLen > 0)
                        {
                            if( (! bOnlySizeNeeded) && ( (sint_t)keyLen < availableSize ) )
                            {
                                (void)strncpy( buffer, crtKey.m_key, keyLen);
                                *(buffer+keyLen) = ListItemsSeparator; /*DG C7MR2R-MISRA-C:2004 Rule 17.4-SSW_PersCommon_1004*/
                                buffer += (keyLen + sizeof(ListItemsSeparator)) ; /*DG C7MR2R-MISRA-C:2004 Rule 17.4-SSW_PersCommon_1004*/
                                availableSize -= (sint_t)(keyLen + sizeof(ListItemsSeparator));
                            }
                            result += (sint_t)(keyLen + sizeof(ListItemsSeparator));
                        }  
                    }
                }

                (void)itzam_btree_cursor_free( & cursor );
            }
        }
        else
        {
            if(ITZAM_FAILED == itzState)
            {
                /* no data found */
                result = 0 ;
            }
            else
            {
                result = PERS_COM_FAILURE;
            }
        }
    }

    if(bLocked)
    {
        (void)lldb_handles_Unlock() ;
    }

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("dbHandler="); DLT_INT(dbHandler);
            DLT_STRING("<<"); DLT_STRING(dbPathnameTemp); DLT_STRING(">>, ");
            DLT_STRING("retval=<"); DLT_INT(result); DLT_STRING(">")) ;

    return result;
}/*DG C7MR2R-ISQP Metric 10-SSW_PersCommon_0001*/ /*DG C7MR2R-ISQP Metric 11-SSW_PersCommon_1001*/ /*DG C7MR2R-ISQP Metric 1-SSW_PersCommon_1006*/ /*DG C7MR2R-ISQP Metric 6-SSW_PersCommon_1007*/

static sint_t GetAllKeysFromItzamRCT( sint_t dbHandler, pstr_t buffer, sint_t size )
{
    bool_t bCanContinue = true ;
    itzam_btree_cursor cursor;
    sint_t availableSize = size;
    sint_t result = 0 ;
    KeyValuePair_RCT_s crtKey;
    bool_t bOnlySizeNeeded = (NIL == buffer) ;
    itzam_state itzState ;
    lldb_handler_s* pLldbHandler = NIL ;
    bool_t bLocked = false ;
    str_t dbPathnameTemp[PERS_ORG_MAX_LENGTH_PATH_FILENAME] = "invalid path" ;

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("dbHandler="); DLT_INT(dbHandler);
            DLT_STRING("buffer="); DLT_UINT((uint_t)buffer);
            DLT_STRING("size="); DLT_INT(size); DLT_STRING("...")) ;

    if(dbHandler >= 0)
    {
        if(lldb_handles_Lock())
        {
            bLocked = true ;
            pLldbHandler = lldb_handles_FindInUseHandle(dbHandler) ;
            if(NIL == pLldbHandler)
            {
                bCanContinue = false ;
                result = PERS_COM_ERR_INVALID_PARAM ;
            }
            else
            {
                if(PersLldbPurpose_RCT != pLldbHandler->ePurpose)
                {/* this would be very bad */
                    bCanContinue = false ;
                    result = PERS_COM_FAILURE ;
                }
                /* to not use DLT while mutex locked */
                (void)strncpy(dbPathnameTemp, pLldbHandler->dbPathname, sizeof(dbPathnameTemp)) ;
            }
        }        
    }
    else
    {
        bCanContinue = false ;
        result = PERS_COM_ERR_INVALID_PARAM ;
    }

    if(bCanContinue)
    {
        if( ( buffer != NIL ) && ( size > 0 ) ) 
        {
            (void)memset( buffer, 0, (size_t)size );
        }

        itzState = itzam_btree_cursor_create( & cursor, & pLldbHandler->btree) ;
        if(ITZAM_OKAY == itzState)
        {
            (void)memset( & crtKey, 0, sizeof( crtKey ) );
            itzState = itzam_btree_cursor_read( & cursor, & crtKey ) ;
            if(ITZAM_OKAY == itzState)
            {
                /* Add the length of the key separator to the count */
                size_t keyLen = strnlen( crtKey.m_key, sizeof( crtKey.m_key ) ) ;
                if(keyLen > 0)
                {
                    if( (! bOnlySizeNeeded) && ( (sint_t)keyLen < availableSize ) )
                    {
                        (void)strncpy( buffer, crtKey.m_key, keyLen);
                        *(buffer+keyLen) = ListItemsSeparator; /*DG C7MR2R-MISRA-C:2004 Rule 17.4-SSW_PersCommon_1004*/
                        buffer += (sint_t)(keyLen + sizeof(ListItemsSeparator)) ; /*DG C7MR2R-MISRA-C:2004 Rule 17.4-SSW_PersCommon_1004*/
                        availableSize -= (sint_t)(keyLen + sizeof(ListItemsSeparator));
                    }
                    result += (sint_t)(keyLen + sizeof(ListItemsSeparator));
                }

                while ( itzam_btree_cursor_next( & cursor ) == itzam_true ) 
                {
                    (void)memset( & crtKey, 0, sizeof( crtKey ) );
                    if( ITZAM_OKAY == itzam_btree_cursor_read( & cursor, & crtKey ) ) 
                    {
                        /* Add the length of the key separator to the count */
                        keyLen = strnlen( crtKey.m_key, sizeof( crtKey.m_key ) ) ; /* + sizeof( ListItemsSeparator ); */
                        if(keyLen > 0)
                        {
                            if( (! bOnlySizeNeeded) && ( (sint_t)keyLen < availableSize ) )
                            {
                                (void)strncpy( buffer, crtKey.m_key, keyLen);
                                *(buffer+keyLen) = ListItemsSeparator; /*DG C7MR2R-MISRA-C:2004 Rule 17.4-SSW_PersCommon_1004*/
                                buffer += (keyLen + sizeof(ListItemsSeparator)) ; /*DG C7MR2R-MISRA-C:2004 Rule 17.4-SSW_PersCommon_1004*/
                                availableSize -= (sint_t)(keyLen + sizeof(ListItemsSeparator));
                            }
                            result += (sint_t)(keyLen + sizeof(ListItemsSeparator));
                        }  
                    }
                }

                (void)itzam_btree_cursor_free( & cursor );
            }
        }
        else
        {
            if(ITZAM_FAILED == itzState)
            {
                /* no data found */
                result = 0 ;
            }
            else
            {
                result = PERS_COM_FAILURE;
            }
        }
    }

    if(bLocked)
    {
        (void)lldb_handles_Unlock() ;
    }

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("dbHandler="); DLT_INT(dbHandler);
            DLT_STRING("<<"); DLT_STRING(dbPathnameTemp); DLT_STRING(">>, ");
            DLT_STRING("retval=<"); DLT_INT(result); DLT_STRING(">")) ;

    return result;
}/*DG C7MR2R-ISQP Metric 10-SSW_PersCommon_0001*/ /*DG C7MR2R-ISQP Metric 11-SSW_PersCommon_0002*/ /*DG C7MR2R-ISQP Metric 1-SSW_PersCommon_1008*/ /*DG C7MR2R-ISQP Metric 6-SSW_PersCommon_1009*/


/* return no of bytes written, or negative value in case of error */
static sint_t SetDataInItzamLocalDB(sint_t dbHandler, pconststr_t key, pconststr_t data, sint_t dataSize)
{
    bool_t bCanContinue = true ;
    sint_t size_written = PERS_COM_FAILURE;
    lldb_handler_s* pLldbHandler = NIL ;
    bool_t bLocked = false ;
    str_t dbPathnameTemp[PERS_ORG_MAX_LENGTH_PATH_FILENAME] = "invalid path" ;

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("dbHandler="); DLT_INT(dbHandler);
            DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>, ");
            DLT_STRING("size<<"); DLT_INT(dataSize); DLT_STRING(">> ...")) ;

    if((    dbHandler >= 0)
        &&  (NIL != key)
        &&  (NIL != data)
        &&  (dataSize > 0) )
    {
        if(lldb_handles_Lock())
        {
            bLocked = true ;
            pLldbHandler = lldb_handles_FindInUseHandle(dbHandler) ;
            if(NIL == pLldbHandler)
            {
                bCanContinue = false ;
                size_written = PERS_COM_ERR_INVALID_PARAM ;
            }
            else
            {
                if(PersLldbPurpose_DB != pLldbHandler->ePurpose)
                {/* this would be very bad */
                    bCanContinue = false ;
                    size_written = PERS_COM_FAILURE ;
                }
                /* to not use DLT while mutex locked */
                (void)strncpy(dbPathnameTemp, pLldbHandler->dbPathname, sizeof(dbPathnameTemp)) ;
            }
        }        
    }
    else
    {
        bCanContinue = false ;
        size_written = PERS_COM_ERR_INVALID_PARAM ;
    }

    if(bCanContinue)
    {
        KeyValuePair_LocalDB_s search_insert; /* use a single variable to reduce stack size */

        if( itzam_true == itzam_btree_find( & pLldbHandler->btree, key, & search_insert ) )
        {            
            if(ITZAM_OKAY != itzam_btree_remove( & pLldbHandler->btree, key ))
            {
                bCanContinue = false ;
            }
        }

        if(bCanContinue)
        {
            (void)memset(search_insert.m_data, 0, sizeof(search_insert.m_data) );
            (void)strncpy(search_insert.m_key, key, sizeof(search_insert.m_key)) ;
            (void)memcpy(search_insert.m_data, data, (size_t)dataSize) ;
            search_insert.m_dataSize = dataSize ;
            if(ITZAM_OKAY == itzam_btree_insert( & pLldbHandler->btree, ( void * )( & search_insert ) ))
            {
                size_written = dataSize;
            }
        }
    }

    if(bLocked)
    {
        (void)lldb_handles_Unlock() ;
    }

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("dbHandler="); DLT_INT(dbHandler);
            DLT_STRING("<<"); DLT_STRING(dbPathnameTemp); DLT_STRING(">>, ");
            DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>, ");
            DLT_STRING("size<<"); DLT_INT(dataSize); DLT_STRING(">>, ");
            DLT_STRING("retval=<"); DLT_INT(size_written); DLT_STRING(">")) ;
    
    return size_written;
}/*DG C7MR2R-ISQP Metric 10-SSW_PersCommon_0001*/

static sint_t SetDataInItzamRCT( sint_t dbHandler, pconststr_t key, PersistenceConfigurationKey_s const * pConfig)
{
    bool_t bCanContinue = true ;
    sint_t size_written = PERS_COM_FAILURE;
    lldb_handler_s* pLldbHandler = NIL ;
    bool_t bLocked = false ;
    str_t dbPathnameTemp[PERS_ORG_MAX_LENGTH_PATH_FILENAME] = "invalid path" ;

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("dbHandler="); DLT_INT(dbHandler);
            DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>...")) ;

    if(     (dbHandler >= 0)
        &&  (NIL != key)
        &&  (NIL != pConfig) )
    {
        if(lldb_handles_Lock())
        {
            bLocked = true ;
            pLldbHandler = lldb_handles_FindInUseHandle(dbHandler) ;
            if(NIL == pLldbHandler)
            {
                bCanContinue = false ;
                size_written = PERS_COM_ERR_INVALID_PARAM ;
            }
            else
            {
                if(PersLldbPurpose_RCT != pLldbHandler->ePurpose)
                {/* this would be very bad */
                    bCanContinue = false ;
                    size_written = PERS_COM_FAILURE ;
                }                
                /* to not use DLT while mutex locked */
                (void)strncpy(dbPathnameTemp, pLldbHandler->dbPathname, sizeof(dbPathnameTemp)) ;
            }
        }        
    }
    else
    {
        bCanContinue = false ;
        size_written = PERS_COM_ERR_INVALID_PARAM ;
    }

    if(bCanContinue) 
    {
        KeyValuePair_RCT_s search, insert;

        (void)memset(insert.m_data, 0, sizeof(insert.m_data) );
        (void)strncpy(insert.m_key, key, sizeof(insert.m_key)) ;
        (void)memcpy(insert.m_data, pConfig, sizeof(PersistenceConfigurationKey_s)) ;

        if( itzam_true == itzam_btree_find( & pLldbHandler->btree, key, & search ) )
        {
            if(ITZAM_OKAY == itzam_btree_remove( & pLldbHandler->btree, key ))
            {
                if(ITZAM_OKAY == itzam_btree_insert( & pLldbHandler->btree, ( void * )( & insert ) ))
                {
                    size_written = (sint_t)sizeof(PersistenceConfigurationKey_s);
                }
            }
        }
        else
        {
            if(ITZAM_OKAY == itzam_btree_insert( & pLldbHandler->btree, ( void * )( & insert ) ))
            {
                size_written = (sint_t)sizeof(PersistenceConfigurationKey_s);
            }
        }
    }

    if(bLocked)
    {
        (void)lldb_handles_Unlock() ;
    }

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("dbHandler="); DLT_INT(dbHandler);
            DLT_STRING("<<"); DLT_STRING(dbPathnameTemp); DLT_STRING(">>, ");
            DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>, ");
            DLT_STRING("retval=<"); DLT_INT(size_written); DLT_STRING(">")) ;
    
    return size_written;
}/*DG C7MR2R-ISQP Metric 10-SSW_PersCommon_0001*/

/* return size of key, or negative value in case of error */
static sint_t GetKeySizeFromItzamLocalDB(sint_t dbHandler, pconststr_t key)
{
    bool_t bCanContinue = true ;
    sint_t size_read = PERS_COM_FAILURE ;
    lldb_handler_s* pLldbHandler = NIL ;
    bool_t bLocked = false ;
    str_t dbPathnameTemp[PERS_ORG_MAX_LENGTH_PATH_FILENAME] = "invalid path" ;

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("dbHandler="); DLT_INT(dbHandler);
            DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">> ...")) ;

    if((dbHandler >= 0) && (NIL != key))
    {
        if(lldb_handles_Lock())
        {
            bLocked = true ;
            pLldbHandler = lldb_handles_FindInUseHandle(dbHandler) ;
            if(NIL == pLldbHandler)
            {
                bCanContinue = false ;
                size_read = PERS_COM_ERR_INVALID_PARAM ;
            }
            else
            {
                if(PersLldbPurpose_DB != pLldbHandler->ePurpose)
                {/* this would be very bad */
                    bCanContinue = false ;
                    size_read = PERS_COM_FAILURE ;
                }
                /* to not use DLT while mutex locked */
                (void)strncpy(dbPathnameTemp, pLldbHandler->dbPathname, sizeof(dbPathnameTemp)) ;
            }
        }        
    }
    else
    {
        bCanContinue = false ;
        size_read = PERS_COM_ERR_INVALID_PARAM ;
    }

    if(bCanContinue)
    {
        KeyValuePair_LocalDB_s search;
        
        if( itzam_btree_find( & pLldbHandler->btree, key, & search ) == itzam_true )
        {
            size_read = search.m_dataSize ;
        }
        else
        {
            size_read = PERS_COM_ERR_NOT_FOUND ;
        }
    }

    if(bLocked)
    {
        (void)lldb_handles_Unlock() ;
    }

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("dbHandler="); DLT_INT(dbHandler);
            DLT_STRING("<<"); DLT_STRING(dbPathnameTemp); DLT_STRING(">>, ");
            DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>, ");
            DLT_STRING("retval=<"); DLT_INT(size_read); DLT_STRING(">")) ;
    
    return size_read;
}/*DG C7MR2R-ISQP Metric 10-SSW_PersCommon_0001*/

/* return no of bytes read, or negative value in case of error */
static sint_t GetDataFromItzamLocalDB(sint_t dbHandler, pconststr_t key, pstr_t buffer_out, sint_t bufSize)
{
    bool_t bCanContinue = true ;
    sint_t size_read = PERS_COM_FAILURE ;
    lldb_handler_s* pLldbHandler = NIL ;
    bool_t bLocked = false ;
    str_t dbPathnameTemp[PERS_ORG_MAX_LENGTH_PATH_FILENAME] = "invalid path" ;

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("dbHandler="); DLT_INT(dbHandler);;
            DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>, ");
            DLT_STRING("bufsize=<<"); DLT_INT(bufSize); DLT_STRING(">> ... ")) ;

    if(     (dbHandler >= 0)
        &&  (NIL != key)
        &&  (NIL != buffer_out)
        &&  (bufSize > 0) )
    {
        if(lldb_handles_Lock())
        {
            bLocked = true ;
            pLldbHandler = lldb_handles_FindInUseHandle(dbHandler) ;
            if(NIL == pLldbHandler)
            {
                bCanContinue = false ;
                size_read = PERS_COM_ERR_INVALID_PARAM ;
            }
            else
            {
                if(PersLldbPurpose_DB != pLldbHandler->ePurpose)
                {/* this would be very bad */
                    bCanContinue = false ;
                    size_read = PERS_COM_FAILURE ;
                }
                /* to not use DLT while mutex locked */
                (void)strncpy(dbPathnameTemp, pLldbHandler->dbPathname, sizeof(dbPathnameTemp)) ;
            }
        }        
    }
    else
    {
        bCanContinue = false ;
        size_read = PERS_COM_ERR_INVALID_PARAM ;
    }
    
    if(bCanContinue)
    {
        KeyValuePair_LocalDB_s search;

        if( itzam_btree_find( & pLldbHandler->btree, key, & search ) == itzam_true )
        {
            if( bufSize >= search.m_dataSize)
            {
                size_read = search.m_dataSize ;
                (void)memcpy(buffer_out, search.m_data, (size_t)size_read) ;
            }
            else
            {
                size_read = PERS_COM_ERR_BUFFER_TOO_SMALL ;
            }
        }
        else
        {
            size_read = PERS_COM_ERR_NOT_FOUND ;
        }
    }

    if(bLocked)
    {
        (void)lldb_handles_Unlock() ;
    }

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("dbHandler="); DLT_INT(dbHandler);
            DLT_STRING("<<"); DLT_STRING(dbPathnameTemp); DLT_STRING(">>, ");
            DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>, ");
            DLT_STRING("bufsize=<<"); DLT_INT(bufSize); DLT_STRING(">>, ");
            DLT_STRING("retval=<"); DLT_INT(size_read); DLT_STRING(">")) ;
    
    return size_read;
}/*DG C7MR2R-ISQP Metric 10-SSW_PersCommon_0001*/

static sint_t GetDataFromItzamRCT( sint_t dbHandler, pconststr_t key, PersistenceConfigurationKey_s* pConfig)
{
    bool_t bCanContinue = true ;
    sint_t size_read = PERS_COM_FAILURE ;
    lldb_handler_s* pLldbHandler = NIL ;
    bool_t bLocked = false ;
    str_t dbPathnameTemp[PERS_ORG_MAX_LENGTH_PATH_FILENAME] = "invalid path" ;

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("dbHandler="); DLT_INT(dbHandler);
            DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">> ...")) ;

    if(     (dbHandler >= 0)
        &&  (NIL != key)
        &&  (NIL != pConfig) )
    {
        if(lldb_handles_Lock())
        {
            bLocked = true ;
            pLldbHandler = lldb_handles_FindInUseHandle(dbHandler) ;
            if(NIL == pLldbHandler)
            {
                bCanContinue = false ;
                size_read = PERS_COM_ERR_INVALID_PARAM ;
            }
            else
            {
                if(PersLldbPurpose_RCT != pLldbHandler->ePurpose)
                {/* this would be very bad */
                    bCanContinue = false ;
                    size_read = PERS_COM_FAILURE ;
                }
                /* to not use DLT while mutex locked */
                (void)strncpy(dbPathnameTemp, pLldbHandler->dbPathname, sizeof(dbPathnameTemp)) ;
            }
        }        
    }
    else
    {
        bCanContinue = false ;
        size_read = PERS_COM_ERR_INVALID_PARAM ;
    }

    if(bCanContinue)
    {
        KeyValuePair_RCT_s search;

        if(itzam_true == itzam_btree_find( & pLldbHandler->btree, key, & search ) )
        {
            (void)memcpy(pConfig, &(search.m_data), sizeof(PersistenceConfigurationKey_s) );
            size_read = sizeof(PersistenceConfigurationKey_s);
        }
        else
        {
            size_read = PERS_COM_ERR_NOT_FOUND ;
        }
    }

    if(bLocked)
    {
        (void)lldb_handles_Unlock() ;
    }

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("dbHandler="); DLT_INT(dbHandler);
            DLT_STRING("<<"); DLT_STRING(dbPathnameTemp); DLT_STRING(">>, ");
            DLT_STRING("key=<<"); DLT_STRING(key); DLT_STRING(">>, ");
            DLT_STRING("retval=<"); DLT_INT(size_read); DLT_STRING(">")) ;
    
    return size_read;
}/*DG C7MR2R-ISQP Metric 10-SSW_PersCommon_0001*/

static bool_t lldb_handles_Lock(void)
{
    bool_t bEverythingOK = true ;
    sint_t siErr = pthread_mutex_lock(&g_mutexLldb) ;
    if(0 != siErr)
    {
        bEverythingOK = false ;
        DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("pthread_mutex_lock failed with error=<"); DLT_INT(siErr); DLT_STRING(">")) ;
    }

    return bEverythingOK ;
}

static bool_t lldb_handles_Unlock(void)
{
    bool_t bEverythingOK = true ;
    sint_t siErr = pthread_mutex_unlock (&g_mutexLldb) ;
    if(0 != siErr)
    {
        bEverythingOK = false ;
        DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
            DLT_STRING("pthread_mutex_unlock failed with error=<"); DLT_INT(siErr); DLT_STRING(">")) ;
    }

    return bEverythingOK ;
}

/* it is assumed dbHandler is checked by the caller */
static lldb_handler_s* lldb_handles_FindInUseHandle(sint_t dbHandler)
{
    lldb_handler_s* pHandler = NIL ;
    
    if(dbHandler <= PERS_LLDB_MAX_STATIC_HANDLES)
    {
        if(g_sHandlers.asStaticHandles[dbHandler].bIsAssigned)
        {
            pHandler = &g_sHandlers.asStaticHandles[dbHandler] ;
        }
    }
    else
    {
        lldb_handles_list_el_s* pListElemCurrent = g_sHandlers.pListHead ;
        while(NIL != pListElemCurrent)
        {
            if(dbHandler == pListElemCurrent->sHandle.dbHandler)
            {
                pHandler = &pListElemCurrent->sHandle;             
                break ;
            }
            pListElemCurrent = pListElemCurrent->pNext ;
        }
    }

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
        DLT_STRING((NIL!=pHandler) ? "Found handler <" : "ERROR can't find handler <"); DLT_INT(dbHandler); DLT_STRING(">");
        DLT_STRING((NIL!=pHandler) ? (dbHandler <= PERS_LLDB_MAX_STATIC_HANDLES ? "in static area" : "in dynamic list") : "")) ;

    return pHandler ;
}

static lldb_handler_s* lldb_handles_FindAvailableHandle(void)
{
    bool_t bCanContinue = true ;
    lldb_handler_s* pHandler = NIL ;
    lldb_handles_list_el_s* psListElemNew = NIL ;

    /* first try to find an available handle in the static area */
    sint_t siIndex = 0 ;
    for(siIndex = 0 ; siIndex <= PERS_LLDB_MAX_STATIC_HANDLES ; siIndex++)
    {
        if( ! g_sHandlers.asStaticHandles[siIndex].bIsAssigned)
        {
            /* index setting should be done only once at the initialization of the static array
             * Anyway, doing it here is more consistent  */
            g_sHandlers.asStaticHandles[siIndex].dbHandler = siIndex ;
            pHandler = &g_sHandlers.asStaticHandles[siIndex] ;
            break ;
        }
    }

    if(NIL == pHandler)
    {
        /* no position available in the static array => we have to use the list
         * allocate memory for the new element and process the situation when the list is headless */

        psListElemNew = (lldb_handles_list_el_s*)malloc(sizeof(lldb_handles_list_el_s)) ; /*DG C7MR2R-MISRA-C:2004 Rule 20.4-SSW_PersCommon_1010*/
        if(NIL == psListElemNew)
        {
            bCanContinue = false ;
            DLT_LOG(persComLldbDLTCtx, DLT_LOG_ERROR, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
                DLT_STRING("malloc failed")) ;
        }
        else
        {
            if(NIL == g_sHandlers.pListHead)
            {
                /* the list not yet used/created, so use the new created element as the head */
                g_sHandlers.pListHead = psListElemNew ;
                g_sHandlers.pListHead->pNext = NIL ;
                g_sHandlers.pListHead->sHandle.dbHandler = PERS_LLDB_MAX_STATIC_HANDLES + 1 ;
                /* the rest of the members will be set by lldb_handles_InitHandle */
                pHandler = &psListElemNew->sHandle;
            }
        }
    }

    if((NIL == pHandler) && bCanContinue)
    {
        /* no position available in the static array => we have to use the list
         * the memory for psListElemNew has been allocated and the list has a head
         * The new element has to get the smallest index 
         * Now lets consider the situation when the head of the list has an index higher than (PERS_LLDB_MAX_STATIC_HANDLES + 1)
         * => the list will have a new head !!! */
        if(g_sHandlers.pListHead->sHandle.dbHandler > (PERS_LLDB_MAX_STATIC_HANDLES + 1))
        {
            psListElemNew->pNext = g_sHandlers.pListHead ;
            psListElemNew->sHandle.dbHandler = PERS_LLDB_MAX_STATIC_HANDLES + 1 ;
            /* the rest of the members will be set by lldb_handles_InitHandle */
            g_sHandlers.pListHead = psListElemNew ;
            pHandler = &psListElemNew->sHandle;
        }
    }

    if((NIL == pHandler) && bCanContinue)
    {
        /* no position available in the static array => we have to use the list
         * the memory for psListElemNew has been allocated and the list has a head (with the smallest index)
         * The new element has to get the smallest available index 
         * So will search for the first gap between two consecutive elements of the list and will introduce the new element between */
        lldb_handles_list_el_s* pListElemCurrent1 = g_sHandlers.pListHead ;
        lldb_handles_list_el_s* pListElemCurrent2 = pListElemCurrent1->pNext;
        while(NIL != pListElemCurrent2)
        {
            if(pListElemCurrent2->sHandle.dbHandler - pListElemCurrent1->sHandle.dbHandler > 1)
            {
                /* found a gap => insert the element between and use the index next to pListElemCurrent1's */
                psListElemNew->pNext = pListElemCurrent2 ;
                psListElemNew->sHandle.dbHandler = pListElemCurrent1->sHandle.dbHandler + 1 ;
                pListElemCurrent1->pNext = psListElemNew ;
                pHandler = &psListElemNew->sHandle;
                break ;
            }
            else
            {
                pListElemCurrent1 = pListElemCurrent2 ;
                pListElemCurrent2 = pListElemCurrent2->pNext ;
            }
        }
        if(NIL == pListElemCurrent2)
        {
            /* reached the end of the list => the list will have a new end */
            psListElemNew->pNext = NIL ;
            psListElemNew->sHandle.dbHandler = pListElemCurrent1->sHandle.dbHandler + 1 ;
            pListElemCurrent1->pNext = psListElemNew ;
            pHandler = &psListElemNew->sHandle;
        }        
    }

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
        DLT_STRING((NIL!=pHandler) ? "Found availble handler <" : "ERROR can't find available handler <"); 
        DLT_INT((NIL!=pHandler) ? pHandler->dbHandler : (-1)); DLT_STRING(">");
        DLT_STRING((NIL!=pHandler) ? (pHandler->dbHandler <= PERS_LLDB_MAX_STATIC_HANDLES ? "in static area" : "in dynamic list") : "") ) ;

    return pHandler ;
}/*DG C7MR2R-ISQP Metric 6-SSW_PersCommon_1011*/

static void lldb_handles_InitHandle(lldb_handler_s* psHandle_inout, pers_lldb_purpose_e ePurpose, str_t const * dbPathname)
{
    psHandle_inout->bIsAssigned = true ;
    psHandle_inout->ePurpose = ePurpose ;
    (void)strncpy(psHandle_inout->dbPathname, dbPathname, sizeof(psHandle_inout->dbPathname)) ;
}

static bool_t lldb_handles_DeinitHandle(sint_t dbHandler)
{
    bool_t bEverythingOK = true ;
    bool_t bHandlerFound = false ;
        

    if(dbHandler <= PERS_LLDB_MAX_STATIC_HANDLES)
    {
        bHandlerFound = true ;
        g_sHandlers.asStaticHandles[dbHandler].bIsAssigned = false ;
    }
    else
    {
        /* consider the situation when the handle is the head of the list */
        if(NIL != g_sHandlers.pListHead)
        {
            if(dbHandler == g_sHandlers.pListHead->sHandle.dbHandler)
            {
                lldb_handles_list_el_s* pListElemTemp = NIL ;
                
                bHandlerFound = true ;
                pListElemTemp = g_sHandlers.pListHead ;
                g_sHandlers.pListHead = g_sHandlers.pListHead->pNext ;
                free(pListElemTemp) ; /*DG C7MR2R-MISRA-C:2004 Rule 20.4-SSW_PersCommon_1012*/
            }
        }
        else
        {
            bEverythingOK = false ;
        }
    }

    if(bEverythingOK && ( ! bHandlerFound))
    {
        /* consider the situation when the handle is in the list (but not the head) */
        lldb_handles_list_el_s* pListElemCurrent1 = g_sHandlers.pListHead ;
        lldb_handles_list_el_s* pListElemCurrent2 = pListElemCurrent1->pNext;
        while(NIL != pListElemCurrent2)
        {
            if(dbHandler == pListElemCurrent2->sHandle.dbHandler)
            {
                /* found the handle */
                bHandlerFound = true ;
                pListElemCurrent1->pNext = pListElemCurrent2->pNext ;
                free(pListElemCurrent2) ; /*DG C7MR2R-MISRA-C:2004 Rule 20.4-SSW_PersCommon_1013*/
                break ;
            }
            else
            {
                pListElemCurrent1 = pListElemCurrent2 ;
                pListElemCurrent2 = pListElemCurrent2->pNext ;
            }
        }
        if(NIL == pListElemCurrent2)
        {
            /* reached the end of the list without finding the handle */
            bEverythingOK = false ;
        }   
    }

    DLT_LOG(persComLldbDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR); DLT_STRING(__FUNCTION__); DLT_STRING(":");
        DLT_STRING("dbHandler=<"); DLT_INT(dbHandler); DLT_STRING("> ");
        DLT_STRING(bEverythingOK ? (dbHandler <= PERS_LLDB_MAX_STATIC_HANDLES ? "deinit handler in static area" : "deinit handler in dynamic list") : "ERROR - handler not found") ) ;

    return bEverythingOK ;
}
