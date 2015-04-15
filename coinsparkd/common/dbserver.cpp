/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#include "db.h"
#include "us_declare.h"

void cs_Database::Lock(cs_int32 write_mode)
{    
    __US_SemWait(m_Semaphore); 
}

void cs_Database::UnLock()
{    
    __US_SemPost(m_Semaphore);
}

cs_int32 cs_Database::BatchRead(cs_char *Data,cs_int32 *results,cs_int32 count,cs_int32 Options)
{
    cs_int32 err,row,row_size,key_size;
    cs_uint64 *rptr;
    cs_char *key;
    cs_char *ptr;
    cs_double start_time;
    
    err=CS_ERR_NOERROR;
    
    switch(m_Options & CS_OPT_DB_DATABASE_TYPE_MASK)
    {
        case CS_OPT_DB_DATABASE_REMOTE_SHMEM:    
            if(m_lpShMemSlot == NULL)
            {
                err=CS_ERR_DB_NOT_OPENED;
                return err;
            }
            
            if(count<=0)
            {
                return CS_ERR_NOERROR;
            }
            
            row_size=m_lpShMem[CS_OFF_DB_SHMEM_HEAD_ROW_SIZE]/sizeof(cs_uint64);
            key_size=m_lpShMem[CS_OFF_DB_SHMEM_HEAD_KEY_SIZE];
            
            m_lpShMemSlot[CS_OFF_DB_SHMEM_SLOT_ROW_COUNT]=0;
            m_lpShMemSlot[CS_OFF_DB_SHMEM_SLOT_RESULT]=CS_PRT_DB_SHMEM_SLOT_RESULT_UNDEFINED;            
            rptr=m_lpShMemSlot+m_lpShMem[CS_OFF_DB_SHMEM_HEAD_SLOT_DATA_OFFSET]/sizeof(cs_uint64);
            ptr=Data;
            for(row=0;row<count;row++)
            {
                rptr[CS_OFF_DB_SHMEM_ROW_RESPONSE]=CS_PRT_DB_SHMEM_ROW_RESPONSE_UNDEFINED;
                rptr[CS_OFF_DB_SHMEM_ROW_REQUEST]=CS_PRT_DB_SHMEM_ROW_REQUEST_READ;
                rptr[CS_OFF_DB_SHMEM_ROW_KEY_SIZE]=m_KeySize;
                key=(cs_char*)(rptr+CS_OFF_DB_SHMEM_ROW_DATA);
                memcpy(key,ptr,m_KeySize);
                ptr+=m_KeySize+m_ValueSize;
                rptr+=row_size;
            }
            
            m_lpShMemSlot[CS_OFF_DB_SHMEM_SLOT_ROW_COUNT]=count;
            start_time=cs_TimeNow();
            while((m_lpShMemSlot[CS_OFF_DB_SHMEM_SLOT_RESULT] == CS_PRT_DB_SHMEM_SLOT_RESULT_UNDEFINED) && (cs_TimeNow()-start_time<CS_DCT_DB_DEFAULT_MAX_SHMEM_TIMEOUT))
            {
                __US_Sleep(1);
            }
            
            if(m_lpShMemSlot[CS_OFF_DB_SHMEM_SLOT_RESULT] == CS_PRT_DB_SHMEM_SLOT_RESULT_ERROR)
            {
                return CS_ERR_DB_READ;
            }
            
            rptr=m_lpShMemSlot+m_lpShMem[CS_OFF_DB_SHMEM_HEAD_SLOT_DATA_OFFSET]/sizeof(cs_uint64);
            ptr=Data;
            for(row=0;row<count;row++)
            {
                results[row]=rptr[CS_OFF_DB_SHMEM_ROW_RESPONSE];
                key=(cs_char*)(rptr+CS_OFF_DB_SHMEM_ROW_DATA);
                memcpy(ptr+m_KeySize,key+key_size,m_ValueSize);
                ptr+=m_KeySize+m_ValueSize;
                rptr+=row_size;
            }            
            
            m_lpShMemSlot[CS_OFF_DB_SHMEM_SLOT_ROW_COUNT]=0;
            m_lpShMemSlot[CS_OFF_DB_SHMEM_SLOT_RESULT]=CS_PRT_DB_SHMEM_SLOT_RESULT_UNDEFINED;            
            
            break;        
        default:
            err=CS_ERR_DB_OPEN;
            break;
    }    
    
    return err;    
}


cs_int32 DBShMemReadServerLoop(void *ThreadData)
{
    cs_int32 err,shmem_size,row_size,slot_size,slot,last_slot,work,count,row,rerr,vlen,nncount;   
    cs_double start_time,this_time;
    cs_handle shmem;
    cs_uint64 *lpShMem;
    cs_uint64 *lpHead;
    cs_uint64 *lpSlots;
    cs_uint64 *uiptr;
    cs_uint64 *rptr;
    cs_uint64 pid;
    cs_Database *db;
    cs_char *lpRead;
    cs_char *lpKey;
    cs_char msg[100];
    
    cs_double tBef,tAft,tTotGen,tTotWork;
    cs_int32 cGen,cWork;

    
    err=CS_ERR_NOERROR;
    
    db=(cs_Database*)ThreadData;
    
    if(db->m_ShMemKey == 0)
    {
        err=CS_ERR_DB_SHMEM_KEY_NOT_SET;
        db->m_LastError=err;
        return err;
    }
    
    shmem=NULL;
    lpShMem=NULL;
    
    row_size=((db->m_KeySize+db->m_ValueSize-1)/(2*sizeof(cs_uint64))+1)*2*sizeof(cs_uint64)+4*sizeof(cs_uint64);
    slot_size=CS_OFF_DB_SHMEM_SLOT_DATA*sizeof(cs_uint64);
    slot_size+=db->m_MaxRows*row_size;
    shmem_size=CS_OFF_DB_SHMEM_HEAD_FIELD_COUNT*sizeof(cs_uint64);
    shmem_size+=db->m_MaxClients*slot_size;
    
    if(__US_ShMemOpen(&shmem,db->m_ShMemKey) == 0)
    {
        if(shmem)
        {
            __US_ShMemClose(shmem);
        }
        err=CS_ERR_DB_SHMEM_ALREADY_OPENED;
        goto exitlbl;
    }
    
    if(__US_ShMemCreate(&shmem,db->m_ShMemKey,shmem_size))
    {
        err=CS_ERR_DB_SHMEM_CANNOT_CREATE;
        goto exitlbl;
    }
    
    lpShMem=(cs_uint64*)__US_ShMemMap(shmem);
    if(lpShMem == NULL)
    {
        err=CS_ERR_DB_SHMEM_CANNOT_CREATE;
        goto exitlbl;
    }

    db->m_lpHandlerShMem=lpShMem;
    
    memset(lpShMem,0,shmem_size);
    
    lpHead=lpShMem;
    lpSlots=lpShMem+CS_OFF_DB_SHMEM_HEAD_FIELD_COUNT;
    
    db->m_Semaphore=__US_SemCreate();
    if(db->m_Semaphore == NULL)
    {
        err=CS_ERR_CANNOT_CREATE_SEMAPHORE;
        goto exitlbl;        
    }
    
    lpHead[CS_OFF_DB_SHMEM_HEAD_STATUS]=0;
    lpHead[CS_OFF_DB_SHMEM_HEAD_SIGNAL]=0;
    lpHead[CS_OFF_DB_SHMEM_HEAD_CLIENT_COUNT]=0;
    lpHead[CS_OFF_DB_SHMEM_HEAD_MAX_CLIENT_COUNT]=db->m_MaxClients;
    lpHead[CS_OFF_DB_SHMEM_HEAD_REQUEST_PROCESS_ID]=0;
    lpHead[CS_OFF_DB_SHMEM_HEAD_RESPONSE_PROCESS_ID]=0;
    lpHead[CS_OFF_DB_SHMEM_HEAD_RESPONSE_SLOT_ID]=0;
    lpHead[CS_OFF_DB_SHMEM_HEAD_KEY_SIZE]=db->m_KeySize;
    lpHead[CS_OFF_DB_SHMEM_HEAD_VALUE_SIZE]=db->m_ValueSize;
    lpHead[CS_OFF_DB_SHMEM_HEAD_ROW_SIZE]=row_size;
    lpHead[CS_OFF_DB_SHMEM_HEAD_SLOT_SIZE]=slot_size;
    lpHead[CS_OFF_DB_SHMEM_HEAD_MAX_ROWS]=db->m_MaxRows;
    lpHead[CS_OFF_DB_SHMEM_HEAD_DATA_OFFSET]=CS_OFF_DB_SHMEM_HEAD_FIELD_COUNT*sizeof(cs_uint64);
    lpHead[CS_OFF_DB_SHMEM_HEAD_SLOT_DATA_OFFSET]=CS_OFF_DB_SHMEM_SLOT_DATA*sizeof(cs_uint64);
    lpHead[CS_OFF_DB_SHMEM_HEAD_ROW_DATA_OFFSET]=CS_OFF_DB_SHMEM_ROW_DATA*sizeof(cs_uint64);
    
    lpHead[CS_OFF_DB_SHMEM_HEAD_STATUS]=1;
    
    if(db->m_Options & CS_OPT_DB_DATABASE_DELAYED_OPEN)
    {
        err=db->Open(db->m_Name,db->m_Options - CS_OPT_DB_DATABASE_DELAYED_OPEN);
        if(err)
        {
            goto exitlbl;
        }
    }
    
    
    db->m_Status |= CS_STT_DB_DATABASE_SHMEM_OPENED;
    
    cs_LogMessage(db->m_Log,CS_LOG_SYSTEM,"C-0078","SHMEM Read server started","");
    
    tTotGen=0;
    tTotWork=0;
    cGen=0;
    cWork=0;
    
    last_slot=-1;
    while(db->m_Signal == 0)
    {
        tBef=cs_TimeNow();
        work=0;
        start_time=cs_TimeNow();
        
        pid=lpHead[CS_OFF_DB_SHMEM_HEAD_REQUEST_PROCESS_ID];
        if(pid)
        {
            if(lpHead[CS_OFF_DB_SHMEM_HEAD_RESPONSE_PROCESS_ID]==0)
            {
                if(lpHead[CS_OFF_DB_SHMEM_HEAD_CLIENT_COUNT]<lpHead[CS_OFF_DB_SHMEM_HEAD_MAX_CLIENT_COUNT])
                {
                    uiptr=lpSlots;
                    slot=0;
                    while(uiptr[CS_OFF_DB_SHMEM_SLOT_PROCESS_ID])
                    {
                        uiptr+=slot_size/sizeof(cs_uint64);
                        slot++;
                    }
                    lpHead[CS_OFF_DB_SHMEM_HEAD_CLIENT_COUNT]++;
                    uiptr[CS_OFF_DB_SHMEM_SLOT_PROCESS_ID]=pid;
                    lpHead[CS_OFF_DB_SHMEM_HEAD_RESPONSE_SLOT_ID]=slot;
                    lpHead[CS_OFF_DB_SHMEM_HEAD_RESPONSE_PROCESS_ID]=pid;
                    work++;
                    sprintf(msg,"Slot: %3d; PID: %6d;",slot,(cs_int32)pid);
                    cs_LogMessage(db->m_Log,CS_LOG_REPORT,"C-0079","New connection",msg);
                }
            }
        }
        
        slot=last_slot;
        count=0;
        
        uiptr=lpSlots+last_slot;
        while(count<db->m_MaxClients)
        {
            count++;
            slot++;
            if(slot>=db->m_MaxClients)
            {
                slot=0;
            }
            uiptr=lpHead+CS_OFF_DB_SHMEM_HEAD_FIELD_COUNT+(slot_size/sizeof(cs_uint64))*slot;
            
            if(uiptr[CS_OFF_DB_SHMEM_SLOT_ROW_COUNT])
            {                
                if(uiptr[CS_OFF_DB_SHMEM_SLOT_RESULT]==CS_PRT_DB_SHMEM_SLOT_RESULT_UNDEFINED)
                {
                    rptr=uiptr+CS_OFF_DB_SHMEM_SLOT_DATA;
                    db->Lock(0);
                    nncount=0;
                    lpRead=NULL;
                    for(row=0;row<(cs_int32)(uiptr[CS_OFF_DB_SHMEM_SLOT_ROW_COUNT]);row++)
                    {
                        switch(rptr[CS_OFF_DB_SHMEM_ROW_REQUEST])
                        {
                            case CS_PRT_DB_SHMEM_ROW_REQUEST_READALL:
                                lpKey=(cs_char*)(rptr+CS_OFF_DB_SHMEM_ROW_DATA);
                                if(row==0)
                                {                                    
                                    nncount=0;
                                    lpRead=db->Read(lpKey,rptr[CS_OFF_DB_SHMEM_ROW_KEY_SIZE],&vlen,CS_OPT_DB_DATABASE_SEEK_ON_READ,&rerr);
                                }
                                else
                                {
                                    if(lpRead)
                                    {
                                        lpRead=db->MoveNext(&rerr);
                                    }
                                    if(lpRead)
                                    {
                                        memcpy(lpKey,lpRead,db->m_KeySize);
                                        if(memcmp(lpKey-row_size,lpRead,uiptr[CS_OFF_DB_SHMEM_SLOT_FIXEDKEYSIZE]))
                                        {
                                            lpRead=NULL;
                                        }
                                        else
                                        {
                                            lpRead+=db->m_KeySize;
                                        }
                                    }          
                                    else
                                    {
                                        memset(lpKey,0x7f,db->m_KeySize);
                                    }
                                }
                                rptr[CS_OFF_DB_SHMEM_ROW_VALUE_SIZE]=vlen;
                                if(rerr)
                                {
                                    rptr[CS_OFF_DB_SHMEM_ROW_RESPONSE]=CS_PRT_DB_SHMEM_ROW_RESPONSE_ERROR;
                                }
                                else
                                {
                                    
                                    if(lpRead)
                                    {
                                        nncount++;
                                        rptr[CS_OFF_DB_SHMEM_ROW_RESPONSE]=CS_PRT_DB_SHMEM_ROW_RESPONSE_NOT_NULL;
                                        memcpy(lpKey+db->m_KeySize,lpRead,vlen);
                                    }
                                    else
                                    {
                                        rptr[CS_OFF_DB_SHMEM_ROW_RESPONSE]=CS_PRT_DB_SHMEM_ROW_RESPONSE_NULL;                                        
                                    }
                                }
                                if(row == ((cs_int32)(uiptr[CS_OFF_DB_SHMEM_SLOT_ROW_COUNT])-1))
                                {
                                    sprintf(msg,"Slot: %3d; PID: %6d; Rows: %3d;",slot,(cs_int32)(uiptr[CS_OFF_DB_SHMEM_SLOT_PROCESS_ID]),nncount);
                                    cs_LogMessage(db->m_Log,CS_LOG_REPORT,"C-0108","Read all request",msg);                          
                                }
                                break;
                            case CS_PRT_DB_SHMEM_ROW_REQUEST_READ:
                                lpKey=(cs_char*)(rptr+CS_OFF_DB_SHMEM_ROW_DATA);
                                lpRead=db->Read(lpKey,rptr[CS_OFF_DB_SHMEM_ROW_KEY_SIZE],&vlen,0,&rerr);
                                rptr[CS_OFF_DB_SHMEM_ROW_VALUE_SIZE]=vlen;
                                if(rerr)
                                {
                                    rptr[CS_OFF_DB_SHMEM_ROW_RESPONSE]=CS_PRT_DB_SHMEM_ROW_RESPONSE_ERROR;
                                }
                                else
                                {
                                    if(lpRead)
                                    {
                                        rptr[CS_OFF_DB_SHMEM_ROW_RESPONSE]=CS_PRT_DB_SHMEM_ROW_RESPONSE_NOT_NULL;
                                        memcpy(lpKey+db->m_KeySize,lpRead,vlen);
                                    }
                                    else
                                    {
                                        rptr[CS_OFF_DB_SHMEM_ROW_RESPONSE]=CS_PRT_DB_SHMEM_ROW_RESPONSE_NULL;                                        
                                    }
                                }
                                if(row==0)
                                {
                                    sprintf(msg,"Slot: %3d; PID: %6d; Rows: %3d;",slot,(cs_int32)(uiptr[CS_OFF_DB_SHMEM_SLOT_PROCESS_ID]),(cs_int32)(uiptr[CS_OFF_DB_SHMEM_SLOT_ROW_COUNT]));
                                    cs_LogMessage(db->m_Log,CS_LOG_REPORT,"C-0080","Read request",msg);                                    
                                }
                                break;
                            case CS_PRT_DB_SHMEM_ROW_REQUEST_CLOSE:
                                sprintf(msg,"Slot: %3d; PID: %6d;",slot,(cs_int32)(uiptr[CS_OFF_DB_SHMEM_SLOT_PROCESS_ID]));
                                cs_LogMessage(db->m_Log,CS_LOG_REPORT,"C-0081","Closed connection",msg);
                                memset(uiptr,0,slot_size);
                                lpHead[CS_OFF_DB_SHMEM_HEAD_CLIENT_COUNT]--;
                                rptr[CS_OFF_DB_SHMEM_ROW_RESPONSE]=CS_PRT_DB_SHMEM_ROW_RESPONSE_SUCCESS;
                                break;
                            case CS_PRT_DB_SHMEM_ROW_REQUEST_WRITE:
                            case CS_PRT_DB_SHMEM_ROW_REQUEST_DELETE:
                            case CS_PRT_DB_SHMEM_ROW_REQUEST_COMMIT:
                                break;
                            default:
                                break;
                        }
                        rptr+=row_size/sizeof(cs_uint64);
                    }
                    db->UnLock();
                    
                    work++;
                    this_time=cs_TimeNow();
                    uiptr[CS_OFF_DB_SHMEM_SLOT_RESULT]=CS_PRT_DB_SHMEM_SLOT_RESULT_SUCCESS;
                    uiptr[CS_OFF_DB_SHMEM_SLOT_TIMESTAMP]=(cs_uint32)this_time;
                    if(this_time-start_time>CS_DCT_DB_DEFAULT_MAX_TIME_PER_SWEEP)
                    {
                        count=db->m_MaxClients;
                    }
                }                
            }            
        }
        last_slot=slot;   
        
        tAft=cs_TimeNow();
        if(work)
        {
            tTotWork+=tAft-tBef;
            cWork++;
        }
        else
        {
            tTotGen+=tAft-tBef;
            cGen++;            
        }
                
        
        if(work == 0)
        {
            __US_Sleep(1);
        }
    }
    
    db->m_Status -= CS_STT_DB_DATABASE_SHMEM_OPENED;
    
    if(cWork==0)
    {
        cWork=-1;
    }
    
    
exitlbl:    

    if(db->m_Semaphore)
    {
        __US_SemDestroy(db->m_Semaphore);
        db->m_Semaphore=NULL;
    }
            
    if(lpShMem)
    {
        __US_ShMemUnmap(lpShMem);
        lpShMem=NULL;
    }

    if(shmem)
    {
        __US_ShMemClose(shmem);
        shmem=NULL;
    }
            
    db->m_LastError=err;    
    
    cs_LogMessage(db->m_Log,CS_LOG_SYSTEM,"C-0082","SHMEM Read server exited","");
    
    return err;
}




