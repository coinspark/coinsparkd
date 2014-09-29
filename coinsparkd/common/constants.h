/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */



#ifndef _CS_CONSTANTS_H
#define	_CS_CONSTANTS_H


#define CS_DCT_MAX_PATH                     1024

/* Allocation options */

#define CS_ALT_DEFAULT                  0x00000013
#define CS_ALT_NOALIGNMENT              0x00000001
#define CS_ALT_INT32                    0x00000002
#define CS_ALT_INT64                    0x00000003
#define CS_ALT_INT                      0x00000004  
#define CS_ALT_ALIGNMENTMASK            0x0000000F          
#define CS_ALT_ZEROINIT                 0x00000010          

/* Log levels */

#define CS_LOG_DEFAULT                  0x00000100
#define CS_LOG_FATAL                    0x00000001
#define CS_LOG_ERROR                    0x00000002
#define CS_LOG_WARNING                  0x00000004
#define CS_LOG_SYSTEM                   0x00000010  
#define CS_LOG_REPORT                   0x00000100          
#define CS_LOG_MINOR                    0x00000200          
#define CS_LOG_DEBUG                    0x00001000          
#define CS_LOG_NO_CODE                  0x00002000
#define CS_LOG_LEVEL                    0x0000FFFF          

/* Error codes */

#define CS_ERR_NOERROR                  0x00000000

/* General errors */

#define CS_ERR_ALLOCATION               0x00000001
#define CS_ERR_BUFFER_FULL              0x00000002
#define CS_ERR_CANNOT_BE_NULL           0x00000003
#define CS_ERR_FILE_NOT_FOUND           0x00000004
#define CS_ERR_READ_ERROR               0x00000005
#define CS_ERR_TIMEOUT                  0x00000006
#define CS_ERR_BUG                      0x00000007
#define CS_ERR_WRITE_ERROR              0x00000008
#define CS_ERR_KEY_NOT_FOUND            0x00000009
#define CS_ERR_KEY_FOUND                0x0000000A
#define CS_ERR_CANNOT_CREATE_SEMAPHORE  0x0000000B
#define CS_ERR_NOT_SUPPORTED            0x0000000C
#define CS_ERR_OUT_OF_RANGE             0x0000000D
#define CS_ERR_FILENAME_TOO_LONG        0x0000000E
#define CS_ERR_WRONG_TRACKED_ASSET      0x0000000F

/* Network errors */

#define CS_ERR_NET_PREFIX               0x00010000
#define CS_ERR_NET                      0x00010001
#define CS_ERR_NET_SND                  0x00010002
#define CS_ERR_NET_RCV                  0x00010003
#define CS_ERR_NET_CANNOT_CONNECT       0x00010004
#define CS_ERR_NET_UNRESOLVED           0x00010005

/* Database errors */

#define CS_ERR_DB_PREFIX                0x00020000
#define CS_ERR_DB                       0x00020001
#define CS_ERR_DB_OPEN                  0x00020002
#define CS_ERR_DB_WRITE                 0x00020003
#define CS_ERR_DB_READ                  0x00020004
#define CS_ERR_DB_DELETE                0x00020005
#define CS_ERR_DB_NOT_OPENED            0x00020006
#define CS_ERR_DB_SHMEM_KEY_NOT_SET     0x00020007
#define CS_ERR_DB_SHMEM_CANNOT_CREATE   0x00020008
#define CS_ERR_DB_SHMEM_CANNOT_OPEN     0x00020009
#define CS_ERR_DB_SHMEM_CANNOT_CONNECT  0x0002000A
#define CS_ERR_DB_SHMEM_ALREADY_OPENED  0x0002000B

/* Bitcoin protocol errors */

#define CS_ERR_BITCOIN_PREFIX           0x00030000
#define CS_ERR_BITCOIN_NOTFOUND         0x00030001


/* Asset errors */

#define CS_ERR_ASSET_PREFIX             0x00040000
#define CS_ERR_ASSET_SPENT              0x00040001
#define CS_ERR_ASSET_GENESIS_NOT_FOUND  0x00040002
#define CS_ERR_ASSET_INVALID_GENESIS    0x00040003



#endif	/* _CS_CONSTANTS_H */

