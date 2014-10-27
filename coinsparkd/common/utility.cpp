/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#include "define.h"
#include "net.h"

cs_uchar c_IsHexNumeric[256]={
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 1,2,3,4,5,6,7,8,9,10,0,0,0,0,0,0,
 0,11,12,13,14,15,16,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,11,12,13,14,15,16,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

cs_uint32 c_BitMask[33]={0x00000000,
                   0x00000001,0x00000003,0x00000007,0x0000000F,0x0000001F,0x0000003F,0x0000007F,0x000000FF,
                   0x000001FF,0x000003FF,0x000007FF,0x00000FFF,0x00001FFF,0x00003FFF,0x00007FFF,0x0000FFFF,
                   0x0001FFFF,0x0003FFFF,0x0007FFFF,0x000FFFFF,0x001FFFFF,0x003FFFFF,0x007FFFFF,0x00FFFFFF,
                   0x01FFFFFF,0x03FFFFFF,0x07FFFFFF,0x0FFFFFFF,0x1FFFFFFF,0x3FFFFFFF,0x7FFFFFFF,0xFFFFFFFF};


cs_double __stdcall cs_TimeNow()
{
    
    struct timeval time_now;

    cs_double num_time;
    
    gettimeofday(&time_now,NULL);
    
    num_time=(cs_double)(time_now.tv_sec)+0.000001f*((cs_double)(time_now.tv_usec));
 
    
    return num_time;
}



void __stdcall cs_MemoryDump(void *ptr, 
                cs_int32 from,         
                cs_int32 len)          
{
    cs_uchar *dptr;
    cs_int32 i,j,n,k;
    cs_char str[17];
    
    dptr=(cs_uchar *)ptr+from;
    
    k=from;
    n=(len-1)/16+1;
    
    str[16]=0;
    
    for(i=0;i<n;i++)
    {
        printf("%04X %08X: ",k,(cs_uint32)(cs_uint64)dptr);
        memset(str,'.',16);
        for(j=0;j<16;j++)
        {
            if(k<len)
            {
                printf("%02X  ",*dptr);            
                if((*dptr>=0x20) && (*dptr<0x7F))
                {
                    str[j]=(cs_char)(*dptr);
                }
            }
            else
            {
                printf("    ");            
            }
            
            dptr++;
            k++;
            if(k%4==0)
            {
                printf(" ");
            }
        }        
        printf("    %s \n",str);            
    }        
}

void __stdcall cs_PutNumberLittleEndian(void *dest,void *src,cs_int32 dest_size,cs_int32 src_size)
{
    memcpy(dest,src,dest_size);                                                 // Assuming all systems are little endian
}

cs_uint64 __stdcall cs_GetUInt64LittleEndian(void *src,cs_int32 size)
{
    cs_int64 result;
    cs_uchar *ptr;
    cs_uchar *ptrEnd;
    cs_int32 shift;
    
    ptr=(cs_uchar*)src;
    ptrEnd=ptr+size;
    
    result=0;
    shift=0;
    while(ptr<ptrEnd)
    {
        result|=((cs_int64)(*ptr))<<shift;
        shift+=8;
        ptr++;
    }
    
    return result;                                                              // Assuming all systems are little endian
}

cs_uint64 __stdcall cs_GetUInt64LittleEndian32(void *src)
{
    return *(cs_int32*)src;
}

cs_uint64 __stdcall cs_GetUInt64LittleEndian64(void *src)
{
    return *(cs_int64*)src;
}

void __stdcall cs_PutNumberBigEndian(void *dest,void *src,cs_int32 dest_size,cs_int32 src_size)
{
    cs_uint64 value;                                                            // Assuming all systems are little endian
    cs_int32 i;
    
    value=0;
    switch(src_size)
    {
        case 2:
            value=(cs_uint64)(*(cs_uint16*)src);
            break;
        case 4:
            value=(cs_uint64)(*(cs_uint32*)src);
            break;
        case 8:
            value=(cs_uint64)(*(cs_uint64*)src);
            break;
    }
    
    for(i=dest_size-1;i>=0;i--)
    {
        *((cs_char*)dest+i)=value%256;
        value/=256;
    }
}

cs_int32 __stdcall cs_AllocSize(cs_int32 items,cs_int32 chunk_size,cs_int32 item_size)
{
    return ((items-1)/chunk_size+1)*chunk_size*item_size;
}

void cs_SwapBytes(cs_uchar *dest,cs_uchar *src,cs_int32 size)
{
    cs_int32 i;
    for(i=0;i<size;i++)
    {
        dest[i]=src[size-1-i];
    }
}

cs_uchar cs_HexToUChar(cs_char *Hex)
{
    cs_uchar r,s;
    
    r=0;
    s=c_IsHexNumeric[(cs_int32)(Hex[0])];
    if(s)r+=s-1;
    r*=16;
    s=c_IsHexNumeric[(cs_int32)(Hex[1])];
    if(s)r+=s-1;
    return r;
}

cs_uint32 cs_GetUInt32BitsLittleEndian(void *src,cs_int32 bits)
{
    cs_uint32 result;
    
    result=(cs_uint32)cs_GetUInt64LittleEndian(src,4);
    
    return result & c_BitMask[bits];
}

cs_int32 cs_TestSubFolder(cs_char *dest,cs_int32 size,cs_char *src,cs_char *subfolder,cs_int32 create)
{
    if(strlen(src)+strlen(subfolder)+2 > (cs_uint32)size)
    {
        return CS_ERR_FILENAME_TOO_LONG;
    }
    
    strcpy(dest,src);
    strcat(dest,subfolder);
    if(dest[strlen(dest)-1] != CS_DCT_FOLDERSEPARATOR)
    {
        dest[strlen(dest)]=CS_DCT_FOLDERSEPARATOR;
        dest[strlen(dest)+1]=0x00;
    }
    
    struct stat st = {0};

    if (stat(dest, &st) == -1) 
    {
        if(create)
        {
            mkdir(dest, 0755);
            if (stat(dest, &st) == -1) 
            {
                return CS_ERR_FILE_NOT_FOUND;                
            }
        }
        else
        {
            return CS_ERR_FILE_NOT_FOUND;
        }
    }    
    
    return CS_ERR_NOERROR;
}