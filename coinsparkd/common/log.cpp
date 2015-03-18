/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#include "declare.h"

#define CS_DCT_LOG_DEFAULT_FILTER           0x00000000
#define CS_DCT_LOG_DEFAULT_BUFFER_SIZE              0

typedef struct cs_Log
{    
    cs_char     *m_lpBuf;
    cs_uint32   m_RecID;
    cs_int32    m_BufSize;
    cs_int32    m_BufPos;
    cs_uint32   m_Filter;
    cs_char     m_FileName[256];
} cs_State;


cs_handle cs_LogInitialize(cs_char *FileName,cs_int32 Size,cs_uint32 Filter,cs_uint32 Mode)
{
    cs_Log *log;
    cs_int32 TSize;
    
    log=NULL;
    log=(cs_Log*)cs_New(sizeof(cs_Log),NULL,CS_ALT_DEFAULT);

    if(log)
    {
        log->m_RecID=(((cs_uint32)cs_TimeNow())&0x00FFFFFF)*256;
        log->m_BufPos=0;
        log->m_BufSize=0;
        log->m_FileName[0]=0;
        if(FileName)
        {
            if(*FileName)
            {
                strcpy(log->m_FileName,FileName);
            }
        }
        log->m_Filter=CS_DCT_LOG_DEFAULT_FILTER;
        if(Filter)
        {
            log->m_Filter=Filter;                
        }
        TSize=CS_DCT_LOG_DEFAULT_BUFFER_SIZE;
        if(Size>=0)
        {
            TSize=Size;
        }
        if(TSize)
        {
            log->m_BufSize=TSize;
            log->m_lpBuf=(cs_char*)cs_New(Size,NULL,CS_ALT_DEFAULT);
            if(log->m_lpBuf == NULL)
            {
                delete log;
                log=NULL;
            }
        }
        else
        {
            log->m_lpBuf=NULL;
        }
    }
    return (cs_handle)log;
}

void cs_LogDestroy(cs_handle LogObject)
{
    cs_Log *log;
    log=(cs_Log*)(LogObject);

    if(log)
    {
        if(log->m_lpBuf)
        {
            cs_Delete(log->m_lpBuf,NULL,CS_ALT_DEFAULT);
        }
        
        cs_Delete(log,NULL,CS_ALT_DEFAULT);
    }
}

void cs_LogShift(cs_handle LogObject)
{
    cs_Log *log;
    log=(cs_Log*)(LogObject);

    if(log)
    {
        log->m_RecID++;
    }    
}

cs_int32 cs_LogMessage(cs_handle LogObject,cs_uint32 LogCode,cs_char *MessageCode,const cs_char *Message,cs_char *Secondary)
{
    cs_Log *log;
    cs_uint32 Filter;
    cs_char Header[48];
    cs_char Code[48];
    cs_int32 size;
    FILE *fout;
    struct timeval time_now;
    struct tm *bdt;
    
    log=(cs_Log*)(LogObject);
    Filter=CS_DCT_LOG_DEFAULT_FILTER;
    if(log)
    {
        Filter=log->m_Filter;
    }
    
    if((LogCode & Filter) == 0)
    {
        return CS_ERR_NOERROR;
    }
    
    gettimeofday(&time_now,NULL);
    
    bdt=localtime(&(time_now.tv_sec));
    sprintf(Header,"%04d-%02d-%02d %02d:%02d:%02d.%03d\tR-%08X\t",1900+bdt->tm_year,
                                                        bdt->tm_mon+1,
                                                        bdt->tm_mday,
                                                        bdt->tm_hour,
                                                        bdt->tm_min,
                                                        bdt->tm_sec,
                                                        (cs_int32)(time_now.tv_usec/1000),
                                                        log->m_RecID);

    switch(LogCode & CS_LOG_LEVEL)
    {
        case CS_LOG_FATAL:
            strcat(Header,"FTL!");
            break;
        case CS_LOG_ERROR:
            strcat(Header,"ERR!");
            break;
        case CS_LOG_WARNING:
            strcat(Header,"WRN!");
            break;
        case CS_LOG_SYSTEM:
            strcat(Header,"SYS ");
            break;
        case CS_LOG_REPORT:
            strcat(Header,"R   ");
            break;
        case CS_LOG_MINOR:
            strcat(Header,"M   ");
            break;
        case CS_LOG_DEBUG:
            strcat(Header,"DBG ");
            break;
        case CS_LOG_NO_CODE:
            strcat(Header,"    ");
            break;
    }
    
    strcpy(Code,"C-0000");
    if(MessageCode[0])
    {
        strcpy(Code,MessageCode);
    }
    
    if(log)
    {
        if(log->m_lpBuf)
        {
            size=strlen(Header)+strlen(MessageCode)+strlen(Message)+strlen(Secondary)+4;
            if(size+log->m_BufPos<=log->m_BufSize)
            {
                memcpy(log->m_lpBuf+log->m_BufPos,Header,strlen(Header));
                log->m_BufPos+=strlen(Header);
                log->m_lpBuf[log->m_BufPos]=0x09;
                log->m_BufPos++;
                memcpy(log->m_lpBuf+log->m_BufPos,MessageCode,strlen(MessageCode));
                log->m_BufPos+=strlen(MessageCode);
                log->m_lpBuf[log->m_BufPos]=0x09;
                log->m_BufPos++;
                memcpy(log->m_lpBuf+log->m_BufPos,Message,strlen(Message));
                log->m_BufPos+=strlen(Message);
                log->m_lpBuf[log->m_BufPos]=0x09;
                log->m_BufPos++;
                memcpy(log->m_lpBuf+log->m_BufPos,Secondary,strlen(Secondary));
                log->m_BufPos+=strlen(Header);
                log->m_lpBuf[log->m_BufPos]=0x0a;
                log->m_BufPos++;
            }
        }        
        else
        {
            if(*(log->m_FileName))
            {
                fout=fopen(log->m_FileName,"a");            
                if(fout)
                {
                    fprintf(fout,"%s\t%s\t%s\t%s\n",Header,Code,Message,Secondary);
                    fclose(fout);
                }
            }
            else
            {
                printf("%s\t%s\t%s\t%s\n",Header,Code,Message,Secondary);                
            }
        }
    }
    else
    {
        printf("%s\t%s\t%s\t%s\n",Header,Code,Message,Secondary);
    }
    
    return CS_ERR_NOERROR;
}

cs_int32 cs_LogString(cs_handle LogObject,const cs_char *Message)
{
    return cs_LogMessage(LogObject,CS_LOG_NO_CODE,"",Message,"");
}

void cs_LogFlush(cs_handle LogObject)
{
    cs_Log *log;
    cs_int32 fhan;
    log=(cs_Log*)(LogObject);
    if(log)
    {
        if(log->m_lpBuf)
        {
            if(log->m_BufPos)
            {
                if(*(log->m_FileName))
                {
                    fhan=open(log->m_FileName,_O_BINARY | O_RDWR | O_CREAT,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                    lseek64(fhan,0,SEEK_END);
                    if(fhan)
                    {
                        (void)(write(fhan,log->m_lpBuf,log->m_BufPos)+1);
                        close(fhan);
                    }
                }
                else
                {
                    (void)(write(1,log->m_lpBuf,log->m_BufPos)+1);
                }
                log->m_BufPos=0;
            }
        }
    }
}


