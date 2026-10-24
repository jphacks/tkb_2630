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

#include "MotorCtrlHWConfig.h"

TEMP_SENS_LUT_t   Temp_Sens_LUT   =
{
    .step = 1.0f / (TEMP_SENS_LUT_WIDTH + 1.0f),    // [%], normalized voltage wrt Vcc
    .step_inv = (TEMP_SENS_LUT_WIDTH + 1.0f),       // [1/%], inverse normalized voltage
    .val = {95.9f, 73.4f, 60.6f, 51.5f, 44.2f, 38.1f, 32.6f, 27.5f, 22.6f, 17.8f, 13.0f, 8.0f, 2.5f, -3.7f, -11.5f, -23.0f} // [degree C]
};

#define ADC_RESULT_ADDR(instance, channel)    ((void*)&ADC_##instance##_CH##channel##_HW->RESULT)


void* ADC_Result_Regs[ADC_SEQ_MAX][ADC_SAMP_PER_SEQ_MAX] = \
        {{ADC_RESULT_ADDR(0,0), ADC_RESULT_ADDR(0,1), ADC_RESULT_ADDR(0,2)},
         {ADC_RESULT_ADDR(1,0), ADC_RESULT_ADDR(1,1), ADC_RESULT_ADDR(1,2)},
         {ADC_RESULT_ADDR(2,0), ADC_RESULT_ADDR(2,1), ADC_RESULT_ADDR(0,3)}};

uint8_t DMA_Result_Indices[ADC_SEQ_MAX][ADC_SAMP_PER_SEQ_MAX] = \
        {{ADC_ISAMPA, ADC_VU, ADC_VBUS},
         {ADC_ISAMPB, ADC_VV, ADC_VPOT},
         {ADC_ISAMPC, ADC_VW, ADC_TEMP}};

cy_stc_dma_descriptor_t* DMA_Descriptors[ADC_SEQ_MAX][ADC_SAMP_PER_SEQ_MAX] = \
        {{&DMA_ADC_0_Descriptor_0, &DMA_ADC_0_Descriptor_1, &DMA_ADC_0_Descriptor_2},
         {&DMA_ADC_1_Descriptor_0, &DMA_ADC_1_Descriptor_1, &DMA_ADC_1_Descriptor_2},
         {&DMA_ADC_2_Descriptor_0, &DMA_ADC_2_Descriptor_1, &DMA_ADC_0_Descriptor_3}};

const cy_stc_dma_descriptor_config_t* DMA_Descriptor_Configs[ADC_SEQ_MAX][ADC_SAMP_PER_SEQ_MAX] = \
        {{&DMA_ADC_0_Descriptor_0_config, &DMA_ADC_0_Descriptor_1_config, &DMA_ADC_0_Descriptor_2_config},
         {&DMA_ADC_1_Descriptor_0_config, &DMA_ADC_1_Descriptor_1_config, &DMA_ADC_1_Descriptor_2_config},
         {&DMA_ADC_2_Descriptor_0_config, &DMA_ADC_2_Descriptor_1_config, &DMA_ADC_0_Descriptor_3_config}};


void MCU_RoutingConfigMUXA()
{
    cy_stc_sar2_channel_config_t adc_channel_config;

    adc_channel_config = *ADC_0_config.channelConfig[0];
    adc_channel_config.pinAddress =  CY_SAR2_PIN_ADDRESS_VMOTOR;
    Cy_SAR2_Channel_Init(ADC_0_HW, 0, &adc_channel_config);

    adc_channel_config = *ADC_1_config.channelConfig[0];
    adc_channel_config.pinAddress =  CY_SAR2_PIN_ADDRESS_VMOTOR;
    Cy_SAR2_Channel_Init(ADC_1_HW, 0, &adc_channel_config);
}

void MCU_RoutingConfigMUXB()
{
    cy_stc_sar2_channel_config_t adc_channel_config;

    adc_channel_config = *ADC_0_config.channelConfig[0];
    adc_channel_config.pinAddress = CY_SAR2_PIN_ADDRESS_AN26;
    Cy_SAR2_Channel_Init(ADC_0_HW, 0, &adc_channel_config);

    adc_channel_config = *ADC_1_config.channelConfig[0];
    adc_channel_config.pinAddress = CY_SAR2_PIN_ADDRESS_AN1;
    Cy_SAR2_Channel_Init(ADC_1_HW, 0, &adc_channel_config);
}

void MCU_DisableTimerReload()
{
    TCPWM_GRP_CNT_TR_OUT_SEL(SYNC_ISR1_HW, TCPWM_GRP_CNT_GET_GRP(SYNC_ISR1_NUM), SYNC_ISR1_NUM) =
      (_VAL2FLD(TCPWM_GRP_CNT_V2_TR_OUT_SEL_OUT0, CY_TCPWM_CNT_TRIGGER_ON_DISABLED) |
       _VAL2FLD(TCPWM_GRP_CNT_V2_TR_OUT_SEL_OUT1, CY_TCPWM_CNT_TRIGGER_ON_DISABLED));
}

void MCU_EnableTimerReload()
{
    TCPWM_GRP_CNT_TR_OUT_SEL(SYNC_ISR1_HW, TCPWM_GRP_CNT_GET_GRP(SYNC_ISR1_NUM), SYNC_ISR1_NUM) =
      (_VAL2FLD(TCPWM_GRP_CNT_V2_TR_OUT_SEL_OUT0, CY_TCPWM_CNT_TRIGGER_ON_OVERFLOW) |
       _VAL2FLD(TCPWM_GRP_CNT_V2_TR_OUT_SEL_OUT1, CY_TCPWM_CNT_TRIGGER_ON_DISABLED));
}

