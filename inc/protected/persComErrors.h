#ifndef OSS_PERSISTENCE_COMMON_ERROR_CODES_ACCESS_H
#define OSS_PERSISTENCE_COMMON_ERROR_CODES_ACCESS_H

/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*
* Author: Ionut.Ieremie@continental-corporation.com
*
* Interface: protected - Error codes that can be returned by PersCommon's functions    
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date       Author             Reason
* 2013.10.24 uidu0250  3.0.1.0  CSP_WZ#6327:  CoC_SSW:Persistence: add IPC specific error code
* 2013.01.23 uidl9757  3.0.0.0  CSP_WZ#2060:  CoC_SSW:Persistence: common interface to be used by both PCL and PAS 
*
**********************************************************************************************************************/

/** \defgroup PERS_COM_ERRORS Error Codes API
 *  \{
 */

#ifdef __cplusplus
extern "C"
{
#endif  /* #ifdef __cplusplus */

/** \defgroup PERS_COM_ERROR_CODES_IF_VERSION Interface version
 *  \{
 */
#define PERS_COM_ERROR_CODES_INTERFACE_VERSION  (0x03000000U)
/** \} */ 



/** \defgroup PERS_COM_ERROR_CODES_DEFINES Error codes PERS_COM_ERR_
 *  \{
 */
/* Error code return by the SW Package, related to SW_PackageID. */
#define PERS_COM_PACKAGEID                                       0x014   //!< Software package identifier, use for return value base
#define PERS_COM_BASERETURN_CODE           (PERS_COM_PACKAGEID << 16)  //!< Basis of the return value containing SW PackageID

#define PERS_COM_SUCCESS                             0x00000000 //!< the function call succeded

#define PERS_COM_ERROR_CODE                  (-(PERS_COM_BASERETURN_CODE))      //!< basis of the error (negative values)
#define PERS_COM_ERR_INVALID_PARAM             (PERS_COM_ERROR_CODE - 1)        //!< An invalid param was passed
#define PERS_COM_ERR_BUFFER_TOO_SMALL          (PERS_COM_ERROR_CODE - 2)        //!< The data found is too large to fit in the provided buffer
#define PERS_COM_ERR_NOT_FOUND                 (PERS_COM_ERROR_CODE - 3)        //!< Tried to access an unexistent key, database, file
#define PERS_COM_ERR_SIZE_TOO_LARGE            (PERS_COM_ERROR_CODE - 4)        //!< Tried to write a too large data 
#define PERS_COM_ERR_OPERATION_NOT_SUPPORTED   (PERS_COM_ERROR_CODE - 5)        //!< Operation is not (yet) supported
#define PERS_COM_ERR_MALLOC                    (PERS_COM_ERROR_CODE - 6)        //!< Dynamic memory allocation failed
#define PERS_COM_ERR_ACCESS_DENIED             (PERS_COM_ERROR_CODE - 7)        //!< Insufficient rights to perform opperation
#define PERS_COM_ERR_OUT_OF_MEMORY             (PERS_COM_ERROR_CODE - 8)        //!< Not enough resources for an opperation

#define PERS_COM_ERR_READONLY                  (PERS_COM_ERROR_CODE - 9)        //!< Database was opened in readonly mode and cannot be written
#define PERS_COM_ERR_SEM_WAIT_TIMEOUT          (PERS_COM_ERROR_CODE - 10)       //!< sem_wait timeout

/* IPC specific error codes */
#define	PERS_COM_IPC_ERR_PCL_NOT_AVAILABLE	   (PERS_COM_ERROR_CODE - 255)		//!< PCL client not available (application was killed)
/* end of IPC specific error codes */

#define PERS_COM_FAILURE                       (PERS_COM_ERROR_CODE - 0xFFFF)   //!< Generic error code - for situations not covered by the defined error codes
/** \} */ 

#ifdef __cplusplus
}
#endif /* extern "C" { */
/** \} */ /* End of PERS_COM_ERRORS */
#endif /* OSS_PERSISTENCE_COMMON_ERROR_CODES_ACCESS_H */
