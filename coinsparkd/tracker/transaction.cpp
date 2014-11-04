/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#include <linux/fs.h>

#include "declare.h"
#include "db.h"
#include "args.h"
#include "state.h"
#include "bitcoin.h"
#include "coinspark.h"

extern cs_Arguments *g_Args;
extern cs_State            *g_State;
extern cs_handle           g_Log;


cs_int32 CalculateTxSize(cs_uchar *lpTx)
{
    cs_int32 varint_size,count,i,size;
    cs_uchar *ptr;    
    
    ptr=lpTx;

    ptr+=4;

    count=(cs_int32)(*ptr);
    varint_size=bitcoin_get_varint_size(*ptr);
    ptr++;
    if(varint_size)
    {
        count=cs_GetUInt64LittleEndian(ptr,varint_size);
        ptr+=varint_size;
    }        

    for(i=0;i<count;i++)
    {
        ptr+=36;
        size=(cs_int32)(*ptr);
        varint_size=bitcoin_get_varint_size(*ptr);
        ptr++;
        if(varint_size)
        {
            size=cs_GetUInt64LittleEndian(ptr,varint_size);
            ptr+=varint_size;
        }        
        ptr+=size;
        ptr+=4;
    }
    
    count=(cs_int32)(*ptr);
    varint_size=bitcoin_get_varint_size(*ptr);
    ptr++;
    if(varint_size)
    {
        count=cs_GetUInt64LittleEndian(ptr,varint_size);
        ptr+=varint_size;
    }        

    for(i=0;i<count;i++)
    {
        ptr+=8;
        size=(cs_int32)(*ptr);
        varint_size=bitcoin_get_varint_size(*ptr);
        ptr++;
        if(varint_size)
        {
            size=cs_GetUInt64LittleEndian(ptr,varint_size);
            ptr+=varint_size;
        }        
        ptr+=size;
    }    
    
    ptr+=4;

    return ptr-lpTx;
}

/*
 * --- Processing BTC asset, preparing asset list, extracting metadata
 */

cs_int32 ParseBTCTransaction(cs_uchar *lpTx,cs_uchar *TxHash,cs_int32 block,cs_int32 offset,cs_uchar *TxAssetMetaData,cs_int32 *TxAssetMetaDataSize,cs_int32 remove_offset,cs_int32 add_offset,cs_uint32 block_ts)
{
    cs_int32 err,varint_size,count,i,size,sum,j,dbvalue_len,spent,remove_count,row,group,msize,asset_count,read_mode,same_block,off,found_in_mempool;
    cs_uint32 ts;
    cs_uchar *ptr;    
    cs_int32 op_return_pos;    
    cs_uchar *dptr;    
    cs_uchar CDBKey[CS_DCT_CDB_KEY_SIZE];
    cs_uchar CDBValue[CS_DCT_CDB_VALUE_SIZE];
    cs_char  CDBNullAsset[CS_DCT_ASSET_ID_SIZE];   
    void *dbvalue;
    cs_char msg[200];
    cs_char op_return_msg[400];
    CoinSparkAssetQty cQty;
    CoinSparkSatoshiQty cSatoshi;
    bool cRegular;
    cs_int64 vSatoshi;
    cs_char filename[CS_DCT_MAX_PATH];
//    FILE *fOut;        
    cs_int32 op_return_file;
    
    err=CS_ERR_NOERROR;
    
    *TxAssetMetaDataSize=0;
    
    g_State->m_TxAssetMatrix->Clear();
    g_State->m_TxAssetList->Clear();
    g_State->m_TxOutputSatoshis->Clear();
    g_State->m_TxOutputRegulars->Clear();
    
    memset(CDBNullAsset,0,CS_DCT_ASSET_ID_SIZE);
    
    read_mode=0;
    if(g_Args->m_TrackerMode == CS_DCT_TRACKER_MODE_FULL)
    {
        read_mode=CS_OPT_DB_DATABASE_SEEK_ON_READ;
    }

    
    ptr=lpTx;

    ptr+=4;

    count=(cs_int32)(*ptr);
    varint_size=bitcoin_get_varint_size(*ptr);
    ptr++;
    if(varint_size)
    {
        count=cs_GetUInt64LittleEndian(ptr,varint_size);
        ptr+=varint_size;
    }        
    
    g_State->m_TxInputCount=count;
    g_State->m_TxSatoshiFee=0;
    
    remove_count=g_State->m_TxOutRemove->GetCount();
    for(i=0;i<count;i++)                                                        // Inputs loop
    {
        memcpy(CDBKey,ptr,CS_DCT_HASH_BYTES);
        ptr+=CS_DCT_HASH_BYTES;
        memcpy(CDBKey+CS_DCT_HASH_BYTES,ptr,4);
        ptr+=4;
        sum=0;for(j=0;j<CS_DCT_HASH_BYTES;j++)sum+=CDBKey[j];
        
                                                                                // Checking inputs exist and not spent
        if(sum)                                                                 // Skipping inputs of newly created coins transactions
        {
            memcpy(CDBKey+CS_DCT_HASH_BYTES+4,g_State->m_AssetState+CS_OFF_ASSET_STATE_ID,CS_DCT_ASSET_ID_SIZE);
            memset(CDBValue,0,CS_DCT_CDB_VALUE_SIZE);
                                                                                // Looking for BTC UTXO
            dbvalue=g_State->m_AssetDB->Read((cs_char*)CDBKey,CS_DCT_CDB_KEY_SIZE,&dbvalue_len,read_mode,&err);
            if(err)
            {
                return err;
            }
            
            same_block=0;
            if(dbvalue==NULL)                                                   // If not found - check previous transactions of this block
            {
                for(j=add_offset;j<g_State->m_TxOutAdd->GetCount();j++)
                {
                    dptr=g_State->m_TxOutAdd->GetRow(j);
                    if(memcmp(CDBKey,dptr,CS_DCT_CDB_KEY_SIZE) == 0)
                    {
                        same_block=j+1;
                        dbvalue=dptr+CS_DCT_CDB_KEY_SIZE;
                        dbvalue_len=CS_DCT_CDB_VALUE_SIZE;
                    }
                }
            }
            else                                                                // If found - check that it is not spent in previous txs of this block
            {
                for(j=remove_offset;j<remove_count;j++)
                {
                    dptr=g_State->m_TxOutRemove->GetRow(j);
                    if(memcmp(CDBKey,dptr,CS_DCT_CDB_KEY_SIZE) == 0)
                    {
                        dbvalue=NULL;
                    }
                }                
            }
            
            if(dbvalue==NULL)                                                   // Cannot find BTX UTXO
            {
                bitcoin_hash_to_string(msg,CDBKey);
                sprintf(msg+64,"-%08X-%08X-%08X-%08X",
                        (cs_int32)cs_GetUInt64LittleEndian(CDBKey+CS_DCT_HASH_BYTES+0,4),
                        (cs_int32)cs_GetUInt64LittleEndian(CDBKey+CS_DCT_HASH_BYTES+4,4),
                        (cs_int32)cs_GetUInt64LittleEndian(CDBKey+CS_DCT_HASH_BYTES+8,4),
                        (cs_int32)cs_GetUInt64LittleEndian(CDBKey+CS_DCT_HASH_BYTES+12,4)
                        );
                g_State->m_TxOutRemove->SetCount(remove_count);
                if(block != CS_DCT_MEMPOOL_BLOCK)
                {
                    cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0062","Cannot find transaction output",msg);            
                }
                else
                {
                    cs_LogMessage(g_Log,CS_LOG_MINOR,"C-0102","Cannot find transaction output",msg);                                
                }
                err=CS_ERR_KEY_NOT_FOUND;
                return err;
            }
            
            g_State->m_TxSatoshiFee+=cs_GetUInt64LittleEndian(dbvalue,8);
                    
            asset_count=0;
            while(dbvalue)                                                      // UTXOC loop
            {          
                                                                                // NULL asset is used for storing TX- asset genesis map and should be ignored                                                                               
                                                                                // Last 4 bytes are used for storing multiple records for single short key
                                                                                // in special records like Asset genesis metadata
                if(memcmp(CDBKey+CS_DCT_HASH_BYTES+4,CDBNullAsset,CS_DCT_ASSET_ID_SIZE-4))
                {
                                                                                // Creating tx asset list
                    row=g_State->m_TxAssetList->Seek(CDBKey+CS_DCT_HASH_BYTES+4);
                    if(row<0)
                    {
                        g_State->m_TxAssetList->Add(CDBKey+CS_DCT_HASH_BYTES+4,NULL);
                        row=g_State->m_TxAssetList->GetCount()-1;
                        cQty=0;
                        for(j=0;j<count;j++)
                        {
                            g_State->m_TxAssetMatrix->Add(NULL,&cQty);          // Initializing qty array for new asset
                        }
                    }

                    cQty=(CoinSparkAssetQty)cs_GetUInt64LittleEndian(dbvalue,8);
                    g_State->m_TxAssetMatrix->PutRow(row*count+i,NULL,&cQty);   // Updating asset qty matrix

                    memcpy(CDBValue,dbvalue,CS_DCT_CDB_VALUE_SIZE);
                    spent=cs_GetUInt64LittleEndian(CDBValue+12,4);
                    if((spent!=0) && (spent!=block))                            // Checking UTXOC is not spent
                    {
                        bitcoin_hash_to_string(msg,CDBKey);
                        sprintf(msg+64,"-%08X-%08X-%08X-%08X",
                                (cs_int32)cs_GetUInt64LittleEndian(CDBKey+CS_DCT_HASH_BYTES+0,4),
                                (cs_int32)cs_GetUInt64LittleEndian(CDBKey+CS_DCT_HASH_BYTES+4,4),
                                (cs_int32)cs_GetUInt64LittleEndian(CDBKey+CS_DCT_HASH_BYTES+8,4),
                                (cs_int32)cs_GetUInt64LittleEndian(CDBKey+CS_DCT_HASH_BYTES+12,4)
                                );
                        g_State->m_TxOutRemove->SetCount(remove_count);
                        if(block != CS_DCT_MEMPOOL_BLOCK)
                        {
                            cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0063","Transaction output spent",msg);            
                        }
                        else
                        {
                            cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0103","Transaction output spent",msg);                                
                        }
                        
                        err=CS_ERR_ASSET_SPENT;
                        return err;
                    }
                    
                    cs_PutNumberLittleEndian(CDBValue+12,&block,4,sizeof(block));
                    err=g_State->m_TxOutRemove->Add(CDBKey,CDBValue);           // Storing input in "Remove" buffer, DB will be updated in StoreTXsInDB
                    if(err)
                    {
                        g_State->m_TxOutRemove->SetCount(remove_count);
                        return err;
                    }
                }
                
                dbvalue=NULL;
                if(g_Args->m_TrackerMode == CS_DCT_TRACKER_MODE_FULL)
                {       
                    if(same_block<=0)
                    {
                        dptr=(cs_uchar*)g_State->m_AssetDB->MoveNext(&err);     // Looking for next UTXOC, BTC asset is always first                        
                        if(err)
                        {
                            return err;
                        }
                    }
                    else
                    {
                        dptr=NULL;
                        while((same_block<g_State->m_TxOutAdd->GetCount()) && (dptr==NULL))
                        {
                            dptr=g_State->m_TxOutAdd->GetRow(same_block);
                            if(memcmp(dptr,CDBKey,CS_DCT_HASH_BYTES) == 0)      // Same transaction
                            {
                                if(memcmp(dptr,CDBKey,CS_DCT_HASH_BYTES+4))     // Outputs may be unordered
                                {
                                    dptr=NULL;
                                }
                                same_block++;
                            }
                            else
                            {
                                dptr=NULL;
                                same_block=g_State->m_TxOutAdd->GetCount();
                            }                            
                        }
                    }
                    
                    if(dptr)
                    {
                        if(memcmp(dptr,CDBKey,CS_DCT_HASH_BYTES+4) == 0)        // Checking UTXOC belongs to the same txout
                        {
                            if(memcmp(dptr+CS_DCT_HASH_BYTES+4,CDBNullAsset,CS_DCT_ASSET_ID_SIZE-4))
                            {
                                bitcoin_hash_to_string(msg,dptr);
/*                                
                                sprintf(msg+64,"-%08X-%08X-%08X-%08X",
                                        (cs_int32)cs_GetUInt64LittleEndian(dptr+CS_DCT_HASH_BYTES+0,4),
                                        (cs_int32)cs_GetUInt64LittleEndian(dptr+CS_DCT_HASH_BYTES+4,4),
                                        (cs_int32)cs_GetUInt64LittleEndian(dptr+CS_DCT_HASH_BYTES+8,4),
                                        (cs_int32)cs_GetUInt64LittleEndian(dptr+CS_DCT_HASH_BYTES+12,4)
                                        );
 */ 
                                sprintf(msg+64,"-%d-%d %d-%d-%d %ld",
                                        (cs_int32)cs_GetUInt64LittleEndian(dptr+CS_DCT_HASH_BYTES+0,4),
                                        (cs_int32)cs_GetUInt64LittleEndian(dptr+CS_DCT_CDB_KEY_SIZE+8,4),
                                        (cs_int32)cs_GetUInt64LittleEndian(dptr+CS_DCT_HASH_BYTES+4,4),
                                        (cs_int32)cs_GetUInt64LittleEndian(dptr+CS_DCT_HASH_BYTES+8,4),
                                        (cs_int32)cs_GetUInt64LittleEndian(dptr+CS_DCT_HASH_BYTES+12,4),
                                        (long int)cs_GetUInt64LittleEndian(dptr+CS_DCT_CDB_KEY_SIZE,8)
                                        );
                                if(block != CS_DCT_MEMPOOL_BLOCK)
                                {
                                    cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0095","INPUT:",msg);            
                                }
//                                bitcoin_hash_to_string(msg,TxHash);
//                                cs_LogMessage(g_Log,CS_LOG_DEBUG,"","In transaction:",msg);                                            
                            }                            
                            memcpy(CDBKey+CS_DCT_HASH_BYTES+4,dptr+CS_DCT_HASH_BYTES+4,CS_DCT_ASSET_ID_SIZE);
                            dbvalue=dptr+CS_DCT_CDB_KEY_SIZE;
                        }
                    }                
                }
                else
                {
                    dbvalue=NULL;
                    if(asset_count==0)
                    {
                        asset_count=1;
                        memset(CDBKey+CS_DCT_HASH_BYTES+4,0,CS_DCT_ASSET_ID_SIZE);
                        memcpy(CDBKey+CS_DCT_HASH_BYTES+4,g_Args->m_TrackedAsset,CS_DCT_ASSET_ID_SIZE-4);
                        memset(CDBValue,0,CS_DCT_CDB_VALUE_SIZE);
                                                                                // Looking for UTXOC
                                                                                // MoveNext is not supported in one-asset mode
                        
                        if(same_block<=0)
                        {
                            dbvalue=g_State->m_AssetDB->Read((cs_char*)CDBKey,CS_DCT_CDB_KEY_SIZE,&dbvalue_len,read_mode,&err);
                            if(err)
                            {
                                return err;
                            }
                            
                        }
                        else
                        {
                            for(j=same_block;j<g_State->m_TxOutAdd->GetCount();j++)
                            {
                                dptr=g_State->m_TxOutAdd->GetRow(j);
                                if(memcmp(CDBKey,dptr,CS_DCT_CDB_KEY_SIZE) == 0)
                                {
                                    same_block=j+1;
                                    dbvalue=dptr+CS_DCT_CDB_KEY_SIZE;
                                    dbvalue_len=CS_DCT_CDB_VALUE_SIZE;
                                }
                            }                            
                        }
                        if(dbvalue)
                        {
                            cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0094","Tracked asset found in inputs","");                                                                               
                        }
                    }
                }
            }                        
        }
        
        size=(cs_int32)(*ptr);
        varint_size=bitcoin_get_varint_size(*ptr);
        ptr++;
        if(varint_size)
        {
            size=cs_GetUInt64LittleEndian(ptr,varint_size);
            ptr+=varint_size;
        }        
        ptr+=size;
        ptr+=4;
    }
    
    g_State->m_TxAssetCount=g_State->m_TxAssetList->GetCount();
    
    count=(cs_int32)(*ptr);
    varint_size=bitcoin_get_varint_size(*ptr);
    ptr++;
    if(varint_size)
    {
        count=cs_GetUInt64LittleEndian(ptr,varint_size);
        ptr+=varint_size;
    }        
    
    g_State->m_TxOutputCount=count;

    cQty=0;
    for(i=0;i<count*g_State->m_TxAssetCount;i++)
    {
        g_State->m_TxAssetMatrix->Add(NULL,&cQty);        
    }
        
    row=g_State->m_TxInputCount*g_State->m_TxAssetCount;
    g_State->m_SkipMemPoolLogs=0;
    
    for(i=0;i<count;i++)                                                        // Outputs loop
    {
        memcpy(CDBKey,TxHash,CS_DCT_HASH_BYTES);
        cs_PutNumberLittleEndian(CDBKey+CS_DCT_HASH_BYTES,&i,4,sizeof(i));
        memcpy(CDBKey+CS_DCT_HASH_BYTES+4,g_State->m_AssetState+CS_OFF_ASSET_STATE_ID,CS_DCT_ASSET_ID_SIZE);
        memset(CDBValue,0,CS_DCT_CDB_VALUE_SIZE);
        memcpy(CDBValue,ptr,8);
        
        cs_PutNumberLittleEndian(CDBValue+8,&block,4,sizeof(block));
        err=g_State->m_TxOutAdd->Add(CDBKey,CDBValue);                          // Adding BTC to "Add" buffer, will be stored in DB in StoreTXsInDB
        if(err)
        {
            return err;
        }
        
        vSatoshi=cs_GetUInt64LittleEndian(ptr,8);
        g_State->m_TxSatoshiFee-=vSatoshi;
        cSatoshi=(CoinSparkSatoshiQty)vSatoshi;
        g_State->m_TxOutputSatoshis->Add(NULL,&cSatoshi);
        
        cQty=(CoinSparkAssetQty)vSatoshi;        
        g_State->m_TxAssetMatrix->PutRow(row,NULL,&cQty);
        row++;
        
        ptr+=8;
        size=(cs_int32)(*ptr);
        varint_size=bitcoin_get_varint_size(*ptr);
        ptr++;
        if(varint_size)
        {
            size=cs_GetUInt64LittleEndian(ptr,varint_size);
            ptr+=varint_size;            
        }        
        
        found_in_mempool=0;
        ts=block_ts;
        
        if(*TxAssetMetaDataSize == 0)                                           // Checking that output contains metadata
        {
            msize=CoinSparkScriptToMetadata((cs_char*)ptr, size, 0, (cs_char*)TxAssetMetaData, size);

//            msize=CoCoGetOutputMetaData((cs_char*)ptr, (cs_char*)TxAssetMetaData, size);
            if(msize)
            {
                *TxAssetMetaDataSize=msize;
                sprintf(msg,"%2d bytes: ",*TxAssetMetaDataSize);
                for(j=0;j<*TxAssetMetaDataSize;j++)
                {
                    sprintf(msg+10+2*j,"%02x",TxAssetMetaData[j]);
                }
                if(block != CS_DCT_MEMPOOL_BLOCK)
                {
                    cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0064","Valid metadata found in block tx: ",msg);                                                        
                }
                else
                {
                    for(j=0;j<g_State->m_MemPoolOpReturns->GetCount();j++)
                    {
                        if(memcmp(TxHash,g_State->m_MemPoolOpReturns->GetRow(j),CS_DCT_HASH_BYTES) == 0)
                        {
                            found_in_mempool=1;
                            g_State->m_SkipMemPoolLogs=1;
                        }                        
                    }
                    
                    if(!found_in_mempool)
                    {
                        for(j=0;j<g_State->m_MemPoolOpReturnsLast->GetCount();j++)
                        {
                            if(memcmp(TxHash,g_State->m_MemPoolOpReturnsLast->GetRow(j),CS_DCT_HASH_BYTES) == 0)
                            {
                                ts=*(cs_uint32*)(g_State->m_MemPoolOpReturnsLast->GetRow(j)+CS_DCT_HASH_BYTES);
                            }                        
                        }                    
                        g_State->m_MemPoolOpReturns->Add(TxHash,&ts);
                        cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0090","Valid metadata found in mempool tx: ",msg);                                                        
                    }
                }
            }
        }
        
        cRegular=CoinSparkScriptIsRegular((cs_char*)ptr,size,0);                  // Checking output is valid for asset transaction
//        cRegular=CoCoIsOutputRegular((cs_char*)ptr,size);                       
        g_State->m_TxOutputRegulars->Add(NULL,&cRegular);
        
        op_return_pos=bitcoin_find_op_return(ptr,size);                         // Storing and logging OP_RETURN outputs
        if(op_return_pos<size)
        {
            if(block>=g_Args->m_StoreOpReturnsBlock)
            {
                if(!found_in_mempool || (block != CS_DCT_MEMPOOL_BLOCK))
                {
                    group=(block/CS_DCT_BLOCK_PER_FOLDER)*CS_DCT_BLOCK_PER_FOLDER;
                    sprintf(filename,"%s%s%c%06d",g_Args->m_DataDir,CS_DCT_FOLDER_OP_RETURN,CS_DCT_FOLDERSEPARATOR,group);
                    sprintf(filename,"%s%c%06d.log",filename,CS_DCT_FOLDERSEPARATOR,block);
    //                fOut=fopen(filename,"a");
                    bitcoin_hash_to_string(msg,CDBKey);
                    off=0;
                    sprintf(op_return_msg+off,"%s\t%d\t",msg,i);
                    off=strlen(op_return_msg);
                    for(j=0;j<size;j++)
                    {
                        if(j<CS_DCT_ASSET_METADATA_SIZE+1)
                        {
                            sprintf(op_return_msg+off,"%02x",ptr[j]);
                            off+=2;
                        }
                    }
                    sprintf(op_return_msg+off,"\t%d\t%d\n",ts,offset);
                    op_return_file=open(filename,O_RDWR | O_CREAT,S_IRUSR | S_IWUSR);
                    if(op_return_file)
                    {
                        flock(op_return_file,LOCK_EX);
                        lseek64(op_return_file,0,SEEK_END);
                        (void)(write(op_return_file,op_return_msg,strlen(op_return_msg))+1);
                        flock(op_return_file,LOCK_UN);
                        close(op_return_file);
                    }
    //                fclose(fOut);
                }                
            }
            
            bitcoin_hash_to_string(msg,TxHash);
            if(block != CS_DCT_MEMPOOL_BLOCK)
            {
                cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0065","OP_RETURN in transaction",msg);                                            
            }
            else
            {
                if(!found_in_mempool)
                {
                    cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0099","OP_RETURN in transaction",msg);                                     
                }                
            }                
            
            if(!found_in_mempool || (block != CS_DCT_MEMPOOL_BLOCK))
            {
                if(ptr[op_return_pos]==size-op_return_pos-1)
                {
                    if(op_return_pos+1+CS_DCT_ASSET_METADATA_SIZE<size)
                    {
                        sprintf(msg,"%d bytes",size-op_return_pos);
                        cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0066","Transaction asset metadata is too long - ignored:",msg);                            
                    }
                }
                else
                {
                    sprintf(msg,"%d bytes, length byte: %d",size-op_return_pos-1,ptr[op_return_pos]);
                    cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0067","Invalid OP_RETURN data - ignored:",msg);                                            
                }
            }
        }
        
        ptr+=size;
    }    
    
    ptr+=4;    
    
    return err;
}

cs_int32 ReadAssetGenesis(CoinSparkAssetRef *lpcAsset,CoinSparkGenesis *lpcGenesis)
{
    cs_int32 err,required_size,size,j,dbvalue_len,bytes_read;
    void *dbvalue;
    
    cs_uchar CDBKey[CS_DCT_CDB_KEY_SIZE];
    cs_uchar CDBValue[CS_DCT_CDB_VALUE_SIZE];
    cs_uchar GenesisBuffer[CS_DCT_HASH_BYTES+8+CS_DCT_ASSET_METADATA_SIZE];
    
    err=CS_ERR_NOERROR;
    
    memset(CDBValue,0,CS_DCT_CDB_VALUE_SIZE);
    memcpy(CDBValue,&(lpcAsset->blockNum),4);
    memcpy(CDBValue+4,&(lpcAsset->txOffset),4);
        
    memset(CDBKey,0,CS_DCT_CDB_KEY_SIZE);
    bitcoin_tx_hash(CDBKey,CDBValue,CS_DCT_ASSET_ID_SIZE);          // Asset ID is hashed to special TxID
    
    memset(GenesisBuffer,0,CS_DCT_HASH_BYTES+8+CS_DCT_ASSET_METADATA_SIZE);

    bytes_read=0;
    j=0;
    size=0;
    required_size=CS_DCT_CDB_VALUE_SIZE;
    while(bytes_read<required_size)
    {
        cs_PutNumberLittleEndian(CDBKey+CS_DCT_HASH_BYTES+12,&j,4,sizeof(j));
        dbvalue=g_State->m_AssetDB->Read((cs_char*)CDBKey,CS_DCT_CDB_KEY_SIZE,&dbvalue_len,0,&err);
        if(dbvalue)
        {
            if(bytes_read==0)
            {
                size=cs_GetUInt64LittleEndian((cs_char*)dbvalue+4,4);
                required_size=cs_AllocSize(CS_DCT_HASH_BYTES+8+size,CS_DCT_CDB_VALUE_SIZE,1);
            }
            memcpy(GenesisBuffer+bytes_read,dbvalue,CS_DCT_CDB_VALUE_SIZE);
            bytes_read+=CS_DCT_CDB_VALUE_SIZE;
            j++;
        }
        else
        {
            return CS_ERR_ASSET_GENESIS_NOT_FOUND;
        }
    }

    if(CoinSparkGenesisDecode(lpcGenesis, (cs_char*)(GenesisBuffer+8),size))
    {        
        for(j=0;j<COINSPARK_ASSETREF_TXID_PREFIX_LEN;j++)
        {
            lpcAsset->txIDPrefix[j] = *(cs_char*)(GenesisBuffer+8+size+CS_DCT_HASH_BYTES-1-j);
        }                            
    }
    else
    {
        return CS_ERR_ASSET_INVALID_GENESIS;
    }
    
    
    return err;
}

cs_int32 ParseAssetTransaction(cs_uchar *TxHash,cs_int32 block,cs_int32 offset,cs_uchar *TxAssetMetaData,cs_int32 TxAssetMetaDataSize)
{
    cs_int32 err,j,transfer_count,asset,input_row,output_row,output_id,take_it,prefix;
    cs_int64 min_fee;
    CoinSparkGenesis cGenesis;
    CoinSparkAssetQty cQty;
    CoinSparkTransfer cTransfer;
    CoinSparkAssetRef cAsset;
    cs_uchar *ptr;
    cs_char msg[200];
    cs_uchar CDBKey[CS_DCT_CDB_KEY_SIZE];
    cs_uchar CDBValue[CS_DCT_CDB_VALUE_SIZE];
    cs_uchar GenesisBuffer[CS_DCT_HASH_BYTES+8+CS_DCT_ASSET_METADATA_SIZE];
    
    err=CS_ERR_NOERROR;

    if((TxAssetMetaDataSize<=0) && (g_State->m_TxAssetCount<=1))
    {
        return err;
    }

    g_State->m_TxAssetTransfers->Clear();    

                                                                                // Asset genesis, only in full mode
    if(CoinSparkGenesisDecode(&cGenesis, (cs_char*)TxAssetMetaData,TxAssetMetaDataSize))
//    if(CoCoDecodeGenesis((cs_char*)TxAssetMetaData,TxAssetMetaDataSize,&cGenesis))
    {
        bitcoin_hash_to_string(msg,TxHash);
        if(g_State->m_SkipMemPoolLogs==0)
            cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0068","Asset genesis in transaction",msg);                                            

        sprintf(msg,"%10d - %10d - ",block,offset);
        for(j=0;j<COINSPARK_ASSETREF_TXID_PREFIX_LEN;j++)
        {
            sprintf(msg+26+2*j,"%02x",TxHash[CS_DCT_HASH_BYTES-1-j]);
        }
        cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0069","Asset ID:",msg);                                            


        transfer_count=0;
                                                                                // Checking asset genesis has sufficient fee
        min_fee=CoinSparkGenesisCalcMinFee(&cGenesis, 
                                   (CoinSparkSatoshiQty*)(g_State->m_TxOutputSatoshis->GetRow(0)), 
                                   (bool*)(g_State->m_TxOutputRegulars->GetRow(0)),
                                   g_State->m_TxOutputCount);
        if(min_fee<=g_State->m_TxSatoshiFee)
        {
            
            memcpy(CDBKey,TxHash,CS_DCT_HASH_BYTES);
            output_id=0;
            cs_PutNumberLittleEndian(CDBKey+CS_DCT_HASH_BYTES,&output_id,4,sizeof(output_id));
            cs_PutNumberLittleEndian(CDBKey+CS_DCT_HASH_BYTES+4,&block,4,sizeof(block));
            cs_PutNumberLittleEndian(CDBKey+CS_DCT_HASH_BYTES+8,&offset,4,sizeof(offset));
            j=0;
            cs_PutNumberLittleEndian(CDBKey+CS_DCT_HASH_BYTES+12,&j,4,sizeof(offset));// Prefix is not part of DB key
            
            take_it=0;
            if(g_Args->m_TrackerMode == CS_DCT_TRACKER_MODE_FULL)
            {
                take_it=1;
            }
            else
            {
                if(memcmp(CDBKey+CS_DCT_HASH_BYTES+4,g_Args->m_TrackedAsset,CS_DCT_ASSET_ID_SIZE-4) == 0)
                {
                    take_it=1;
                    for(j=0;j<COINSPARK_ASSETREF_TXID_PREFIX_LEN;j++)
                    {
                        if(g_Args->m_TrackedAsset[8+j] != TxHash[CS_DCT_HASH_BYTES-1-j])
                        {
                            take_it=0;
                        }
                    }                    
                    if(take_it == 0)
                    {
                        if(g_State->m_SkipMemPoolLogs==0)
                            cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0070","Wrong TxID prefix in genesis","");                                                                    
                    }
                }
                else
                {
                    if(g_State->m_SkipMemPoolLogs==0)
                        cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0093","Asset should not be tracked","");  
                }
            }
            
            if(take_it)
            {
                cQty=0;
                for(output_id=0;output_id<g_State->m_TxOutputCount;output_id++)
                {
                    g_State->m_TxAssetMatrix->Add(NULL,&cQty);                            
                }

                output_row=g_State->m_TxOutputCount*g_State->m_TxAssetCount;
                CoinSparkGenesisApply(&cGenesis,
                                      (bool*)(g_State->m_TxOutputRegulars->GetRow(0)),
                                      (CoinSparkAssetQty*)(g_State->m_TxAssetMatrix->GetRow(output_row)),                                      
                                      g_State->m_TxOutputCount);
                                      
                memset(CDBValue,0,CS_DCT_CDB_VALUE_SIZE);
                cs_PutNumberLittleEndian(CDBValue+8,&block,4,sizeof(block));
                for(output_id=0;output_id<g_State->m_TxOutputCount;output_id++)
                {
                    cQty=*(CoinSparkAssetQty*)(g_State->m_TxAssetMatrix->GetRow(output_row+output_id));
                    if(cQty>0)
                    {
                        cs_PutNumberLittleEndian(CDBKey+CS_DCT_HASH_BYTES,&output_id,4,sizeof(output_id));
                        cs_PutNumberLittleEndian(CDBValue,&cQty,8,sizeof(cQty));        
                        err=g_State->m_TxOutAdd->Add(CDBKey,CDBValue);              // Asset quantity record
                        if(err)
                        {
                            return err;
                        }                            
                        

                        
                        if(block != CS_DCT_MEMPOOL_BLOCK)
                        {
                            prefix=0;
                            for(j=0;j<COINSPARK_ASSETREF_TXID_PREFIX_LEN;j++)
                            {
                                prefix*=256;
                                prefix+=TxHash[CS_DCT_HASH_BYTES-1-j];
                            }
                            bitcoin_hash_to_string(msg,TxHash);
                            sprintf(msg+64,"-%d-%d %d-%d-%d %ld",
                                output_id,
                                block,
                                block,
                                offset,
                                prefix,
                                (long int)cQty
                                );
                            if(g_State->m_SkipMemPoolLogs==0)
                                cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0096","OUTPUT:",msg);            
                        }
                        else
                        {    
                            sprintf(msg,"%10d - %10d - ",block,offset);
                            for(j=0;j<COINSPARK_ASSETREF_TXID_PREFIX_LEN;j++)
                            {
                                sprintf(msg+26+2*j,"%02x",TxHash[CS_DCT_HASH_BYTES-1-j]);
                            }
                            sprintf(msg+strlen(msg)," Output: %d, Value: %d",output_id,(cs_int32)cQty);
                            if(g_State->m_SkipMemPoolLogs==0)
                                cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0091","DB:",msg);                                            
                        }
                    }
                }
                
                memcpy(CDBKey,TxHash,CS_DCT_HASH_BYTES);
                j=0;
                cs_PutNumberLittleEndian(CDBKey+CS_DCT_HASH_BYTES,&j,4,sizeof(j));
                memset(CDBValue,0,CS_DCT_CDB_VALUE_SIZE);
                j=CS_DCT_CDB_GENESIS_TX_ASSET_MAP;
                cs_PutNumberLittleEndian(CDBValue,&j,4,sizeof(j));
                memcpy(CDBValue+4,CDBKey+CS_DCT_HASH_BYTES+4,CS_DCT_ASSET_ID_SIZE);
                memset(CDBKey+CS_DCT_HASH_BYTES+4,0,CS_DCT_ASSET_ID_SIZE);

                err=g_State->m_TxOutAdd->Add(CDBKey,CDBValue);                  // Special TxID -> AssetID map record
                if(err)
                {
                    return err;
                }        


                memset(CDBKey,0,CS_DCT_CDB_KEY_SIZE);                           // Storing Genesis metadata in special "transaction" record
                bitcoin_tx_hash(CDBKey,CDBValue+4,CS_DCT_ASSET_ID_SIZE);        // Asset ID is hashed to special TxID

                memset(GenesisBuffer,0,CS_DCT_HASH_BYTES+8+CS_DCT_ASSET_METADATA_SIZE);
                j=CS_DCT_CDB_GENESIS_METADATA;                                  // special record format code
                cs_PutNumberLittleEndian(GenesisBuffer,&j,4,sizeof(j));
                cs_PutNumberLittleEndian(GenesisBuffer+4,&TxAssetMetaDataSize,4,sizeof(TxAssetMetaDataSize));
                memcpy(GenesisBuffer+8,TxAssetMetaData,TxAssetMetaDataSize);
                memcpy(GenesisBuffer+8+TxAssetMetaDataSize,TxHash,CS_DCT_HASH_BYTES);                   // Real TxID

                j=0;
                while(j*CS_DCT_CDB_VALUE_SIZE<CS_DCT_HASH_BYTES+8+TxAssetMetaDataSize)
                {
                    cs_PutNumberLittleEndian(CDBKey+CS_DCT_HASH_BYTES+12,&j,4,sizeof(j));
                    memcpy(CDBValue,GenesisBuffer+j*CS_DCT_CDB_VALUE_SIZE,CS_DCT_CDB_VALUE_SIZE);
                    err=g_State->m_TxOutAdd->Add(CDBKey,CDBValue);              // Genesis metadata chunk
                    if(err)
                    {
                        return err;
                    }        
                    j++;
                }

                bitcoin_hash_to_string(msg,CDBKey);
                if(g_State->m_SkipMemPoolLogs==0)
                    cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0071","Asset genesis hash: ",msg);                                                        
            }
            
            if(g_State->m_SkipMemPoolLogs==0)
                cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0072","Valid asset genesis","");                                            
        }        
        else
        {
            sprintf(msg,"Min fee: %d, fee in tx: %d",(cs_int32)min_fee,(cs_int32)(g_State->m_TxOutputCount));
            if(g_State->m_SkipMemPoolLogs==0)
                cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0073","Invalid asset genesis",msg);                                                        
        }
    }
    else
    {                                                   
       transfer_count=CoinSparkTransfersDecodeCount((cs_char*)TxAssetMetaData, TxAssetMetaDataSize);
       if(transfer_count)                                                       // Explicit asset transfers
       {
            memset(&cTransfer,0,sizeof(CoinSparkTransfer));
            for(j=0;j<transfer_count;j++)
            {
                g_State->m_TxAssetTransfers->Add(0,&cTransfer);
            }
                                                                                // Decoding asset transfers
            if(CoinSparkTransfersDecode((CoinSparkTransfer*)(g_State->m_TxAssetTransfers->GetRow(0)),
                                        transfer_count,
                                        g_State->m_TxInputCount,
                                        g_State->m_TxOutputCount,
                                        (cs_char*)TxAssetMetaData,                   
                                        TxAssetMetaDataSize) == transfer_count)
                
            {
                min_fee=CoinSparkTransfersCalcMinFee((CoinSparkTransfer*)(g_State->m_TxAssetTransfers->GetRow(0)), 
                                               transfer_count, 
                                               g_State->m_TxInputCount,
                                               g_State->m_TxOutputCount,
                                               (CoinSparkSatoshiQty*)(g_State->m_TxOutputSatoshis->GetRow(0)),
                                               (bool*)(g_State->m_TxOutputRegulars->GetRow(0)));
                if(min_fee<=g_State->m_TxSatoshiFee)
                {
                    if(g_State->m_SkipMemPoolLogs==0)
                        cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0074","Valid asset transfers","");                                                                
                }
                else
                {
                    sprintf(msg,"Min fee: %d, fee in tx: %d",(cs_int32)min_fee,(cs_int32)(g_State->m_TxOutputCount));
                    if(g_State->m_SkipMemPoolLogs==0)
                        cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0075","Invalid asset transfers",msg);                                                                                    
                    g_State->m_TxAssetTransfers->Clear();
                }
            }
            else
            {
                g_State->m_TxAssetTransfers->Clear();    
                if(g_State->m_SkipMemPoolLogs==0)
                    cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0076","Cannot decode asset transfer","");                                                                        
            }
            
        }
    }

    input_row=g_State->m_TxInputCount;
    output_row=g_State->m_TxInputCount*g_State->m_TxAssetCount+g_State->m_TxOutputCount;
    
    memcpy(CDBKey,TxHash,CS_DCT_HASH_BYTES);
    memset(CDBValue,0,CS_DCT_CDB_VALUE_SIZE);
    cs_PutNumberLittleEndian(CDBValue+8,&block,4,sizeof(block));
    
//    sprintf(msg,"Inp: %d, Out: %d, Col: %d,Tr: %d",g_State->m_TxInputCount,g_State->m_TxOutputCount,g_State->m_TxAssetCount,transfer_count);
//    cs_LogMessage(g_Log,CS_LOG_DEBUG,"","Asset: ",msg);                                            
    
    for(asset=1;asset<g_State->m_TxAssetCount;asset++)                          // Processing asset transfers - implicit and explicit
                                                                                // Started from 1 as asset=0 is BTC
                                                                                // For single asset mode there may be only one asset in asset list
    {
        
        ptr=g_State->m_TxAssetList->GetRow(asset);
        cAsset.blockNum=cs_GetUInt64LittleEndian(ptr,4);
        cAsset.txOffset=cs_GetUInt64LittleEndian(ptr+4,4);
        
        err=ReadAssetGenesis(&cAsset,&cGenesis);
        if(err)
        {
            sprintf(msg,"Error: %08X, Asset Ref: %10d - %10d",err,(cs_int32)cAsset.blockNum,(cs_int32)cAsset.txOffset);
            if(g_State->m_SkipMemPoolLogs==0)
                cs_LogMessage(g_Log,CS_LOG_WARNING,"C-0077","Cannot read asset genesis",msg);                                            
            return err;            
        }
        else
        {
            sprintf(msg,"%10d - %10d - ",(cs_int32)cAsset.blockNum,(cs_int32)cAsset.txOffset);
            for(j=0;j<COINSPARK_ASSETREF_TXID_PREFIX_LEN;j++)
            {
                sprintf(msg+26+2*j,"%02x",*((cs_uchar*)(cAsset.txIDPrefix)+j));
            }
        }
                
        if(transfer_count)
        {
            CoinSparkTransfersApply(&cAsset,                                             // Applying asset transfers
                                    &cGenesis,
                                    (CoinSparkTransfer*)(g_State->m_TxAssetTransfers->GetRow(0)),
                                    transfer_count,
                                    (CoinSparkAssetQty*)(g_State->m_TxAssetMatrix->GetRow(input_row)),
                                    g_State->m_TxInputCount,
                                    (bool*)(g_State->m_TxOutputRegulars->GetRow(0)),
                                    (CoinSparkAssetQty*)(g_State->m_TxAssetMatrix->GetRow(output_row)),
                                    g_State->m_TxOutputCount);
        }
        else
        {
            CoinSparkTransfersApplyNone(&cAsset,                                             // Applying asset transfers
                                    &cGenesis,
                                    (CoinSparkAssetQty*)(g_State->m_TxAssetMatrix->GetRow(input_row)),
                                    g_State->m_TxInputCount,
                                    (bool*)(g_State->m_TxOutputRegulars->GetRow(0)),
                                    (CoinSparkAssetQty*)(g_State->m_TxAssetMatrix->GetRow(output_row)),
                                    g_State->m_TxOutputCount);            
        }
        
        memcpy(CDBKey,TxHash,CS_DCT_HASH_BYTES);
        cs_PutNumberLittleEndian(CDBKey+CS_DCT_HASH_BYTES+4,&(cAsset.blockNum),4,sizeof(cAsset.blockNum));
        cs_PutNumberLittleEndian(CDBKey+CS_DCT_HASH_BYTES+8,&(cAsset.txOffset),4,sizeof(cAsset.txOffset));
        j=0;
        cs_PutNumberLittleEndian(CDBKey+CS_DCT_HASH_BYTES+12,&j,4,sizeof(offset));// Prefix is not part of DB key
        memset(CDBValue,0,CS_DCT_CDB_VALUE_SIZE);
        cs_PutNumberLittleEndian(CDBValue+8,&block,4,sizeof(block));
        
        for(output_id=0;output_id<g_State->m_TxOutputCount;output_id++)
        {
            cQty=*(CoinSparkAssetQty*)(g_State->m_TxAssetMatrix->GetRow(output_row+output_id));
            if((cs_int64)cQty>0)
            {
                cs_PutNumberLittleEndian(CDBKey+CS_DCT_HASH_BYTES,&output_id,4,sizeof(j));
                cs_PutNumberLittleEndian(CDBValue,&cQty,8,sizeof(CoinSparkAssetQty));        
                err=g_State->m_TxOutAdd->Add(CDBKey,CDBValue);                  // Adding new asset value to "Add" buffer, will be stored in DB in StoreTXsInDB
                if(err)
                {
                    return err;
                }        
                if(block != CS_DCT_MEMPOOL_BLOCK)
                {
                    prefix=0;
                    for(j=0;j<COINSPARK_ASSETREF_TXID_PREFIX_LEN;j++)
                    {
                        prefix*=256;
                        prefix+=*((cs_uchar*)(cAsset.txIDPrefix)+j);
                    }
                    bitcoin_hash_to_string(msg,TxHash);
                    sprintf(msg+64,"-%d-%d %d-%d-%d %ld",
                        output_id,
                        block,
                        (cs_int32)cAsset.blockNum,
                        (cs_int32)cAsset.txOffset,
                        prefix,
                        (long int)cQty
                        );
                    if(g_State->m_SkipMemPoolLogs==0)
                        cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0097","OUTPUT:",msg);            
                }
                else
                {    
                    sprintf(msg,"%10d - %10d - ",(cs_int32)cAsset.blockNum,(cs_int32)cAsset.txOffset);
                    for(j=0;j<COINSPARK_ASSETREF_TXID_PREFIX_LEN;j++)
                    {
                        sprintf(msg+26+2*j,"%02x",*((cs_uchar*)(cAsset.txIDPrefix)+j));
                    }
                    sprintf(msg+strlen(msg)," Output: %d, Value: %d",output_id,(cs_int32)cQty);
                    
                    if(g_State->m_SkipMemPoolLogs==0)
                        cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0092","OUTPUT:",msg);                                    
                }
            }
        }
        
        input_row+=g_State->m_TxInputCount;
        output_row+=g_State->m_TxOutputCount;
    }
    

    
    return err;
}


