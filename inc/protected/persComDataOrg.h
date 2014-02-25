#ifndef OSS_PERSISTENCE_COMMON_DATA_ORGANIZATION_H
#define OSS_PERSISTENCE_COMMON_DATA_ORGANIZATION_H

/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*
* Author: Ionut.Ieremie@continental-corporation.com
*
* Interface: protected - specifies the organization of Genivi's persistence data    
*
* The file defines contains the defines according to
* https://collab.genivi.org/wiki/display/genivi/SysInfraEGPersistenceConceptInterface   
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date       Author             Reason
* 2013.03.21 uidl9757  3.1.0.0  CSP_WZ#2798:  Updates according to changes in data organization
* 2013.01.23 uidl9757  3.0.0.0  CSP_WZ#2060:  CoC_SSW:Persistence: common interface to be used by both PCL and PAS 
*
**********************************************************************************************************************/

/** \defgroup PERS_COM_DATA_ORG Data organization API
 *  \{
 */

#ifdef __cplusplus
extern "C"
{
#endif  /** #ifdef __cplusplus */

/** \defgroup PERS_DATA_ORG_IF_VERSION Interface version
 *  \{
 */
#define PERS_COM_DATA_ORG_INTERFACE_VERSION  (0x03010000U)
/** \} */ 

/** \defgroup PERS_ORG_DEFINES Max path length
 *  \{
 */
/** max path length when accessing a file (absolute path + filename)  */
#define PERS_ORG_MAX_LENGTH_PATH_FILENAME 255
/** \} */ 

/** \defgroup PERS_ORG_DATABASE_NAMES Databases' names
 *  \{
 */
 
/** resource configuration table name */
#define PERS_ORG_RCT_NAME "resource-table-cfg.itz"
#define PERS_ORG_RCT_NAME_ "/"PERS_ORG_RCT_NAME
extern const char* gResTableCfg; /**< PERS_ORG_RCT_NAME_ */

/** local factory-default database */
#define PERS_ORG_LOCAL_FACTORY_DEFAULT_DB_NAME "default-data.itz"
#define PERS_ORG_LOCAL_FACTORY_DEFAULT_DB_NAME_ "/"PERS_ORG_LOCAL_FACTORY_DEFAULT_DB_NAME
extern const char* gLocalFactoryDefault; /**< PERS_ORG_LOCAL_FACTORY_DEFAULT_DB_NAME_ */

/** local configurable-default database */
#define PERS_ORG_LOCAL_CONFIGURABLE_DEFAULT_DB_NAME "configurable-default-data.itz"
#define PERS_ORG_LOCAL_CONFIGURABLE_DEFAULT_DB_NAME_ "/"PERS_ORG_LOCAL_CONFIGURABLE_DEFAULT_DB_NAME
extern const char* gLocalConfigurableDefault; /**< PERS_ORG_LOCAL_CONFIGURABLE_DEFAULT_DB_NAME_ */


/** shared cached default database */
#define PERS_ORG_SHARED_CACHE_DEFAULT_DB_NAME "cached-default.itz"
#define PERS_ORG_SHARED_CACHE_DEFAULT_DB_NAME_ "/"PERS_ORG_SHARED_CACHE_DEFAULT_DB_NAME
extern const char* gSharedCachedDefault; /**< PERS_ORG_SHARED_CACHE_DEFAULT_DB_NAME_ */


/** shared cached database */
#define PERS_ORG_SHARED_CACHE_DB_NAME "cached.itz"
#define PERS_ORG_SHARED_CACHE_DB_NAME_ "/"PERS_ORG_SHARED_CACHE_DB_NAME
extern const char* gSharedCached; /**< PERS_ORG_SHARED_CACHE_DB_NAME_ */


/** shared write through default database */
#define PERS_ORG_SHARED_WT_DEFAULT_DB_NAME "wt-default.itz"
#define PERS_ORG_SHARED_WT_DEFAULT_DB_NAME_ "/"PERS_ORG_SHARED_WT_DEFAULT_DB_NAME
extern const char* gSharedWtDefault; /**< PERS_ORG_SHARED_WT_DEFAULT_DB_NAME_ */


/** shared write through database */
#define PERS_ORG_SHARED_WT_DB_NAME "wt.itz"
#define PERS_ORG_SHARED_WT_DB_NAME_ "/"PERS_ORG_SHARED_WT_DB_NAME
extern const char* gSharedWt; /**< PERS_ORG_SHARED_WT_DB_NAME_ */


/** local cached default database */
#define PERS_ORG_LOCAL_CACHE_DEFAULT_DB_NAME "cached-default.itz"
#define PERS_ORG_LOCAL_CACHE_DEFAULT_DB_NAME_ "/" PERS_ORG_LOCAL_CACHE_DEFAULT_DB_NAME
extern const char* gLocalCachedDefault; /**< PERS_ORG_LOCAL_CACHE_DEFAULT_DB_NAME_ */


/** local cached database */
#define PERS_ORG_LOCAL_CACHE_DB_NAME "cached.itz"
#define PERS_ORG_LOCAL_CACHE_DB_NAME_ "/"PERS_ORG_LOCAL_CACHE_DB_NAME
extern const char* gLocalCached; /**< PERS_ORG_LOCAL_CACHE_DB_NAME_ */


/** local write through default database */
#define PERS_ORG_LOCAL_WT_DEFAULT_DB_NAME "wt-default.itz"
#define PERS_ORG_LOCAL_WT_DEFAULT_DB_NAME_ "/"PERS_ORG_LOCAL_WT_DEFAULT_DB_NAME
extern const char* gLocalWtDefault; /**< PERS_ORG_LOCAL_WT_DEFAULT_DB_NAME_ */

/** local write through default database */
#define PERS_ORG_LOCAL_WT_DB_NAME "wt.itz"
#define PERS_ORG_LOCAL_WT_DB_NAME_ "/" PERS_ORG_LOCAL_WT_DB_NAME
extern const char* gLocalWt; /**< PERS_ORG_LOCAL_WT_DB_NAME_ */

/** \} */ 


/** \defgroup PERS_ORG_FOLDER_NAMES Folders' names
 *  \{
 */

/** directory structure node name definition */
#define PERS_ORG_NODE_FOLDER_NAME "node"
#define PERS_ORG_NODE_FOLDER_NAME_ "/" PERS_ORG_NODE_FOLDER_NAME
extern const char* gNode; /**< PERS_ORG_NODE_FOLDER_NAME_ */


/** directory structure user name definition */
#define PERS_ORG_USER_FOLDER_NAME "user"
#define PERS_ORG_USER_FOLDER_NAME_ "/"PERS_ORG_USER_FOLDER_NAME "/"
extern const char* gUser; /**< PERS_ORG_USER_FOLDER_NAME_ */


/** directory structure seat name definition */
#define PERS_ORG_SEAT_FOLDER_NAME "seat"
#define PERS_ORG_SEAT_FOLDER_NAME_ "/"PERS_ORG_SEAT_FOLDER_NAME "/"
extern const char* gSeat; /**< PERS_ORG_SEAT_FOLDER_NAME_ */

/** directory structure shared name definition */
#define PERS_ORG_SHARED_FOLDER_NAME "shared"
#define PERS_ORG_SHARED_FOLDER_NAME_ "/"PERS_ORG_SHARED_FOLDER_NAME
extern const char* gSharedPathName; /**< PERS_ORG_SHARED_FOLDER_NAME */

/** directory structure group name definition */
#define PERS_ORG_GROUP_FOLDER_NAME "group"
#define PERS_ORG_GROUP_FOLDER_NAME_ "/"PERS_ORG_GROUP_FOLDER_NAME

/** directory structure public name definition */
#define PERS_ORG_PUBLIC_FOLDER_NAME "public"
#define PERS_ORG_PUBLIC_FOLDER_NAME_ "/"PERS_ORG_PUBLIC_FOLDER_NAME

/** directory structure defaultData name definition */
#define PERS_ORG_DEFAULT_DATA_FOLDER_NAME "defaultData"
#define PERS_ORG_DEFAULT_DATA_FOLDER_NAME_ "/"PERS_ORG_DEFAULT_DATA_FOLDER_NAME

/** directory structure configurableDefaultData name definition */
#define PERS_ORG_CONFIG_DEFAULT_DATA_FOLDER_NAME "configurableDefaultData"
#define PERS_ORG_CONFIG_DEFAULT_DATA_FOLDER_NAME_ "/"PERS_ORG_CONFIG_DEFAULT_DATA_FOLDER_NAME


/** directory structure cached name definition */
#define PERS_ORG_CACHE_FOLDER_NAME "mnt-c"
#define PERS_ORG_CACHE_FOLDER_NAME_ "/"PERS_ORG_CACHE_FOLDER_NAME
/** directory structure write-through name definition */
#define PERS_ORG_WT_FOLDER_NAME "mnt-wt"
#define PERS_ORG_WT_FOLDER_NAME_ "/"PERS_ORG_WT_FOLDER_NAME


/** path prefix for all data: /Data */
#define PERS_ORG_ROOT_PATH "/Data"
extern const char* gRootPath; /**< PERS_ORG_ROOT_PATH */

/** \} */ 


/** \defgroup PERS_ORG_PATHS Paths
 *  \{
 */

/** cache root path application: /Data/mnt-c */
#define PERS_ORG_LOCAL_APP_CACHE_PATH PERS_ORG_ROOT_PATH PERS_ORG_CACHE_FOLDER_NAME_
#define PERS_ORG_LOCAL_APP_CACHE_PATH_ PERS_ORG_LOCAL_APP_CACHE_PATH"/"
/** wt root path application: /Data/mnt-wt */
#define PERS_ORG_LOCAL_APP_WT_PATH PERS_ORG_ROOT_PATH PERS_ORG_WT_FOLDER_NAME_
#define PERS_ORG_LOCAL_APP_WT_PATH_ PERS_ORG_LOCAL_APP_WT_PATH"/"

/** cache root path shared: /Data/mnt-c/shared */
#define PERS_ORG_SHARED_CACHE_PATH PERS_ORG_ROOT_PATH PERS_ORG_CACHE_FOLDER_NAME_ PERS_ORG_SHARED_FOLDER_NAME_
#define PERS_ORG_SHARED_CACHE_PATH_ PERS_ORG_SHARED_CACHE_PATH"/"
/** wt root path shared: /Data/mnt-wt/shared */
#define PERS_ORG_SHARED_WT_PATH PERS_ORG_ROOT_PATH PERS_ORG_WT_FOLDER_NAME_ PERS_ORG_SHARED_FOLDER_NAME_
#define PERS_ORG_SHARED_WT_PATH_ PERS_ORG_SHARED_WT_PATH"/"

/** cache root path shared group: /Data/mnt-c/shared/group */
#define PERS_ORG_SHARED_GROUP_CACHE_PATH PERS_ORG_SHARED_CACHE_PATH PERS_ORG_GROUP_FOLDER_NAME_
#define PERS_ORG_SHARED_GROUP_CACHE_PATH_ PERS_ORG_SHARED_GROUP_CACHE_PATH"/"
/** wt root path application: /Data/mnt-wt/shared/group */
#define PERS_ORG_SHARED_GROUP_WT_PATH PERS_ORG_SHARED_WT_PATH PERS_ORG_GROUP_FOLDER_NAME_
#define PERS_ORG_SHARED_GROUP_WT_PATH_ PERS_ORG_SHARED_GROUP_WT_PATH"/"

/** cache root path shared public: /Data/mnt-c/shared/public */
#define PERS_ORG_SHARED_PUBLIC_CACHE_PATH PERS_ORG_SHARED_CACHE_PATH PERS_ORG_PUBLIC_FOLDER_NAME_
#define PERS_ORG_SHARED_PUBLIC_CACHE_PATH_ PERS_ORG_SHARED_PUBLIC_CACHE_PATH"/"
/** wt root path application: /Data/mnt-wt/shared/public */
#define PERS_ORG_SHARED_PUBLIC_WT_PATH PERS_ORG_SHARED_WT_PATH PERS_ORG_PUBLIC_FOLDER_NAME_
#define PERS_ORG_SHARED_PUBLIC_WT_PATH_ PERS_ORG_SHARED_PUBLIC_WT_PATH"/"

/** path prefix for local cached database: /Data/mnt-c/\<appId\>/\<database_name\> */
#define PERS_ORG_LOCAL_CACHE_PATH_FORMAT PERS_ORG_LOCAL_APP_CACHE_PATH"/%s%s"
extern const char* gLocalCachePath; /**< PERS_ORG_LOCAL_CACHE_PATH_FORMAT */

/** path prefix for local write through database /Data/mnt-wt/\<appId\>/\<database_name\> */
#define PERS_ORG_LOCAL_WT_PATH_FORMAT PERS_ORG_LOCAL_APP_WT_PATH "/%s%s"
extern const char* gLocalWtPath; /**< PERS_ORG_LOCAL_WT_PATH_FORMAT */

/** path prefix for shared cached database: /Data/mnt-c/shared/group/  */
extern const char* gSharedCachePathRoot; /**< PERS_ORG_SHARED_GROUP_CACHE_PATH_ */

/** path format for shared cached database: /Data/mnt-c/shared/group/\<group_no\>/\<database_name\> */
#define PERS_ORG_SHARED_CACHE_PATH_FORMAT PERS_ORG_SHARED_GROUP_CACHE_PATH_"%x%s"
extern const char* gSharedCachePath; /**< PERS_ORG_SHARED_CACHE_PATH_FORMAT */

/** path prefix for shared cached database: /Data/mnt-c/shared/group/\<group_no\>/\<database_name\>  */
#define PERS_ORG_SHARED_CACHE_PATH_STRING_FORMAT PERS_ORG_SHARED_GROUP_CACHE_PATH_"%s%s"
extern const char* gSharedCachePathString; /**< PERS_ORG_SHARED_CACHE_PATH_STRING_FORMAT */

/** path prefix for shared write through database: /Data/mnt-wt/shared/group/   */
extern const char* gSharedWtPathRoot; /**< PERS_ORG_SHARED_GROUP_WT_PATH_ */

/** path prefix for shared write through database: /Data/mnt_wt/Shared/Group/\<group_no\>/\<database_name\> */
#define PERS_ORG_SHARED_WT_PATH_FORMAT PERS_ORG_SHARED_GROUP_WT_PATH_"%x%s"
extern const char* gSharedWtPath ; /**< PERS_ORG_SHARED_WT_PATH_FORMAT */

/** path prefix for shared write through database: /Data/mnt-wt/shared/group/\<group_no\>/\<database_name\>  */
#define PERS_ORG_SHARED_WT_PATH_STRING_FORMAT PERS_ORG_SHARED_GROUP_WT_PATH_"%s%s"
extern const char* gSharedWtPathString; /**< PERS_ORG_SHARED_WT_PATH_STRING_FORMAT */

/** path prefix for shared public cached database: /Data/mnt-c/shared/public/\<database_name\>   */
#define PERS_ORG_SHARED_PUBLIC_CACHE_PATH_FORMAT PERS_ORG_SHARED_PUBLIC_CACHE_PATH"%s"
extern const char* gSharedPublicCachePath; /**< PERS_ORG_SHARED_PUBLIC_CACHE_PATH_FORMAT */

/** path prefix for shared public write through database: /Data/mnt-wt/shared/public/\<database_name\>   */
#define PERS_ORG_SHARED_PUBLIC_WT_PATH_FORMAT PERS_ORG_SHARED_PUBLIC_WT_PATH"%s"
extern const char* gSharedPublicWtPath; /**< PERS_ORG_SHARED_PUBLIC_WT_PATH_FORMAT */

/** \} */ 



/** \defgroup PERS_ORG_LINKS_NAMES Links' names
 *  \{
 */

/** symlinks to shared group folder have the format "shared_group_XX", e.g. "shared_group_0A" */
#define PERS_ORG_SHARED_GROUP_SYMLINK_PREFIX "shared_group_"

/** symlinks to shared public folder */
#define PERS_ORG_SHARED_PUBLIC_SYMLINK_NAME "shared_public"

/** \} */ 

#ifdef __cplusplus
}
#endif /** extern "C" { */

/** \} */ /** End of PERS_COM_DATA_ORG */
#endif /** OSS_PERSISTENCE_COMMON_DATA_ORGANIZATION_H */
