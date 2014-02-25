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
* 2013.02.05       uidl9757            CSP_WZ#2220:  Adaptation for open source
* 2013.01.03       uidl9757            CSP_WZ#2060:  Remove "cursor" interface
* 2012.12.17       uidl9757            CSP_WZ#2060:  Changes to allow optimized access to DB
* 2012.12.10       uidl9757            CSP_WZ#2060:  Created
*
**********************************************************************************************************************/

#include "persComTypes.h"
#include <stdio.h>
#include "string.h"

#include "persComDataOrg.h"
#include "pers_low_level_db_access_if.h"
#include "persComDbAccess.h"
#include "persComErrors.h"

/**
 * \brief Obtain a handler to DB indicated by dbPathname
 * \note : DB is created if it does not exist and (bForceCreationIfNotPresent != 0)
 *
 * \param dbPathname                    [in] absolute path to database (length limited to \ref PERS_ORG_MAX_LENGTH_PATH_FILENAME)
 * \param bForceCreationIfNotPresent    [in] if !=0x0, the database is created if it does not exist
 *
 * \return >= 0 for valid handler, negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComDbOpen(char const * dbPathname, unsigned char bForceCreationIfNotPresent)
{
    sint_t iErrCode = PERS_COM_SUCCESS ;

    if(NIL != dbPathname)
    {
        if(strlen(dbPathname) >= PERS_ORG_MAX_LENGTH_PATH_FILENAME)
        {
            iErrCode = PERS_COM_ERR_INVALID_PARAM ;
        }
    }
    else
    {
        iErrCode = PERS_COM_ERR_INVALID_PARAM ;
    }

    if(PERS_COM_SUCCESS == iErrCode)
    {
        iErrCode = pers_lldb_open(dbPathname, PersLldbPurpose_DB, bForceCreationIfNotPresent) ;
    }

    return iErrCode ;
}

/**
 * \brief Close handler to DB
 *
 * \param handlerDB     [in] handler obtained with persComDbOpen
 *
 * \return 0 for success, negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComDbClose(signed int handlerDB)
{
    sint_t iErrCode = PERS_COM_SUCCESS ;

    if(handlerDB < 0)
    {
        iErrCode = PERS_COM_ERR_INVALID_PARAM ;
    }

    if(PERS_COM_SUCCESS == iErrCode)
    {
        iErrCode = pers_lldb_close(handlerDB) ;
    }

    return iErrCode ;
}

/**
 * \brief write a key-value pair into local/shared database
 *
 * \param handlerDB     [in] handler obtained with persComDbOpen
 * \param key           [in] key's name (length limited to \ref PERS_DB_MAX_LENGTH_KEY_NAME)
 * \param data          [in] buffer with key's data
 * \param dataSize      [in] size of key's data (max allowed \ref PERS_DB_MAX_SIZE_KEY_DATA)
 *
 * \return 0 for success, negative value otherwise (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComDbWriteKey(signed int handlerDB, char const * key, char const * data, signed int dataSize)
{
    sint_t iErrCode = PERS_COM_SUCCESS ;

    if(     (handlerDB < 0)
        ||  (NIL == key)
        ||  (NIL == data)
        ||  (dataSize <= 0)
        ||  (dataSize > PERS_DB_MAX_SIZE_KEY_DATA)
    )
    {
        iErrCode = PERS_COM_ERR_INVALID_PARAM ;
    }
    else
    {
        if(strlen(key) >= PERS_DB_MAX_LENGTH_KEY_NAME)
        {
            iErrCode = PERS_COM_ERR_INVALID_PARAM ;
        }
    }

    if(PERS_COM_SUCCESS == iErrCode)
    {
        iErrCode = pers_lldb_write_key(handlerDB, PersLldbPurpose_DB, key, data, dataSize) ;
    }

    return iErrCode ;
}


/**
 * \brief read a key's value from local/shared database
 *
 * \param handlerDB         [in] handler obtained with persComDbOpen
 * \param key               [in] key's name (length limited to \ref PERS_DB_MAX_LENGTH_KEY_NAME)
 * \param dataBuffer_out    [out]buffer where to return the read data
 * \param dataBufferSize    [in] size of dataBuffer_out
 *
 * \return read size, or negative value in case of error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComDbReadKey(signed int handlerDB, char const * key, char* dataBuffer_out, signed int dataBufferSize)
{
    sint_t iErrCode = PERS_COM_SUCCESS ;

    if(     (handlerDB < 0)
        ||  (NIL == key)
        ||  (NIL == dataBuffer_out)
        ||  (dataBufferSize <= 0)
    )
    {
        iErrCode = PERS_COM_ERR_INVALID_PARAM ;
    }
    else
    {
        if(strlen(key) >= PERS_DB_MAX_LENGTH_KEY_NAME)
        {
            iErrCode = PERS_COM_ERR_INVALID_PARAM ;
        }
    }

    if(PERS_COM_SUCCESS == iErrCode)
    {
        iErrCode = pers_lldb_read_key(handlerDB, PersLldbPurpose_DB, key, dataBuffer_out, dataBufferSize) ;
    }

    return iErrCode ;
}

/**
 * \brief read a key's value from local/shared database
 *
 * \param handlerDB         [in] handler obtained with persComDbOpen
 * \param key               [in] key's name (length limited to \ref PERS_DB_MAX_LENGTH_KEY_NAME)
 *
 * \return key's size, or negative value in case of error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComDbGetKeySize(signed int handlerDB, char const * key)
{
    sint_t iErrCode = PERS_COM_SUCCESS ;

    if(     (handlerDB < 0)
        ||  (NIL == key)
    )
    {
        iErrCode = PERS_COM_ERR_INVALID_PARAM ;
    }
    else
    {
        if(strlen(key) >= PERS_DB_MAX_LENGTH_KEY_NAME)
        {
            iErrCode = PERS_COM_ERR_INVALID_PARAM ;
        }
    }

    if(PERS_COM_SUCCESS == iErrCode)
    {
        iErrCode = pers_lldb_get_key_size(handlerDB, PersLldbPurpose_DB, key) ;
    }

    return iErrCode ;
}

/**
 * \brief delete key from local/shared database
 *
 * \param handlerDB     [in] handler obtained with persComDbOpen
 * \param key           [in] key's name (length limited to \ref PERS_DB_MAX_LENGTH_KEY_NAME)
 *
 * \return 0 for success, negative value otherwise (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComDbDeleteKey(signed int handlerDB, char const * key)
{
    sint_t iErrCode = PERS_COM_SUCCESS ;

    if(     (handlerDB < 0)
        ||  (NIL == key)
    )
    {
        iErrCode = PERS_COM_ERR_INVALID_PARAM ;
    }
    else
    {
        if(strlen(key) >= PERS_DB_MAX_LENGTH_KEY_NAME)
        {
            iErrCode = PERS_COM_ERR_INVALID_PARAM ;
        }
    }

    if(PERS_COM_SUCCESS == iErrCode)
    {
        iErrCode = pers_lldb_delete_key(handlerDB, PersLldbPurpose_DB, key) ;
    }

    return iErrCode ;
}


/**
 * \brief Find the buffer's size needed to accomodate the list of keys' names in local/shared database
 *
 * \param handlerDB     [in] handler obtained with persComDbOpen
 *
 * \return needed size, or negative value in case of error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComDbGetSizeKeysList(signed int handlerDB)
{
    sint_t iErrCode = PERS_COM_SUCCESS ;

    if(handlerDB < 0)
    {
        iErrCode = PERS_COM_ERR_INVALID_PARAM ;
    }

    if(PERS_COM_SUCCESS == iErrCode)
    {
        iErrCode = pers_lldb_get_size_keys_list(handlerDB, PersLldbPurpose_DB) ;
    }

    return iErrCode ;
}


/**
 * \brief Obtain the list of the keys' names in local/shared database
 * \note : keys in the list are separated by '\0'
 *
 * \param handlerDB         [in] handler obtained with persComDbOpen
 * \param listBuffer_out    [out]buffer where to return the list of keys
 * \param listBufferSize    [in] size of listingBuffer_out
 * \return >=0 for size of the list, or negative value in case of error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComDbGetKeysList(signed int handlerDB, char* listBuffer_out, signed int listBufferSize)
 {
    sint_t iErrCode = PERS_COM_SUCCESS ;

    if(     (handlerDB < 0)
        ||  (NIL == listBuffer_out)
        ||  (listBufferSize <= 0)
    )
    {
        iErrCode = PERS_COM_ERR_INVALID_PARAM ;
    }

    if(PERS_COM_SUCCESS == iErrCode)
    {
        iErrCode = pers_lldb_get_keys_list(handlerDB, PersLldbPurpose_DB, listBuffer_out, listBufferSize) ;
    }

    return iErrCode ;
}

