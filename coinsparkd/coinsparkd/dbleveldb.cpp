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
#include "leveldb/c.h"

cs_int32 cs_Database::Zero()
{
    
    m_DB=NULL;
    m_OpenOptions=NULL;
    m_ReadOptions=NULL;
    m_WriteOptions=NULL;
    m_WriteBatch=NULL;
    
    m_ReadBuffer=NULL;
    m_ReadBufferSize=0;
    
    m_ShMemKey=0;
    m_MaxClients=CS_DCT_DB_DEFAULT_MAX_CLIENTS;
    m_KeySize=CS_DCT_DB_DEFAULT_MAX_KEY_SIZE;
    m_ValueSize=CS_DCT_DB_DEFAULT_MAX_VALUE_SIZE;
    m_MaxRows=CS_DCT_DB_DEFAULT_MAX_ROWS;
    m_Semaphore=NULL;
    m_Signal=0;            
    m_LastError=0;
    
    m_hShMem=NULL;
    m_lpShMem=NULL;
    m_lpShMemSlot=NULL;
    
    m_Options=CS_OPT_DB_DATABASE_DEFAULT;
    m_Status=CS_STT_DB_DATABASE_CLOSED;  
    m_Log=NULL;

    m_WriteCount=0;
    m_DeleteCount=0;
    
    m_Name[0]=0;
    
    return CS_ERR_NOERROR;
}

cs_int32 cs_Database::Destroy()
{
    if(m_lpShMem)
    {
        __US_ShMemUnmap(m_lpShMem);
        m_lpShMem=NULL;
    }

    if(m_hShMem)
    {
        __US_ShMemClose(m_hShMem);
        m_hShMem=NULL;
    }

    
    if(m_ReadBuffer)
    {
        cs_Delete(m_ReadBuffer,NULL,CS_ALT_DEFAULT);
        m_ReadBuffer=NULL;
    }
    
    Zero();
    
    return CS_ERR_NOERROR;
}

cs_int32 cs_Database::Open(cs_char *name,cs_int32 Options)
{
    cs_char *err = NULL;
    cs_int32 ierr;
    cs_uint32 pid;
    cs_double start_time;

    ierr=CS_ERR_NOERROR;

    if(m_ReadBuffer  == NULL)
    {
        m_ReadBuffer=(cs_char*)cs_New(CS_DCT_DB_READ_BUFFER_SIZE,NULL,CS_ALT_DEFAULT);
        if(m_ReadBuffer  == NULL)
        {
            return CS_ERR_ALLOCATION;
        }
    }
    
    strcpy(m_Name,name);
    
    m_Options=Options;
    
    switch(m_Options & CS_OPT_DB_DATABASE_TYPE_MASK)
    {
        case CS_OPT_DB_DATABASE_LEVELDB:    
//            return CS_ERR_NOT_SUPPORTED;

            if((m_Options & CS_OPT_DB_DATABASE_DELAYED_OPEN) == 0)
            {
                m_OpenOptions = (cs_handle)leveldb_options_create();
                m_ReadOptions = (cs_handle)leveldb_readoptions_create();
                m_WriteOptions = (cs_handle)leveldb_writeoptions_create();

                if(Options & CS_OPT_DB_DATABASE_CREATE_IF_MISSING)
                {
                    leveldb_options_set_create_if_missing((leveldb_options_t*)m_OpenOptions, 1);
                }

                if(Options & CS_OPT_DB_DATABASE_TRANSACTIONAL)
                {
                    m_WriteBatch=(cs_handle)leveldb_writebatch_create();
                    if(m_WriteBatch == NULL)
                    {
                        return CS_ERR_DB_OPEN;            
                    }
                    leveldb_writebatch_clear((leveldb_writebatch_t*)m_WriteBatch);
                }

                m_DB = leveldb_open((leveldb_options_t*)m_OpenOptions, name, &err);

                if (err != NULL) 
                {
                    Destroy();
                    printf("%s\n",err);
                    leveldb_free(err);

                    return CS_ERR_DB_OPEN;
                }
            }
            
            break;
            
        case CS_OPT_DB_DATABASE_REMOTE_SHMEM:    
            
            if(m_ShMemKey == 0)
            {
                ierr=CS_ERR_DB_SHMEM_KEY_NOT_SET;
                return ierr;
            }
    
            m_hShMem=NULL;            
            m_lpShMem=NULL;
    
            if(__US_ShMemOpen(&m_hShMem,m_ShMemKey))
            {
                ierr=CS_ERR_DB_SHMEM_CANNOT_OPEN;
                return ierr;
            }
    
            m_lpShMem=(cs_uint64*)__US_ShMemMap(m_hShMem);
            if(m_lpShMem == NULL)
            {
                __US_ShMemClose(m_hShMem);
                m_hShMem=NULL;
                ierr=CS_ERR_DB_SHMEM_CANNOT_OPEN;
                return ierr;
            }
            
            pid=__US_GetProcessId();
            
            m_lpShMemSlot=NULL;
            start_time=cs_TimeNow();
            while((m_lpShMemSlot==NULL) && (cs_TimeNow()-start_time<CS_DCT_DB_DEFAULT_MAX_SHMEM_TIMEOUT))
            {
                if(m_lpShMem[CS_OFF_DB_SHMEM_HEAD_RESPONSE_PROCESS_ID]!=pid)
                {
                    m_lpShMem[CS_OFF_DB_SHMEM_HEAD_REQUEST_PROCESS_ID]=pid;
                }
                else
                {
                    m_lpShMemSlot=m_lpShMem+(m_lpShMem[CS_OFF_DB_SHMEM_HEAD_DATA_OFFSET]+
                            m_lpShMem[CS_OFF_DB_SHMEM_HEAD_RESPONSE_SLOT_ID]*m_lpShMem[CS_OFF_DB_SHMEM_HEAD_SLOT_SIZE])/sizeof(cs_uint64);
                    m_lpShMem[CS_OFF_DB_SHMEM_HEAD_REQUEST_PROCESS_ID]=0;
                    m_lpShMem[CS_OFF_DB_SHMEM_HEAD_RESPONSE_PROCESS_ID]=0;
                }                
            }
            
            if(m_lpShMemSlot==NULL)
            {
                __US_ShMemUnmap(m_lpShMem);
                m_lpShMem=NULL;                
                __US_ShMemClose(m_hShMem);
                m_hShMem=NULL;
                ierr=CS_ERR_DB_SHMEM_CANNOT_CONNECT;
                return ierr;
            }
            
            break;
        default:
            return CS_ERR_DB_OPEN;
            break;
    }
            
    m_Status=CS_STT_DB_DATABASE_OPENED;  
    
    return CS_ERR_NOERROR;    
}

cs_int32 cs_Database::Close()
{
    cs_uint64 *rptr;
    cs_uint32 pid; 
    cs_double start_time;
    
    switch(m_Options & CS_OPT_DB_DATABASE_TYPE_MASK)
    {
        case CS_OPT_DB_DATABASE_LEVELDB:    
            if(m_OpenOptions)
            {
                leveldb_options_destroy((leveldb_options_t*)m_OpenOptions);
                m_OpenOptions=NULL;
            }

            if(m_ReadOptions)
            {
                leveldb_readoptions_destroy((leveldb_readoptions_t*)m_ReadOptions);
                m_ReadOptions=NULL;
            }

            if(m_WriteOptions)
            {
                leveldb_writeoptions_destroy((leveldb_writeoptions_t*)m_WriteOptions);
                m_WriteOptions=NULL;
            }

            if(m_WriteBatch)
            {
                leveldb_writebatch_destroy((leveldb_writebatch_t*)m_WriteBatch);
                m_WriteBatch=NULL;
            }

            if(m_DB)
            {
                switch(m_Options & CS_OPT_DB_DATABASE_TYPE_MASK)
                {
                    case CS_OPT_DB_DATABASE_LEVELDB:    
                        leveldb_close((leveldb_t*)m_DB);
                        break;
                }
                m_DB=NULL;
            }
            break;
        case CS_OPT_DB_DATABASE_REMOTE_SHMEM:    
            if(m_lpShMemSlot)
            {
                pid=__US_GetProcessId();
                if(m_lpShMemSlot[CS_OFF_DB_SHMEM_SLOT_PROCESS_ID] == pid)
                {
                    m_lpShMemSlot[CS_OFF_DB_SHMEM_SLOT_ROW_COUNT]=0;
                    m_lpShMemSlot[CS_OFF_DB_SHMEM_SLOT_RESULT]=CS_PRT_DB_SHMEM_SLOT_RESULT_UNDEFINED;
                    rptr=m_lpShMemSlot+m_lpShMem[CS_OFF_DB_SHMEM_HEAD_SLOT_DATA_OFFSET]/sizeof(cs_uint64);
                    rptr[CS_OFF_DB_SHMEM_ROW_RESPONSE]=CS_PRT_DB_SHMEM_ROW_RESPONSE_UNDEFINED;
                    rptr[CS_OFF_DB_SHMEM_ROW_REQUEST]=CS_PRT_DB_SHMEM_ROW_REQUEST_CLOSE;
                    m_lpShMemSlot[CS_OFF_DB_SHMEM_SLOT_ROW_COUNT]=1;
                    start_time=cs_TimeNow();
                    while((m_lpShMemSlot[CS_OFF_DB_SHMEM_SLOT_PROCESS_ID] == pid) && (cs_TimeNow()-start_time<CS_DCT_DB_DEFAULT_MAX_SHMEM_TIMEOUT))
                    {
                        __US_Sleep(1);
                    }
                }
            }
            break;
    }
    
    return CS_ERR_NOERROR;
}

cs_int32 cs_Database::Write(cs_char *key,cs_int32 key_len,cs_char *value,cs_int32 value_len,cs_int32 Options)
{
    cs_char *err = NULL;    
    cs_int32 klen=key_len;
    cs_int32 vlen=value_len;
    
    
    m_WriteCount++;
    
    if(key == NULL)
    {
        return CS_ERR_CANNOT_BE_NULL;
    }
    if(value == NULL)
    {
        return CS_ERR_CANNOT_BE_NULL;
    }
    if(klen<0)klen=strlen(key);
    if(vlen<0)vlen=strlen(value);
    
    if(m_DB == NULL)
    {
        return CS_ERR_DB_NOT_OPENED;
    }
    
    switch(m_Options & CS_OPT_DB_DATABASE_TYPE_MASK)
    {
        case CS_OPT_DB_DATABASE_LEVELDB:    
//            return CS_ERR_NOT_SUPPORTED;

            if(Options & CS_OPT_DB_DATABASE_TRANSACTIONAL)
            {
                leveldb_writebatch_put((leveldb_writebatch_t*)m_WriteBatch, key, klen, value, vlen);        
            }
            else
            {
                leveldb_put((leveldb_t*)m_DB, (leveldb_writeoptions_t*)m_WriteOptions, key, klen, value, vlen, &err);        
            }

            if (err != NULL) 
            {
                leveldb_free(err);

                return CS_ERR_DB_WRITE;
            }

            break;
    }
    
    return CS_ERR_NOERROR;
}

cs_char *cs_Database::MoveNext(cs_int32 *error)
{
    *error=CS_ERR_NOT_SUPPORTED;
    return NULL;
}

cs_char *cs_Database::MoveNextKeyLevels(cs_int32 key_levels,cs_int32 *error)
{
    *error=CS_ERR_NOT_SUPPORTED;
    return NULL;
}

cs_char *cs_Database::Read(cs_char *key,cs_int32 key_len,cs_int32 *value_len,cs_int32 Options,cs_int32 *error)
{
    cs_char *err = NULL;
    cs_char *lpRead;
    cs_char *lpNewBuffer;
    cs_int32 NewSize;
    size_t vallen;

    cs_int32 klen=key_len;
    
    *value_len=0;
    *error=CS_ERR_NOERROR;
    
    if(key == NULL)
    {
        *error=CS_ERR_CANNOT_BE_NULL;
        return NULL;
    }
    if(klen<0)klen=strlen(key);
    
    if(m_DB == NULL)
    {
        *error=CS_ERR_DB_NOT_OPENED;
        return NULL;
    }

    
    switch(m_Options & CS_OPT_DB_DATABASE_TYPE_MASK)
    {
        case CS_OPT_DB_DATABASE_LEVELDB:    
            
//            *error=CS_ERR_NOT_SUPPORTED;
//            return NULL;

            if(Options & CS_OPT_DB_DATABASE_SEEK_ON_READ)
            {
                *error=CS_ERR_NOT_SUPPORTED;
                return NULL;                
            }
            
            lpRead = leveldb_get((leveldb_t*)m_DB, (leveldb_readoptions_t*)m_ReadOptions, key, klen, &vallen, &err);

            if (err != NULL) 
            {
                leveldb_free(err);
                *error=CS_ERR_DB_READ;
                return NULL;
            }

            if(lpRead == NULL)
            {
                return NULL;    
            }

            *value_len=vallen;

            break;
        default:
            *error=CS_ERR_DB_OPEN;
            return NULL;
            break;
    }
    
    if(*value_len+1>m_ReadBufferSize)
    {
        NewSize=((*value_len)/CS_DCT_DB_READ_BUFFER_SIZE + 1) * CS_DCT_DB_READ_BUFFER_SIZE;
        lpNewBuffer=(cs_char*)cs_New(NewSize,NULL,CS_ALT_DEFAULT);
        if(lpNewBuffer  == NULL)
        {
            *value_len=0;
            *error=CS_ERR_ALLOCATION;
            return NULL;
        }

        cs_Delete(m_ReadBuffer,NULL,CS_ALT_DEFAULT);
        m_ReadBuffer=lpNewBuffer;
        m_ReadBufferSize=NewSize;
    }

    memcpy(m_ReadBuffer,lpRead,*value_len);
    m_ReadBuffer[*value_len]=0;
    
    switch(m_Options & CS_OPT_DB_DATABASE_TYPE_MASK)
    {
        case CS_OPT_DB_DATABASE_LEVELDB:    

            leveldb_free(lpRead);

            break;
    }
    
    return m_ReadBuffer;    
}

cs_int32 cs_Database::Delete(cs_char *key,cs_int32 key_len,cs_int32 Options)
{    
    cs_char *err = NULL;
    cs_int32 klen=key_len;

    m_DeleteCount++;
    
    if(key == NULL)
    {
        return CS_ERR_CANNOT_BE_NULL;
    }
    if(klen<0)klen=strlen(key);

    if(m_DB == NULL)
    {
        return CS_ERR_DB_NOT_OPENED;
    }
    
    switch(m_Options & CS_OPT_DB_DATABASE_TYPE_MASK)
    {
        case CS_OPT_DB_DATABASE_LEVELDB:    
//            return CS_ERR_NOT_SUPPORTED;


            if(Options & CS_OPT_DB_DATABASE_TRANSACTIONAL)
            {
                leveldb_writebatch_delete((leveldb_writebatch_t*)m_WriteBatch, key, klen);        
            }
            else
            {
                leveldb_delete((leveldb_t*)m_DB, (leveldb_writeoptions_t*)m_WriteOptions, key, klen, &err);
            }

            if (err != NULL) 
            {
                leveldb_free(err);

                return CS_ERR_DB_DELETE;
            }

            break;
    }
    
    return CS_ERR_NOERROR;
    
}

cs_int32 cs_Database::Commit(cs_int32 Options)
{
    cs_char *err = NULL;
    cs_char msg[100];

    sprintf(msg,"Writes: %6d; Deletes: %6d;",m_WriteCount,m_DeleteCount);
    cs_LogMessage(m_Log,CS_LOG_REPORT,"C-0083","Commit",msg);
    
    m_WriteCount=0;
    m_DeleteCount=0;
    
    if(m_DB == NULL)
    {
        return CS_ERR_DB_NOT_OPENED;
    }
    
    switch(m_Options & CS_OPT_DB_DATABASE_TYPE_MASK)
    {
        case CS_OPT_DB_DATABASE_LEVELDB:    
//            return CS_ERR_NOT_SUPPORTED;


            if(Options & CS_OPT_DB_DATABASE_TRANSACTIONAL)
            {
                leveldb_write((leveldb_t*)m_DB, (leveldb_writeoptions_t*)m_WriteOptions, (leveldb_writebatch_t*)m_WriteBatch, &err);        
                leveldb_writebatch_clear((leveldb_writebatch_t*)m_WriteBatch);
            }

            if (err != NULL) 
            {
                leveldb_free(err);

                return CS_ERR_DB_WRITE;
            }

            break;
    }
    return CS_ERR_NOERROR;    
}

cs_int32 cs_Database::Synchronize()
{
    Close();
    
    return Open(m_Name,m_Options);
}


cs_int32 cs_Database::SetOption(cs_char* option, cs_int32 suboption, cs_int32 value)
{
    if(strcmp(option,"ShMemKey") == 0)
    {
        m_ShMemKey=value;
    }
    
    if(strcmp(option,"KeySize") == 0)
    {
        m_KeySize=value;
    }

    if(strcmp(option,"ValueSize") == 0)
    {
        m_ValueSize=value;
    }
    
    if(strcmp(option,"MaxRows") == 0)
    {
        m_ValueSize=value;
    }
    
    return CS_ERR_NOERROR;
    
}




