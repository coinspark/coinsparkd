/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#include "declare.h"
#include "db.h"
#include "args.h"
#include "state.h"
#include "bitcoin.h"

extern cs_Arguments *g_Args;
extern cs_State            *g_State;
extern cs_handle           g_Log;
extern cs_Message          *g_Message;

cs_int32 TrackerProcessAsyncMessage(cs_int32 count);
cs_int32 RemoveBlock(cs_int32 mempool);

/*
 * --- Writes block headers to file
 */


cs_int32 WriteBlockChain()
{
    cs_char BlockChainFile[CS_DCT_MAX_PATH];
    cs_uchar buf[CS_DCT_BLOCK_HEAD_SIZE];
    cs_int fHan;
    cs_int32 bytes_read,bytes_written,size,offset,old_chain;
    cs_int32 err;
    
    err=CS_ERR_NOERROR;
    cs_LogShift(g_Log);
    
    memset(buf,0,CS_DCT_BLOCK_HEAD_SIZE);
    
    sprintf(BlockChainFile,"%s%s",g_Args->m_DataDir,CS_DCT_FILE_BLOCKCHAIN);

    fHan=open(BlockChainFile,_O_BINARY | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    
    if(fHan<=0)
    {
        cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0022","Cannot open blockchain file","");      
        err=CS_ERR_FILE_NOT_FOUND;
        return err;
    }
    
    old_chain=0;
    bytes_read=read(fHan,buf,CS_DCT_BLOCK_HEAD_SIZE);        
    if(bytes_read==CS_DCT_BLOCK_HEAD_SIZE)
    {
        old_chain=cs_GetUInt64LittleEndian(buf+CS_DCT_HASH_BYTES,4);        
    }
    else
    {        
        cs_LogMessage(g_Log,CS_LOG_SYSTEM,"C-0023","Blockchain file created","");      
        lseek64(fHan,0,SEEK_SET);
        bytes_written=write(fHan,g_State->m_BlockChain,CS_DCT_BLOCK_HEAD_SIZE);
        if(bytes_written!=CS_DCT_BLOCK_HEAD_SIZE)
        {
            cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0024","Cannot initialize blockchain data","");      
            err=CS_ERR_WRITE_ERROR;
            goto exitlbl;
        }
    }
    
    
    if(g_State->m_BlockChainHeight>old_chain)
    {
        size=(g_State->m_BlockChainHeight-old_chain)*CS_DCT_BLOCK_HEAD_SIZE;
        offset=(old_chain+1)*CS_DCT_BLOCK_HEAD_SIZE;
                
        lseek64(fHan,offset,SEEK_SET);
        bytes_written=write(fHan,g_State->m_BlockChain+offset,size);
        if(bytes_written!=size)
        {
            cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0025","Cannot write blockchain data","");      
            err=CS_ERR_WRITE_ERROR;
            goto exitlbl;
        }
    }
    
    old_chain=g_State->m_BlockChainHeight;
    
    cs_PutNumberLittleEndian(buf,&old_chain,4,sizeof(old_chain));
    lseek64(fHan,CS_DCT_HASH_BYTES,SEEK_SET);
    bytes_written=write(fHan,buf,4);
    if(bytes_written!=4)
    {
        cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0026","Cannot write blockchain data header","");      
        err=CS_ERR_WRITE_ERROR;
        goto exitlbl;
    }

exitlbl:    

    close(fHan);
    
    return err;
}

/*
 * --- Read block headers from file
 */


cs_int32 ReadBlockChain()
{
    cs_char BlockChainFile[CS_DCT_MAX_PATH];
    cs_uchar buf[CS_DCT_BLOCK_HEAD_SIZE];
    cs_int fHan;
    cs_int32 bytes_read,size;
    cs_int32 err;
    
    err=CS_ERR_NOERROR;
    cs_LogShift(g_Log);
    
    if(strlen(g_Args->m_GenesisBlock)==0)
    {
        if(strcmp(g_Args->m_BitcoinNetwork,"main")==0)
        {
            strcpy(g_Args->m_GenesisBlock,CS_DCT_GENESIS_BLOCK_MAIN);
        }
        if(strcmp(g_Args->m_BitcoinNetwork,"testnet3")==0)
        {
            strcpy(g_Args->m_GenesisBlock,CS_DCT_GENESIS_BLOCK_TESTNET3);
        }
    }
    
    
    sprintf(BlockChainFile,"%s%s",g_Args->m_DataDir,CS_DCT_FILE_BLOCKCHAIN);
    g_State->m_BlockChainHeight=0;
    fHan=open(BlockChainFile,_O_BINARY | O_RDONLY);

    if(fHan>0)
    {
        bytes_read=read(fHan,buf,CS_DCT_BLOCK_HEAD_SIZE);        
        g_State->m_BlockChainHeight=cs_GetUInt64LittleEndian(buf+CS_DCT_HASH_BYTES,4);        
    }
    
    g_State->m_BlockChainAllocSize=cs_AllocSize(g_State->m_BlockChainHeight+1,32768,CS_DCT_BLOCK_HEAD_SIZE);

    g_State->m_BlockChain=(cs_uchar*)cs_New(g_State->m_BlockChainAllocSize,NULL,CS_ALT_DEFAULT);
    if(g_State->m_BlockChain == NULL)
    {
        err=CS_ERR_ALLOCATION;
        goto exitlbl;
    }
    
    if(fHan>0)
    {
        memcpy(g_State->m_BlockChain,buf,CS_DCT_BLOCK_HEAD_SIZE);
        size=g_State->m_BlockChainHeight*CS_DCT_BLOCK_HEAD_SIZE;
        bytes_read=read(fHan,g_State->m_BlockChain+CS_DCT_BLOCK_HEAD_SIZE,size);        
        if(bytes_read!=size)
        {
            err=CS_ERR_READ_ERROR;
            close(fHan);
            goto exitlbl;
        }                
        close(fHan);
        fHan=0;
    }
    else
    {
        memset(g_State->m_BlockChain,0,CS_DCT_BLOCK_HEAD_SIZE);
        bitcoin_string_to_hash(g_Args->m_GenesisBlock,g_State->m_BlockChain);
    }
    
exitlbl:    
    return err;
}

/*
 * --- Writes asset state to file
 */

cs_int32 WriteAssetState()
{
    cs_char AssetStateFile[CS_DCT_MAX_PATH];
    cs_uchar buf[CS_DCT_ASSET_STATE_SIZE];
    cs_int fHan;
    cs_int32 bytes_read,bytes_written,size,old_count;
    cs_int32 err;
    
    err=CS_ERR_NOERROR;
    cs_LogShift(g_Log);
    
    memset(buf,0,CS_DCT_ASSET_STATE_SIZE);
    
    sprintf(AssetStateFile,"%s%s",g_Args->m_DataDir,CS_DCT_FILE_ASSETSTATE);

    fHan=open(AssetStateFile,_O_BINARY | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    
    if(fHan<=0)
    {
        cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0027","Cannot open assetstate file","");      
        err=CS_ERR_FILE_NOT_FOUND;
        return err;
    }
    
    old_count=0;
    bytes_read=read(fHan,buf,CS_DCT_ASSET_STATE_SIZE);        
    if(bytes_read==CS_DCT_ASSET_STATE_SIZE)
    {
        old_count=cs_GetUInt64LittleEndian(buf,4);        
    }
    else
    {        
        cs_LogMessage(g_Log,CS_LOG_SYSTEM,"C-0028","Assetstate file created","");      
    }
    
    
    if(g_State->m_AssetStateCount>old_count)
    {
        lseek64(fHan,0,SEEK_SET);
        
        size=(g_State->m_AssetStateCount)*CS_DCT_ASSET_STATE_SIZE;
        
        bytes_written=write(fHan,g_State->m_AssetState,size);
        if(bytes_written!=size)
        {
            cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0029","Cannot write assetstate data","");      
            err=CS_ERR_WRITE_ERROR;
            goto exitlbl;
        }
    }
    
exitlbl:    

    close(fHan);
    
    return err;
}


/*
 * --- Read asset state from file
 */

cs_int32 ReadAssetState()
{
    cs_char AssetStateFile[CS_DCT_MAX_PATH];
    cs_uchar buf[CS_DCT_ASSET_STATE_SIZE];
    cs_int fHan;
    cs_int32 bytes_read,size,value;
    cs_int32 err;
    
    err=CS_ERR_NOERROR;
    cs_LogShift(g_Log);
        
    sprintf(AssetStateFile,"%s%s",g_Args->m_DataDir,CS_DCT_FILE_ASSETSTATE);
    g_State->m_AssetStateCount=0;
    fHan=open(AssetStateFile,_O_BINARY | O_RDONLY);

    bytes_read=0;
    if(fHan>0)
    {
        bytes_read=read(fHan,buf,CS_DCT_ASSET_STATE_SIZE);        
        if(bytes_read==CS_DCT_ASSET_STATE_SIZE)
        {
            g_State->m_AssetStateCount=cs_GetUInt64LittleEndian(buf+CS_OFF_ASSET_STATE_ASSET_COUNT,4);        
        }
    }
    
    g_State->m_AssetStateAllocSize=cs_AllocSize(g_State->m_AssetStateCount+1,256,CS_DCT_ASSET_STATE_SIZE);

    g_State->m_AssetState=(cs_uchar*)cs_New(g_State->m_AssetStateAllocSize,NULL,CS_ALT_DEFAULT);
    if(g_State->m_AssetState == NULL)
    {
        err=CS_ERR_ALLOCATION;
        goto exitlbl;
    }
    
    if(bytes_read==CS_DCT_ASSET_STATE_SIZE)
    {
        memcpy(g_State->m_AssetState,buf,CS_DCT_ASSET_STATE_SIZE);
        if(g_State->m_AssetStateCount>1)
        {
            size=(g_State->m_AssetStateCount-1)*CS_DCT_ASSET_STATE_SIZE;
            bytes_read=read(fHan,g_State->m_AssetState+CS_DCT_ASSET_STATE_SIZE,size);        
            if(bytes_read!=size)
            {
                err=CS_ERR_READ_ERROR;
                close(fHan);
                goto exitlbl;
            }                
        }
        close(fHan);
        fHan=0;
    }
    else
    {
        memset(g_State->m_AssetState,0,CS_DCT_ASSET_STATE_SIZE);
        value=0;
        cs_PutNumberLittleEndian(g_State->m_AssetState,&value,4,sizeof(value));
        value=81;
        cs_PutNumberLittleEndian(g_State->m_AssetState+4,&value,4,sizeof(value));
        value=0;
        cs_PutNumberLittleEndian(g_State->m_AssetState+8,&value,4,sizeof(value));
        g_State->m_AssetStateCount=1;
        value=g_State->m_AssetStateCount;
        cs_PutNumberLittleEndian(g_State->m_AssetState+CS_OFF_ASSET_STATE_ASSET_COUNT,&value,4,sizeof(value));
        
        
        WriteAssetState();
        
        
    }
    
exitlbl:    
    return err;
}

/*
 * --- Returns minimal/maximal not-processed block id 
 */

cs_int32 GetRelevantBlockID(cs_int32 mode)
{
    cs_int32 value,block,i,state;
    
    block=g_State->m_BlockChainHeight+1;

    for(i=0;i<g_State->m_AssetStateCount;i++)
    {
        state=cs_GetUInt64LittleEndian(g_State->m_AssetState+i*CS_DCT_ASSET_STATE_SIZE+CS_OFF_ASSET_STATE_STATE,4);
        if(state==0)
        {
            value=cs_GetUInt64LittleEndian(g_State->m_AssetState+i*CS_DCT_ASSET_STATE_SIZE+CS_OFF_ASSET_STATE_NEXT_BLOCK,4);            
            if(value<=g_State->m_BlockChainHeight)
            {
                switch(mode)
                {
                    case 0:                                                     // Minimal relevant
                        if(value<block)
                        {
                            block=value;
                        }
                        break;
                    case 1:                                                     // Maximal relevant
                        if(value>block)
                        {
                            block=value;
                        }
                        break;
                }
            }
        }
    }

    return block;
}

/*
 * --- Updates blockchain structure
 */


cs_int32 UpdateBlockChain()
{
    cs_int32 hash_count,pos,shift,varint_size,count,last;
    cs_int32 err;
    cs_uint32 value;
    cs_uchar buf[CS_DCT_BLOCK_HEAD_SIZE+1];    
    cs_char  Out[65];    
    cs_uchar *ptr;
    cs_uchar *lpNewChain;
    cs_int32 NewChainSize;

    cs_LogShift(g_Log);

    hash_count=0;                                                               /// Calculates number of hashes which should be sent in getheaders message
    if(g_State->m_BlockChainHeight)
    {        
        shift=1;
        pos=g_State->m_BlockChainHeight;
        while(pos>1)
        {
            pos-=shift;
            hash_count++;
            if(hash_count>10)
            {
                shift*=2;
            }
        }
        hash_count++;
    }
    else
    {
        hash_count=1;
    }
    
    err=CS_ERR_NOERROR;
    
    g_Message->Clear();
    value=CS_DCT_MSG_PROTOCOL_VERSION;
    cs_PutNumberLittleEndian(buf,&value,4,sizeof(value));
    
    if(!err)g_Message->PutData(buf,4);    
    if(!err)g_Message->PutVarInt(hash_count);
    
    hash_count=0;
    if(g_State->m_BlockChainHeight)
    {        
        shift=1;
        pos=g_State->m_BlockChainHeight;
        while(pos>1)
        {
            if(!err)g_Message->PutData(g_State->m_BlockChain+pos*CS_DCT_BLOCK_HEAD_SIZE,CS_DCT_HASH_BYTES);
            pos-=shift;
            hash_count++;
            if(hash_count>10)
            {
                shift*=2;
            }
        }
        if(!err)g_Message->PutData(g_State->m_BlockChain+1*CS_DCT_BLOCK_HEAD_SIZE,CS_DCT_HASH_BYTES);
        hash_count++;        
    }
    else
    {
        g_Message->PutData(g_State->m_BlockChain+0*CS_DCT_BLOCK_HEAD_SIZE,CS_DCT_HASH_BYTES);
        hash_count++;
    }
    
    memset(buf,0,CS_DCT_HASH_BYTES);
    if(!err)g_Message->PutData(buf,CS_DCT_HASH_BYTES);
    
    if(err)
    {
        return err;
    }
    
    g_Message->Complete("getheaders");                                          // GetHeaders message to bitcoin client

    err=TrackerProcessAsyncMessage(1);
    
    if(err)
    {
        sprintf((cs_char*)buf,"Error: %08X",err);
        cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0030","Get headers failed",(cs_char*)buf);      
        return err;
    }
    
    ptr=g_State->m_ResponseMessage+CS_DCT_MSG_HEADERSIZE;
    count=(cs_int32)(*ptr);
    varint_size=bitcoin_get_varint_size(*ptr);
    ptr++;
    if(varint_size)
    {
        count=cs_GetUInt64LittleEndian(ptr,varint_size);
        ptr+=varint_size;
    }
    
    bitcoin_hash_to_string(Out,ptr+4);
    
    pos=g_State->m_BlockChainHeight;                                            // Identifying last block
    if(count)
    {
        if(g_State->m_BlockChainHeight)
        {
            while(memcmp(g_State->m_BlockChain+pos*CS_DCT_BLOCK_HEAD_SIZE,ptr+4,CS_DCT_HASH_BYTES))
            {
                bitcoin_hash_to_string(Out,g_State->m_BlockChain+pos*CS_DCT_BLOCK_HEAD_SIZE);
                pos--;
                if(pos==0)
                {
                    err=CS_ERR_BITCOIN_NOTFOUND;
                    cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0031","Last block in main chain not found","");      
                    return err;                
                }
            }
        }
    }
    
    last=pos;
    if(last<g_State->m_BlockChainHeight)                                        // Blockchain substitution
    {
        
        sprintf((cs_char*)buf,"%8d -> %8d",g_State->m_BlockChainHeight,last);
        cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0032","Blockchain substitution",(cs_char*)buf);      

        if(g_State->m_BlockChainHeight-last>=CS_DCT_BLOCK_PURGE_DELAY)
        {
            sprintf((cs_char*)buf,"%8d",pos);
            cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0033","Blockchain substitution too deep: ",(cs_char*)buf);      
            return err;            
        }

        pos=g_State->m_BlockChainHeight;
        while(pos>last)
        {
            err=RemoveBlock(0);
            if(err)
            {
                sprintf((cs_char*)buf,"%8d",pos);
                cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0034","Blockchain substitution failed on block: ",(cs_char*)buf);      
                return err;
            }
            pos--;
        }        
    }        
    
                                                                                // Blockchain reallocation
    if((g_State->m_BlockChainHeight+1+count)*CS_DCT_BLOCK_HEAD_SIZE >= g_State->m_BlockChainAllocSize)
    {
        NewChainSize=cs_AllocSize(g_State->m_BlockChainHeight+1+count,32768,CS_DCT_BLOCK_HEAD_SIZE);
        lpNewChain=(cs_uchar*)cs_New(NewChainSize,NULL,CS_ALT_DEFAULT);
        if(lpNewChain == NULL)
        {
            return CS_ERR_ALLOCATION;
        }
    
        if(g_State->m_BlockChain)
        {
            cs_Delete(g_State->m_BlockChain,NULL,CS_ALT_DEFAULT);
        }
                
        g_State->m_BlockChainAllocSize=NewChainSize;
        g_State->m_BlockChain=lpNewChain;
    }

    if(count)
    {
        sprintf((cs_char*)buf,"%8d -> %8d",g_State->m_BlockChainHeight,g_State->m_BlockChainHeight+count);
        cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0035","Blockchain extension",(cs_char*)buf);      
    }
    
    while(count)                                                                // Adding new blocks to blockchain
    {
        g_State->m_BlockChainHeight++;

        memcpy(g_State->m_BlockChain+(g_State->m_BlockChainHeight)*CS_DCT_BLOCK_HEAD_SIZE+CS_DCT_BLOCK_HEAD_OFFSET,ptr,81);
        bitcoin_block_hash(g_State->m_BlockChain+(g_State->m_BlockChainHeight)*CS_DCT_BLOCK_HEAD_SIZE);
        
        bitcoin_hash_to_string(Out,g_State->m_BlockChain+(g_State->m_BlockChainHeight)*CS_DCT_BLOCK_HEAD_SIZE);
        
        sprintf((cs_char*)buf,"New block %8d: ",g_State->m_BlockChainHeight);
        cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0036",(cs_char*)buf,Out);      
        
        ptr+=81;
        count--;
    }    
    
    err=WriteBlockChain();
    if(err)
    {
        cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0037","Blockchain extension failed: ",(cs_char*)buf);              
    }
    
    return err;    
}


