/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#ifndef _CS_DECLARE_H
#define	_CS_DECLARE_H

#include "define.h"
#include "constants.h"
#include "stubs.h"

#ifdef	__cplusplus
extern "C" {
#endif

void *cs_New(                                                                   // Allocates memory segment
    cs_int      Size,                                                           // Size in bytes
    void        *Pool,                                                          // Memory pool (NULL - no pool)
    cs_int32    Option);                                                        // Allocate option - BC_ALT constants

void cs_Delete(                                                                 // Frees memory  segment created by cs_New
    void *ptr,                                                                  // Pointer to memory segment
    void *Pool,                                                                 // Memory pool (NULL - no pool)
    cs_int32    Option);                                                          // Allocate option - BC_ALT constants, same as cs_New

cs_double cs_TimeNow();                                                         // Returns current microtime

void  cs_MemoryDump(                                                            // Prints memory content
    void *ptr,                                                                  // Start pointer
    cs_int32 from,                                                              // Offset
    cs_int32 len);                                                              // Size

void cs_PutNumberLittleEndian(                                                  // Copy number to output buffer, little endian
    void *dest,                                                                 // Destination pointer
    void *src,                                                                  // Source number
    cs_int32 dest_size,                                                         // Number of bytes to copy
    cs_int32 src_size);                                                         // Source number size

void cs_PutNumberBigEndian(                                                     // Copy number to output buffer, big endian
    void *dest,                                                                 // Destination pointer
    void *src,                                                                  // Source number
    cs_int32 dest_size,                                                         // Number of bytes to copy
    cs_int32 src_size);                                                         // Source number size

cs_uint64 __stdcall cs_GetUInt64LittleEndian(                                   // Extract uint64 from little-endian string
    void *src,                                                                  // Source string
    cs_int32 size);                                                             // Bytes to read

cs_uint64 __stdcall cs_GetUInt64LittleEndian32(                                 // Extract uint64 from little-endian 32-bit string
    void *src);                                                                 // Source string

cs_uint64 __stdcall cs_GetUInt64LittleEndian64(                                 // Extract uint64 from little-endian 64-bit string
    void *src);                                                                 // Source string

cs_uint32 cs_GetUInt32BitsLittleEndian(                                         // Extract uint64 from little-endian string
    void *src,                                                                  // Source string
    cs_int32 bits);                                                             // Bits to read

cs_int32 cs_TestSubFolder(                                                      // Checks whether directory exist and creates if needed
    cs_char *dest,                                                              // Resulting directory name
    cs_int32 size,                                                              // Size of result buffer
    cs_char *src,                                                               // Source parent directory
    cs_char *subfolder,                                                         // Subfolder relative to src
    cs_int32 create);                                                           // Create flag


cs_int32 __stdcall cs_AllocSize(                                                // Returns required allocation size
    cs_int32 items,                                                             // Required number of items
    cs_int32 chunk_size,                                                        // Number of items in one chunk
    cs_int32 item_size);                                                        // Item Size

void cs_SwapBytes(                                                              // Swaps bytes in string
    cs_uchar *dest,                                                             // Destination string
    cs_uchar *src,                                                              // Source string
    cs_int32 size);                                                             // String length

cs_uchar cs_HexToUChar(                                                         // Transforms Hex 2-byte string to uchar
    cs_char *Hex);                                                              // Source string

cs_handle cs_LogInitialize(                                                     // Creates log object
    cs_char *FileName,                                                          // Log file name, NULL if stdout
    cs_int32 Size,                                                              // Log buffer size, -1 - default, 0 - immediate output
    cs_uint32 Filter,                                                           // Log filter (BC_LOG constants)
    cs_uint32 Mode);                                                            // Not used

void cs_LogDestroy(                                                             // Destroys log object
    cs_handle LogObject);                                                       // Log object

void cs_LogShift(                                                               // Shifts logical Record ID
    cs_handle LogObject);                                                       // Log object

cs_int32 cs_LogMessage(                                                         // Logs message
    cs_handle LogObject,                                                        // Log object
    cs_uint32 LogCode,                                                          // Log Code (BC_LOG constants)
    cs_char *MessageCode,                                                       // Message code, "" of none
    const cs_char *Message,                                                     // Primary message to log
    cs_char *Secondary);                                                        // Secondary message to log

cs_int32 cs_LogString(                                                          // Logs string
    cs_handle LogObject,                                                        // Log object
    const cs_char *Message);                                                    // Message to log

void cs_LogFlush(                                                               // Flush to log file
    cs_handle LogObject);                                                       // Log object


cs_int32 DBShMemReadServerLoop(                                                 // Asynchronous database server loop
    void *ThreadData);                                                          // cs_Database object


#ifdef	__cplusplus
}
#endif

#endif	/* _CS_DECLARE_H */

