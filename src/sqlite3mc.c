/*
** Name:        sqlite3mc.c
** Purpose:     Amalgamation of the SQLite3 Multiple Ciphers encryption extension for SQLite
** Author:      Ulrich Telle
** Created:     2020-02-28
** Copyright:   (c) 2006-2025 Ulrich Telle
** License:     MIT
*/

/*
** Force some options required for WASM builds
*/

#ifdef __WASM__

/* Disable User Authentication Extension */
#ifdef SQLITE_USER_AUTHENTICATION
#undef SQLITE_USER_AUTHENTICATION
#endif

/* Disable AES hardware support */
/* Note: this may be changed in the future depending on available support */
#ifdef SQLITE3MC_OMIT_AES_HARDWARE_SUPPORT
#undef SQLITE3MC_OMIT_AES_HARDWARE_SUPPORT
#endif
#define SQLITE3MC_OMIT_AES_HARDWARE_SUPPORT 1

#endif

/*
** Enable SQLite debug assertions if requested
*/

#ifndef SQLITE_DEBUG
#if defined(SQLITE_ENABLE_DEBUG) && (SQLITE_ENABLE_DEBUG == 1)
#define SQLITE_DEBUG 1
#endif
#endif

/*
** Declare all internal functions as 'static' unless told otherwise
*/
#ifndef SQLITE_PRIVATE
#define SQLITE_PRIVATE static
#endif

SQLITE_PRIVATE int sqlite3mc_initialize(const char* arg);
SQLITE_PRIVATE void sqlite3mc_shutdown(void);

/*
** To enable the extension functions define SQLITE_ENABLE_EXTFUNC on compiling this module
** To enable the reading CSV files define SQLITE_ENABLE_CSV on compiling this module
** To enable the SHA3 support define SQLITE_ENABLE_SHA3 on compiling this module
** To enable the CARRAY support define SQLITE_ENABLE_CARRAY on compiling this module
** To enable the FILEIO support define SQLITE_ENABLE_FILEIO on compiling this module
** To enable the SERIES support define SQLITE_ENABLE_SERIES on compiling this module
** To enable the UUID support define SQLITE_ENABLE_UUID on compiling this module
*/

/*
** Disable the user authentication feature by default
*/
#ifdef SQLITE_USER_AUTHENTICATION
/* Option defined and disabled, therefore undefine option */
#undef SQLITE_USER_AUTHENTICATION
#endif

#if defined(_WIN32) || defined(WIN32)

#ifndef SQLITE3MC_USE_RAND_S
#define SQLITE3MC_USE_RAND_S 1
#endif

#if SQLITE3MC_USE_RAND_S
/* Force header stdlib.h to define rand_s() */
#if !defined(_CRT_RAND_S)
#define _CRT_RAND_S
#endif

#else /* !WIN32 */

/* Define this before <string.h> is included to */
/* retrieve memset_s() declaration if available. */
#define __STDC_WANT_LIB_EXT1__ 1

#endif

#ifndef SQLITE_API
#define SQLITE_API
#endif

/*
** We need to do the following check here BEFORE including <windows.h>,
** because the header <arm_neon.h> is included from somewhere within
** <windows.h>, and we need support for the new Neon intrinsics, if
** AES hardware support is enabled.
*/
#if defined (_MSC_VER)
#if defined _M_ARM
#define _ARM_USE_NEW_NEON_INTRINSICS
#endif
#endif

#include <windows.h>

/* SQLite functions only needed on Win32 */
SQLITE_API void sqlite3_win32_write_debug(const char*, int);
SQLITE_API char *sqlite3_win32_unicode_to_utf8(LPCWSTR);
SQLITE_API char *sqlite3_win32_mbcs_to_utf8(const char*);
SQLITE_API char *sqlite3_win32_mbcs_to_utf8_v2(const char*, int);
SQLITE_API char *sqlite3_win32_utf8_to_mbcs(const char*);
SQLITE_API char *sqlite3_win32_utf8_to_mbcs_v2(const char*, int);
SQLITE_API LPWSTR sqlite3_win32_utf8_to_unicode(const char*);
#endif

/*
** Include SQLite3 amalgamation
*/
#include "sqlite3patched.c"

/*
** Include SQLite3MultiCipher components
*/
#include "sqlite3mc_config.h"
#include "sqlite3mc.h"

SQLITE_API const char*
sqlite3mc_version()
{
  static const char* version = SQLITE3MC_VERSION_STRING;
  return version;
}

SQLITE_PRIVATE void
sqlite3mcVersion(sqlite3_context* context, int argc, sqlite3_value** argv)
{
  assert(argc == 0);
  sqlite3_result_text(context, sqlite3mc_version(), -1, 0);
}

#ifndef SQLITE3MC_SECURE_MEMORY
#define SQLITE3MC_SECURE_MEMORY 0
#endif

#if SQLITE3MC_SECURE_MEMORY

#define SECURE_MEMORY_NONE 0
#define SECURE_MEMORY_FILL 1
#define SECURE_MEMORY_LOCK 2

SQLITE_PRIVATE void sqlite3mcSetMemorySecurity(int value);
SQLITE_PRIVATE int sqlite3mcGetMemorySecurity();

/* Memory locking is currently not supported */
#ifdef SQLITE3MC_ENABLE_MEMLOCK
#undef SQLITE3MC_ENABLE_MEMLOCK
#endif
#define SQLITE3MC_ENABLE_MEMLOCK 0

#endif

/*
** Crypto algorithms
*/
#include "md5.c"
#include "sha1.c"
#include "sha2.c"

#if HAVE_CIPHER_CHACHA20 || HAVE_CIPHER_SQLCIPHER || HAVE_CIPHER_ASCON128 || HAVE_CIPHER_AEGIS
#include "fastpbkdf2.c"

/* Prototypes for several crypto functions to make pedantic compilers happy */
SQLITE_PRIVATE void chacha20_xor(void* data, size_t n, const uint8_t key[32], const uint8_t nonce[12], uint32_t counter);
SQLITE_PRIVATE void poly1305(const uint8_t* msg, size_t n, const uint8_t key[32], uint8_t tag[16]);
SQLITE_PRIVATE int poly1305_tagcmp(const uint8_t tag1[16], const uint8_t tag2[16]);
SQLITE_PRIVATE void chacha20_rng(void* out, size_t n);

#include "chacha20poly1305.c"
#endif

/*
** Declare function prototype for registering the codec extension functions
*/
static int
mcRegisterCodecExtensions(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi);

/*
** Codec implementation
*/
#if HAVE_CIPHER_AES_128_CBC || HAVE_CIPHER_AES_256_CBC || HAVE_CIPHER_SQLCIPHER
#include "rijndael.c"
#endif

#if HAVE_CIPHER_AEGIS

/* Incremental encryption/decryption not needed */
#define AEGIS_OMIT_INCREMENTAL
/* API for generating MAC not needed */
#define AEGIS_OMIT_MAC_API

#include "aegis/libaegis.c"
#include "argon2/libargon2.c"
#endif

#include "codec_algos.c"

#include "cipher_wxaes128.c"
#include "cipher_wxaes256.c"
#include "cipher_chacha20.c"
#include "cipher_sqlcipher.c"
#include "cipher_sds_rc4.c"
#include "cipher_ascon.c"
#include "cipher_aegis.c"
#include "cipher_common.c"
#include "cipher_config.c"

#include "codecext.c"

/*
** Functions for securing allocated memory
*/
#include "memory_secure.c"

/*
** Extension functions
*/
#ifdef SQLITE_ENABLE_EXTFUNC
/* Prototype for initialization function of EXTENSIONFUNCTIONS extension */
int RegisterExtensionFunctions(sqlite3* db);
#include "extensionfunctions.c"
#endif

/*
** CSV import
*/
#ifdef SQLITE_ENABLE_CSV
/* Prototype for initialization function of CSV extension */
SQLITE_API
int sqlite3_csv_init(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi);
#include "csv.c"
#endif

/*
** VSV import
*/
#ifdef SQLITE_ENABLE_VSV
/* Prototype for initialization function of VSV extension */
SQLITE_API
int sqlite3_vsv_init(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi);
#include "vsv.c"
#endif

/*
** SHA3
*/
#ifdef SQLITE_ENABLE_SHA3
/* Prototype for initialization function of SHA3 extension */
SQLITE_API
int sqlite3_shathree_init(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi);
#include "shathree.c"
#endif

/*
** CARRAY
*/
#ifdef SQLITE_ENABLE_CARRAY
/* Prototype for initialization function of CARRAY extension */
SQLITE_API
int sqlite3_carray_init(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi);
#include "carray.c"
#endif

/*
** FILEIO
*/
#ifdef SQLITE_ENABLE_FILEIO
/* Prototype for initialization function of FILEIO extension */
SQLITE_API
int sqlite3_fileio_init(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi);

/* MinGW specifics */
#if (!defined(_WIN32) && !defined(WIN32)) || defined(__MINGW32__)
# include <unistd.h>
# include <dirent.h>
# if defined(__MINGW32__)
#  define DIRENT dirent
#  ifndef S_ISLNK
#   define S_ISLNK(mode) (0)
#  endif
# endif
#endif

#include "test_windirent.c"
#include "fileio.c"
#endif

/*
** SERIES
*/
#ifdef SQLITE_ENABLE_SERIES
/* Prototype for initialization function of SERIES extension */
SQLITE_API
int sqlite3_series_init(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi);
#include "series.c"
#endif

/*
** UUID
*/
#ifdef SQLITE_ENABLE_UUID
/* Prototype for initialization function of UUID extension */
SQLITE_API
int sqlite3_uuid_init(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi);
#include "uuid.c"
#endif

/*
** REGEXP
*/
#ifdef SQLITE_ENABLE_REGEXP
/* Prototype for initialization function of REGEXP extension */
SQLITE_API
int sqlite3_regexp_init(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi);
#include "regexp.c"
#endif

#if defined(SQLITE_ENABLE_COMPRESS) || defined(SQLITE_ENABLE_SQLAR) || defined(SQLITE_ENABLE_ZIPFILE)
#if SQLITE3MC_USE_MINIZ != 0
#include "miniz.c"
#endif
#endif

/*
** COMPRESS
*/
#ifdef SQLITE_ENABLE_COMPRESS
SQLITE_API
int sqlite3_compress_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi);
#include "compress.c"
#endif

/*
** SQLAR
*/
#ifdef SQLITE_ENABLE_SQLAR
SQLITE_API
int sqlite3_sqlar_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi);
#include "sqlar.c"
#endif

/*
** ZIPFILE
*/
#ifdef SQLITE_ENABLE_ZIPFILE
SQLITE_API
int sqlite3_zipfile_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi);
#include "zipfile.c"
#endif

/*
** Multi cipher VFS
*/
#include "sqlite3mc_vfs.c"

static int
mcRegisterCodecExtensions(sqlite3* db, char** pzErrMsg, const sqlite3_api_routines* pApi)
{
  int rc = SQLITE_OK;
  CodecParameter* codecParameterTable = NULL;

  void* codecParamTable = sqlite3_get_clientdata(db, globalConfigTableName);
  if (codecParamTable)
  {
    /* Return if codec extension functions are already defined */
    return rc;
  }

  /* Generate copy of global codec parameter table */
  codecParameterTable = sqlite3mcCloneCodecParameterTable();
  rc = (codecParameterTable != NULL) ? SQLITE_OK : SQLITE_NOMEM;
  if (rc == SQLITE_OK)
  {
    sqlite3_set_clientdata(db, globalConfigTableName, codecParameterTable, sqlite3mcFreeCodecParameterTable);
  }

  rc = (codecParameterTable != NULL) ? SQLITE_OK : SQLITE_NOMEM;
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_create_function(db, "sqlite3mc_config", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC,
                                 codecParameterTable, sqlite3mcConfigParams, 0, 0);
  }
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_create_function(db, "sqlite3mc_config", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC,
                                 codecParameterTable, sqlite3mcConfigParams, 0, 0);
  }
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_create_function(db, "sqlite3mc_config", 3, SQLITE_UTF8 | SQLITE_DETERMINISTIC,
                                 codecParameterTable, sqlite3mcConfigParams, 0, 0);
  }
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_create_function(db, "sqlite3mc_codec_data", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC,
                                 NULL, sqlite3mcCodecDataSql, 0, 0);
  }
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_create_function(db, "sqlite3mc_codec_data", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC,
                                 NULL, sqlite3mcCodecDataSql, 0, 0);
  }
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_create_function(db, "sqlite3mc_version", 0, SQLITE_UTF8 | SQLITE_DETERMINISTIC,
                                 NULL, sqlite3mcVersion, 0, 0);
  }
  return rc;
}

#ifdef SQLITE_ENABLE_EXTFUNC
static int
sqlite3_extfunc_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi)
{
  return RegisterExtensionFunctions(db);
}
#endif

static int
mcCheckValidName(const char* name)
{
  size_t nl;
  if (!name)
    return SQLITE_ERROR;

  /* Check for valid cipher name length */
  nl = strlen(name);
  if (nl < 1 || nl >= CIPHER_NAME_MAXLEN)
    return SQLITE_ERROR;

  /* Check for already registered names */
  CipherName* cipherNameTable = &globalCipherNameTable[0];
  for (; cipherNameTable->m_name[0] != 0; ++cipherNameTable)
  {
    if (sqlite3_stricmp(name, cipherNameTable->m_name) == 0) break;
  }
  if (cipherNameTable->m_name[0] != 0)
    return SQLITE_ERROR;

  /* Check for valid character set (1st char = alpha, rest = alpha-numeric or underscore) */
  if (sqlite3Isalpha(name[0]))
  {
    size_t j;
    for (j = 1; j < nl && (name[j] == '_' || sqlite3Isalnum(name[j])); ++j) {}
    if (j == nl)
      return SQLITE_OK;
  }
  return SQLITE_ERROR;
}

SQLITE_PRIVATE int
sqlite3mcGetGlobalCipherCount()
{
  int cipherCount = 0;
  sqlite3_mutex_enter(sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_MAIN));
  cipherCount = globalCipherCount;
  sqlite3_mutex_leave(sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_MAIN));
  return cipherCount;
}

static int
sqlite3mcRegisterCipher(const CipherDescriptor* desc, const CipherParams* params, int makeDefault)
{
  int rc;
  int np;
  CipherParams* cipherParams;

  /* Sanity checks */

  /* Cipher description AND parameter are required */
  if (!desc || !params)
    return SQLITE_ERROR;

  /* ALL methods of the cipher descriptor need to be defined */
  if (!desc->m_name ||
      !desc->m_allocateCipher ||
      !desc->m_freeCipher ||
      !desc->m_cloneCipher ||
      !desc->m_getLegacy ||
      !desc->m_getPageSize ||
      !desc->m_getReserved ||
      !desc->m_getSalt ||
      !desc->m_generateKey ||
      !desc->m_encryptPage ||
      !desc->m_decryptPage)
    return SQLITE_ERROR;

  /* Check for valid cipher name */
  if (mcCheckValidName(desc->m_name) != SQLITE_OK)
    return SQLITE_ERROR;

  /* Check cipher parameters */
  for (np = 0; np < CIPHER_PARAMS_COUNT_MAX; ++np)
  {
    CipherParams entry = params[np];
    /* Check for sentinel parameter */
    if (entry.m_name == 0 || entry.m_name[0] == 0)
      break;
    /* Check for valid parameter name */
    if (mcCheckValidName(entry.m_name) != SQLITE_OK)
      return SQLITE_ERROR;
    /* Check for valid parameter specification */
    if (!(entry.m_minValue >= 0 && entry.m_maxValue >= 0 && entry.m_minValue <= entry.m_maxValue &&
          entry.m_value >= entry.m_minValue && entry.m_value <= entry.m_maxValue &&
          entry.m_default >= entry.m_minValue && entry.m_default <= entry.m_maxValue))
      return SQLITE_ERROR;
  }

  /* Check for parameter count in valid range and valid sentinel parameter */
  if (np >= CIPHER_PARAMS_COUNT_MAX || params[np].m_name == 0)
    return SQLITE_ERROR;

  /* Sanity checks were successful, now register cipher */

  cipherParams = (CipherParams*) sqlite3_malloc((np+1) * sizeof(CipherParams));
  if (!cipherParams)
    return SQLITE_NOMEM;

  /* Check for */
  if (globalCipherCount < CODEC_COUNT_MAX)
  {
    int n;
    char* cipherName;
    ++globalCipherCount;
    cipherName = globalCipherNameTable[globalCipherCount].m_name;
    strcpy(cipherName, desc->m_name);

    globalCodecDescriptorTable[globalCipherCount - 1] = *desc;
    globalCodecDescriptorTable[globalCipherCount - 1].m_name = cipherName;

    globalCodecParameterTable[globalCipherCount].m_name = cipherName;
    globalCodecParameterTable[globalCipherCount].m_id = globalCipherCount;
    globalCodecParameterTable[globalCipherCount].m_params = cipherParams;

    /* Copy parameters */
    for (n = 0; n < np; ++n)
    {
      char* paramName = (char*) sqlite3_malloc((int)strlen(params[n].m_name) + 1);
      strcpy(paramName, params[n].m_name);
      cipherParams[n] = params[n];
      cipherParams[n].m_name = paramName;
    }
    /* Add sentinel */
    cipherParams[n] = params[n];
    cipherParams[n].m_name = globalSentinelName;

    /* Make cipher default, if requested */
    if (makeDefault)
    {
      CipherParams* param = globalCodecParameterTable[0].m_params;
      for (; param->m_name[0] != 0; ++param)
      {
        if (sqlite3_stricmp("cipher", param->m_name) == 0) break;
      }
      if (param->m_name[0] != 0)
      {
        param->m_value = param->m_default = globalCipherCount;
      }
    }

    rc = SQLITE_OK;
  }
  else
  {
    rc = SQLITE_NOMEM;
  }

  return rc;
}

SQLITE_API int
sqlite3mc_register_cipher(const CipherDescriptor* desc, const CipherParams* params, int makeDefault)
{
  int rc;
#ifndef SQLITE_OMIT_AUTOINIT
  rc = sqlite3_initialize();
  if (rc) return rc;
#endif
  sqlite3_mutex_enter(sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_MAIN));
  rc = sqlite3mcRegisterCipher(desc, params, makeDefault);
  sqlite3_mutex_leave(sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_MAIN));
  return rc;
}

SQLITE_PRIVATE int
sqlite3mcInitCipherTables()
{
  size_t n;

  /* Initialize global configuration table name */
  sqlite3_randomness(CIPHER_NAME_MAXLEN, globalConfigTableName);
  for (n = 0; n < CIPHER_NAME_MAXLEN-1; ++n)
  {
    if (globalConfigTableName[n] == 0)
      globalConfigTableName[n] = '@';
  }
  globalConfigTableName[CIPHER_NAME_MAXLEN-1] = 0;

  /* Initialize cipher name table */
  strcpy(globalCipherNameTable[0].m_name, "global");
  for (n = 1; n < CODEC_COUNT_MAX + 2; ++n)
  {
    strcpy(globalCipherNameTable[n].m_name, "");
  }

  /* Initialize cipher descriptor table */
  for (n = 0; n < CODEC_COUNT_MAX + 1; ++n)
  {
    globalCodecDescriptorTable[n] = mcSentinelDescriptor;
  }

  /* Initialize cipher parameter table */
  globalCodecParameterTable[0] = globalCommonParams;
  for (n = 1; n < CODEC_COUNT_MAX + 2; ++n)
  {
    globalCodecParameterTable[n] = globalSentinelParams;
  }

  return SQLITE_OK;
}

SQLITE_PRIVATE void
sqlite3mcTermCipherTables()
{
  size_t n;
  for (n = CODEC_COUNT_MAX+1; n > 0; --n)
  {
    if (globalCodecParameterTable[n].m_name[0] != 0)
    {
      int k;
      CipherParams* params = globalCodecParameterTable[n].m_params;
      for (k = 0; params[k].m_name[0] != 0; ++k)
      {
        sqlite3_free((char*) params[k].m_name);
      }
      sqlite3_free(globalCodecParameterTable[n].m_params);
    }
  }
  globalCipherCount = 0;
}

SQLITE_PRIVATE int
sqlite3mc_initialize(const char* arg)
{
  int rc = sqlite3mcInitCipherTables();
#if HAVE_CIPHER_AES_128_CBC
  if (rc == SQLITE_OK)
  {
    rc = sqlite3mcRegisterCipher(&mcAES128Descriptor, mcAES128Params, (CODEC_TYPE_AES128 == CODEC_TYPE));
  }
#endif
#if HAVE_CIPHER_AES_256_CBC
  if (rc == SQLITE_OK)
  {
    rc = sqlite3mcRegisterCipher(&mcAES256Descriptor, mcAES256Params, (CODEC_TYPE_AES256 == CODEC_TYPE));
  }
#endif
#if HAVE_CIPHER_CHACHA20
  if (rc == SQLITE_OK)
  {
    rc = sqlite3mcRegisterCipher(&mcChaCha20Descriptor, mcChaCha20Params, (CODEC_TYPE_CHACHA20 == CODEC_TYPE));
  }
#endif
#if HAVE_CIPHER_SQLCIPHER
  if (rc == SQLITE_OK)
  {
    rc = sqlite3mcRegisterCipher(&mcSQLCipherDescriptor, mcSQLCipherParams, (CODEC_TYPE_SQLCIPHER == CODEC_TYPE));
  }
#endif
#if HAVE_CIPHER_RC4
  if (rc == SQLITE_OK)
  {
    rc = sqlite3mcRegisterCipher(&mcRC4Descriptor, mcRC4Params, (CODEC_TYPE_RC4 == CODEC_TYPE));
  }
#endif
#if HAVE_CIPHER_ASCON128
  if (rc == SQLITE_OK)
  {
    rc = sqlite3mcRegisterCipher(&mcAscon128Descriptor, mcAscon128Params, (CODEC_TYPE_ASCON128 == CODEC_TYPE));
  }
#endif
#if HAVE_CIPHER_AEGIS
  if (rc == SQLITE_OK)
  {
    aegis_init();
    rc = sqlite3mcRegisterCipher(&mcAegisDescriptor, mcAegisParams, (CODEC_TYPE_AEGIS == CODEC_TYPE));
  }
#endif

  /*
  ** Initialize and register MultiCipher VFS as default VFS
  ** if it isn't already registered
  */
  if (rc == SQLITE_OK)
  {
    rc = sqlite3mc_vfs_create(NULL, 1);
  }
  return rc;
}

SQLITE_PRIVATE int
sqlite3mc_builtin_extensions(sqlite3* db)
{
  char* errmsg = NULL;
  int rc = SQLITE_OK;

  /*
  ** Register Multi Cipher extension
  */
  if (rc == SQLITE_OK)
  {
    rc = mcRegisterCodecExtensions(db, &errmsg, NULL);
  }
#ifdef SQLITE_ENABLE_EXTFUNC
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_extfunc_init(db, &errmsg, NULL);
  }
#endif
#ifdef SQLITE_ENABLE_CSV
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_csv_init(db, &errmsg, NULL);
  }
#endif
#ifdef SQLITE_ENABLE_VSV
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_vsv_init(db, &errmsg, NULL);
  }
#endif
#ifdef SQLITE_ENABLE_SHA3
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_shathree_init(db, &errmsg, NULL);
  }
#endif
#ifdef SQLITE_ENABLE_CARRAY
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_carray_init(db, &errmsg, NULL);
  }
#endif
#ifdef SQLITE_ENABLE_FILEIO
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_fileio_init(db, &errmsg, NULL);
  }
#endif
#ifdef SQLITE_ENABLE_SERIES
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_series_init(db, &errmsg, NULL);
  }
#endif
#ifdef SQLITE_ENABLE_UUID
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_uuid_init(db, &errmsg, NULL);
  }
#endif
#ifdef SQLITE_ENABLE_REGEXP
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_regexp_init(db, &errmsg, NULL);
  }
#endif
#ifdef SQLITE_ENABLE_COMPRESS
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_compress_init(db, &errmsg, NULL);
  }
#endif
#ifdef SQLITE_ENABLE_SQLAR
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_sqlar_init(db, &errmsg, NULL);
  }
#endif
#ifdef SQLITE_ENABLE_ZIPFILE
  if (rc == SQLITE_OK)
  {
    rc = sqlite3_zipfile_init(db, &errmsg, NULL);
  }
#endif
  return rc;
}

SQLITE_PRIVATE void
sqlite3mc_shutdown(void)
{
  sqlite3mc_vfs_shutdown();
  sqlite3mcTermCipherTables();
}

/*
** TCL/TK Extension and/or Shell
*/
#ifdef SQLITE_ENABLE_TCL
#include "tclsqlite.c"
#endif
