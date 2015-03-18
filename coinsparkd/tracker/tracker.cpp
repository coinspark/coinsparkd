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
#include "net.h"
#include "db.h"
#include "args.h"
#include "state.h"
#include "bitcoin.h"
#include "us_declare.h"
#include "coinspark.h"

#define CS_DCT_RECV_CACHE_SIZE                16777216




extern cs_Arguments *g_Args;

cs_State            *g_State;
cs_handle           g_Log;
cs_Connection       *g_Connection;
cs_Message          *g_Message;
cs_handle           g_Semaphore;
cs_int32            g_ProcessMessage;
cs_int32            g_ErrorCode;

cs_int32 RemoveBlock(cs_int32 mempool);
cs_int32 PurgeBlock(cs_int32 block);
cs_int32 PurgeBlock(cs_int32 block);
cs_int32 ProcessBlock(cs_int32 block,cs_int32 block_count);
cs_int32 ProcessMemPool(cs_int32 attempt);

cs_int32 ReadBlockChain();
cs_int32 ReadAssetState();
cs_int32 GetRelevantBlockID(cs_int32 mode);
cs_int32 UpdateBlockChain();






typedef struct cs_RecvThreadData
{
    cs_int32    m_StopFlag;
} cs_RecvThreadData;

/*
 * --- Asynchronous message processing. Send message, wait for [count] valid responses.
 */


cs_int32 TrackerProcessAsyncMessage(cs_int32 count)
{
    cs_int32 StopTime;
    cs_int32 err;
    
    err=CS_ERR_NOERROR;
    
    StopTime=(cs_int32)cs_TimeNow()+CS_DCT_MSG_TIMEOUT;
    cs_int32 size;
    
    size=g_Message->GetSize();
    
    cs_LogMessage(g_Log,CS_LOG_MINOR,"C-0001","Bitcoin message request: ",(cs_char*)(g_Message->GetData())+4);                      
    
    g_ProcessMessage=count;                                                     // Setting response count, if 0 receiving thread ignores all messaged
    
    err=g_Connection->SendNow(g_Message->GetData(),size);                       // Sending request
    if(err)
    {
        return err;
    }
    
    while(g_ProcessMessage)                                                     // Waiting for response, receiving thread decrements g_ProcessMessage
    {
        if((cs_int32)cs_TimeNow()>=StopTime)
        {
            cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0002","Timeout","");      
            return CS_ERR_TIMEOUT;                            
        }
        
        __US_Sleep(1);
    }
    
    return g_ErrorCode;
}


/*
 * --- Checked that received message is response to request 
 */

cs_int32 IsResponse(cs_uchar *lpMessageHeader)                      
{
    cs_uchar *lpRequest;
    lpRequest=(cs_uchar *)(g_Message->GetData());    
    
    if(strcmp((cs_char*)lpRequest+4,"version") == 0)
    {
        if(strcmp((cs_char*)lpMessageHeader+4,"verack") == 0)
        {
            return 1;
        }        
    }
    
    if(strcmp((cs_char*)lpRequest+4,"getblocks") == 0)
    {
        if(strcmp((cs_char*)lpMessageHeader+4,"inv") == 0)
        {
            return 1;
        }        
    }

    if(strcmp((cs_char*)lpRequest+4,"getheaders") == 0)
    {
        if(strcmp((cs_char*)lpMessageHeader+4,"headers") == 0)
        {
            return 1;
        }        
    }
    
    if(strcmp((cs_char*)lpRequest+4,"getdata") == 0)
    {
        if(strcmp((cs_char*)lpMessageHeader+4,"notfound") == 0)
        {
            return -1;
        }        
        if(strcmp((cs_char*)lpMessageHeader+4,"block") == 0)
        {
            return 1;
        }        
        if(strcmp((cs_char*)lpMessageHeader+4,"tx") == 0)
        {
            return 1;
        }        
    }
    
    if(strcmp((cs_char*)lpRequest+4,"mempool") == 0)
    {
        if(strcmp((cs_char*)lpMessageHeader+4,"inv") == 0)
        {
            return 1;
        }        
    }
    
    return 0;
}

/*
 * --- Receiving thread loop
 */

cs_int32 TrackerRecvLoop(void *ThreadData)
{
    cs_int32 err;
    cs_int32 size,total,payload_size,offset,varint_size;
    cs_int32 RelevantMessage;
    cs_uchar *NewBuffer;
    cs_int32 NewSize;
    
    cs_uchar MessageHeader[CS_DCT_MSG_HEADERSIZE];
        
    cs_RecvThreadData *lpRecvThreadData;
    
    lpRecvThreadData=(cs_RecvThreadData*)ThreadData;
    
    err= CS_ERR_NOERROR;
    offset=0;

    while(!(lpRecvThreadData->m_StopFlag))
    {
        size=g_Connection->GetSize();

        while(size<CS_DCT_MSG_HEADERSIZE)                                       // Waiting to new message
        {
            err=g_Connection->Recv();
            if(err)
            {
                return err;
            }
            size=g_Connection->GetSize();
        }

        memcpy(MessageHeader,g_Connection->GetData(CS_DCT_MSG_HEADERSIZE),CS_DCT_MSG_HEADERSIZE);
        payload_size=(cs_int32)cs_GetUInt64LittleEndian(MessageHeader+16,4);
        
        RelevantMessage=0;
        if(g_ProcessMessage)                                                    // if g_ProcessMessage==0 all messages are ignored.g_ProcessMessage is set by sending thread
        {
            RelevantMessage=IsResponse(MessageHeader);
        }
        
        if(RelevantMessage)
        {
                                                                                // Reallocation
            if(offset+CS_DCT_MSG_HEADERSIZE+payload_size>g_State->m_ResponseMessageAllocSize)
            {
                NewSize=cs_AllocSize(offset+CS_DCT_MSG_HEADERSIZE+payload_size,65536,1);
                NewBuffer=(cs_uchar*)cs_New(NewSize,NULL,CS_ALT_DEFAULT);
                if(NewBuffer == NULL)
                {
                    g_ErrorCode=CS_ERR_ALLOCATION;
                    RelevantMessage=0;
                    g_ProcessMessage=0;
                }
                else
                {
                    if(g_State->m_ResponseMessage)
                    {
                        if(offset)
                        {
                            memcpy(NewBuffer,g_State->m_ResponseMessage,offset);
                        }
                        cs_Delete(g_State->m_ResponseMessage,NULL,CS_ALT_DEFAULT);
                    }
                    g_State->m_ResponseMessage=NULL;                    
                }
                
                g_State->m_ResponseMessageAllocSize=NewSize;
                g_State->m_ResponseMessage=NewBuffer;
            }
            memcpy(g_State->m_ResponseMessage+offset,MessageHeader,CS_DCT_MSG_HEADERSIZE);
        }
        
        total=0;    
        while(total<payload_size)                                               // Reading message    
        {
            size=g_Connection->GetSize();
            if(total+size>payload_size)
            {
                size=payload_size-total;
            }
            if(RelevantMessage)                                                 // Storing message
            {
                memcpy(g_State->m_ResponseMessage+offset+CS_DCT_MSG_HEADERSIZE+total,g_Connection->GetData(size),size);                
//                cs_MemoryDump(g_State->m_ResponseMessage+offset,0,size+CS_DCT_MSG_HEADERSIZE+total);
                if(RelevantMessage==-1)
                {
                    if(total+size>0)
                    {
                        varint_size=bitcoin_get_varint_size(g_State->m_ResponseMessage[offset+CS_DCT_MSG_HEADERSIZE]);
                        if(varint_size>0)
                        {
                            if(total+size>=1+varint_size)
                            {
                                RelevantMessage=cs_GetUInt64LittleEndian(g_State->m_ResponseMessage+offset+CS_DCT_MSG_HEADERSIZE+1,varint_size);
                            }
                        }
                        else
                        {
                            RelevantMessage=g_State->m_ResponseMessage[offset+CS_DCT_MSG_HEADERSIZE];
                        }
                    }
                }
            }
            else                                                                // Irrelevant message is skipped, GetData shifts internal pointer
            {
                g_Connection->GetData(size);                                
            }
            
            total+=size;
            if(total<payload_size)                                              // If only part of the message is received - wait for the nest chunk    
            {
                err=g_Connection->Recv();
                if(err)
                {
                    g_ProcessMessage=0;
                    g_ErrorCode=err;
                    return err;
                }
            }
        }                 
        
        if(RelevantMessage)
        {
            offset+=CS_DCT_MSG_HEADERSIZE+payload_size;
        }
        
        if(g_ProcessMessage)
        {
            if(RelevantMessage)                                                 // Decrementing g_ProcessMessage
            {
                cs_LogMessage(g_Log,CS_LOG_MINOR,"C-0003","Bitcoin message response: ",(cs_char*)MessageHeader+4);                      
                g_ProcessMessage-=RelevantMessage;
                if(g_ProcessMessage==0)
                {
                    offset=0;
                }
            }
            else
            {
                cs_LogMessage(g_Log,CS_LOG_MINOR,"C-0004","Bitcoin message ignored: ",(cs_char*)MessageHeader+4);                      
            }
        }
    }            
        
    return CS_ERR_NOERROR;
}


/*
 * --- Backup
 */

cs_int32 cs_BackUp(cs_int32 block)
{
    cs_char folder[CS_DCT_MAX_PATH];
    cs_char suffix[20];
    cs_char command[CS_DCT_MAX_PATH];

    g_State->m_AssetDB->Lock(1);
    g_State->m_AssetDB->Synchronize();
    g_State->m_AssetDB->UnLock();
    
    sprintf(suffix,"%d",block);
    
    cs_LogMessage(g_Log,CS_LOG_SYSTEM,"C-0005","Backup, block: ",suffix);
    
    sprintf(suffix,"_%10d",(cs_int32)cs_TimeNow());

    sprintf(folder,"%s%s",g_Args->m_DataDir,CS_DCT_FOLDER_BACKUP);
    sprintf(command,"mkdir %s",folder);    
    __US_Shell(command);

    sprintf(folder,"%s%s%c%06d%c",g_Args->m_DataDir,CS_DCT_FOLDER_BACKUP,CS_DCT_FOLDERSEPARATOR,block,CS_DCT_FOLDERSEPARATOR);
    sprintf(command,"mkdir %s",folder);    
    __US_Shell(command);
    
    sprintf(command,"mv %s %stracker%s.log",g_Args->m_TrackerLogFile,folder,suffix);    
    __US_Shell(command);
    
    sprintf(command,"cp -R %s%s %s",g_Args->m_DataDir,CS_DCT_FILE_ASSETDB,folder);    
    __US_Shell(command);

    sprintf(command,"cp -R %s%s %s",g_Args->m_DataDir,CS_DCT_FOLDER_NEW_TXOUTS,folder);    
    __US_Shell(command);
    
    sprintf(command,"cp -R %s%s %s",g_Args->m_DataDir,CS_DCT_FOLDER_SPENT_TXOUTS,folder);    
    __US_Shell(command);
    
    sprintf(command,"cp -R %s%s %s",g_Args->m_DataDir,CS_DCT_FOLDER_OP_RETURN,folder);    
    __US_Shell(command);
    
    sprintf(command,"cp %s%s %s",g_Args->m_DataDir,CS_DCT_FILE_ASSETSTATE,folder);    
    __US_Shell(command);

    sprintf(command,"cp %s%s %s",g_Args->m_DataDir,CS_DCT_FILE_BLOCKCHAIN,folder);    
    __US_Shell(command);
    
    sprintf(command,"mv %sdb.log %s",g_Args->m_DataDir,folder);    
    __US_Shell(command);
    
    
    sprintf(suffix,"%d",block);
    cs_LogMessage(g_Log,CS_LOG_SYSTEM,"C-0006","Starting new log file, block: ",suffix);
    
    return CS_ERR_NOERROR;
}

/*
 * Restores from backup before specified block
 */

cs_int32 cs_RestoreFromBackUp(cs_int32 *height)
{
    cs_int32 err;
    cs_char folder[CS_DCT_MAX_PATH];
    cs_char subfolder[CS_DCT_MAX_PATH];
    cs_char renamefolder[CS_DCT_MAX_PATH];
    cs_char command[CS_DCT_MAX_PATH];
    cs_char dbname[CS_DCT_MAX_PATH];    
    cs_int32 restore_block,block,dboptions,moved;
    cs_char *ptr;
    cs_char suffix[20];
    
    
    err=CS_ERR_NOERROR;
    
    sprintf(suffix,"%10d",(cs_int32)cs_TimeNow());
    sprintf(folder,"%s%s",g_Args->m_DataDir,CS_DCT_FOLDER_BACKUP);
    
    struct dirent* dent;
    DIR* srcdir = opendir(folder);
    struct stat st;

    restore_block=-1;
    moved=0;
    
    if (srcdir)
    {
        while((dent = readdir(srcdir)) != NULL)
        {
            if((strcmp(dent->d_name, ".") != 0) && (strcmp(dent->d_name, "..") != 0))
            {
                if (fstatat(dirfd(srcdir), dent->d_name, &st, 0) >=0)
                {
                    if (S_ISDIR(st.st_mode))
                    {
                        ptr=dent->d_name;
                        block=0;
                        while(*ptr)
                        {
                            if(block>=0)
                            {
                                if((*ptr>=0x30) && (*ptr<=0x39))
                                {
                                    block=10*block+(*ptr-0x30);
                                }
                                else
                                {
                                    block=-1;
                                }
                            }
                            ptr++;
                        }
                        if(block<*height)
                        {
                            if(block>restore_block)
                            {
                                restore_block=block;
                            }                            
                        }
                        else
                        {
                            sprintf(subfolder,"%s%s%c%06d%c",g_Args->m_DataDir,CS_DCT_FOLDER_BACKUP,CS_DCT_FOLDERSEPARATOR,block,CS_DCT_FOLDERSEPARATOR);
                            sprintf(renamefolder,"%s%s%c%06d-%s%c",g_Args->m_DataDir,CS_DCT_FOLDER_BACKUP,CS_DCT_FOLDERSEPARATOR,block,suffix,CS_DCT_FOLDERSEPARATOR);
                            sprintf(command,"mv %s %s",subfolder,renamefolder);    
                            __US_Shell(command);
                            moved++;
                        }
                    }
                }                
            }
        }
        closedir(srcdir);
    }
 
    strcpy(dbname,g_State->m_AssetDB->m_Name);
    dboptions=g_State->m_AssetDB->m_Options;
    
    g_State->m_AssetDB->Lock(1);
    g_State->m_AssetDB->Close();
    g_State->m_AssetDB->UnLock();
    
    sprintf(renamefolder,"%s%s%cerror-%s%c",g_Args->m_DataDir,CS_DCT_FOLDER_BACKUP,CS_DCT_FOLDERSEPARATOR,suffix,CS_DCT_FOLDERSEPARATOR);
    sprintf(command,"mkdir %s",renamefolder);    
    __US_Shell(command);
    sprintf(command,"mv %s%c* %s",g_Args->m_DataDir,CS_DCT_FOLDERSEPARATOR,renamefolder);    
    __US_Shell(command);

    if(moved)
    {
        sprintf(command,"rm -Rf %s",renamefolder);    
        __US_Shell(command);       
    }

    if(restore_block>=0)
    {
        sprintf(subfolder,"%s%s%c%06d%c",g_Args->m_DataDir,CS_DCT_FOLDER_BACKUP,CS_DCT_FOLDERSEPARATOR,restore_block,CS_DCT_FOLDERSEPARATOR);
        sprintf(command,"cp -Rf %s* %s%c",subfolder,g_Args->m_DataDir,CS_DCT_FOLDERSEPARATOR);    
        __US_Shell(command);        
    }
    
    g_State->m_AssetDB->Lock(1);
    g_State->m_AssetDB->Open(dbname,dboptions);
    g_State->m_AssetDB->UnLock();
    
    *height=restore_block;
    
    return err;
}


/*
 * --- Initializes Tracker 
 */

cs_int32 cs_InitTracker()
{
    cs_int32 err;
    CoinSparkAssetRef *assetRef;
    cs_char encodedAsset[64];
    
    err=CS_ERR_NOERROR;
    
    if(strlen(g_Args->m_DataDir) == 0)
    {
        if(__US_GetHomeFolder(g_Args->m_DataDir,CS_DCT_MAX_PATH) < 0)
        {
            return CS_ERR_FILENAME_TOO_LONG;
        }
        
        err=cs_TestSubFolder(g_Args->m_DataDir,CS_DCT_MAX_PATH,g_Args->m_DataDir,CS_DCT_DATA_FOLDER,1);
        if(err)
        {
            return err;        
        }
        
        err=cs_TestSubFolder(g_Args->m_DataDir,CS_DCT_MAX_PATH,g_Args->m_DataDir,CS_DCT_DATA_ASSETS_FOLDER,1);
        if(err)
        {
            return err;        
        }

        if(strcmp(g_Args->m_BitcoinNetwork,"testnet3")==0)
        {
            err=cs_TestSubFolder(g_Args->m_DataDir,CS_DCT_MAX_PATH,g_Args->m_DataDir,CS_DCT_DATA_TESTNET_FOLDER,1);
            if(err)
            {
                return err;        
            }
        }

        if(g_Args->m_TrackerMode == CS_DCT_TRACKER_MODE_SINGLE)
        {
            assetRef=new CoinSparkAssetRef;
            assetRef->blockNum=cs_GetUInt64LittleEndian(g_Args->m_TrackedAsset,4);
            assetRef->txOffset=cs_GetUInt64LittleEndian(g_Args->m_TrackedAsset+4,4);
            memcpy(assetRef->txIDPrefix,g_Args->m_TrackedAsset+8,COINSPARK_ASSETREF_TXID_PREFIX_LEN);
/*            
            sprintf(asset_ref, "%d-%d-%d",(cs_int32)cs_GetUInt64LittleEndian(g_Args->m_TrackedAsset,4),
                    (cs_int32)cs_GetUInt64LittleEndian(g_Args->m_TrackedAsset+4,4),
                    256*(cs_int32)cs_GetUInt64LittleEndian(g_Args->m_TrackedAsset+9,1)+(cs_int32)cs_GetUInt64LittleEndian(g_Args->m_TrackedAsset+8,1));

            err=cs_TestSubFolder(g_Args->m_DataDir,CS_DCT_MAX_PATH,g_Args->m_DataDir,asset_ref,1);
 */ 
            CoinSparkAssetRefEncode(assetRef,encodedAsset,64);
            err=cs_TestSubFolder(g_Args->m_DataDir,CS_DCT_MAX_PATH,g_Args->m_DataDir,encodedAsset,1);
            if(err)
            {
                return err;        
            }        
        }

        if(g_Args->m_TrackerMode == CS_DCT_TRACKER_MODE_FULL)
        {
            err=cs_TestSubFolder(g_Args->m_DataDir,CS_DCT_MAX_PATH,g_Args->m_DataDir,CS_DCT_DATA_ALL_ASSETS_FOLDER,1);
            if(err)
            {
                return err;        
            }        
        }    
    }
        
        
    strcpy(g_Args->m_TrackerLogFile,g_Args->m_DataDir);
    strcat(g_Args->m_TrackerLogFile,"tracker.log");
    strcpy(g_Args->m_DBLogFile,g_Args->m_DataDir);
    strcat(g_Args->m_DBLogFile,"db.log");
    
//    g_Args->m_TrackerLogFilter=CS_LOG_FATAL | CS_LOG_ERROR | CS_LOG_WARNING | CS_LOG_SYSTEM | CS_LOG_REPORT | CS_LOG_DEBUG | CS_LOG_NO_CODE;// | CS_LOG_MINOR;    
    
    return CS_ERR_NOERROR;
}

cs_int32 cs_CheckTrackedAsset(cs_Database *db)
{
    cs_int32 j;
    cs_uchar TxHash[CS_DCT_HASH_BYTES];
    cs_uchar CDBKey[CS_DCT_CDB_KEY_SIZE];
    cs_uchar CDBValue[CS_DCT_CDB_VALUE_SIZE];
    cs_int32 err,dbvalue_len;
    cs_uchar *dbvalue;
    CoinSparkAssetRef *assetRef;
    cs_char encodedAsset[64];
    
    
    err=CS_ERR_NOERROR;
    
    memset(CDBKey,0,CS_DCT_CDB_KEY_SIZE);    
    strcpy((cs_char*)TxHash,"trackedasset");
    bitcoin_tx_hash(CDBKey,TxHash,strlen((cs_char*)TxHash));          
        
    dbvalue=(cs_uchar *)db->Read((cs_char*)CDBKey,CS_DCT_CDB_KEY_SIZE,&dbvalue_len,0,&err);
    
    if(err)
    {
        goto exitlbl;
    }
    
    if(dbvalue)
    {
        switch(cs_GetUInt64LittleEndian(dbvalue,4))
        {
            case CS_DCT_CDB_TRACKED_ASSET:
                if(g_Args->m_TrackerMode == CS_DCT_TRACKER_MODE_FULL)
                {
                    printf("This database tracks specific asset, cannot run full mode\n");
                    err=CS_ERR_WRONG_TRACKED_ASSET;
                    goto exitlbl;
                }
                if(g_Args->m_TrackerMode == CS_DCT_TRACKER_MODE_SINGLE)
                {
                    if(memcmp(dbvalue+4,g_Args->m_TrackedAsset,CS_DCT_ASSET_ID_SIZE) != 0)
                    {
                        assetRef=new CoinSparkAssetRef;
                        assetRef->blockNum=cs_GetUInt64LittleEndian(dbvalue+4,4);
                        assetRef->txOffset=cs_GetUInt64LittleEndian(dbvalue+8,4);
                        memcpy(assetRef->txIDPrefix,dbvalue+12,COINSPARK_ASSETREF_TXID_PREFIX_LEN);

/*                        
                        block=cs_GetUInt64LittleEndian(dbvalue+4,sizeof(block));
                        offset=cs_GetUInt64LittleEndian(dbvalue+8,sizeof(offset));
                        iprefix=cs_GetUInt64LittleEndian(dbvalue+12+j,2);
                        printf("This database tracks different asset: %d-%d-%d\n",block,offset,iprefix);
 */
                        
                        CoinSparkAssetRefEncode(assetRef,encodedAsset,64);
                        printf("This database tracks different asset: %s\n",encodedAsset);
                        err=CS_ERR_WRONG_TRACKED_ASSET;
                        goto exitlbl;
                    }                    
                }                
                break;
            case CS_DCT_CDB_ALL_ASSETS:
                if(g_Args->m_TrackerMode == CS_DCT_TRACKER_MODE_SINGLE)
                {
                    printf("This database tracks all assets, cannot run single asset mode\n");
                    err=CS_ERR_WRONG_TRACKED_ASSET;
                    goto exitlbl;
                }                
        }
        goto exitlbl;        
    }
    
    memset(CDBValue,0,CS_DCT_CDB_VALUE_SIZE);
    if(g_Args->m_TrackerMode == CS_DCT_TRACKER_MODE_FULL)
    {
        j=CS_DCT_CDB_ALL_ASSETS;
        cs_PutNumberLittleEndian(CDBValue,&j,4,sizeof(j));
    }
    if(g_Args->m_TrackerMode == CS_DCT_TRACKER_MODE_SINGLE)
    {
        j=CS_DCT_CDB_TRACKED_ASSET;
        cs_PutNumberLittleEndian(CDBValue,&j,4,sizeof(j));
        memcpy(CDBValue+4,g_Args->m_TrackedAsset,CS_DCT_ASSET_ID_SIZE);        
    }    
    
    err=db->Write((cs_char*)CDBKey,CS_DCT_CDB_KEY_SIZE,(cs_char*)CDBValue,CS_DCT_CDB_VALUE_SIZE,0);
    if(err)
    {
        return err;
    }        
    
    err=db->Commit(0);
    if(err)
    {
        return err;
    }        
    
exitlbl:
                
    return err;
}

void cs_initMemPoolOpReturnsLast()
{
    cs_int32 group,count,ts;
    cs_uchar TxHash[CS_DCT_HASH_BYTES];
    cs_char filename[CS_DCT_MAX_PATH];
    cs_char buf[256];
    cs_char *ptr;
    cs_char *ptrStart;
    cs_char *ptrEnd;
    FILE *fHan;
    
    g_State->m_MemPoolOpReturnsLast->Clear();
    
    group=(CS_DCT_MEMPOOL_BLOCK/CS_DCT_BLOCK_PER_FOLDER)*CS_DCT_BLOCK_PER_FOLDER;
    sprintf(filename,"%s%s%c%06d",g_Args->m_DataDir,CS_DCT_FOLDER_OP_RETURN,CS_DCT_FOLDERSEPARATOR,group);
    sprintf(filename,"%s%c%06d.log",filename,CS_DCT_FOLDERSEPARATOR,CS_DCT_MEMPOOL_BLOCK);
    
    fHan=fopen(filename,"r");
    if(fHan)
    {
        while(fgets(buf,256,fHan))
        {
            count=0;
            ts=-1;
            ptrStart=buf;
            ptr=ptrStart;
            ptrEnd=ptrStart+strlen(ptrStart)+1;
            while(ptr<ptrEnd)
            {
                if(*ptr == '\t')
                {
                    *ptr=0x00;
                }
                if(*ptr == 0x00)
                {
                    if(count == 0)
                    {
                        bitcoin_string_to_hash(ptrStart,TxHash);
                    }
                    if(count == 3)
                    {
                        ts=atoi(ptrStart);
                    }
                    ptrStart=ptr+1;
                    count++;                    
                }
                ptr++;                
            }
            if(ts>=0)
            {
                g_State->m_MemPoolOpReturnsLast->Add(TxHash,&ts);                
            }            
        }
        fclose(fHan);
    }       
}


/*
 * --- Tracker main function, normally called in separate thread. Parameter - cs_Database
 */


cs_int32 cs_Tracker(void *ThreadData)
{
    cs_int32 err;
    cs_int32 size;
    cs_int32 block,last,processed,block_count,count,i,mempool_attempt;
    cs_char msg[CS_DCT_MAX_PATH];
    cs_double start_time;
    cs_int32 new_op_return_file;    
    cs_handle hRecvThread;
    cs_uint32 idRecvThread;
    cs_int32 restore_block;
    cs_RecvThreadData *lpRecvThreadData;

    
    err=CS_ERR_NOERROR;
    
    g_State=NULL;
    g_Log=NULL;
    g_Connection=NULL;
    g_Semaphore=NULL;;
    hRecvThread=NULL;
    lpRecvThreadData=NULL;
    
    sprintf(msg,"mkdir %s%s",g_Args->m_DataDir,CS_DCT_FOLDER_NEW_TXOUTS);
    __US_Shell(msg);
    sprintf(msg,"mkdir %s%s",g_Args->m_DataDir,CS_DCT_FOLDER_SPENT_TXOUTS);
    __US_Shell(msg);
    sprintf(msg,"mkdir %s%s",g_Args->m_DataDir,CS_DCT_FOLDER_OP_RETURN);
    __US_Shell(msg);
        
  
    g_State=new cs_State;
    
    g_State->m_TxOutAdd=new cs_Buffer;
    g_State->m_TxOutAdd->Initialize(CS_DCT_CDB_KEY_SIZE,CS_DCT_CDB_KEY_SIZE+CS_DCT_CDB_VALUE_SIZE,0);
    g_State->m_TxOutRemove=new cs_Buffer;
    g_State->m_TxOutRemove->Initialize(CS_DCT_CDB_KEY_SIZE,CS_DCT_CDB_KEY_SIZE+CS_DCT_CDB_VALUE_SIZE,0);
    g_State->m_MemPoolOpReturns=new cs_Buffer;
    g_State->m_MemPoolOpReturns->Initialize(CS_DCT_HASH_BYTES,CS_DCT_HASH_BYTES+sizeof(cs_uint32),0);
    g_State->m_MemPoolOpReturnsLast=new cs_Buffer;
    g_State->m_MemPoolOpReturnsLast->Initialize(CS_DCT_HASH_BYTES,CS_DCT_HASH_BYTES+sizeof(cs_uint32),0);
    g_State->m_MemoryPool=new cs_List;
    
    g_State->m_TxAssetMatrix=new cs_Buffer;
    g_State->m_TxAssetMatrix->Initialize(0,sizeof(CoinSparkAssetQty),0);
    g_State->m_TxAssetList=new cs_Buffer;
    g_State->m_TxAssetList->Initialize(CS_DCT_ASSET_ID_SIZE,CS_DCT_ASSET_ID_SIZE,0);
    g_State->m_TxAssetTransfers=new cs_Buffer;
    g_State->m_TxAssetTransfers->Initialize(0,sizeof(CoinSparkTransfer),0);
    g_State->m_TxOutputSatoshis=new cs_Buffer;
    g_State->m_TxOutputSatoshis->Initialize(0,sizeof(CoinSparkSatoshiQty),0);
    g_State->m_TxOutputRegulars=new cs_Buffer;
    g_State->m_TxOutputRegulars->Initialize(0,sizeof(bool),0);
        
        
    
    g_State->m_AssetDB=(cs_Database*)ThreadData;

    g_Log=cs_LogInitialize(g_Args->m_TrackerLogFile,-1,g_Args->m_TrackerLogFilter,g_Args->m_TrackerLogMode);

    
    cs_LogShift(g_Log);
    cs_LogMessage(g_Log,CS_LOG_SYSTEM,"C-0007","Tracker process, Starting...","");
    
    
    g_Connection=new cs_Connection;
    err=g_Connection->Connect(g_Args->m_BitcoinClientAddress,g_Args->m_BitcoinClientPort,-1,-1,0);    
        
    if(err)
    {
        cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0009","Cannot connect to Bitcoin node","");      
        goto exitlbl;
    }

    
    g_Semaphore=__US_SemCreate();
    if(g_Semaphore == NULL)
    {
        cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0010","Cannot create semaphore","");      
        goto exitlbl;        
    }
    
    g_ProcessMessage=0;
    g_ErrorCode=0;

    lpRecvThreadData=new cs_RecvThreadData;
    lpRecvThreadData->m_StopFlag=0;
    
    hRecvThread=__US_CreateThread(NULL,0,(void *)TrackerRecvLoop,(void *)(lpRecvThreadData),THREAD_SET_INFORMATION,&idRecvThread);//CREATE_SUSPENDED
    if(hRecvThread == NULL)
    {
        cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0011","Cannot create receiving thread","");      
        goto exitlbl;                
    }
    
    g_Message=new cs_Message;
    g_Message->Initialize(g_Args,-1);

    cs_LogMessage(g_Log,CS_LOG_SYSTEM,"C-0012","Tracker process started","");
    
    
    size=bitcoin_prepare_message_version(g_Message,0);        
    err=TrackerProcessAsyncMessage(1);
    
    if(err)
    {
        cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0013","Version exchange failed","");      
        goto exitlbl;        
    }
    
    restore_block=CS_DCT_MEMPOOL_BLOCK;
    
    while(restore_block>=-1)
    {
        ReadAssetState();
        if(err)
        {
            cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0008","Cannot initialize assetstate","");      
            goto errorlbl;        
        }

        err=ReadBlockChain();
        if(err)
        {
            cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0014","Cannot initialize blockchain","");      
            goto errorlbl;        
        }

        sprintf(msg,"%d",g_State->m_BlockChainHeight);
        cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0104","Blockchain height:",msg);

        UpdateBlockChain();
        block=GetRelevantBlockID(0);
        processed=0;

        new_op_return_file=1;
        block_count=CS_DCT_BLOCKS_CHUNK;

        cs_initMemPoolOpReturnsLast();    

        start_time=cs_TimeNow();

        while((g_State->m_AssetDB->m_lpHandlerShMem[CS_OFF_DB_SHMEM_HEAD_SIGNAL] == 0) && (cs_TimeNow()-start_time<7200))
        {
            if(block<=g_State->m_BlockChainHeight)
            {            
                err=RemoveBlock(1);
                if(err)
                {
                    sprintf(msg,"Error: %08X",err);
                    cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0015","Cannot clear memory pool",msg);                  
                    goto errorlbl;
                }
                g_State->m_MemPoolOpReturnsLast->CopyFrom(g_State->m_MemPoolOpReturns);
                g_State->m_MemPoolOpReturns->Clear();
                new_op_return_file=1;
                block=GetRelevantBlockID(0);
                last=block+10000;
                if(last>g_State->m_BlockChainHeight)
                {
                    last=g_State->m_BlockChainHeight;
                }

                while(block<=last)
                {
                    count=block_count;
                    if(count>g_State->m_BlockChainHeight-block+1)
                    {
                        count=g_State->m_BlockChainHeight-block+1;
                    }
                    err=ProcessBlock(block,count);
                    if(err)
                    {
                        sprintf(msg,"Error: %08X",err);
                        cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0016","Error while processing block",msg);                  
                        goto errorlbl;
                    }

                    if(block>=CS_DCT_BLOCK_PURGE_DELAY)
                    {
                        for(i=0;i<count;i++)
                        {
                            err=PurgeBlock(block+i-CS_DCT_BLOCK_PURGE_DELAY);
                            if(err)
                            {
                                sprintf(msg,"Error: %08X",err);
                                cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0017","Error while purging block",msg);                  
                                goto errorlbl;     
                            }
                        }
                    }        


                    if(block/10000 != (block+count)/10000)
                    {
                        err=RemoveBlock(1);
                        cs_BackUp(block+count);                    
                        goto exitlbl;
                    }

                    processed+=count;
                    block+=count;

                    if(g_State->m_AssetDB->m_lpHandlerShMem[CS_OFF_DB_SHMEM_HEAD_SIGNAL])
                    {
                        block=last+1;
                    }
                }
            }
            else
            {
                __US_Sleep(5000);
            }

            if(g_State->m_AssetDB->m_lpHandlerShMem[CS_OFF_DB_SHMEM_HEAD_SIGNAL] == 0)
            {
                if(block>g_State->m_BlockChainHeight)
                {
                    sprintf(msg,"Blockchain height: %d",g_State->m_BlockChainHeight);
                    cs_LogMessage(g_Log,CS_LOG_MINOR,"C-0056","Processing mempool",msg);            
                    err=RemoveBlock(1);
                    if(err)
                    {
                        sprintf(msg,"Error: %08X",err);
                        cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0018","Cannot clear memory pool",msg);                  
                        goto errorlbl;
                    }
                    for(mempool_attempt=0;mempool_attempt<1;mempool_attempt++)
                    {
                        err=ProcessMemPool(new_op_return_file);
                        new_op_return_file=0;
                    }
                    if(err)
                    {
                        sprintf(msg,"Error: %08X",err);
                        cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0019","Error while processing mempool",msg);                  
//                        goto errorlbl;
                        err=RemoveBlock(1);
                    }
                }
            }

            if(g_State->m_AssetDB->m_lpHandlerShMem[CS_OFF_DB_SHMEM_HEAD_SIGNAL] == 0)
            {
                size=-1;
                while(size != g_State->m_BlockChainHeight)
                {
                    size=g_State->m_BlockChainHeight;
                    UpdateBlockChain();
                }
            }
        }    
        
        restore_block=-2;
        
errorlbl:

        if(err == CS_ERR_TIMEOUT)
        {
            restore_block=-2;
        }
                
        if(restore_block>=0)
        {
            sprintf(msg,"%d",restore_block-1);
            cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0105","Trying to restore from height",msg);
            err=cs_RestoreFromBackUp(&restore_block);
            if(err)
            {
                sprintf(msg,"Error: %08X",err);
                cs_LogMessage(g_Log,CS_LOG_FATAL,"C-0106","Cannot restore",msg);                  
                goto exitlbl;
            }
            else
            {
                sprintf(msg,"%d",restore_block);
                cs_LogMessage(g_Log,CS_LOG_REPORT,"C-0107","Restoring from height",msg);            
            }
        }
        else
        {
            restore_block=-2;
        }
    }
    
    err=RemoveBlock(1);

    
exitlbl:

    cs_LogMessage(g_Log,CS_LOG_SYSTEM,"C-0020","Tracker process, Exiting...","");


    if(g_Connection)
    {
        g_Connection->Disconnect();
    }

    if(hRecvThread)
    {
        __US_WaitForSingleThread(hRecvThread, INFINITE);
        hRecvThread=NULL;
        idRecvThread=0;
    }

    if(g_Semaphore)
    {
        __US_SemDestroy(g_Semaphore);
        g_Semaphore=NULL;
    }

    if(g_Connection)
    {
        delete g_Connection;
        g_Connection=NULL;
    }
 
    if(g_Message)
    {
        delete g_Message;
        g_Message=NULL;
    }

    if(lpRecvThreadData)
    {
        delete lpRecvThreadData;
    }
    

    if(g_State)
    {
        g_State->m_AssetDB=NULL;
        delete g_State;
        g_State=NULL;
    }

    cs_LogMessage(g_Log,CS_LOG_SYSTEM,"C-0021","Tracker process, Exited","");
    
    cs_LogDestroy(g_Log);
    
    g_Log=NULL;
    
    return err;
}


