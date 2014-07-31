#ifndef OSS_PERSISTENCE_RESOURCE_CONFIG_TABLE_H
#define OSS_PERSISTENCE_RESOURCE_CONFIG_TABLE_H

/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*
* Author: Ionut.Ieremie@continental-corporation.com
*
* Interface: protected - Access to resource configuration table   
*
* For additional details see
* https://collab.genivi.org/wiki/display/genivi/SysInfraEGPersistenceConceptInterface   
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date       Author             Reason
* 2013.04.02 uidl9757  5.0.0.0  CSP_WZ#3321:  Update of PersistenceConfigurationKey_s.permission 
* 2013.03.21 uidl9757  4.0.0.0  CSP_WZ#2798:  Update of PersistenceConfigurationKey_s 
* 2013.01.23 uidl9757  3.0.0.0  CSP_WZ#2060:  CoC_SSW:Persistence: common interface to be used by both PCL and PAS 
*
**********************************************************************************************************************/

/** 
 * \brief For details see https://collab.genivi.org/wiki/display/genivi/SysInfraEGPersistenceConceptInterface
 */


/** \defgroup PERS_COM_RCT Resource Config Table API
 *  \{
 */


#ifdef __cplusplus
extern "C"
{
#endif  /* #ifdef __cplusplus */

/** \defgroup PERS_RCT_IF_VERSION Interface version
 *  \{
 */
#define PERS_COM_RESOURCE_CONFIG_TABLE_INTERFACE_VERSION  (0x05000000U)
/** \} */ /* end of PERS_RCT_IF_VERSION */



/** \defgroup PERS_RCT_CONFIG Configuration parameters
 * see the defines below for their meaning
 *  \{
 */

#define PERS_RCT_MAX_LENGTH_RESOURCE_ID 64 /**< Max. length of the resource identifier */
#define PERS_RCT_MAX_LENGTH_RESPONSIBLE 64 /**< Max. length of the responsible application */
#define PERS_RCT_MAX_LENGTH_CUSTOM_NAME 64 /**< Max. length of the customer plugin */
#define PERS_RCT_MAX_LENGTH_CUSTOM_ID   64 /**< Max. length of the custom ID */

/** \} */ /* End of PERS_RCT_CONFIG */



/** \defgroup PERS_RCT_ENUM Enumerators managed in the RCT
 *  \{
 */
/** data policies */
typedef enum PersistencePolicy_e_
{
   PersistencePolicy_wc    = 0,  /**< the data is managed write cached */
   PersistencePolicy_wt,         /**< the data is managed write through */
   PersistencePolicy_na,         /**< the data is not applicable */

   /** insert new entries here ... */
   PersistencePolicy_LastEntry         /**< last entry */

} PersistencePolicy_e;


/** storages to manage the data */
typedef enum PersistenceStorage_e_
{
   PersistenceStorage_local    = 0,  /**< the data is managed local */
   PersistenceStorage_shared,        /**< the data is managed shared */
   PersistenceStorage_custom,        /**< the data is managed over custom client implementation */

   /** insert new entries here ... */
   PersistenceStorage_LastEntry      /**< last entry */

} PersistenceStorage_e;

/** specifies the type of the resource */
typedef enum PersistenceResourceType_e_
{
   PersistenceResourceType_key    = 0,  /**< key type resource */
   PersistenceResourceType_file,        /**< file type resourced */
   
   /** insert new entries here ... */
   PersistenceResourceType_LastEntry    /**< last entry */

} PersistenceResourceType_e;

/** specifies the permission on resource's data */
typedef enum PersistencePermission_e_
{
    PersistencePermission_ReadWrite = 0,    /**< random access to data is allowed */
    PersistencePermission_ReadOnly,         /**< only read access to data is allowed */
    PersistencePermission_WriteOnly,        /**< only write access to data is allowed */
    
    /** insert new entries here ... */
    PersistencePermission_LastEntry         /**< last entry */
} PersistencePermission_e;

/** \} */ /* End of PERS_RCT_ENUM */



/** \defgroup PERS_RCT_STRUCT Structures managed in the RCT
 *  \{
 */
/** resource configuration */
typedef struct PersistenceConfigurationKey_s_
{
   PersistencePolicy_e          policy;                                              /**< policy  */
   PersistenceStorage_e         storage;                                             /**< definition of storage to use */
   PersistenceResourceType_e    type;                                                /**< type of the resource */
   PersistencePermission_e      permission;                                          /**< access right */
   unsigned int                 max_size;                                            /**< max size expected for the key */
   char                         reponsible[PERS_RCT_MAX_LENGTH_RESPONSIBLE];         /**< name of responsible application */
   char                         custom_name[PERS_RCT_MAX_LENGTH_CUSTOM_NAME];        /**< name of the customer plugin */
   char                         customID[PERS_RCT_MAX_LENGTH_CUSTOM_ID];             /**< internal ID for the custom type resource */
} PersistenceConfigurationKey_s;
/** \} */ /* End of PERS_RCT_STRUCT */



/** \defgroup PERS_RCT_FUNCTIONS Functions
 *  \{
 */


/** 
 * \brief Obtain a handler to RCT indicated by rctPathname
 * \note : RCT is created if it does not exist and (bForceCreationIfNotPresent != 0)
 *
 * \param rctPathname                   [in] absolute path to RCT (length limited to \ref PERS_ORG_MAX_LENGTH_PATH_FILENAME)
 * \param bForceCreationIfNotPresent    [in] if !=0x0, the RCT is created if it does not exist
 *
 * \return >= 0 for valid handler, negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComRctOpen(char const * rctPathname, unsigned char bForceCreationIfNotPresent) ;

/**
 * \brief Close handler to RCT
 *
 * \param handlerRCT    [in] handler obtained with persComRctOpen
 *
 * \return 0 for success, negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComRctClose(signed int handlerRCT) ;

/**
 * \brief write a resourceID-value pair into RCT
 *
 * \param handlerRCT    [in] handler obtained with persComRctOpen
 * \param resourceID    [in] resource's identifier (length limited to \ref PERS_RCT_MAX_LENGTH_RESOURCE_ID)
 * \param psConfig      [in] configuration for resourceID
 *
 * \return 0 for success, negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComRctWrite(signed int handlerRCT, char const * resourceID, PersistenceConfigurationKey_s const * psConfig) ;


/**
 * \brief read a resourceID's configuration from RCT
 *
 * \param handlerRCT    [in] handler obtained with persComRctOpen
 * \param resourceID    [in] resource's identifier (length limited to \ref PERS_RCT_MAX_LENGTH_RESOURCE_ID)
 * \param psConfig_out  [out]where to return the configuration for resourceID
 *
 * \return read size [byte], or negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComRctRead(signed int handlerRCT, char const * resourceID, PersistenceConfigurationKey_s const * psConfig_out) ;


/**
 * \brief delete a resourceID's configuration from RCT
 *
 * \param handlerRCT    [in] handler obtained with persComRctOpen
 * \param resourceID    [in] resource's identifier (length limited to \ref PERS_RCT_MAX_LENGTH_RESOURCE_ID)
 *
 * \return 0 for success, or negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComRctDelete(signed int handlerRCT, char const * resourceID) ;


/**
 * \brief Find the buffer's size needed to accomodate the listing of resourceIDs in RCT
 *
 * \param handlerRCT    [in] handler obtained with persComRctOpen
 *
 * \return needed size [byte], or negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComRctGetSizeResourcesList(signed int handlerRCT) ;


/**
 * \brief Get the list of the resourceIDs in RCT
 * \note : resourceIDs in the list are separated by '\0'
 *
 * \param handlerRCT        [in] handler obtained with persComRctOpen
 * \param listBuffer_out    [out]buffer where to return the list of resourceIDs
 * \param listBufferSize    [in] size of listBuffer_out
 *
 * \return list size [byte], or negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComRctGetResourcesList(signed int handlerRCT, char* listBuffer_out, signed int listBufferSize) ;


/** \} */ /* End of PERS_RCT_FUNCTIONS */

#ifdef __cplusplus
}
#endif /* extern "C" { */

/** \} */ /* End of PERS_COM_RCT */
#endif /* OSS_PERSISTENCE_DATA_ORGANIZATION_H */
