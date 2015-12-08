//
//  c4MAC.c
//  C4
//
//  Created by vincent Moscaritolo on 11/5/15.
//  Copyright © 2015 4th-A Technologies, LLC. All rights reserved.
//

#include "c4internal.h"


#ifdef __clang__
#pragma mark - MAC
#endif


typedef struct MAC_Context    MAC_Context;

struct MAC_Context
{
#define kMAC_ContextMagic		0x63344D61
    uint32_t                magic;
    MAC_Algorithm           macAlgor;
    
#if  _USES_COMMON_CRYPTO_
    CCHmacAlgorithm         ccAlgor;
#endif
    
    size_t                  hashsize;
    
    union
    {
        hmac_state              hmac;
        skeinmac_state          skeinmac;
#if  _USES_COMMON_CRYPTO_
        CCHmacContext           ccMac;
#endif
    }state;
    
    int (*process)(void *ctx, const unsigned char *in, unsigned long inlen);
    
    int (*done)(void *ctx, unsigned char *out, unsigned long *outlen);
    
};


/*____________________________________________________________________________
 validity test
 ____________________________________________________________________________*/

static bool sMAC_ContextIsValid( const MAC_ContextRef  ref)
{
    bool	valid	= false;
    
    valid	= IsntNull( ref ) && ref->magic	 == kMAC_ContextMagic;
    
    return( valid );
}

#define validateMACContext( s )		\
ValidateParam( sMAC_ContextIsValid( s ) )

#if  _USES_COMMON_CRYPTO_


int sCCMacUpdate(CCHmacContext *ctx, const unsigned char *in, unsigned long inlen)
{
    CCHmacUpdate(ctx, in, inlen);
    
    return CRYPT_OK;
}


int sCCMacFinal(CCHmacContext *ctx, unsigned char *out, unsigned long *outlen)
{
    
    u08b_t    macBuf[64];
    u08b_t    *p = (*outlen < sizeof(macBuf))?macBuf:out;
    
    CCHmacFinal(ctx, p);
    
    if(p!= out)
        memcpy( out,macBuf, *outlen);
    
    
#ifdef LTC_CLEAN_STACK
    zeromem(ctx, sizeof(CCHmacContext));
    zeromem(macBuf, sizeof(macBuf));
#endif
    
    return CRYPT_OK;
}


#endif

C4Err MAC_Init(MAC_Algorithm mac, HASH_Algorithm hash, const void *macKey, size_t macKeyLen, MAC_ContextRef * ctx)
{
    int             err = kC4Err_NoErr;
    const struct    ltc_hash_descriptor* hashDesc = NULL;
    MAC_Context*   macCTX = NULL;
    
    ValidateParam(ctx);
    *ctx = NULL;
    
    hashDesc = sDescriptorForHash(hash);
    
    if(IsNull(hashDesc))
        RETERR( kC4Err_BadHashNumber);
    
    
    macCTX = XMALLOC(sizeof (MAC_Context)); CKNULL(macCTX);
    
    macCTX->magic = kMAC_ContextMagic;
    macCTX->macAlgor = mac;
    macCTX->hashsize = 0;
    
    switch(mac)
    {
        case  kMAC_Algorithm_HMAC:
            
#if  _USES_COMMON_CRYPTO_
            
            switch(hash)
        {
            case kHASH_Algorithm_MD5:
                macCTX->ccAlgor = kCCHmacAlgMD5;
                macCTX->hashsize = 16;
                break;
                
            case kHASH_Algorithm_SHA1:
                macCTX->ccAlgor = kCCHmacAlgSHA1;
                macCTX->hashsize = 20;
                break;
                
            case kHASH_Algorithm_SHA224:
                macCTX->ccAlgor = kCCHmacAlgSHA224;
                macCTX->hashsize = 28;
                break;
                
            case kHASH_Algorithm_SHA384:
                macCTX->ccAlgor = kCCHmacAlgSHA384;
                macCTX->hashsize = 48;
                break;
                
            case kHASH_Algorithm_SHA256:
                macCTX->ccAlgor = kCCHmacAlgSHA256;
                macCTX->hashsize = 32;
                break;
                
            case kHASH_Algorithm_SHA512:
                macCTX->ccAlgor = kCCHmacAlgSHA512;
                macCTX->hashsize = 64;
                break;
                
            default:
                macCTX->ccAlgor =  kCCHmacAlgInvalid;
                break;
        }
            
            if(macCTX->ccAlgor != kCCHmacAlgInvalid)
            {
                CCHmacInit(&macCTX->state.ccMac, macCTX->ccAlgor, macKey, macKeyLen);
                macCTX->process = (void*) sCCMacUpdate;
                macCTX->done = (void*) sCCMacFinal;
            }
            else
            {
                err = hmac_init(&macCTX->state.hmac,  find_hash_id(hashDesc->ID) , macKey, macKeyLen) ; CKERR;
                macCTX->process = (void*) hmac_process;
                macCTX->done = (void*) hmac_done;
                macCTX->hashsize = hashDesc->hashsize;
            }
            
#else
            
            err = hmac_init(&macCTX->state.hmac,  find_hash_id(hashDesc->ID) , macKey, macKeyLen) ; CKERR;
            macCTX->process = (void*) hmac_process;
            macCTX->done = (void*) hmac_done;
            macCTX->hashsize = hashDesc->hashsize;
            
#endif
            break;
            
        case  kMAC_Algorithm_SKEIN:
        {
            switch(hash)
            {
                    
                case kHASH_Algorithm_SKEIN256:
                    err = skeinmac_init(&macCTX->state.skeinmac, Skein256, macKey, macKeyLen);
                    macCTX->process = (void*) skeinmac_process;
                    macCTX->done = (void*) skeinmac_done;
                    macCTX->hashsize = 32;
                    break;
                    
                case kHASH_Algorithm_SKEIN512:
                    err = skeinmac_init(&macCTX->state.skeinmac, Skein512, macKey, macKeyLen);
                    macCTX->process = (void*) skeinmac_process;
                    macCTX->done = (void*) skeinmac_done;
                    macCTX->hashsize = 64;
                    break;
                    
                default:
                    RETERR( kC4Err_BadHashNumber) ;
            }
        }
            break;
            
        default:
            RETERR( kC4Err_BadHashNumber) ;
    }
    
    *ctx = macCTX;
    
done:
    
    if(IsC4Err(err))
    {
        if(IsntNull(macCTX))
        {
            XFREE(macCTX);
        }
    }
    return err;
    
}


C4Err MAC_HashSize( MAC_ContextRef  ctx, size_t * bytes)
{
    int  err = kC4Err_NoErr;
    
    validateMACContext(ctx);
    
    *bytes = ctx->hashsize;
    
    // done:
    
    return (err);
}


C4Err MAC_Update(MAC_ContextRef  ctx, const void *data, size_t dataLength)
{
    int             err = kC4Err_NoErr;
    
    validateMACContext(ctx);
    
    if(ctx->process)
        err = (ctx->process)(&ctx->state,  data, dataLength );
    
    return (err);
}

C4Err MAC_Final(MAC_ContextRef  ctx, void *macOut,  size_t *resultLen)
{
    int             err = kC4Err_NoErr;
    unsigned long  outlen = *resultLen;
    
    validateMACContext(ctx);
    
    if(ctx->done)
        err = (ctx->done)(&ctx->state,  macOut, &outlen );
    
    return err;
    
}



void MAC_Free(MAC_ContextRef  ctx)
{
    
    if(sMAC_ContextIsValid(ctx))
    {
        ZERO(ctx, sizeof(MAC_Context));
        XFREE(ctx);
    }
}


C4Err  MAC_KDF(  MAC_Algorithm      mac,
               HASH_Algorithm     hash,
               uint8_t*           K,
               unsigned long      Klen,
               const char*        label,
               const uint8_t*     context,
               unsigned long      contextLen,
               uint32_t           hashLen,
               unsigned long      outLen,
               uint8_t            *out)
{
    C4Err             err = kC4Err_NoErr;
    MAC_ContextRef       macRef = kInvalidMAC_ContextRef;
    uint8_t              L[4];
    size_t               resultLen = 0;
    
    L[0] = (hashLen >> 24) & 0xff;
    L[1] = (hashLen >> 16) & 0xff;
    L[2] = (hashLen >> 8) & 0xff;
    L[3] = hashLen & 0xff;
    
    err  = MAC_Init( mac,
                    hash,
                    K, Klen, &macRef); CKERR;
    
    MAC_Update(macRef,  "\x00\x00\x00\x01",  4);
    MAC_Update(macRef,  label,  strlen(label));
    MAC_Update(macRef,  "\x00",  1);
    MAC_Update(macRef,  context, contextLen);
    MAC_Update(macRef,  L,  4);
    
    resultLen = outLen;
    MAC_Final( macRef, out, &resultLen);
    
done:
    
    if(IsntNull(macRef))
        MAC_Free(macRef);
    
    return err;
}


