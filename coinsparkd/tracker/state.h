/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#ifndef _CS_STATE_H
#define	_CS_STATE_H

#include "declare.h"
#include "buffer.h"
#include "net.h"
#include "db.h"
#include "bitcoin.h"

#include "coinspark.h"


#ifdef	__cplusplus
extern "C" {
#endif

typedef struct cs_State
{    
    cs_State()
    {
        InitDefaults();
    }    

    ~cs_State()
    {
        Destroy();
    }

    cs_int32        m_BlockChainHeight;
    cs_uchar        *m_BlockChain;
    cs_int32        m_BlockChainAllocSize;
    cs_uchar        *m_ResponseMessage;
    cs_int32        m_ResponseMessageAllocSize;
    cs_int32        m_AssetStateCount;
    cs_uchar        *m_AssetState;
    cs_int32        m_AssetStateAllocSize;
    cs_Buffer       *m_TxOutAdd;
    cs_Buffer       *m_TxOutRemove;
    cs_Buffer       *m_MemPoolOpReturns;
    cs_Buffer       *m_MemPoolOpReturnsLast;
    cs_Database     *m_AssetDB;

    cs_Buffer       *m_TxAssetMatrix;
    cs_Buffer       *m_TxAssetList;
    cs_Buffer       *m_TxAssetTransfers;
    cs_Buffer       *m_TxOutputSatoshis;
    cs_Buffer       *m_TxOutputRegulars;
    
    cs_int32        m_TxInputCount;
    cs_int32        m_TxOutputCount;
    cs_int32        m_TxAssetCount;
    cs_int64        m_TxSatoshiFee;
    
    cs_int32        m_BlockInsertCount;
    cs_int32        m_BlockUpdateCount;
    cs_int32        m_BlockDeleteCount;
    
    void  InitDefaults()
    {
        m_BlockChainHeight=0;
        m_BlockChain=NULL;
        m_BlockChainAllocSize=0;
        m_ResponseMessage=NULL;
        m_ResponseMessageAllocSize=0;
        m_AssetStateCount=0;
        m_AssetState=NULL;
        m_AssetStateAllocSize=0;
        m_TxOutAdd=NULL;
        m_TxOutRemove=NULL;
        m_MemPoolOpReturns=NULL;        
        m_MemPoolOpReturnsLast=NULL;        
        m_AssetDB=NULL;
        
        m_TxAssetMatrix=NULL;
        m_TxAssetList=NULL;
        m_TxAssetTransfers=NULL;
        m_TxOutputSatoshis=NULL;
        m_TxOutputRegulars=NULL;
        
        m_TxInputCount=0;
        m_TxOutputCount=0;
        m_TxAssetCount=0;
        m_TxSatoshiFee=0;
    
        m_BlockInsertCount=0;
        m_BlockUpdateCount=0;
        m_BlockDeleteCount=0;
    }
    
    void  Destroy()
    {
        if(m_BlockChain)
        {
            cs_Delete(m_BlockChain,NULL,CS_ALT_DEFAULT);    
        }
        if(m_ResponseMessage)
        {
            cs_Delete(m_ResponseMessage,NULL,CS_ALT_DEFAULT);    
        }
        if(m_TxOutAdd)
        {
            delete m_TxOutAdd;
        }
        if(m_TxOutRemove)
        {
            delete m_TxOutRemove;
        }
        if(m_MemPoolOpReturns)
        {
            delete m_MemPoolOpReturns;            
        }
        if(m_MemPoolOpReturnsLast)
        {
            delete m_MemPoolOpReturnsLast;            
        }
            
        if(m_TxAssetMatrix)
        {
            delete m_TxAssetMatrix;
        }
        if(m_TxAssetList)
        {
            delete m_TxAssetList;
        }
        if(m_TxAssetTransfers)
        {
            delete m_TxAssetTransfers;
        }
        if(m_TxOutputSatoshis)
        {
            delete m_TxOutputSatoshis;
        }
        if(m_TxOutputRegulars)
        {
            delete m_TxOutputRegulars;
        }
        if(m_AssetDB)
        {
            delete m_AssetDB;
        }

        if(m_AssetState)
        {
            cs_Delete(m_AssetState,NULL,CS_ALT_DEFAULT);    
        }
        
        InitDefaults();
    }
    
} cs_State;


#ifdef	__cplusplus
}
#endif



#endif	/* _CS_STATE_H */

