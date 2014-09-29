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
#include "us_declare.h"
#include "args.h"
#include "state.h"
#include "bitcoin.h"

extern cs_Arguments *g_Args;
extern cs_State            *g_State;
extern cs_handle           g_Log;
extern cs_Message          *g_Message;

cs_int32 CalculateTxSize(cs_uchar *lpTx);
cs_int32 ParseBTCTransaction(cs_uchar *lpTx,cs_uchar *TxHash,cs_int32 block,cs_uchar *TxAssetMetaData,cs_int32 *TxAssetMetaDataSize,cs_int32 remove_offset,cs_int32 add_offset,cs_uint32 block_ts);
cs_int32 ParseAssetTransaction(cs_uchar *TxHash,cs_int32 block,cs_int32 offset,cs_uchar *TxAssetMetaData,cs_int32 TxAssetMetaDataSize);
cs_int32 TrackerProcessAsyncMessage(cs_int32 count);
cs_int32 WriteBlockChain();
cs_int32 WriteAssetState();

typedef struct cs_AssetStats
{
    cs_int64 m_TxCount;
    cs_int64 m_TxUnspentOutCount;
    cs_int64 m_TotalQty;
} cs_AssetStats;

/*
 * --- Removes block (or mempool) from the blockchain. All txouts spent by this block become unspent, all "new" txouts deleted
 */

cs_int32 RemoveBlock(cs_int32 mempool)
{
    cs_int32 err,size,bytes_read,asset,next_block;
    cs_char filename[CS_DCT_MAX_PATH];
    cs_char command[CS_DCT_MAX_PATH];
    cs_uchar CDBRow[CS_DCT_CDB_KEY_SIZE+CS_DCT_CDB_VALUE_SIZE];
    cs_int32 block,group;
    cs_int32 fHan;
    cs_char msg[100];
    cs_char  Out[65];    

    g_State->m_BlockInsertCount=0;
    g_State->m_BlockUpdateCount=0;
    g_State->m_BlockDeleteCount=0;
    
    err=CS_ERR_NOERROR;
    cs_LogShift(g_Log);

    if(mempool)
    {
        block=CS_DCT_MEMPOOL_BLOCK;
        cs_LogMessage(g_Log,CS_LOG_MINOR,"C-0038","Removing mempool","");            
    }
    else
    {
        block=g_State->m_BlockChainHeight;

        sprintf(msg,"%d",block);
        cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0039","Removing block",msg);            
    }
    
    group=block/CS_DCT_BLOCK_PER_FOLDER;
    
    sprintf(filename,"%s%s%c%06d",g_Args->m_DataDir,CS_DCT_FOLDER_NEW_TXOUTS,CS_DCT_FOLDERSEPARATOR,group);
    sprintf(filename,"%s%cnew_%d.dat",filename,CS_DCT_FOLDERSEPARATOR,block);

    fHan=open(filename,_O_BINARY | O_RDONLY);

    size=CS_DCT_CDB_KEY_SIZE+CS_DCT_CDB_VALUE_SIZE;
    
    if(fHan>0)
    {
        bytes_read=size;
        g_State->m_AssetDB->Lock(1);
        while(bytes_read==size)                                                 // Deleting "new" txouts
        {
            bytes_read=read(fHan,CDBRow,size);        
            if(bytes_read==size)
            {
                bitcoin_hash_to_string(Out,CDBRow);
                sprintf(msg,"%s - %d",Out,(cs_int32)cs_GetUInt64LittleEndian(CDBRow+32,4));
                cs_LogMessage(g_Log,CS_LOG_MINOR,"C-0040","Removed mempool txout",msg);            
                g_State->m_BlockDeleteCount+=1;
                err=g_State->m_AssetDB->Delete((cs_char*)CDBRow,CS_DCT_CDB_KEY_SIZE,0);
                if(err)
                {
                    g_State->m_AssetDB->UnLock();
                    return err;
                }
            }
        }
        g_State->m_AssetDB->Commit(0);        
        g_State->m_AssetDB->UnLock();
        close(fHan);
    }

    sprintf(filename,"%s%s%c%06d",g_Args->m_DataDir,CS_DCT_FOLDER_SPENT_TXOUTS,CS_DCT_FOLDERSEPARATOR,group);
    sprintf(filename,"%s%cspent_%d.dat",filename,CS_DCT_FOLDERSEPARATOR,block);
    fHan=open(filename,_O_BINARY | O_RDONLY);

    size=CS_DCT_CDB_KEY_SIZE+CS_DCT_CDB_VALUE_SIZE;
    
    if(fHan>0)
    {
        bytes_read=size;
        g_State->m_AssetDB->Lock(1);
        while(bytes_read==size)                                                 // Unspending spent txouts
        {
            bytes_read=read(fHan,CDBRow,size);        
            if(bytes_read==size)
            {
                bitcoin_hash_to_string(Out,CDBRow);
                sprintf(msg,"%s - %d",Out,(cs_int32)cs_GetUInt64LittleEndian(CDBRow+32,4));
                cs_LogMessage(g_Log,CS_LOG_MINOR,"C-0041","Unspending mempool txout",msg);            
                memset(CDBRow+CS_DCT_CDB_KEY_SIZE+12,0,4);
                g_State->m_BlockUpdateCount+=1;
                err=g_State->m_AssetDB->Write((cs_char*)CDBRow,CS_DCT_CDB_KEY_SIZE,(cs_char*)CDBRow+CS_DCT_CDB_KEY_SIZE,CS_DCT_CDB_VALUE_SIZE,0);
                if(err)
                {
                    g_State->m_AssetDB->UnLock();
                    return err;
                }
            }
        }
        g_State->m_AssetDB->Commit(0);
        g_State->m_AssetDB->UnLock();
        close(fHan);
        strcpy(command,"rm ");
        strcat(command,filename);
        __US_Shell(command);        
    }

    sprintf(filename,"%s%s%c%06d",g_Args->m_DataDir,CS_DCT_FOLDER_NEW_TXOUTS,CS_DCT_FOLDERSEPARATOR,group);
    sprintf(filename,"%s%cnew_%d.dat",filename,CS_DCT_FOLDERSEPARATOR,block);

    fHan=open(filename,_O_BINARY | O_RDONLY);

    size=CS_DCT_CDB_KEY_SIZE+CS_DCT_CDB_VALUE_SIZE;
    
    if(fHan>0)                                                                  // Trying to delete "new" txouts again. If the txout was spent in the same block (or memory pool)
                                                                                // it was not deleted in previous attempt, but inserted by Unspending loop
    {
        bytes_read=size;
        g_State->m_AssetDB->Lock(1);
        while(bytes_read==size)                                                 // Deleting "new" txouts
        {
            bytes_read=read(fHan,CDBRow,size);        
            if(bytes_read==size)
            {
                err=g_State->m_AssetDB->Delete((cs_char*)CDBRow,CS_DCT_CDB_KEY_SIZE,0);
                if(err)
                {
                    g_State->m_AssetDB->UnLock();
                    return err;
                }
            }
        }
        g_State->m_AssetDB->Commit(0);        
        g_State->m_AssetDB->UnLock();
        close(fHan);
        strcpy(command,"rm ");
        strcat(command,filename);
        __US_Shell(command);        
    }
    
    if((block-1)/CS_DCT_BLOCK_PER_FOLDER!=group)                                // If deleted block file is the last in folder - delete entire folder
    {
        sprintf(filename,"%s%s%c%06d",g_Args->m_DataDir,CS_DCT_FOLDER_SPENT_TXOUTS,CS_DCT_FOLDERSEPARATOR,group);
        strcpy(command,"rmdir ");
        strcat(command,filename);
        __US_Shell(command);                
        sprintf(filename,"%s%s%c%06d",g_Args->m_DataDir,CS_DCT_FOLDER_NEW_TXOUTS,CS_DCT_FOLDERSEPARATOR,group);
        strcpy(command,"rmdir ");
        strcat(command,filename);
        __US_Shell(command);                
    }        

    if(mempool==0)                                                              // Updating blockhain
    {
        for(asset=0;asset<g_State->m_AssetStateCount;asset++)
        {
            next_block=cs_GetUInt64LittleEndian(g_State->m_AssetState+asset*CS_DCT_ASSET_STATE_SIZE+CS_OFF_ASSET_STATE_NEXT_BLOCK,4);
            if(next_block-1==block)
            {
                next_block--;
                cs_PutNumberLittleEndian(g_State->m_AssetState+asset*CS_DCT_ASSET_STATE_SIZE+CS_OFF_ASSET_STATE_NEXT_BLOCK,&next_block,4,sizeof(next_block));
            }
        }

        err=WriteAssetState();
        if(err)
        {
            return err;
        }

        g_State->m_BlockChainHeight--;

        err=WriteBlockChain();

        if(err)
        {
            g_State->m_BlockChainHeight++;
        }
    }
    
    return err;
}

/*
 * --- Purging old block. All txouts spent by this block are deleted. Cached block files are removed.
 */

cs_int32 PurgeBlock(cs_int32 block)
{
    cs_int32 err,size,bytes_read;
    cs_char filename[CS_DCT_MAX_PATH];
    cs_char command[CS_DCT_MAX_PATH];
    cs_uchar CDBRow[CS_DCT_CDB_KEY_SIZE+CS_DCT_CDB_VALUE_SIZE];
    cs_int32 group;
    cs_int32 fHan;
    cs_char msg[100];
    
    g_State->m_BlockInsertCount=0;
    g_State->m_BlockUpdateCount=0;
    g_State->m_BlockDeleteCount=0;
    
    err=CS_ERR_NOERROR;
    cs_LogShift(g_Log);

    group=block/CS_DCT_BLOCK_PER_FOLDER;
    if(block>g_State->m_BlockChainHeight-4*CS_DCT_BLOCK_PURGE_DELAY)
    {

        sprintf(filename,"%s%s%c%06d",g_Args->m_DataDir,CS_DCT_FOLDER_SPENT_TXOUTS,CS_DCT_FOLDERSEPARATOR,group);
        sprintf(filename,"%s%cspent_%d.dat",filename,CS_DCT_FOLDERSEPARATOR,block);

        fHan=open(filename,_O_BINARY | O_RDONLY);

        size=CS_DCT_CDB_KEY_SIZE+CS_DCT_CDB_VALUE_SIZE;

        if(fHan>0)
        {
            g_State->m_AssetDB->Lock(1);
            bytes_read=size;
            while(bytes_read==size)                                                 // Deleting spent txouts
            {
                bytes_read=read(fHan,CDBRow,size);        
                if(bytes_read==size)
                {
                    g_State->m_BlockDeleteCount+=1;
                    err=g_State->m_AssetDB->Delete((cs_char*)CDBRow,CS_DCT_CDB_KEY_SIZE,0);
                    if(err)
                    {
                        g_State->m_AssetDB->UnLock();
                        return err;
                    }
                }
            }
            close(fHan);
            g_State->m_AssetDB->Commit(0);
            g_State->m_AssetDB->UnLock();
            strcpy(command,"rm ");
            strcat(command,filename);
            __US_Shell(command);        
        }

        sprintf(filename,"%s%s%c%06d",g_Args->m_DataDir,CS_DCT_FOLDER_NEW_TXOUTS,CS_DCT_FOLDERSEPARATOR,group);
        sprintf(filename,"%s%cnew_%d.dat",filename,CS_DCT_FOLDERSEPARATOR,block);
        strcpy(command,"rm ");
        strcat(command,filename);
        __US_Shell(command);        
    }
    
    if((block+1)/CS_DCT_BLOCK_PER_FOLDER!=group)
    {
        sprintf(filename,"%s%s%c%06d",g_Args->m_DataDir,CS_DCT_FOLDER_SPENT_TXOUTS,CS_DCT_FOLDERSEPARATOR,group);
        strcpy(command,"rmdir ");
        strcat(command,filename);
        __US_Shell(command);                
        sprintf(filename,"%s%s%c%06d",g_Args->m_DataDir,CS_DCT_FOLDER_NEW_TXOUTS,CS_DCT_FOLDERSEPARATOR,group);
        strcpy(command,"rmdir ");
        strcat(command,filename);
        __US_Shell(command);                
    }
    
    sprintf(msg,"%d; Deleted: %6d",block,g_State->m_BlockDeleteCount);
    cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0042","Purging block",msg);            
    
    return err;
}

/*
 * --- Storing "Add" and "Remove" buffers created by ParseTransaction functions in DB and in file cache
 */

cs_int32 StoreTXsInDB(cs_int32 block,cs_int32 txcount,cs_int32 remove_offset,cs_int32 add_offset)
{
    cs_int32 err,i,j,count,size,bytes_written;
    cs_char *ptr;
    cs_char filename[CS_DCT_MAX_PATH];
    cs_char command[CS_DCT_MAX_PATH];
    cs_int32 group;
    cs_int32 fHan;
    cs_char  Out[65];    
    
    cs_char msg[100];
    
    g_State->m_BlockInsertCount=0;
    g_State->m_BlockUpdateCount=0;
    g_State->m_BlockDeleteCount=0;
    
    cs_LogShift(g_Log);
    
    
    err=CS_ERR_NOERROR;
    
    count=g_State->m_TxOutRemove->GetCount()-remove_offset;
    
    if(count)                                                                   // Spending txouts (row is written in "spent" cache file)
    {
        if((block>g_State->m_BlockChainHeight-2*CS_DCT_BLOCK_PURGE_DELAY) && (block>10000))
        {
            group=block/CS_DCT_BLOCK_PER_FOLDER;

            sprintf(filename,"%s%s%c%06d",g_Args->m_DataDir,CS_DCT_FOLDER_SPENT_TXOUTS,CS_DCT_FOLDERSEPARATOR,group);
            strcpy(command,"mkdir ");
            strcat(command,filename);
            __US_Shell(command);

            sprintf(filename,"%s%cspent_%d.dat",filename,CS_DCT_FOLDERSEPARATOR,block);

            fHan=open(filename,_O_BINARY | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

            if(fHan<=0)
            {
                cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0044","Cannot open spent file","");      
                err=CS_ERR_FILE_NOT_FOUND;
                return err;
            }

            lseek64(fHan,0,SEEK_END);
            size=(g_State->m_TxOutRemove->GetCount()-remove_offset)*(CS_DCT_CDB_KEY_SIZE+CS_DCT_CDB_VALUE_SIZE);
            bytes_written=write(fHan,g_State->m_TxOutRemove->GetRow(remove_offset),size);
            close(fHan);
            if(size!=bytes_written)
            {
                cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0045","Cannot write spent file","");      
                err=CS_ERR_WRITE_ERROR;
                return err;            
            }        
        }
    }    
    
    count=g_State->m_TxOutAdd->GetCount()-add_offset;
    
    if(count)                                                                   // New txouts (row is written in "new" cache file)
    {
        if((block>g_State->m_BlockChainHeight-2*CS_DCT_BLOCK_PURGE_DELAY) && (block>10000))
        {
            group=block/CS_DCT_BLOCK_PER_FOLDER;

            sprintf(filename,"%s%s%c%06d",g_Args->m_DataDir,CS_DCT_FOLDER_NEW_TXOUTS,CS_DCT_FOLDERSEPARATOR,group);
            strcpy(command,"mkdir ");
            strcat(command,filename);
            __US_Shell(command);

            sprintf(filename,"%s%cnew_%d.dat",filename,CS_DCT_FOLDERSEPARATOR,block);

            fHan=open(filename,_O_BINARY | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

            if(fHan<=0)
            {
                cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0046","Cannot open spent txout file","");      
                err=CS_ERR_FILE_NOT_FOUND;
                return err;
            }

            lseek64(fHan,0,SEEK_END);
            size=(g_State->m_TxOutAdd->GetCount()-add_offset)*(CS_DCT_CDB_KEY_SIZE+CS_DCT_CDB_VALUE_SIZE);
            bytes_written=write(fHan,g_State->m_TxOutAdd->GetRow(add_offset),size);
            close(fHan);
            if(size!=bytes_written)
            {
                cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0047","Cannot write new txout file","");      
                err=CS_ERR_WRITE_ERROR;
                return err;            
            }        
        }
    }    
        
    count=g_State->m_TxOutRemove->GetCount();
    for(i=remove_offset;i<count;i++)                                            // Spending txouts (spent field in DB is set)
    {
        ptr=(cs_char*)(g_State->m_TxOutRemove->GetRow(i));
        bitcoin_hash_to_string(Out,(cs_uchar*)ptr);
        sprintf(msg,"%s - %d",Out,(cs_int32)cs_GetUInt64LittleEndian(ptr+32,4));
        cs_LogMessage(g_Log,CS_LOG_MINOR,"C-0048","Spending txout",msg);            
        if(block>g_State->m_BlockChainHeight-2*CS_DCT_BLOCK_PURGE_DELAY)
        {
            g_State->m_BlockUpdateCount+=1;
            err=g_State->m_AssetDB->Write(ptr,CS_DCT_CDB_KEY_SIZE,ptr+CS_DCT_CDB_KEY_SIZE,CS_DCT_CDB_VALUE_SIZE,0);
            if(err)
            {
                return err;
            }
        }
        else
        {
            g_State->m_BlockDeleteCount+=1;
            err=g_State->m_AssetDB->Delete(ptr,CS_DCT_CDB_KEY_SIZE,0);
            if(err)
            {
                return err;
            }            
        }
    }
//    printf("Removed %d rows from DB\n",count);
    
    count=g_State->m_TxOutAdd->GetCount();
    for(i=add_offset;i<count;i++)                                               // New txouts (new row is added to DB)
    {
        ptr=(cs_char*)(g_State->m_TxOutAdd->GetRow(i));
        bitcoin_hash_to_string(Out,(cs_uchar*)ptr);
        sprintf(msg,"%s - %d",Out,(cs_int32)cs_GetUInt64LittleEndian(ptr+32,4));
        cs_LogMessage(g_Log,CS_LOG_MINOR,"C-0049","New txout",msg);            
        g_State->m_BlockInsertCount+=1;
        err=g_State->m_AssetDB->Write(ptr,CS_DCT_CDB_KEY_SIZE,ptr+CS_DCT_CDB_KEY_SIZE,CS_DCT_CDB_VALUE_SIZE,0);
        if(err)
        {
//            printf("%08X\n",err);
            for(j=i-1;j>=add_offset;j--)
            {
                ptr=(cs_char*)(g_State->m_TxOutAdd->GetRow(j));
                g_State->m_AssetDB->Delete(ptr,CS_DCT_CDB_KEY_SIZE,0);                
            }
            return err;
        }
    }
//    printf("Added %d rows to DB\n",count);
        
    if(block<=g_State->m_BlockChainHeight-2*CS_DCT_BLOCK_PURGE_DELAY)           // If input was in the same block it was not really deleted in previous spending attempt
                                                                                // If also spent_txout file was not saved it will not be deleted in purging too
                                                                                // This loop is required for deleting thses spent inputs
    {
        count=g_State->m_TxOutRemove->GetCount();
        for(i=remove_offset;i<count;i++)                                        
        {
            ptr=(cs_char*)(g_State->m_TxOutRemove->GetRow(i));
            err=g_State->m_AssetDB->Delete(ptr,CS_DCT_CDB_KEY_SIZE,0);
            if(err)
            {
                return err;
            }            
        }
    }
    
    g_State->m_AssetDB->Commit(0);
    
    if(block == CS_DCT_MEMPOOL_BLOCK)
    {
        sprintf(msg,"MemPool - %4d txs ",txcount);
        cs_LogMessage(g_Log,CS_LOG_MINOR,"C-0043","Stored block",msg);            
    }
    else
    {
        sprintf(msg,"%8d (%d) %4d txs; Inserted: %6d, Deleted: %6d, Updated: %6d",
                block,(cs_int32)cs_GetUInt64LittleEndian(g_State->m_BlockChain+(block)*CS_DCT_BLOCK_HEAD_SIZE+CS_DCT_BLOCK_HEAD_OFFSET+68,4),txcount,
                g_State->m_BlockInsertCount,g_State->m_BlockDeleteCount,g_State->m_BlockUpdateCount);        
        cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0098","Stored block",msg);          
    }
    
    return err;
}

/*
 * --- Processing new blocks
 */

cs_int32 ProcessBlock(cs_int32 block,cs_int32 block_count)
{
    cs_int32 err,j,value,txcount,txsize,varint_size,count,asset,next_block,i,TxAssetMetaDataSize,group;
    cs_uint32 block_ts;
    cs_uchar buf[36];    
    cs_uchar *ptr;
    cs_uchar *block_start;
    cs_uchar TxHash[CS_DCT_HASH_BYTES];
    cs_uchar TxAssetMetaData[CS_DCT_ASSET_METADATA_SIZE];
    cs_char  Out[65];    
    cs_char msg[100];
    cs_char filename[CS_DCT_MAX_PATH];
    cs_char command[CS_DCT_MAX_PATH];
    cs_uchar CDBKey[CS_DCT_CDB_KEY_SIZE];
    cs_uchar CDBValue[CS_DCT_CDB_VALUE_SIZE];
    
    err=CS_ERR_NOERROR;
    cs_LogShift(g_Log);

    sprintf(msg,"%d - %d",block,block+block_count-1);
    cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0050","Processing blocks",msg);            

    g_Message->Clear();
    
    value=block_count;
    cs_PutNumberLittleEndian(buf,&value,1,sizeof(value));    
    if(!err)g_Message->PutData(buf,1);    

    value=2;
    for(i=0;i<block_count;i++)
    {
        cs_PutNumberLittleEndian(buf,&value,4,sizeof(value));    
        memcpy(buf+4,g_State->m_BlockChain+(block+i)*CS_DCT_BLOCK_HEAD_SIZE,CS_DCT_HASH_BYTES);
        if(!err)g_Message->PutData(buf,36);    
    }
    
    
    g_Message->Complete("getdata");
        
    err=TrackerProcessAsyncMessage(block_count);                                  // GetData message to bitcoin client
    
    if(err)
    {
        sprintf((cs_char*)buf,"Error: %08X",err);
        cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0051","Get block failed",(cs_char*)buf);      
        return err;
    }
    
    
    ptr=g_State->m_ResponseMessage;
    for(i=0;i<block_count;i++)
    {
        cs_LogShift(g_Log);
        if(strcmp((cs_char*)ptr+4,"block"))
        {
            sprintf((cs_char*)buf,"%8d: %s",block+i,(cs_char*)ptr+4);        
            cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0052","Non-block response for block",(cs_char*)buf);      
            return -1;        
        }
        
        if(block+i>=g_Args->m_StoreOpReturnsBlock)
        {
            group=((block+i)/CS_DCT_BLOCK_PER_FOLDER)*CS_DCT_BLOCK_PER_FOLDER;      // Preparing OP_RETURN log file
            sprintf(filename,"%s%s%c%06d",g_Args->m_DataDir,CS_DCT_FOLDER_OP_RETURN,CS_DCT_FOLDERSEPARATOR,group);
            strcpy(command,"mkdir ");
            strcat(command,filename);
            __US_Shell(command);
            sprintf(filename,"%s%c%06d.log",filename,CS_DCT_FOLDERSEPARATOR,block+i);
            strcpy(command,"rm ");
            strcat(command,filename);
            __US_Shell(command);
            strcpy(command,"touch ");
            strcat(command,filename);
            __US_Shell(command);
        }
        
        ptr+=CS_DCT_MSG_HEADERSIZE;
        block_start=ptr;
        ptr+=80;

        txcount=(cs_int32)(*ptr);
        varint_size=bitcoin_get_varint_size(*ptr);
        ptr++;
        if(varint_size)
        {
            txcount=cs_GetUInt64LittleEndian(ptr,varint_size);
            ptr+=varint_size;
        }        

        g_State->m_TxOutAdd->Clear();
        g_State->m_TxOutRemove->Clear();

        memset(CDBKey,0,CS_DCT_CDB_KEY_SIZE);
        memcpy(CDBKey,g_State->m_BlockChain+(block+i)*CS_DCT_BLOCK_HEAD_SIZE,CS_DCT_HASH_BYTES);
        
        memset(CDBValue,0,CS_DCT_CDB_VALUE_SIZE);
        
        j=CS_DCT_CDB_BLOCK_BY_HASH;
        cs_PutNumberLittleEndian(CDBValue,&j,4,sizeof(j));
        j=block+i;
        cs_PutNumberLittleEndian(CDBValue+4,&j,4,sizeof(j));
        j=cs_GetUInt64LittleEndian(g_State->m_BlockChain+(block+i)*CS_DCT_BLOCK_HEAD_SIZE+CS_DCT_BLOCK_HEAD_OFFSET+68,4);//Timestamp        
        cs_PutNumberLittleEndian(CDBValue+8,&j,4,sizeof(j));
        block_ts=(cs_uint32)j;        
        j=txcount;
        cs_PutNumberLittleEndian(CDBValue+12,&j,4,sizeof(j));

        err=g_State->m_TxOutAdd->Add(CDBKey,CDBValue);                          // Special Block header record
        if(err)
        {
            return err;
        }        
        

        j=block+i;
        memset(TxHash,0,CS_DCT_HASH_BYTES);                           
        cs_PutNumberLittleEndian(TxHash,&j,4,sizeof(j));
        
        memset(CDBKey,0,CS_DCT_CDB_KEY_SIZE);                           
        bitcoin_tx_hash(CDBKey,TxHash,CS_DCT_HASH_BYTES);                       // Block ID is hashed to special TxID

        bitcoin_hash_to_string(Out,TxHash);
        
        memset(CDBValue,0,CS_DCT_CDB_VALUE_SIZE);
        j=CS_DCT_CDB_BLOCK_BY_INDEX;
        cs_PutNumberLittleEndian(CDBValue,&j,4,sizeof(j));
        for(j=0;j<3;j++)
        {
            cs_PutNumberLittleEndian(CDBKey+CS_DCT_HASH_BYTES+12,&j,4,sizeof(j));
            memcpy(CDBValue+4,g_State->m_BlockChain+(block+i)*CS_DCT_BLOCK_HEAD_SIZE+j*12,12);
            err=g_State->m_TxOutAdd->Add(CDBKey,CDBValue);                      // Special index->hash map divided into 3 12-byte chunks
            if(err)
            {
                return err;
            }        
        }

        memset(CDBKey,0,CS_DCT_CDB_KEY_SIZE);    
        strcpy((cs_char*)TxHash,"lastblock");
        bitcoin_tx_hash(CDBKey,TxHash,strlen((cs_char*)TxHash));          
        
        memset(CDBValue,0,CS_DCT_CDB_VALUE_SIZE);
        j=CS_DCT_CDB_LAST_BLOCK;
        cs_PutNumberLittleEndian(CDBValue,&j,4,sizeof(j));
        j=block+i;
        cs_PutNumberLittleEndian(CDBValue+4,&j,4,sizeof(j));
        j=cs_GetUInt64LittleEndian(g_State->m_BlockChain+(block+i)*CS_DCT_BLOCK_HEAD_SIZE+CS_DCT_BLOCK_HEAD_OFFSET+68,4);//Timestamp        
        cs_PutNumberLittleEndian(CDBValue+8,&j,4,sizeof(j));
        j=(cs_int32)cs_TimeNow();
        cs_PutNumberLittleEndian(CDBValue+12,&j,4,sizeof(j));
        
        err=g_State->m_TxOutAdd->Add(CDBKey,CDBValue);                          // Special record containing info about last processed block
        if(err)
        {
            return err;
        }        
        
        
        count=0;
        while(count<txcount)                                                    // Parsing transactions
        {
            cs_LogShift(g_Log);
            txsize=CalculateTxSize(ptr);

            bitcoin_tx_hash(TxHash,ptr,txsize);

            bitcoin_hash_to_string(Out,TxHash);
            
            g_State->m_AssetDB->Lock(0);
            err=ParseBTCTransaction(ptr,TxHash,block+i,TxAssetMetaData,&TxAssetMetaDataSize,0,0,block_ts);
            g_State->m_AssetDB->UnLock();
            if(err)
            {
                cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0053","Orphan transaction",Out);            
                return err;
            }
            g_State->m_AssetDB->Lock(0);
            err=ParseAssetTransaction(TxHash,block+i,ptr-block_start,TxAssetMetaData,TxAssetMetaDataSize);
            g_State->m_AssetDB->UnLock();
            if(err)
            {
                cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0054","Asset error on tx",Out);            
                return err;
            }
            cs_LogMessage(g_Log,CS_LOG_MINOR,"C-0055","Accepted transaction",Out);            

            ptr+=txsize;
            count++;
        }        

        g_State->m_AssetDB->Lock(1);
        err=StoreTXsInDB(block+i,txcount,0,0);                                  // Storing block in database
        g_State->m_AssetDB->UnLock();
        
        g_State->m_TxOutAdd->Clear();
        g_State->m_TxOutRemove->Clear();
        
        if(err)
        {
            return err;
        }

        for(asset=0;asset<g_State->m_AssetStateCount;asset++)                   // Updating blockchain state
        {
            next_block=cs_GetUInt64LittleEndian(g_State->m_AssetState+asset*CS_DCT_ASSET_STATE_SIZE+CS_OFF_ASSET_STATE_NEXT_BLOCK,4);
            if(next_block==block+i)
            {
                next_block++;
                cs_PutNumberLittleEndian(g_State->m_AssetState+asset*CS_DCT_ASSET_STATE_SIZE+CS_OFF_ASSET_STATE_NEXT_BLOCK,&next_block,4,sizeof(next_block));
            }
        }

        err=WriteAssetState();
        if(err)
        {
            return err;
        }

    }
    return err;
}

/*
 * --- Processing mempool. Memory pool is considered is considered as separate block removed before processing any block (including mempool itself)
 */

cs_int32 ProcessMemPool(cs_int32 new_op_return_file)
{
    cs_int32 err,value,txcount,txsize,varint_size,count,block,dbvalue_len,i,remove_offset,add_offset,acc_count,total,TxAssetMetaDataSize,txfrom,txchunk,txpos,group;
    cs_uint32 block_ts;
    cs_uchar buf[36];    
    cs_uchar *ptr;
    cs_uchar *inv_ptr;
    cs_uchar TxHash[CS_DCT_HASH_BYTES];
    cs_uchar TxAssetMetaData[CS_DCT_ASSET_METADATA_SIZE];
    cs_char  Out[65];    
    cs_char msg[100];
    cs_char *lpRequiredTxs;
    cs_uchar *lpInvData;
    cs_uchar CDBKey[CS_DCT_CDB_KEY_SIZE];
    void *dbvalue;
    cs_char filename[CS_DCT_MAX_PATH];
    cs_char command[CS_DCT_MAX_PATH];
    
    err=CS_ERR_NOERROR;

    cs_LogShift(g_Log);

    g_Message->Clear();
        
    g_Message->Complete("mempool");
    
    err=TrackerProcessAsyncMessage(1);                                            // mempool message to bitcoin client
    
    if(err)
    {
        sprintf((cs_char*)buf,"Error: %08X",err);
        cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0057","Get mempool failed",(cs_char*)buf);      
        return err;
    }

    ptr=g_State->m_ResponseMessage;
        
    ptr+=CS_DCT_MSG_HEADERSIZE;

    txcount=(cs_int32)(*ptr);
    varint_size=bitcoin_get_varint_size(*ptr);
    ptr++;
    if(varint_size)
    {
        txcount=cs_GetUInt64LittleEndian(ptr,varint_size);
        ptr+=varint_size;
    }        

    if(txcount==0)
    {
        return err;
    }
    
    total=txcount;
    
    lpRequiredTxs=(cs_char*)cs_New(txcount,NULL,CS_ALT_DEFAULT);
    if(lpRequiredTxs == NULL)
    {
        err=CS_ERR_ALLOCATION;
        return err;
    }
        
    lpInvData=(cs_uchar*)cs_New(txcount*(4+CS_DCT_HASH_BYTES),NULL,CS_ALT_DEFAULT);
    if(lpInvData == NULL)
    {
        err=CS_ERR_ALLOCATION;
        return err;
    }

    memcpy(lpInvData,ptr,txcount*(4+CS_DCT_HASH_BYTES));
    
    g_State->m_AssetDB->Lock(0);

    count=0;
    for(i=0;i<txcount;i++)
    {
        lpRequiredTxs[i]=1;
        value=cs_GetUInt64LittleEndian(ptr, 4);
        if(value!=1)
        {
            lpRequiredTxs[i]=0;
        }   
        ptr+=4;
        memcpy(CDBKey,ptr,CS_DCT_HASH_BYTES);
        ptr+=CS_DCT_HASH_BYTES;
        value=0;
        cs_PutNumberLittleEndian(CDBKey+CS_DCT_HASH_BYTES,&value,4,sizeof(value));
        memcpy(CDBKey+CS_DCT_HASH_BYTES+4,g_State->m_AssetState+CS_OFF_ASSET_STATE_ID,CS_DCT_ASSET_ID_SIZE);
        
        if(lpRequiredTxs[i])                                                    // Checking that transaction is not in DB yet. Only BTC row is checked
        {
            dbvalue=g_State->m_AssetDB->Read((cs_char*)CDBKey,CS_DCT_CDB_KEY_SIZE,&dbvalue_len,0,&err);
            if(err)
            {
                goto exitlbl;
            }
            if(dbvalue)
            {
                lpRequiredTxs[i]=0;
            }
        }        
        
        if(lpRequiredTxs[i])
        {
            count++;
        }
    }
    g_State->m_AssetDB->UnLock();

    
    if(count==0)
    {
        goto exitlbl;
    }
    
    txcount=count;
    
    remove_offset=g_State->m_TxOutRemove->GetCount();
    add_offset=g_State->m_TxOutAdd->GetCount();
    
    block=CS_DCT_MEMPOOL_BLOCK;
    if(block>=g_Args->m_StoreOpReturnsBlock)
    {
        if(new_op_return_file)
        {
            group=(block/CS_DCT_BLOCK_PER_FOLDER)*CS_DCT_BLOCK_PER_FOLDER;      // Preparing OP_RETURN log file
            sprintf(filename,"%s%s%c%06d",g_Args->m_DataDir,CS_DCT_FOLDER_OP_RETURN,CS_DCT_FOLDERSEPARATOR,group);
            strcpy(command,"mkdir ");
            strcat(command,filename);
            __US_Shell(command);
            sprintf(filename,"%s%c%06d.log",filename,CS_DCT_FOLDERSEPARATOR,block);
            strcpy(command,"rm ");
            strcat(command,filename);
            __US_Shell(command);
            strcpy(command,"touch ");
            strcat(command,filename);
            __US_Shell(command);
        }
    }
    block_ts=(cs_int32)cs_TimeNow();
    
    acc_count=0;
    txfrom=0;
    inv_ptr=lpInvData;
    i=0;
    while(txfrom<count)
    {
        txchunk=count-txfrom;
        if(txchunk>CS_DCT_TXS_CHUNK)
        {
            txchunk=CS_DCT_TXS_CHUNK;
        }
        
        g_Message->Clear();

        varint_size=bitcoin_put_varint(buf,txchunk);
    
        if(!err)g_Message->PutData(buf,varint_size);    

        txpos=0;
        value=1;
        while(txpos<txchunk)
        {
            if(lpRequiredTxs[i])
            {
                g_Message->PutData(inv_ptr,36);                    
                txpos++;
            }
            inv_ptr+=36;
            i++;
        }

        txfrom+=txchunk;
        
        g_Message->Complete("getdata");
    
        err=TrackerProcessAsyncMessage(txchunk);                                  // GetData message to bitcoin client
    
        if(err)
        {
            sprintf((cs_char*)buf,"Error: %08X",err);
            cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0058","Get tx failed",(cs_char*)buf);      
            return err;
        }
        
        ptr=g_State->m_ResponseMessage;

        txpos=0;
        while(txpos<txchunk)
        {
            if(strcmp((cs_char*)ptr+4,"tx"))
            {
                txpos=txchunk;
            }
            else
            {
                ptr+=CS_DCT_MSG_HEADERSIZE;

                cs_LogShift(g_Log);
                txsize=CalculateTxSize(ptr);

                bitcoin_tx_hash(TxHash,ptr,txsize);

                bitcoin_hash_to_string(Out,TxHash);

                g_State->m_AssetDB->Lock(0);
                err=ParseBTCTransaction(ptr,TxHash,block,TxAssetMetaData,&TxAssetMetaDataSize,remove_offset,add_offset,block_ts);
                g_State->m_AssetDB->UnLock();
                if(err)
                {
                    cs_LogMessage(g_Log,CS_LOG_MINOR,"C-0059","Orphan or seen transaction",Out);            
                    err=CS_ERR_NOERROR;                                             // Orphan transactions are ignored
                }
                else
                {
                    acc_count++;
                    cs_LogMessage(g_Log,CS_LOG_MINOR,"C-0060","Accepted transaction",Out);            
                    g_State->m_AssetDB->Lock(0);
//                    err=ParseAssetTransaction(TxHash,block,0,TxAssetMetaData,TxAssetMetaDataSize);
                    ParseAssetTransaction(TxHash,block,0,TxAssetMetaData,TxAssetMetaDataSize);
                    g_State->m_AssetDB->UnLock();
                }
                ptr+=txsize;
                txpos++;        
            }
        }
    }
    
    if(acc_count)
    {
        g_State->m_AssetDB->Lock(1);
        err=StoreTXsInDB(block,acc_count,remove_offset,add_offset);             // Storing memory pool block in database
        g_State->m_AssetDB->UnLock();
        if(err)
        {
            return err;
        }
        sprintf((cs_char*)msg,"Accepted: %d txs, Required: %d txs, Total: %d txs",acc_count,txcount,total);
        cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0061","Mempool update successful",(cs_char*)msg);      
    }
    
    
    g_State->m_TxOutRemove->Clear();
    g_State->m_TxOutAdd->Clear();
    
exitlbl:

    cs_Delete(lpInvData,NULL,CS_ALT_DEFAULT);
    cs_Delete(lpRequiredTxs,NULL,CS_ALT_DEFAULT);
    
    return err;
}



