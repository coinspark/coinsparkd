/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#ifndef _CS_STUBS_H
#define _CS_STUBS_H

#ifdef _UNIX

#define CS_DCT_FOLDERSEPARATOR    '/'

#else

#define CS_DCT_FOLDERSEPARATOR    '\\'

#endif


#ifdef _UNIX

#define __declspec(x) x
#define __stdcall
#define _stdcall
#define __forceinline inline

#define WINAPI __stdcall 

#endif

#define INFINITE            0xFFFFFFFF  // Infinite timeout
#define THREAD_SET_INFORMATION         (0x0020)

struct  HINSTANCE__ { int unused; }; typedef struct HINSTANCE__ *HINSTANCE;
typedef HINSTANCE	HMODULE;    
typedef int (_stdcall *FARPROC)();

#define INVALID_SOCKET  0
#define SOCKET_ERROR  -1

#endif //_CS_STUBS_H


