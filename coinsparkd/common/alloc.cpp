/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#include "declare.h"

void __stdcall *cs_New(cs_int Size, void *Pool, cs_int32 Option)
{   
    cs_int TSize;
    cs_int32 TOption;
    void *ptr;

    TOption=Option;
    if(TOption == 0)
    {
        TOption = CS_ALT_DEFAULT;
    }
    ptr=NULL;

    switch(Option & CS_ALT_ALIGNMENTMASK)
    {
        
        case CS_ALT_NOALIGNMENT:
            TSize=Size;
            ptr=new cs_uchar[(cs_size_t)Size];
            break;
            
        case CS_ALT_INT32:
            TSize=(Size-1)/sizeof(cs_uint32)+1;
            ptr=new cs_uint32[(cs_size_t)TSize];
            break;
            
        case CS_ALT_INT64:
            TSize=(Size-1)/sizeof(cs_uint64)+1;
            ptr=new cs_uint64[(cs_size_t)TSize];
            break;
            
        case CS_ALT_INT:
            TSize=(Size-1)/sizeof(cs_int)+1;
            ptr=new cs_int[(cs_size_t)TSize];
            break;
    }

    if(ptr)
    {
        if(Option & CS_ALT_ZEROINIT)
        {
            memset(ptr,0,(cs_size_t)Size);
        }
    }

    return ptr;
}

void __stdcall cs_Delete(void *ptr, void *Pool, cs_int32 Option)
{
    cs_int32 TOption;

    TOption=Option;
    if(TOption == 0)
    {
        TOption = CS_ALT_DEFAULT;
    }
    
    if(ptr)
    {
        switch(Option & CS_ALT_ALIGNMENTMASK)
        {
            
            case CS_ALT_NOALIGNMENT:
                delete [] (cs_uchar*)ptr;
                break;
                
            case CS_ALT_INT32:
                delete [] (cs_uint32*)ptr;
                break;
                
            case CS_ALT_INT64:
                delete [] (cs_uint64*)ptr;
                break;
                
            case CS_ALT_INT:
                delete [] (cs_uint*)ptr;
                break;
                
        }
    }
    
}


