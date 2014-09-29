/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#include "buffer.h"

void cs_Buffer::Zero()
{
	m_lpData=NULL;   
    m_AllocSize=0;
    m_Size=0;
    m_KeySize=0;
    m_RowSize=0;
    m_Count=0;
    m_Mode=0;    
}

cs_int32 cs_Buffer::Destroy()
{
    if(m_lpData)
    {
        cs_Delete(m_lpData,NULL,CS_ALT_DEFAULT);
    }
    
    Zero();
    
    return CS_ERR_NOERROR;
}

cs_int32 cs_Buffer::Initialize(cs_int32 KeySize,cs_int32 RowSize,cs_uint32 Mode)
{
    cs_int32 err;
    
    err=CS_ERR_NOERROR;
    
    Destroy();
    
    m_Mode=Mode;
    m_KeySize=KeySize;
    m_RowSize=RowSize;
    
    m_AllocSize=cs_AllocSize(1,CS_DCT_BUF_ALLOC_ITEMS,m_RowSize);
    
    m_lpData=(cs_uchar*)cs_New(m_AllocSize,NULL,CS_ALT_DEFAULT);
    if(m_lpData==NULL)
    {
        Zero();
        err=CS_ERR_ALLOCATION;
        return err;
    }
    
    return err;
}
    
cs_int32 cs_Buffer::Clear()
{
    m_Size=0;
    m_Count=0;
    
    return CS_ERR_NOERROR;
}

cs_int32 cs_Buffer::Add(void *lpKey,void *lpValue)
{
    cs_uchar *lpNewBuffer;
    cs_int32 NewSize;
    cs_int32 err;
    
    err=CS_ERR_NOERROR;
    
    if(m_Size+m_RowSize>m_AllocSize)
    {
        NewSize=cs_AllocSize(m_Count+1,CS_DCT_BUF_ALLOC_ITEMS,m_RowSize);
        lpNewBuffer=(cs_uchar*)cs_New(NewSize,NULL,CS_ALT_DEFAULT);
        
        if(lpNewBuffer==NULL)
        {
            err=CS_ERR_ALLOCATION;
            return err;
        }

        memcpy(lpNewBuffer,m_lpData,m_AllocSize);
        cs_Delete(m_lpData,NULL,CS_ALT_DEFAULT);

        m_AllocSize=NewSize;
        m_lpData=lpNewBuffer;                
    }
    
    if(m_KeySize)
    {
        memcpy(m_lpData+m_Size,lpKey,m_KeySize);
    }
    
    if(m_KeySize<m_RowSize)
    {
        memcpy(m_lpData+m_Size+m_KeySize,lpValue,m_RowSize-m_KeySize);
    }
    
    m_Size+=m_RowSize;
    m_Count++;
    
    return err;
}

cs_int32 cs_Buffer::PutRow(cs_int32 RowID,void *lpKey,void *lpValue)
{
    cs_uchar *ptr;
    
    if(RowID>=m_Count)
    {
        return CS_ERR_OUT_OF_RANGE;
    }
    
    ptr=m_lpData+m_RowSize*RowID;
    
    if(m_KeySize)
    {
        memcpy(ptr,lpKey,m_KeySize);
    }
    
    if(m_KeySize<m_RowSize)
    {
        memcpy(ptr+m_KeySize,lpValue,m_RowSize-m_KeySize);
    }

    return CS_ERR_NOERROR;
}


cs_int32 cs_Buffer::Seek(void *lpKey)
{
    cs_uchar *ptr;
    cs_int32 row;
    
    ptr=m_lpData;
    row=0;
    
    while(row<m_Count)
    {
        if(memcmp(ptr,lpKey,m_KeySize)==0)
        {
            return row;
        }
        ptr+=m_RowSize;
        row++;
    }
    
    return -1;
}


cs_uchar *cs_Buffer::GetRow(cs_int32 RowID)
{
    return m_lpData+m_RowSize*RowID;
}

cs_int32 cs_Buffer::GetCount()
{
    return m_Count;
}

void cs_Buffer::SetCount(cs_int32 count)
{
    m_Count=count;
    m_Size=count*m_RowSize;
}

void cs_Buffer::CopyFrom(cs_Buffer *source)
{
    Clear();
    int i;
    cs_uchar *ptr;
    
    for(i=0;i<source->GetCount();i++)    
    {
        ptr=source->GetRow(i);
        Add(ptr,ptr+m_KeySize);
    }
}

