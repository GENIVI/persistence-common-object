#ifndef OSS_PERSISTENCE_COMMON_IPC_DBUS_H
#define OSS_PERSISTENCE_COMMON_IPC_DBUS_H

/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*
* Author: Petrica.Manoila@continental-corporation.com
*
* Interface: private - specifies the DBus interface for PersCommonIPC
*
* The file defines contains the defines according to
* https://collab.genivi.org/wiki/display/genivi/SysInfraEGPersistenceConceptInterface
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date       Author    Version  Reason
* 2013.04.03 uidu0250  1.0.0.0  CSP_WZ#2739 :  Initial version of the interface
*
**********************************************************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif  /* #ifdef __cplusplus */

#include "persComTypes.h"

#define PERSIST_IPC_DBUS_INTERFACE_VERSION  (0x01000000U)


#define PERSISTENCE_ADMIN_BUS_TYPE						G_BUS_TYPE_SYSTEM
#define PERSISTENCE_ADMIN_BUS_NAME						"org.genivi.persistence.admin"
#define PERSISTENCE_ADMIN_OBJ_PATH						"/org/genivi/persistence/admin"
#define PERSISTENCE_ADMIN_IFACE							"org.genivi.persistence.admin"
#define PERSISTENCE_ADMIN_CONSUMER_OBJ_PATH				"/org/genivi/persistence/adminconsumer"

/* TO DO: remove when PCL implementation is adapted to PersCommonIPC */
#define	PERSISTENCE_ADMIN_CONSUMER_IFACE					"org.genivi.persistence.adminconsumer"
#define PERSISTENCE_ADMIN_CONSUMER_METHOD_PERS_ADMIN_REQ	"PersistenceAdminRequest"

/**
 * \brief Initialize PAS IPC DBus component
 *
 * \note : The function creates the DBus connection and tries to obtain the DBus name
 * 		   and exports the org.genivi.persistence.admin interface.
 *		   It runs the DBus main loop on a second thread.
 *
 * \param  pInitInfo	[in] pointer to a \ref PersAdminPASInitInfo_s structure containing
 *                           the supported callbacks
 *
 * \return 0 for success, negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
sint_t 		persIpcInitPAS_DBus_high(PersAdminPASInitInfo_s 		*pInitInfo);


/**
 * \brief Sends over DBus a request to the PCL client specified by clientId.
 *
 * \note : Each requestId should be unique.
 *
 * \param clientID    					[in] the client ID returned by the supplied pRegCB callback
 * \param requestID    					[in] a unique identifier generated for every request
 * \param request    					[in] the request to be sent (bitfield using a valid 
 *                                           combination of any of the following flags : 
 *                                           ::PERSISTENCE_MODE_LOCK, ::PERSISTENCE_MODE_SYNC and ::PERSISTENCE_MODE_UNLOCK)
 *
 * \return 0 for success, negative value for error (see \ref PERS_COM_ERROR_CODES_DEFINES)
 */
sint_t		persIpcSendRequestToPCL_DBus_high(	sint_t 			clientID,
												sint_t			requestID,
												uint_t			request);


/**
 * \brief Register PCL client to PAS over DBus
 *
 * \note : An additional thread is created for communication purposes.
 * 		   Initialize members of the supplied PersAdminPCLInitInfo_s structure before calling this function.
 * 
 * \param pInitInfo 					[in] pointer to a \ref PersAdminPCLInitInfo_s structure containing
 *                           				 the supported callbacks
 * \param flags    						[in] supported notification flags
 * \param timeout    					[in] maximum time needed to process any supported request
 *
 * \return 0 for success, negative value for error (see \ref PERS_COM_ERROR_CODES_DEFINES)
 */
sint_t 		persIpcRegisterToPAS_DBus_high(	PersAdminPCLInitInfo_s         *    pInitInfo,
											uint_t 								flags,
											uint_t 								timeout);


/**
 * \brief Un-Register PCL client application from PAS over DBus
 *
 * \note : The additional thread created for communication purposes is stopped.
 *
 * \param flags    						[in] supported notification flags
 *
 * \return 0 for success, negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
sint_t 		persIpcUnRegisterFromPAS_DBus_high(	uint_t 		flags);


/**
 * \brief Send 'request processed' confirmation to PAS over DBus
 * \note : Send confirmation to PAS that the request specified by requestId has been processed.
 * 		   The status parameter should reflect this request and could also return an error.
 *
 * \param requestID    					[in] the ID of the processed request
 * \param status    					[in] the status of the request processed by PCL
 *                                           - In case of success: bitfield using any of the following flags, depending on the request : ::PERSISTENCE_STATUS_LOCKED.
 *                                           - In case of error: the sum of ::PERSISTENCE_STATUS_ERROR and an error code \ref PERS_COM_IPC_DEFINES_ERROR is returned.
 *
 * \return 0 for success, negative value for error (see \ref PERS_COM_ERROR_CODES_DEFINES)
 */
sint_t 		persIpcSendConfirmationToPAS_DBus_high(	sint_t 	requestID,
													uint_t 	status);

#ifdef __cplusplus
}
#endif /* extern "C" { */

#endif /* OSS_PERSISTENCE_COMMON_IPC_DBUS_H */

