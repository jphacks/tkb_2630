/*******************************************************************************
* Copyright 2021-2024, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/


#include "HardwareIface.h"

/* -----------------------------------------------------------------------------
 * Hardware configuration details (summary)
 * 1. Hall sensor interface     : HALL_0_PORT, HALL_1_PORT, HALL_2_PORT
 * 2. Hall Enable pin           : N_HALL_EN_PORT
 * 3. Encode interface          : POSIF_ENC_HW
 * 4. Encoder Enable pin        : ENC_EN_PORT
 * 5. Phase voltage measurement : ADC sampling flags (VV/VU/VW)
 * 6. Current measurement       : (to be defined)
 * 7. Temperature measurement   : ADC_SAMP_TEMP_ENABLED
 * 8. POT measurement           : ADC_SAMP_VPOT_ENABLED (to be defined)
 * 9. Fault Input               : N_FAULT_HW_PORT
 * 10. Direction LED            : DIR_LED_PORT
 * 11. Direction input          : DIR_SWITCH_PORT and N_DIR_PUSHBTN_PORT
 * 12. Fault LED                : N_FAULT_LED_PORT, N_FAULT_LED_ALL_PORT
 * 13. Brake input              : N_BRK_SWITCH_PORT (NA)
 * 14. Smart Gate Driver        : USING_SGD (NA)
 * ----------------------------------------------------------------------------*/


/* -----------------------------------------------------------------------------
 * Hardware-agnostic to hardware-dependent function mapping
 * -----------------------------------------------------------------------------
 * Connect the generic hardware interface function pointers to their MCU-specific
 * implementations. This allows the rest of the system to call through a uniform
 * interface without needing to know the underlying specifics.
 * -----------------------------------------------------------------------------
 */
void HW_IFACE_ConnectFcnPointers(void)
{
    /* Core lifecycle and control callbacks */
    hw_fcn.HardwareIfaceInit        = MCU_Init;                      /* system initialization */
    hw_fcn.EnterCriticalSection     = MCU_EnterCriticalSection;      /* Enter critical region */
    hw_fcn.ExitCriticalSection      = MCU_ExitCriticalSection;       /* Exit  critical region */

    /* Gate driver related callbacks (profiler/state machine usage) */
    hw_fcn.GateDriverEnterHighZ     = MCU_GateDriverEnterHighZ;      /* Put gate driver in high-Z  */
    hw_fcn.GateDriverExitHighZ      = MCU_GateDriverExitHighZ;       /* Exit high-Z on gate driver */

    /* Peripherals control */
    hw_fcn.StartPeripherals         = MCU_StartPeripherals;          /* Power-up peripherals       */
    hw_fcn.StopPeripherals          = MCU_StopPeripherals;           /* Power-down peripherals     */

    /* Non-volatile storage */
    hw_fcn.FlashRead                = MCU_FlashRead;                 /* Read from flash memory     */
    hw_fcn.FlashWrite               = MCU_FlashWrite;                /* Write to flash memory      */

    /* Sensor measurements */
    hw_fcn.ArePhaseVoltagesMeasured = MCU_ArePhaseVoltagesMeasured;  /* Confirm phase voltage sensing  */
}
