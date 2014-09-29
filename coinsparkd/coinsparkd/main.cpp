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
#include "us_declare.h"
#include "coinspark.h"


cs_Arguments *g_Args;
cs_int32 cs_InitTracker();
cs_int32 cs_CheckTrackedAsset(cs_Database *db);
cs_int32 cs_Tracker(void *ThreadData);


int main(int argc, char** argv) 
{
    cs_int32 mode,err,argi,iprefix,start,assetargi;
    cs_char AssetDBName[CS_DCT_MAX_PATH];
    cs_Database *db;
	cs_handle hDBThread;
	cs_uint32 idDBThread;
	cs_handle hTrackerThread;
	cs_uint32 idTrackerThread;
    cs_handle hShMem;            
    cs_uint64 *lpShMem;
    CoinSparkAssetRef *assetRef;
    
    err=CS_ERR_NOERROR;
    
    g_Args=NULL;
    hDBThread=NULL;
    idDBThread=0;
    db=NULL;
    hShMem=NULL;     
    lpShMem=NULL;
    
    mode=0;
    
    g_Args=new cs_Arguments;
    
    g_Args->m_TrackerMode=CS_DCT_TRACKER_MODE_SINGLE;
        
    mode=1;
    iprefix=-2;
    
    argi=1;
    assetargi=0;
    
    while(argi<argc)
    {                
        if(strcmp(argv[argi],"start") == 0)
        {
            mode=1;
        }
        if(strcmp(argv[argi],"stop") == 0)
        {
            mode=2;
        }
        
        if(strcmp(argv[argi],"-testnet") == 0)
        {
            g_Args->m_BitcoinClientPort=18333;            
            strcpy(g_Args->m_BitcoinNetwork,"testnet3");            
        }        
        
        if(strcmp(argv[argi],"-d") == 0)
        {
            if(argi+1<argc)
            {
                argi++;
                strcpy(g_Args->m_DataDir,argv[argi]);
                strcat(g_Args->m_DataDir,"/");                
//                sprintf(g_Args->m_TrackerLogFile,"%stracker.log",g_Args->m_DataDir);
//                sprintf(g_Args->m_DBLogFile,"%sdb.log",g_Args->m_DataDir);
            }
        }        

        if(memcmp(argv[argi],"-asset=",7) == 0)
        {
            assetargi=argi;
            start=7;
            
            assetRef=new CoinSparkAssetRef;
            if(CoinSparkAssetRefDecode(assetRef,argv[argi]+start,strlen(argv[argi]+start)))
            {
                cs_PutNumberLittleEndian(g_Args->m_TrackedAsset,&(assetRef->blockNum),4,4);
                cs_PutNumberLittleEndian(g_Args->m_TrackedAsset+4,&(assetRef->txOffset),4,4);            
                memcpy(g_Args->m_TrackedAsset+8,assetRef->txIDPrefix,COINSPARK_ASSETREF_TXID_PREFIX_LEN);
                iprefix=1;
            }
            else
            {
                printf("Invalid asset reference %s\n",argv[argi]+start);
                return 0;                
            }
/*            
            iprefix=0;
            while(j<(cs_int32)strlen(argv[argi]))
            {
                if(argv[argi][j]=='-')
                {
                    if(block<0)
                    {
                        argv[argi][j]=0;
                        block=atoi(argv[argi]+start);
                        argv[argi][j]='-';
                    }
                    else
                    {
                        if(offset<0)
                        {
                            argv[argi][j]=0;
                            offset=atoi(argv[argi]+start);
                            argv[argi][j]='-';
                        }
                        else
                        {
                            iprefix=-1;
                        }                    
                    }
                    start=j+1;
                }
                j++;
            }
            
            if(iprefix>=0)
            {
                iprefix=-1;
                if(offset>0)
                {
                    if(start<(cs_int32)strlen(argv[argi]))
                    {
                        iprefix=atoi(argv[argi]+start);
                        cs_PutNumberLittleEndian(g_Args->m_TrackedAsset+8,&iprefix,2,sizeof(iprefix));
                    }
                }
            }

            cs_PutNumberLittleEndian(g_Args->m_TrackedAsset,&block,4,sizeof(block));
            cs_PutNumberLittleEndian(g_Args->m_TrackedAsset+4,&offset,4,sizeof(offset));            
 */ 
        }
        
        
        if(memcmp(argv[argi],"-datadir=",9) == 0)
        {
            strcpy(g_Args->m_DataDir,argv[argi]+9);
            strcat(g_Args->m_DataDir,"/");                
//            sprintf(g_Args->m_TrackerLogFile,"%stracker.log",g_Args->m_DataDir);
//            sprintf(g_Args->m_DBLogFile,"%sdb.log",g_Args->m_DataDir);            
        }
        
        if(strcmp(argv[argi],"-k") == 0)
        {
            argi++;
            g_Args->m_DBShMemKey=atoi(argv[argi]);
        }        

        if(memcmp(argv[argi],"-shmemkey=",10) == 0)
        {
            g_Args->m_DBShMemKey=atoi(argv[argi]+10);            
        }   
        
        if(strcmp(argv[argi],"-bitcoinip") == 0)
        {
            if(argi+1<argc)
            {
                argi++;
                strcpy(g_Args->m_BitcoinClientAddress,argv[argi]);
            }
        }        
        
        if(memcmp(argv[argi],"-bitcoinip=",11) == 0)
        {
            strcpy(g_Args->m_BitcoinClientAddress,argv[argi]+11);
        }   
        
        argi++;
    }

    if((iprefix < 0) && (mode==1))
    {
        printf("Usage: coinsparkd -asset=<asset-reference> [-testnet] [-datadir=<data_path>] [-shmem=<shmem_key>] [-bitcoinip=<IP of server running bitcoind>] [start | stop]\n");
        return 0;
    }
    
    err=cs_InitTracker();
    if(err)
    {
        printf("Cannot initialize tracker, error: %X\n",err);
        goto exitlbl;
    }
    
    switch(mode)
    {
        case 1:
            
            printf("Starting coinsparkd...\n");

            printf("\n");//Don't remove this printf, otherwise parent process doesn't exit on Linux 64 bit

            if(__US_BecomeDaemon())
            { 
                goto exitlbl;
            }

            db=NULL;
            db=new cs_Database;
            
            db->SetOption("ShMemKey",0,g_Args->m_DBShMemKey);
            db->SetOption("KeySize",0,48);
            db->SetOption("ValueSize",0,16);
            db->m_Log=cs_LogInitialize(g_Args->m_DBLogFile,-1,g_Args->m_TrackerLogFilter,g_Args->m_TrackerLogMode);
            
            sprintf(AssetDBName,"%s%s",g_Args->m_DataDir,CS_DCT_FILE_ASSETDB);
            err=db->Open(AssetDBName,CS_OPT_DB_DATABASE_CREATE_IF_MISSING | CS_OPT_DB_DATABASE_TRANSACTIONAL | CS_OPT_DB_DATABASE_LEVELDB);

            
            if(err)
            {
                printf("Cannot open database, probably coinsparkd server is already running\n");      
                goto exitlbl;                
            }

            err=cs_CheckTrackedAsset(db);
            
            if(err)
            {
                printf("Cannot start tracking server\n");      
                goto exitlbl;                                
            }
            
            hDBThread=__US_CreateThread(NULL,0,(void *)DBShMemReadServerLoop,(void *)db,THREAD_SET_INFORMATION,&idDBThread);
            if(hDBThread == NULL)
            {
                printf("Cannot start database server\n");      
                goto exitlbl;                
            }
            
            while(((db->m_Status & CS_STT_DB_DATABASE_SHMEM_OPENED) == 0) && (db->m_LastError == 0))
            {
                __US_Sleep(1);
            }
            
            if(db->m_LastError)
            {
                switch(db->m_LastError)
                {
                    case CS_ERR_DB_SHMEM_ALREADY_OPENED:
                        printf("Cannot start database server, probably server with shared memory key %d is already started\n",g_Args->m_DBShMemKey);
                        printf("If you are sure there is no other server using this shared memory key, please run \n\n");
                        printf("ipcrm -m %d\n\n",g_Args->m_DBShMemKey);
                        printf("and try starting the server again.\n");
                        break;
                    default:
                        printf("Cannot start database server, error: %08X\n",db->m_LastError);      
                        break;
                }
                goto exitlbl;
            }
            
            printf("Server tracking asset %s successfully started\n",argv[assetargi]+7);

            while(db->m_lpHandlerShMem[CS_OFF_DB_SHMEM_HEAD_SIGNAL] == 0)
            {
                hTrackerThread=__US_CreateThread(NULL,0,(void *)cs_Tracker,(void *)db,THREAD_SET_INFORMATION,&idTrackerThread);
                if(hTrackerThread == NULL)
                {
                    __US_Sleep(5000);
                }
                else
                {
                    __US_WaitForSingleThread(hTrackerThread, INFINITE);
                    hTrackerThread=NULL;
                    hTrackerThread=0;                    
                }                
                if(db->m_lpHandlerShMem[CS_OFF_DB_SHMEM_HEAD_SIGNAL] == 0)
                {
                    __US_Sleep(5000);
                }
            }
            db->m_Signal=1;
            
            break;
            
        case 2:
                        
            hShMem=NULL;            
            lpShMem=NULL;
    
            if(__US_ShMemOpen(&hShMem,g_Args->m_DBShMemKey))
            {
                printf("Cannot stop bitcoind, probably it is not running\n");
                goto exitlbl;
            }
    
            lpShMem=(cs_uint64*)__US_ShMemMap(hShMem);
            if(lpShMem == NULL)
            {
                printf("Cannot connect to bitcoind, probably it is not running\n");
                __US_ShMemClose(hShMem);
                hShMem=NULL;
                goto exitlbl;
            }

            lpShMem[CS_OFF_DB_SHMEM_HEAD_SIGNAL]=1;
            
            if(lpShMem)
            {
                __US_ShMemUnmap(lpShMem);
                lpShMem=NULL;
            }

            if(hShMem)
            {
                __US_ShMemClose(hShMem);
                hShMem=NULL;
            }
            
            break;
    }
    
exitlbl:

    if(hDBThread)
    {
        __US_WaitForSingleThread(hDBThread, INFINITE);
        hDBThread=NULL;
        idDBThread=0;
    }
            
    if(db)
    {
        if(db->m_Log)
        {
            cs_LogDestroy(db->m_Log);
            db->m_Log=NULL;
        }
        db->Close();
        delete db;
        db=NULL;
    }
            
    if(g_Args)
    {
        delete g_Args;
    }
    
    return 0;
}




