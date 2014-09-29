/*
 * CoinSpark asset tracking server 1.0 
 * 
 * Copyright (c) 2014 Coin Sciences Ltd
 * 
 * Distributed under the AGPLv3 software license, see the accompanying AGPLv3.txt or
 * http://www.gnu.org/licenses/agpl-3.0.txt
 *
 */

#ifndef _CS_DB_H
#define	_CS_DB_H

#include "declare.h"

#define CS_DCT_DB_READ_BUFFER_SIZE                         4096
#define CS_DCT_DB_DEFAULT_MAX_CLIENTS                       256
#define CS_DCT_DB_DEFAULT_MAX_ROWS                          256
#define CS_DCT_DB_DEFAULT_MAX_KEY_SIZE                      256
#define CS_DCT_DB_DEFAULT_MAX_VALUE_SIZE                    256
#define CS_DCT_DB_DEFAULT_MAX_TIME_PER_SWEEP                1.f
#define CS_DCT_DB_DEFAULT_MAX_SHMEM_TIMEOUT                 3.f

#define CS_STT_DB_DATABASE_CLOSED                           0x00000000
#define CS_STT_DB_DATABASE_OPENED                           0x00000001
#define CS_STT_DB_DATABASE_SHMEM_OPENED                     0x00000002


#define CS_OPT_DB_DATABASE_DEFAULT                          0x00000000
#define CS_OPT_DB_DATABASE_CREATE_IF_MISSING                0x00000001
#define CS_OPT_DB_DATABASE_TRANSACTIONAL                    0x00000002
#define CS_OPT_DB_DATABASE_READONLY                         0x00000004
#define CS_OPT_DB_DATABASE_SEEK_ON_READ                     0x00000010
#define CS_OPT_DB_DATABASE_DELAYED_OPEN                     0x00000020
#define CS_OPT_DB_DATABASE_OPTION_MASK                      0x000FFFFF
#define CS_OPT_DB_DATABASE_LEVELDB                          0x00100000
#define CS_OPT_DB_DATABASE_FSR_UTXOC_BLOCKS                 0x00200000
#define CS_OPT_DB_DATABASE_REMOTE_SHMEM                     0x01000000
#define CS_OPT_DB_DATABASE_TYPE_MASK                        0x0FF00000

#define CS_OFF_DB_SHMEM_HEAD_STATUS                              0
#define CS_OFF_DB_SHMEM_HEAD_SIGNAL                              1
#define CS_OFF_DB_SHMEM_HEAD_CLIENT_COUNT                        2
#define CS_OFF_DB_SHMEM_HEAD_MAX_CLIENT_COUNT                    3
#define CS_OFF_DB_SHMEM_HEAD_REQUEST_PROCESS_ID                  4
#define CS_OFF_DB_SHMEM_HEAD_RESPONSE_PROCESS_ID                 5
#define CS_OFF_DB_SHMEM_HEAD_RESPONSE_SLOT_ID                    6
#define CS_OFF_DB_SHMEM_HEAD_KEY_SIZE                            8
#define CS_OFF_DB_SHMEM_HEAD_VALUE_SIZE                          9
#define CS_OFF_DB_SHMEM_HEAD_ROW_SIZE                           10
#define CS_OFF_DB_SHMEM_HEAD_SLOT_SIZE                          11
#define CS_OFF_DB_SHMEM_HEAD_MAX_ROWS                           12
#define CS_OFF_DB_SHMEM_HEAD_DATA_OFFSET                        16
#define CS_OFF_DB_SHMEM_HEAD_SLOT_DATA_OFFSET                   17
#define CS_OFF_DB_SHMEM_HEAD_ROW_DATA_OFFSET                    18
#define CS_OFF_DB_SHMEM_HEAD_FIELD_COUNT                        32

#define CS_OFF_DB_SHMEM_SLOT_PROCESS_ID                          0
#define CS_OFF_DB_SHMEM_SLOT_ROW_COUNT                           1
#define CS_OFF_DB_SHMEM_SLOT_RESULT                              2
#define CS_OFF_DB_SHMEM_SLOT_TIMESTAMP                           3
#define CS_OFF_DB_SHMEM_SLOT_DATA                                8

#define CS_OFF_DB_SHMEM_ROW_REQUEST                              0
#define CS_OFF_DB_SHMEM_ROW_RESPONSE                             1
#define CS_OFF_DB_SHMEM_ROW_KEY_SIZE                             2
#define CS_OFF_DB_SHMEM_ROW_VALUE_SIZE                           3
#define CS_OFF_DB_SHMEM_ROW_DATA                                 4

#define CS_PRT_DB_SHMEM_SLOT_RESULT_UNDEFINED                       0
#define CS_PRT_DB_SHMEM_SLOT_RESULT_SUCCESS                         1
#define CS_PRT_DB_SHMEM_SLOT_RESULT_ERROR                          16

#define CS_PRT_DB_SHMEM_ROW_REQUEST_READ                            1
#define CS_PRT_DB_SHMEM_ROW_REQUEST_WRITE                           2
#define CS_PRT_DB_SHMEM_ROW_REQUEST_DELETE                          3
#define CS_PRT_DB_SHMEM_ROW_REQUEST_COMMIT                          4
#define CS_PRT_DB_SHMEM_ROW_REQUEST_CLOSE                          16

#define CS_PRT_DB_SHMEM_ROW_RESPONSE_UNDEFINED                      0
#define CS_PRT_DB_SHMEM_ROW_RESPONSE_SUCCESS                        1
#define CS_PRT_DB_SHMEM_ROW_RESPONSE_NULL                           1
#define CS_PRT_DB_SHMEM_ROW_RESPONSE_NOT_NULL                       2
#define CS_PRT_DB_SHMEM_ROW_RESPONSE_ERROR                         16



#ifdef	__cplusplus
extern "C" {
#endif

typedef struct cs_Database
{
    cs_Database()
    {
         Zero();
    }

    ~cs_Database()
    {
         Destroy();
    }
    
    cs_handle               m_DB;
    cs_handle               m_OpenOptions;
    cs_handle               m_ReadOptions;
    cs_handle               m_WriteOptions;
    cs_handle               m_WriteBatch;
    
    cs_uint32               m_ShMemKey;
    cs_int32                m_MaxClients;
    cs_int32                m_KeySize;
    cs_int32                m_ValueSize;
    cs_int32                m_MaxRows;
    cs_handle               m_Semaphore;
    cs_int32                m_Signal;
    cs_int32                m_LastError;
    cs_uint64               *m_lpHandlerShMem;    
    
    cs_handle               m_hShMem;
    cs_uint64               *m_lpShMem;    
    cs_uint64               *m_lpShMemSlot;
    
    cs_char                 *m_ReadBuffer;    
    cs_int32               m_ReadBufferSize;
    cs_int32               m_Status;                                            /* Database status - CS_STT_DB_DATABASE constants*/
    cs_int32                m_Options;
    cs_handle               m_Log;
    cs_int32                m_WriteCount;
    cs_int32                m_DeleteCount;

    cs_char                 m_Name[CS_DCT_MAX_PATH];
    
    cs_char                 m_LogFile[CS_DCT_MAX_PATH];
    
    cs_char                 *m_LogBuffer;    
    cs_int32                m_LogSize;    
    
    cs_int32 LogWrite(cs_int32 op,
                      cs_char  *key,                                            /* key */
                      cs_int32 key_len,                                         /* key length, -1 if strlen is should be used */
                      cs_char  *value,                                          /* value */
                      cs_int32 value_len                                        /* value length, -1 if strlen is used */
    );    
    cs_int32 LogFlush();
    
    cs_int32 Destroy();
    cs_int32 Zero();
    
    cs_int32 SetOption(cs_char *option,cs_int32 suboption,cs_int32 value);
    
    cs_int32 Open(                                                              /* Open database */
        cs_char   *name,                                                        /* Database name, path in case of leveldb */
        cs_int32  Options                                                       /* Open options, CS_OPT_DB_DATABASE constants */
    );
    cs_int32 Close();                                                           /* Close databse */
    cs_int32 Write(                                                             /* Writes (key,value) to database */
        cs_char  *key,                                                          /* key */
        cs_int32 key_len,                                                       /* key length, -1 if strlen is should be used */
        cs_char  *value,                                                        /* value */
        cs_int32 value_len,                                                     /* value length, -1 if strlen is used */
        cs_int32 Options                                                        /* Options - not used */
    );
    cs_char *Read(                                                              /* Reads value for specified key, no freeing required, pointer is valid until next read */
        cs_char  *key,                                                          /* key */
        cs_int32 key_len,                                                       /* key length, -1 if strlen is should be used */
        cs_int32 *value_len,                                                    /* value length */
        cs_int32 Options,                                                       /* Options - not used */
        cs_int32 *error                                                         /* Error */
    );
    cs_int32 BatchRead(
        cs_char *Data,
        cs_int32 *results,
        cs_int32 count,
        cs_int32 Options
    );
    cs_int32 Delete(                                                            /* Delete key from database */
        cs_char  *key,                                                          /* key */
        cs_int32 key_len,                                                       /* key length, -1 if strlen is should be used */
        cs_int32 Options                                                        /* Options - not used */
    );
    cs_int32 Commit(
        cs_int32 Options                                                        /* Options - not used */
    );
    
    void Lock(cs_int32 write_mode);
    void UnLock();
    cs_int32 Synchronize();
    
    cs_char *MoveNext(
        cs_int32 *error
    );
    
    cs_char *MoveNextKeyLevels(
        cs_int32 key_levels,
        cs_int32 *error
    );
    
} cs_Database;




#ifdef	__cplusplus
}
#endif

#endif	/* _CS_DB_H */

