/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#include "bitcoin.h"

void SHA256(void *src,cs_int32 len,cs_uchar *hash)
{
    CoinSparkCalcSHA256Hash((cs_uchar*)src,len,hash);
}


void cs_Message::Zero()
{
    
    m_lpBuf=NULL;
	m_BufSize=CS_DCT_MSG_BUFFERSIZE;
	m_Size=CS_DCT_MSG_HEADERSIZE;
    m_lpArgs=NULL;
    m_BitcoinMagicValue=CS_DCT_MSG_NETWORK_MAIN;
    
}

cs_int32 cs_Message::Initialize(cs_Arguments *lpArgs,cs_int32 Size)
{
    
    Destroy();
    
    if(Size>0)m_BufSize=Size;
    
    m_lpBuf=(cs_uchar *)cs_New(sizeof(cs_uchar)*m_BufSize,NULL,CS_ALT_DEFAULT);
    if(m_lpBuf==NULL)
     return CS_ERR_ALLOCATION | CS_ERR_BITCOIN_PREFIX;
                    
    
    m_lpArgs=lpArgs;
    
    if(m_lpArgs)
    {
        if(strcmp(lpArgs->m_BitcoinNetwork,"testnet") == 0)
        {
            m_BitcoinMagicValue=CS_DCT_MSG_NETWORK_TESTNET;
        }
        if(strcmp(lpArgs->m_BitcoinNetwork,"testnet3") == 0)
        {
            m_BitcoinMagicValue=CS_DCT_MSG_NETWORK_TESTNET3;
        }
        if(strcmp(lpArgs->m_BitcoinNetwork,"namecoin") == 0)
        {
            m_BitcoinMagicValue=CS_DCT_MSG_NETWORK_NAMECOIN;
        }
    }

    return CS_ERR_NOERROR;
    
}

cs_int32 cs_Message::Destroy()
{
    
    cs_Delete(m_lpBuf,NULL,CS_ALT_DEFAULT);

    Zero();
 
    return CS_ERR_NOERROR;
    
}

cs_int32 cs_Message::Clear()
{
    
	m_Size=CS_DCT_MSG_HEADERSIZE;    
    
    return CS_ERR_NOERROR;
}

cs_int32 cs_Message::PutVarInt(cs_uint64 value)
{
    
    cs_int32 size;
    cs_uchar buf[9];
    
    if(value<0xfd)
    {
        buf[0]=(cs_uchar)value;
        size=0;
    }
    else        
    {        
        if(value<0xffff)
        {
            buf[0]=0xfd;size=2;
        }
        else
        {
            if(value<0xffffffff)
            {
                buf[0]=0xfe;size=4;
            }
            else
            {
                buf[0]=0xff;size=8;
            }            
        }        
        cs_PutNumberLittleEndian(buf+1,&value,size,sizeof(value));
    }
    
    return PutData(buf,size+1);                    
    
}


cs_int32 cs_Message::PutData(void *lpData,cs_int32 len)
{
    
    cs_uchar *lpNewBuffer;
    cs_int32 NewSize;

    if(m_Size+len+1>m_BufSize)
    {
        NewSize=((m_Size+len)/CS_DCT_MSG_BUFFERSIZE + 1) * CS_DCT_MSG_BUFFERSIZE;
        lpNewBuffer=(cs_uchar*)cs_New(NewSize,NULL,CS_ALT_DEFAULT);
        if(lpNewBuffer  == NULL)
        {
            return CS_ERR_ALLOCATION;
        }
        
        cs_Delete(m_lpBuf,NULL,CS_ALT_DEFAULT);
        m_lpBuf=lpNewBuffer;
        m_BufSize=NewSize;
    }
    
    memcpy(m_lpBuf+m_Size,lpData,len);
    m_Size+=len;
    m_lpBuf[m_Size]=0;    
    
	return CS_ERR_NOERROR;
    
}

void *cs_Message::GetData()
{
    return m_lpBuf;
}

cs_int32 cs_Message::GetSize()
{
    return m_Size;
}

cs_int32 cs_Message::Complete(cs_char *lpCommand)
{

    cs_int32 len;
    cs_uchar hash[SHA256_DIGEST_LENGTH];
    
    cs_PutNumberLittleEndian(m_lpBuf+0,&m_BitcoinMagicValue,4,sizeof(m_BitcoinMagicValue));
    
    len=strlen(lpCommand);
    memset(m_lpBuf+4,0,12);
    memcpy(m_lpBuf+4,lpCommand,len);
    m_lpBuf[4+len]=0;
    
    len=m_Size-CS_DCT_MSG_HEADERSIZE;
    cs_PutNumberLittleEndian(m_lpBuf+16,&len,4,sizeof(len));
    
    SHA256(m_lpBuf+CS_DCT_MSG_HEADERSIZE, len, hash);
    SHA256(hash, SHA256_DIGEST_LENGTH, hash);
    
    memcpy(m_lpBuf+20,hash,4);
    
    return m_Size;
    
}

void bitcoin_block_hash(cs_uchar *block_header)
{
    SHA256(block_header+CS_DCT_BLOCK_HEAD_OFFSET, 80, block_header);
    SHA256(block_header, SHA256_DIGEST_LENGTH, block_header);    
}

void bitcoin_tx_hash(cs_uchar *lpTxHash,cs_uchar *lpTx,cs_int32 TxSize)
{
    SHA256(lpTx, TxSize, lpTxHash);
    SHA256(lpTxHash, SHA256_DIGEST_LENGTH, lpTxHash);    
}


void bitcoin_hash_to_string(cs_char *out,cs_uchar *hash)
{
    cs_int32 i;
    for(i=0;i<CS_DCT_HASH_BYTES;i++)
    {
        sprintf(out+(i*2),"%02x",hash[CS_DCT_HASH_BYTES-1-i]);
    }
    
    out[64]=0;      
}



void bitcoin_string_to_hash(cs_char *out,cs_uchar *hash)
{
    cs_int32 i;
    for(i=0;i<CS_DCT_HASH_BYTES;i++)
    {
        hash[CS_DCT_HASH_BYTES-1-i]=cs_HexToUChar(out+i*2);
    }    
}


cs_int32 bitcoin_get_varint_size(cs_uchar byte)
{
    if(byte<0xfd)return 0;
    if(byte==0xfd)return 2;
    if(byte==0xfe)return 4;
    return  8;
}

cs_int32 bitcoin_put_varint(cs_uchar *buf,cs_int64 value)
{
    cs_int32 varint_size,shift;
    
    varint_size=1;
    shift=0;
    if(value>=0xfd)
    {
        shift=1;
        if(value>=0xffff)
        {
            if(value>=0xffffffff)
            {
                buf[0]=0xff;
                varint_size=8;
            }        
            else
            {
                buf[0]=0xfe;
                varint_size=4;
            }
        }        
        else
        {
            buf[0]=0xfd;
            varint_size=2;            
        }
    }

    cs_PutNumberLittleEndian(buf+shift,&value,varint_size,sizeof(value));
    return shift+varint_size;
}

cs_int32 bitcoin_wait_for_message(cs_Connection *lpConnection,cs_char *lpCommand,cs_uint64 *payload_size,cs_handle Log)
{
    cs_int32 StopTime;
    cs_int32 err;
    cs_uint64 size,total;
    cs_char *ptr;
    
    err=CS_ERR_NOERROR;
    
    StopTime=(cs_int32)cs_TimeNow()+CS_DCT_MSG_TIMEOUT;

    while((cs_int32)cs_TimeNow()<StopTime)
    {
        size=lpConnection->GetSize();

        while(size<CS_DCT_MSG_HEADERSIZE)
        {
            err=lpConnection->Recv();
            if(err)
            {
                return err;
            }
            size=lpConnection->GetSize();
        }
        
        ptr=(cs_char*)(lpConnection->GetData(CS_DCT_MSG_HEADERSIZE));
        *payload_size=cs_GetUInt64LittleEndian(ptr+16,4);
        cs_LogMessage(Log,CS_LOG_MINOR,"C-0078","Bitcoin message: ",ptr+4);      
        if(strcmp(ptr+4,lpCommand) == 0)
        {
            return CS_ERR_NOERROR;
        }
        
        size=lpConnection->GetSize();
        ptr=(cs_char*)(lpConnection->GetData(size));
        total=size;
        while(total<*payload_size)
        {
            err=lpConnection->Recv();
            if(err)
            {
                return err;
            }
            if((cs_int32)cs_TimeNow()>=StopTime)
            {
                return CS_ERR_TIMEOUT;                            
            }
            size=lpConnection->GetSize();
            if(total+size>*payload_size)
            {
                size=*payload_size-total;
            }
            ptr=(cs_char*)(lpConnection->GetData(size));
            total+=size;
        }
    }            
    
    return CS_ERR_TIMEOUT;            
}

void bitcoin_prepare_address(cs_uchar* dest,cs_char* lpAddress,cs_int32 Port,cs_int32 SetTimestamp)
{
    cs_uint64 value;
    cs_int32 pos;
    cs_char *ptr;
    
    if(SetTimestamp)
    {
        value=(cs_uint64)cs_TimeNow();        
        cs_PutNumberLittleEndian(dest+0,&value,4,sizeof(value));
    }
    
    value=1;
    cs_PutNumberLittleEndian(dest+4,&value,8,sizeof(value));
    
    memset(dest+12,0,10);
    value=0xFFFF;
    cs_PutNumberLittleEndian(dest+22,&value,2,sizeof(value));
    
    ptr=lpAddress;
    pos=4;
    while(pos)
    {
        value=0;
        while((*ptr != '.') && (*ptr!=0x00))
        {
            value=value*10+(*ptr-0x30);
            ptr++;
        }
        ptr++;
        cs_PutNumberLittleEndian(dest+28-pos,&value,1,sizeof(value));
        pos--;
    }
    
    cs_PutNumberBigEndian(dest+28,&Port,2,sizeof(Port));
}


cs_int32 bitcoin_prepare_message_version(cs_Message *lpMessage,cs_int32 PretendRealNode)
{
    cs_uint64 value;
    cs_uchar buf[128];
    
    lpMessage->Clear();
    
    value=CS_DCT_MSG_PROTOCOL_VERSION;
    cs_PutNumberLittleEndian(buf+0,&value,4,sizeof(value));
    
    value=1;
    cs_PutNumberLittleEndian(buf+4,&value,8,sizeof(value));
        
    value=(cs_uint64)cs_TimeNow();        
    cs_PutNumberLittleEndian(buf+12,&value,8,sizeof(value));
    
    bitcoin_prepare_address(buf+20-4,lpMessage->m_lpArgs->m_BitcoinClientAddress,lpMessage->m_lpArgs->m_BitcoinClientPort,0);
    if(PretendRealNode)
    {
        bitcoin_prepare_address(buf+46-4,"127.0.0.1",8333,0);
    }
    else
    {
        bitcoin_prepare_address(buf+46-4,"0.0.0.0",0,0);        
    }
    
    value=(cs_uint64)cs_TimeNow();        
    cs_PutNumberLittleEndian(buf+72,&value,8,sizeof(value));
    
    value=0;
    cs_PutNumberLittleEndian(buf+80,&value,1,sizeof(value));
 
    value=0;
    cs_PutNumberLittleEndian(buf+81,&value,4,sizeof(value));
    
    value=0;
    cs_PutNumberLittleEndian(buf+85,&value,1,sizeof(value));
    
    lpMessage->PutData(buf,86);
    
    return lpMessage->Complete("version");
}

cs_int32 bitcoin_prepare_message_getdata(cs_Message *lpMessage,cs_int32 type,cs_uchar *lpHash) 
{
    cs_uint64 value;
    cs_uchar buf[128];
    
    lpMessage->Clear();
    
    value=1;
    cs_PutNumberLittleEndian(buf+0,&value,1,sizeof(value));
    
    value=type;
    cs_PutNumberLittleEndian(buf+1,&value,4,sizeof(value));
    
    lpMessage->PutData(buf,5);
    lpMessage->PutData(lpHash,32);
    
    
    return lpMessage->Complete("getdata");
}

cs_int32 bitcoin_prepare_message_mempool(cs_Message *lpMessage) 
{
    lpMessage->Clear();
    
    return lpMessage->Complete("mempool");
}

cs_int32 bitcoin_find_op_return(cs_uchar *script,cs_int32 size)
{
    cs_uchar *ptr;
    cs_uchar *ptrEnd;
    
    ptr=script;
    ptrEnd=ptr+size;
    
    while(ptr<ptrEnd)
    {
        if(*ptr==0x6a)
        {
            return ptr+1-script;
        }
        if(*ptr>=1)
        {
            if(*ptr<76)
            {
                ptr+=*ptr;
            }
            else
            {
                if(*ptr<79)
                {
                    if(*ptr==76)
                    {
                        ptr+=*(ptr+1)+1;
                    }
                    if(*ptr==77)
                    {
                        ptr+=cs_GetUInt64LittleEndian(ptr+1,2)+2;
                    }
                    if(*ptr==78)
                    {
                        ptr+=cs_GetUInt64LittleEndian(ptr+1,4)+4;
                    }
                }
            }
        }
        ptr++;            
    }
    
    return ptr-script;
}

