/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*
* Author: Petrica.Manoila@continental-corporation.com
*
* Implementation of persComIpc.h
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date             Author              Reason
* 2013.04.03       uidu0250            CSP_WZ#2739 :  Initial creation
*
**********************************************************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <dlt.h>
#include "persComErrors.h"
#include "persComTypes.h"
#include "persComIpc.h"
#include "pers_ipc_dbus_if.h"


/* ---------- local defines, macros, constants and type definitions ------------ */

typedef enum persIpcChannel_e_
{
	persIpcChannelDBus_high = 0,
	persIpcChannelDBus_low,
	persIpcChannelSysSyncObj,
	persIpcChannelLastEntry
}persIpcChannel_e;


#ifndef	PERS_COM_IPC_PROTOCOL
#define	PERS_COM_IPC_PROTOCOL 	persIpcChannelDBus_high
#endif	//PERS_COM_IPC_PROTOCOL


typedef sint_t 	(*persIpcInitPAS_handler_f)(PersAdminPASInitInfo_s *);
typedef sint_t	(*persIpcSendRequestToPCL_handler_f)(sint_t, sint_t, uint_t);
typedef sint_t 	(*persIpcRegisterToPAS_handler_f)(PersAdminPCLInitInfo_s *, uint_t, uint_t);
typedef	sint_t 	(*persIpcUnRegisterFromPAS_handler_f)(uint_t);
typedef	sint_t 	(*persIpcSendConfirmationToPAS_handler_f)(sint_t, uint_t);


/* ----------global variables. initialization of global contexts ------------ */

DLT_DECLARE_CONTEXT(persComIpcDLTCtx);

#define LT_HDR                          					"COMMON_IPC >>"

static bool_t	g_bDltCtxInitialized = false;			/* PersCommonIPC DLT context initialized */

static bool_t	g_bPersCommonIPCInitPAS = false;	/* PersCommonIPC module initialized for PAS */
static bool_t	g_bPersCommonIPCInitPCL = false;	/* PersCommonIPC module initialized for PCL */


static persIpcInitPAS_handler_f 	persIpcInitPAS_handler[persIpcChannelLastEntry] =
{
	&persIpcInitPAS_DBus_high,
	NIL,
	NIL
};

static persIpcSendRequestToPCL_handler_f persIpcSendRequestToPCL_handler[persIpcChannelLastEntry] =
{
	&persIpcSendRequestToPCL_DBus_high,
	NIL,
	NIL
};

static persIpcRegisterToPAS_handler_f persIpcRegisterToPAS_handler[persIpcChannelLastEntry] =
{
	&persIpcRegisterToPAS_DBus_high,
	NIL,
	NIL
};

static persIpcUnRegisterFromPAS_handler_f persIpcUnRegisterFromPAS_handler[persIpcChannelLastEntry] =
{
	&persIpcUnRegisterFromPAS_DBus_high,
	NIL,
	NIL
};

static persIpcSendConfirmationToPAS_handler_f persIpcSendConfirmationToPAS_handler[persIpcChannelLastEntry] =
{
	&persIpcSendConfirmationToPAS_DBus_high,
	NIL,
	NIL
};



/**
 * \brief Initialize PAS IPC component
 * \note  An additional thread is created for communication purposes.
 *        Initialize members of the supplied PersAdminPASInitInfo_s structure before calling this function.
 *
 * \param pInitInfo                     [in] pointer to a \ref PersAdminPASInitInfo_s structure containing
 *                                           the supported callbacks
 *
 * \return 0 for success, negative value for error (see \ref PERS_COM_ERROR_CODES_DEFINES)
 */
int persIpcInitPAS(	PersAdminPASInitInfo_s 		*pInitInfo)
{
	int retVal = PERS_COM_SUCCESS;

	if(false == g_bDltCtxInitialized)
	{
		/* Initialize the logging interface */
		DLT_REGISTER_CONTEXT(persComIpcDLTCtx, "PCOM", "PersistenceCommonIPC Context");
		g_bDltCtxInitialized = true;
	}

	if(true == g_bPersCommonIPCInitPAS)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_WARN,	DLT_STRING(LT_HDR), 
												DLT_STRING("PAS IPC protocol already initialized."));
		return PERS_COM_SUCCESS;
	}

	if(NIL == pInitInfo)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("Invalid parameter in persIpcInitPAS call."));
		return PERS_COM_ERR_INVALID_PARAM;
	}

	if(NIL == persIpcInitPAS_handler[PERS_COM_IPC_PROTOCOL])
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("Internal configuration error. No handler for persIpcInitPAS."));
		return PERS_COM_ERR_INVALID_PARAM;
	}

	retVal = persIpcInitPAS_handler[PERS_COM_IPC_PROTOCOL](pInitInfo);
	if(PERS_COM_SUCCESS == retVal)
	{
		g_bPersCommonIPCInitPAS = true;
	}

	return retVal;
}


/**
 * \brief Sends a request to the PCL client specified by clientID.
 * \note  Each requestID should be unique.
 *
 * \param clientID                      [in] the client ID returned by the supplied pRegCB callback
 * \param requestID                     [in] a unique identifier generated for every request
 * \param request                       [in] the request to be sent (bitfield using a valid 
 *                                           combination of any of the following flags : 
 *                                           ::PERSISTENCE_MODE_LOCK, ::PERSISTENCE_MODE_SYNC and ::PERSISTENCE_MODE_UNLOCK)
 *
 * \return 0 for success, negative value for error (see \ref PERS_COM_ERROR_CODES_DEFINES)
 */
int persIpcSendRequestToPCL(    int             clientID,
                                int             requestID,
                                unsigned int    request)
{
	if(false == g_bPersCommonIPCInitPAS)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("PAS IPC protocol not initialized."));
		return PERS_COM_FAILURE;
	}

	if(NIL == persIpcSendRequestToPCL_handler[PERS_COM_IPC_PROTOCOL])
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("Internal configuration error. No handler for persIpcSendRequestToPCL."));
		return PERS_COM_ERR_INVALID_PARAM;
	}

	return persIpcSendRequestToPCL_handler[PERS_COM_IPC_PROTOCOL](	clientID,
																	requestID,
																	request);
}


/**
 * \brief Register PCL client application to PAS
 * \note  Registers the PCL (Persistence Client Library) client application to PAS (Persistence Administration Service)
 *        in order to receive persistence mode change notifications (i.e. the memory access blocked/un-blocked).
 *        The initialization is performed based on the supplied pInitInfo parameter.
 *        Call \ref persIpcUnRegisterFromPAS to unregister from PAS when closing the application.
 *        Initialize members of the supplied PersAdminPCLInitInfo_s structure before calling this function.
 *
 * \param pInitInfo                     [in] pointer to a \ref PersAdminPCLInitInfo_s structure containing
 *                                           the supported callbacks
 * \param flags                         [in] bitfield using a valid combination of any of the following flags : ::PERSISTENCE_MODE_LOCK, ::PERSISTENCE_MODE_SYNC and ::PERSISTENCE_MODE_UNLOCK
 * \param timeout                       [in] maximum time needed to process any supported request (in milliseconds)
 *
 * \return 0 for success, negative value for error (see \ref PERS_COM_ERROR_CODES_DEFINES)
 */
int persIpcRegisterToPAS(PersAdminPCLInitInfo_s         *       pInitInfo,
                                unsigned    int                 flags,
                                unsigned    int                 timeout)
{
	int retVal = PERS_COM_SUCCESS;

	if(false == g_bDltCtxInitialized)
	{
		/* Initialize the logging interface */
		DLT_REGISTER_CONTEXT(persComIpcDLTCtx, "PCOM", "PersistenceCommonIPC Context");
		g_bDltCtxInitialized = true;
	}

	if(true == g_bPersCommonIPCInitPCL)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_WARN,	DLT_STRING(LT_HDR), 
												DLT_STRING("PCL IPC protocol already initialized."));
		return PERS_COM_SUCCESS;
	}

	if(NIL == pInitInfo)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("Invalid parameter in persIpcRegisterToPAS call."));
		return PERS_COM_ERR_INVALID_PARAM;
	}

	if(NIL == persIpcRegisterToPAS_handler[PERS_COM_IPC_PROTOCOL])
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("Internal configuration error. No handler for persIpcRegisterToPAS."));
		return PERS_COM_ERR_INVALID_PARAM;
	}

	retVal =  persIpcRegisterToPAS_handler[PERS_COM_IPC_PROTOCOL](	pInitInfo,
																	flags,
																	timeout);
	if(PERS_COM_SUCCESS == retVal)
	{
		g_bPersCommonIPCInitPCL = true;
	}

	return retVal;
}


/**
 * \brief Un-Register PCL client application from PAS
 * \note  Un-registers the PCL (Persistence Client Library) client application from  PAS (Persistence Administration Service) for
 *        the notifications specified trough flags.
 *        The PCL client application will no longer receive from PAS the notifications specified in flags.
 *
 * \param flags                         [in] bitfield using a valid combination of any of the following flags : ::PERSISTENCE_MODE_LOCK, ::PERSISTENCE_MODE_SYNC and ::PERSISTENCE_MODE_UNLOCK
 *
 * \return 0 for success, negative value for error (see \ref PERS_COM_ERROR_CODES_DEFINES)
 */
int persIpcUnRegisterFromPAS(   unsigned    int     flags )
{
	sint_t	retVal = PERS_COM_SUCCESS;

	if(NIL == persIpcUnRegisterFromPAS_handler[PERS_COM_IPC_PROTOCOL])
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("Internal configuration error. No handler for persIpcUnRegisterFromPAS."));
		return PERS_COM_ERR_INVALID_PARAM;
	}

	retVal = persIpcUnRegisterFromPAS_handler[PERS_COM_IPC_PROTOCOL](flags);
	if(PERS_COM_SUCCESS == retVal)
	{
		g_bPersCommonIPCInitPCL = false;
	}

	/* Initialize the logging interface */
	if(true == g_bDltCtxInitialized)
	{
		DLT_UNREGISTER_CONTEXT(persComIpcDLTCtx);
		g_bDltCtxInitialized = false;
	}

	return retVal;
}


/**
 * \brief Send 'request processed' confirmation to PAS.
 * \note  Sends confirmation to PAS that the request specified by requestID has been processed.
 *        The status parameter should reflect this request and could also return an error.
 *
 * \param requestID                     [in] the ID of the processed request
 * \param status                        [in] the status of the request processed by PCL
 *                                           - In case of success: bitfield using any of the following flags, depending on the request : ::PERSISTENCE_STATUS_LOCKED.
 *                                           - In case of error: the sum of ::PERSISTENCE_STATUS_ERROR and an error code \ref PERS_COM_IPC_DEFINES_ERROR is returned.
 *
 * \return 0 for success, negative value for error (see \ref PERS_COM_ERROR_CODES_DEFINES)
 */
int persIpcSendConfirmationToPAS(               int     requestID,
                                    unsigned    int     status)
{
	if(false == g_bPersCommonIPCInitPCL)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("PCL IPC protocol not initialized."));
		return PERS_COM_FAILURE;
	}

	if(NIL == persIpcSendConfirmationToPAS_handler[PERS_COM_IPC_PROTOCOL])
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("Internal configuration error. No handler for persIpcSendConfirmationToPAS."));
		return PERS_COM_ERR_INVALID_PARAM;
	}

	return persIpcSendConfirmationToPAS_handler[PERS_COM_IPC_PROTOCOL](requestID, status);
}
