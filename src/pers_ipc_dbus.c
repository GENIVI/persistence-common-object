/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*
* Author: Petrica.Manoila@continental-corporation.com
*
* Implementation of pers_ipc_dbus_if.h
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date             Author              Reason
* 2013.10.24	   uidu0250			   CSP_WZ#6327 :  Change error handling for missing PCL clients
* 2013.04.03       uidu0250            CSP_WZ#2739 :  Initial creation
*
**********************************************************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <gio/gio.h>
#include <dlt.h>

#include "persComErrors.h"
#include "persComTypes.h"
#include "persComIpc.h"
#include "PasClientNotificationGen.h"
#include "pers_ipc_dbus_if.h"

/* ---------- local defines, macros, constants and type definitions ------------ */

#define	 PERS_IPC_INIT_DBUS_INFO_ARRAY_SIZE				(uint32_t)24
#define	 DBUS_MAIN_LOOP_THREAD_FLAG_SET					1

typedef struct persIpcDBusClientInfo_s_
{
	pstr_t								busName;
	pstr_t								objName;
}persIpcDBusClientInfo_s;


/* ----------global variables. initialization of global contexts ------------ */

DLT_IMPORT_CONTEXT(persComIpcDLTCtx);

#define LT_HDR                          					"COMMON_IPC >>"


static PersAdminPASInitInfo_s					g_pPersIpcPASInfo;		/* PAS IPC initialization struct */
static PersAdminPCLInitInfo_s					g_pPersIpcPCLInfo;		/* PCL IPC initialization struct */

static persIpcDBusClientInfo_s					**g_persIpcDBusClientInfoArray		= NIL;
static uint32_t									g_persIpcDBusClientInfoArraySize	= 0;
static uint32_t									g_persIpcDBusClientInfoMaxArraySize = 0;
static pthread_mutex_t 							g_persIpcDBusClientInfoArrayMtx;	 	 /* mutex for the access to the DBus client info array */

static pthread_t           						g_hPASDBusThread            		= 0;		 /* PAS DBus main loop thread */
static int 										g_PASDBusMainLoopThreadFlag;
static pthread_cond_t 							g_PASDBusMainLoopThreadFlagCV;
static pthread_mutex_t 							g_PASDBusMainLoopThreadFlagMtx;
static pthread_t           						g_hPCLDBusThread            		= 0;		 /* PCL DBus main loop thread */
static int 										g_PCLDBusMainLoopThreadFlag;
static pthread_cond_t 							g_PCLDBusMainLoopThreadFlagCV;
static pthread_mutex_t 							g_PCLDBusMainLoopThreadFlagMtx;


static GMainLoop          						*g_pPASMainLoop          	= NIL;			/* PAS DBus main loop */
static GDBusConnection    						*g_pPASDBusConnection     	= NIL;			/* PAS DBus connection */
static GMainLoop          						*g_pPCLMainLoop          	= NIL;			/* PCL DBus main loop */
static GDBusConnection    						*g_pPCLDBusConnection     	= NIL;			/* PCL DBus connection */
static OipPersistenceAdminSkeleton				*g_persIpcDBusPASSkeleton	= NIL;
static OipPersistenceAdminProxy					*g_persIpcDBusPASProxy		= NIL;
static OipPersistenceAdminconsumerSkeleton		*g_persIpcDBusPCLSkeleton	= NIL;
static volatile bool_t							g_bPASDBusConnInit			= false;
static volatile bool_t							g_bPCLDBusConnInit			= false;


/* ---------------------- local functions ---------------------------------- */

static void 	OnBusAcquired_cb(	GDBusConnection *connection, const gchar     *name, gpointer user_data);
static void 	OnNameAcquired_cb(	GDBusConnection *connection, const gchar     *name, gpointer user_data);
static void 	OnNameLost_cb(	GDBusConnection *connection, const gchar     *name, gpointer     user_data);

/* RegisterPersAdminNotification */
static gboolean OnHandleRegisterPersAdminNotification (	OipPersistenceAdmin 	*object,
														GDBusMethodInvocation 	*invocation,
														const gchar 			*arg_BusName,
														const gchar 			*arg_ObjName,
														gint 					arg_NotificationFlag,
														guint 					arg_TimeoutMs);

/* UnregisterPersAdminNotification */
static gboolean OnHandleUnregisterPersAdminNotification(OipPersistenceAdmin 	*object,
														GDBusMethodInvocation 	*invocation,
														const gchar 			*arg_BusName,
														const gchar 			*arg_ObjName,
														gint 					arg_NotificationFlag,
														guint 					arg_TimeoutMs);

/* PersistenceAdminRequestCompleted */
static gboolean OnHandlePersAdminRequestCompleted(	OipPersistenceAdmin 	*object,
													GDBusMethodInvocation 	*invocation,
													guint 					arg_RequestId,
													gint 					arg_StatusFlag);

/* PersAdminRequest */
static gboolean  OnHandlePersAdminRequest(	OipPersistenceAdminconsumer *object,
											GDBusMethodInvocation 		*invocation,
											guint 						arg_RequestId,
											gint 						arg_StatusFlag );

/* PAS DBus loop thread */
static void*		persIpcPASLoopThread(void *lpParam);

/* PCL DBus loop thread */
static void* 		persIpcPCLLoopThread(void *lpParam);

/* PCL DBus initialization function */
static sint_t 		persIpcInitPCL_DBus_high();

/* Get DBus client ID */
static sint_t		persIpcGetIdForDBusInfo(const 		pstr_t	busName,
											const 		pstr_t	objName,
											uint32_t			*clientID);

/* Export org.genivi.persistence.admin interface */
static bool_t		ExportPersistenceAdminIF(GDBusConnection    	*connection);

/* Export org.genivi.persistence.adminconsumer interface */
static bool_t		ExportPersistenceAdminConsumerIF(GDBusConnection    	*connection);



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
sint_t 		persIpcInitPAS_DBus_high(PersAdminPASInitInfo_s 		*pInitInfo)
{
	sint_t			retVal 				= PERS_COM_SUCCESS;

	if(NIL == pInitInfo)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("Invalid parameter in persIpcInitPAS_DBus_high call."));
		return PERS_COM_ERR_INVALID_PARAM;
	}

	if((NIL == pInitInfo->pRegCB) 		||
	   (NIL == pInitInfo->pUnRegCB)		||
	   (NIL == pInitInfo->pReqCompleteCB))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("Invalid parameter in persIpcInitPAS_DBus_high call."));
		return PERS_COM_ERR_INVALID_PARAM;
	}

	g_pPersIpcPASInfo.pRegCB 			= pInitInfo->pRegCB;
	g_pPersIpcPASInfo.pUnRegCB			= pInitInfo->pUnRegCB;
	g_pPersIpcPASInfo.pReqCompleteCB	= pInitInfo->pReqCompleteCB;

	/* Init synchronization objects */
	if(0 != pthread_mutex_init (&g_persIpcDBusClientInfoArrayMtx, NIL))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to create mutex."));
		return PERS_COM_FAILURE;
	}
	if(0 != pthread_mutex_init (&g_PASDBusMainLoopThreadFlagMtx, NIL))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to create mutex."));
		(void)pthread_mutex_destroy(&g_persIpcDBusClientInfoArrayMtx);
		return PERS_COM_FAILURE;
	}
	if(0 != pthread_cond_init  (&g_PASDBusMainLoopThreadFlagCV, NIL))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to create thread cond."));
		(void)pthread_mutex_destroy(&g_PASDBusMainLoopThreadFlagMtx);
		(void)pthread_mutex_destroy(&g_persIpcDBusClientInfoArrayMtx);
		return PERS_COM_FAILURE;
	}
	g_PASDBusMainLoopThreadFlag = 0;

	/* Create DBus main loop thread */
	if(0 != pthread_create(&g_hPASDBusThread, NIL, persIpcPASLoopThread, NIL))
	{

		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to create thread."));
		(void)pthread_cond_destroy(&g_PASDBusMainLoopThreadFlagCV);
		(void)pthread_mutex_destroy(&g_PASDBusMainLoopThreadFlagMtx);
		(void)pthread_mutex_destroy(&g_persIpcDBusClientInfoArrayMtx);
		return PERS_COM_FAILURE;
	}

	/* Wait for DBus connection initialization */
	if(0 != pthread_mutex_lock (&g_PASDBusMainLoopThreadFlagMtx))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to lock mutex."));
		return PERS_COM_FAILURE;
	}
	while (!g_PASDBusMainLoopThreadFlag)
	{
		(void)pthread_cond_wait (&g_PASDBusMainLoopThreadFlagCV, &g_PASDBusMainLoopThreadFlagMtx);
	}
	(void)pthread_mutex_unlock (&g_PASDBusMainLoopThreadFlagMtx);

	if(false == g_bPASDBusConnInit)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("PAS DBus connection setup failed."));

		(void)pthread_cond_destroy(&g_PASDBusMainLoopThreadFlagCV);
		(void)pthread_mutex_destroy(&g_PASDBusMainLoopThreadFlagMtx);
		(void)pthread_mutex_destroy(&g_persIpcDBusClientInfoArrayMtx);

		return PERS_COM_FAILURE; /* PERS_COM_IPC_DBUS_ERROR */
	}

	return retVal;
}


/**
 * \brief Sends over DBus a request to the PCL client specified by clientId.
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
												uint_t			request)
{
	sint_t								retVal 					= PERS_COM_SUCCESS;
/*	gboolean							gbRetVal				= false; */
	gint32								outErrorCode			= PERS_COM_SUCCESS;
	persIpcDBusClientInfo_s	 			dbusClientInfo;
/*	OipPersistenceAdminconsumerProxy	*pClientNotifProxy		= NIL;   */
	GError								*gError					= NIL;
	GVariant							*gVarReturnVal			= NIL;
	gint								clientTimeout			= 500;		/* default timeout in ms */

	if(false == g_bPASDBusConnInit)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("DBus connection not initialized."));
		return PERS_COM_FAILURE; /* PERS_COM_IPC_ERR_NO_CONNECTION */
	}

	(void)memset(&dbusClientInfo, 0, sizeof(dbusClientInfo));


	/* Get client DBus info */

	/* Acquire mutex on array of DBus client info */
	if(0 != pthread_mutex_lock (&g_persIpcDBusClientInfoArrayMtx))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to lock mutex."));
		return PERS_COM_FAILURE;
	}

	if(clientID >= g_persIpcDBusClientInfoArraySize)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR, 	DLT_STRING(LT_HDR),
													DLT_STRING("Invalid client specified :"),
													DLT_INT(clientID));
		(void)pthread_mutex_unlock (&g_persIpcDBusClientInfoArrayMtx);
		return PERS_COM_ERR_INVALID_PARAM;
	}

	if(NIL != g_persIpcDBusClientInfoArray[clientID])
	{
		dbusClientInfo.busName = (pstr_t)malloc((strlen(g_persIpcDBusClientInfoArray[clientID]->busName) + 1) * sizeof(*(dbusClientInfo.busName)));
		if(NIL == dbusClientInfo.busName)
		{
			DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 	
													DLT_STRING("Error allocating memory for client info data."));
			(void)pthread_mutex_unlock (&g_persIpcDBusClientInfoArrayMtx);
			return PERS_COM_ERR_MALLOC;
		}
		else
		{
			(void)memset(dbusClientInfo.busName, 0, (strlen(g_persIpcDBusClientInfoArray[clientID]->busName) + 1) * sizeof(*(dbusClientInfo.busName)));
			(void)memcpy(dbusClientInfo.busName, g_persIpcDBusClientInfoArray[clientID]->busName, strlen(g_persIpcDBusClientInfoArray[clientID]->busName) * sizeof(*(dbusClientInfo.busName)));
		}

		dbusClientInfo.objName = (pstr_t)malloc((strlen(g_persIpcDBusClientInfoArray[clientID]->objName) + 1) * sizeof(*(dbusClientInfo.objName)));
		if(NIL == dbusClientInfo.objName)
		{
			DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 	
													DLT_STRING("Error allocating memory for client info data."));
			(void)free(dbusClientInfo.busName);
			(void)pthread_mutex_unlock (&g_persIpcDBusClientInfoArrayMtx);
			return PERS_COM_ERR_MALLOC;
		}
		else
		{
			(void)memset(dbusClientInfo.objName, 0, (strlen(g_persIpcDBusClientInfoArray[clientID]->objName) + 1) * sizeof(*(dbusClientInfo.objName)));
			(void)memcpy(dbusClientInfo.objName, g_persIpcDBusClientInfoArray[clientID]->objName, strlen(g_persIpcDBusClientInfoArray[clientID]->objName) * sizeof(*(dbusClientInfo.objName)));
		}
	}

	/* Release array mutex */
	(void)pthread_mutex_unlock (&g_persIpcDBusClientInfoArrayMtx);


	/* Synchronous call to "PersistenceAdminRequest" */

	/* Does not work using generated code because PCL does not contain an implementation of 'GetAll' : */
	/*
	 * checkPersAdminMsg 'org.freedesktop.DBus.Properties' -> 'GetAll'
	 * handleObjectPathMessageFallback Object: ':1.11' -> Interface: 'org.freedesktop.DBus.Properties' -> Message: 'GetAll'
	 * handleObjectPathMessageFallback -> not a signal 'GetAll'
	 */

	/* TO DO : un-comment when PCL implementation is adapted to PersCommonIPC */

	/* Get the proxy object */
//	pClientNotifProxy = NIL;
//	pClientNotifProxy = (OipPersistenceAdminconsumerProxy *)oip_persistence_adminconsumer_proxy_new_sync(	g_pPASDBusConnection,
//																											G_DBUS_PROXY_FLAGS_NONE,
//																											dbusClientInfo.busName,
//																											dbusClientInfo.objName,
//																											NIL,
//																											NIL);
//	if(NIL == pClientNotifProxy)
//	{
//		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
//												DLT_STRING("DBus PersAdminConsumer proxy creation failed."));
//		(void)free(dbusClientInfo.busName);
//		(void)free(dbusClientInfo.objName);
//		return PERS_COM_FAILURE; /* PERS_COM_IPC_DBUS_ERROR */
//	}
//
//	(void)free(dbusClientInfo.busName);
//	(void)free(dbusClientInfo.objName);
//
//	/* Synchronous call to "PersistenceAdminRequest" */
//	gbRetVal = oip_persistence_adminconsumer_call_persistence_admin_request_sync(	(OipPersistenceAdminconsumer *)pClientNotifProxy,
//																					(gint)request,
//																					(guint)requestID,
//																					&outErrorCode,
//																					NIL,
//																					&gError);
//
//	/* Release the created proxy object */
//	g_object_unref(pClientNotifProxy);
//	pClientNotifProxy = NIL;
//
//	if(false == gbRetVal)
//	{
//		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
//												DLT_STRING("DBus PersistenceAdminRequest call failed with DBus error :"),
//												DLT_STRING(gError->message));
//		g_error_free(gError);
//		return PERS_COM_FAILURE; /* PERS_COM_IPC_DBUS_ERROR */
//	}

	gVarReturnVal = g_dbus_connection_call_sync (	g_pPASDBusConnection,
													dbusClientInfo.busName,
													dbusClientInfo.objName,
													PERSISTENCE_ADMIN_CONSUMER_IFACE,
													PERSISTENCE_ADMIN_CONSUMER_METHOD_PERS_ADMIN_REQ,
													g_variant_new ("(ii)", (sint32_t)request, (sint32_t)requestID),
													G_VARIANT_TYPE ("(i)"),
													G_DBUS_CALL_FLAGS_NONE,
													clientTimeout,
													NIL,
													&gError);
	if(NIL == gVarReturnVal)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("DBus PersistenceAdminRequest call failed with DBus error :"),
												DLT_STRING(gError->message));
		g_error_free(gError);
		(void)free(dbusClientInfo.busName);
		(void)free(dbusClientInfo.objName);

		/* consider that PCL client is not available on DBus */
		return PERS_COM_IPC_ERR_PCL_NOT_AVAILABLE;
	}

	outErrorCode = g_variant_get_int32(gVarReturnVal);

	g_variant_unref(gVarReturnVal);


	DLT_LOG(persComIpcDLTCtx, DLT_LOG_DEBUG, 	DLT_STRING(LT_HDR),
												DLT_STRING("Notified client identified by DBus Name : \""),
												DLT_STRING(dbusClientInfo.busName),
												DLT_STRING("\" and Object Path : \""),
												DLT_STRING(dbusClientInfo.objName),
												DLT_STRING("\" Flags="),
												DLT_INT(request),
												DLT_STRING(" RequestId="),
												DLT_INT(requestID),
												DLT_STRING("\". Client returned "),
												DLT_INT(outErrorCode));

	DLT_LOG(persComIpcDLTCtx, DLT_LOG_DEBUG,DLT_STRING(LT_HDR),
											DLT_STRING("DBus PersistenceAdminRequest call returned output error code :"),
											DLT_INT(outErrorCode));

	(void)free(dbusClientInfo.busName);
	(void)free(dbusClientInfo.objName);

	retVal = outErrorCode;

	return retVal;
}


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
											uint_t 								timeout)
{
	sint_t			retVal 			= PERS_COM_SUCCESS;
	gboolean		gbRetVal		= false;
	const gchar	   *gUniqueName		= NIL;
	gint			outErrorCode;
	GError		   *gError			= NIL;


	if(NIL == pInitInfo)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("Invalid parameter in persIpcRegisterToPAS_DBus_high call."));
		return PERS_COM_ERR_INVALID_PARAM;
	}

	if(NIL == pInitInfo->pReqCB)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("Invalid parameter in persIpcRegisterToPAS_DBus_high call."));
		return PERS_COM_ERR_INVALID_PARAM;
	}

	g_pPersIpcPCLInfo.pReqCB			= pInitInfo->pReqCB;

	/* Init DBus connection */
	if(false == g_bPCLDBusConnInit)
	{
		retVal = persIpcInitPCL_DBus_high();
		if(PERS_COM_SUCCESS != retVal)
		{
			DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 	
													DLT_STRING("persIpcRegisterToPAS_DBus_high call failed with error code :"),
													DLT_INT(retVal));
			return retVal;
		}
	}

	if(NIL == g_persIpcDBusPASProxy)
	{
		/* Get the PAS proxy object */
		g_persIpcDBusPASProxy = (OipPersistenceAdminProxy *)oip_persistence_admin_proxy_new_sync(	g_pPCLDBusConnection,
																									G_DBUS_PROXY_FLAGS_NONE,
																									PERSISTENCE_ADMIN_BUS_NAME,
																									PERSISTENCE_ADMIN_OBJ_PATH,
																									NIL,
																									NIL);
		if(NIL == g_persIpcDBusPASProxy)
		{
			DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 	
													DLT_STRING("DBus PersAdmin proxy creation failed."));
			return PERS_COM_FAILURE; /* PERS_COM_IPC_DBUS_ERROR */
		}
	}

	gUniqueName = g_dbus_connection_get_unique_name(g_pPCLDBusConnection);
	if(NIL == gUniqueName)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 	
												DLT_STRING("Failed to obtain the unique DBus name."));
		return PERS_COM_FAILURE; /* PERS_COM_IPC_ERR_NO_CONNECTION */
	}
	else
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_DEBUG,DLT_STRING(LT_HDR), 	
												DLT_STRING("Successfully obtained unique BusName :"),
												DLT_STRING(gUniqueName));
	}

	/* Synchronous call to "RegisterPersAdminNotification" */
	gbRetVal = oip_persistence_admin_call_register_pers_admin_notification_sync((OipPersistenceAdmin *)g_persIpcDBusPASProxy,
																				gUniqueName,
																				PERSISTENCE_ADMIN_CONSUMER_OBJ_PATH,
																				(gint)flags,
																				(guint)timeout,
																				&outErrorCode,
																				NIL,
																				&gError);
	if(false == gbRetVal)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 	
												DLT_STRING("DBus RegisterPersAdminNotification call failed with DBus error :"),
												DLT_STRING(gError->message));
		g_error_free(gError);
		return PERS_COM_FAILURE; /* PERS_COM_IPC_DBUS_ERROR */
	}

	DLT_LOG(persComIpcDLTCtx, DLT_LOG_DEBUG,DLT_STRING(LT_HDR),
											DLT_STRING("DBus RegisterPersAdminNotification call returned output error code :"),
											DLT_INT(outErrorCode));

	retVal = outErrorCode;

	return retVal;
}


/**
 * \brief Initialize PCL IPC DBus component
 *
 * \note : An additional thread is created for communication purposes.
 *
 * \return 0 for success, negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
static sint_t persIpcInitPCL_DBus_high()
{

	sint_t			retVal 				= PERS_COM_SUCCESS;
	bool_t			bRetVal;

	if(0 != pthread_mutex_init (&g_PCLDBusMainLoopThreadFlagMtx, NIL))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to create mutex."));
		return PERS_COM_FAILURE;
	}

	if(0 != pthread_cond_init  (&g_PCLDBusMainLoopThreadFlagCV, NIL))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to create thread cond."));
		(void)pthread_mutex_destroy(&g_PCLDBusMainLoopThreadFlagMtx);
		return PERS_COM_FAILURE;
	}

	g_PCLDBusMainLoopThreadFlag = 0;

	if(0 != pthread_create(&g_hPCLDBusThread, NIL, persIpcPCLLoopThread, NIL))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to create thread."));
		(void)pthread_cond_destroy(&g_PCLDBusMainLoopThreadFlagCV);
		(void)pthread_mutex_destroy(&g_PCLDBusMainLoopThreadFlagMtx);
		return PERS_COM_FAILURE;
	}

	/* Wait for DBus connection */
	if(0 != pthread_mutex_lock (&g_PCLDBusMainLoopThreadFlagMtx))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to lock mutex."));
		return PERS_COM_FAILURE;
	}
	while (!g_PCLDBusMainLoopThreadFlag)
	{
		pthread_cond_wait (&g_PCLDBusMainLoopThreadFlagCV, &g_PCLDBusMainLoopThreadFlagMtx);
	}
	(void)pthread_mutex_unlock (&g_PCLDBusMainLoopThreadFlagMtx);

	/* Check DBus connection status */
	if(false == g_bPCLDBusConnInit)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("PCL DBus connection setup failed."));
		(void)pthread_cond_destroy(&g_PCLDBusMainLoopThreadFlagCV);
		(void)pthread_mutex_destroy(&g_PCLDBusMainLoopThreadFlagMtx);
		return PERS_COM_FAILURE; /* PERS_COM_IPC_DBUS_ERROR */
	}

	/* Export the org.genivi.persistence.adminconsumer interface over DBus */
	bRetVal = ExportPersistenceAdminConsumerIF(g_pPCLDBusConnection);
	if(false == bRetVal)
	{
		/* Error: the interface could not be exported. */
		g_main_loop_quit(g_pPCLMainLoop);
	}
	else
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR),
												DLT_STRING("Successfully connected to D-Bus and exported object."));
	}

	return retVal;
}


/**
 * \brief Un-Register PCL client application from PAS over DBus
 * \note : The additional thread created for communication purposes is stopped.
 *
 * \param flags    						[in] supported notification flags
 *
 * \return 0 for success, negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
sint_t 		persIpcUnRegisterFromPAS_DBus_high(	uint_t 		flags)
{
	sint_t			retVal 		= PERS_COM_SUCCESS;
	gboolean		gbRetVal	= false;
	const gchar	   *gUniqueName	= NIL;
	gint			outErrorCode;
	GError		   *gError		= NIL;

	if(false == g_bPCLDBusConnInit)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("DBus connection not initialized."));
		return PERS_COM_FAILURE; /* PERS_COM_IPC_ERR_NO_CONNECTION */
	}

	if(NIL == g_persIpcDBusPASProxy)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("DBus PersAdmin proxy not available."));
		return PERS_COM_FAILURE; /* PERS_COM_IPC_DBUS_ERROR */
	}

	gUniqueName = g_dbus_connection_get_unique_name(g_pPCLDBusConnection);
	if(NIL == gUniqueName)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),	
												DLT_STRING("Failed to obtain the unique DBus name."));
		return PERS_COM_FAILURE; /* PERS_COM_IPC_ERR_NO_CONNECTION */
	}

	/* Synchronous call to "UnRegisterPersAdminNotification" */
	gbRetVal = oip_persistence_admin_call_un_register_pers_admin_notification_sync(	(OipPersistenceAdmin *)g_persIpcDBusPASProxy,
																					gUniqueName,
																					PERSISTENCE_ADMIN_CONSUMER_OBJ_PATH,
																					(gint)flags,
																					(guint)0,		// deprecated
																					&outErrorCode,
																					NIL,
																					&gError);
	if(false == gbRetVal)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 	
												DLT_STRING("DBus UnRegisterPersAdminNotification call failed with DBus error :"),
												DLT_STRING(gError->message));
		g_error_free(gError);
		return PERS_COM_FAILURE; /* PERS_COM_IPC_DBUS_ERROR */
	}

	DLT_LOG(persComIpcDLTCtx, DLT_LOG_DEBUG,DLT_STRING(LT_HDR),
											DLT_STRING("DBus UnRegisterPersAdminNotification call returned output error code :"),
											DLT_INT(outErrorCode));

	retVal = outErrorCode;

	/* Quit DBus main loop */
	g_main_loop_quit(g_pPCLMainLoop);

	/* Wait for persIpcPCLLoopThread */
	pthread_join(g_hPCLDBusThread, NIL);

	/* Release any created proxy object */
	if(NIL != g_persIpcDBusPASProxy)
	{
		g_object_unref(g_persIpcDBusPASProxy);
		g_persIpcDBusPASProxy = NIL;
	}

	return retVal;
}


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
													uint_t 	status)
{
	gboolean		gbRetVal		= false;
	gint			outErrorCode;
	GError		   *gError			= NIL;

	if(false == g_bPCLDBusConnInit)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("DBus connection not initialized."));
		return PERS_COM_FAILURE; /* PERS_COM_IPC_ERR_NO_CONNECTION */
	}

	if(NIL == g_persIpcDBusPASProxy)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("DBus PersAdmin proxy not available."));
		return PERS_COM_FAILURE; /* PERS_COM_IPC_DBUS_ERROR */
	}

	/* Synchronous call to "PersistenceAdminRequestCompleted" */
	gbRetVal = oip_persistence_admin_call_persistence_admin_request_completed_sync(	(OipPersistenceAdmin *)g_persIpcDBusPASProxy,
																					(guint)requestID,
																					(gint)status,
																					&outErrorCode,
																					NIL,
																					&gError);
	if(false == gbRetVal)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 	
												DLT_STRING("DBus PersistenceAdminRequestCompleted call failed with DBus error :"),
												DLT_STRING(gError->message));
		g_error_free(gError);
		return PERS_COM_FAILURE; /* PERS_COM_IPC_DBUS_ERROR */
	}

	DLT_LOG(persComIpcDLTCtx, DLT_LOG_DEBUG,DLT_STRING(LT_HDR),
											DLT_STRING("DBus PersistenceAdminRequestCompleted call returned output error code :"),
											DLT_INT(outErrorCode));

	return (sint_t)outErrorCode;
}


/**
 * \brief Connection to DBus callback
 *
 * \note  The function is called when a connection to the D-Bus could be established.
 * 		  According to the documentation the objects should be exported here.
 *
 * \param connection 					[in] Connection, which was acquired
 * \param name 							[in] Bus name
 * \param user_data    					[in] Optionally user data
 *
 * \return void
 */
static void OnBusAcquired_cb(	GDBusConnection *connection,
								const gchar     *name,
								gpointer         user_data)
{
	bool_t	bRetVal;

	DLT_LOG(persComIpcDLTCtx, DLT_LOG_DEBUG,DLT_STRING(LT_HDR), 
											DLT_STRING("Successfully connected to DBus"));

	/* Store the connection. */
	g_pPASDBusConnection = connection;

	/* Export the org.genivi.persistence.admin interface over DBus */
	bRetVal = ExportPersistenceAdminIF(connection);
	if(false == bRetVal)
	{
		/* Error: the interface could not be exported. */
	  	g_main_loop_quit(g_pPASMainLoop);
	}
	else
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR),
												DLT_STRING("Successfully connected to D-Bus and exported object."));
	}
}


/**
 * \brief 	DBus name obtained callback
 *
 * \note 	The function is called when the "bus name" could be acquired on the D-Bus.
 * 
 * \param connection 					[in] Connection over which the bus name was acquired
 * \param name 							[in] Acquired bus name
 * \param user_data    					[in] Optionally user data
 *
 * \return void
 */
static void OnNameAcquired_cb(	GDBusConnection *connection,
        						const gchar     *name,
        						gpointer         user_data)
{
	DLT_LOG(persComIpcDLTCtx, DLT_LOG_INFO, DLT_STRING(LT_HDR),
											DLT_STRING("Successfully obtained D-Bus name:"), DLT_STRING(name));

	/* DBus connection initialized */
	g_bPASDBusConnInit = true;

	/* Notify that the DBus connection is ready */
	if(0 != pthread_mutex_lock (&g_PASDBusMainLoopThreadFlagMtx))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to lock mutex."));
		return;
	}
	g_PASDBusMainLoopThreadFlag = DBUS_MAIN_LOOP_THREAD_FLAG_SET;
	(void)pthread_cond_signal (&g_PASDBusMainLoopThreadFlagCV);
	(void)pthread_mutex_unlock (&g_PASDBusMainLoopThreadFlagMtx);
}


/**
 * \brief 	DBus name lost callback
 *
 * \note 	The function is called if either no connection to D-Bus could be established or
 * 			the bus name could not be acquired.
 * 
 * \param connection 					[in] Connection. If it is NIL, no D-Bus connection could be established.
 *											 Otherwise the bus name was lost.
 * \param name 							[in] Bus name
 * \param user_data    					[in] Optionally user data
 *
 * \return void
 */
static void OnNameLost_cb(	GDBusConnection *connection,
							const gchar     *name,
							gpointer         user_data )
{
	uint32_t	clientIdx;

	if(NIL == connection)
	{
		/* Error: the connection could not be established. */
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("Failed to establish D-Bus connection."));
	}
	else
	{
		/* Error: connection established, but name not obtained. This might be a second instance of the application */
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to obtain / Lost D-Bus name :"),
												DLT_STRING(name));
	}

	/* DBus connection lost */
	g_bPASDBusConnInit = false;

	/* In both cases leave the main loop. */
	g_main_loop_quit(g_pPASMainLoop);

	/* Notify the DBus connection is not ready */
	if(0 != pthread_mutex_lock (&g_PASDBusMainLoopThreadFlagMtx))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to lock mutex."));
		return;
	}
	g_PASDBusMainLoopThreadFlag = DBUS_MAIN_LOOP_THREAD_FLAG_SET;
	(void)pthread_cond_signal (&g_PASDBusMainLoopThreadFlagCV);
	(void)pthread_mutex_unlock (&g_PASDBusMainLoopThreadFlagMtx);

	/* Acquire mutex on array of DBus client info */
	if(0 != pthread_mutex_lock(&g_persIpcDBusClientInfoArrayMtx))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to lock mutex."));
	}
	else
	{
		for(clientIdx = 0; clientIdx < g_persIpcDBusClientInfoArraySize; clientIdx++)
		{
			if(NIL != g_persIpcDBusClientInfoArray[clientIdx])
			{
				if(NIL != g_persIpcDBusClientInfoArray[clientIdx]->busName)
				{
					(void)free(g_persIpcDBusClientInfoArray[clientIdx]->busName);
				}
				if(NIL != g_persIpcDBusClientInfoArray[clientIdx]->objName)
				{
					(void)free(g_persIpcDBusClientInfoArray[clientIdx]->objName);
				}
			}
		}
		(void)pthread_mutex_unlock(&g_persIpcDBusClientInfoArrayMtx);
	}
}

/**
 * \brief 	RegisterPersAdminNotification DBus method callback
 *
 * \note 	Handler for RegisterPersAdminNotification.
 * 			Signature based on generated code.
 */
static gboolean OnHandleRegisterPersAdminNotification (	OipPersistenceAdmin 	*object,
														GDBusMethodInvocation 	*invocation,
														const gchar 			*arg_BusName,
														const gchar 			*arg_ObjName,
														gint 					arg_NotificationFlag,
														guint 					arg_TimeoutMs)
{
	sint_t					retVal 			= PERS_COM_SUCCESS;
	uint32_t				clientId;

	persIpcDBusClientInfo_s	**tmpPersIpcDBusClientInfoArray		= NIL;
	persIpcDBusClientInfo_s	 *pNewClientInfo					= NIL;

	if((NIL == arg_BusName) || (NIL == arg_ObjName))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("Invalid parameter in RegisterPersAdminNotification handler."));
		retVal = PERS_COM_ERR_INVALID_PARAM;
		oip_persistence_admin_complete_register_pers_admin_notification(	object,
																			invocation,
																			(gint)retVal);
		return(TRUE);
	}

	DLT_LOG(persComIpcDLTCtx, DLT_LOG_DEBUG, 	DLT_STRING(LT_HDR),
												DLT_STRING("RegisterPersAdminNotification called for BusName :"),
												DLT_STRING(arg_BusName),
												DLT_STRING("and ObjName :"),
												DLT_STRING(arg_ObjName),
												DLT_STRING("with params : NotificationFlag="),
												DLT_INT(arg_NotificationFlag),
												DLT_STRING(" TimeoutMs="),
												DLT_INT(arg_TimeoutMs));

	/* Check if the client is already registered */
	retVal = persIpcGetIdForDBusInfo(	(pstr_t)arg_BusName,
										(pstr_t)arg_ObjName,
										&clientId);
	if(PERS_COM_SUCCESS == retVal)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_WARN, DLT_STRING(LT_HDR),
												DLT_STRING("DBus client already registered with BusName :"),
												DLT_STRING(arg_BusName),
												DLT_STRING("and ObjName :"),
												DLT_STRING(arg_ObjName));
		retVal = PERS_COM_FAILURE; /* PERS_COM_IPC_ERR_ALREADY_DONE */
		oip_persistence_admin_complete_register_pers_admin_notification(	object,
																			invocation,
																			(gint)retVal);
		return(TRUE);
	}

	/* Acquire mutex on array of DBus client info */
	if(0 != pthread_mutex_lock (&g_persIpcDBusClientInfoArrayMtx))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to lock mutex."));
		retVal = PERS_COM_FAILURE;
		oip_persistence_admin_complete_register_pers_admin_notification(	object,
																			invocation,
																			(gint)retVal);
		return(TRUE);
	}

	/* Add DBus info for the new client */
	if(g_persIpcDBusClientInfoArraySize >= g_persIpcDBusClientInfoMaxArraySize)
	{
		if(g_persIpcDBusClientInfoArraySize > 0)
		{
			tmpPersIpcDBusClientInfoArray = g_persIpcDBusClientInfoArray;

			g_persIpcDBusClientInfoMaxArraySize *= 2;
		}
		else
		{
			g_persIpcDBusClientInfoMaxArraySize = PERS_IPC_INIT_DBUS_INFO_ARRAY_SIZE;
		}

		g_persIpcDBusClientInfoArray = (persIpcDBusClientInfo_s**)malloc(g_persIpcDBusClientInfoMaxArraySize * sizeof(*g_persIpcDBusClientInfoArray));
		if(NIL == g_persIpcDBusClientInfoArray)
		{
			DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 	
													DLT_STRING("Error allocating memory for client info mapping list."));
			g_persIpcDBusClientInfoArray = tmpPersIpcDBusClientInfoArray;
			(void)pthread_mutex_unlock (&g_persIpcDBusClientInfoArrayMtx);
			retVal = PERS_COM_ERR_MALLOC;
			oip_persistence_admin_complete_register_pers_admin_notification(	object,
																				invocation,
																				(gint)retVal);
			return(TRUE);
		}
		else
		{
			(void)memset(g_persIpcDBusClientInfoArray, 0, g_persIpcDBusClientInfoMaxArraySize * sizeof(*g_persIpcDBusClientInfoArray));
		}

		if(g_persIpcDBusClientInfoArraySize > 0)
		{
			(void)memcpy(g_persIpcDBusClientInfoArray, tmpPersIpcDBusClientInfoArray, g_persIpcDBusClientInfoArraySize * sizeof(*g_persIpcDBusClientInfoArray));
		}
	}

	pNewClientInfo = NIL;
	pNewClientInfo = (persIpcDBusClientInfo_s*)malloc(sizeof(*pNewClientInfo));
	if(NIL == pNewClientInfo)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 	
												DLT_STRING("Error allocating memory for client info."));
		(void)pthread_mutex_unlock (&g_persIpcDBusClientInfoArrayMtx);
		retVal = PERS_COM_ERR_MALLOC;
		oip_persistence_admin_complete_register_pers_admin_notification(	object,
																			invocation,
																			(gint)retVal);
		return(TRUE);
	}
	else
	{
		(void)memset(pNewClientInfo, 0, sizeof(*pNewClientInfo));
	}

	/* received Bus Name should be a null terminated string */
	pNewClientInfo->busName = (pstr_t)malloc((strlen(arg_BusName) + 1) * sizeof(*(pNewClientInfo->busName)));
	if(NIL == pNewClientInfo->busName)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 	
												DLT_STRING("Error allocating memory for client info data (busName)."));
		(void)free(pNewClientInfo);
		pNewClientInfo = NIL;
		(void)pthread_mutex_unlock (&g_persIpcDBusClientInfoArrayMtx);
		retVal = PERS_COM_ERR_MALLOC;
		oip_persistence_admin_complete_register_pers_admin_notification(	object,
																			invocation,
																			(gint)retVal);
		return(TRUE);
	}
	else
	{
		(void)memset(pNewClientInfo->busName, 0, (strlen(arg_BusName) + 1) * sizeof(*(pNewClientInfo->busName)));
		(void)memcpy(pNewClientInfo->busName, arg_BusName, (strlen(arg_BusName) * sizeof(*(pNewClientInfo->busName))));
	}

	// received object name should be a null terminated string
	pNewClientInfo->objName = (pstr_t)malloc((strlen(arg_ObjName) + 1) * sizeof(*(pNewClientInfo->objName)));/*DG C8MR2R-MISRA-C:2004 Rule 20.4-SSW_Administrator_0002*/
	if(NIL == pNewClientInfo->objName)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 	
												DLT_STRING("Error allocating memory for client info data (objName)."));
		(void)free(pNewClientInfo->busName);
		(void)free(pNewClientInfo);
		pNewClientInfo = NIL;
		(void)pthread_mutex_unlock (&g_persIpcDBusClientInfoArrayMtx);
		retVal = PERS_COM_ERR_MALLOC;
		oip_persistence_admin_complete_register_pers_admin_notification(	object,
																			invocation,
																			(gint)retVal);
		return(TRUE);
	}
	else
	{
		(void)memset(pNewClientInfo->objName, 0, (strlen(arg_ObjName) + 1) * sizeof(*(pNewClientInfo->objName)));
		(void)memcpy(pNewClientInfo->objName, arg_ObjName, (strlen(arg_ObjName) * sizeof(*(pNewClientInfo->objName))));
	}

	g_persIpcDBusClientInfoArray[g_persIpcDBusClientInfoArraySize++] = pNewClientInfo;

	clientId = g_persIpcDBusClientInfoArraySize - 1;

	DLT_LOG(persComIpcDLTCtx, DLT_LOG_DEBUG, 	DLT_STRING(LT_HDR),
												DLT_STRING("ClientId :"),
												DLT_UINT32(clientId),
												DLT_STRING("for BusName :"),
												DLT_STRING(arg_BusName),
												DLT_STRING("and ObjName :"),
												DLT_STRING(arg_ObjName));

	/* Release array mutex */
	(void)pthread_mutex_unlock (&g_persIpcDBusClientInfoArrayMtx);

	/* Forward the call to the PAS callback */
	retVal = g_pPersIpcPASInfo.pRegCB(	clientId,				/* clientId is the array index */
							 	 	 	arg_NotificationFlag,
							 	 	 	arg_TimeoutMs);
	if(PERS_COM_SUCCESS != retVal)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR, 	DLT_STRING(LT_HDR),
													DLT_STRING("RegisterPersAdminNotification client callback failed with error code :"),
													DLT_INT(retVal));

		/* 
		 * remove the registered client if the PAS callback returned an error,
		 * and forward the error to the PCL client
		 */

		if(0 != pthread_mutex_lock (&g_persIpcDBusClientInfoArrayMtx))
		{
			DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
													DLT_STRING("Failed to lock mutex."));
		}else
		{
			(void)free(g_persIpcDBusClientInfoArray[clientId]->busName);
			(void)free(g_persIpcDBusClientInfoArray[clientId]->objName);
			(void)free(g_persIpcDBusClientInfoArray[clientId]);
			g_persIpcDBusClientInfoArray[clientId] = NIL;
			--g_persIpcDBusClientInfoArraySize;

			(void)pthread_mutex_unlock (&g_persIpcDBusClientInfoArrayMtx);
		}
	}

	oip_persistence_admin_complete_register_pers_admin_notification(	object,
																		invocation,
																		(gint)retVal);

	return(TRUE);
}


/**
 * \brief 	UnregisterPersAdminNotification DBus method callback
 *
 * \note 	Handler for UnregisterPersAdminNotification.
 * 			Signature based on generated code.
 */
static gboolean OnHandleUnregisterPersAdminNotification(OipPersistenceAdmin 	*object,
														GDBusMethodInvocation 	*invocation,
														const gchar 			*arg_BusName,
														const gchar 			*arg_ObjName,
														gint 					arg_NotificationFlag,
														guint 					arg_TimeoutMs)			/* currently not used */
{
	sint_t						retVal 				= PERS_COM_SUCCESS;
	uint32_t					clientId;

	if((NIL == arg_BusName) || (NIL == arg_ObjName))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("Invalid parameter in UnregisterPersAdminNotification handler."));
		retVal = PERS_COM_ERR_INVALID_PARAM;
		oip_persistence_admin_complete_un_register_pers_admin_notification(	object,
																			invocation,
																			(gint)retVal);
		return(TRUE);
	}

	DLT_LOG(persComIpcDLTCtx, DLT_LOG_DEBUG, 	DLT_STRING(LT_HDR),
												DLT_STRING("UnregisterPersAdminNotification called for BusName :"),
												DLT_STRING(arg_BusName),
												DLT_STRING("and ObjName :"),
												DLT_STRING(arg_ObjName),
												DLT_STRING("with params : NotificationFlag="),
												DLT_INT(arg_NotificationFlag),
												DLT_STRING(" TimeoutMs="),
												DLT_INT(arg_TimeoutMs));

	/* Check if client is registered */
	retVal = persIpcGetIdForDBusInfo(	(pstr_t)arg_BusName,
										(pstr_t)arg_ObjName,
										&clientId);
	if(PERS_COM_SUCCESS != retVal)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_WARN, 		DLT_STRING(LT_HDR),
														DLT_STRING("DBus client not registered. BusName :"),
														DLT_STRING(arg_BusName),
														DLT_STRING(" and ObjName :"),
														DLT_STRING(arg_ObjName));
		retVal = PERS_COM_ERR_NOT_FOUND;
		oip_persistence_admin_complete_un_register_pers_admin_notification(	object,
																			invocation,
																			(gint)retVal);
		return(TRUE);
	}

	/* Forward the call to the PAS callback */
	retVal = g_pPersIpcPASInfo.pUnRegCB(clientId,				/* clientId is the array index */
										arg_NotificationFlag);
	if(PERS_COM_SUCCESS != retVal)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR, 	DLT_STRING(LT_HDR),
													DLT_STRING("UnregisterPersAdminNotification client callback failed with error code :"),
													DLT_INT(retVal));
	}
	else
	{
		/* remove the registered client if the PAS callback returned success */
		if(0 != pthread_mutex_lock (&g_persIpcDBusClientInfoArrayMtx))
		{
			DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
													DLT_STRING("Failed to lock mutex."));
		}
		else
		{
			(void)free(g_persIpcDBusClientInfoArray[clientId]->busName);
			(void)free(g_persIpcDBusClientInfoArray[clientId]->objName);
			(void)free(g_persIpcDBusClientInfoArray[clientId]);
			g_persIpcDBusClientInfoArray[clientId] = NIL;
			(void)pthread_mutex_unlock (&g_persIpcDBusClientInfoArrayMtx);
		}
	}

	oip_persistence_admin_complete_un_register_pers_admin_notification(	object,
																		invocation,
																		(gint)retVal);
	return(TRUE);
}


/**
 * \brief 	PersAdminRequestCompleted DBus method callback
 *
 * \note 	Handler for PersistenceAdminRequestCompleted.
 * 			Signature based on generated code.
 */
static gboolean  OnHandlePersAdminRequestCompleted(	OipPersistenceAdmin 	*object,
													GDBusMethodInvocation 	*invocation,
													guint 					arg_RequestId,
													gint 					arg_StatusFlag)
{
	sint_t					retVal 				= PERS_COM_SUCCESS;
	const gchar * 			senderBusName 		= NIL;
	uint32_t				clientId;

	/* Get the sender BusName */
	senderBusName = g_dbus_method_invocation_get_sender(invocation);

	DLT_LOG(persComIpcDLTCtx, DLT_LOG_DEBUG, 	DLT_STRING(LT_HDR),
												DLT_STRING("PersistenceAdminRequestCompleted called by "),
												DLT_STRING(senderBusName),
												DLT_STRING("with params : RequestId="),
												DLT_UINT32(arg_RequestId),
												DLT_STRING("and StatusFlag="),
												DLT_INT(arg_StatusFlag));

	retVal = persIpcGetIdForDBusInfo(	(pstr_t)senderBusName,
										NIL,
										&clientId);
	if(PERS_COM_SUCCESS != retVal)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 
												DLT_STRING("DBus client not registered. BusName :"),
												DLT_STRING(senderBusName));
		retVal = PERS_COM_ERR_NOT_FOUND;
		oip_persistence_admin_complete_persistence_admin_request_completed(	object,
																			invocation,
																			(gint)retVal);
		return (TRUE);
	}

	/* Forward the call to the PAS callback */
	retVal = g_pPersIpcPASInfo.pReqCompleteCB( 	clientId,
												arg_RequestId,
												arg_StatusFlag);
	if(PERS_COM_SUCCESS != retVal)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 	
												DLT_STRING("PersistenceAdminRequestCompleted client callback failed with error code :"),
												DLT_INT(retVal));
	}

	oip_persistence_admin_complete_persistence_admin_request_completed(	object,
																		invocation,
																		(gint)retVal);

	return (TRUE);
}


/**
 * \brief 	PersAdminRequest DBus method callback
 *
 * \note 	Handler for PersistenceAdminRequest.
 * 			Signature based on generated code.
 */
static gboolean  OnHandlePersAdminRequest(	OipPersistenceAdminconsumer *object,
											GDBusMethodInvocation 		*invocation,
											guint 						arg_RequestId,
											gint 						arg_StatusFlag )
{
	sint_t					retVal 				= PERS_COM_SUCCESS;

	DLT_LOG(persComIpcDLTCtx, DLT_LOG_DEBUG, 	DLT_STRING(LT_HDR),
												DLT_STRING("PersistenceAdminRequest called with params : RequestId="),
												DLT_UINT(arg_RequestId),
												DLT_STRING("and StatusFlag="),
												DLT_INT(arg_StatusFlag));

	/* Forward the call to the PCL callback */
	retVal = g_pPersIpcPCLInfo.pReqCB(	arg_RequestId,
										arg_StatusFlag );
	if(PERS_COM_SUCCESS != retVal)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 	
												DLT_STRING("PersistenceAdminRequest client callback failed with error code :"),
												DLT_INT(retVal));
	}

	oip_persistence_adminconsumer_complete_persistence_admin_request(	object,
																		invocation,
																		(gint)retVal);

	return (TRUE);
}


/**
 * \brief Function that exports the org.genivi.persistence.admin interface over DBus
 *
 * \param connection:  Connection over which the interface should be exported
 *
 * \return true if successful, false otherwise
 */
static bool_t		ExportPersistenceAdminIF(GDBusConnection    	*connection)
{
	GError *pError = NIL;

	/* Create object to offer on the DBus */
	g_persIpcDBusPASSkeleton = NIL;
	g_persIpcDBusPASSkeleton = (OipPersistenceAdminSkeleton*) oip_persistence_admin_skeleton_new();
	if(NIL == g_persIpcDBusPASSkeleton)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR), 	
												DLT_STRING("Failed to create PersistenceAdmin object."));
		return false;
	}

	(void)g_signal_connect(g_persIpcDBusPASSkeleton, "handle-register-pers-admin-notification", G_CALLBACK(&OnHandleRegisterPersAdminNotification), NIL);
	(void)g_signal_connect(g_persIpcDBusPASSkeleton, "handle-un-register-pers-admin-notification", G_CALLBACK(&OnHandleUnregisterPersAdminNotification), NIL);
	(void)g_signal_connect(g_persIpcDBusPASSkeleton, "handle-persistence-admin-request-completed", G_CALLBACK(&OnHandlePersAdminRequestCompleted), NIL);


	/* Attach interfaces to the objects and export them */
	if(FALSE == g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(g_persIpcDBusPASSkeleton),
												connection,
												PERSISTENCE_ADMIN_OBJ_PATH,
												&pError))
	{
		/* Error: PersistenceAdmin interface could not be exported. */
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,	DLT_STRING(LT_HDR),
													DLT_STRING("Failed to export PersistenceAdmin interface. Error :"),
													DLT_STRING(pError->message));
		g_error_free(pError);
		g_object_unref(g_persIpcDBusPASSkeleton);
		g_persIpcDBusPASSkeleton = NIL;
		return false;
	}

	return true;
}


/**
 * \brief Function that exports the org.genivi.persistence.adminconsumer interface over DBus
 *
 * \param connection:  Connection over which the interface should be exported
 *
 * \return true if successful, false otherwise
 */
static bool_t		ExportPersistenceAdminConsumerIF(GDBusConnection    	*connection)
{
	GError *pError = NIL;

	/* Create object to offer on the DBus */
	g_persIpcDBusPCLSkeleton = NIL;
	g_persIpcDBusPCLSkeleton = (OipPersistenceAdminconsumerSkeleton*)oip_persistence_adminconsumer_skeleton_new();
	if(NIL == g_persIpcDBusPCLSkeleton)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),	
												DLT_STRING("Failed to create PersistenceAdminConsumer object."));
		return false;
	}

	(void)g_signal_connect(g_persIpcDBusPCLSkeleton, "handle-persistence-admin-request", G_CALLBACK(&OnHandlePersAdminRequest), NIL);

	/* Attach interfaces to the objects and export them */
	if(FALSE == g_dbus_interface_skeleton_export(	G_DBUS_INTERFACE_SKELETON(g_persIpcDBusPCLSkeleton),
													connection,
													PERSISTENCE_ADMIN_CONSUMER_OBJ_PATH,
													&pError))
	{
		/* Error: PersistenceAdminConsumer interface could not be exported. */
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,	DLT_STRING(LT_HDR),
													DLT_STRING("Failed to export PersistenceAdminConsumer interface. Error :"),
													DLT_STRING(pError->message));
		g_error_free(pError);
		g_object_unref(g_persIpcDBusPCLSkeleton);
		g_persIpcDBusPCLSkeleton = NIL;
		return false;
	}
	return true;
}


/* PAS DBus loop thread */
static void*	persIpcPASLoopThread(void *lpParam)
{
	uint32_t 		u32ConnectionId 	= 0;

	/* Create the main loop */
	g_pPASMainLoop = g_main_loop_new(NIL, FALSE);
	if(NIL == g_pPASMainLoop)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to create DBus main loop."));

		/* Notify that the DBus connection is not ready */
		if(0 != pthread_mutex_lock (&g_PASDBusMainLoopThreadFlagMtx))
		{
			DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
													DLT_STRING("Failed to lock mutex."));
		}
		else
		{
			g_PASDBusMainLoopThreadFlag = DBUS_MAIN_LOOP_THREAD_FLAG_SET;
			(void)pthread_cond_signal (&g_PASDBusMainLoopThreadFlagCV);
			(void)pthread_mutex_unlock (&g_PASDBusMainLoopThreadFlagMtx);
		}
		return NIL;
	}

	/* Connect to the D-Bus. Obtain a bus name to offer PAS objects */
	u32ConnectionId = g_bus_own_name( PERSISTENCE_ADMIN_BUS_TYPE
									, PERSISTENCE_ADMIN_BUS_NAME
									, G_BUS_NAME_OWNER_FLAGS_NONE
									, &OnBusAcquired_cb
									, &OnNameAcquired_cb
									, &OnNameLost_cb
									, NIL
									, NIL);

	/* The main loop is only canceled if the Node is completely shut down or the D-Bus connection fails */
	g_main_loop_run(g_pPASMainLoop);

	/* If the main loop returned, clean up. Release bus name and main loop */
	g_bus_unown_name(u32ConnectionId);
	g_main_loop_unref(g_pPASMainLoop);
	g_pPASMainLoop = NIL;

	/* Release the skeleton object */
	if(NIL != g_persIpcDBusPASSkeleton)
	{
		g_object_unref(g_persIpcDBusPASSkeleton);
		g_persIpcDBusPASSkeleton = NIL;
	}

	return NIL;
}


/* PCL DBus loop thread */
static void* 	persIpcPCLLoopThread(void *lpParam)
{
	GError 	*pGError 			= NIL;

	/* Create the main loop */
	g_pPCLMainLoop = g_main_loop_new(NIL, FALSE);

	/* Connect to D-Bus */
	g_pPCLDBusConnection = g_bus_get_sync(	PERSISTENCE_ADMIN_BUS_TYPE,
											NULL,
											&pGError);

	if(NIL == g_pPCLDBusConnection)
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to obtain a DBus connection. Error :"),
												DLT_STRING(pGError->message));
		g_error_free(pGError);
		g_main_loop_unref(g_pPCLMainLoop);
		if(0 != pthread_mutex_lock (&g_PCLDBusMainLoopThreadFlagMtx))
		{
			DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
													DLT_STRING("Failed to lock mutex."));
		}
		else
		{
			g_PCLDBusMainLoopThreadFlag = DBUS_MAIN_LOOP_THREAD_FLAG_SET;
			(void)pthread_cond_signal (&g_PCLDBusMainLoopThreadFlagCV);
			(void)pthread_mutex_unlock (&g_PCLDBusMainLoopThreadFlagMtx);
		}
		return NIL;
	}


	/* DBus connection initialized */
	g_bPCLDBusConnInit = true;

	/* Notify the DBus connection is ready (or not available) */
	if(0 != pthread_mutex_lock (&g_PCLDBusMainLoopThreadFlagMtx))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to lock mutex."));
	}
	g_PCLDBusMainLoopThreadFlag = DBUS_MAIN_LOOP_THREAD_FLAG_SET;
	(void)pthread_cond_signal (&g_PCLDBusMainLoopThreadFlagCV);
	(void)pthread_mutex_unlock (&g_PCLDBusMainLoopThreadFlagMtx);

	g_main_loop_run(g_pPCLMainLoop);

	g_bPCLDBusConnInit = false;

	/* If the main loop returned, clean up */
	g_main_loop_unref(g_pPCLMainLoop);

	/* Release the skeleton object */
	if(NIL != g_persIpcDBusPCLSkeleton)
	{
		g_object_unref(g_persIpcDBusPCLSkeleton);
		g_persIpcDBusPCLSkeleton = NIL;
	}

	/* Release any created proxy object */
	if(NIL != g_persIpcDBusPASProxy)
	{
		g_object_unref(g_persIpcDBusPASProxy);
		g_persIpcDBusPASProxy = NIL;
	}

	return NIL;
}


/* Get DBus client ID */
static sint_t		persIpcGetIdForDBusInfo(const 		pstr_t	busName,
											const 		pstr_t	objName,
											uint32_t			*clientID)
{
	uint32_t				clientIdx;

	if((NIL == busName) || (NIL == clientID))
	{
		return PERS_COM_ERR_INVALID_PARAM;
	}

	/* Acquire mutex on array of DBus client info */
	if(0 != pthread_mutex_lock (&g_persIpcDBusClientInfoArrayMtx))
	{
		DLT_LOG(persComIpcDLTCtx, DLT_LOG_ERROR,DLT_STRING(LT_HDR),
												DLT_STRING("Failed to lock mutex."));
		return PERS_COM_FAILURE;
	}

	/* Check if client already registered */
	for(clientIdx = 0; clientIdx < g_persIpcDBusClientInfoArraySize; ++clientIdx)
	{
		if(NIL != g_persIpcDBusClientInfoArray[clientIdx])
		{
			if(0 == strcmp(busName, g_persIpcDBusClientInfoArray[clientIdx]->busName))
			{
				if(NIL != objName)
				{
					if(0 == strcmp(objName, g_persIpcDBusClientInfoArray[clientIdx]->objName))
					{
						(*clientID) = clientIdx;
						(void)pthread_mutex_unlock (&g_persIpcDBusClientInfoArrayMtx);
						return PERS_COM_SUCCESS;
					}
				}
				else
				{
					(*clientID) = clientIdx;
					(void)pthread_mutex_unlock (&g_persIpcDBusClientInfoArrayMtx);
					return PERS_COM_SUCCESS;
				}
			}
		}
	}

	/* Release array mutex */
	(void)pthread_mutex_unlock (&g_persIpcDBusClientInfoArrayMtx);

	return PERS_COM_ERR_NOT_FOUND;
}
