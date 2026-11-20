/* The layout of what the library shares with the firmware, as the library was compiled: one array per item, its size is
 * the size of the type (or the value of the constant). Not part of libMatterApp.a: tools/abi.py reads the sizes of
 * these symbols from this object (nm -S) and writes matter_abi.h of the release package, which the firmware compiles
 * with its own SDK and config (matter_requirements.h): a firmware whose lwIP, FreeRTOS or mbedTLS lays these out
 * otherwise stops at build time instead of corrupting memory at run time.
 *
 * The names: abi_<kind>_<what>, <kind> = type (sizeof) or value (a positive constant). */

#include <stdint.h>

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

#include <lwip/ip_addr.h>
#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <lwip/udp.h>

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

#define ABI_TYPE(name, type) char abi_type_##name[sizeof(type)];
#define ABI_VALUE(name, value) char abi_value_##name[(value)];

/* FreeRTOS: the kernel objects the library creates statically or hands to the kernel, the tick */
ABI_TYPE(TickType_t, TickType_t)
ABI_TYPE(BaseType_t, BaseType_t)
ABI_TYPE(StaticTask_t, StaticTask_t)
ABI_TYPE(StaticQueue_t, StaticQueue_t)
ABI_TYPE(StaticSemaphore_t, StaticSemaphore_t)
ABI_TYPE(StaticTimer_t, StaticTimer_t)
ABI_VALUE(configTICK_RATE_HZ, configTICK_RATE_HZ)
ABI_VALUE(configMAX_PRIORITIES, configMAX_PRIORITIES)
#if defined(configNUMBER_OF_CORES)
ABI_VALUE(configNUMBER_OF_CORES, configNUMBER_OF_CORES)
#endif
ABI_VALUE(tskKERNEL_VERSION_MAJOR, tskKERNEL_VERSION_MAJOR)

/* lwIP: addresses, the interfaces the platform walks, the UDP endpoint's PCB and buffers */
ABI_TYPE(ip_addr_t, ip_addr_t)
ABI_TYPE(ip4_addr_t, ip4_addr_t)
ABI_TYPE(ip6_addr_t, ip6_addr_t)
ABI_TYPE(struct_netif, struct netif)
ABI_TYPE(struct_pbuf, struct pbuf)
ABI_TYPE(struct_udp_pcb, struct udp_pcb)
/* LOCK_TCPIP_CORE() expands in the library's code: with core locking on one side only, lwIP runs unlocked */
ABI_VALUE(LWIP_TCPIP_CORE_LOCKING_1, LWIP_TCPIP_CORE_LOCKING + 1)

/* mbedTLS: the contexts CHIPCryptoPAL keeps in its own objects and the firmware's mbedTLS works on */
ABI_VALUE(MBEDTLS_VERSION_MAJOR, MBEDTLS_VERSION_MAJOR)
ABI_VALUE(MBEDTLS_VERSION_MINOR_1, MBEDTLS_VERSION_MINOR + 1)
ABI_TYPE(mbedtls_mpi, mbedtls_mpi)
ABI_TYPE(mbedtls_aes_context, mbedtls_aes_context)
ABI_TYPE(mbedtls_ccm_context, mbedtls_ccm_context)
ABI_TYPE(mbedtls_sha256_context, mbedtls_sha256_context)
ABI_TYPE(mbedtls_md_context_t, mbedtls_md_context_t)
ABI_TYPE(mbedtls_ecp_group, mbedtls_ecp_group)
ABI_TYPE(mbedtls_ecp_point, mbedtls_ecp_point)
ABI_TYPE(mbedtls_ecp_keypair, mbedtls_ecp_keypair)
ABI_TYPE(mbedtls_ecdh_context, mbedtls_ecdh_context)
ABI_TYPE(mbedtls_entropy_context, mbedtls_entropy_context)
ABI_TYPE(mbedtls_ctr_drbg_context, mbedtls_ctr_drbg_context)
ABI_TYPE(mbedtls_hmac_drbg_context, mbedtls_hmac_drbg_context)
ABI_TYPE(mbedtls_pk_context, mbedtls_pk_context)
ABI_TYPE(mbedtls_x509_crt, mbedtls_x509_crt)
ABI_TYPE(mbedtls_x509_csr, mbedtls_x509_csr)
ABI_TYPE(mbedtls_x509write_csr, mbedtls_x509write_csr)
