#ifndef OSS_PERSISTENCE_COMMON_DB_ACCESS_H
#define OSS_PERSISTENCE_COMMON_DB_ACCESS_H

/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*
* Author: Ionut.Ieremie@continental-corporation.com
*         guy.sagnes@continental-corporation.com
*
* Interface: protected - Access to local and shared DBs   
*
* For additional details see
* https://collab.genivi.org/wiki/display/genivi/SysInfraEGPersistenceConceptInterface   
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date       Author             Reason
* 2015.03.05 uid66235  5.0.0.0  merge of the interface extension provided by MentorGraphic:
*                               - Default Max size of the key descreased to 8KiB
*                               - add function persComDbgetMaxKeyValueSize()
*                               - add bitfield definition for persComDbOpen() bOption: create, write through, read only
* 2013.01.23 uidl9757  4.0.0.0  CSP_WZ#2798:  Change PERS_DB_MAX_SIZE_KEY_DATA to 16KiB 
* 2013.01.23 uidl9757  3.0.0.0  CSP_WZ#2060:  CoC_SSW:Persistence: common interface to be used by both PCL and PAS 
*
**********************************************************************************************************************/

/** \defgroup PERS_COM_DB_ACCESS Database access API
 *  \{
 */

#ifdef __cplusplus
extern "C"
{
#endif  /* #ifdef __cplusplus */

/** \defgroup PERS_DB_ACCESS_IF_VERSION Interface version
 *  \{
 */
#define PERS_COM_DB_ACCESS_INTERFACE_VERSION  (0x05000000U)
/** \} */ 



/** \defgroup PERS_DB_ACCESS_CONFIG Database configurations
 *  \{
 */
/* maximum data size for a key type resourceID */
#define PERS_DB_MAX_LENGTH_KEY_NAME 128   /**< Max. length of the key identifier */
#define PERS_DB_MAX_SIZE_KEY_DATA   8028  /**< Max. size of the key entry (slot definition) */
/** \} */


/** \defgroup PERS_DB_ACCESS_FUNCTIONS Functions
 *  \{
 */

 /**
 * \brief returns the max key data size supported in the database
 * \remarks: the key with higher size should be managed as file or splitted
 * \return max size in byte
 */
int persComDbgetMaxKeyValueSize(void);


/**
 * \brief Obtain a handler to DB indicated by dbPathname
 * \note : DB is created if it does not exist and (bForceCreationIfNotPresent != 0)
 *
 * \param dbPathname    [in] absolute path to database (length limited to \ref PERS_ORG_MAX_LENGTH_PATH_FILENAME)
 * \param bOption       [in] bitfield option: 0x01: create if not exists, 0x02: write through, 0x04: read only
 * \Remarks the support of the option depends from backend database realisation
 * \return >= 0 for valid handler, negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComDbOpen(char const * dbPathname, unsigned char bOption);

/**
 * \brief Close handler to DB
 *
 * \param handlerDB     [in] handler obtained with persComDbOpen
 *
 * \return 0 for success, negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComDbClose(signed int handlerDB) ;

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
signed int persComDbWriteKey(signed int handlerDB, char const * key, char const * data, signed int dataSize) ;


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
signed int persComDbReadKey(signed int handlerDB, char const * key, char* dataBuffer_out, signed int dataBufferSize) ;

/**
 * \brief read a key's value from local/shared database
 *
 * \param handlerDB         [in] handler obtained with persComDbOpen
 * \param key               [in] key's name (length limited to \ref PERS_DB_MAX_LENGTH_KEY_NAME)
 *
 * \return key's size, or negative value in case of error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComDbGetKeySize(signed int handlerDB, char const * key) ;

/**
 * \brief delete key from local/shared database
 *
 * \param handlerDB     [in] handler obtained with persComDbOpen
 * \param key           [in] key's name (length limited to \ref PERS_DB_MAX_LENGTH_KEY_NAME)
 *
 * \return 0 for success, negative value otherwise (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComDbDeleteKey(signed int handlerDB, char const * key) ;


/**
 * \brief Find the buffer's size needed to accomodate the list of keys' names in local/shared database
 *
 * \param handlerDB     [in] handler obtained with persComDbOpen
 *
 * \return needed size, or negative value in case of error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComDbGetSizeKeysList(signed int handlerDB) ;


/**
 * \brief Obtain the list of the keys' names in local/shared database
 * \note : keys in the list are separated by '\0'
 *
 * \param handlerDB         [in] handler obtained with persComDbOpen
 * \param listBuffer_out    [out]buffer where to return the list of keys
 * \param listBufferSize    [in] size of listingBuffer_out
 * \return >=0 for size of the list, or negative value in case of error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComDbGetKeysList(signed int handlerDB, char* listBuffer_out, signed int listBufferSize) ;

/** \} */ /* End of PERS_DB_ACCESS_FUNCTIONS */


#ifdef __cplusplus
}
#endif /* extern "C" { */
/** \} */ /* End of PERS_COM_DB_ACCESS */
#endif /* OSS_PERSISTENCE_COMMON_DB_ACCESS_H */




