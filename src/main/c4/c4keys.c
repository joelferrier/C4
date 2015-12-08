//
//  c4Key.c
//  C4
//
//  Created by vincent Moscaritolo on 11/9/15.
//  Copyright © 2015 4th-A Technologies, LLC. All rights reserved.
//

#include "c4internal.h"


#include <yajl_parse.h>
#include <yajl_gen.h>

#ifdef __clang__
#pragma mark - YAJL memory management
#endif


#define CKYJAL  if((stat != yajl_gen_status_ok)) {\
printf("ERROR %d (%d)  %s:%d \n",  err, stat, __FILE__, __LINE__); \
err = kC4Err_CorruptData; \
goto done; }


static void yajlFree(void * ctx, void * ptr)
{
    XFREE(ptr);
}

static void * yajlMalloc(void * ctx, size_t sz)
{
    return XMALLOC(sz);
}

static void * yajlRealloc(void * ctx, void * ptr, size_t sz)
{
    
    return XREALLOC(ptr, sz);
}


#define CMP2(b1, l1, b2, l2)							\
(((l1) == (l2)) && (memcmp((void *)(b1), (void *)(b2), (l1)) == 0))

#define STRCMP2(s1, s2) \
(CMP2((s1), strlen(s1), (s2), strlen(s2)))


#define kC4KeyProtocolVersion  0x01

#define K_KEYTYPE           "keyType"
#define K_KEYSUITE          "keySuite"
#define K_KEYDATA           "keyData"

#define K_INDEX             "index"
#define K_THRESHLOLD        "theshold"
#define K_SHAREHASH         "sharehash"

#define K_KEYSUITE_AES128     "AES-128"
#define K_KEYSUITE_AES192     "AES-192"
#define K_KEYSUITE_AES256     "AES-256"
#define K_KEYSUITE_2FISH256   "Twofish-256"
#define K_KEYSUITE_3FISH256   "ThreeFish-256"
#define K_KEYSUITE_3FISH512   "ThreeFish-512"
#define K_KEYSUITE_3FISH1024  "ThreeFish-1024"
#define K_KEYSUITE_SPLIT      "Shamir"

#define K_KEYSUITE_ECC384     "ecc384"
#define K_KEYSUITE_ECC414     "Curve41417"

#define K_PROP_VERSION          "version"
#define K_PROP_ENCODING         "encoding"
#define K_PROP_SALT             "salt"
#define K_PROP_ROUNDS           "rounds"
#define K_PROP_MAC              "mac"
#define K_PROP_ENCRYPTED        "encrypted"
#define K_PROP_KEYID            "keyID"
#define K_PROP_KEYIDSTR         "keyID-String"

char *const kC4KeyProp_KeyType          = K_KEYTYPE;
char *const kC4KeyProp_KeySuite     = K_KEYSUITE;
char *const kC4KeyProp_KeyData          = K_KEYDATA;
char *const kC4KeyProp_KeyID            = K_PROP_KEYID;
char *const kC4KeyProp_KeyIDString      = K_PROP_KEYIDSTR;


static char *const kC4KeyProp_SCKeyVersion      = K_PROP_VERSION;
static char *const kC4KeyProp_Encoding          = K_PROP_ENCODING;
static char *const kC4KeyProp_Encoding_PBKDF2_AES256    = "pbkdf2-AES256";
static char *const kC4KeyProp_Encoding_PBKDF2_2FISH256  = "pbkdf2-Twofish-256";

static char *const kC4KeyProp_Encoding_PUBKEY_ECC384   =  "ECC-384";
static char *const kC4KeyProp_Encoding_PUBKEY_ECC414   =  "Curve41417";
static char *const kC4KeyProp_Salt              = K_PROP_SALT;
static char *const kC4KeyProp_Rounds            = K_PROP_ROUNDS;
static char *const kC4KeyProp_Mac               = K_PROP_MAC;
static char *const kC4KeyProp_EncryptedKey      = K_PROP_ENCRYPTED;

static char *const kC4KeyProp_ShareIndex      = K_INDEX;
static char *const kC4KeyProp_ShareThreshold  = K_THRESHLOLD;
static char *const kC4KeyProp_ShareHash        = K_SHAREHASH;


typedef struct C4KeyPropertyInfo  C4KeyPropertyInfo;

struct C4KeyPropertyInfo
{
    char      *const name;
    C4KeyPropertyType type;
    bool              readOnly;
} ;


static C4KeyPropertyInfo sPropertyTable[] = {
    
    { K_PROP_VERSION,           C4KeyPropertyType_Numeric,  true},
    { K_KEYTYPE,                C4KeyPropertyType_Numeric,  true},
    { K_KEYSUITE,           C4KeyPropertyType_Numeric,  true},
    { K_KEYDATA,                C4KeyPropertyType_Binary,  true},

    { K_PROP_ENCODING,          C4KeyPropertyType_UTF8String,  true},
    { K_PROP_SALT,              C4KeyPropertyType_Binary,  true},
    { K_PROP_ROUNDS,            C4KeyPropertyType_Numeric,  true},
    { K_PROP_MAC,               C4KeyPropertyType_Binary,  true},
    { K_PROP_ENCRYPTED,         C4KeyPropertyType_Binary,  true},
    { K_PROP_KEYID,             C4KeyPropertyType_Binary,  true},
    { K_PROP_KEYIDSTR,          C4KeyPropertyType_UTF8String,  true},

    { K_SHAREHASH,              C4KeyPropertyType_Binary,  true},
    { K_INDEX,                  C4KeyPropertyType_Numeric,  true},
    { K_THRESHLOLD,              C4KeyPropertyType_Numeric,  true},

    { NULL,                     C4KeyPropertyType_Invalid,  true},
};


#ifdef __clang__
#pragma mark - Key utilities.
#endif

static char *cipher_algor_table(Cipher_Algorithm algor)
{
    switch (algor )
    {
        case kCipher_Algorithm_AES128: 		return (K_KEYSUITE_AES128);
        case kCipher_Algorithm_AES192: 		return (K_KEYSUITE_AES192);
        case kCipher_Algorithm_AES256: 		return (K_KEYSUITE_AES256);
        case kCipher_Algorithm_2FISH256:    return (K_KEYSUITE_2FISH256);
            
        case kCipher_Algorithm_3FISH256:    return (K_KEYSUITE_3FISH256);
        case kCipher_Algorithm_3FISH512:    return (K_KEYSUITE_3FISH512);
        case kCipher_Algorithm_3FISH1024:   return (K_KEYSUITE_3FISH1024);
   
        case kCipher_Algorithm_SharedKey: 		return (K_KEYSUITE_SPLIT);
            
  
        default:				return (("Invalid"));
    }
}


static bool sC4KeyContextIsValid( const C4KeyContextRef  ref)
{
    bool       valid	= false;
    
    valid	= IsntNull( ref ) && ref->magic	 == kC4KeyContextMagic;
    
    return( valid );
}

#define validateC4KeyContext( s )		\
ValidateParam( sC4KeyContextIsValid( s ) )


#ifdef __clang__
#pragma mark - Key property management.
#endif

static C4KeyProperty* sFindProperty(C4KeyContext *ctx, const char *propName )
{
    C4KeyProperty* prop = ctx->propList;
    
    while(prop)
    {
        if(CMP2(prop->prop, strlen((char *)(prop->prop)), propName, strlen(propName)))
        {
            break;
        }else
            prop = prop->next;
    }
    
    return prop;
}

static void sInsertProperty(C4KeyContext *ctx, const char *propName,
                            C4KeyPropertyType propType, void *data,  size_t  datSize)
{
    C4KeyProperty* prop = sFindProperty(ctx,propName);
    if(!prop)
    {
        prop = XMALLOC(sizeof(C4KeyProperty));
        ZERO(prop,sizeof(C4KeyProperty));
        prop->prop = (uint8_t *)strndup(propName, strlen(propName));
        prop->next = ctx->propList;
        ctx->propList = prop;
    }
    
    if(prop->value) XFREE(prop->value);
    prop->value = XMALLOC(datSize);
    prop->type = propType;
    COPY(data, prop->value, datSize );
    prop->valueLen = datSize;
    
};


static void sCloneProperties(C4KeyContext *src, C4KeyContext *dest )
{
    C4KeyProperty* sprop = NULL;
    C4KeyProperty** lastProp = &dest->propList;
    
    for(sprop = src->propList; sprop; sprop = sprop->next)
    {
        C4KeyProperty* newProp =  XMALLOC(sizeof(C4KeyProperty));
        ZERO(newProp,sizeof(C4KeyProperty));
        newProp->prop = (uint8_t *)strndup((char *)(sprop->prop), strlen((char *)(sprop->prop)));
        newProp->type = sprop->type;
        newProp->value = XMALLOC(sprop->valueLen);
        COPY(sprop->value, newProp->value, sprop->valueLen );
        newProp->valueLen = sprop->valueLen;
        *lastProp = newProp;
        lastProp = &newProp->next;
    }
    *lastProp = NULL;
    
}



C4Err C4Key_SetProperty( C4KeyContextRef ctx,
                          const char *propName, C4KeyPropertyType propType,
                          void *data,  size_t  datSize)
{
    
    C4Err               err = kC4Err_NoErr;
    C4KeyPropertyInfo  *propInfo = NULL;
    bool found = false;
    
    validateC4KeyContext(ctx);
    
    for(propInfo = sPropertyTable; propInfo->name; propInfo++)
    {
        if(CMP2(propName, strlen(propName), propInfo->name, strlen(propInfo->name)))
        {
            if(propInfo->readOnly)
                RETERR(kC4Err_BadParams);
            
            if(propType != propInfo->type)
                RETERR(kC4Err_BadParams);
            
            found = true;
            break;
        }
    }
    
    if(!found)
        sInsertProperty(ctx, propName, propType, data, datSize);CKERR;
    
done:
    return err;
    
}



static C4Err s4Key_GetPropertyInternal( C4KeyContextRef ctx,
                                             const char *propName, C4KeyPropertyType *outPropType,
                                             void *outData, size_t bufSize, size_t *datSize, bool doAlloc,
                                             uint8_t** allocBuffer)
{
    C4Err               err = kC4Err_NoErr;
    C4KeyPropertyInfo   *propInfo   = NULL;
    C4KeyProperty*      otherProp   = NULL;
    C4KeyPropertyType   propType    = C4KeyPropertyType_Invalid;
    bool                found       = false;
    
    size_t          actualLength = 0;
    uint8_t*        buffer = NULL;
    
    if(datSize)
        *datSize = 0;
    
    // write code here to process internal properties
    for(propInfo = sPropertyTable;propInfo->name; propInfo++)
    {
        if(CMP2(propName, strlen(propName), propInfo->name, strlen(propInfo->name)))
        {
            propType = propInfo->type;
            
            found = true;
            
            if(STRCMP2(propName, kC4KeyProp_KeyType))
            {
                actualLength =  sizeof(C4KeyType);
            }
            else if(STRCMP2(propName, kC4KeyProp_KeySuite))
            {
                actualLength =  sizeof(uint32_t);
            }
            else if(STRCMP2(propName, kC4KeyProp_KeyData))
            {
                switch (ctx->type) {
                    case kC4KeyType_Symmetric:
                        actualLength = ctx->sym.keylen;
                        break;
                        
                    case kC4KeyType_Tweekable:
                         actualLength = ctx->tbc.keybits >> 3 ;
                         break;
                        
                    case kC4KeyType_PublicEncrypted:
                    case kC4KeyType_PBKDF2:
                    default:
                        RETERR(kC4Err_BadParams);
                 }
            }
            else if(STRCMP2(propName, kC4KeyProp_KeyID))
            {
                switch (ctx->type) {
                    case kC4KeyType_PublicEncrypted:
                        actualLength = sizeof(ctx->publicKeyEncoded.keyID);
                        break;
  
                    default:
                        RETERR(kC4Err_BadParams);
                }
            }
            
            else if(STRCMP2(propName, kC4KeyProp_KeyIDString))
            {
                switch (ctx->type) {
                    case kC4KeyType_PublicEncrypted:
                         actualLength = (((sizeof(ctx->publicKeyEncoded.keyID) + 2) / 3) * 4) + 1;
                          break;
                        
                    default:
                        RETERR(kC4Err_BadParams);
                }
            }
 
            
            else
                found = false;
            
            break;

        }
     }
    
    if(!found)
    {
        otherProp = sFindProperty(ctx,propName);
        if(otherProp)
        {
            actualLength = (unsigned long)(otherProp->valueLen);
            propType = otherProp->type;
            found = true;
        }
    }
    
    if(!found)
        RETERR(kC4Err_BadParams);
    
    
    if(!actualLength)
        goto done;
    
    if(doAlloc)
    {
        buffer = XMALLOC(actualLength + sizeof('\0')); CKNULL(buffer);
        *allocBuffer = buffer;
    }
    else
    {
        actualLength = (actualLength < (unsigned long)bufSize) ? actualLength : (unsigned long)bufSize;
        buffer = outData;
    }
    
    if(STRCMP2(propName, kC4KeyProp_KeyType))
    {
        COPY(&ctx->type, buffer, actualLength);
    }
    else if(STRCMP2(propName, kC4KeyProp_KeySuite))
    {
        switch (ctx->type) {
            case kC4KeyType_Symmetric:
                 COPY(&ctx->sym.symAlgor , buffer, actualLength);
                break;
                
            case kC4KeyType_Tweekable:
                COPY(&ctx->tbc.tbcAlgor , buffer, actualLength);
                break;
                
            case kC4KeyType_PublicEncrypted:
            case kC4KeyType_PBKDF2:
            default:
                RETERR(kC4Err_BadParams);
                
        }
    }
    else if(STRCMP2(propName, kC4KeyProp_KeyData))
    {
        switch (ctx->type) {
            case kC4KeyType_Symmetric:
                COPY(&ctx->sym.symKey , buffer, actualLength);
                break;
                
            case kC4KeyType_Tweekable:
                COPY(&ctx->tbc.key , buffer, actualLength);
                 break;
                
            case kC4KeyType_PublicEncrypted:
            case kC4KeyType_PBKDF2:
            default:
                RETERR(kC4Err_BadParams);
        }
    }
    if(STRCMP2(propName, kC4KeyProp_KeyID))
    {
        switch (ctx->type) {
            case kC4KeyType_PublicEncrypted:
              
                COPY(&ctx->publicKeyEncoded.keyID , buffer, actualLength);
               break;
                
            default:
                RETERR(kC4Err_BadParams);
        }
    }
    else if(STRCMP2(propName, kC4KeyProp_KeyIDString))
    {
        switch (ctx->type) {
            case kC4KeyType_PublicEncrypted:
                err = base64_encode(ctx->publicKeyEncoded.keyID, sizeof(ctx->publicKeyEncoded.keyID), buffer, &actualLength); CKERR;
                actualLength++;
                buffer[actualLength]= '\0';
              break;
                
            default:
                RETERR(kC4Err_BadParams);
        }
    }

    
    else if(otherProp)
    {
        COPY(otherProp->value,  buffer, actualLength);
        propType = otherProp->type;
    }

    
    if(outPropType)
        *outPropType = propType;
    
    if(datSize)
        *datSize = actualLength;
    
    
done:
    return err;

    
}

C4Err C4Key_GetProperty( C4KeyContextRef ctx,
                          const char *propName,
                          C4KeyPropertyType *outPropType, void *outData, size_t bufSize, size_t *datSize)
{
    C4Err               err = kC4Err_NoErr;
    
    validateC4KeyContext(ctx);
    ValidateParam(outData);
    
    if ( IsntNull( outData ) )
    {
        ZERO( outData, bufSize );
    }
    
    err =  s4Key_GetPropertyInternal(ctx, propName, outPropType, outData, bufSize, datSize, false, NULL);
    
    return err;
}



C4Err SCKeyGetAllocatedProperty( C4KeyContextRef ctx,
                                   const char *propName,
                                   C4KeyPropertyType *outPropType, void **outData, size_t *datSize)
{
    C4Err               err = kC4Err_NoErr;
  
    validateC4KeyContext(ctx);
    ValidateParam(outData);
    
    err =  s4Key_GetPropertyInternal(ctx, propName, outPropType, NULL, 0, datSize, true, (uint8_t**) outData);
    
    return err;
}



#ifdef __clang__
#pragma mark - create Key.
#endif

C4Err C4Key_NewSymmetric(Cipher_Algorithm       algorithm,
                             const void             *key,
                             C4KeyContextRef    *ctxOut)
{
    C4Err               err = kC4Err_NoErr;
    C4KeyContext*    keyCTX  = NULL;
    
    ValidateParam(ctxOut);
  
    int             keylen  = 0;
    
    switch(algorithm)
    {
        case kCipher_Algorithm_AES128:
            keylen = 128 >> 3;
             break;
            
        case kCipher_Algorithm_AES192:
            keylen = 192 >> 3;
             break;
 
        case kCipher_Algorithm_AES256:
            keylen = 256 >> 3;
             break;
            
        case kCipher_Algorithm_2FISH256:
            keylen = 256 >> 3;
             break;
            
        default:
            RETERR(kC4Err_BadCipherNumber);
    }

    
    keyCTX = XMALLOC(sizeof (C4KeyContext)); CKNULL(keyCTX);
    keyCTX->magic = kC4KeyContextMagic;
    keyCTX->type  = kC4KeyType_Symmetric;
    keyCTX->propList = NULL;
 
    keyCTX->sym.symAlgor = algorithm;
    keyCTX->sym.keylen = keylen;
    
    // leave null bytes at end of key, for odd size keys (like 192)
    ZERO(keyCTX->sym.symKey, sizeof(keyCTX->sym.symKey) );
    COPY(key, keyCTX->sym.symKey, keylen);
    
    *ctxOut = keyCTX;
    
done:
    if(IsC4Err(err))
    {
        if(keyCTX)
        {
            memset(keyCTX, sizeof (C4KeyContext), 0);
            XFREE(keyCTX);
        }
    }
     return err;
}


C4Err C4Key_NewTBC(     Cipher_Algorithm       algorithm,
                             const void     *key,
                            C4KeyContextRef   *ctxOut)
{
    C4Err               err = kC4Err_NoErr;
    C4KeyContext*    keyCTX  = NULL;
    
    ValidateParam(ctxOut);
    
    int             keybits  = 0;
    
    switch(algorithm)
    {
        case kCipher_Algorithm_3FISH256:
            keybits = Threefish256;
            break;
            
        case kCipher_Algorithm_3FISH512:
            keybits = Threefish512;
            break;
            
        case kCipher_Algorithm_3FISH1024:
            keybits = Threefish1024 ;
            break;
            
        default:
            RETERR(kC4Err_BadCipherNumber);
    }
    
    
    
    keyCTX = XMALLOC(sizeof (C4KeyContext)); CKNULL(keyCTX);
    keyCTX->magic = kC4KeyContextMagic;
    keyCTX->type  = kC4KeyType_Tweekable;
    keyCTX->propList = NULL;
   
    keyCTX->tbc.tbcAlgor = algorithm;
    keyCTX->tbc.keybits = keybits;
   
    Skein_Get64_LSB_First(keyCTX->tbc.key, key, keybits >>5);   /* bits to words */
    
    *ctxOut = keyCTX;
    
done:
    if(IsC4Err(err))
    {
        if(keyCTX)
        {
            memset(keyCTX, sizeof (C4KeyContext), 0);
            XFREE(keyCTX);
        }
    }
    return err;
}


C4Err C4Key_NewShare(SHARES_ShareInfo   *share,
                     C4KeyContextRef    *ctxOut)
{
    C4Err               err = kC4Err_NoErr;
    C4KeyContext*    keyCTX  = NULL;
    
    ValidateParam(ctxOut);
    ValidateParam(share->shareSecretLen <= 64);
    
    keyCTX = XMALLOC(sizeof (C4KeyContext)); CKNULL(keyCTX);
    keyCTX->magic = kC4KeyContextMagic;
    keyCTX->type  = kC4KeyType_Share;
    keyCTX->propList = NULL;
    keyCTX->share.xCoordinate = share->xCoordinate;
    keyCTX->share.threshold   = share->threshold;
    COPY(share->shareHash, keyCTX->share.shareHash, kC4ShareInfo_HashBytes);
    keyCTX->share.shareSecretLen    = share->shareSecretLen;
    COPY(share->shareSecret, keyCTX->share.shareSecret, share->shareSecretLen);
    
    *ctxOut = keyCTX;
    
done:
    if(IsC4Err(err))
    {
        if(keyCTX)
        {
            memset(keyCTX, sizeof (C4KeyContext), 0);
            XFREE(keyCTX);
        }
    }
    return err;

}

void C4Key_Free(C4KeyContextRef ctx)
{
    if(sC4KeyContextIsValid(ctx))
    {
        
        C4KeyProperty *prop = ctx->propList;
        
        while(prop)
        {
            C4KeyProperty *nextProp = prop->next;
            XFREE(prop->prop);
            XFREE(prop->value);
            XFREE(prop);
            prop = nextProp;
        }

        ZERO(ctx, sizeof(C4KeyContext));
        XFREE(ctx);
    }
}

C4Err C4Key_Copy(C4KeyContextRef ctx, C4KeyContextRef *ctxOut)
{
    C4Err               err = kC4Err_NoErr;
    C4KeyContext*    keyCTX  = NULL;
  
    validateC4KeyContext(ctx);
    ValidateParam(ctxOut);
    
    
    keyCTX = XMALLOC(sizeof (C4KeyContext)); CKNULL(keyCTX);
    keyCTX->magic = kC4KeyContextMagic;
    keyCTX->type = ctx->type;
    
    switch (ctx->type)
    {
        case kC4KeyType_Symmetric:
            keyCTX->sym = ctx->sym;
            break;
            
        case kC4KeyType_Tweekable:
            keyCTX->tbc = ctx->tbc;
            break;

        case kC4KeyType_PBKDF2:
            keyCTX->pbkdf2 = ctx->pbkdf2;
            break;
            
        case kC4KeyType_PublicEncrypted:
            keyCTX->publicKeyEncoded = ctx->publicKeyEncoded;
            break;
            
        case kC4KeyType_Share:
            keyCTX->share = ctx->share;
            break;
            
        default:
            break;
    }
   
    sCloneProperties(ctx, keyCTX);
    
    *ctxOut = keyCTX;
    
done:
    if(IsC4Err(err))
    {
        if(keyCTX)
        {
            memset(keyCTX, sizeof (C4KeyContext), 0);
            XFREE(keyCTX);
        }
    }
       return err;

}


#ifdef __clang__
#pragma mark - export key.
#endif


static C4Err sPASSPHRASE_HASH( const uint8_t  *key,
                                unsigned long  key_len,
                                uint8_t       *salt,
                                unsigned long  salt_len,
                                uint32_t        roundsIn,
                                uint8_t        *mac_buf,
                                unsigned long  mac_len)
{
    C4Err           err = kC4Err_NoErr;
  
    MAC_ContextRef  macRef     = kInvalidMAC_ContextRef;
    
    uint32_t        rounds = roundsIn;
    uint8_t         L[4];
    char*           label = "passphrase-hash";
    
    L[0] = (salt_len >> 24) & 0xff;
    L[1] = (salt_len >> 16) & 0xff;
    L[2] = (salt_len >> 8) & 0xff;
    L[3] = salt_len & 0xff;

    err = MAC_Init(kMAC_Algorithm_SKEIN,
                   kHASH_Algorithm_SKEIN256,
                   key, key_len, &macRef); CKERR
    
    MAC_Update(macRef,  "\x00\x00\x00\x01",  4);
    MAC_Update(macRef,  label,  strlen(label));
    
    err = MAC_Update( macRef, salt, salt_len); CKERR;
    MAC_Update(macRef,  L,  4);
    
    err = MAC_Update( macRef, &rounds, sizeof(rounds)); CKERR;
    MAC_Update(macRef,  "\x00\x00\x00\x04",  4);
  
    size_t mac_len_SZ = (size_t)mac_len;
    err = MAC_Final( macRef, mac_buf, &mac_len_SZ); CKERR;
    
done:
    
    MAC_Free(macRef);
    
    return err;
}

static C4Err sKEY_HASH( const uint8_t  *key,
                        unsigned long  key_len,
                         C4KeyType     keyTypeIn,
                         int           keyAlgorithmIn,
                        uint8_t        *mac_buf,
                        unsigned long  mac_len)
{
    C4Err           err = kC4Err_NoErr;
    
    MAC_ContextRef  macRef     = kInvalidMAC_ContextRef;
    
    uint32_t        keyType = keyTypeIn;
    uint32_t        algorithm = keyAlgorithmIn;
 
    char*           label = "key-hash";
    
    err = MAC_Init(kMAC_Algorithm_SKEIN,
                   kHASH_Algorithm_SKEIN256,
                   key, key_len, &macRef); CKERR
    
    MAC_Update(macRef,  "\x00\x00\x00\x01",  4);
    MAC_Update(macRef,  label,  strlen(label));
    
    err = MAC_Update( macRef, &keyType, sizeof(keyType)); CKERR;
    MAC_Update(macRef,  "\x00\x00\x00\x04",  4);
  
    err = MAC_Update( macRef, &algorithm, sizeof(algorithm)); CKERR;
    MAC_Update(macRef,  "\x00\x00\x00\x04",  4);

    size_t mac_len_SZ = (size_t)mac_len;
    err = MAC_Final( macRef, mac_buf, &mac_len_SZ); CKERR;
    
done:
    
    MAC_Free(macRef);
    
    return err;
}



static yajl_gen_status sGenPropStrings(C4KeyContextRef ctx, yajl_gen g)

{
    static const char *kRfc339Format = "%Y-%m-%dT%H:%M:%SZ";
    

    C4Err           err = kC4Err_NoErr;
    yajl_gen_status     stat = yajl_gen_status_ok;
    
    C4KeyProperty *prop = ctx->propList;
    while(prop)
    {
        stat = yajl_gen_string(g, prop->prop, strlen((char *)(prop->prop))) ; CKYJAL;
        switch(prop->type)
        {
            case C4KeyPropertyType_UTF8String:
                stat = yajl_gen_string(g, prop->value, prop->valueLen) ; CKYJAL;
                
                break;
                
            case C4KeyPropertyType_Binary:
            {
                size_t propLen =  prop->valueLen*4;
                uint8_t     *propBuf =  XMALLOC(propLen);
 
                base64_encode(prop->value, prop->valueLen, propBuf, &propLen);
                stat = yajl_gen_string(g, propBuf, (size_t)propLen) ; CKYJAL;
                XFREE(propBuf);
            }
                break;
                
            case C4KeyPropertyType_Time:
            {
                uint8_t     tempBuf[32];
                size_t      tempLen;
                time_t      gTime;
                struct      tm *nowtm;
                
                COPY(prop->value, &gTime, sizeof(gTime));
                nowtm = gmtime(&gTime);
                tempLen = strftime((char *)tempBuf, sizeof(tempBuf), kRfc339Format, nowtm);
                stat = yajl_gen_string(g, tempBuf, tempLen) ; CKYJAL;
            }
                break;
                
            default:
                yajl_gen_string(g, (uint8_t *)"NULL", 4) ;
                break;
        }
        
        prop = prop->next;
    }
    
done:
    return err;
}

/*

 {
 "version": 1,
 "keySuite": "aes256",
 "encoding": "pbkdf2",
 "salt": "qzbdGRxw4js=",
 "rounds": 192307,
 "hash": "KSA9JcWT/i4TvAIC3lYKrQ==",
 "encrypted": "3+lt1R5cYBO7aNxp/WA8xbjieKtblezx3M8siskX40I="
 }

 */

C4Err C4Key_SerializeToPassPhrase(C4KeyContextRef  ctx,
                               const uint8_t       *passphrase,
                               size_t           passphraseLen,
                               uint8_t          **outData,
                               size_t           *outSize)
{
    C4Err           err = kC4Err_NoErr;
    yajl_gen_status     stat = yajl_gen_status_ok;
    
    uint8_t             *yajlBuf = NULL;
    size_t              yajlLen = 0;
    yajl_gen            g = NULL;
 
    uint8_t             tempBuf[1024];
    size_t              tempLen;
    uint8_t             *outBuf = NULL;
    
    uint32_t        rounds;
    uint8_t         keyHash[kC4KeyPBKDF2_HashBytes] = {0};
    uint8_t         salt[kC4KeyPBKDF2_SaltBytes] = {0};
   
    uint8_t         unlocking_key[32] = {0};
    
    Cipher_Algorithm    encyptAlgor = kCipher_Algorithm_Invalid;
    uint8_t             encrypted_key[128] = {0};
    size_t              keyBytes = 0;
    void*               keyToEncrypt = NULL;
    
    char*           encodingPropString = NULL;
    char*           keySuiteString = "Invalid";
    
    yajl_alloc_funcs allocFuncs = {
        yajlMalloc,
        yajlRealloc,
        yajlFree,
        (void *) NULL
    };
    

    validateC4KeyContext(ctx);
    ValidateParam(passphrase);
    ValidateParam(outData);
    
    switch (ctx->type)
    {
        case kC4KeyType_Symmetric:
            keyBytes = ctx->sym.keylen ;
            keyToEncrypt = ctx->sym.symKey;
            
            switch (ctx->sym.symAlgor) {
                case kCipher_Algorithm_2FISH256:
                        encyptAlgor = kCipher_Algorithm_2FISH256;
                        encodingPropString =  kC4KeyProp_Encoding_PBKDF2_2FISH256;
                        break;
                    
                case kCipher_Algorithm_AES192:
                    encyptAlgor = kCipher_Algorithm_AES256;
                    encodingPropString =  kC4KeyProp_Encoding_PBKDF2_AES256;
    
                    //  pad the end  (treat it like it was 256 bits)
                    ZERO(&ctx->sym.symKey[24], 8);
                    keyBytes = 32;
                    break;
                    
                default:
                        encyptAlgor = kCipher_Algorithm_AES256;
                        encodingPropString =  kC4KeyProp_Encoding_PBKDF2_AES256;
                    break;
            }
            
            keySuiteString = cipher_algor_table(ctx->sym.symAlgor);
            break;
   
        case kC4KeyType_Tweekable:
            keyBytes = ctx->tbc.keybits >> 3 ;
            encyptAlgor = kCipher_Algorithm_2FISH256;
            keySuiteString = cipher_algor_table(ctx->tbc.tbcAlgor);
            encodingPropString =  kC4KeyProp_Encoding_PBKDF2_2FISH256;
            keyToEncrypt = ctx->tbc.key;

          break;
            
        case kC4KeyType_Share:
            keyBytes = (int)ctx->share.shareSecretLen ;
            encyptAlgor = kCipher_Algorithm_2FISH256;
            keySuiteString = cipher_algor_table(kCipher_Algorithm_SharedKey);
            keyToEncrypt = ctx->share.shareSecret;
            encodingPropString =  kC4KeyProp_Encoding_PBKDF2_2FISH256;
           
            // we only encode block sizes of 16, 32, 48 and 64
            ASSERTERR((keyBytes % 16) != 0, kC4Err_FeatureNotAvailable);
            ASSERTERR(keyBytes > 64, kC4Err_FeatureNotAvailable);
            
              break;
            
        default:
            break;
    }
    
    
    err = RNG_GetBytes( salt, kC4KeyPBKDF2_SaltBytes ); CKERR;
    
    err = PASS_TO_KEY_SETUP(passphraseLen, keyBytes,
                            salt, sizeof(salt),
                             &rounds); CKERR;

    err = PASS_TO_KEY(passphrase, passphraseLen,
                      salt, sizeof(salt), rounds,
                      unlocking_key, sizeof(unlocking_key)); CKERR;
    
    err = sPASSPHRASE_HASH(unlocking_key, sizeof(unlocking_key),
                           salt, sizeof(salt),
                           rounds,
                           keyHash, kC4KeyPBKDF2_HashBytes); CKERR;
 
    err =  ECB_Encrypt(encyptAlgor, unlocking_key, keyToEncrypt, keyBytes, encrypted_key); CKERR;
    
      g = yajl_gen_alloc(&allocFuncs); CKNULL(g);
    
#if DEBUG
    yajl_gen_config(g, yajl_gen_beautify, 1);
#else
    yajl_gen_config(g, yajl_gen_beautify, 0);
    
#endif
    yajl_gen_config(g, yajl_gen_validate_utf8, 1);
    stat = yajl_gen_map_open(g);
    
    stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_SCKeyVersion, strlen(kC4KeyProp_SCKeyVersion)) ; CKYJAL;
    sprintf((char *)tempBuf, "%d", kC4KeyProtocolVersion);
    stat = yajl_gen_number(g, (char *)tempBuf, strlen((char *)tempBuf)) ; CKYJAL;
   
    stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_Encoding, strlen(kC4KeyProp_Encoding)) ; CKYJAL;
    stat = yajl_gen_string(g, (uint8_t *)encodingPropString, strlen(encodingPropString)) ; CKYJAL;
    
    stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_KeySuite, strlen(kC4KeyProp_KeySuite)) ; CKYJAL;
    stat = yajl_gen_string(g, (uint8_t *)keySuiteString, strlen(keySuiteString)) ; CKYJAL;
    
    stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_Salt, strlen(kC4KeyProp_Salt)) ; CKYJAL;
    tempLen = sizeof(tempBuf);
    base64_encode(salt, kC4KeyPBKDF2_SaltBytes, tempBuf, &tempLen);
    
    stat = yajl_gen_string(g, tempBuf, (size_t)tempLen) ; CKYJAL;
    
    stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_Rounds, strlen(kC4KeyProp_Rounds)) ; CKYJAL;
    sprintf((char *)tempBuf, "%d", rounds);
    stat = yajl_gen_number(g, (char *)tempBuf, strlen((char *)tempBuf)) ; CKYJAL;
    
    stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_Mac, strlen(kC4KeyProp_Mac)) ; CKYJAL;
    tempLen = sizeof(tempBuf);
    base64_encode(keyHash, kC4KeyPBKDF2_HashBytes, tempBuf, &tempLen);
    stat = yajl_gen_string(g, tempBuf, (size_t)tempLen) ; CKYJAL;
    
    switch (ctx->type)
    {
        case kC4KeyType_Symmetric:
        case kC4KeyType_Tweekable:
            break;
            
        case kC4KeyType_Share:
            stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_ShareIndex, strlen(kC4KeyProp_ShareIndex)) ; CKYJAL;
            sprintf((char *)tempBuf, "%d", ctx->share.xCoordinate);
            stat = yajl_gen_number(g, (char *)tempBuf, strlen((char *)tempBuf)) ; CKYJAL;
            
            stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_ShareThreshold, strlen(kC4KeyProp_ShareThreshold)) ; CKYJAL;
            sprintf((char *)tempBuf, "%d", ctx->share.threshold);
            stat = yajl_gen_number(g, (char *)tempBuf, strlen((char *)tempBuf)) ; CKYJAL;
            
            stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_ShareHash, strlen(kC4KeyProp_ShareHash)) ; CKYJAL;
            tempLen = sizeof(tempBuf);
            base64_encode(ctx->share.shareHash, kC4ShareInfo_HashBytes, tempBuf, &tempLen);
            stat = yajl_gen_string(g, tempBuf, (size_t)tempLen) ; CKYJAL;
            break;
            
        default:
            break;
    }
    
    stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_EncryptedKey, strlen(kC4KeyProp_EncryptedKey)) ; CKYJAL;
    tempLen = sizeof(tempBuf);
    base64_encode(encrypted_key, keyBytes, tempBuf, &tempLen);
    stat = yajl_gen_string(g, tempBuf, (size_t)tempLen) ; CKYJAL;
 
    err = sGenPropStrings(ctx, g); CKERR;
    
    stat = yajl_gen_map_close(g); CKYJAL;
    stat =  yajl_gen_get_buf(g, (const unsigned char**) &yajlBuf, &yajlLen);CKYJAL;

    
    outBuf = XMALLOC(yajlLen+1); CKNULL(outBuf);
    memcpy(outBuf, yajlBuf, yajlLen);
    outBuf[yajlLen] = 0;
    
    *outData = outBuf;
    
    if(outSize)
        *outSize = yajlLen;
    
 done:
    if(IsntNull(g))
        yajl_gen_free(g);
    
     return err;
   
}

C4Err C4Key_SerializeToPubKey(C4KeyContextRef   ctx,
                              ECC_ContextRef    eccPub,
                              uint8_t          **outData,
                              size_t           *outSize)
{
    C4Err           err = kC4Err_NoErr;
    yajl_gen_status     stat = yajl_gen_status_ok;
    
    uint8_t             *yajlBuf = NULL;
    size_t              yajlLen = 0;
    yajl_gen            g = NULL;
    
    uint8_t             tempBuf[1024];
    size_t              tempLen;
    uint8_t             *outBuf = NULL;
  
    char                curveName[32]  = {0};
  
    uint8_t             keyID[kC4Key_KeyIDBytes];
    size_t              keyIDLen = 0;
  
    uint8_t             keyHash[kC4KeyPublic_Encrypted_HashBytes];
    int                 keyAlgorithm = 0;
    
     uint8_t            encrypted[256] = {0};       // typical 199 bytes
    size_t              encryptedLen = 0;
    
    size_t              keyBytes = 0;
    void*               keyToEncrypt = NULL;
    
     char*              keySuiteString = "Invalid";
    
    yajl_alloc_funcs allocFuncs = {
        yajlMalloc,
        yajlRealloc,
        yajlFree,
        (void *) NULL
    };
    
    
    validateC4KeyContext(ctx);
    validateECCContext(eccPub);
    ValidateParam(outData);
    
    switch (ctx->type)
    {
        case kC4KeyType_Symmetric:
            keyBytes = ctx->sym.keylen ;
            keyToEncrypt = ctx->sym.symKey;
            keyAlgorithm = ctx->sym.symAlgor;
            keySuiteString = cipher_algor_table(ctx->sym.symAlgor);
             break;
            
        case kC4KeyType_Tweekable:
            keyBytes = ctx->tbc.keybits >> 3 ;
            keyToEncrypt = ctx->tbc.key;
            keyAlgorithm = ctx->tbc.tbcAlgor;
            keySuiteString = cipher_algor_table(ctx->tbc.tbcAlgor);
            break;
  
        case kC4KeyType_Share:
            keyBytes = (int)ctx->share.shareSecretLen ;
            keyToEncrypt = ctx->share.shareSecret;
            keyAlgorithm = kCipher_Algorithm_SharedKey;
            keySuiteString = cipher_algor_table(kCipher_Algorithm_SharedKey);
            break;

        default:
            break;
    }
    
    /* limit ECC encryption to <= 512 bits of data */
//    ValidateParam(keyBytes <= (512 >>3));
    
    err = sKEY_HASH(keyToEncrypt, keyBytes, ctx->type,
                    keyAlgorithm, keyHash, kC4KeyPublic_Encrypted_HashBytes ); CKERR;
    
    err = ECC_CurveName(eccPub, curveName, sizeof(curveName), NULL); CKERR;
    err = ECC_PubKeyHash(eccPub, keyID, kC4Key_KeyIDBytes, &keyIDLen);CKERR;
    
    err = ECC_Encrypt(eccPub, keyToEncrypt, keyBytes,  encrypted, sizeof(encrypted), &encryptedLen);CKERR;
    
    g = yajl_gen_alloc(&allocFuncs); CKNULL(g);
    
#if DEBUG
    yajl_gen_config(g, yajl_gen_beautify, 1);
#else
    yajl_gen_config(g, yajl_gen_beautify, 0);
    
#endif
    yajl_gen_config(g, yajl_gen_validate_utf8, 1);
    stat = yajl_gen_map_open(g);
    
    stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_SCKeyVersion, strlen(kC4KeyProp_SCKeyVersion)) ; CKYJAL;
    sprintf((char *)tempBuf, "%d", kC4KeyProtocolVersion);
    stat = yajl_gen_number(g, (char *)tempBuf, strlen((char *)tempBuf)) ; CKYJAL;
    
    stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_Encoding, strlen(kC4KeyProp_Encoding)) ; CKYJAL;
    stat = yajl_gen_string(g, (uint8_t *)curveName, strlen(curveName)) ; CKYJAL;
 
    stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_KeyID, strlen(kC4KeyProp_KeyID)) ; CKYJAL;
    tempLen = sizeof(tempBuf);
    base64_encode(keyID, keyIDLen, tempBuf, &tempLen);
    stat = yajl_gen_string(g, tempBuf, (size_t)tempLen) ; CKYJAL;
    
    stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_KeySuite, strlen(kC4KeyProp_KeySuite)) ; CKYJAL;
    stat = yajl_gen_string(g, (uint8_t *)keySuiteString, strlen(keySuiteString)) ; CKYJAL;
    
   stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_Mac, strlen(kC4KeyProp_Mac)) ; CKYJAL;
    tempLen = sizeof(tempBuf);
    base64_encode(keyHash, kC4KeyPublic_Encrypted_HashBytes, tempBuf, &tempLen);
    stat = yajl_gen_string(g, tempBuf, (size_t)tempLen) ; CKYJAL;
    
 
    switch (ctx->type)
    {
        case kC4KeyType_Symmetric:
        case kC4KeyType_Tweekable:
             break;
            
        case kC4KeyType_Share:
             stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_ShareIndex, strlen(kC4KeyProp_ShareIndex)) ; CKYJAL;
            sprintf((char *)tempBuf, "%d", ctx->share.xCoordinate);
            stat = yajl_gen_number(g, (char *)tempBuf, strlen((char *)tempBuf)) ; CKYJAL;
          
            stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_ShareThreshold, strlen(kC4KeyProp_ShareThreshold)) ; CKYJAL;
            sprintf((char *)tempBuf, "%d", ctx->share.threshold);
            stat = yajl_gen_number(g, (char *)tempBuf, strlen((char *)tempBuf)) ; CKYJAL;

            stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_ShareHash, strlen(kC4KeyProp_ShareHash)) ; CKYJAL;
            tempLen = sizeof(tempBuf);
            base64_encode(ctx->share.shareHash, kC4ShareInfo_HashBytes, tempBuf, &tempLen);
            stat = yajl_gen_string(g, tempBuf, (size_t)tempLen) ; CKYJAL;
            break;
            
        default:
            break;
    }
 
    
    
    stat = yajl_gen_string(g, (uint8_t *)kC4KeyProp_EncryptedKey, strlen(kC4KeyProp_EncryptedKey)) ; CKYJAL;
    tempLen = sizeof(tempBuf);
    base64_encode(encrypted, encryptedLen, tempBuf, &tempLen);
    stat = yajl_gen_string(g, tempBuf, (size_t)tempLen) ; CKYJAL;
    
    err = sGenPropStrings(ctx, g); CKERR;
    
    stat = yajl_gen_map_close(g); CKYJAL;
    stat =  yajl_gen_get_buf(g, (const unsigned char**) &yajlBuf, &yajlLen);CKYJAL;
    
    outBuf = XMALLOC(yajlLen+1); CKNULL(outBuf);
    memcpy(outBuf, yajlBuf, yajlLen);
    outBuf[yajlLen] = 0;
    
    *outData = outBuf;
    if(outSize)
        *outSize = yajlLen;
    
done:
    if(IsntNull(g))
        yajl_gen_free(g);
    
    return err;

}

#ifdef __clang__
#pragma mark - import key.
#endif


#define _base(x) ((x >= '0' && x <= '9') ? '0' : \
(x >= 'a' && x <= 'f') ? 'a' - 10 : \
(x >= 'A' && x <= 'F') ? 'A' - 10 : \
'\255')
#define HEXOF(x) (x - _base(x))


enum C4Key_JSON_Type_
{
    C4Key_JSON_Type_Invalid ,
    C4Key_JSON_Type_BASE ,
    C4Key_JSON_Type_VERSION,
    C4Key_JSON_Type_KEYALGORITHM,
 
    C4Key_JSON_Type_ROUNDS,
    C4Key_JSON_Type_SALT,
    C4Key_JSON_Type_ENCODING,
    C4Key_JSON_Type_MAC,
    C4Key_JSON_Type_ENCRYPTED_SYMKEY,
    C4Key_JSON_Type_KEYID,
    C4Key_JSON_Type_SYMKEY,
    
    C4Key_JSON_Type_SHAREHASH,
    C4Key_JSON_Type_THRESHOLD,
    C4Key_JSON_Type_SHAREINDEX,
    
    C4Key_JSON_Type_PROPERTY,
    
    ENUM_FORCE( C4Key_JSON_Type_ )
};
ENUM_TYPEDEF( C4Key_JSON_Type_, C4Key_JSON_Type   );

struct C4KeyJSONcontext
{
    uint8_t             version;    // message version
//    C4KeyContext       key;        // used for decoding messages
 
    C4KeyContext        *keys;     // pointer to array of C4KeyContext
    int                 index;      // current key
    
    int                 level;
    
    C4Key_JSON_Type jType[8];
    void*           jItem;
    size_t*         jItemSize;
    uint8_t*        jTag;
    
 };

typedef struct C4KeyJSONcontext C4KeyJSONcontext;

static time_t parseRfc3339(const unsigned char *s, size_t stringLen)
{
    struct tm tm;
    time_t t;
    const unsigned char *p = s;
    
    if(stringLen < strlen("YYYY-MM-DDTHH:MM:SSZ"))
        return 0;
    
    memset(&tm, 0, sizeof tm);
    
    /* YYYY- */
    if (!isdigit(s[0]) || !isdigit(s[1]) ||  !isdigit(s[2]) || !isdigit(s[3]) || s[4] != '-')
        return 0;
    tm.tm_year = (((s[0] - '0') * 10 + s[1] - '0') * 10 +  s[2] - '0') * 10 + s[3] - '0' - 1900;
    s += 5;
    
    /* mm- */
    if (!isdigit(s[0]) || !isdigit(s[1]) || s[2] != '-')
        return 0;
    tm.tm_mon = (s[0] - '0') * 10 + s[1] - '0';
    if (tm.tm_mon < 1 || tm.tm_mon > 12)
        return 0;
    --tm.tm_mon;	/* 0-11 not 1-12 */
    s += 3;
    
    /* ddT */
    if (!isdigit(s[0]) || !isdigit(s[1]) || toupper(s[2]) != 'T')
        return 0;
    tm.tm_mday = (s[0] - '0') * 10 + s[1] - '0';
    s += 3;
    
    /* HH: */
    if (!isdigit(s[0]) || !isdigit(s[1]) || s[2] != ':')
        return 0;
    tm.tm_hour = (s[0] - '0') * 10 + s[1] - '0';
    s += 3;
    
    /* MM: */
    if (!isdigit(s[0]) || !isdigit(s[1]) || s[2] != ':')
        return 0;
    tm.tm_min = (s[0] - '0') * 10 + s[1] - '0';
    s += 3;
    
    /* SS */
    if (!isdigit(s[0]) || !isdigit(s[1]))
        return 0;
    tm.tm_sec = (s[0] - '0') * 10 + s[1] - '0';
    s += 2;
    
    if (*s == '.') {
        do
            ++s;
        while (isdigit(*s));
    }
    
   	if (toupper(s[0]) == 'Z' &&  ((s-p == stringLen -1) ||  s[1] == '\0'))
        tm.tm_gmtoff = 0;
    else if (s[0] == '+' || s[0] == '-')
    {
        char tzsign = *s++;
        
        /* HH: */
        if (!isdigit(s[0]) || !isdigit(s[1]) || s[2] != ':')
            return 0;
        tm.tm_gmtoff = ((s[0] - '0') * 10 + s[1] - '0') * 3600;
        s += 3;
        
        /* MM */
        if (!isdigit(s[0]) || !isdigit(s[1]) || s[2] != '\0')
            return 0;
        tm.tm_gmtoff += ((s[0] - '0') * 10 + s[1] - '0') * 60;
        
        if (tzsign == '-')
            tm.tm_gmtoff = -tm.tm_gmtoff;
    } else
        return 0;
    
    t = timegm(&tm);
    if (t < 0)
        return 0;
    return t;  
    
    //  	return t - tm.tm_gmtoff;
    
}

static C4Err sParseKeySuiteString(const unsigned char * stringVal,  size_t stringLen,
                                  C4KeyType *keyTypeOut, int32_t *algorithmOut)
{
    
    C4Err               err = kC4Err_NoErr;
    C4KeyType   keyType = kC4KeyType_Invalid;
    int32_t     algorithm = kEnumMaxValue;
    
    
    if(CMP2(stringVal, stringLen, K_KEYSUITE_AES128, strlen(K_KEYSUITE_AES128)))
    {
        keyType  = kC4KeyType_Symmetric;
        algorithm = kCipher_Algorithm_AES128;
    }
    else if(CMP2(stringVal, stringLen, K_KEYSUITE_AES192, strlen(K_KEYSUITE_AES192)))
    {
        keyType  = kC4KeyType_Symmetric;
        algorithm = kCipher_Algorithm_AES192;
    }
    else if(CMP2(stringVal, stringLen, K_KEYSUITE_AES256, strlen(K_KEYSUITE_AES256)))
    {
        keyType  = kC4KeyType_Symmetric;
        algorithm = kCipher_Algorithm_AES256;
    }
    else if(CMP2(stringVal, stringLen, K_KEYSUITE_2FISH256, strlen(K_KEYSUITE_2FISH256)))
    {
        keyType  = kC4KeyType_Symmetric;
        algorithm = kCipher_Algorithm_2FISH256;
    }
    else if(CMP2(stringVal, stringLen, K_KEYSUITE_3FISH256, strlen(K_KEYSUITE_3FISH256)))
    {
        keyType  = kC4KeyType_Tweekable;
        algorithm = kCipher_Algorithm_3FISH256;
    }
    else if(CMP2(stringVal, stringLen, K_KEYSUITE_3FISH512, strlen(K_KEYSUITE_3FISH512)))
    {
        keyType  = kC4KeyType_Tweekable;
        algorithm = kCipher_Algorithm_3FISH512;
    }
    else if(CMP2(stringVal, stringLen, K_KEYSUITE_3FISH1024, strlen(K_KEYSUITE_3FISH1024)))
    {
        keyType  = kC4KeyType_Tweekable;
        algorithm = kCipher_Algorithm_3FISH1024;
    }
 
    else if(CMP2(stringVal, stringLen, K_KEYSUITE_SPLIT, strlen(K_KEYSUITE_SPLIT)))
    {
        keyType  = kC4KeyType_Share;
        algorithm = kCipher_Algorithm_SharedKey;
    }
    
    if(keyType == kC4KeyType_Invalid)
        err = kC4Err_CorruptData;
    
    *keyTypeOut = keyType;
    *algorithmOut = algorithm;
    
    return err;
}


int sGetKeyLength(C4KeyType keyType, int32_t algorithm)
{
    int          keylen = 0;
    
    switch(keyType)
    {
        case kC4KeyType_Symmetric:
            
            switch(algorithm)
            {
                case kCipher_Algorithm_AES128:
                    keylen = 16;
                    break;
                    
                case kCipher_Algorithm_AES192:
                    keylen = 24;
                    break;
 
                case kCipher_Algorithm_AES256:
                    keylen = 32;
                    break;
                    
                case kCipher_Algorithm_2FISH256:
                    keylen = 32;
                    break;
                    
                default:;
            }

            break;
 
        case kC4KeyType_Tweekable:
            switch(algorithm)
        {
            case kCipher_Algorithm_3FISH256:
                keylen = 32;
                break;
                
            case kCipher_Algorithm_3FISH512:
                keylen = 64;
                break;
                
            case kCipher_Algorithm_3FISH1024:
                keylen = 128;
                break;
                
            default:;
        }
            break;
            
        default:;
      }
    
    
    return keylen;
    
}



static int sParse_start_map(void * ctx)
{
    C4KeyJSONcontext *jctx = (C4KeyJSONcontext*) ctx;
    int retval = 0;
    
      jctx->level++;
//      printf("sParse_start_map\n");
    
    if(!jctx->keys)
    {
        jctx->index = 0;

        jctx->keys = XMALLOC(sizeof (C4KeyContext));
    }
    else
    {
       jctx->index++;
       jctx->keys =  XREALLOC(jctx->keys, sizeof(C4KeyContext) * (jctx->index + 1));
    }
    
    C4KeyContext* keyP = &jctx->keys[jctx->index];
    ZERO(keyP, sizeof(C4KeyContext));
    keyP->magic = kC4KeyContextMagic;
    keyP->type = kC4KeyType_Invalid;
    
    if(IsntNull(jctx))
    {
        retval = 1;
        
    }
    
    return retval;
 }

static int sParse_end_map(void * ctx)
{
    C4KeyJSONcontext *jctx = (C4KeyJSONcontext*) ctx;
    int retval = 0;
    
//     printf("sParse_end_map\n");
    if(IsntNull(jctx)  )
    {
        retval = 1;
        
         jctx->level--;
        
    }
       return retval;
}

static int sParse_start_array(void * ctx)
{
    C4KeyJSONcontext *jctx = (C4KeyJSONcontext*) ctx;
    int retval = 0;
    
//    printf("sParse_start_array\n");
    
    if(IsntNull(jctx))
    {
        retval = 1;
        
    }
    
    return retval;
}

static int sParse_end_array(void * ctx)
{
    C4KeyJSONcontext *jctx = (C4KeyJSONcontext*) ctx;
    int retval = 0;
    
//    printf("sParse_end_array\n");
     
    if(IsntNull(jctx))
    {
        retval = 1;
        
    }
    
    return retval;
}


static int sParse_number(void * ctx, const char * str, size_t len)
{
    C4KeyJSONcontext *jctx = (C4KeyJSONcontext*) ctx;
   char buf[32] = {0};
    int valid = 0;

//     printf("sParse_number\n");

    C4KeyContext* keyP = &jctx->keys[jctx->index];
    
    if(len < sizeof(buf))
    {
        COPY(str,buf,len);
        if(jctx->jType[jctx->level] == C4Key_JSON_Type_VERSION)
        {
            uint8_t val = atoi(buf);
            if(val == kC4KeyProtocolVersion)
            {
                jctx->version = val;
                valid = 1;

            }
         }
         else if(jctx->jType[jctx->level] == C4Key_JSON_Type_ROUNDS)
        {
            int val = atoi(buf);
            keyP->type = kC4KeyType_PBKDF2;
            keyP->pbkdf2.rounds = val;
            valid = 1;
        }
         else if(jctx->jType[jctx->level] == C4Key_JSON_Type_THRESHOLD)
         {
             int val = atoi(buf);
             
             if(keyP->type == kC4KeyType_PublicEncrypted)
             {
                 keyP->publicKeyEncoded.threshold = val;
                 valid = 1;
             }
             else if(keyP->type == kC4KeyType_PBKDF2)
             {
                 keyP->pbkdf2.threshold = val;
                 valid = 1;
             }
             else  if(keyP->type == kC4KeyType_Share)
             {
                 keyP->share.threshold = val;
                 valid = 1;
             }
          }
         else if(jctx->jType[jctx->level] == C4Key_JSON_Type_SHAREINDEX)
         {
             int val = atoi(buf);
             
             if(keyP->type == kC4KeyType_PublicEncrypted)
             {
                 keyP->publicKeyEncoded.xCoordinate = val;
                 valid = 1;
             }
             else if(keyP->type == kC4KeyType_PBKDF2)
             {
                 keyP->pbkdf2.xCoordinate = val;
                 valid = 1;
             }
             else  if(keyP->type == kC4KeyType_Share)
             {
                 keyP->share.xCoordinate = val;
                 valid = 1;
             }
        }
    }
    
    return valid;
}

static int sParse_string(void * ctx, const unsigned char * stringVal,
                         size_t stringLen)
{
    C4KeyJSONcontext *jctx = (C4KeyJSONcontext*) ctx;

    C4KeyContext* keyP = &jctx->keys[jctx->index];

    int valid = 0;
//     printf("sParse_string\n");
    
    if(jctx->jType[jctx->level] == C4Key_JSON_Type_PROPERTY)
    {
        C4KeyPropertyInfo  *propInfo = NULL;
        
        for(propInfo = sPropertyTable;  propInfo->name  && !valid; propInfo++)
        {
            if(CMP2(jctx->jTag, strlen((char *)(jctx->jTag)), propInfo->name, strlen(propInfo->name)))
            {
                switch (propInfo->type)
                {
                    case C4KeyPropertyType_UTF8String:
                        sInsertProperty(keyP, propInfo->name, C4KeyPropertyType_UTF8String, (void*)stringVal, stringLen);
                        valid = 1;
                        break;
                        
                    case C4KeyPropertyType_Time:
                    {
                        time_t t = parseRfc3339(stringVal, stringLen);
                        sInsertProperty(keyP, propInfo->name, C4KeyPropertyType_UTF8String,  &t, sizeof(time_t));
                        valid = 1;
                        break;
                    }
                        
                    case C4KeyPropertyType_Binary:
                    {
                        size_t dataLen = stringLen;
                        uint8_t     *buf = XMALLOC(dataLen);
                        
                        if(base64_decode(stringVal, stringLen, buf, &dataLen) == CRYPT_OK)
                        {
                            sInsertProperty(keyP, propInfo->name, C4KeyPropertyType_Binary, (void*)buf, dataLen);
                            valid = 1;
                        }
                        XFREE(buf);
                        break;
                    }
                        
                    default:
                        break;
                }
            }
        }
        
        // else just copy it
        if(!valid)
        {
            sInsertProperty(keyP, (char *)(jctx->jTag), C4KeyPropertyType_UTF8String, (void*)stringVal, stringLen);
            valid = 1;
        }
        
        if(jctx->jTag)
        {
            free(jctx->jTag);
            jctx->jTag = NULL;
        }
        
    }
    else if(jctx->jType[jctx->level] == C4Key_JSON_Type_SALT)
    {
        uint8_t     buf[128];
        size_t dataLen = sizeof(buf);
        
        if(( base64_decode(stringVal, stringLen, buf, &dataLen) == CRYPT_OK)
           && (dataLen == kC4KeyPBKDF2_SaltBytes))
        {
            keyP->type = kC4KeyType_PBKDF2;
            
            COPY(buf, keyP->pbkdf2.salt, dataLen);
            valid = 1;
        }
    }
    else if(jctx->jType[jctx->level] == C4Key_JSON_Type_MAC)
    {
        uint8_t     buf[128];
        size_t dataLen = sizeof(buf);
        
        if( base64_decode(stringVal,  stringLen, buf, &dataLen)  == CRYPT_OK)
        {
            
            if(keyP->type == kC4KeyType_PBKDF2 && (dataLen == kC4KeyPBKDF2_HashBytes))
            {
                COPY(buf, keyP->pbkdf2.keyHash, dataLen);
                valid = 1;
             }
            else  if(keyP->type == kC4KeyType_PublicEncrypted && (dataLen == kC4KeyPublic_Encrypted_HashBytes))
            {
                COPY(buf, keyP->publicKeyEncoded.keyHash, dataLen);
                valid = 1;
             }
               
        }
    }
    else if(jctx->jType[jctx->level] == C4Key_JSON_Type_SHAREHASH)
    {
        uint8_t     buf[128];
        size_t dataLen = sizeof(buf);
        
        if( (base64_decode(stringVal,  stringLen, buf, &dataLen)  == CRYPT_OK)
            && (dataLen == kC4ShareInfo_HashBytes))
        {
            if(keyP->type == kC4KeyType_PublicEncrypted)
            {
                COPY(buf, keyP->publicKeyEncoded.shareHash, dataLen);
                valid = 1;
            }
            else if(keyP->type == kC4KeyType_PBKDF2)
            {
                COPY(buf, keyP->pbkdf2.shareHash, dataLen);
                valid = 1;
            }
            else  if(keyP->type == kC4KeyType_Share)
            {
                COPY(buf, keyP->share.shareHash, dataLen);
                valid = 1;
            }
        }
    }
    else if(jctx->jType[jctx->level] == C4Key_JSON_Type_KEYID)
    {
        uint8_t     buf[128];
        size_t dataLen = sizeof(buf);
        
        if(( base64_decode(stringVal,  stringLen, buf, &dataLen)  == CRYPT_OK)
           && (dataLen  == kC4Key_KeyIDBytes))
        {
            keyP->type = kC4KeyType_PublicEncrypted;
            
            COPY(buf, keyP->publicKeyEncoded.keyID, dataLen);
            
            valid = 1;
        }
    }
    else if(jctx->jType[jctx->level] == C4Key_JSON_Type_ENCRYPTED_SYMKEY)
    {
        uint8_t     buf[256];
        size_t dataLen = sizeof(buf);
        
        if(( base64_decode(stringVal,  stringLen, buf, &dataLen)  == CRYPT_OK))
        {
            size_t keyLength = 0;
            
            if(keyP->type == kC4KeyType_PBKDF2)
            {
                if(keyP->pbkdf2.keyAlgorithmType == kC4KeyType_Symmetric)
                {
                    keyLength = sGetKeyLength(kC4KeyType_Symmetric, keyP->pbkdf2.cipherAlgor);
                    
                    keyLength = keyLength == 24?32:keyLength;
                    
                }
                else  if(keyP->pbkdf2.keyAlgorithmType == kC4KeyType_Tweekable)
                {
                    keyLength = sGetKeyLength(kC4KeyType_Tweekable, keyP->pbkdf2.cipherAlgor);
                    
                }
                else  if(keyP->pbkdf2.keyAlgorithmType == kC4KeyType_Share)
                {
                    keyLength = dataLen;
                }
                
                if(keyLength > 0 && keyLength == dataLen)
                {
                    COPY(buf, keyP->pbkdf2.encrypted, dataLen);
                    keyP->pbkdf2.encryptedLen = dataLen;
                    valid = 1;
                    
                }
              }
            else  if(keyP->type == kC4KeyType_PublicEncrypted)
            {
                
                if(dataLen <= kC4KeyPublic_Encrypted_BufferMAX)
                {
                    COPY(buf, keyP->publicKeyEncoded.encrypted, dataLen);
                    keyP->publicKeyEncoded.encryptedLen = dataLen;
                    valid = 1;
                 }
  
            }
        }
    }
    else if(jctx->jType[jctx->level] == C4Key_JSON_Type_ENCODING)
    {
        
        if(CMP2(stringVal, stringLen, kC4KeyProp_Encoding_PBKDF2_2FISH256, strlen(kC4KeyProp_Encoding_PBKDF2_2FISH256)))
        {
            keyP->type = kC4KeyType_PBKDF2;
            keyP->pbkdf2.encyptAlgor = kCipher_Algorithm_2FISH256;
            valid = 1;
        }
        else if(CMP2(stringVal, stringLen, kC4KeyProp_Encoding_PBKDF2_AES256, strlen(kC4KeyProp_Encoding_PBKDF2_AES256)))
        {
                keyP->type = kC4KeyType_PBKDF2;
                keyP->pbkdf2.encyptAlgor = kCipher_Algorithm_AES256;
                valid = 1;
        }
        else if(CMP2(stringVal, stringLen, kC4KeyProp_Encoding_PUBKEY_ECC384, strlen(kC4KeyProp_Encoding_PUBKEY_ECC384)))
        {
            keyP->type = kC4KeyType_PublicEncrypted;
            keyP->publicKeyEncoded.keysize = 384;
            valid = 1;
        }
        else if(CMP2(stringVal, stringLen, kC4KeyProp_Encoding_PUBKEY_ECC414, strlen(kC4KeyProp_Encoding_PUBKEY_ECC414)))
        {
            keyP->type = kC4KeyType_PublicEncrypted;
            keyP->publicKeyEncoded.keysize = 414;
            valid = 1;
        }
     }
    else if(jctx->jType[jctx->level] == C4Key_JSON_Type_KEYALGORITHM)
    {
        C4KeyType   keyType = kC4KeyType_Invalid;
        int32_t     algorithm = kEnumMaxValue;
        
        if(IsntC4Err( sParseKeySuiteString(stringVal,  stringLen, &keyType, &algorithm)))
        {
            if( keyP->type == kC4KeyType_PBKDF2)
            {
                keyP->pbkdf2.keyAlgorithmType = keyType;

                if(keyType == kC4KeyType_Symmetric)
                {
                    keyP->pbkdf2.cipherAlgor = algorithm;
                    valid = 1;
                    
                }
                else  if(keyType == kC4KeyType_Tweekable)
                {
                    keyP->pbkdf2.cipherAlgor = algorithm;
                    valid = 1;
                }
                else if (keyType == kC4KeyType_Share)
                {
                    keyP->pbkdf2.cipherAlgor = algorithm;
                    valid = 1;
                }

            }
            else if( keyP->type == kC4KeyType_PublicEncrypted)
            {
                keyP->publicKeyEncoded.keyAlgorithmType = keyType;
                if(keyType == kC4KeyType_Symmetric)
                {
                    keyP->publicKeyEncoded.cipherAlgor = algorithm;
                    valid = 1;
                    
                }
                else  if(keyType == kC4KeyType_Tweekable)
                {
                    keyP->publicKeyEncoded.cipherAlgor = algorithm;
                    valid = 1;
                }
                else if (keyType == kC4KeyType_Share)
                {
                    keyP->publicKeyEncoded.cipherAlgor = algorithm;
                    valid = 1;
                }
            }
            else
            {
                keyP->type = keyType;
                
                if(keyType == kC4KeyType_Symmetric)
                {
                    keyP->sym.symAlgor = algorithm;
                    valid = 1;
                    
                }
                else  if(keyType == kC4KeyType_Tweekable)
                {
                    keyP->tbc.tbcAlgor = algorithm;
                    valid = 1;
                }
            }
        }
     }

    return valid;

}

static int sParse_map_key(void * ctx, const unsigned char * stringVal, size_t stringLen )
{    int valid = 0;
 
    C4KeyJSONcontext *jctx = (C4KeyJSONcontext*) ctx;
   
//      printf("sParse_map_key[%d] \"%.*s\"\n",(int)jctx->level, (int)stringLen, stringVal);
    
    if(CMP2(stringVal, stringLen,kC4KeyProp_SCKeyVersion, strlen(kC4KeyProp_SCKeyVersion)))
    {
        jctx->jType[jctx->level] = C4Key_JSON_Type_VERSION;
        valid = 1;
    }
    else  if(CMP2(stringVal, stringLen,kC4KeyProp_Rounds, strlen(kC4KeyProp_Rounds)))
    {
        jctx->jType[jctx->level] = C4Key_JSON_Type_ROUNDS;
        valid = 1;
    }
    else  if(CMP2(stringVal, stringLen,kC4KeyProp_KeySuite, strlen(kC4KeyProp_KeySuite)))
    {
        jctx->jType[jctx->level] = C4Key_JSON_Type_KEYALGORITHM;
        valid = 1;
    }
    else  if(CMP2(stringVal, stringLen,kC4KeyProp_Encoding, strlen(kC4KeyProp_Encoding)))
    {
        jctx->jType[jctx->level] = C4Key_JSON_Type_ENCODING;
        valid = 1;
    }
    else  if(CMP2(stringVal, stringLen,kC4KeyProp_Salt, strlen(kC4KeyProp_Salt)))
    {
        jctx->jType[jctx->level] = C4Key_JSON_Type_SALT;
        valid = 1;
    }
    else  if(CMP2(stringVal, stringLen,kC4KeyProp_Mac, strlen(kC4KeyProp_Mac)))
    {
        jctx->jType[jctx->level] = C4Key_JSON_Type_MAC;
        valid = 1;
    }
    else  if(CMP2(stringVal, stringLen,kC4KeyProp_EncryptedKey, strlen(kC4KeyProp_EncryptedKey)))
    {
        jctx->jType[jctx->level] = C4Key_JSON_Type_ENCRYPTED_SYMKEY;
        valid = 1;
    }
    else  if(CMP2(stringVal, stringLen,kC4KeyProp_KeyID, strlen(kC4KeyProp_KeyID)))
    {
        jctx->jType[jctx->level] = C4Key_JSON_Type_KEYID;
        valid = 1;
    }
    else  if(CMP2(stringVal, stringLen,kC4KeyProp_ShareHash, strlen(kC4KeyProp_ShareHash)))
    {
        jctx->jType[jctx->level] = C4Key_JSON_Type_SHAREHASH;
        valid = 1;
    }
    else  if(CMP2(stringVal, stringLen,kC4KeyProp_ShareIndex, strlen(kC4KeyProp_ShareIndex)))
    {
        jctx->jType[jctx->level] = C4Key_JSON_Type_SHAREINDEX;
        valid = 1;
    }
    else  if(CMP2(stringVal, stringLen,kC4KeyProp_ShareThreshold, strlen(kC4KeyProp_ShareThreshold)))
    {
        jctx->jType[jctx->level] = C4Key_JSON_Type_THRESHOLD;
        valid = 1;
    }
    else
    {
        jctx->jType[jctx->level] = C4Key_JSON_Type_PROPERTY;
        if(jctx->jTag) free(jctx->jTag);
        jctx->jTag = (uint8_t *)strndup((char *)stringVal, stringLen);
        valid = 1;
        
    }

   return valid;

}


C4Err C4Key_DeserializeKeys( uint8_t *inData, size_t inLen,
                            size_t           *outCount,
                            C4KeyContextRef  *ctxArray[])
{
    C4Err               err = kC4Err_NoErr;
    yajl_status             stat = yajl_status_ok;
    yajl_handle             pHand = NULL;

    C4KeyJSONcontext       *jctx = NULL;
    size_t                  keyCount = 0;
    
    static yajl_callbacks callbacks = {
        NULL,
        NULL,
        NULL,
        NULL,
        sParse_number,
        sParse_string,
        sParse_start_map,
        sParse_map_key,
        sParse_end_map,
        sParse_start_array,
        sParse_end_array
    };
    
    yajl_alloc_funcs allocFuncs = {
        yajlMalloc,
        yajlRealloc,
        yajlFree,
        (void *) NULL
    };

//    ValidateParam(ctxArray);
    ValidateParam(inData);

    jctx = XMALLOC(sizeof (C4KeyJSONcontext)); CKNULL(jctx);
    ZERO(jctx, sizeof(C4KeyJSONcontext));
    jctx->level = 0;
    jctx->index = -1;
    jctx->jType[jctx->level] = C4Key_JSON_Type_BASE;
    
    pHand = yajl_alloc(&callbacks, &allocFuncs, (void *) jctx);
    
    yajl_config(pHand, yajl_allow_comments, 1);
    stat = yajl_parse(pHand, inData,  inLen); CKYJAL;
    stat = yajl_complete_parse(pHand); CKYJAL;
    keyCount = jctx->index + 1;
    
    if(outCount)
    {
        *outCount = keyCount;
    }
    
    if(ctxArray)
    {
        if(!keyCount)
        {
            *ctxArray = NULL;
        }
        else
        {
            int index = 0;
            C4KeyContextRef  *keys = XMALLOC(sizeof(C4KeyContextRef) *  keyCount);
            ZERO(keys , sizeof(C4KeyContextRef) *  keyCount);
            
            for(index = 0; index < keyCount; index++)
            {
                C4KeyContext* keyP = &jctx->keys[index];
                
                keys[index] =  XMALLOC(sizeof (C4KeyContext)); CKNULL(keys[index]);
                COPY(keyP, keys[index], sizeof (C4KeyContext));
          }
            *ctxArray = keys;
         }
      }
    
    
done:
    if(jctx)
    {
        if(jctx->keys)
        {
            ZERO(jctx->keys, sizeof(C4KeyContext) *  jctx->index);
            XFREE(jctx->keys);
        }
  
        XFREE(jctx);
    }
    
    
    if(IsntNull(pHand))
        yajl_free(pHand);
    
    return err;
}

#ifdef __clang__
#pragma mark - verify passphrase.
#endif

C4Err C4Key_VerifyPassPhrase(   C4KeyContextRef  ctx,
                             const uint8_t    *passphrase,
                             size_t           passphraseLen)
{
    C4Err           err = kC4Err_NoErr;
    uint8_t         unlocking_key[32] = {0};
    size_t           keyBytes = 0;
    uint8_t         keyHash[kC4KeyPBKDF2_HashBytes] = {0};

    validateC4KeyContext(ctx);
    ValidateParam(passphrase);
    
    ValidateParam(ctx->type == kC4KeyType_PBKDF2);
    
    if(ctx->pbkdf2.keyAlgorithmType == kC4KeyType_Symmetric)
    {
        keyBytes = sGetKeyLength(kC4KeyType_Symmetric, ctx->pbkdf2.cipherAlgor);
        
    }
    else  if(ctx->pbkdf2.keyAlgorithmType == kC4KeyType_Tweekable)
    {
        keyBytes = sGetKeyLength(kC4KeyType_Tweekable, ctx->pbkdf2.cipherAlgor);
    }
    
    err = PASS_TO_KEY(passphrase, passphraseLen,
                      ctx->pbkdf2.salt, sizeof(ctx->pbkdf2.salt), ctx->pbkdf2.rounds,
                      unlocking_key, sizeof(unlocking_key)); CKERR;
    
    
    err = sPASSPHRASE_HASH(unlocking_key, sizeof(unlocking_key),
                          ctx->pbkdf2.salt, sizeof(ctx->pbkdf2.salt), ctx->pbkdf2.rounds,
                           keyHash, kC4KeyPBKDF2_HashBytes); CKERR;
    
   ASSERTERR(!CMP(keyHash, ctx->pbkdf2.keyHash, kC4KeyPBKDF2_HashBytes), kC4Err_BadIntegrity)
    

done:
    
    ZERO(unlocking_key, sizeof(unlocking_key));
    
    return err;

}

C4Err C4Key_DecryptFromPassPhrase( C4KeyContextRef  passCtx,
                                  const uint8_t    *passphrase,
                                  size_t           passphraseLen,
                                  C4KeyContextRef       *symCtx)
{
    C4Err           err = kC4Err_NoErr;
    C4KeyContext*   keyCTX = NULL;

    Cipher_Algorithm    encyptAlgor = kCipher_Algorithm_Invalid;
    uint8_t             unlocking_key[32] = {0};
     size_t             keyBytes = 0;
    uint8_t             decrypted_key[128] = {0};
    uint8_t             keyHash[kC4KeyPBKDF2_HashBytes] = {0};
    
    validateC4KeyContext(passCtx);
    ValidateParam(passphrase);
    
    ValidateParam(passCtx->type == kC4KeyType_PBKDF2);
    
    if(passCtx->pbkdf2.keyAlgorithmType == kC4KeyType_Symmetric)
    {
        keyBytes = sGetKeyLength(kC4KeyType_Symmetric, passCtx->pbkdf2.cipherAlgor);
    
        switch (passCtx->pbkdf2.cipherAlgor)
        {
            case kCipher_Algorithm_2FISH256:
                encyptAlgor = kCipher_Algorithm_2FISH256;
                 break;
                
            case kCipher_Algorithm_AES192:
                encyptAlgor = kCipher_Algorithm_AES256;
                break;
                
            default:
                encyptAlgor = kCipher_Algorithm_AES256;
                 break;
        }
 
    }
    else  if(passCtx->pbkdf2.keyAlgorithmType == kC4KeyType_Tweekable)
    {
        encyptAlgor = kCipher_Algorithm_2FISH256;

        keyBytes = sGetKeyLength(kC4KeyType_Tweekable, passCtx->pbkdf2.cipherAlgor);
    }
    else  if(passCtx->pbkdf2.keyAlgorithmType == kC4KeyType_Share)
    {
        encyptAlgor = kCipher_Algorithm_2FISH256;
        keyBytes = passCtx->pbkdf2.encryptedLen;
    }

    
    err = PASS_TO_KEY(passphrase, passphraseLen,
                      passCtx->pbkdf2.salt, sizeof(passCtx->pbkdf2.salt), passCtx->pbkdf2.rounds,
                      unlocking_key, sizeof(unlocking_key)); CKERR;
    
    err = sPASSPHRASE_HASH(unlocking_key, sizeof(unlocking_key),
                           passCtx->pbkdf2.salt, sizeof(passCtx->pbkdf2.salt), passCtx->pbkdf2.rounds,
                           keyHash, kC4KeyPBKDF2_HashBytes); CKERR;
    
    ASSERTERR(!CMP(keyHash, passCtx->pbkdf2.keyHash, kC4KeyPBKDF2_HashBytes), kC4Err_BadIntegrity)
    
    keyCTX = XMALLOC(sizeof (C4KeyContext)); CKNULL(keyCTX);
    ZERO(keyCTX, sizeof(C4KeyContext));
    
    keyCTX->magic = kC4KeyContextMagic;
    
    if(passCtx->pbkdf2.keyAlgorithmType == kC4KeyType_Symmetric)
    {
        size_t bytesToDecrypt = keyBytes == 24?32:keyBytes;
        keyCTX->type  = kC4KeyType_Symmetric;
        keyCTX->sym.symAlgor = passCtx->pbkdf2.cipherAlgor;
        keyCTX->sym.keylen = keyBytes;
        
        err =  ECB_Decrypt(encyptAlgor, unlocking_key, passCtx->pbkdf2.encrypted,
                           bytesToDecrypt, decrypted_key); CKERR;

        COPY(decrypted_key, keyCTX->sym.symKey, bytesToDecrypt);
      
    }
    else  if(passCtx->pbkdf2.keyAlgorithmType == kC4KeyType_Tweekable)
    {
        keyCTX->type  = kC4KeyType_Tweekable;
        keyCTX->tbc.tbcAlgor = passCtx->pbkdf2.cipherAlgor;
        keyCTX->tbc.keybits = keyBytes << 3;
        
        err =  ECB_Decrypt(encyptAlgor, unlocking_key, passCtx->pbkdf2.encrypted,
                           keyBytes,  decrypted_key); CKERR;

        Skein_Get64_LSB_First(keyCTX->tbc.key, decrypted_key, keyBytes >>2);   /* bytes to words */
      }
    else  if(passCtx->pbkdf2.keyAlgorithmType == kC4KeyType_Share)
    {
        // we dont have a way to determine the expected length of a split key.

        // is the Share to big?
        ASSERTERR(keyBytes > 64 , kC4Err_CorruptData );
        
        keyCTX->type  = kC4KeyType_Share;
        keyCTX->share.threshold = passCtx->pbkdf2.threshold;
        keyCTX->share.xCoordinate = passCtx->pbkdf2.xCoordinate;
        COPY(passCtx->pbkdf2.shareHash, keyCTX->share.shareHash,  kC4ShareInfo_HashBytes);
        
        err =  ECB_Decrypt(encyptAlgor, unlocking_key, passCtx->pbkdf2.encrypted,
                           keyBytes, decrypted_key); CKERR;
        
        keyCTX->share.shareSecretLen = keyBytes;
        COPY(decrypted_key, keyCTX->share.shareSecret, keyBytes);
    }

    

    *symCtx = keyCTX;
    
done:
    if(IsC4Err(err))
    {
        if(IsntNull(keyCTX))
        {
            XFREE(keyCTX);
        }
    }
    
    ZERO(decrypted_key, sizeof(decrypted_key));
    ZERO(unlocking_key, sizeof(unlocking_key));
    
    return err;

}

C4Err C4Key_DecryptFromPubKey( C4KeyContextRef      encodedCtx,
                                    ECC_ContextRef    eccPriv,
                                    C4KeyContextRef       *symCtx)
{
    C4Err           err = kC4Err_NoErr;
    C4KeyContext*   keyCTX = NULL;
  
    int                 encyptAlgor = kCipher_Algorithm_Invalid;
    size_t              keyBytes = 0;
   
    uint8_t             decrypted_key[128] = {0};
    size_t              decryptedLen = 0;
    
    uint8_t             keyHash[kC4KeyPublic_Encrypted_HashBytes] = {0};
    
    validateC4KeyContext(encodedCtx);
    validateECCContext(eccPriv);
    ValidateParam(symCtx);
    
    ValidateParam(encodedCtx->type == kC4KeyType_PublicEncrypted);
   
    ValidateParam (ECC_isPrivate(eccPriv));
    
    if(encodedCtx->publicKeyEncoded.keyAlgorithmType == kC4KeyType_Symmetric)
    {
        keyBytes = sGetKeyLength(kC4KeyType_Symmetric, encodedCtx->publicKeyEncoded.cipherAlgor);
        encyptAlgor = encodedCtx->publicKeyEncoded.cipherAlgor;
        
    }
    else  if(encodedCtx->publicKeyEncoded.keyAlgorithmType == kC4KeyType_Tweekable)
    {
        keyBytes = sGetKeyLength(kC4KeyType_Tweekable, encodedCtx->publicKeyEncoded.cipherAlgor);
        encyptAlgor = encodedCtx->publicKeyEncoded.cipherAlgor;
    }
    else  if(encodedCtx->publicKeyEncoded.keyAlgorithmType == kC4KeyType_Share)
    {
        encyptAlgor = kCipher_Algorithm_SharedKey;
    }

    
    keyCTX = XMALLOC(sizeof (C4KeyContext)); CKNULL(keyCTX);
    ZERO(keyCTX, sizeof(C4KeyContext));
    
    keyCTX->magic = kC4KeyContextMagic;
    
    if(encodedCtx->publicKeyEncoded.keyAlgorithmType == kC4KeyType_Symmetric)
    {
        keyCTX->type  = kC4KeyType_Symmetric;
        keyCTX->sym.symAlgor = encodedCtx->publicKeyEncoded.cipherAlgor;
        keyCTX->sym.keylen = keyBytes;
        
        err = ECC_Decrypt(eccPriv,
                          encodedCtx->publicKeyEncoded.encrypted, encodedCtx->publicKeyEncoded.encryptedLen,
                          decrypted_key, sizeof(decrypted_key), &decryptedLen  );CKERR;
        
        ASSERTERR(decryptedLen != keyBytes, kC4Err_CorruptData );
        
        COPY(decrypted_key, keyCTX->sym.symKey, decryptedLen);
        
    }
    else  if(encodedCtx->publicKeyEncoded.keyAlgorithmType == kC4KeyType_Tweekable)
    {
        keyCTX->type  = kC4KeyType_Tweekable;
        keyCTX->tbc.tbcAlgor = encodedCtx->publicKeyEncoded.cipherAlgor;
        keyCTX->tbc.keybits = keyBytes << 3;

        err = ECC_Decrypt(eccPriv,
                          encodedCtx->publicKeyEncoded.encrypted, encodedCtx->publicKeyEncoded.encryptedLen,
                          decrypted_key, sizeof(decrypted_key), &decryptedLen  );CKERR;
        
        ASSERTERR(decryptedLen != keyBytes , kC4Err_CorruptData );
        
        Skein_Get64_LSB_First(keyCTX->tbc.key, decrypted_key, keyBytes >>2);   /* bytes to words */
    }
    else  if(encodedCtx->publicKeyEncoded.keyAlgorithmType == kC4KeyType_Share)
    {
        keyCTX->type  = kC4KeyType_Share;
        keyCTX->share.threshold = encodedCtx->publicKeyEncoded.threshold;
        keyCTX->share.xCoordinate = encodedCtx->publicKeyEncoded.xCoordinate;
        COPY(encodedCtx->publicKeyEncoded.shareHash, keyCTX->share.shareHash,  kC4ShareInfo_HashBytes);
        
        err = ECC_Decrypt(eccPriv,
                          encodedCtx->publicKeyEncoded.encrypted, encodedCtx->publicKeyEncoded.encryptedLen,
                          decrypted_key, sizeof(decrypted_key), &decryptedLen  );CKERR;
       
        // is the Share to big?
        ASSERTERR(decryptedLen > 64 , kC4Err_CorruptData );
        
        // we dont have a way to determine the expected length of a split key.
        keyBytes =  decryptedLen;
        keyCTX->share.shareSecretLen = decryptedLen;
        COPY(decrypted_key, keyCTX->share.shareSecret, decryptedLen);
     }

    // check integrity of decypted value against the MAC
    err = sKEY_HASH(decrypted_key, keyBytes, keyCTX->type,  encyptAlgor,
                    keyHash, kC4KeyPublic_Encrypted_HashBytes ); CKERR;
    
    ASSERTERR( !CMP(keyHash, encodedCtx->publicKeyEncoded.keyHash, kC4KeyPublic_Encrypted_HashBytes),
              kC4Err_BadIntegrity)

   

    *symCtx = keyCTX;
 

    
done:
    
    if(IsC4Err(err))
    {
        if(IsntNull(keyCTX))
        {
            XFREE(keyCTX);
        }
    }
    
    ZERO(decrypted_key, sizeof(decrypted_key));
 
    return err;

}



