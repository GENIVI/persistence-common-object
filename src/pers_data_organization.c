/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*
* Author: Ionut.Ieremie@continental-corporation.com
*
* Implementation of persComDataOrg.h
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date             Author              Reason
* 2012.12.10       uild9757            CSP_WZ#2798: Added gLocalFactoryDefault and gLocalConfigurableDefault
* 2012.12.10       uild9757            CSP_WZ#388:  Initial creation
*
**********************************************************************************************************************/

#include "persComDataOrg.h"



/* resource configuration table name */
const char* gResTableCfg = PERS_ORG_RCT_NAME_ ;

/** local factory-default database */
const char* gLocalFactoryDefault = PERS_ORG_LOCAL_FACTORY_DEFAULT_DB_NAME_ ;

/** local configurable-default database */
const char* gLocalConfigurableDefault = PERS_ORG_LOCAL_CONFIGURABLE_DEFAULT_DB_NAME_ ;

/* shared cached default database */
const char* gSharedCachedDefault = PERS_ORG_SHARED_CACHE_DEFAULT_DB_NAME_ ;

/* shared cached database */
const char* gSharedCached = PERS_ORG_SHARED_CACHE_DB_NAME_ ;

/* shared write through default database */
const char* gSharedWtDefault = PERS_ORG_SHARED_WT_DEFAULT_DB_NAME_ ;

/* shared write through database */
const char* gSharedWt = PERS_ORG_SHARED_WT_DB_NAME_ ;

/* local cached default database */
const char* gLocalCachedDefault = PERS_ORG_LOCAL_CACHE_DEFAULT_DB_NAME_ ;

/* local cached database */
const char* gLocalCached = PERS_ORG_LOCAL_CACHE_DB_NAME_ ;

/* local write through default database */
const char* gLocalWtDefault = PERS_ORG_LOCAL_WT_DEFAULT_DB_NAME_ ;

/* local write through default database */
const char* gLocalWt = PERS_ORG_LOCAL_WT_DB_NAME_ ;

/* directory structure node name definition */
const char* gNode = PERS_ORG_NODE_FOLDER_NAME_ ;

/* directory structure user name definition */
const char* gUser = PERS_ORG_USER_FOLDER_NAME_ ;

/* directory structure seat name definition */
const char* gSeat = PERS_ORG_SEAT_FOLDER_NAME_ ;

/* directory structure shared name definition */
const char* gSharedPathName = PERS_ORG_SHARED_FOLDER_NAME ;

/* path prefix for all data */
const char* gRootPath = PERS_ORG_ROOT_PATH ;

/* path prefix for local cached database: /Data/mnt-c/<appId>/<database_name> */
const char* gLocalCachePath = PERS_ORG_LOCAL_CACHE_PATH_FORMAT ;

/* path prefix for local write through database /Data/mnt-wt/<appId>/<database_name> */
const char* gLocalWtPath = PERS_ORG_LOCAL_WT_PATH_FORMAT ;

/* path prefix for shared cached database: /Data/mnt-c/shared/group/  */
const char* gSharedCachePathRoot = PERS_ORG_SHARED_GROUP_CACHE_PATH_ ;

/* path format for shared cached database: /Data/mnt-c/shared/group/<group_no>/<database_name> */
const char* gSharedCachePath = PERS_ORG_SHARED_CACHE_PATH_FORMAT ;

/* path prefix for shared cached database: /Data/mnt-c/shared/group/<group_no>/<database_name>  */
const char* gSharedCachePathString = PERS_ORG_SHARED_CACHE_PATH_STRING_FORMAT ;

/* path prefix for shared write through database: /Data/mnt-wt/shared/group/   */
const char* gSharedWtPathRoot = PERS_ORG_SHARED_GROUP_WT_PATH_ ;

/* path prefix for shared write through database: /Data/mnt_wt/shared/group/<group_no>/<database_name> */
const char* gSharedWtPath = PERS_ORG_SHARED_WT_PATH_FORMAT ;

/* path prefix for shared write through database: /Data/mnt-wt/shared/group/<group_no>/<database_name>  */
const char* gSharedWtPathString = PERS_ORG_SHARED_WT_PATH_STRING_FORMAT ;

/* path prefix for shared public cached database: /Data/mnt-c/shared/public/<database_name>   */
const char* gSharedPublicCachePath = PERS_ORG_SHARED_PUBLIC_CACHE_PATH_FORMAT ;

/* path prefix for shared public write through database: /Data/mnt-wt/shared/public/<database_name>   */
const char* gSharedPublicWtPath = PERS_ORG_SHARED_PUBLIC_WT_PATH_FORMAT ;


