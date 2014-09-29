/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#ifndef _CS_LINUX_H
#define	_CS_LINUX_H

#ifndef _AIX
#include <pthread.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef _LINUX
#include <semaphore.h>
#else // _linux
#include <sys/semaphore.h>
#endif // _linux
#include <sys/shm.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <syslog.h>

#include <glob.h>
#include <pwd.h>
#include <errno.h>
#include <net/if.h>
#include <sys/ioctl.h>
//#include "us_define.h"
#include <string.h>
#include <sys/stat.h>


#include <dlfcn.h>  // We need if of dlopen
#include <unistd.h> // We need it for sleep

#include "stubs.h"

#define SHM_FAILED        (void *)-1L
typedef  void * (* opaque_func)(void *);
#define WAIT_FAILED ((cs_int32)0xFFFFFFFF)
#define WAIT_OBJECT_0       0


#include "define.h"
#include "constants.h"
#include "us_declare.h"


#endif	/* _CS_LINUX_H */

