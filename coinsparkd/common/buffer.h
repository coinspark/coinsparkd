/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#ifndef _CS_BUFFER_H
#define	_CS_BUFFER_H

#include "declare.h"

#define CS_DCT_BUF_ALLOC_ITEMS          32768
#define CS_DCT_LIST_ALLOC_MIN_SIZE      32768
#define CS_DCT_LIST_ALLOC_MAX_SIZE      268435456

#ifdef	__cplusplus
extern "C" {
#endif


typedef struct cs_Buffer
{
    cs_Buffer()
    {
         Zero();
    }

    ~cs_Buffer()
    {
         Destroy();
    }

	cs_uchar           *m_lpData;   
    cs_int32            m_AllocSize;
    cs_int32            m_Size;
    cs_int32            m_KeySize;
    cs_int32            m_RowSize;
    cs_int32            m_Count;
    cs_uint32           m_Mode;
    
    void Zero();
    cs_int32 Destroy();
    cs_int32 Initialize(cs_int32 KeySize,cs_int32 TotalSize,cs_uint32 Mode);
    
    cs_int32 Clear();
    cs_int32 Add(void *lpKey,void *lpValue);
    cs_int32 Seek(void *lpKey);
    cs_uchar *GetRow(cs_int32 RowID);    
    cs_int32 PutRow(cs_int32 RowID,void *lpKey,void *lpValue);    
    cs_int32 GetCount();    
    void SetCount(cs_int32 count);
    
    void CopyFrom(cs_Buffer *source);
    
} cs_Buffer;

    
typedef struct cs_List
{
    cs_List()
    {
         Zero();
    }

    ~cs_List()
    {
         Destroy();
    }

    cs_uchar           *m_lpData;   
    cs_int32            m_AllocSize;
    cs_int32            m_Size;
    cs_int32            m_Pos;
    cs_int32            m_ItemSize;
    
    void Zero();
    cs_int32 Destroy();
    
    void Clear();
    cs_int32 Put(cs_uchar *ptr, cs_int32 size);
    cs_uchar *First();
    cs_uchar *Next();
    
} cs_List;


#ifdef	__cplusplus
}
#endif

#endif	/* _CS_BUFFER_H */

