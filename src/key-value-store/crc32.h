#ifndef CRC32_H
#define CRC32_H

/******************************************************************************
 * Project         Persistence key value store
 * (c) copyright   2014
 * Company         XS Embedded GmbH
 *****************************************************************************/
/******************************************************************************
 * Copyright
 *
 * This Source Code Form is subject to the terms of the
 * Mozilla Public License, v. 2.0. If a  copy of the MPL was not distributed
 * with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
******************************************************************************/
 /**
 * @file           crc32.h
 * @ingroup        Persistence key value store
 * @author         Simon Disch
 * @brief          Header of crc32 checksum generation
 * @see            
 */

#ifdef __cplusplus
extern "C" {
#endif


#define PERS_COM_CRC32_INTERFACE_VERSION  (0x01000000U)
#define CHKSUMBUFSIZE 64

#include <string.h>
#include <stdio.h>

unsigned int pcoCrc32(unsigned int crc, const unsigned char *buf, size_t theSize);
int pcoCalcCrc32Csum(int fd, int startOffset);


#ifdef __cplusplus
}
#endif

#endif /* CRC32_H */
