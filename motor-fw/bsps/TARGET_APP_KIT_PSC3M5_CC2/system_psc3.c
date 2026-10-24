/***************************************************************************//**
* \file system_psc3.c
* \version 1.0
*
* The device system-source file.
*
********************************************************************************
* \copyright
* (c) 2020-2026, Infineon Technologies AG or an affiliate of
* Infineon Technologies AG.
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include "cy_device.h"

#if defined (CY_DEVICE_PSC3)

#include <stdbool.h>
#include "system_cat1b.h"
#include "cy_syslib.h"
#include "cy_wdt.h"
#include "cy_sysclk.h"
#include "cy_syspm.h"

/*******************************************************************************
* SystemCoreClockUpdate()
*******************************************************************************/

/** Default HFClk frequency in Hz. This frequency is configured during boot
 * initialization. Update this value with the exact frequency configured in the
 * provisioning policy. The maximum possible value is used by default.
 */
#define CY_CLK_HFCLK0_FREQ_HZ_DEFAULT       (CY_HF_CLK_MAX_FREQ)

/** Default PeriClk frequency in Hz. This frequency is configured during boot
 * initialization. Update this value with the exact frequency configured in the
 * provisioning policy. The maximum possible value is used by default to ensure
 * delays are not shorter than expected.
 */
#define CY_CLK_PERICLK_FREQ_HZ_DEFAULT      (CY_HF_CLK_MAX_FREQ)

/** Default system core frequency in Hz. This frequency is configured during boot
 * initialization. Update this value with the exact frequency configured in the
 * provisioning policy. The maximum possible value is used by default.
 */
#define CY_CLK_SYSTEM_FREQ_HZ_DEFAULT       (CY_HF_CLK_MAX_FREQ)

/** Holds the CLK_HF0 system core clock. */
uint32_t SystemCoreClock = CY_CLK_SYSTEM_FREQ_HZ_DEFAULT;

/** Holds the HFClk0 clock frequency. Updated by \ref SystemCoreClockUpdate(). */
uint32_t cy_Hfclk0FreqHz  = CY_CLK_HFCLK0_FREQ_HZ_DEFAULT;

/** Holds the PeriClk clock frequency. Updated by \ref SystemCoreClockUpdate(). */
uint32_t cy_PeriClkFreqHz = CY_CLK_PERICLK_FREQ_HZ_DEFAULT;

/** Holds the AHB frequency. Updated by \ref SystemCoreClockUpdate(). */
uint32_t cy_AhbFreqHz = CY_CLK_SYSTEM_FREQ_HZ_DEFAULT;

/*******************************************************************************
* SystemCoreClockUpdate (void)
*******************************************************************************/

/* Do not use these definitions directly in your application */
#define CY_DELAY_MS_OVERFLOW_THRESHOLD  (0x8000u)
#define CY_DELAY_1K_THRESHOLD           (1000u)
#define CY_DELAY_1K_MINUS_1_THRESHOLD   (CY_DELAY_1K_THRESHOLD - 1u)
#define CY_DELAY_1M_THRESHOLD           (1000000u)
#define CY_DELAY_1M_MINUS_1_THRESHOLD   (CY_DELAY_1M_THRESHOLD - 1u)
uint32_t cy_delayFreqHz   = CY_CLK_SYSTEM_FREQ_HZ_DEFAULT;

uint32_t cy_delayFreqKhz  = (CY_CLK_SYSTEM_FREQ_HZ_DEFAULT + CY_DELAY_1K_MINUS_1_THRESHOLD) /
                            CY_DELAY_1K_THRESHOLD;

uint8_t cy_delayFreqMhz  = (uint8_t)((CY_CLK_SYSTEM_FREQ_HZ_DEFAULT + CY_DELAY_1M_MINUS_1_THRESHOLD) /
                            CY_DELAY_1M_THRESHOLD);

/* Reset values for each PERI group */
#define CY_PERI_GR1_SL_CTL 0x0F
#define CY_PERI_GR2_SL_CTL 0x03
#define CY_PERI_GR3_SL_CTL 0x3F
#define CY_PERI_GR4_SL_CTL 0x03
#define CY_PERI_GR5_SL_CTL 0x01


#if !defined(CY_DISABLE_WARM_BOOT)

CY_SECTION_RAMFUNC_BEGIN
/*******************************************************************************
* Function Name: SystemInit_Warmboot_CAT1B_CM33
****************************************************************************//**
*
* Prepares the system to work after warmboot:
* - Initializes Vector Table
* - Enables all the IP's through Slave Control Registers
* - Unfreezes the IO's
*
*******************************************************************************/
void SystemInit_Warmboot_CAT1B_CM33(void)
{
    #if defined(CY_PDL_TZ_ENABLED)
    SCB->VTOR = (uint32_t)__s_vector_table_rw;
    #else
    SCB->VTOR = (uint32_t)__ns_vector_table_rw;
    #endif
    (void)Cy_SysClk_PeriGroupSetSlaveCtl(1, CY_SYSCLK_PERI_GROUP_SL_CTL2, 0x0U);
    (void)Cy_SysClk_PeriGroupSetSlaveCtl(2, CY_SYSCLK_PERI_GROUP_SL_CTL2, 0x0U);
    (void)Cy_SysClk_PeriGroupSetSlaveCtl(3, CY_SYSCLK_PERI_GROUP_SL_CTL2, 0x0U);
    (void)Cy_SysClk_PeriGroupSetSlaveCtl(4, CY_SYSCLK_PERI_GROUP_SL_CTL2, 0x0U);
    (void)Cy_SysClk_PeriGroupSetSlaveCtl(5, CY_SYSCLK_PERI_GROUP_SL_CTL2, 0x0U);

    (void)Cy_SysClk_PeriGroupSetSlaveCtl(1, CY_SYSCLK_PERI_GROUP_SL_CTL, 0xFFFFFFFFU);
    (void)Cy_SysClk_PeriGroupSetSlaveCtl(2, CY_SYSCLK_PERI_GROUP_SL_CTL, 0xFFFFFFFFU);
    (void)Cy_SysClk_PeriGroupSetSlaveCtl(3, CY_SYSCLK_PERI_GROUP_SL_CTL, 0xFFFFFFFFU);
    (void)Cy_SysClk_PeriGroupSetSlaveCtl(4, CY_SYSCLK_PERI_GROUP_SL_CTL, 0xFFFFFFFFU);
    (void)Cy_SysClk_PeriGroupSetSlaveCtl(5, CY_SYSCLK_PERI_GROUP_SL_CTL, 0xFFFFFFFFU);

    if (Cy_SysPm_DeepSleepIoIsFrozen())
    {
        Cy_SysPm_DeepSleepIoUnfreeze();
    }

    cy_WakeupFromWarmBootStatus = true;
}
CY_SECTION_RAMFUNC_END

#define CY_NVIC_REG_COUNT 3U
#define CY_NVIC_IPR_REG_COUNT 69U

uint32_t nvicStoreRestore[CY_NVIC_REG_COUNT];
uint32_t nvicIPRStoreRestore[CY_NVIC_IPR_REG_COUNT];
uint32_t scbSHPR3StoreRestore;
#define SCB_SHPR3_REG     ( *( ( volatile uint32_t * ) 0xe000ed20 ) )

/*******************************************************************************
* Function Name: System_Store_NVIC_Reg
****************************************************************************//**
*
* Stores the NVIC register before Deepsleep RAM:
*
*******************************************************************************/
CY_SECTION_RAMFUNC_BEGIN
void System_Store_NVIC_Reg(void)
{
    for (uint32_t idx = 0; idx < CY_NVIC_REG_COUNT; idx++)
    {
        nvicStoreRestore[idx] = NVIC->ISER[idx];
    }

    for (uint32_t idx = 0; idx < CY_NVIC_IPR_REG_COUNT; idx++)
    {
        nvicIPRStoreRestore[idx] = NVIC->IPR[idx];
    }

    scbSHPR3StoreRestore = SCB_SHPR3_REG;
}
CY_SECTION_RAMFUNC_END


/*******************************************************************************
* Function Name: System_Restore_NVIC_Reg
****************************************************************************//**
*
* Restores the NVIC register After Deepsleep RAM Wakeup i.e. Warmboot:
*
* Note: the priority registers (NVIC->IPR and SCB_SHPR3) MUST be
* restored BEFORE the interrupt-enable registers (NVIC->ISER). Otherwise
* an interrupt can be enabled while its priority is still 0 (reset
* value), which violates configMAX_SYSCALL_INTERRUPT_PRIORITY and
* triggers a Hard Fault via vPortValidateInterruptPriority() on
* FreeRTOS v10.6+.
*
*******************************************************************************/
CY_SECTION_RAMFUNC_BEGIN
void System_Restore_NVIC_Reg(void)
{
    /* Restore priorities first */
    for (uint32_t idx = 0; idx < CY_NVIC_IPR_REG_COUNT; idx++)
    {
        NVIC->IPR[idx] = nvicIPRStoreRestore[idx];
    }

    SCB_SHPR3_REG = scbSHPR3StoreRestore;

    /* Then enable the interrupts */
    for (uint32_t idx = 0; idx < CY_NVIC_REG_COUNT; idx++)
    {
        NVIC->ISER[idx] = nvicStoreRestore[idx];
    }
}
CY_SECTION_RAMFUNC_END
#endif /* !defined(CY_DISABLE_WARM_BOOT) */

void SystemInit(void)
{
    (void)Cy_SystemInit(); /* typecast void to suppress a compiler warning about unused return value */
}

/*******************************************************************************
* Function Name: Cy_SystemInit
****************************************************************************//**
*
* The function is called during device startup.
*
*******************************************************************************/
__WEAK void Cy_SystemInit(void)
{
     /* Empty weak function.
     */
     __NOP(); /* No operation */
}

/*******************************************************************************
* Function Name: SystemCoreClockUpdate
****************************************************************************//**
*
* The function is called during device startup.
*
*******************************************************************************/
void SystemCoreClockUpdate (void)
{
    uint32_t pathFreqHz;
    uint32_t clkHfPath;

    /* Get frequency for the high-frequency clock # 0 */
    clkHfPath = CY_SYSCLK_CLK_CORE_HF_PATH_NUM;

    pathFreqHz = Cy_SysClk_ClkHfGetFrequency(clkHfPath);

    SystemCoreClock = pathFreqHz;

    cy_Hfclk0FreqHz = SystemCoreClock;

    /* Get frequency for the high-frequency clock # 2 , which is used for PERI PCLK*/
    clkHfPath = CY_SYSCLK_CLK_PERI_HF_PATH_NUM;

    pathFreqHz = Cy_SysClk_ClkHfGetFrequency(clkHfPath);

    cy_PeriClkFreqHz = pathFreqHz;

    /* Sets clock frequency for Delay API */
    cy_delayFreqHz = SystemCoreClock;
    cy_delayFreqMhz = (uint8_t)((cy_delayFreqHz + CY_DELAY_1M_MINUS_1_THRESHOLD) / CY_DELAY_1M_THRESHOLD);
    cy_delayFreqKhz = (cy_delayFreqHz + CY_DELAY_1K_MINUS_1_THRESHOLD) / CY_DELAY_1K_THRESHOLD;

    /* Get the frequency of AHB source, CLK HF0 is the source for AHB*/
    cy_AhbFreqHz = Cy_SysClk_ClkHfGetFrequency(0UL);
}


/*******************************************************************************
* Function Name: SystemCoreClockSetup
****************************************************************************//**
*
* Populates system clock frequency variables with the provided value.
* Sets the clock frequencies of \ref SystemCoreClock, \ref cy_AhbFreqHz,
* \ref cy_Hfclk0FreqHz and \ref cy_PeriClkFreqHz variables.
*
* \param systemCoreClk_freq_hz
* Frequency in Hz for the System Core Clock
*
* \param ahb_freq_hz
* Frequency in Hz of the AHB source
*
*******************************************************************************/
void SystemCoreClockSetup(uint32_t systemCoreClk_freq_hz, uint32_t ahb_freq_hz)
{
    SystemCoreClock = systemCoreClk_freq_hz;

    cy_Hfclk0FreqHz = systemCoreClk_freq_hz;

    cy_PeriClkFreqHz = systemCoreClk_freq_hz;

    /* Sets clock frequency for Delay API */
    cy_delayFreqHz = SystemCoreClock;
    cy_delayFreqMhz = (uint8_t)((cy_delayFreqHz + CY_DELAY_1M_MINUS_1_THRESHOLD) / CY_DELAY_1M_THRESHOLD);
    cy_delayFreqKhz = (cy_delayFreqHz + CY_DELAY_1K_MINUS_1_THRESHOLD) / CY_DELAY_1K_THRESHOLD;

    /* Get the frequency of AHB source, CLK HF0 is the source for AHB*/
    cy_AhbFreqHz = ahb_freq_hz;
}

#endif /* defined (CY_DEVICE_PSC3) */
/* [] END OF FILE */

