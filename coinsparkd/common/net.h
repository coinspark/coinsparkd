/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#ifndef _CS_NET_H
#define	_CS_NET_H

#include "declare.h"

#define CS_DCT_NET_SNDCHUNKSIZE     4096
#define CS_DCT_NET_RCVREFRESHPOS   65536
#define CS_DCT_NET_SNDBUFFERSIZE   65536
#define CS_DCT_NET_RCVBUFFERSIZE  262144

#define CS_STT_NET_CONNECTION_INVALID                             0
#define CS_STT_NET_CONNECTION_INITIALIZED                         1
#define CS_STT_NET_CONNECTION_CONNECTED                           2

#define CS_OPT_NET_CONNECT_DEFAULT                          0x00000000
#define CS_OPT_NET_CONNECT_SET_10S_TIMEOUT                  0x00000001


#ifdef _UNIX

#define __declspec(x) x
#define __stdcall
#define _stdcall
#define __forceinline inline

#define WINAPI __stdcall 

extern int errno;
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
//#include <stropts.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "stubs.h"

#else // _UNIX

 #include <winsock.h>

#endif // _UNIX

typedef u_int SOCKET;

#ifdef	__cplusplus
extern "C" {
#endif

/* --- cs_Connection --- */
/* Connection structure */

    
typedef struct cs_Connection
{
    cs_Connection()
    {
         Zero();
    }

    ~cs_Connection()
    {
         Destroy();
    }

	cs_uchar           *m_lpRcvBuf;                                             /* Receive buffer */
	cs_uchar           *m_lpSndBuf;                                             /* Send buffer */
	cs_int32           m_RcvBufSize;                                            /* Receive buffer Size */
	cs_int32           m_SndBufSize;                                            /* Send buffer Size */
	cs_int32           m_RcvSize;                                               /* Receive buffer write pointer */
	cs_int32           m_RcvPos;                                                /* Receive buffer read pointer*/
	cs_int32           m_SndSize;                                               /* Send buffer write pointer */
	cs_int32           m_SndPos;                                                /* Send buffer read pointer*/
	cs_uint32          m_RcvTime;                                               /* Last receive timestamp */
	cs_uint32          m_SndTime;                                               /* Last send timestamp */
    cs_int64           m_Socket;                                                /* Socket */
    cs_int32           m_Status;                                                /* Connection status - CS_STT_NET_CONNECTION constants*/

    void Zero();
    cs_int32 Destroy();
    cs_int32 Initialize(cs_int32 RcvSize,cs_int32 SndSize);
    
    cs_int32 Connect(                                                           /* Opens connection to server */
        cs_char  *lpHost,                                                       /* Host */
        cs_int32 Port,                                                          /* Port */
        cs_int32 RcvBufferSize,                                                 /* Receive buffer size, -1 - use defaults (CS_DCT_NET_RCVBUFFERSIZE) */
        cs_int32 SndBufferSize,                                                 /* Send buffer size, -1 - use defaults (CS_DCT_NET_SNDBUFFERSIZE) */
        cs_int32 Options);                                                      /* Connect options CS_OPT_NET_CONNECT */
    
    void Disconnect();                                                          /* Disconnects from server */
    
    cs_int32 PutData(                                                           /* Put data to send buffer, send if needed */
        void *lpData,                                                           /* Pointer to input data */
        cs_int32 len                                                            /* Bytes to send */
    );
    
    cs_int32 Send(                                                              /* Send data to socket */
    );
    
    cs_int32 SendNow(                                                           /* Send data immediately, without buffer */
        void *lpData,                                                           /* Pointer to input data */
        cs_int32 len                                                            /* Bytes to send */
    );
    
    cs_int32 Recv(                                                              /* Receives data from socket */        
    );
    
    cs_int32 GetSize(                                                           /* Returns number of unread bytes on socket */
    );
    
    void *GetData(                                                              /* Returns pointer to data, valid until next Recv call */
        cs_int32 Size                                                           /* Number of bytes to read */
    );
    
} cs_Connection;





#ifdef	__cplusplus
}
#endif

#endif	/* _CS_NET_H */

