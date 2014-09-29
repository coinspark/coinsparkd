/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#ifndef _CS_US_DECLARE_H
#define	_CS_US_DECLARE_H

#include "define.h"
#include "stubs.h"

#ifdef	__cplusplus
extern "C" {
#endif

cs_int32 __US_GetHomeFolder(cs_char *result, cs_int32 size);

cs_handle __US_CreateThread(void* lpThreadAttributes, cs_uint32 dwStackSize,
                        void* lpStartAddress, void* lpParameter,cs_uint32 dwCreationFlags, cs_uint32* lpThreadId);
cs_uint32 __US_GetProcessId();
cs_uint32 __US_WaitForSingleThread(cs_handle hHandle, cs_uint32 dwMilliseconds);
cs_uint32 __US_CloseThread( cs_handle hObject);
void __US_Sleep (cs_uint32 dwMilliseconds);
 
HMODULE	   __US_LoadLibrary   (const cs_char *lpLibFileName);
void		__US_FreeLibrary   (HMODULE hModule);
FARPROC    __US_GetProcAddress(HMODULE hModule,const cs_char *lpProcName);

cs_int32 __US_Fork(cs_char *lpCommandLine,cs_int32 WinState,cs_int32 *lpChildPID);

cs_int32 __US_ShMemCreate(cs_handle *ShMem,  cs_int32 Key, cs_int32 Size);
cs_int32 __US_ShMemOpen(cs_handle *ShMem,  cs_int32 Key);
cs_int32 __US_ShMemClose(cs_handle ShMem);
void*     __US_ShMemMap(cs_handle ShMem);
cs_int32 __US_ShMemUnmap(void *ShMemPtr);
cs_int32 __US_KillProcess(cs_int32 pid);
cs_int32 __US_ChangeWindowVisibility(cs_int32 Visible);
cs_int32 __US_CheckProcess(cs_int32 pid);
cs_int32 __US_BecomeDaemon();
cs_handle __US_SemCreate();
void __US_SemWait(cs_handle sem);
void __US_SemPost(cs_handle sem);
void __US_SemDestroy(cs_handle sem);

cs_int32 __US_Shell(cs_char *command);


#ifdef	__cplusplus
}
#endif

#endif	/* _CS_US_DECLARE_H */

