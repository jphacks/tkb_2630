#ifndef _PARTITION_PSC3_H_
#define _PARTITION_PSC3_H_

#include "cy_device_headers.h"

#define SRAM0_NS_SAHB_START         0x24000000
#define SRAM0_S_SAHB_START          0x34000000
#define SRAM0_NS_CBUS_START         0x04000000
#define SRAM0_S_CBUS_START          0x14000000
#define SRAM0_SIZE                  0x00010000
#define MMIO_NS_START               0x42000000
#define MMIO_S_START                0x52000000
#define MMIO_SIZE                   0x02000000
#define FLASH_NS_SAHB_START         0x22000000
#define FLASH_S_SAHB_START          0x32000000
#define FLASH_NS_CBUS_START         0x02000000
#define FLASH_S_CBUS_START          0x12000000
#define FLASH_SIZE                  CY_FLASH_SIZE
#if defined(USER_FLASH_S_SIZE)
#define FLASH_S_SIZE                (USER_FLASH_S_SIZE)
#else
#define FLASH_S_SIZE                0x00040000
#endif
#define SRAM0_S_CBUS_CODE_START     0x14000000
#if defined(USER_SRAM_S_SIZE)
#define SRAM0_S_SIZE                (USER_SRAM_S_SIZE)
#else
#define SRAM0_S_SIZE                0x00010000
#endif
#define SRAM0_SHM_SIZE              0x00001000
/* NSC (Non-Secure Callable) region size - use USER_NSC_SIZE to overwrite the NSC
 * region size from the application Makefile:
 *     DEFINES += USER_NSC_SIZE=0x00000100
 *
 * ARMv8-M SAU regions are 32-byte aligned, so USER_NSC_SIZE must be a
 * multiple of 32 bytes (0x20). */
#if (CY_PDL_FLASH_BOOT)
    #if defined(USER_NSC_SIZE)
        #define FLASH_NSC_SIZE      (USER_NSC_SIZE)
    #else
        #define FLASH_NSC_SIZE      0x00000100
    #endif
    #define SRAM0_NSC_SIZE          0x00000000
#else
    #if defined(USER_NSC_SIZE)
        #define SRAM0_NSC_SIZE      (USER_NSC_SIZE)
    #else
        #define SRAM0_NSC_SIZE      0x00000800
    #endif
    #define FLASH_NSC_SIZE          0x00000100   /* unused (RAM boot) */
#endif

/* SAU regions have 32-byte granularity: ARMv8-M SAU_RBAR/RLAR use bits [31:5],
 * so a region size must be a multiple of 32 bytes.
 */
#define CY_SAU_REGION_ALIGNMENT       0x20U
#define CY_SAU_SIZE_IS_ALIGNED(size)  (((size) % CY_SAU_REGION_ALIGNMENT) == 0U)

#if defined(USER_NSC_SIZE) && !CY_SAU_SIZE_IS_ALIGNED(USER_NSC_SIZE)
#error "USER_NSC_SIZE must be a multiple of 32 bytes (SAU region alignment)."
#endif

#define SRAM0_SHM_S_SIZE            0x00000800
#define SRAM0_SHM_NS_SIZE           0x00000800
#define SRAM0_NS_SIZE               (SRAM0_SIZE - SRAM0_S_SIZE - SRAM0_SHM_SIZE - SRAM0_NSC_SIZE)
#define SRAM0_NS_CBUS_CODE_START    (SRAM0_NS_CBUS_START + SRAM0_S_SIZE)
#define FLASH_NS_CBUS_CODE_START    (FLASH_NS_CBUS_START + FLASH_S_SIZE)

#define SRAM0_NS_OFFSET             (SRAM0_S_SIZE)
#define SRAM0_SHM_OFFSET            (SRAM0_S_SIZE + SRAM0_NS_SIZE)
#define SRAM0_NSC_OFFSET            (SRAM0_S_SIZE + SRAM0_NS_SIZE + SRAM0_SHM_SIZE)
#define FLASH_NS_OFFSET             (FLASH_S_SIZE)
#define FLASH_NS_SIZE               (FLASH_SIZE - FLASH_S_SIZE)

#define SRAM0_S_SAHB_SHARED_START   (SRAM0_S_SAHB_START + SRAM0_SIZE - SRAM0_SHM_S_SIZE - SRAM0_NSC_SIZE)
#define SRAM0_S_CBUS_SHARED_START   (SRAM0_S_CBUS_START + SRAM0_SIZE - SRAM0_SHM_S_SIZE - SRAM0_NSC_SIZE)
#define SRAM0_NS_SAHB_SHARED_START  (SRAM0_NS_SAHB_START + SRAM0_SIZE - SRAM0_SHM_SIZE - SRAM0_NSC_SIZE)
#define SRAM0_NS_CBUS_SHARED_START  (SRAM0_NS_CBUS_START + SRAM0_SIZE - SRAM0_SHM_SIZE - SRAM0_NSC_SIZE)

/** SAU regions */
#define SRAM0_NS_SBUS_SAU_START     (SRAM0_NS_SAHB_START + SRAM0_S_SIZE)
#define SRAM0_NS_SBUS_SAU_END       (SRAM0_NS_SAHB_START + SRAM0_SIZE - SRAM0_NSC_SIZE)
#define SRAM0_NS_CBUS_SAU_START     (SRAM0_NS_CBUS_START + SRAM0_S_SIZE)
#define SRAM0_NS_CBUS_SAU_END       (SRAM0_NS_CBUS_START + SRAM0_SIZE - SRAM0_NSC_SIZE)
#define MMIO_NS_SAU_START           (MMIO_NS_START)
#define MMIO_NS_SAU_END             (MMIO_NS_START + MMIO_SIZE)
#define FLASH_NS_SBUS_SAU_START     (FLASH_NS_SAHB_START + FLASH_S_SIZE)
#define FLASH_NS_SBUS_SAU_END       (FLASH_NS_SAHB_START + FLASH_SIZE)
#define FLASH_NS_CBUS_SAU_START     (FLASH_NS_CBUS_START + FLASH_S_SIZE)
#define FLASH_NS_CBUS_SAU_END       (FLASH_NS_CBUS_START + FLASH_SIZE)
#if (CY_PDL_FLASH_BOOT)
#define NS_BOOT_START               (FLASH_NS_CBUS_CODE_START)
#define NSC_SAU_START               (FLASH_S_CBUS_START + FLASH_S_SIZE - FLASH_NSC_SIZE)
#define NSC_SAU_END                 (FLASH_S_CBUS_START + FLASH_S_SIZE)
#else
#define NS_BOOT_START               (SRAM0_NS_CBUS_CODE_START)
#define NSC_SAU_START               (SRAM0_S_CBUS_START + SRAM0_SIZE - SRAM0_NSC_SIZE)
#define NSC_SAU_END                 (SRAM0_S_CBUS_START + SRAM0_SIZE)
#endif

#endif /* _PARTITION_PSC3_H_ */

