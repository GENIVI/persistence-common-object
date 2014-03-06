#ifndef PERSISTENCE_LOW_LEVEL_DB_ACCESS_H
#define PERSISTENCE_LOW_LEVEL_DB_ACCESS_H

/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*
* Author: Ionut.Ieremie@continental-corporation.com
*
* Interface TODO
*
* The file defines the interfaces TODO
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date       Author    Version  Reason
* 2013.02.05 uidl9757  1.0.0.0  CSP_WZ#2220:  Adaptation for open source
* 2013.01.03 uidl9757  1.0.0.0  CSP_WZ#2060:  Remove "cursor" interface
* 2012.12.17 uidl9757  1.0.0.0  CSP_WZ#2060:  Changes to allow optimized access to DB
* 2012.12.10 uidl9757  1.0.0.0  CSP_WZ#2060:  Initial version of the interface
*
**********************************************************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif  /* #ifdef __cplusplus */

#include "persComTypes.h"

#define PERSIST_LOW_LEVEL_DB_ACCESS_INTERFACE_VERSION  (0x03000000U)

/* The supported purposes of low level DBs 
 * Needed to allow different setups of DBs according to their purposes
 */
typedef enum pers_lldb_purpose_e_
{
    PersLldbPurpose_RCT = 0,    /* Resource-Configuration-Table */
    PersLldbPurpose_DB,         /* Local/Shared DB */
    /* add new entries here */
    PersLldbPurpose_LastEntry
}pers_lldb_purpose_e ;


/**
 * @brief write a key-value pair into database
 * @note : DB type is identified from dbPathname (based on extension)
 *
 * @param dbPathname                    [in] absolute path to DB
 * @param ePurpose                      [in] see pers_lldb_purpose_e
 * @param bForceCreationIfNotPresent    [in] if true, the DB is created if it does not exist
 *
 * @return >=0 for success, negative value otherway (see pers_error_codes.h)
 */
sint_t pers_lldb_open(str_t const * dbPathname, pers_lldb_purpose_e ePurpose, bool_t bForceCreationIfNotPresent) ;


/**
 * @brief write a key-value pair into database
 * @note : DB type is identified from dbPathname (based on extension)
 *
 * @param handlerDB     [in] handler obtained with pers_lldb_open
 *
 * @return 0 for success, negative value otherway (see pers_error_codes.h)
 */
sint_t pers_lldb_close(sint_t handlerDB) ;

/**
 * @brief write a key-value pair into database
 * @note : DB type is identified from dbPathname (based on extension)
 * @note : DB is created if it does not exist
 *
 * @param handlerDB     [in] handler obtained with pers_lldb_open
 * @param ePurpose      [in] see pers_lldb_purpose_e
 * @param key           [in] key's name
 * @param data          [in] buffer with key's data
 * @param dataSize      [in] size of key's data
 *
 * @return 0 for success, negative value otherway (see pers_error_codes.h)
 */
sint_t pers_lldb_write_key(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const * key, str_t const * data, sint_t dataSize) ;


/**
 * @brief read a key's value from database
 * @note : DB type is identified from dbPathname (based on extension)
 *
 * @param handlerDB         [in] handler obtained with pers_lldb_open
 * @param ePurpose          [in] see pers_lldb_purpose_e
 * @param key               [in] key's name
 * @param dataBuffer_out    [out]buffer where to return the read data
 * @param bufSize           [in] size of dataBuffer_out
 *
 * @return read size, or negative value in case of error (see pers_error_codes.h)
 */
sint_t pers_lldb_read_key(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const * key, pstr_t dataBuffer_out, sint_t bufSize) ;

/**
 * @brief read a key's value from database
 * @note : DB type is identified from dbPathname (based on extension)
 *
 * @param handlerDB         [in] handler obtained with pers_lldb_open
 * @param ePurpose          [in] see pers_lldb_purpose_e
 * @param key               [in] key's name
 * @return key's size, or negative value in case of error (see pers_error_codes.h)
 */
sint_t pers_lldb_get_key_size(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const * key) ;

/**
 * @brief delete key from database
 * @note : DB type is identified from dbPathname (based on extension)
 *
 * @param handlerDB         [in] handler obtained with pers_lldb_open
 * @param ePurpose          [in] see pers_lldb_purpose_e
 * @param key               [in] key's name
 *
 * @return 0 for success, negative value otherway (see pers_error_codes.h)
 */
sint_t pers_lldb_delete_key(sint_t handlerDB, pers_lldb_purpose_e ePurpose, str_t const * key) ;


/**
 * @brief Find the buffer's size needed to accomodate the listing of keys' names in database
 * @note : DB type is identified from dbPathname (based on extension)
 *
 * @param handlerDB         [in] handler obtained with pers_lldb_open
 * @param ePurpose          [in] see pers_lldb_purpose_e
 *
 * @return needed size, or negative value in case of error (see pers_error_codes.h)
 */
sint_t pers_lldb_get_size_keys_list(sint_t handlerDB, pers_lldb_purpose_e ePurpose) ;


/**
 * @brief List the keys' names in database
 * @note : DB type is identified from dbPathname (based on extension)
 * @note : keys are separated by '\0'
 *
 * @param handlerDB         [in] handler obtained with pers_lldb_open
 * @param ePurpose          [in] see pers_lldb_purpose_e
 * @param listingBuffer_out [out]buffer where to return the listing
 * @param bufSize           [in] size of listingBuffer_out
 *
 * @return listing size, or negative value in case of error (see pers_error_codes.h)
 */
 sint_t pers_lldb_get_keys_list(sint_t handlerDB, pers_lldb_purpose_e ePurpose, pstr_t listingBuffer_out, sint_t bufSize) ;



#ifdef __cplusplus
}
#endif /* extern "C" { */
/** \} */ /* End of API */
#endif /* PERSISTENCE_LOW_LEVEL_DB_ACCESS_H */

