/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#ifndef _CS_DEFINE_H
#define _CS_DEFINE_H

#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#ifdef _UNIX

#include <unistd.h>
#define _O_BINARY 0

#else

#include <io.h>

#endif

#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h> 
#include <stdlib.h>
#include <math.h>

//#pragma warning( disable : 4996 )

#ifndef CS32

#define CS64

#endif



/* Basic types declarations */

typedef          char           cs_char;                                        /*  8-bit   signed integer */
typedef unsigned char           cs_uchar;                                       /*  8-bit unsigned integer */

#ifdef _UNIX

typedef          short          cs_int16;                                       /* 16-bit  signed integer */
typedef unsigned short          cs_uint16;                                      /* 16-bit unsigned integer */
typedef          int            cs_int32;                                       /* 32-bit  signed integer */
typedef unsigned int            cs_uint32;                                      /* 32-bit unsigned integer */
typedef          long long      cs_int64;                                       /* 64-bit  signed integer */
typedef unsigned long long      cs_uint64;                                      /* 64-bit unsigned integer */

#else

typedef          __int16        cs_int16;                                       /* 16-bit  signed integer */
typedef unsigned __int16        cs_uint16;                                      /* 16-bit unsigned integer */
typedef          __int32        cs_int32;                                       /* 32-bit  signed integer */
typedef unsigned __int32        cs_uint32;                                      /* 32-bit unsigned integer */
typedef          __int64        cs_int64;                                       /* 64-bit  signed integer */
typedef unsigned __int64        cs_uint64;                                      /* 64-bit unsigned integer */

#endif

typedef float                   cs_float;
typedef double                  cs_double;
typedef void *                  cs_handle; 
typedef void *                  cs_lpvoid; 
typedef int                     cs_int;
typedef unsigned int            cs_uint;


#ifdef BC32

#define BITSINTWORD           32   
#define BYTESINTWORD          4   


#else

#define BITSINTWORD           64   
#define BYTESINTWORD          8   

#endif

typedef size_t                  cs_size_t;                                      /* Integer data type used in allocation, memcpy etc. */



#endif /* _CS_DEFINE_H */



