/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#include "linux.h"

cs_int32 __US_GetHomeFolder(cs_char *result, cs_int32 size)
{
    cs_char *ptr;
    
    ptr=getenv("HOME");
    if(ptr == NULL)
    {
        return -1;
    }
    
    if(strlen(ptr)+2 > (cs_uint32)size)
    {
        return -2;
    }
    
    strcpy(result,ptr);
    if(result[strlen(result)-1] != CS_DCT_FOLDERSEPARATOR)
    {
        result[strlen(result)]=CS_DCT_FOLDERSEPARATOR;
        result[strlen(result)+1]=0x00;
    }
    return strlen(result);
}

cs_handle __US_CreateThread(void* lpThreadAttributes, cs_uint32 dwStackSize,
                        void* lpStartAddress, void* lpParameter,
                        cs_uint32 dwCreationFlags, cs_uint32* lpThreadId)
{
    pthread_t *tp1;
	pthread_attr_t attr;

    cs_int32 ret = 0;

    tp1 = (pthread_t *)malloc(sizeof(pthread_t)); 	
	if ( ( ret = pthread_attr_init(&attr) ) != 0 ) 
    {
		return NULL;
	}

	if ( ( ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) ) != 0 )
    {
		return NULL;
    }

	if ( ( ret = pthread_create(tp1, &attr, (opaque_func)lpStartAddress,(void *)lpParameter) ) != 0 )
	{
		return NULL;
    }

	memcpy(lpThreadId, tp1, sizeof(pthread_t));
	return (cs_handle)tp1;
}


cs_uint32 __US_GetProcessId()
{
    return getpid();
}

cs_uint32 __US_WaitForSingleThread(cs_handle hHandle, cs_uint32 dwMilliseconds)
{
    cs_int32 rc;

    if ((hHandle!= NULL) && (*(pthread_t*)hHandle != 0))
    {
        rc = pthread_join(*(pthread_t *)hHandle,NULL);
        if (rc)
            return WAIT_FAILED;

        return WAIT_OBJECT_0;
    }

    return WAIT_FAILED;
}


cs_uint32 __stdcall __US_CloseThread(cs_handle hObject)
{
	pthread_t *pth;
	cs_int32 rc;
	
	pth = (pthread_t *)hObject;	
	rc = pthread_detach(*pth);	
	if (rc) 
    {
		return 0;
	}

    free(pth);
	return 1;
}

void __US_Sleep (cs_uint32 dwMilliseconds)
{
    timespec ts;

    ts.tv_sec = (time_t)(dwMilliseconds/1000);
    ts.tv_nsec = (cs_int32) ((dwMilliseconds % 1000) * 1000000);

    nanosleep(&ts, NULL);
}


HMODULE __US_LoadLibrary(const cs_char *lpLibFileName)
{
    void *ret;
    cs_char FullLibName[CS_DCT_MAX_PATH];
    const cs_char *LibToUse;

    if(!strchr(lpLibFileName, '.') && strlen(lpLibFileName)<CS_DCT_MAX_PATH-6)
    {
        strcpy(FullLibName, "lib");
        strcat(FullLibName, lpLibFileName);
        strcat(FullLibName, ".so");
        LibToUse = FullLibName;
    }   
    else
        LibToUse = lpLibFileName;

    ret = dlopen(LibToUse, RTLD_NOW);
 
 return (HMODULE)ret;
}


void __US_FreeLibrary(HMODULE hModule)
{
    dlclose(hModule);
}


FARPROC __US_GetProcAddress(HMODULE hModule,const cs_char * lpProcName)
{
    FARPROC proc = NULL;

    proc = (FARPROC)dlsym(hModule,lpProcName);
    return proc;
}


cs_int32 __US_ShMemCreate(cs_handle *ShMem,  cs_int32 Key, cs_int32 Size)
{
    cs_int32 err;
	cs_int shm_id;

	err = 0;
    *ShMem=NULL;

    umask( 0 );

	if ( ( shm_id = shmget( Key, Size, 0666 | IPC_CREAT | IPC_EXCL ) ) < 0 )
    {
        err = 1; 
		return err;
	}
    else
    {
        err=0;
        *ShMem=(cs_handle)(cs_uint64)(shm_id+1);
    }

    return err;
}


cs_int32 __US_ShMemOpen(cs_handle *ShMem,  cs_int32 Key)
{  
    cs_int32 err;
    cs_int shm_id;

    err = 0;
    *ShMem=NULL;

    if ( ( shm_id = shmget( Key, 0, 0666 ) ) < 0 ) 
    {
        err=1;
    }
    else
    {
        err=0;
        *ShMem=(cs_handle)(cs_uint64)(shm_id+1);
    }

    return err;
} 


cs_int32 __US_ShMemClose(cs_handle ShMem)
{
    struct shmid_ds shminfo;
    cs_int  dwShMemObj;

    dwShMemObj=(cs_int)(cs_int64)ShMem;

    dwShMemObj--;

    if ( shmctl( dwShMemObj, IPC_STAT, &shminfo ) < 0 )
    {
       return -1;
    }

    if ( shminfo.shm_nattch == 0 )
    if ( shmctl( dwShMemObj, IPC_RMID, NULL ) < 0 ) 
    {
        return -1;
    }

    return 0;
}


void* __US_ShMemMap(cs_handle ShMem)
{
	cs_int32 prot;
	void *p;
    cs_int  dwShMemObj;

    dwShMemObj=(cs_int)(cs_int64)ShMem;

    dwShMemObj--;

    prot = 0;

	if ( ( p = shmat( dwShMemObj, 0, prot ) ) == SHM_FAILED )
    {
		return NULL;
	}

	return p;
}

cs_int32 __US_ShMemUnmap(void *ShMemPtr)
{
	if ( shmdt( ShMemPtr ) < 0 ) 
    {
        return -1;
	}
	return 0;
}
 
cs_int32 __US_Fork(cs_char *lpCommandLine,cs_int32 WinState,cs_int32 *lpChildPID)
{
    cs_int32 res;
    int statloc;

    statloc=0;
    res=fork();
    if(res==0)
    {
        res=fork();
        if(res)
        {
            *lpChildPID=res;
            exit(0); //Child exits
        }
    }
    else
    {
        if(res>0)
        {
            while(*lpChildPID==0)
            {
                 __US_Sleep(1);
            }
            waitpid(res, &statloc, 0);
            res=*lpChildPID;
        }
    }

    return res;//Parent exits with PID of grandchild , GrandChild exits with 0
}

cs_int32 __US_BecomeDaemon()
{
    cs_int32 ChildPID;
    cs_int32 res;
    cs_int32 i,fd;

    res=__US_Fork(NULL,0,&ChildPID);

    if(res)
         exit(0);

    for (i = getdtablesize()-1; i > 2; --i)
          close(i);

    fd = open("/dev/tty", O_RDWR);
    ioctl(fd, TIOCNOTTY, 0);
    close (fd);

    return res;
}


cs_int32 __US_KillProcess(cs_int32 pid)
{
    cs_int32 res;

    res=kill(pid,SIGKILL);
    return res;
}


cs_int32 __US_ChangeWindowVisibility(cs_int32 Visible)
{
     return 0;
}


cs_int32 __US_CheckProcess(cs_int32 pid)
{
    cs_int32 res;

    res=kill(pid,0);
    return res;
}

cs_handle __US_SemCreate()
{
    sem_t *lpsem;
    
    lpsem=NULL;
    
    lpsem=new sem_t;
    if(sem_init(lpsem,0666,1))
    {
        delete lpsem;
        return NULL;
    }

    return (cs_handle)lpsem;
}

void __US_SemWait(cs_handle sem)
{
    if(sem)
    {
        sem_wait((sem_t *)sem);
    }
}

void __US_SemPost(cs_handle sem)
{
    if(sem)
    {
        sem_post((sem_t*)sem);
    }
}

void __US_SemDestroy(cs_handle sem)
{
    sem_t *lpsem;
    if(sem)
    {
        sem_close((sem_t*)sem);
        lpsem=(sem_t*)sem;
        delete lpsem;
    }
}

cs_int32 __US_Shell(cs_char *command)
{
    cs_char devnull_command[CS_DCT_MAX_PATH];
    
    strcpy(devnull_command,command);
    strcat(devnull_command," 1>/dev/null 2>/dev/null");
    
    return system(devnull_command);
}

