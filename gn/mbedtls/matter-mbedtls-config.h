/* mbedTLS 3.6.6 config of the BK7231N library: what Matter (CHIPCryptoPALmbedTLS) needs - P-256 ECDSA / ECDH,
 * SHA-256, HMAC, HKDF, PBKDF2, AES-CCM, the CTR / HMAC DRBG, X.509 certificates and CSR. Software only, no SSL/TLS,
 * no PSA. Entropy: the platform adds mbedtls_hardware_poll() to the DRBG of the library (PlatformManagerImpl.cpp).
 *
 * The library is compiled against these headers but carries no mbedTLS code: the firmware links its own mbedTLS
 * 3.6.6, which has to have all of the modules below and the same layout of the contexts (the sizes are checked by
 * matter_requirements.h of the release package). */
#pragma once

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_SNPRINTF_MACRO snprintf
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_ENTROPY_FORCE_SHA256
/* the size of mbedtls_entropy_context (the library keeps one): as the BDK glue of ESPurna has it */
#define MBEDTLS_ENTROPY_MAX_SOURCES 10
#define MBEDTLS_ERROR_STRERROR_DUMMY
#define MBEDTLS_HAVE_ASM

#define MBEDTLS_AES_C
#define MBEDTLS_AES_ROM_TABLES
#define MBEDTLS_AES_FEWER_TABLES
#define MBEDTLS_CCM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CIPHER_MODE_CTR
/* the cipher context inside mbedtls_ccm_context has its padding fields with CBC (a TLS firmware has it) */
#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_MD_C
#define MBEDTLS_SHA256_C
/* mbedtls_sha256_context has its is224 field with SHA224_C (a TLS firmware has it) */
#define MBEDTLS_SHA224_C
#define MBEDTLS_SHA256_SMALLER
#define MBEDTLS_HKDF_C
#define MBEDTLS_HMAC_DRBG_C
#define MBEDTLS_PKCS5_C

#define MBEDTLS_BIGNUM_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECDSA_DETERMINISTIC
#define MBEDTLS_MPI_WINDOW_SIZE 1
#define MBEDTLS_MPI_MAX_SIZE 32
#define MBEDTLS_ECP_WINDOW_SIZE 2
#define MBEDTLS_ECP_FIXED_POINT_OPTIM 0

#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_OID_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PK_WRITE_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CREATE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_CSR_PARSE_C
#define MBEDTLS_X509_CSR_WRITE_C
