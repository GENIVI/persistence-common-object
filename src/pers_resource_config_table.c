/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*
* Author: Ionut.Ieremie@continental-corporation.com
*
* Implementation of persComRct.h
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date             Author              Reason
* 2013.02.05       uidl9757            CSP_WZ#2220:  Adaptation for open source
* 2012.12.10       uidl9757            CSP_WZ#2060:  Created
*
**********************************************************************************************************************/

#include "persComTypes.h"
#include "string.h"

#include "persComErrors.h"
#include "persComDataOrg.h"
#include "pers_low_level_db_access_if.h"
#include "persComRct.h"

/** 
 * \brief Obtain a handler to RCT indicated by rctPathname
 * \note : RCT is created if it does not exist and (bForceCreationIfNotPresent != 0)
 *
 * \param rctPathname                   [in] absolute path to RCT (length limited to \ref PERS_ORG_MAX_LENGTH_PATH_FILENAME)
 * \param bForceCreationIfNotPresent    [in] if !=0x0, the RCT is created if it does not exist
 *
 * \return >= 0 for valid handler, negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComRctOpen(char const * rctPathname, unsigned char bForceCreationIfNotPresent)
{
    sint_t iErrCode = PERS_COM_SUCCESS ;

    if(NIL != rctPathname)
    {
        if(strlen(rctPathname) >= PERS_ORG_MAX_LENGTH_PATH_FILENAME)
        {
            iErrCode = PERS_COM_ERR_INVALID_PARAM ;
        }
    }
    else
    {
        iErrCode = PERS_COM_ERR_INVALID_PARAM ;
    }

    if(PERS_COM_SUCCESS == iErrCode)
    {
        iErrCode = pers_lldb_open(rctPathname, PersLldbPurpose_RCT, bForceCreationIfNotPresent);
    }

    return iErrCode ;
}

/**
 * \brief Close handler to RCT
 *
 * \param handlerRCT    [in] handler obtained with persComRctOpen
 *
 * \return 0 for success, negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComRctClose(signed int handlerRCT)
{
    sint_t iErrCode = PERS_COM_SUCCESS ;

    if(handlerRCT < 0)
    {
        iErrCode = PERS_COM_ERR_INVALID_PARAM ;
    }

    if(PERS_COM_SUCCESS == iErrCode)
    {
        iErrCode = pers_lldb_close(handlerRCT) ;
    }

    return iErrCode ;
}

/**
 * \brief write a resourceID-value pair into RCT
 *
 * \param handlerRCT    [in] handler obtained with persComRctOpen
 * \param resourceID    [in] resource's identifier (length limited to \ref PERS_RCT_MAX_LENGTH_RESOURCE_ID)
 * \param psConfig      [in] configuration for resourceID
 *
 * \return 0 for success, negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComRctWrite(signed int handlerRCT, char const * resourceID, PersistenceConfigurationKey_s const * psConfig)
{
    sint_t iErrCode = PERS_COM_SUCCESS ;

    if(     (handlerRCT < 0)
        ||  (NIL == resourceID)
        ||  (NIL == psConfig)
    )
    {
        iErrCode = PERS_COM_ERR_INVALID_PARAM ;
    }
    else
    {
        if(strlen(resourceID) >= PERS_RCT_MAX_LENGTH_RESOURCE_ID)
        {
            iErrCode = PERS_COM_ERR_INVALID_PARAM ;
        }
    }

    if(PERS_COM_SUCCESS == iErrCode)
    {
        iErrCode = pers_lldb_write_key(handlerRCT, PersLldbPurpose_RCT, resourceID, (pstr_t)psConfig, sizeof(PersistenceConfigurationKey_s)) ;
    }

    return iErrCode ;
}


/**
 * \brief read a resourceID's configuration from RCT
 *
 * \param handlerRCT    [in] handler obtained with persComRctOpen
 * \param resourceID    [in] resource's identifier (length limited to \ref PERS_RCT_MAX_LENGTH_RESOURCE_ID)
 * \param psConfig_out  [out]where to return the configuration for resourceID
 *
 * \return read size [byte], or negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComRctRead(signed int handlerRCT, char const * resourceID, PersistenceConfigurationKey_s const * psConfig_out)
{
    sint_t iErrCode = PERS_COM_SUCCESS ;

    if(     (handlerRCT < 0)
        ||  (NIL == resourceID)
        ||  (NIL == psConfig_out)
    )
    {
        iErrCode = PERS_COM_ERR_INVALID_PARAM ;
    }
    else
    {
        if(strlen(resourceID) >= PERS_RCT_MAX_LENGTH_RESOURCE_ID)
        {
            iErrCode = PERS_COM_ERR_INVALID_PARAM ;
        }
    }

    if(PERS_COM_SUCCESS == iErrCode)
    {
        iErrCode = pers_lldb_read_key(handlerRCT, PersLldbPurpose_RCT, resourceID, (pstr_t)psConfig_out, sizeof(PersistenceConfigurationKey_s)) ;
    }

    return iErrCode ;
}


/**
 * \brief delete a resourceID's configuration from RCT
 *
 * \param handlerRCT    [in] handler obtained with persComRctOpen
 * \param resourceID    [in] resource's identifier (length limited to \ref PERS_RCT_MAX_LENGTH_RESOURCE_ID)
 *
 * \return 0 for success, or negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComRctDelete(signed int handlerRCT, char const * resourceID)
{
    sint_t iErrCode = PERS_COM_SUCCESS ;

    if(     (handlerRCT < 0)
        ||  (NIL == resourceID)
    )
    {
        iErrCode = PERS_COM_ERR_INVALID_PARAM ;
    }
    else
    {
        if(strlen(resourceID) >= PERS_RCT_MAX_LENGTH_RESOURCE_ID)
        {
            iErrCode = PERS_COM_ERR_INVALID_PARAM ;
        }
    }

    if(PERS_COM_SUCCESS == iErrCode)
    {
        iErrCode = pers_lldb_delete_key(handlerRCT, PersLldbPurpose_RCT, resourceID) ;
    }

    return iErrCode ;
}


/**
 * \brief Find the buffer's size needed to accomodate the listing of resourceIDs in RCT
 *
 * \param handlerRCT    [in] handler obtained with persComRctOpen
 *
 * \return needed size [byte], or negative value for error (\ref PERS_COM_ERROR_CODES_DEFINES)
 */
signed int persComRctGetSizeResourcesList(signed int handlerRCT)
{
    sint_t iErrCode = PERS_COM_SUCCESS ;

    if(handlerRCT < 0)
    {
        iErrCode = PERS_COM_ERR_INVALID_PARAM ;
    }

    if(PERS_COM_SUCCESS == iErrCode)
    {
        iErrCode = pers_lldb_get_size_keys_list(handlerRCT, PersLldbPurpose_RCT) ;
    }

    return iErrCode ;
}


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
signed int persComRctGetResourcesList(signed int handlerRCT, char* listBuffer_out, signed int listBufferSize)
{
    sint_t iErrCode = PERS_COM_SUCCESS ;

    if(     (handlerRCT < 0)
        ||  (NIL == listBuffer_out)
        ||  (listBufferSize <= 0)
    )
    {
        iErrCode = PERS_COM_ERR_INVALID_PARAM ;
    }

    if(PERS_COM_SUCCESS == iErrCode)
    {
        iErrCode = pers_lldb_get_keys_list(handlerRCT, PersLldbPurpose_RCT, (pstr_t)listBuffer_out, listBufferSize) ;
    }

    return iErrCode ;
}

