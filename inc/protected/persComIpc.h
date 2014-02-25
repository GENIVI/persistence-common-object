#ifndef OSS_PERSISTENCE_COMMON_IPC_H
#define OSS_PERSISTENCE_COMMON_IPC_H

/**********************************************************************************************************************
*
* Copyright (C) 2013 Continental Automotive Systems, Inc.
*
* Author: petrica.manoila@continental-corporation.com
*
* Interface: protected - IPC protocol for communication between PAS and PCL
*
* The file defines contains the defines according to
* https://collab.genivi.org/wiki/display/genivi/SysInfraEGPersistenceConceptInterface   
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date       Author             Reason
* 2013.02.26 uidu0250  1.0.0.0  CR CSP_WZ#2739
*
**********************************************************************************************************************/

/** \defgroup PERS_COM_IPC IPC protocol API
 *  \{
 * Persistence Common library IPC component provides centralized access to the communication between
 * PAS (Persistence Administration Service) and PCL (Persistence Client Library) trough C functions.
 * This IPC protocol is based on the Genivi administration / DBUS interface specification.
 * The interface is independent of the IPC stack, that can be specified as a symbol in the build process,
 * and by default will use the GLib DBus binding.
 * 
 * \image html PersCommonIPC.png "PersCommonIPC" 
 */
#ifdef __cplusplus
extern "C"
{
#endif /* #ifdef __cplusplus */

/** \defgroup PERS_COM_IPC_IF_VERSION Interface version
 *  \{
 */
#define PERS_COM_IPC_INTERFACE_VERSION  (0x01000000U)
/** \} */

/** \defgroup PERS_COM_IPC_DEFINES_MODE Persistence mode flags
 * Persistence mode flags
 *  \{
 */
#define PERSISTENCE_MODE_LOCK           (0x0001U)   /**< Request to lock access to device */
#define PERSISTENCE_MODE_SYNC           (0x0002U)   /**< Request to synchronize the cache */
#define PERSISTENCE_MODE_UNLOCK         (0x0004U)   /**< Request to unlock access to device */
/** \} */ /* End of PERS_COM_IPC_DEFINES */

/** \defgroup PERS_COM_IPC_DEFINES_STATUS Persistence status flags
 * Persistence status flags
 *  \{
 */
#define PERSISTENCE_STATUS_LOCKED       (0x0001U)   /**< Access to device locked status flag */
#define PERSISTENCE_STATUS_ERROR        (0x8000U)   /**< Error present status flag */
/** \} */ /* End of PERS_COM_IPC_DEFINES_STATUS */


/** \defgroup PERS_COM_IPC_DEFINES_ERROR Persistence status error codes
 * Persistence status error codes
 *  \{
 */
#define PERSISTENCE_STATUS_ERROR_LOCK_FAILED    (0x01U) /**< Lock request failed */
#define PERSISTENCE_STATUS_ERROR_SYNC_FAILED    (0x02U) /**< Sync request failed */
/** \} */ /* End of PERS_COM_IPC_DEFINES_ERROR */




/*------------------------------------------------------------------------------------
 * Interface to be used by PAS (Persistence Administration Service)
 *------------------------------------------------------------------------------------
 */

/** \defgroup PERS_COM_IPC_PAS API for Persistence Administrator
 *  \{
 *  Definition of the callbacks, structures and functions used by the Persistence Administration Service 
 */

/** \defgroup PERS_COM_IPC_PAS_CALLBACKS Callbacks
 *  \{
 *  The callbacks specified here should be implemented by the Persistence Administration Service and
 *  supplied to the \ref persIpcInitPAS call trough the \ref PersAdminPASInitInfo_s
 *  initialization structure.
 */

/* Register callback signature (to be implemented by PAS) */
/**
 * \brief PCL client registration callback
 * \note  Should be implemented by PAS (Persistence Administration Service) and  used to populate the
 *        \ref PersAdminPASInitInfo_s structure passed to \ref persIpcInitPAS.
 *        Called when a client registers to PAS.
 *        The values of the input parameters (flags, timeout) are the values that the PCL client specified when 
 *        calling \ref persIpcRegisterToPAS.
 *
 * \param clientID                      [in] unique identifier assigned to the registered client
 * \param flags                         [in] flags specifying the notifications to register for (bitfield using any of the flags : ::PERSISTENCE_MODE_LOCK, ::PERSISTENCE_MODE_SYNC and ::PERSISTENCE_MODE_UNLOCK)
 * \param timeout                       [in] maximum time needed to process any supported request (in milliseconds)
 *
 * \return 0 for success, negative value for error (see \ref PERS_COM_ERROR_CODES_DEFINES)
 */     
typedef int     (*persIpcRegisterToPAS_f)(  int             clientID,
                                            unsigned int    flags,
                                            unsigned int    timeout);

/* Un-register callback signature (to be implemented by PAS) */
/**
 * \brief PCL client un-registration callback
 * \note  Should be implemented by PAS (Persistence Administration Service) and  used to populate the
 *        \ref PersAdminPASInitInfo_s structure passed to \ref persIpcInitPAS.
 *        Called when a client un-registers from PAS.
 *        The values of the input parameter flags is the value that the PCL client specified when 
 *        calling \ref persIpcUnRegisterFromPAS.
 *
 * \param clientID                      [in] unique identifier assigned to the registered client
 * \param flags                         [in] flags specifying the notifications to un-register from (bitfield using any of the flags : ::PERSISTENCE_MODE_LOCK, ::PERSISTENCE_MODE_SYNC and ::PERSISTENCE_MODE_UNLOCK)
 *
 * \return 0 for success, negative value for error (see \ref PERS_COM_ERROR_CODES_DEFINES)
 */     
typedef int     (*persIpcUnRegisterFromPAS_f)(  int             clientID,
                                                unsigned int    flags);

/* PersAdminRequestCompleted callback signature (to be implemented by PAS) */
/**
 * \brief PCL request confirmation callback
 * \note  Should be implemented by PAS (Persistence Administration Service) and  used to populate the
 *        \ref PersAdminPASInitInfo_s structure passed to \ref persIpcInitPAS.
 *        Called when a client confirms a request sent by PAS.
 *        The values of the input parameters (requestID, status) are the values that the PCL client specified when 
 *        calling \ref persIpcSendConfirmationToPAS.
 *
 * \param clientID                      [in] unique identifier assigned to the registered client
 * \param requestID                     [in] unique identifier of the request sent by PAS. Should have the same value
 *                                           as the parameter requestID specified by PAS when calling 
                                             \ref persIpcSendRequestToPCL
 * \param status                        [in] the status of the request processed by PCL
 *                                           - In case of success: bitfield using any of the flags, depending on the request : ::PERSISTENCE_STATUS_LOCKED.
 *                                           - In case of error: the sum of ::PERSISTENCE_STATUS_ERROR and an error code \ref PERS_COM_IPC_DEFINES_ERROR is returned.
 *
 * \return 0 for success, negative value for error (see \ref PERS_COM_ERROR_CODES_DEFINES)
 */     
typedef int     (*persIpcSendConfirmationToPAS_f)(  int             clientID,
                                                    int             requestID,
                                                    unsigned int    status);
/** \} */ /* End of PERS_COM_IPC_PAS_CALLBACKS */

/** \defgroup PERS_COM_IPC_PAS_STRUCTURES Structures
 *  \{
 */
/* PAS init struct */
typedef struct PersIpcPASInitInfo_s_
{
    persIpcRegisterToPAS_f              pRegCB;             /* callback for RegisterPersAdminNotification   */
    persIpcUnRegisterFromPAS_f          pUnRegCB;           /* callback for UnRegisterPersAdminNotification */
    persIpcSendConfirmationToPAS_f      pReqCompleteCB;     /* callback for PersistenceAdminRequestCompleted*/
}PersAdminPASInitInfo_s;
/** \} */ /* End of PERS_COM_IPC_PAS_STRUCTURES */


/** \defgroup PERS_COM_IPC_PAS_FUNCTIONS Functions
 *  \{
 */

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
int persIpcInitPAS( PersAdminPASInitInfo_s      * pInitInfo);


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
                                unsigned int    request);

/** \} */ /* End of PERS_COM_IPC_PAS_FUNCTIONS */

/** \} */ /* End of PERS_COM_IPC_PAS */


/*------------------------------------------------------------------------------------
 * Interface to be used by PCL (Persistence Client Library)
 *------------------------------------------------------------------------------------
 */

/** \defgroup PERS_COM_IPC_PCL API for Persistence Client Library
 *  \{
 *  Definition of the callbacks, structures and functions used by the Persistence Client Library
 */

/** \defgroup PERS_COM_IPC_PCL_CALLBACKS Callbacks
 *  \{
 *  The callbacks specified here should be implemented by the Persistence Client Library and
 *  supplied to the \ref persIpcRegisterToPAS call trough the \ref PersAdminPCLInitInfo_s
 *  initialization structure.
 */ 

/* PersAdminRequest callback signature (to be implemented by PCL) */
/**
 * \brief PAS request callback
 * \note  Should be implemented by PCL (Persistence Client Library) and  used to populate the
 *        \ref PersAdminPCLInitInfo_s structure passed to \ref persIpcRegisterToPAS.
 *        Called when PAS performs a request by calling \ref persIpcSendRequestToPCL.
 *        The values of the input parameters (requestID, request) are the values that PAS specified when 
 *        calling \ref persIpcSendRequestToPCL.
 *
 * \param requestID                     [in] a unique identifier generated for every request
 * \param request                       [in] the request received (bitfield using a valid combination of 
 *                                           any of the following flags : 
                                             ::PERSISTENCE_MODE_LOCK, ::PERSISTENCE_MODE_SYNC and ::PERSISTENCE_MODE_UNLOCK)
 *
 * \return 0 for success, negative value for error (see \ref PERS_COM_ERROR_CODES_DEFINES)
 */     
typedef int     (*persIpcSendRequestToPCL_f)(   int             requestID,
                                                unsigned int    request);
/** \} */ /* End of PERS_COM_IPC_PCL_CALLBACKS */


/** \defgroup PERS_COM_IPC_PCL_STRUCTURES Structures
 *  \{
 */
/* PCL init struct  */
typedef struct PersAdminPCLInitInfo_s_
{
    persIpcSendRequestToPCL_f   pReqCB;             /* callback for PersistenceAdminRequest */
}PersAdminPCLInitInfo_s;
/** \} */ /* End of PERS_COM_IPC_PCL_STRUCTURES */


/** \defgroup PERS_COM_IPC_PCL_FUNCTIONS Functions
 *  \{
 */

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
                                unsigned    int                 timeout);

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
int persIpcUnRegisterFromPAS(   unsigned    int     flags );

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
                                    unsigned    int     status);

/** \} */ /* End of PERS_COM_IPC_PCL_FUNCTIONS */

/** \} */ /* End of PERS_COM_IPC_PCL */

#ifdef __cplusplus
}
#endif /* extern "C" { */
/** \} */ /* End of PERS_COM_IPC */
#endif /* OSS_PERSISTENCE_COMMON_IPC_H */
