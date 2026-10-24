/*******************************************************************************
* Copyright 2024, Cypress Semiconductor Corporation (an Infineon company) or
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

#pragma once

#include "cybsp.h"
#include "General.h"

#define MOTOR_CTRL_MOTOR0_ENABLED  (1U)  /*Always true, minimum one motor should be configured*/
/* Temperature sensor configurations */
#define ACTIVE_TEMP_SENSOR  false        // Active IC (e.g. MCP9700T-E/TT) vs Passive NTC (e.g. NCP18WF104J03RB)
#define TEMP_SENSOR_1D_MV  (10.0f)  //mV
#define TEMP_SENSOR_0D_MV  (-250.0f) //mV
#define TEMP_SENSOR_SCALE  ((TEMP_SENSOR_1D_MV * (1 << 12U))/(ADC_VREF_GAIN * CY_CFG_PWR_VDDA_MV))
#define TEMP_SENSOR_OFFSET ((TEMP_SENSOR_SCALE/TEMP_SENSOR_1D_MV)*TEMP_SENSOR_0D_MV) 

extern  TEMP_SENS_LUT_t     Temp_Sens_LUT;


/* PWM configurations*/
#define PWM_INVERSION       (false)
#define PWM_TRIG_ADVANCE    (0U)        // [ticks]

/* Miscellaneous BSP definitions */
#define KIT_ID                (0x0009UL)    // For GUI's recognition of HW
/* CPU load calculation configuration */
#define CPU_LOAD_CALC_ENABLED           (1U)      // Set to (0U) to disable CPU load measurement
#define CPU_LOAD_CALC_TIMEOUT           (10.0f)   // [s] Overflow threshold: cpu_load set to 100% if exceeded
#define CPU_LOAD_CALC_IDLE_TASKTIME     (0.100f)  // [s] Idle busy-wait measurement window
#define CPU_LOAD_CALC_IDLE_TASKOVERHEAD (0.000f)  // [s] Known overhead to subtract from idle measurement


enum
{
    // ADC sequence 0 results
    ADC_ISAMPA = 0, ADC_ISAMPC = 1, ADC_VBUS = 2, ADC_TEMP = 3, ADC_VV = 4,
    // ADC sequence 1 results
    ADC_ISAMPB = 5, ADC_ISAMPD = 6, ADC_VPOT = 7, ADC_VU = 8,   ADC_VW = 9,
    // Totals
    ADC_SEQ_MAX = 2, ADC_SAMP_PER_SEQ_MAX = 5, ADC_MAX = 10
};
extern void* ADC_Result_Regs[ADC_SEQ_MAX][ADC_SAMP_PER_SEQ_MAX];
extern uint8_t DMA_Result_Indices[ADC_SEQ_MAX][ADC_SAMP_PER_SEQ_MAX];
extern cy_stc_dma_descriptor_t* DMA_Descriptors[ADC_SEQ_MAX][ADC_SAMP_PER_SEQ_MAX];
extern const cy_stc_dma_descriptor_config_t* DMA_Descriptor_Configs[ADC_SEQ_MAX][ADC_SAMP_PER_SEQ_MAX];

// 2 simultaneous sampling ADCs
void MCU_RoutingConfigMUXA();  // Routing ADC sequences, ADC0::[ISAMPA,ISAMPC,VBUS,TEMP,VV] & ADC1::[ISAMPB,ISAMPD,VPOT,VU,VW], {ISAMPA,ISAMPB,ISAMPC,ISAMPD}={IU,IV,IW,IDCLINKAVG}
void MCU_RoutingConfigMUXB();  // Routing ADC sequences, ADC0::[ISAMPA,ISAMPC,VBUS,TEMP,VV] & ADC1::[ISAMPB,ISAMPD,VPOT,VU,VW], {ISAMPA,ISAMPB,ISAMPC,ISAMPD}={IDCLINK,IDCLINK,IDCLINKAVG,IDCLINKAVG}

