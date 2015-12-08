//
//  c4Hash.c
//  C4
//
//  Created by vincent Moscaritolo on 11/5/15.
//  Copyright © 2015 4th-A Technologies, LLC. All rights reserved.
//

#include "c4internal.h"

#ifdef __clang__
#pragma mark - Hash
#endif

typedef struct HASH_Context    HASH_Context;

struct HASH_Context
{
#define kHASH_ContextMagic		0x63344861
    uint32_t                magic;
    HASH_Algorithm          algor;
    size_t                  hashsize;
    
#if _USES_COMMON_CRYPTO_
    CCHmacAlgorithm         ccAlgor;
#endif
    
    union
    {
        hash_state              tc_state;
#if _USES_COMMON_CRYPTO_
        CC_MD5_CTX              ccMD5_state;
        CC_SHA1_CTX             ccSHA1_state;
        CC_SHA256_CTX           ccSHA256_state;
        CC_SHA512_CTX           ccSHA512_state;
#endif
        
    }state;
    
    
    int (*process)(void *ctx, const unsigned char *in, unsigned long inlen);
    
    int (*done)(void *ctx, unsigned char *out);
    
};


static bool
sHASH_ContextIsValid( const HASH_ContextRef  ref)
{
    bool	valid	= false;
    
    valid	= IsntNull( ref ) && ref->magic	 == kHASH_ContextMagic;
    
    return( valid );
}

#define validateHASHContext( s )		\
ValidateParam( sHASH_ContextIsValid( s ) )

C4Err HASH_Import(void *inData, size_t bufSize, HASH_ContextRef * ctx)
{
    C4Err        err = kC4Err_NoErr;
    HASH_Context*   hashCTX = NULL;
    
    ValidateParam(ctx);
    *ctx = NULL;
    
    
    if(sizeof(HASH_Context) != bufSize)
        RETERR( kC4Err_BadParams);
    
    hashCTX = XMALLOC(sizeof (HASH_Context)); CKNULL(hashCTX);
    
    COPY( inData, hashCTX, sizeof(HASH_Context));
    
    validateHASHContext(hashCTX);
    
    *ctx = hashCTX;
    
done:
    
    if(IsC4Err(err))
    {
        if(IsntNull(hashCTX))
        {
            XFREE(hashCTX);
        }
    }
    
    return err;
}

C4Err HASH_Export(HASH_ContextRef ctx, void *outData, size_t bufSize, size_t *datSize)
{
    C4Err        err = kC4Err_NoErr;
    const struct    ltc_hash_descriptor* desc = NULL;
    
    validateHASHContext(ctx);
    ValidateParam(outData);
    ValidateParam(datSize);
    
    desc = sDescriptorForHash(ctx->algor);
    
    if(IsNull(desc))
        RETERR( kC4Err_BadHashNumber);
    
    if(sizeof(HASH_Context) > bufSize)
        RETERR( kC4Err_BufferTooSmall);
    
    COPY( ctx, outData, sizeof(HASH_Context));
    
    *datSize = sizeof(HASH_Context);
    
done:
    
    return err;
    
}

#if  _USES_COMMON_CRYPTO_

int sCCHashUpdateMD5(void *ctx, const unsigned char *in, unsigned long inlen)
{
    CC_MD5_Update(ctx, in, (CC_LONG)inlen);
    return CRYPT_OK;
}

int sCCHashUpdateSHA1(void *ctx, const unsigned char *in, unsigned long inlen)
{
    CC_SHA1_Update(ctx, in, (CC_LONG)inlen);
    return CRYPT_OK;
}

int sCCHashUpdateSHA224(void *ctx, const unsigned char *in, unsigned long inlen)
{
    CC_SHA224_Update(ctx, in, (CC_LONG)inlen);
    return CRYPT_OK;
}

int sCCHashUpdateSHA256(void *ctx, const unsigned char *in, unsigned long inlen)
{
    CC_SHA256_Update(ctx, in, (CC_LONG)inlen);
    return CRYPT_OK;
}

int sCCHashUpdateSHA384(void *ctx, const unsigned char *in, unsigned long inlen)
{
    CC_SHA384_Update(ctx, in, (CC_LONG)inlen);
    return CRYPT_OK;
}

int sCCHashUpdateSHA512(void *ctx, const unsigned char *in, unsigned long inlen)
{
    CC_SHA512_Update(ctx, in, (CC_LONG)inlen);
    return CRYPT_OK;
}


int sCCHashFinalMD5(void *ctx, unsigned char *out)
{
    CC_MD5_Final(out, ctx);
    
#ifdef LTC_CLEAN_STACK
    zeromem(ctx, sizeof(CC_SHA1_CTX));
#endif
    
    return CRYPT_OK;
}

int sCCHashFinalSHA1(void *ctx, unsigned char *out)
{
    CC_SHA1_Final(out, ctx);
    
#ifdef LTC_CLEAN_STACK
    zeromem(ctx, sizeof(CC_SHA1_CTX));
#endif
    
    return CRYPT_OK;
}

int sCCHashFinalSHA224(void *ctx, unsigned char *out)
{
    CC_SHA224_Final(out, ctx);
    
#ifdef LTC_CLEAN_STACK
    zeromem(ctx, sizeof(CC_SHA1_CTX));
#endif
    
    return CRYPT_OK;
}

int sCCHashFinalSHA256(void *ctx, unsigned char *out)
{
    CC_SHA256_Final(out, ctx);
    
#ifdef LTC_CLEAN_STACK
    zeromem(ctx, sizeof(CC_SHA1_CTX));
#endif
    
    return CRYPT_OK;
}


int sCCHashFinalSHA384(void *ctx, unsigned char *out)
{
    CC_SHA384_Final(out, ctx);
    
#ifdef LTC_CLEAN_STACK
    zeromem(ctx, sizeof(CC_SHA1_CTX));
#endif
    
    return CRYPT_OK;
}


int sCCHashFinalSHA512(void *ctx, unsigned char *out)
{
    CC_SHA512_Final(out, ctx);
    
#ifdef LTC_CLEAN_STACK
    zeromem(ctx, sizeof(CC_SHA1_CTX));
#endif
    
    return CRYPT_OK;
}

#endif


C4Err HASH_Init(HASH_Algorithm algorithm, HASH_ContextRef * ctx)
{
    int             err = kC4Err_NoErr;
    HASH_Context*   hashCTX = NULL;
    const struct ltc_hash_descriptor* desc = NULL;
    
    ValidateParam(ctx);
    *ctx = NULL;
    
    hashCTX = XMALLOC(sizeof (HASH_Context)); CKNULL(hashCTX);
    
    hashCTX->magic = kHASH_ContextMagic;
    hashCTX->algor = algorithm;
    
#if _USES_COMMON_CRYPTO_
    
    switch(algorithm)
    {
        case kHASH_Algorithm_MD5:
            hashCTX->ccAlgor = kCCHmacAlgMD5;
            hashCTX->process = (void*) sCCHashUpdateMD5;
            hashCTX->done = (void*) sCCHashFinalMD5;
            hashCTX->hashsize = 16;
            CC_MD5_Init(&hashCTX->state.ccMD5_state);
            break;
            
        case kHASH_Algorithm_SHA1:
            hashCTX->ccAlgor = kCCHmacAlgSHA1;
            hashCTX->hashsize = 20;
            hashCTX->process = (void*) sCCHashUpdateSHA1;
            hashCTX->done = (void*) sCCHashFinalSHA1;;
            CC_SHA1_Init(&hashCTX->state.ccSHA1_state);
            break;
            
        case kHASH_Algorithm_SHA224:
            hashCTX->ccAlgor = kCCHmacAlgSHA224;
            hashCTX->hashsize = 28;
            hashCTX->process = (void*) sCCHashUpdateSHA224;
            hashCTX->done = (void*) sCCHashFinalSHA224;
            CC_SHA224_Init(&hashCTX->state.ccSHA256_state);
            break;
            
        case kHASH_Algorithm_SHA256:
            hashCTX->ccAlgor = kCCHmacAlgSHA256;
            hashCTX->hashsize = 32;
            hashCTX->process = (void*) sCCHashUpdateSHA256;
            hashCTX->done = (void*) sCCHashFinalSHA256;;
            CC_SHA256_Init(&hashCTX->state.ccSHA256_state);
            break;
            
        case kHASH_Algorithm_SHA384:
            hashCTX->ccAlgor = kCCHmacAlgSHA384;
            hashCTX->hashsize = 48;
            hashCTX->process = (void*) sCCHashUpdateSHA384;
            hashCTX->done = (void*) sCCHashFinalSHA384;
            CC_SHA384_Init(&hashCTX->state.ccSHA512_state);
            break;
            
        case kHASH_Algorithm_SHA512:
            hashCTX->ccAlgor = kCCHmacAlgSHA512;
            hashCTX->hashsize = 64;
            hashCTX->process = (void*) sCCHashUpdateSHA512;
            hashCTX->done = (void*) sCCHashFinalSHA512;
            CC_SHA512_Init(&hashCTX->state.ccSHA512_state);
            break;
            
        default:
            hashCTX->ccAlgor =  kCCHmacAlgInvalid;
            break;
    }
    
    if(hashCTX->ccAlgor == kCCHmacAlgInvalid)
    {
        desc = sDescriptorForHash(algorithm);
        hashCTX->hashsize = desc->hashsize;
        hashCTX->process = (void*) desc->process;
        hashCTX->done =     (void*) desc->done;
        
        if(IsNull(desc))
            RETERR( kC4Err_BadHashNumber);
        
        if(desc->init)
            err = (desc->init)(&hashCTX->state.tc_state);
        CKERR;
        
    }
    
#else
    
    desc = sDescriptorForHash(algorithm);
    hashCTX->hashsize = desc->hashsize;
    hashCTX->process = (void*) desc->process;
    hashCTX->done =     (void*) desc->done;
    
    if(IsNull(desc))
        RETERR( kC4Err_BadHashNumber);
    
    
    if(desc->init)
        err = (desc->init)(&hashCTX->state.tc_state);
    CKERR;
    
#endif
    
    *ctx = hashCTX;
    
done:
    
    
    if(IsC4Err(err))
    {
        if(IsntNull(hashCTX))
        {
            XFREE(hashCTX);
        }
    }
    
    return err;
    
}

C4Err HASH_Update(HASH_ContextRef ctx, const void *data, size_t dataLength)
{
    int             err = kC4Err_NoErr;
    //    const struct    ltc_hash_descriptor* desc = NULL;
    
    validateHASHContext(ctx);
    ValidateParam(data);
    
    if(ctx->process)
        err = (ctx->process)(&ctx->state,  data, dataLength );
    //
    //
    //    desc = sDescriptorForHash(ctx->algor);
    //
    //    if(IsNull(desc))
    //        RETERR( kC4Err_BadHashNumber);
    //
    //    if(desc->process)
    //        err = (desc->process)(&ctx->state.tc_state,data,  dataLength );
    //    CKERR;
    //
    //done:
    
    return err;
    
}



C4Err HASH_Final(HASH_ContextRef  ctx, void *hashOut)
{
    int             err = kC4Err_NoErr;
    //    const struct    ltc_hash_descriptor* desc = NULL;
    
    validateHASHContext(ctx);
    
    if(ctx->done)
        err = (ctx->done)(&ctx->state, hashOut );
    //
    //
    //    desc = sDescriptorForHash(ctx->algor);
    //
    //    if(IsNull(desc))
    //        RETERR( kC4Err_BadHashNumber);
    //
    //    if(desc->done)
    //        err = (desc->done)(&ctx->state.tc_state, hashOut );
    //    CKERR;
    //
    //done:
    //
    return err;
}

void HASH_Free(HASH_ContextRef  ctx)
{
    if(sHASH_ContextIsValid(ctx))
    {
        ZERO(ctx, sizeof(HASH_Context));
        XFREE(ctx);
    }
}

C4Err HASH_GetSize(HASH_ContextRef  ctx, size_t *hashSize)
{
    int             err = kC4Err_NoErr;
    
    validateHASHContext(ctx);
    
    *hashSize = ctx->hashsize;
    
    return err;
}


C4Err HASH_DO(HASH_Algorithm algorithm, const unsigned char *in, unsigned long inlen, unsigned long outLen, uint8_t *out)
{
    
    C4Err             err         = kC4Err_NoErr;
    HASH_ContextRef     hashRef     = kInvalidHASH_ContextRef;
    uint8_t             hashBuf[128];
    uint8_t             *p = (outLen < sizeof(hashBuf))?hashBuf:out;
    
    
#if  _USES_COMMON_CRYPTO_
    
    /* use apple algorithms if possible*/
    switch(algorithm)
    {
        case kHASH_Algorithm_MD5:
            CC_MD5(in, (CC_LONG) inlen, hashBuf);
            goto complete;
            break;
            
        case  kHASH_Algorithm_SHA1:
            CC_SHA1(in, (CC_LONG) inlen, hashBuf);
            goto complete;
            break;
            
        case  kHASH_Algorithm_SHA224:
            CC_SHA224(in, (CC_LONG) inlen, hashBuf);
            goto complete;
            break;
            
        case  kHASH_Algorithm_SHA256:
            CC_SHA256(in, (CC_LONG) inlen, hashBuf);
            goto complete;
            break;
            
        case  kHASH_Algorithm_SHA384:
            CC_SHA384(in, (CC_LONG) inlen, hashBuf);
            goto complete;
            break;
            
        case  kHASH_Algorithm_SHA512:
            CC_SHA512(in, (CC_LONG) inlen, hashBuf);
            goto complete;
            break;
            
        default:
            break;
    }
    
#endif
    
    err = HASH_Init( algorithm, & hashRef); CKERR;
    err = HASH_Update( hashRef, in,  inlen); CKERR;
    err = HASH_Final( hashRef, p); CKERR;
    
complete:
    if((err == kC4Err_NoErr) & (p!= out))
        COPY(hashBuf, out, outLen);
    
done:
    if(!IsNull(hashRef))
        HASH_Free(hashRef);
    
    return err;
}

