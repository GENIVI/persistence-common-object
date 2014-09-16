#ifndef PERS_COM_TYPES_H
#define PERS_COM_TYPES_H

/**********************************************************************************************************************
*
* Copyright (C) 2012 Continental Automotive Systems, Inc.
*
* Author: Ionut.Ieremie@continental-corporation.com, guy.sagnes@continental-corporation.com
*
* Interface: protected - Type and constant definitions.
*
* For additional details see
* https://collab.genivi.org/wiki/display/genivi/SysInfraEGPersistenceConceptInterface   
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Date       Author             Reason
* 2014.09.12 uid66235  1.0.1.0  GENIVI:allow support of the standard C99 instead of redeclaration
* 2013.02.05 uidl9757  1.0.0.0  CoC_SSW:Persistence:  interface that defines type and constants
*
**********************************************************************************************************************/

#include <stdbool.h>

#ifdef _ISOC99_SOURCE
#include <stdint.h>
#endif

#ifndef char_t
typedef char char_t ;
#endif

#ifndef NIL
/**
 * \brief Definition of the NIL value to be used.
 */
#define NIL     (0)
#endif /* #ifndef NIL */

#ifndef NULL
/**
 * \brief Definition of NULL to be the same as NIL.
 * \note
 * It is not allowed to use NULL furthermore, use NIL instead. NULL
 * is defined for legacy code only.
 */
#define NULL    NIL
#endif /* #ifndef NULL */




/**
 * \brief Definition to make boolean type common for C and C++
 */
typedef unsigned char bool_t;

/**
 * \brief Definition of false for boolean type in C.
 */
#ifndef false
#define false ((bool_t)0)
#endif

/**
 * \brief Definition of true for boolean type in C.
 */
#ifndef true
#define true ((bool_t)1)
#endif


#include <linux/types.h>  /* from within kernel source-tree ! */

typedef __s8  INT8;
typedef __s16 INT16;
typedef __s32 INT32;
typedef __s64 INT64;

#ifndef _ISOC99_SOURCE
typedef __u8  uint8_t;
typedef __u16 uint16_t;
typedef __u32 uint32_t;
typedef __u64 uint64_t;
#endif

/**
 * \brief 8 bit signed
 */
typedef INT8 sint8_t;

/**
 * \brief 16 bit signed
 */
typedef INT16 sint16_t;

/**
 * \brief 32 bit signed
 */
typedef INT32 sint32_t;

/**
 * \brief 64 bit signed
 */
typedef INT64 sint64_t;


/**
 * \brief Storage for unsigned characters (8 bit).
 */
typedef unsigned char uc8_t;
/**
 * \brief Pointer to storage for unsigned characters (8 bit).
 */
typedef uc8_t * puc8_t;

/**
 * \brief Storage for signed characters (8 bit).
 */
typedef signed char sc8_t;
/**
 * \brief Pointer to storage for signed characters (8 bit).
 */
typedef sc8_t * psc8_t;

/**
 * \brief Definition of a single string element.
 */
typedef char str_t;
/**
 * \brief Pointer to string (to differentiate between characters and strings).
 */
typedef str_t * pstr_t;
/**
 * \brief Pointer to constant string.
 */
typedef const str_t * pconststr_t;
/**
 * \brief Constant pointer to string.
 */
typedef str_t *const constpstr_t;
/**
 * \brief Constant pointer to constant string.
 */
typedef const str_t *const constpconststr_t;

/**
 * \brief Storage for wide characters (16 bit) to support Unicode.
 */
typedef unsigned short uc16_t;
/**
 * \brief Pointer to storage for wide characters (16 bit) to support Unicode.
 */
typedef uc16_t * puc16_t;

/**
 * \brief Pointer to wide string (to differentiate between wide characters
 * and wide strings).
 */
typedef puc16_t pwstr_t;
/**
 * \brief Pointer to constant wide string.
 */
typedef const uc16_t * pconstwstr_t;
/**
 * \brief Constant pointer to wide string.
 */
typedef uc16_t *const constpwstr_t;
/**
 * \brief Constant pointer to constant wide string.
 */
typedef const uc16_t *const constpconstwstr_t;

/**
 * \brief Pointer to UNSIGNED-8-Bit
 */
typedef uint8_t* puint8_t;
/**
 * \brief Pointer to SIGNED-8-Bit
 */
typedef sint8_t* psint8_t;

/**
 * \brief Pointer to UNSIGNED-16-Bit
 */
typedef uint16_t* puint16_t;
/**
 * \brief Pointer to SIGNED-16-Bit
 */
typedef sint16_t* psint16_t;

/**
 * \brief Pointer to UNSIGNED-32-Bit
 */
typedef uint32_t* puint32_t;
/**
 * \brief Pointer to SIGNED-32-Bit
 */
typedef sint32_t* psint32_t;

/**
 * \brief Pointer to UNSIGNED-64-Bit
 */
typedef uint64_t* puint64_t;
/**
 * \brief Pointer to SIGNED-64-Bit
 */
typedef sint64_t* psint64_t;


typedef unsigned int uint_t;


typedef signed int sint_t;


#endif /* #ifndef PERS_COM_TYPES_H */

