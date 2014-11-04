/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#ifndef _CS_ARGS_H
#define	_CS_ARGS_H

#include "declare.h"

#define CS_DCT_TRACKER_MODE_FULL                    0
#define CS_DCT_TRACKER_MODE_SINGLE                  1


#define CS_DCT_FILE_ASSETDB                 "coinsparkdb" 
#define CS_DCT_FILE_BLOCKCHAIN              "blockchain.dat" 
#define CS_DCT_FILE_ASSETSTATE              "assetstate.dat" 
#define CS_DCT_FOLDER_NEW_TXOUTS            "new_txouts" 
#define CS_DCT_FOLDER_SPENT_TXOUTS          "spent_txouts" 
#define CS_DCT_FOLDER_OP_RETURN             "op_returns" 
#define CS_DCT_FOLDER_BACKUP                "backup" 
#define CS_DCT_DATA_FOLDER                  ".coinspark/" 
#define CS_DCT_DATA_ASSETS_FOLDER           "assets/" 
#define CS_DCT_DATA_TESTNET_FOLDER          "testnet3/" 
#define CS_DCT_DATA_ALL_ASSETS_FOLDER       "all/" 
#define CS_DCT_MEMPOOL_BLOCK                1234567890 

#define CS_DCT_ASSET_ID_SIZE                     12
#define CS_DCT_CDB_KEY_SIZE                      48
#define CS_DCT_CDB_VALUE_SIZE                    16

#define CS_DCT_ASSET_STATE_SIZE                 128

#define CS_OFF_ASSET_STATE_ID                     0
#define CS_OFF_ASSET_STATE_NEXT_BLOCK            16
#define CS_OFF_ASSET_STATE_METHOD                20
#define CS_OFF_ASSET_STATE_STATE                 24
#define CS_OFF_ASSET_STATE_ASSET_COUNT           28
#define CS_OFF_ASSET_STATE_METADATA              48
#define CS_DCT_ASSET_METADATA_SIZE               80


#define CS_DCT_BLOCK_PER_FOLDER                1000
#define CS_DCT_BLOCK_PURGE_DELAY               1008
#define CS_DCT_BLOCKS_CHUNK                      20
#define CS_DCT_TXS_CHUNK                        500

/* DB special record codes */

#define CS_DCT_CDB_GENESIS_TX_ASSET_MAP          0x01
#define CS_DCT_CDB_GENESIS_METADATA              0x02
#define CS_DCT_CDB_BLOCK_BY_HASH                 0x03
#define CS_DCT_CDB_BLOCK_BY_INDEX                0x04
#define CS_DCT_CDB_LAST_BLOCK                    0x80
#define CS_DCT_CDB_TRACKED_ASSET                 0x81
#define CS_DCT_CDB_ALL_ASSETS                    0x82



#ifdef	__cplusplus
extern "C" {
#endif

typedef struct cs_Arguments
{    
    cs_Arguments()
    {
        InitDefaults();
    }    

    ~cs_Arguments()
    {
    }

    cs_char         m_BitcoinNetwork[16];
    cs_char         m_BitcoinClientAddress[16];
    cs_uint32       m_BitcoinClientPort;
    cs_char         m_ConfigFile[CS_DCT_MAX_PATH];
    cs_char         m_TrackerLogFile[CS_DCT_MAX_PATH];
    cs_char         m_DBLogFile[CS_DCT_MAX_PATH];
    cs_uint32       m_TrackerLogFilter;
    cs_uint32       m_TrackerLogMode;
    cs_int32        m_DBShMemKey;
    cs_char         m_DataDir[CS_DCT_MAX_PATH];
    cs_char         m_GenesisBlock[65];    

    cs_uchar        m_TrackedAsset[CS_DCT_ASSET_ID_SIZE];
    cs_int32        m_TrackerMode;
    cs_int32        m_StoreOpReturnsBlock;
    
    void  InitDefaults()
    {
        strcpy(m_BitcoinNetwork,"main");
        strcpy(m_BitcoinClientAddress,"127.0.0.1");
        m_BitcoinClientPort=8333;
/*        
        realpath("../../coinspark_data/",m_DataDir);
        strcat(m_DataDir,"/");
  */
        
        
        strcpy(m_DataDir,"");
/*        
        strcpy(m_TrackerLogFile,m_DataDir);
        strcat(m_TrackerLogFile,"tracker.log");
        strcpy(m_DBLogFile,m_DataDir);
        strcat(m_DBLogFile,"db.log");
  */      

        m_TrackerLogFilter=CS_LOG_FATAL | CS_LOG_ERROR | CS_LOG_WARNING | CS_LOG_SYSTEM | CS_LOG_REPORT | CS_LOG_DEBUG | CS_LOG_NO_CODE;// | CS_LOG_MINOR;
        m_TrackerLogMode=0;
        strcpy(m_GenesisBlock,"");
        
        m_DBShMemKey=0x3133;
        
        memset(m_TrackedAsset,0,CS_DCT_ASSET_ID_SIZE);
        
        m_TrackerMode=CS_DCT_TRACKER_MODE_FULL;
        m_StoreOpReturnsBlock=CS_DCT_MEMPOOL_BLOCK+1;
    }
    
} cs_Arguments;


#ifdef	__cplusplus
}
#endif



#endif	/* _CS_ARGS_H */

