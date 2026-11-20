/* What the Matter library needs from the firmware that links it - <app/matter_requirements.h> of the release package
 * (include/chip/gn/app, on the include path of the library's API).
 *
 * The library is a library: it carries no SDK of its own. It is compiled against stock SDKs (ESP32: ESP-IDF; BK7231N:
 * the BDK, FreeRTOS-Kernel, lwIP, Mbed TLS) of the versions its release names, with its own configs, and links with
 * the firmware's FreeRTOS, lwIP and mbedTLS. Compile this header once, in a C++ source of the firmware with the flags
 * the firmware builds its SDK with: a firmware that lacks something the library calls, or lays out a type the
 * library shares with it otherwise, stops here with the name of the item instead of failing at run time.
 *
 *   - this file: the features (mbedTLS modules, lwIP IPv4 + IPv6 + UDP, FreeRTOS 11) and the minimums
 *   - matter_abi.h (generated from the library's build, tools/abi.py): the sizes of the shared types and constants */
#pragma once

#if __has_include(<freertos/FreeRTOS.h>)
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#else
#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>
#include <timers.h>
#endif

#include <lwip/opt.h>
#include <lwip/ip_addr.h>
#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <lwip/udp.h>

#include <mbedtls/build_info.h>
#include <mbedtls/aes.h>
#include <mbedtls/bignum.h>
#include <mbedtls/ccm.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <mbedtls/hmac_drbg.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/x509_csr.h>

// --- FreeRTOS: version 11 (the timer commands, the task API the library calls) ---------------------------------------

#if !defined(tskKERNEL_VERSION_MAJOR) || (tskKERNEL_VERSION_MAJOR != 11)
#error "Matter library: needs FreeRTOS-Kernel 11"
#endif

// --- lwIP: IPv4 + IPv6 UDP with multicast (operational discovery, mDNS), the packet buffers of Matter --------------

#if !LWIP_IPV4 || !LWIP_IPV6
#error "Matter library: lwIP needs LWIP_IPV4 and LWIP_IPV6"
#endif
#if !LWIP_UDP
#error "Matter library: lwIP needs LWIP_UDP"
#endif
#if !LWIP_IGMP || !LWIP_IPV6_MLD
#error "Matter library: lwIP needs multicast: LWIP_IGMP and LWIP_IPV6_MLD"
#endif
// Matter's packet buffers come from the pbuf pool: 3 ran empty at AddNOC (the reply and the operational
// announcement were dropped, 2026-11-05)
#if defined(PBUF_POOL_SIZE) && (PBUF_POOL_SIZE < 6)
#error "Matter library: lwIP needs PBUF_POOL_SIZE 6 or more"
#endif

// --- mbedTLS 3.6: the modules CHIPCryptoPALmbedTLS calls (the firmware links them) -----------------------------------

#if (MBEDTLS_VERSION_MAJOR != 3) || (MBEDTLS_VERSION_MINOR != 6)
#error "Matter library: needs mbedTLS 3.6"
#endif

#if !defined(MBEDTLS_AES_C)
#error "Matter library: mbedTLS needs MBEDTLS_AES_C"
#endif
#if !defined(MBEDTLS_CCM_C)
#error "Matter library: mbedTLS needs MBEDTLS_CCM_C"
#endif
#if !defined(MBEDTLS_CIPHER_C)
#error "Matter library: mbedTLS needs MBEDTLS_CIPHER_C"
#endif
#if !defined(MBEDTLS_CIPHER_MODE_CTR)
#error "Matter library: mbedTLS needs MBEDTLS_CIPHER_MODE_CTR"
#endif
#if !defined(MBEDTLS_CTR_DRBG_C)
#error "Matter library: mbedTLS needs MBEDTLS_CTR_DRBG_C"
#endif
#if !defined(MBEDTLS_ENTROPY_C)
#error "Matter library: mbedTLS needs MBEDTLS_ENTROPY_C"
#endif
#if !defined(MBEDTLS_MD_C)
#error "Matter library: mbedTLS needs MBEDTLS_MD_C"
#endif
#if !defined(MBEDTLS_SHA256_C)
#error "Matter library: mbedTLS needs MBEDTLS_SHA256_C"
#endif
#if !defined(MBEDTLS_HKDF_C)
#error "Matter library: mbedTLS needs MBEDTLS_HKDF_C"
#endif
#if !defined(MBEDTLS_HMAC_DRBG_C)
#error "Matter library: mbedTLS needs MBEDTLS_HMAC_DRBG_C"
#endif
#if !defined(MBEDTLS_PKCS5_C)
#error "Matter library: mbedTLS needs MBEDTLS_PKCS5_C"
#endif
#if !defined(MBEDTLS_BIGNUM_C)
#error "Matter library: mbedTLS needs MBEDTLS_BIGNUM_C"
#endif
#if !defined(MBEDTLS_ECP_C)
#error "Matter library: mbedTLS needs MBEDTLS_ECP_C"
#endif
#if !defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED)
#error "Matter library: mbedTLS needs MBEDTLS_ECP_DP_SECP256R1_ENABLED"
#endif
#if !defined(MBEDTLS_ECDH_C)
#error "Matter library: mbedTLS needs MBEDTLS_ECDH_C"
#endif
#if !defined(MBEDTLS_ECDSA_C)
#error "Matter library: mbedTLS needs MBEDTLS_ECDSA_C"
#endif
#if !defined(MBEDTLS_ECDSA_DETERMINISTIC)
#error "Matter library: mbedTLS needs MBEDTLS_ECDSA_DETERMINISTIC"
#endif
#if !defined(MBEDTLS_ASN1_PARSE_C)
#error "Matter library: mbedTLS needs MBEDTLS_ASN1_PARSE_C"
#endif
#if !defined(MBEDTLS_ASN1_WRITE_C)
#error "Matter library: mbedTLS needs MBEDTLS_ASN1_WRITE_C"
#endif
#if !defined(MBEDTLS_OID_C)
#error "Matter library: mbedTLS needs MBEDTLS_OID_C"
#endif
#if !defined(MBEDTLS_PK_C) || !defined(MBEDTLS_PK_PARSE_C) || !defined(MBEDTLS_PK_WRITE_C)
#error "Matter library: mbedTLS needs MBEDTLS_PK_C, MBEDTLS_PK_PARSE_C and MBEDTLS_PK_WRITE_C"
#endif
#if !defined(MBEDTLS_X509_USE_C) || !defined(MBEDTLS_X509_CREATE_C)
#error "Matter library: mbedTLS needs MBEDTLS_X509_USE_C and MBEDTLS_X509_CREATE_C"
#endif
#if !defined(MBEDTLS_X509_CRT_PARSE_C) || !defined(MBEDTLS_X509_CSR_PARSE_C) || !defined(MBEDTLS_X509_CSR_WRITE_C)
#error "Matter library: mbedTLS needs MBEDTLS_X509_CRT_PARSE_C, MBEDTLS_X509_CSR_PARSE_C and MBEDTLS_X509_CSR_WRITE_C"
#endif

// --- the layout: the sizes the library was built with ----------------------------------------------------------------

#include "matter_abi.h"
