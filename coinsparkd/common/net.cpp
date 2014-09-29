/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#include "net.h"

void __US_CloseSocket( SOCKET sock )
{
    
#ifdef _UNIX
    
    shutdown( sock, SHUT_RDWR );
    close( sock );
#else // _UNIX
    
    closesocket( sock );
    
#endif // _UNIX
 
}

void __US_SocketCleanup()
{
    
#ifdef _UNIX
    return;
#else // _UNIX
    WSACleanup();
#endif // _UNIX
    
}

void cs_Connection::Zero()
{
    
    m_lpRcvBuf=NULL;
	m_lpSndBuf=NULL;
	m_RcvBufSize=CS_DCT_NET_RCVBUFFERSIZE;
	m_SndBufSize=CS_DCT_NET_SNDBUFFERSIZE;
	m_RcvSize=0;
	m_RcvPos=0;
	m_SndSize=0;
    m_SndPos=0;
	m_RcvTime=0;            
	m_SndTime=0;            
    m_Socket=SOCKET_ERROR;  
    m_Status=CS_STT_NET_CONNECTION_INVALID;
    
}

cs_int32 cs_Connection::Initialize(cs_int32 RcvSize,cs_int32 SndSize)
{
    Destroy();
    
    if(RcvSize>0)m_RcvBufSize=RcvSize;
    if(SndSize>=0)m_SndBufSize=SndSize;
    
    m_lpRcvBuf=(cs_uchar *)cs_New(sizeof(cs_uchar)*m_RcvBufSize,NULL,CS_ALT_DEFAULT);
    if(m_lpRcvBuf==NULL)
     return CS_ERR_ALLOCATION | CS_ERR_NET_PREFIX;
    
    if(m_SndBufSize)
    {
        m_lpSndBuf=(cs_uchar *)cs_New(sizeof(cs_uchar)*m_SndBufSize,NULL,CS_ALT_DEFAULT);
        if(m_lpSndBuf==NULL)
         return CS_ERR_ALLOCATION | CS_ERR_NET_PREFIX;
    }
                 
    m_Status=CS_STT_NET_CONNECTION_INITIALIZED;
            
    return CS_ERR_NOERROR;
}

cs_int32 cs_Connection::Destroy()
{
    
    cs_Delete(m_lpRcvBuf,NULL,CS_ALT_DEFAULT);
    cs_Delete(m_lpSndBuf,NULL,CS_ALT_DEFAULT);

    Zero();
 
    return 0;
}

cs_int32 cs_Connection::Connect(cs_char *lpHost, cs_int32 Port,cs_int32 RcvBufferSize,cs_int32 SndBufferSize,cs_int32 Options)
{
	cs_int32 err;
	cs_int32 timestamp;

	struct hostent *hostEntP;
	struct sockaddr_in sockAddr;

#ifdef _UNIX
    
	in_addr_t addr;
//    struct timeval  timer;
#else // _UNIX
    
    FC_UDWORD   addr;
    
#endif // not _UNIX
    	
    SOCKET newsock;

#ifndef _UNIX
    
	struct linger slinger;
	unsigned int keepa;
	unsigned int nodelay;
	int status;
	WSADATA WSAData;
    
#endif // not _UNIX

#ifdef _USE_UNIX_DOMAIN_
    
	struct sockaddr sockAddr;
    
#endif // _USE_UNIX_DOMAIN_

#ifndef _UNIX
    
	status = WSAStartup(MAKEWORD(1,1), &WSAData);
	if (status != 0)
		return CS_ERR_NET;
	if (HIBYTE(WSAData.wVersion) != 1 || LOBYTE(WSAData.wVersion) != 1) 
    {
		return CS_ERR_NET;
	}

#ifndef WIN32

	WSASetBlockingHook(BlockFunc);

#endif // not WIN32
    
#endif // not _UNIX

#ifndef _USE_UNIX_DOMAIN_
    
	addr = inet_addr(lpHost);

    hostEntP=NULL;
	if (addr != INADDR_NONE)
    {
//		hostEntP = gethostbyaddr((FC_SBYTE *) &addr, sizeof(addr), PF_INET);
    }
	else
    {
		hostEntP = gethostbyname(lpHost);
        if (!hostEntP) 
        {
            __US_SocketCleanup();
    		return CS_ERR_NET_UNRESOLVED;
        }
    }

	newsock = socket(PF_INET, SOCK_STREAM, 0);
	if (newsock == INVALID_SOCKET) 
    {
		__US_SocketCleanup();
		return CS_ERR_NET;
	}
    
 /*
FC_SDWORD flags;
flags = fcntl(newsock,F_GETFL,0);
assert(flags != -1);
fcntl(newsock, F_SETFL, flags | O_NONBLOCK); 
*/
	memset(&sockAddr, 0, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons((cs_uint16)Port);
    if(hostEntP)
    {
        memcpy(&sockAddr.sin_addr, hostEntP->h_addr, hostEntP->h_length);
    }
    else
    {
        memcpy(&sockAddr.sin_addr, &addr, sizeof(addr));
    }
//	memcpy(&sockAddr.sin_addr, &addr, 4);//hostEntP->h_length);

#ifdef _UNIX
/*
    if(Options & CS_OPT_NET_CONNECT_SET_10S_TIMEOUT)
    {
        timer.tv_sec  = 10;
        timer.tv_usec = 0;
        if (setsockopt(newsock, SOL_SOCKET, SO_SNDTIMEO,(char *)&timer, sizeof(timer)) < 0)
        {
            __US_CloseSocket(newsock);                                          // printf("Cannot set send timeout\n");
            __US_SocketCleanup();
            return CS_ERR_NET;            
        }
        if (setsockopt(newsock, SOL_SOCKET, SO_RCVTIMEO,(char *)&timer, sizeof(timer)) < 0)
        {
            __US_CloseSocket(newsock);                                          printf("Cannot set receive timeout\n");
            __US_SocketCleanup();
            return CS_ERR_NET;                        
        }
    }
*/
#endif

	err = connect(newsock, (struct sockaddr *)&sockAddr, sizeof(sockAddr));
	if (err == SOCKET_ERROR) 
    {		
        __US_CloseSocket(newsock);
		__US_SocketCleanup();
		return CS_ERR_NET_CANNOT_CONNECT;
	}

#else // not _USE_UNIX_DOMAIN_
    
	newsock = socket(PF_UNIX, SOCK_STREAM, 0);
	if (newsock == INVALID_SOCKET)
		return CS_ERR_NET;

	_fmemset(&sockAddr, 0, sizeof(sockAddr));
	sockAddr.sa_family = AF_UNIX;
	strcpy(sockAddr.sa_data, "compactdb");
	ret = connect(newsock, &sockAddr, sizeof(sockAddr));
	if (ret == SOCKET_ERROR)
		return CS_ERR_NET;
    
#endif // _USE_UNIX_DOMAIN_
	
    
#ifndef _UNIX
    
	nodelay = FALSE;//TRUE;
	setsockopt(newsock, IPPROTO_TCP, TCP_NODELAY, (char far *)&nodelay, sizeof(nodelay));
	keepa = TRUE;
	setsockopt(newsock, SOL_SOCKET, SO_KEEPALIVE, (char far *)&keepa, sizeof(keepa));
	slinger.l_onoff = 0;
	slinger.l_linger = 0;   /* don't care really */
	setsockopt(newsock, SOL_SOCKET, SO_LINGER, (char far *)&slinger, sizeof(slinger));
    
#endif // not _UNIX

    err=CS_ERR_NOERROR;
    err=Initialize(RcvBufferSize,SndBufferSize);
    
    if(err==0)
    {
        m_Socket=newsock;
        m_Status=CS_STT_NET_CONNECTION_CONNECTED;
        timestamp=(cs_int32)(cs_TimeNow());                
        m_RcvTime=timestamp;
        m_SndTime=timestamp;
    }
    else
    {
		__US_CloseSocket(newsock);
		__US_SocketCleanup();        
    }
    
	return err;
}

void cs_Connection::Disconnect()
{	
    __US_CloseSocket((SOCKET)(m_Socket));
        
    Destroy();
    
	__US_SocketCleanup();
}

cs_int32 cs_Connection::PutData(void *lpData,cs_int32 len)
{
	cs_int32 ret;
	cs_int32 sent;
    
    sent=0;
    while(sent<len)
    {
        if(m_SndSize+len-sent<m_SndBufSize)
        {
            memcpy(m_lpSndBuf+m_SndSize,(cs_char*)lpData+sent,len-sent);
            m_SndSize+=len-sent;
            sent=len;
        }
        else
        {
            memcpy(m_lpSndBuf+m_SndSize,(cs_char*)lpData+sent,m_SndBufSize-m_SndSize);
            m_SndSize=m_SndBufSize;
            sent+=m_SndBufSize-m_SndSize;  
            ret = Send();
            if(ret)
            {
                return ret;
            }
        }
    }
    
	return CS_ERR_NOERROR;
}

cs_int32 cs_Connection::Send()
{
	cs_int32 ret;
	cs_int32 offset,size;
    
    offset=0;
    size=CS_DCT_NET_SNDCHUNKSIZE;
    
    while(offset<m_SndSize)
    {
        if(offset+size>m_SndSize)
        {
            size=m_SndSize-offset;
        }
        ret = send((SOCKET)(m_Socket), (cs_char*)m_lpSndBuf+offset, size, 0);
        if(ret!=m_SndSize)
        {
            return CS_ERR_NET_SND;
        }
        offset+=size;
    }
    
    m_SndSize=0;
    m_SndTime=(cs_int32)cs_TimeNow();
    
	return CS_ERR_NOERROR;
}

cs_int32 cs_Connection::SendNow(void *lpData,cs_int32 len)
{
	cs_int32 ret;
	cs_int32 offset,size;
    
    ret=Send();
    if(ret)
    {
        return ret;
    }
    
    offset=0;
    size=CS_DCT_NET_SNDCHUNKSIZE;
    
    while(offset<len)
    {
        if(offset+size>len)
        {
            size=len-offset;
        }
        ret = send((SOCKET)(m_Socket), (cs_char*)lpData+offset, size, 0);
        if(ret!=size)
        {
            return CS_ERR_NET_SND;
        }
        offset+=size;
    }
    
    m_SndTime=(cs_int32)cs_TimeNow();
    
	return CS_ERR_NOERROR;
}

cs_int32 cs_Connection::Recv()
{
	cs_int32 ret;
	cs_int32 len;
    
    if(m_RcvPos)
    {
        if(m_RcvPos == m_RcvSize)
        {
            m_RcvPos=0;
            m_RcvSize=0;
        }
        else
        {
            if(m_RcvPos>CS_DCT_NET_RCVREFRESHPOS)
            {
                memmove(m_lpRcvBuf,m_lpRcvBuf+m_RcvPos,m_RcvSize-m_RcvPos);
                m_RcvSize-=m_RcvPos;
                m_RcvPos=0;
            }
        }
    }

    len=m_RcvBufSize-m_RcvSize;
    if(len<=0)
    {
        return CS_ERR_BUFFER_FULL;
    }

    ret = recv((SOCKET)(m_Socket), (cs_char*) m_lpRcvBuf+m_RcvSize, len, 0);

    if(ret == SOCKET_ERROR)
    {
        return CS_ERR_NET_RCV;        
    }
   
    if(ret == 0)
    {
        return CS_ERR_NET_RCV;
    }
    
    if (len < ret)
    {
        return CS_ERR_NET_RCV;
    }
    
    m_RcvSize+=ret;
    m_RcvTime=(cs_int32)cs_TimeNow();
    
	return CS_ERR_NOERROR;
}

cs_int32 cs_Connection::GetSize()
{
    return m_RcvSize-m_RcvPos;
}
    
void *cs_Connection::GetData(cs_int32 Size)
{
    if(Size>m_RcvSize-m_RcvPos)
    {
        return NULL;
    }
    
    m_RcvPos+=Size;
    return m_lpRcvBuf+m_RcvPos-Size;
}


