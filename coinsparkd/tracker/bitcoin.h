/*
 * CoinSpark asset tracking server 1.0 beta
 * 
 * Copyright (c) 2014 Coin Sciences Ltd - coinspark.org
 * 
 * Distributed under the AGPLv3 software license, see the accompanying 
 * file COPYING or http://www.gnu.org/licenses/agpl-3.0.txt
 */


#ifndef _CS_BITCOIN_H
#define	_CS_BITCOIN_H

#include "declare.h"
#include "args.h"
#include "net.h"
#include "coinspark.h"

#ifndef SHA256_DIGEST_LENGTH
#define SHA256_DIGEST_LENGTH 32
#endif

#define CS_DCT_MSG_BUFFERSIZE   65536
#define CS_DCT_MSG_HEADERSIZE      24
#define CS_DCT_MSG_MAX_BLOCKS    2000

#define CS_DCT_HASH_BYTES                   32
#define CS_DCT_BLOCK_HEAD_OFFSET            44
#define CS_DCT_BLOCK_HEAD_SIZE             128


#define CS_DCT_MSG_NETWORK_MAIN                 0xD9B4BEF9
#define CS_DCT_MSG_NETWORK_TESTNET              0xDAB5BFFA
#define CS_DCT_MSG_NETWORK_TESTNET3             0x0709110B
#define CS_DCT_MSG_NETWORK_NAMECOIN             0xFEB4BEF9

#define CS_DCT_MSG_PROTOCOL_VERSION             80600
#define CS_DCT_MSG_TIMEOUT                      30

#define CS_DCT_GENESIS_BLOCK_MAIN               "000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"
#define CS_DCT_GENESIS_BLOCK_TESTNET3           "000000000933ea01ad0ee984209779baaec3ced90fa3f408719526f8d77f4943"

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct cs_Message
{
    cs_Message()
    {
         Zero();
    }

    ~cs_Message()
    {
         Destroy();
    }

	cs_uchar           *m_lpBuf;                                                /* Buffer */
	cs_int32           m_BufSize;                                               /* Buffer Size */
	cs_int32           m_Size;                                                  /* Buffer write pointer */
    cs_Arguments       *m_lpArgs;                                               /* Arguments  */
    cs_uint32          m_BitcoinMagicValue;                                     /* Bitcoin network magic number */
    
    void Zero();
    cs_int32 Destroy();
    cs_int32 Initialize(cs_Arguments *lpArgs,cs_int32 Size);
        
    cs_int32 Clear(                                                             /* Clears message */
    );
    
    cs_int32 PutVarInt(cs_uint64 value);
    
    cs_int32 PutData(                                                           /* Put data to send buffer, send if needed */
        void *lpData,                                                           /* Pointer to input data */
        cs_int32 len                                                            /* Bytes to send */
    );
    
    cs_int32 Complete(                                                          /* Completes message building - creates header, etc. Returns message size including header */
        cs_char *lpCommand                                                      /* Message command */
    );
            
    void *GetData(                                                              /* Returns pointer to data */
    );
    
    cs_int32 GetSize(                                                               /* Returns message size */
    );
    
} cs_Message;
    
void bitcoin_block_hash(cs_uchar *block_header);
void bitcoin_tx_hash(cs_uchar *lpTxHash,cs_uchar *lpTx,cs_int32 TxSize);
void bitcoin_hash_to_string(cs_char *out,cs_uchar *hash);
void bitcoin_string_to_hash(cs_char *out,cs_uchar *hash);
cs_int32 bitcoin_get_varint_size(cs_uchar byte);
cs_int32 bitcoin_put_varint(cs_uchar *buf,cs_int64 value);
cs_int32 bitcoin_wait_for_message(cs_Connection *lpConnection,cs_char *lpCommand,cs_uint64 *payload_size,cs_handle Log);
cs_int32 bitcoin_prepare_message_version(cs_Message *lpMessage,cs_int32 PretendRealNode);    
cs_int32 bitcoin_prepare_message_getdata(cs_Message *lpMessage,cs_int32 type,cs_uchar *lpHash);
cs_int32 bitcoin_prepare_message_mempool(cs_Message *lpMessage); 
cs_int32 bitcoin_find_op_return(cs_uchar *script,cs_int32 size);


#ifdef	__cplusplus
}
#endif

#endif	/* _CS_BITCOIN_H */

