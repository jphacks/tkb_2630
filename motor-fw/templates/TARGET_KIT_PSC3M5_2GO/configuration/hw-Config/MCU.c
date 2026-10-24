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
#include "MCU.h"
#include "mtb_oscilloscope.h"
#include "mtb_oscilloscope_cfg.h"
#include "ParamConfig.h"
#include "PgOut.h"

// ADC Configuration Constants
#define ADC_RESOLUTION_BITS       (12U)                          // HPPASS SAR ADC resolution
#define ADC_MAX_VALUE             (1U << ADC_RESOLUTION_BITS)    // 4096 ticks
#define ADC_VOLTAGE_PER_TICK_V    ((CY_CFG_PWR_VDDA_MV * 1.0E-3f) / (float)ADC_MAX_VALUE)
#define CURR_ADC_HALF_POINT_TICKS (0x1<<11)

// Current sensing offset detection
#define CURR_OFF_FAULT_PHASE_U    (0x01U)
#define CURR_OFF_FAULT_PHASE_V    (0x02U)
#define CURR_OFF_FAULT_PHASE_W    (0x04U)
#define CURR_OFF_ALL_FAULT_PHASE  (0x07U)

MCU_t mcu[MOTOR_CTRL_NO_OF_MOTOR];
const uint8_t *Em_Eeprom_Storage[MOTOR_CTRL_NO_OF_MOTOR] = {
        (uint8_t *)(CY_FLASH_BASE + CY_FLASH_SIZE - (srss_0_eeprom_0_PHYSICAL_SIZE))
};

#if defined(ANALOG_ROUTING_MUX_RUNTIME)
void (*MCU_RoutingConfigMUX0Wrap)() = &EmptyFcn;   // Either MUXA0 or MUXB0
void (*MCU_RoutingConfigMUX1Wrap)() = &EmptyFcn;   // Either MUXA1 or MUXB1
#else
void (*MCU_RoutingConfigMUXWrap)() = &EmptyFcn;    // Either MUXA or MUXB
#endif

/******************************************************************************/
static void DMA_ADC_0_RunISR(void);
static void DMA_ADC_1_RunISR(void);

static void MCU_InitChipInfo(void);

static void MCU_PhaseUEnterHighZ(void);
static void MCU_PhaseUEnterHighZ(void);
static void MCU_PhaseVEnterHighZ(void);
static void MCU_PhaseVExitHighZ(void);
static void MCU_PhaseWEnterHighZ(void);
static void MCU_PhaseWExitHighZ(void);

static void MCU_RestartKilledPWMs(void);

static float MCU_TempSensorCalc(void);

static void MCU_InitADCs(void);
static void MCU_InitTimers(void);
static void MCU_FlashInit(void);
static void MCU_InitAnalogRouting(void);
static void MCU_InitDMAs(void);
static void MCU_InitPosInterface(void);
static void MCU_InitWatchdog(void);
static void MCU_InitInterrupts(void);

static void MCU_ComputeOffsetVoltage(uint8_t motor_id);
/******************************************************************************/
static void MCU_InitChipInfo(void)
{
    mc_info.chip_id = Cy_SysLib_GetDevice();
    mc_info.chip_id <<= 16;
    mc_info.chip_id |= Cy_SysLib_GetDeviceRevision();

}

RAMFUNC_BEGIN
static void MCU_PhaseUEnterHighZ(void)
{
    Cy_GPIO_SetHSIOM(PWMUL_PORT, PWMUL_NUM, HSIOM_SEL_GPIO);
    Cy_GPIO_SetHSIOM(PWMUH_PORT, PWMUH_NUM, HSIOM_SEL_GPIO);
    Cy_GPIO_Clr(PWMUL_PORT, PWMUL_NUM);
    Cy_GPIO_Clr(PWMUH_PORT, PWMUH_NUM);
}

RAMFUNC_END

RAMFUNC_BEGIN
static void MCU_PhaseUExitHighZ(void)
{

    Cy_GPIO_SetHSIOM(PWMUL_PORT, PWMUL_NUM, PWMUL_HSIOM);
    Cy_GPIO_SetHSIOM(PWMUH_PORT, PWMUH_NUM, PWMUH_HSIOM);

}
RAMFUNC_END

RAMFUNC_BEGIN
static void MCU_PhaseVEnterHighZ(void)
{

    Cy_GPIO_SetHSIOM(PWMVL_PORT, PWMVL_NUM, HSIOM_SEL_GPIO);
    Cy_GPIO_SetHSIOM(PWMVH_PORT, PWMVH_NUM, HSIOM_SEL_GPIO);
    Cy_GPIO_Clr(PWMVL_PORT, PWMVL_NUM);
    Cy_GPIO_Clr(PWMVH_PORT, PWMVH_NUM);
    
}
RAMFUNC_END

RAMFUNC_BEGIN
static void MCU_PhaseVExitHighZ(void)
{

    Cy_GPIO_SetHSIOM(PWMVL_PORT, PWMVL_NUM, PWMVL_HSIOM);
    Cy_GPIO_SetHSIOM(PWMVH_PORT, PWMVH_NUM, PWMVH_HSIOM);

}
RAMFUNC_END

RAMFUNC_BEGIN
static void MCU_PhaseWEnterHighZ(void)
{

    Cy_GPIO_SetHSIOM(PWMWL_PORT, PWMWL_NUM, HSIOM_SEL_GPIO);
    Cy_GPIO_SetHSIOM(PWMWH_PORT, PWMWH_NUM, HSIOM_SEL_GPIO);
    Cy_GPIO_Clr(PWMWL_PORT, PWMWL_NUM);
    Cy_GPIO_Clr(PWMWH_PORT, PWMWH_NUM);

}
RAMFUNC_END

RAMFUNC_BEGIN
static void MCU_PhaseWExitHighZ(void)
{
    Cy_GPIO_SetHSIOM(PWMWL_PORT, PWMWL_NUM, PWMWL_HSIOM);
    Cy_GPIO_SetHSIOM(PWMWH_PORT, PWMWH_NUM, PWMWH_HSIOM);

}
RAMFUNC_END

RAMFUNC_BEGIN
static float MCU_TempSensorCalc(void)
{
    float result;
#if (ACTIVE_TEMP_SENSOR) // Active IC
    result = (mcu[0].adc_scale.temp_ps * (uint16_t)mcu[0].dma_results[ADC_TEMP]) - (TEMP_SENSOR_OFFSET / TEMP_SENSOR_SCALE);
#else // Passive NTC
    float lut_input = mcu[0].adc_scale.temp_ps * (uint16_t)mcu[0].dma_results[ADC_TEMP];
    uint32_t index = SAT(1U, TEMP_SENS_LUT_WIDTH - 1U, (uint32_t)(lut_input * Temp_Sens_LUT.step_inv));
    float input_index = Temp_Sens_LUT.step * index;
    result = Temp_Sens_LUT.val[index-1U] + (lut_input - input_index) * Temp_Sens_LUT.step_inv * (Temp_Sens_LUT.val[index] - Temp_Sens_LUT.val[index-1U]);
#endif
    return result;
}
RAMFUNC_END

static void MCU_InitADCs(void)
{
    // ADC conversion coefficients .............................................
    float cs_gain = motor[0].params_ptr->sys.analog.shunt.opamp_gain;

    if(motor[0].params_ptr->sys.analog.cs_meas_type == Active_Sensor)
    {
      mcu[0].adc_scale.i_uvw = (ADC_VREF_GAIN * CY_CFG_PWR_VDDA_MV * 1.0E-3f) / ((1<<12U) * motor[0].params_ptr->sys.analog.shunt.current_sensitivity * cs_gain); // [A/ticks]
    }
    else
    {
      mcu[0].adc_scale.i_uvw = (ADC_VREF_GAIN * CY_CFG_PWR_VDDA_MV * 1.0E-3f) / ((1<<12U) * motor[0].params_ptr->sys.analog.shunt.res * cs_gain); // [A/ticks]
    }


    mcu[0].adc_scale.v_uvw = (ADC_VREF_GAIN * CY_CFG_PWR_VDDA_MV * 1.0E-3f) / ((1<<12U) * motor[0].params_ptr->sys.analog.volt.vuvw_adc_scale); // [V/ticks]
    mcu[0].adc_scale.v_dc = (ADC_VREF_GAIN * CY_CFG_PWR_VDDA_MV * 1.0E-3f) / ((1<<12U) * motor[0].params_ptr->sys.analog.volt.vdc_adc_scale); // [V/ticks]
    mcu[0].adc_scale.v_pot = 1.0f / (1<<12U); // [%/ticks]
#if (ACTIVE_TEMP_SENSOR)
    mcu[0].adc_scale.temp_ps = (ADC_VREF_GAIN * CY_CFG_PWR_VDDA_MV * 1.0E-3f) / ((1<<12U) * TEMP_SENSOR_SCALE); // [Celsius/ticks]
#else // passive NTC
    mcu[0].adc_scale.temp_ps = 1.0f / (1<<12U); // [1/ticks], normalized voltage wrt Vcc
#endif  
    
    // Configure ADC modules ...................................................
    Cy_HPPASS_Init(&pass_0_config);
}

static void MCU_InitTimers(void)
{
    // Clock frequencies .......................................................
    mcu[0].clk.tcpwm = Cy_SysClk_PeriPclkGetFrequency((en_clk_dst_t)CLK_TCPWM_GRP_NUM, CY_SYSCLK_DIV_8_BIT, CLK_TCPWM_NUM); // [Hz]
    mcu[0].clk.hall = Cy_SysClk_PeriPclkGetFrequency((en_clk_dst_t)CLK_HALL_GRP_NUM, CY_SYSCLK_DIV_8_BIT, CLK_HALL_NUM); // [Hz]
    mcu[0].clk.encoder = Cy_SysClk_PeriPclkGetFrequency((en_clk_dst_t)CLK_ENC_GRP_NUM, CY_SYSCLK_DIV_8_BIT, CLK_ENC_NUM); // [Hz]

    // Timer calculations ......................................................
    motor[0].hall_ptr->per_cap_freq = mcu[0].clk.hall; // [Hz]
    motor[0].inc_encoder_ptr->per_cap_freq = mcu[0].clk.encoder; // [Hz]
    mcu[0].pwm.count = 0U;
    mcu[0].pwm.period = ((uint32_t)(mcu[0].clk.tcpwm * motor[0].params_ptr->sys.samp.tpwm)); // must be even
    mcu[0].pwm.duty_cycle_coeff = (float)(mcu[0].pwm.period >> 1);
    mcu[0].pwm.deadtime = ((uint32_t)(mcu[0].clk.tcpwm * motor[0].params_ptr->sys.samp.deadtime)); 
    mcu[0].isr0.count = 0U;
    mcu[0].isr0.period = mcu[0].pwm.period * motor[0].params_ptr->sys.samp.fpwm_fs0_ratio;
    mcu[0].isr0.duty_cycle_coeff = (float)(mcu[0].isr0.period);
    mcu[0].isr1.count = 0U;
    mcu[0].isr1.period = mcu[0].isr0.period * motor[0].params_ptr->sys.samp.fs0_fs1_ratio;
    mcu[0].isr1.duty_cycle_coeff = (float)(mcu[0].isr1.period);
    mcu[0].isr0_exe.sec_per_tick = (1.0f/mcu[0].clk.tcpwm); // [sec/ticks]
    mcu[0].isr0_exe.inv_max_time = motor[0].params_ptr->sys.samp.fs0; // [1/sec]
    mcu[0].isr1_exe.sec_per_tick = (1.0f/mcu[0].clk.tcpwm); // [sec/ticks]
    mcu[0].isr1_exe.inv_max_time = motor[0].params_ptr->sys.samp.fs1; // [1/sec]A
  
    // Configure timers (TCPWMs) .....................................  
    uint32_t cc0 = PWM_INVERSION ? (mcu[0].pwm.period - PWM_TRIG_ADVANCE) : (mcu[0].pwm.period >> 1);
    
    Cy_TCPWM_PWM_Init(ADC0_ISR0_HW, ADC0_ISR0_NUM, &ADC0_ISR0_config);
    Cy_TCPWM_PWM_Init(ADC1_ISR0_HW, ADC1_ISR0_NUM, &ADC1_ISR0_config);
    Cy_TCPWM_PWM_SetPeriod0(ADC0_ISR0_HW, ADC0_ISR0_NUM, mcu[0].isr0.period - 1U); // Sawtooth carrier
    Cy_TCPWM_PWM_SetPeriod0(ADC1_ISR0_HW, ADC1_ISR0_NUM, mcu[0].isr0.period - 1U); // Sawtooth carrier
    Cy_TCPWM_PWM_SetCompare0Val(ADC0_ISR0_HW, ADC0_ISR0_NUM, cc0); // Read ADCs at the middle of lower switches' on-times
    Cy_TCPWM_PWM_SetCompare1Val(ADC1_ISR0_HW, ADC1_ISR0_NUM, cc0); // Read ADCs at the middle of lower switches' on-times
    Cy_TCPWM_PWM_SetCompare0BufVal(ADC0_ISR0_HW, ADC0_ISR0_NUM, cc0); // Read ADCs at the middle of lower switches' on-times   
    Cy_TCPWM_PWM_SetCompare1BufVal(ADC1_ISR0_HW, ADC1_ISR0_NUM, cc0); // Read ADCs at the middle of lower switches' on-times
    
    Cy_TCPWM_PWM_Init(PWM_SYNC_HW, PWM_SYNC_NUM, &PWM_SYNC_config);
    Cy_TCPWM_PWM_SetPeriod0(PWM_SYNC_HW, PWM_SYNC_NUM, mcu[0].isr0.period - 1U); // Sawtooth carrier
    Cy_TCPWM_PWM_SetCompare0Val(PWM_SYNC_HW, PWM_SYNC_NUM, cc0); // Swap PWMs CC0/CC1 at the middle of lower switches' on-times

    Cy_TCPWM_PWM_Init(PWM_U_HW, PWM_U_NUM, &PWM_U_config);

    Cy_TCPWM_PWM_SetPeriod0(PWM_U_HW, PWM_U_NUM, mcu[0].pwm.period -1); // Triangle carrier
    Cy_TCPWM_PWM_SetCompare0Val(PWM_U_HW, PWM_U_NUM, (mcu[0].pwm.period >> 1) - (mcu[0].pwm.period >> 2)); // Start with duty cycle = 50%
    Cy_TCPWM_PWM_SetCompare1Val(PWM_U_HW, PWM_U_NUM, (mcu[0].pwm.period >> 1) + (mcu[0].pwm.period >> 2)); // Start with duty cycle = 50%
    Cy_TCPWM_PWM_SetCompare0BufVal(PWM_U_HW, PWM_U_NUM, (mcu[0].pwm.period >> 1) - (mcu[0].pwm.period >> 2)); // Start with duty cycle = 50%
    Cy_TCPWM_PWM_SetCompare1BufVal(PWM_U_HW, PWM_U_NUM, (mcu[0].pwm.period >> 1) + (mcu[0].pwm.period >> 2)); // Start with duty cycle = 50%

    Cy_TCPWM_PWM_PWMDeadTime(PWM_U_HW, PWM_U_NUM, mcu[0].pwm.deadtime); // set dead time value
    Cy_TCPWM_PWM_PWMDeadTimeN(PWM_U_HW, PWM_U_NUM, mcu[0].pwm.deadtime); // set dead time value
    Cy_TCPWM_PWM_PWMDeadTimeBuff(PWM_U_HW, PWM_U_NUM, mcu[0].pwm.deadtime); // set dead time value
    Cy_TCPWM_PWM_PWMDeadTimeBuffN(PWM_U_HW, PWM_U_NUM, mcu[0].pwm.deadtime); // set dead time value

    Cy_TCPWM_PWM_Init(PWM_V_HW, PWM_V_NUM, &PWM_V_config);

    Cy_TCPWM_PWM_SetPeriod0(PWM_V_HW, PWM_V_NUM, mcu[0].pwm.period -1); // Triangle carrier
    Cy_TCPWM_PWM_SetCompare0Val(PWM_V_HW, PWM_V_NUM, (mcu[0].pwm.period >> 1) - (mcu[0].pwm.period >> 2)); // Start with duty cycle = 50%
    Cy_TCPWM_PWM_SetCompare1Val(PWM_V_HW, PWM_V_NUM, (mcu[0].pwm.period >> 1) + (mcu[0].pwm.period >> 2)); // Start with duty cycle = 50%
    Cy_TCPWM_PWM_SetCompare0BufVal(PWM_V_HW, PWM_V_NUM, (mcu[0].pwm.period >> 1) - (mcu[0].pwm.period >> 2)); // Start with duty cycle = 50%
    Cy_TCPWM_PWM_SetCompare1BufVal(PWM_V_HW, PWM_V_NUM, (mcu[0].pwm.period >> 1) + (mcu[0].pwm.period >> 2)); // Start with duty cycle = 50%


    Cy_TCPWM_PWM_PWMDeadTime(PWM_V_HW, PWM_V_NUM, mcu[0].pwm.deadtime); // set dead time value
    Cy_TCPWM_PWM_PWMDeadTimeN(PWM_V_HW, PWM_V_NUM, mcu[0].pwm.deadtime); // set dead time value
    Cy_TCPWM_PWM_PWMDeadTimeBuff(PWM_V_HW, PWM_V_NUM, mcu[0].pwm.deadtime); // set dead time value
    Cy_TCPWM_PWM_PWMDeadTimeBuffN(PWM_V_HW, PWM_V_NUM, mcu[0].pwm.deadtime); // set dead time value
    
    Cy_TCPWM_PWM_Init(PWM_W_HW, PWM_W_NUM, &PWM_W_config);

    Cy_TCPWM_PWM_SetPeriod0(PWM_W_HW, PWM_W_NUM, mcu[0].pwm.period -1); // Triangle carrier
    Cy_TCPWM_PWM_SetCompare0Val(PWM_W_HW, PWM_W_NUM, (mcu[0].pwm.period >> 1) - (mcu[0].pwm.period >> 2)); // Start with duty cycle = 50%
    Cy_TCPWM_PWM_SetCompare1Val(PWM_W_HW, PWM_W_NUM, (mcu[0].pwm.period >> 1) + (mcu[0].pwm.period >> 2)); // Start with duty cycle = 50%
    Cy_TCPWM_PWM_SetCompare0BufVal(PWM_W_HW, PWM_W_NUM, (mcu[0].pwm.period >> 1) - (mcu[0].pwm.period >> 2)); // Start with duty cycle = 50%
    Cy_TCPWM_PWM_SetCompare1BufVal(PWM_W_HW, PWM_W_NUM, (mcu[0].pwm.period >> 1) + (mcu[0].pwm.period >> 2)); // Start with duty cycle = 50%

    Cy_TCPWM_PWM_PWMDeadTime(PWM_W_HW, PWM_W_NUM, mcu[0].pwm.deadtime); // set dead time value
    Cy_TCPWM_PWM_PWMDeadTimeN(PWM_W_HW, PWM_W_NUM, mcu[0].pwm.deadtime); // set dead time value
    Cy_TCPWM_PWM_PWMDeadTimeBuff(PWM_W_HW, PWM_W_NUM, mcu[0].pwm.deadtime); // set dead time value
    Cy_TCPWM_PWM_PWMDeadTimeBuffN(PWM_W_HW, PWM_W_NUM, mcu[0].pwm.deadtime); // set dead time value
    
    cc0 = PWM_INVERSION ? (mcu[0].isr1.period - (mcu[0].pwm.period >> 1)) : mcu[0].isr1.period - 1U;
    Cy_TCPWM_PWM_Init(SYNC_ISR1_HW, SYNC_ISR1_NUM, &SYNC_ISR1_config);
    Cy_TCPWM_PWM_SetPeriod0(SYNC_ISR1_HW, SYNC_ISR1_NUM, mcu[0].isr1.period - 1U); // Sawtooth carrier
    Cy_TCPWM_PWM_SetCompare0Val(SYNC_ISR1_HW, SYNC_ISR1_NUM, cc0);

#if defined(CTRL_METHOD_RFO) || defined(CTRL_METHOD_TBC)
    Cy_TCPWM_Counter_Init(HALL_TIMER_HW, HALL_TIMER_NUM, &HALL_TIMER_config);   // Hall sensor speed capture
#endif
    Cy_TCPWM_Counter_Init(EXE_TIMER_HW, EXE_TIMER_NUM, &EXE_TIMER_config);      // Execution timer

#if defined(POSIF_ENC_HW)
    Cy_TCPWM_Counter_Init(ENC_POS_CNTR_HW, ENC_POS_CNTR_NUM, &ENC_POS_CNTR_config);
    Cy_TCPWM_Counter_Init(ENC_TIME_BTW_TICKS_HW, ENC_TIME_BTW_TICKS_NUM, &ENC_TIME_BTW_TICKS_config);
#endif

    motor[0].sensor_iface_ptr->uvw_idx = PWM_INVERSION ? &motor[0].ctrl_ptr->volt_mod.uvw_idx_prev : &motor[0].ctrl_ptr->volt_mod.uvw_idx;
    
}

static inline void MCU_FlashInit(void)
{
    // EEPROM Emulator
    mcu[0].eeprom.config.eepromSize = srss_0_eeprom_0_SIZE,
    mcu[0].eeprom.config.simpleMode = srss_0_eeprom_0_SIMPLEMODE,
    mcu[0].eeprom.config.wearLevelingFactor = srss_0_eeprom_0_WEARLEVELING_FACTOR,
    mcu[0].eeprom.config.redundantCopy = srss_0_eeprom_0_REDUNDANT_COPY,
    mcu[0].eeprom.config.blockingWrite = srss_0_eeprom_0_BLOCKINGMODE,
    mcu[0].eeprom.config.userFlashStartAddr = (uint32_t)(Em_Eeprom_Storage[0]),
    mcu[0].eeprom.status = Cy_Em_EEPROM_Init(&mcu[0].eeprom.config, &mcu[0].eeprom.context);

    mcu[0].eeprom.init_done = true;
}

static void MCU_InitAnalogRouting(void)
{
    // Default indices (may be changed by routing configuration mux functions) ......
    mcu[0].adc_mux.idx_isamp[0] = ADC_ISAMPA; 
    mcu[0].adc_mux.idx_isamp[1] = ADC_ISAMPB;
    mcu[0].adc_mux.idx_isamp[2] = ADC_ISAMPC;

    // Routing configuration mux functions ..........................................
#if defined(ANALOG_ROUTING_MUX_RUNTIME)
    MCU_RoutingConfigMUX0Wrap = (motor[0].params_ptr->sys.analog.shunt.type == Single_Shunt) ? MCU_RoutingConfigMUXB0 : MCU_RoutingConfigMUXA0;
    MCU_RoutingConfigMUX1Wrap = (motor[0].params_ptr->sys.analog.shunt.type == Single_Shunt) ? MCU_RoutingConfigMUXB1 : MCU_RoutingConfigMUXA1;
    MCU_RoutingConfigMUX0Wrap();
#else
    MCU_RoutingConfigMUXWrap = (motor[0].params_ptr->sys.analog.shunt.type == Single_Shunt) ? MCU_RoutingConfigMUXB : MCU_RoutingConfigMUXA;
    MCU_RoutingConfigMUXWrap();
#if defined(ADC_SAMP_VU_ENABLED) && defined(ADC_SAMP_VV_ENABLED) && defined(ADC_SAMP_VW_ENABLED)
    mcu[0].adc_mux.en = true;
#else
    mcu[0].adc_mux.en = false;
#endif
#endif
}


static void MCU_InitDMAs(void)
{
    // Configure DMA descriptors ...............................................
    for (uint8_t seq_idx=0U; seq_idx<ADC_SEQ_MAX; ++seq_idx)
    {
        for (uint8_t samp_idx=0U; samp_idx<ADC_SAMP_PER_SEQ_MAX; ++samp_idx)
        {
            Cy_DMA_Descriptor_Init(DMA_Descriptors[seq_idx][samp_idx], DMA_Descriptor_Configs[seq_idx][samp_idx]);
            Cy_DMA_Descriptor_SetSrcAddress(DMA_Descriptors[seq_idx][samp_idx], ADC_Result_Regs[seq_idx][samp_idx]);
            Cy_DMA_Descriptor_SetDstAddress(DMA_Descriptors[seq_idx][samp_idx], &mcu[0].dma_results[DMA_Result_Indices[seq_idx][samp_idx]]);
        }
    }
    // Configure DMA channels ..................................................
    Cy_DMA_Channel_Init(DMA_ADC_0_HW, DMA_ADC_0_CHANNEL, &DMA_ADC_0_channelConfig);
    Cy_DMA_Channel_SetDescriptor(DMA_ADC_0_HW, DMA_ADC_0_CHANNEL, &DMA_ADC_0_Descriptor_0);
    Cy_DMA_Channel_SetInterruptMask(DMA_ADC_0_HW, DMA_ADC_0_CHANNEL, CY_DMA_INTR_MASK);
    Cy_DMA_Channel_Enable(DMA_ADC_0_HW, DMA_ADC_0_CHANNEL);

    Cy_DMA_Channel_Init(DMA_ADC_1_HW, DMA_ADC_1_CHANNEL, &DMA_ADC_1_channelConfig);
    Cy_DMA_Channel_SetDescriptor(DMA_ADC_1_HW, DMA_ADC_1_CHANNEL, &DMA_ADC_1_Descriptor_0);
    Cy_DMA_Channel_SetInterruptMask(DMA_ADC_1_HW, DMA_ADC_1_CHANNEL, CY_DMA_INTR_MASK);
    Cy_DMA_Channel_Enable(DMA_ADC_1_HW, DMA_ADC_1_CHANNEL);

}

static void MCU_InitPosInterface(void)
{
#if defined(POSIF_ENC_HW)

    Cy_TCPWM_MOTIF_Enable(POSIF_ENC_HW);
    Cy_TCPWM_MOTIF_Quaddec_Init(POSIF_ENC_HW, &POSIF_ENC_quaddec_config);
    Cy_TCPWM_MOTIF_Quaddec_Output_Config(POSIF_ENC_HW, &POSIF_ENC_quaddec_output_config);
#endif
}

static void MCU_InitWatchdog(void)
{
#if defined(CY_USING_HAL)
    // Start the watchdog now that we are running
    cyhal_wdt_init(&mcu[0].wdt_obj, motor[0].params_ptr->sys.faults.watchdog_time);
    cyhal_wdt_start(&mcu[0].wdt_obj);

#endif
}

static void MCU_InitInterrupts(void)
{
    // Interrupt callbacks and priorities (higher value = lower urgency) .......
    // DMA_ADC_0:
    cy_stc_sysint_t DMA_ADC_0_cfg = { .intrSrc = DMA_ADC_0_IRQ, .intrPriority = 0 };
    Cy_SysInt_Init(&DMA_ADC_0_cfg, DMA_ADC_0_RunISR);
    // DMA_ADC_1:
    cy_stc_sysint_t DMA_ADC_1_cfg = { .intrSrc = DMA_ADC_1_IRQ, .intrPriority = 0 };
    Cy_SysInt_Init(&DMA_ADC_1_cfg, DMA_ADC_1_RunISR);

    // ISR1:
    cy_stc_sysint_t ISR1_cfg = { .intrSrc = SYNC_ISR1_IRQ, .intrPriority = 2 };
    Cy_SysInt_Init(&ISR1_cfg, MCU_RunISR1);
    
    // NVIC connections ........................................................
    mcu[0].interrupt.nvic_dma_adc_0 = DMA_ADC_0_IRQ;
    mcu[0].interrupt.nvic_dma_adc_1 = DMA_ADC_1_IRQ;
    mcu[0].interrupt.nvic_sync_isr1 = SYNC_ISR1_IRQ;

}


RAMFUNC_BEGIN
/**
 * @brief Validates current sensor offset voltages and sets fault flags if out of range
 *
 * This function computes the actual voltage at the ADC input for each phase's current
 * sensor offset and compares it against configured min/max limits. The number of phases
 * checked depends on the shunt configuration (Single, Two, or Three shunt).
 *
 * The voltage calculation converts the stored current offset back to ADC ticks, applies
 * current sense polarity, and scales to actual voltage using ADC reference parameters.
 *
 * @param[in] motor_id Motor instance identifier (0 to MOTOR_CTRL_NO_OF_MOTOR-1)
 *
 * @note This function is placed in RAM for fast execution (RAMFUNC)
 * @note Phase checking logic:
 *       - Phase U: Always checked (all shunt types)
 *       - Phase V: Checked for Three_Shunt and Two_Shunt only
 *       - Phase W: Checked for Three_Shunt only
 *
 * @warning If ADC scale factor is invalid (near zero), all phases are flagged as faulty
 *
 * @post Updates motor[motor_id].vars_ptr->curr_off_fault with fault bits:
 *       - Bit 0 (0x01): Phase U fault
 *       - Bit 1 (0x02): Phase V fault
 *       - Bit 2 (0x04): Phase W fault
 *       - All bits (0x07): Invalid ADC scale
 */
static void MCU_ComputeOffsetVoltage(uint8_t motor_id)
{
    float polarity = (float)motor[motor_id].params_ptr->sys.analog.shunt.current_sense_polarity;
    float min_limit = motor[motor_id].params_ptr->sys.faults.offset_curr_min_limit;
    float max_limit = motor[motor_id].params_ptr->sys.faults.offset_curr_max_limit;

    SHUNT_TYPE_t shunt = motor[motor_id].params_ptr->sys.analog.shunt.type;

    // Initialize: no fault
    motor[motor_id].vars_ptr->curr_off_fault = 0;

    // Check for invalid ADC scale
    if (fabsf(mcu[motor_id].adc_scale.i_uvw) < EPSILON)
    {
        motor[motor_id].vars_ptr->curr_off_fault = CURR_OFF_ALL_FAULT_PHASE;  // All phases fault
        return;
    }

    // Phase U � always present regardless of shunt type
    float offset_ticks_u = motor[motor_id].sensor_iface_ptr->i_uvw_offset_null.u
                           / mcu[motor_id].adc_scale.i_uvw;
    float v_u = ((float)CURR_ADC_HALF_POINT_TICKS - offset_ticks_u * polarity) * ADC_VOLTAGE_PER_TICK_V;

    if ((v_u < min_limit) || (v_u > max_limit))
    {
        motor[motor_id].vars_ptr->curr_off_fault |= CURR_OFF_FAULT_PHASE_U;  // Bit 0: Phase U fault
    }

    // Phase V � Three_Shunt and Two_Shunt
    if (shunt == Three_Shunt || shunt == Two_Shunt)
    {
        float offset_ticks_v = motor[motor_id].sensor_iface_ptr->i_uvw_offset_null.v
                               / mcu[motor_id].adc_scale.i_uvw;
        float v_v = ((float)CURR_ADC_HALF_POINT_TICKS - offset_ticks_v * polarity) * ADC_VOLTAGE_PER_TICK_V;

        if ((v_v < min_limit) || (v_v > max_limit))
        {
            motor[motor_id].vars_ptr->curr_off_fault |= CURR_OFF_FAULT_PHASE_V;  // Bit 1: Phase V fault
        }
    }

    // Phase W � Three_Shunt only
    if (shunt == Three_Shunt)
    {
        float offset_ticks_w = motor[motor_id].sensor_iface_ptr->i_uvw_offset_null.w
                               / mcu[motor_id].adc_scale.i_uvw;
        float v_w = ((float)CURR_ADC_HALF_POINT_TICKS - offset_ticks_w * polarity) * ADC_VOLTAGE_PER_TICK_V;

        if ((v_w < min_limit) || (v_w > max_limit))
        {
            motor[motor_id].vars_ptr->curr_off_fault |= CURR_OFF_FAULT_PHASE_W;  // Bit 2: Phase W fault
        }
    }
}
RAMFUNC_END

/******************************************************************************/
RAMFUNC_BEGIN
void MCU_RunISR0(void)
{
    if((++mcu[0].isr0.count) % 2U != 0U) { return; }

    MCU_StartTimeCap(&mcu[0].isr0_exe);

#if defined(CTRL_METHOD_TBC)
    if(motor[0].ctrl_ptr->block_comm.enter_high_z_flag.u) { MCU_PhaseUEnterHighZ(); }
    if(motor[0].ctrl_ptr->block_comm.enter_high_z_flag.v) { MCU_PhaseVEnterHighZ(); }
    if(motor[0].ctrl_ptr->block_comm.enter_high_z_flag.w) { MCU_PhaseWEnterHighZ(); }

    if(motor[0].ctrl_ptr->block_comm.exit_high_z_flag.u) { MCU_PhaseUExitHighZ(); }
    if(motor[0].ctrl_ptr->block_comm.exit_high_z_flag.v) { MCU_PhaseVExitHighZ(); }
    if(motor[0].ctrl_ptr->block_comm.exit_high_z_flag.w) { MCU_PhaseWExitHighZ(); }
#endif

#if defined(CTRL_METHOD_RFO) || defined(CTRL_METHOD_TBC)

#if defined(POSIF_ENC_HW)
    if(motor[0].params_ptr->sys.fb.mode == AqB_Enc)
    {
        motor[0].inc_encoder_ptr->pos_cap = Cy_TCPWM_Counter_GetCounter(ENC_POS_CNTR_HW, ENC_POS_CNTR_NUM);
        motor[0].inc_encoder_ptr->per_cap = Cy_TCPWM_Counter_GetCapture(ENC_TIME_BTW_TICKS_HW, ENC_TIME_BTW_TICKS_NUM);
        motor[0].inc_encoder_ptr->dir_cap = Cy_TCPWM_MOTIF_Get_Quaddec_Rotation_Direction(POSIF_ENC_HW) ? +1.0f : -1.0f;
        //motor[0].faults_ptr->flags.sw.encoder = 0b0;
    }
#endif

#if defined(HALL_0_PORT) && defined(HALL_1_PORT) && defined(HALL_2_PORT)
    if(motor[0].params_ptr->sys.fb.mode == Hall)
    {
        motor[0].hall_ptr->signal.u = !Cy_GPIO_Read(HALL_0_PORT, HALL_0_NUM);
        motor[0].hall_ptr->signal.v = !Cy_GPIO_Read(HALL_1_PORT, HALL_1_NUM);
        motor[0].hall_ptr->signal.w = !Cy_GPIO_Read(HALL_2_PORT, HALL_2_NUM);

    // SW capture (w/o POSIF)
    static bool hall_cap_sig, hall_cap_sig_prev = false;
    static uint32_t hall_cap_val, hall_cap_val_prev = 0U;
    hall_cap_sig = motor[0].hall_ptr->signal.u ^ motor[0].hall_ptr->signal.v ^ motor[0].hall_ptr->signal.w; // 6 steps per revolution
    if(TRANS_EDGE(hall_cap_sig_prev, hall_cap_sig))
    {

        hall_cap_val = Cy_TCPWM_Counter_GetCounter(HALL_TIMER_HW, HALL_TIMER_NUM);
        motor[0].hall_ptr->per_cap = hall_cap_val - hall_cap_val_prev;
        hall_cap_val_prev = hall_cap_val;
    }
    hall_cap_sig_prev = hall_cap_sig;
    }
#endif  /*End of #if defined (HALL_0_PORT && HALL_1_PORT && HALL_2_PORT)*/
#endif  /*#if defined(CTRL_METHOD_RFO) || defined(CTRL_METHOD_TBC)*/

    motor[0].sensor_iface_ptr->i_samp_0.raw = mcu[0].adc_scale.i_uvw * (CURR_ADC_HALF_POINT_TICKS - (uint16_t)mcu[0].dma_results[mcu[0].adc_mux.idx_isamp[0]])*motor[0].params_ptr->sys.analog.shunt.current_sense_polarity;
    motor[0].sensor_iface_ptr->i_samp_1.raw = mcu[0].adc_scale.i_uvw * (CURR_ADC_HALF_POINT_TICKS - (uint16_t)mcu[0].dma_results[mcu[0].adc_mux.idx_isamp[1]])*motor[0].params_ptr->sys.analog.shunt.current_sense_polarity;
    motor[0].sensor_iface_ptr->i_samp_2.raw = mcu[0].adc_scale.i_uvw * (CURR_ADC_HALF_POINT_TICKS - (uint16_t)mcu[0].dma_results[mcu[0].adc_mux.idx_isamp[2]])*motor[0].params_ptr->sys.analog.shunt.current_sense_polarity;
#if defined(ANALOG_ROUTING_MUX_RUNTIME)
    if(mcu[0].adc_mux.en)
    {
        mcu[0].adc_mux.seq = (mcu[0].isr0.count >> 1) & 0x1;
        switch(mcu[0].adc_mux.seq)
        {
        case Analog_Routing_MUX_0:
        default:
            motor[0].sensor_iface_ptr->v_dc.raw = mcu[0].adc_scale.v_dc * (uint16_t)mcu[0].dma_results[ADC_VBUS];
            motor[0].sensor_iface_ptr->pot.raw = mcu[0].adc_scale.v_pot * (uint16_t)mcu[0].dma_results[ADC_VPOT];
            motor[0].sensor_iface_ptr->temp_ps.raw = MCU_TempSensorCalc();
            MCU_RoutingConfigMUX1Wrap();
            break;
        case Analog_Routing_MUX_1:
            motor[0].sensor_iface_ptr->v_uz.raw = mcu[0].adc_scale.v_uvw * (uint16_t)mcu[0].dma_results[ADC_VU];
            motor[0].sensor_iface_ptr->v_vz.raw = mcu[0].adc_scale.v_uvw * (uint16_t)mcu[0].dma_results[ADC_VV];
            motor[0].sensor_iface_ptr->v_wz.raw = mcu[0].adc_scale.v_uvw * (uint16_t)mcu[0].dma_results[ADC_VW];
            MCU_RoutingConfigMUX0Wrap();
            break;
        }
    }
    else
    {
        motor[0].sensor_iface_ptr->v_dc.raw = mcu[0].adc_scale.v_dc * (uint16_t)mcu[0].dma_results[ADC_VBUS];
        motor[0].sensor_iface_ptr->pot.raw = mcu[0].adc_scale.v_pot * (uint16_t)mcu[0].dma_results[ADC_VPOT];
        motor[0].sensor_iface_ptr->temp_ps.raw = MCU_TempSensorCalc();
    }
#else
#if !defined(MOTOR_CTRL_DISABLE_ADDON_FEATURES)
#if defined(ADC_SAMP_VU_ENABLED) && defined(ADC_SAMP_VV_ENABLED) && defined(ADC_SAMP_VW_ENABLED)
    if(MCU_ArePhaseVoltagesMeasured(0))
    {
      motor[0].sensor_iface_ptr->v_uz.raw = mcu[0].adc_scale.v_uvw * (uint16_t)mcu[0].dma_results[ADC_VU];
      motor[0].sensor_iface_ptr->v_vz.raw = mcu[0].adc_scale.v_uvw * (uint16_t)mcu[0].dma_results[ADC_VV];
      motor[0].sensor_iface_ptr->v_wz.raw = mcu[0].adc_scale.v_uvw * (uint16_t)mcu[0].dma_results[ADC_VW];
    }
#endif
#endif

    motor[0].sensor_iface_ptr->v_dc.raw = mcu[0].adc_scale.v_dc * (uint16_t)mcu[0].dma_results[ADC_VBUS];
#if defined(ADC_SAMP_VPOT_ENABLED)
    motor[0].sensor_iface_ptr->pot.raw = mcu[0].adc_scale.v_pot * (uint16_t)mcu[0].dma_results[ADC_VPOT];
#endif
#endif

    STATE_MACHINE_RunISR0(&motor[0]);

#if defined(PG_OUT_ENABLED)
    PgOut_Run();
#endif

    uint32_t pwm_u_cc0,pwm_u_cc1,pwm_v_cc0,pwm_v_cc1,pwm_w_cc0,pwm_w_cc1;
    
    UVW_t d_uvw_cmd_adj = PWM_INVERSION ? (UVW_t){.w=(1.0f - motor[0].vars_ptr->d_uvw_cmd.w), .v=(1.0f - motor[0].vars_ptr->d_uvw_cmd.v), .u=(1.0f - motor[0].vars_ptr->d_uvw_cmd.u)} :
                                          (UVW_t){.w=motor[0].vars_ptr->d_uvw_cmd.w, .v=motor[0].vars_ptr->d_uvw_cmd.v, .u=motor[0].vars_ptr->d_uvw_cmd.u};

     pwm_u_cc0 = (uint32_t)(mcu[0].pwm.duty_cycle_coeff * d_uvw_cmd_adj.u);
     pwm_v_cc0 = (uint32_t)(mcu[0].pwm.duty_cycle_coeff * d_uvw_cmd_adj.v);
     pwm_w_cc0 = (uint32_t)(mcu[0].pwm.duty_cycle_coeff * d_uvw_cmd_adj.w);
     
     if(pwm_u_cc0<1){pwm_u_cc0=1;}
     if(pwm_v_cc0<1){pwm_v_cc0=1;}
     if(pwm_w_cc0<1){pwm_w_cc0=1;}
    
    d_uvw_cmd_adj = PWM_INVERSION ? (UVW_t){.w=(1.0f - motor[0].vars_ptr->d_uvw_cmd_fall.w), .v=(1.0f - motor[0].vars_ptr->d_uvw_cmd_fall.v), .u=(1.0f - motor[0].vars_ptr->d_uvw_cmd_fall.u)} :
                                          (UVW_t){.w=motor[0].vars_ptr->d_uvw_cmd_fall.w, .v=motor[0].vars_ptr->d_uvw_cmd_fall.v, .u=motor[0].vars_ptr->d_uvw_cmd_fall.u};

     pwm_u_cc1 = (uint32_t)((mcu[0].pwm.period) - (mcu[0].pwm.duty_cycle_coeff * d_uvw_cmd_adj.u));
     pwm_v_cc1 = (uint32_t)((mcu[0].pwm.period) - (mcu[0].pwm.duty_cycle_coeff * d_uvw_cmd_adj.v));
     pwm_w_cc1 = (uint32_t)((mcu[0].pwm.period) - (mcu[0].pwm.duty_cycle_coeff * d_uvw_cmd_adj.w));
  
    Cy_TCPWM_PWM_SetCompare0BufVal(PWM_U_HW, PWM_U_NUM, pwm_u_cc0);
    Cy_TCPWM_PWM_SetCompare1BufVal(PWM_U_HW, PWM_U_NUM, pwm_u_cc1);
    Cy_TCPWM_PWM_SetCompare0BufVal(PWM_V_HW, PWM_V_NUM, pwm_v_cc0);
    Cy_TCPWM_PWM_SetCompare1BufVal(PWM_V_HW, PWM_V_NUM, pwm_v_cc1);
    Cy_TCPWM_PWM_SetCompare0BufVal(PWM_W_HW, PWM_W_NUM, pwm_w_cc0);
    Cy_TCPWM_PWM_SetCompare1BufVal(PWM_W_HW, PWM_W_NUM, pwm_w_cc1);

    uint32_t adc_isr0_cc_samp0, adc_isr0_cc_samp1;
    if(motor[0].params_ptr->sys.analog.shunt.type == Single_Shunt)
    {
        adc_isr0_cc_samp0 = (uint32_t)(mcu[0].pwm.duty_cycle_coeff * motor[0].vars_ptr->d_samp[0]);
        adc_isr0_cc_samp1 = (uint32_t)(mcu[0].pwm.duty_cycle_coeff * motor[0].vars_ptr->d_samp[1]);

        Cy_TCPWM_PWM_SetCompare0BufVal(ADC0_ISR0_HW, ADC0_ISR0_NUM, adc_isr0_cc_samp0);
        Cy_TCPWM_PWM_SetCompare1BufVal(ADC1_ISR0_HW, ADC1_ISR0_NUM, adc_isr0_cc_samp1);

    }
    else /*Three_Shunt or Two_Shunt*/
    {
        adc_isr0_cc_samp0 = (uint32_t)(mcu[0].pwm.duty_cycle_coeff * (1.0f+ motor[0].params_ptr->sys.analog.shunt.leg_shunt.adc_d_sample_delay ));
        Cy_TCPWM_PWM_SetCompare0BufVal(ADC0_ISR0_HW, ADC0_ISR0_NUM, adc_isr0_cc_samp0);
        Cy_TCPWM_PWM_SetCompare1BufVal(ADC1_ISR0_HW, ADC1_ISR0_NUM, adc_isr0_cc_samp0);
    }
#if MTB_OSCILLOSCOPE_MAX_CH > 0 /*if scope is enabled*/
    mtb_oscilloscope_sampling();
#endif

#if defined (EXE_TIMER_ENABLED)
    MCU_StopTimeCap(&mcu[0].isr0_exe);
#endif  

}
RAMFUNC_END


void MCU_RunISR1(void)
{
    Cy_TCPWM_ClearInterrupt(SYNC_ISR1_HW, SYNC_ISR1_NUM, SYNC_ISR1_config.interruptSources);
    NVIC_ClearPendingIRQ(mcu[0].interrupt.nvic_sync_isr1);
#if defined (EXE_TIMER_ENABLED)
    MCU_StartTimeCap(&mcu[0].isr1_exe);
#endif  
    if(mcu[0].isr1.count++ == 1U) {MCU_DisableTimerReload();}

#if defined (N_FAULT_HW_PORT)
    motor[0].sensor_iface_ptr->digital.fault = !Cy_GPIO_Read(N_FAULT_HW_PORT, N_FAULT_HW_NUM);
    // Detect kill via TCPWM: counter not running indicates kill event
    if (!(Cy_TCPWM_PWM_GetStatus(PWM_U_HW, PWM_U_NUM) & CY_TCPWM_PWM_STATUS_COUNTER_RUNNING))
    {
        motor[0].sensor_iface_ptr->digital.fault = true;
    }
    motor[0].faults_ptr->flags.hw.cs_ocp = motor[0].sensor_iface_ptr->digital.fault ? 0b111 : 0b000; // hw faults only cover over-current without SGD
#endif

    // Direction switch
#if defined(DIR_SWITCH_PORT) // switch
    motor[0].sensor_iface_ptr->digital.dir = Cy_GPIO_Read(DIR_SWITCH_PORT, DIR_SWITCH_NUM);
#elif defined(N_DIR_PUSHBTN_PORT) // push button
    static bool user_btn_prev, user_btn = true;
    user_btn_prev = user_btn;
    user_btn = Cy_GPIO_Read(N_DIR_PUSHBTN_PORT, N_DIR_PUSHBTN_NUM);
    motor[0].sensor_iface_ptr->digital.dir = FALL_EDGE(user_btn_prev, user_btn) ? ~motor[0].sensor_iface_ptr->digital.dir : motor[0].sensor_iface_ptr->digital.dir; // toggle switch
#endif

    // Direction LED
#if defined(DIR_LED_PORT)
    Cy_GPIO_Write(DIR_LED_PORT, DIR_LED_NUM, (motor[0].vars_ptr->dir == +1.0f));
#endif

    // Brake switch
#if defined(N_BRK_SWITCH_PORT)
    motor[0].sensor_iface_ptr->digital.brk = !Cy_GPIO_Read(N_BRK_SWITCH_PORT, N_BRK_SWITCH_NUM);
#else
    motor[0].sensor_iface_ptr->digital.brk = 0x0; // no brake switch
#endif

#if defined(ADC_SAMP_TEMP_ENABLED)
    motor[0].sensor_iface_ptr->temp_ps.raw = MCU_TempSensorCalc();
#endif

    // Voltage reading for offset protection fault
    if (true == motor[0].vars_ptr->curr_off_check_enable)
    {
        MCU_ComputeOffsetVoltage(0);
    }

    // Control ISR1
    STATE_MACHINE_RunISR1(&motor[0]);

#if defined(PG_OUT_ENABLED)
    PgOut_UpdateState();
#endif

    // SW fault LED
#if defined(N_FAULT_LED_SW_PORT) // seperate leds for hw and sw faults
    Cy_GPIO_Write(N_FAULT_LED_SW_PORT, N_FAULT_LED_SW_NUM, (bool)(!motor[0].faults_ptr->flags_latched.sw.reg));
#elif defined(FAULT_LED_ALL_PORT) // one led for all faults
    Cy_GPIO_Write(FAULT_LED_ALL_PORT, FAULT_LED_ALL_NUM, (bool)(motor[0].faults_ptr->flags_latched.all));
#endif
#if (DRIVE_ENABLE_CONTROL_POT)
    DriveEnableControlforPot(&motor[0]);  // Update drive enable/disable based on potentiometer command (Motor 0)
#endif
#if defined (EXE_TIMER_ENABLED)
    MCU_StopTimeCap(&mcu[0].isr1_exe);
    //Process the execution time calculation
    MCU_ProcessTimeCapISR1(&mcu[0].isr0_exe);
    MCU_ProcessTimeCapISR1(&mcu[0].isr1_exe);
#endif
    
    // Watchdog kick
#if defined(CY_USING_HAL)
    cyhal_wdt_kick(&mcu[0].wdt_obj);
#endif
#if (CPU_LOAD_CALC_ENABLED)
    StopWatchRun(&mcu[0].cpu_calc.timer);  /* Advance CPU load measurement timer (tick every ts1) */
#endif
}


RAMFUNC_BEGIN
static void DMA_ADC_0_RunISR(void) {
    Cy_DMA_Channel_ClearInterrupt(DMA_ADC_0_HW, DMA_ADC_0_CHANNEL);
    NVIC_ClearPendingIRQ(mcu[0].interrupt.nvic_dma_adc_0);
    Cy_DMA_Channel_SetDescriptor(DMA_ADC_0_HW, DMA_ADC_0_CHANNEL, &DMA_ADC_0_Descriptor_0);
    MCU_RunISR0();
}
RAMFUNC_END

RAMFUNC_BEGIN
static void DMA_ADC_1_RunISR() {
    Cy_DMA_Channel_ClearInterrupt(DMA_ADC_1_HW, DMA_ADC_1_CHANNEL);
    NVIC_ClearPendingIRQ(mcu[0].interrupt.nvic_dma_adc_1);
    Cy_DMA_Channel_SetDescriptor(DMA_ADC_1_HW, DMA_ADC_1_CHANNEL, &DMA_ADC_1_Descriptor_0);
    MCU_RunISR0();
}
RAMFUNC_END

/******************************************************************************/
void MCU_Init(uint8_t motor_id)
{
    MCU_InitChipInfo();
    MCU_InitInterrupts();
    MCU_InitADCs();
    MCU_InitAnalogRouting();
    MCU_InitDMAs();
    MCU_InitTimers();
    MCU_InitPosInterface();
    MCU_InitWatchdog();
#if MTB_OSCILLOSCOPE_MAX_CH > 0 /*if scope is enabled*/
    static bool oscilloscope_initialized = false;
    if (!oscilloscope_initialized)
    {
        mtb_oscilloscope_init((uint32_t)motor[0].params_ptr->sys.samp.fs0);
        oscilloscope_initialized = true;
    }
#endif
    motor[motor_id].sensor_iface_ptr->digital.dir = true; // initial direction is positive

#if defined(PG_OUT_ENABLED)
    PgOut_Init();
#endif
}

RAMFUNC_BEGIN
void MCU_EnterCriticalSection(void)
{
    mcu[0].interrupt.state = Cy_SysLib_EnterCriticalSection();
}
RAMFUNC_END

RAMFUNC_BEGIN
void MCU_ExitCriticalSection(void)
{
    Cy_SysLib_ExitCriticalSection(mcu[0].interrupt.state);
}
RAMFUNC_END

RAMFUNC_BEGIN
static void MCU_RestartKilledPWMs(void)
{
    // Restart PWM counters if stopped by kill (counter not running)
    if (!(Cy_TCPWM_PWM_GetStatus(PWM_U_HW, PWM_U_NUM) & CY_TCPWM_PWM_STATUS_COUNTER_RUNNING))
    {
        Cy_TCPWM_PWM_Disable(PWM_U_HW, PWM_U_NUM);
        Cy_TCPWM_PWM_Disable(PWM_V_HW, PWM_V_NUM);
        Cy_TCPWM_PWM_Disable(PWM_W_HW, PWM_W_NUM);
        Cy_TCPWM_PWM_Enable(PWM_U_HW, PWM_U_NUM);
        Cy_TCPWM_PWM_Enable(PWM_V_HW, PWM_V_NUM);
        Cy_TCPWM_PWM_Enable(PWM_W_HW, PWM_W_NUM);
        // Re-enable reload trigger so next SYNC_ISR1 overflow starts all PWMs in sync
        mcu[0].isr1.count = 0U;
        MCU_EnableTimerReload();
    }
}
RAMFUNC_END

RAMFUNC_BEGIN
void MCU_GateDriverEnterHighZ(uint8_t motor_id)
{
    uint8_t phase_control = (motor_id>>5)&0x7;  //Special handling to control  driver per phase, Bit 7 - Phase W, Bit 6 - Phase V, Bit 5 - Phase U
    uint8_t motor_inst    =  motor_id &0x1F;

    MCU_RestartKilledPWMs();

    if ((phase_control == 0) ||(phase_control == 0x7) ) //Control all the 3 phases 
    {
        if(motor_inst == MOTOR_CTRL_ID_MOTOR0)  /*Motor 0*/
        {
#if defined(ANALOG_ROUTING_MUX_RUNTIME)
            mcu[motor_id].adc_mux.en = true;
#endif
            MCU_PhaseUEnterHighZ();
            MCU_PhaseVEnterHighZ();
            MCU_PhaseWEnterHighZ();
        }
    }
    else 
    {
        if(motor_inst == MOTOR_CTRL_ID_MOTOR0)  /*Motor 0*/
        {
            if(phase_control&0x1){MCU_PhaseUEnterHighZ(); }  //Phase U control enabled
            if(phase_control&0x2){MCU_PhaseVEnterHighZ(); }  //Phase V control enabled
            if(phase_control&0x4){MCU_PhaseWEnterHighZ(); }  //Phase W control enabled
        }           
    }
}
RAMFUNC_END

RAMFUNC_BEGIN
void MCU_GateDriverExitHighZ(uint8_t motor_id)
{
    uint8_t phase_control = (motor_id>>5)&0x7;  //Special handling to control gate driver per phase separately, Bit 7 - Phase W, Bit 6 - Phase V, Bit 5 - Phase U
    uint8_t motor_inst    =  motor_id &0x1F;

    MCU_RestartKilledPWMs();

    if ((phase_control == 0) ||(phase_control == 0x7) ) //Control all the 3 phases 
    {
        if(motor_inst == MOTOR_CTRL_ID_MOTOR0)  /*Motor 0*/
        {
#if defined(ANALOG_ROUTING_MUX_RUNTIME)
            mcu[motor_id].adc_mux.en = false;
            MCU_RoutingConfigMUX0Wrap();
#endif
            MCU_PhaseUExitHighZ();
            MCU_PhaseVExitHighZ();
            MCU_PhaseWExitHighZ();
        }
    }
    else 
    {
        if(motor_inst == MOTOR_CTRL_ID_MOTOR0)  /*Motor 0*/
        {
            if(phase_control&0x1){MCU_PhaseUExitHighZ(); }  //Phase U control enabled
            if(phase_control&0x2){MCU_PhaseVExitHighZ(); }  //Phase V control enabled
            if(phase_control&0x4){MCU_PhaseWExitHighZ(); }  //Phase W control enabled
        }            
    }    
}
RAMFUNC_END

RAMFUNC_BEGIN
bool MCU_FlashRead(uint8_t motor_id, PARAMS_ID_t id,PARAMS_t* ram_data )
{
    if(!mcu[motor_id].eeprom.init_done)
    {
        MCU_FlashInit();
    }

    if (CY_EM_EEPROM_SUCCESS != mcu[motor_id].eeprom.status)
    {
        return false;
    }

    mcu[motor_id].eeprom.status = Cy_Em_EEPROM_Read((sizeof(PARAMS_t)), ram_data, sizeof(PARAMS_t), &mcu[motor_id].eeprom.context);
    if (CY_EM_EEPROM_SUCCESS != mcu[motor_id].eeprom.status)
    {
        return false;
    }

    if (ram_data->id.code != id.code || ram_data->id.build_config != id.build_config || ram_data->id.ver != id.ver)
    {
        return false;
    }
    return true;
}
RAMFUNC_END

RAMFUNC_BEGIN
bool MCU_FlashWrite(uint8_t motor_id,PARAMS_t* ram_data)
{
  if(!mcu[motor_id].eeprom.init_done)
  {
    MCU_FlashInit();
  }

  if (CY_EM_EEPROM_SUCCESS != mcu[motor_id].eeprom.status)
  {
    return false;
  }

  mcu[motor_id].eeprom.status = Cy_Em_EEPROM_Write((sizeof(PARAMS_t)), ram_data, sizeof(PARAMS_t), &mcu[motor_id].eeprom.context);
  if (CY_EM_EEPROM_SUCCESS != mcu[motor_id].eeprom.status)
  {
    return false;
  }

  return true;
}
RAMFUNC_END

RAMFUNC_BEGIN
void MCU_StartPeripherals(uint8_t motor_id)
{
    MCU_EnterCriticalSection(); // No ISRs beyond this point

    NVIC_EnableIRQ(mcu[motor_id].interrupt.nvic_dma_adc_0);
    Cy_DMA_Enable(DMA_ADC_0_HW);
    NVIC_EnableIRQ(mcu[motor_id].interrupt.nvic_dma_adc_1);
    Cy_DMA_Enable(DMA_ADC_1_HW);
    NVIC_EnableIRQ(mcu[motor_id].interrupt.nvic_sync_isr1);

#if defined(POSIF_ENC_HW)
    Cy_TCPWM_Counter_Enable(ENC_POS_CNTR_HW, ENC_POS_CNTR_NUM);
    Cy_TCPWM_TriggerStart_Single(ENC_POS_CNTR_HW, ENC_POS_CNTR_NUM);
    Cy_TCPWM_Counter_Enable(ENC_TIME_BTW_TICKS_HW, ENC_TIME_BTW_TICKS_NUM);
    Cy_TCPWM_MOTIF_Start(POSIF_ENC_HW); 
#endif
    
    mcu[motor_id].isr1.count = 0U;
    mcu[motor_id].isr0.count = 0U;
    
    Cy_TCPWM_PWM_Enable(ADC0_ISR0_HW, ADC0_ISR0_NUM);
    Cy_TCPWM_PWM_Enable(ADC1_ISR0_HW, ADC1_ISR0_NUM);    
    Cy_TCPWM_PWM_Enable(PWM_SYNC_HW, PWM_SYNC_NUM);
    Cy_TCPWM_PWM_Enable(PWM_U_HW, PWM_U_NUM);
    Cy_TCPWM_PWM_Enable(PWM_V_HW, PWM_V_NUM);
    Cy_TCPWM_PWM_Enable(PWM_W_HW, PWM_W_NUM);   
    Cy_TCPWM_PWM_Enable(SYNC_ISR1_HW, SYNC_ISR1_NUM);
    MCU_EnableTimerReload();
    Cy_TCPWM_TriggerStart_Single(SYNC_ISR1_HW, SYNC_ISR1_NUM); // Start ISR1 which will also start U,V,W
#if defined(CTRL_METHOD_RFO) || defined(CTRL_METHOD_TBC)
    Cy_TCPWM_Counter_Enable(HALL_TIMER_HW, HALL_TIMER_NUM);
    Cy_TCPWM_TriggerStart_Single(HALL_TIMER_HW, HALL_TIMER_NUM); 
#endif
    Cy_TCPWM_Counter_Enable(EXE_TIMER_HW, EXE_TIMER_NUM);
    Cy_TCPWM_TriggerStart_Single(EXE_TIMER_HW, EXE_TIMER_NUM);   
  
#if defined(N_HALL_EN_PORT) && (defined(CTRL_METHOD_RFO) || defined(CTRL_METHOD_TBC))
    if (motor[motor_id].params_ptr->sys.fb.mode == Hall)
    { 
        Cy_GPIO_Clr(N_HALL_EN_PORT, N_HALL_EN_PIN);
    }
#endif

#if defined(ENC_EN_PORT) && (defined(CTRL_METHOD_RFO) || defined(CTRL_METHOD_TBC))
    if (motor[motor_id].params_ptr->sys.fb.mode == AqB_Enc)
    { 
        Cy_GPIO_Set(ENC_EN_PORT, ENC_EN_PIN);
    }
#endif
    
    MCU_ExitCriticalSection();
}
RAMFUNC_END

RAMFUNC_BEGIN
void MCU_StopPeripherals(uint8_t motor_id)
{
    MCU_EnterCriticalSection(); // No ISRs beyond this point

#if defined(ENC_EN_PORT) && (defined(CTRL_METHOD_RFO) || defined(CTRL_METHOD_TBC))
    if (motor[motor_id].params_ptr->sys.fb.mode == AqB_Enc)
    { 
        Cy_GPIO_Clr(ENC_EN_PORT, ENC_EN_PIN);
    }
#endif
    
#if defined(N_HALL_EN_PORT) && (defined(CTRL_METHOD_RFO) || defined(CTRL_METHOD_TBC))
    if (motor[motor_id].params_ptr->sys.fb.mode == Hall)
    { 
        Cy_GPIO_Set(N_HALL_EN_PORT, N_HALL_EN_PIN);
    }
#endif

    Cy_TCPWM_Counter_Disable(EXE_TIMER_HW, EXE_TIMER_NUM);
#if defined(CTRL_METHOD_RFO) || defined(CTRL_METHOD_TBC)
    Cy_TCPWM_Counter_Disable(HALL_TIMER_HW, HALL_TIMER_NUM);
#endif
    Cy_TCPWM_PWM_Disable(SYNC_ISR1_HW, SYNC_ISR1_NUM);
    Cy_TCPWM_PWM_Disable(PWM_W_HW, PWM_W_NUM);
    Cy_TCPWM_PWM_Disable(PWM_V_HW, PWM_V_NUM);
    Cy_TCPWM_PWM_Disable(PWM_U_HW, PWM_U_NUM);
    Cy_TCPWM_PWM_Disable(PWM_SYNC_HW, PWM_SYNC_NUM);
    Cy_TCPWM_PWM_Disable(ADC1_ISR0_HW, ADC1_ISR0_NUM); 
    Cy_TCPWM_PWM_Disable(ADC0_ISR0_HW, ADC0_ISR0_NUM);

#if defined(POSIF_ENC_HW)
    Cy_TCPWM_MOTIF_Stop(POSIF_ENC_HW); 
    Cy_TCPWM_Counter_Disable(ENC_TIME_BTW_TICKS_HW, ENC_TIME_BTW_TICKS_NUM);
    Cy_TCPWM_Counter_Disable(ENC_POS_CNTR_HW, ENC_POS_CNTR_NUM);
#endif

    NVIC_DisableIRQ(mcu[motor_id].interrupt.nvic_sync_isr1);
#if defined(DMA_ADC_2_HW)
    Cy_DMA_Disable(DMA_ADC_2_HW);
    NVIC_DisableIRQ(mcu[motor_id].interrupt.nvic_dma_adc_2);
#endif 
    Cy_DMA_Disable(DMA_ADC_1_HW);
    NVIC_DisableIRQ(mcu[motor_id].interrupt.nvic_dma_adc_1);
    Cy_DMA_Disable(DMA_ADC_0_HW);
    NVIC_DisableIRQ(mcu[motor_id].interrupt.nvic_dma_adc_0);

    MCU_ExitCriticalSection();
}
RAMFUNC_END

RAMFUNC_BEGIN
bool MCU_ArePhaseVoltagesMeasured(uint8_t motor_id)
{
  return mcu[motor_id].adc_mux.en;
}
RAMFUNC_END

#if defined (EXE_TIMER_ENABLED)
/**
 * @brief  Initializes the CPU load calculation module.
 *
 * Sets up the StopWatch timer used to periodically trigger CPU load
 * measurements. Must be called after STATE_MACHINE_Init() since it
 * requires motor[0].params_ptr->sys.samp.ts1 to be valid.
 * The timer is incremented each ISR1 tick via StopWatchRun() in MCU_RunISR1().
 */
 
 /**
 * @brief  Calculates and updates the CPU load periodically.
 *
 * Called from the main loop. Each time the StopWatch timer expires
 * (every CPU_LOAD_CALC_EXERATE seconds), a 100ms idle measurement is
 * performed using Cy_SysLib_Delay(). ISRs preempt this busy-wait loop,
 * causing it to take longer than the pure idle time. The extra elapsed
 * time relative to the idle baseline (CPU_LOAD_CALC_IDLE_TASKTIME +
 * CPU_LOAD_CALC_IDLE_TASKOVERHEAD) represents ISR execution time.
 *
 * cpu_load = (end_time - idle_baseline) / end_time
 *
 * Results are stored in mcu[0].cpu_calc.cpu_load [0.0 = 0%, 1.0 = 100%].
 * If end_time exceeds CPU_LOAD_CALC_TIMEOUT, cpu_load is set to 1.0 (100%).
 */
void MCU_CPULoadCalc(void)
{
  TIMER_t *timer_ptr = &mcu[0].cpu_calc.timer;

    StopWatchInit(timer_ptr, CPU_LOAD_CALC_TIMEOUT, motor[0].params_ptr->sys.samp.ts1); 
    StopWatchReset(timer_ptr);

    Cy_SysLib_Delay((uint32_t)(CPU_LOAD_CALC_IDLE_TASKTIME * 1000U));
    float end_time = StopWatchGetTime(timer_ptr);

    if(StopWatchIsDone(timer_ptr))
    {   mcu[0].cpu_calc.cpu_load = 1.0f; } 
    else if(end_time <= 0.0f)
    {   mcu[0].cpu_calc.cpu_load = 0.0f; } /* guard: no ticks counted yet */
    else
    {
        mcu[0].cpu_calc.cpu_load = (end_time - (CPU_LOAD_CALC_IDLE_TASKTIME + CPU_LOAD_CALC_IDLE_TASKOVERHEAD)) / end_time;
        /* Clamp to [0.0, 1.0] - tick resolution can cause small negative values */
        if(mcu[0].cpu_calc.cpu_load < 0.0f) { mcu[0].cpu_calc.cpu_load = 0.0f; }
        if(mcu[0].cpu_calc.cpu_load > 1.0f) { mcu[0].cpu_calc.cpu_load = 1.0f; }
    }
}
RAMFUNC_BEGIN
void MCU_StartTimeCap(MCU_TIME_CAP_t* time_cap)
{
    time_cap->start = (int32_t)(Cy_TCPWM_Counter_GetCounter(EXE_TIMER_HW, EXE_TIMER_NUM));
}
RAMFUNC_END

RAMFUNC_BEGIN
void MCU_StopTimeCap(MCU_TIME_CAP_t* time_cap)
{
    // Over flow and roll-over is OK as long as int32_t is used for 32bit timer/counters
    time_cap->stop = (int32_t)(Cy_TCPWM_Counter_GetCounter(EXE_TIMER_HW, EXE_TIMER_NUM));
    time_cap->duration_ticks = time_cap->stop - time_cap->start;
}
RAMFUNC_END

RAMFUNC_BEGIN
void MCU_ProcessTimeCapISR1(MCU_TIME_CAP_t* time_cap)
{
    time_cap->duration_sec = ((float)(time_cap->duration_ticks)) * (time_cap->sec_per_tick);
    time_cap->util = time_cap->duration_sec * time_cap->inv_max_time;
}
RAMFUNC_END
#endif

void MCU_DisableTimerReload(void)
{
    TCPWM_GRP_CNT_TR_OUT_SEL(SYNC_ISR1_HW, TCPWM_GRP_CNT_GET_GRP(SYNC_ISR1_NUM), SYNC_ISR1_NUM) =
      (_VAL2FLD(TCPWM_GRP_CNT_V2_TR_OUT_SEL_OUT0, CY_TCPWM_CNT_TRIGGER_ON_DISABLED) |
       _VAL2FLD(TCPWM_GRP_CNT_V2_TR_OUT_SEL_OUT1, CY_TCPWM_CNT_TRIGGER_ON_DISABLED));
}

void MCU_EnableTimerReload(void)
{
    TCPWM_GRP_CNT_TR_OUT_SEL(SYNC_ISR1_HW, TCPWM_GRP_CNT_GET_GRP(SYNC_ISR1_NUM), SYNC_ISR1_NUM) =
      (_VAL2FLD(TCPWM_GRP_CNT_V2_TR_OUT_SEL_OUT0, CY_TCPWM_CNT_TRIGGER_ON_DISABLED) |
       _VAL2FLD(TCPWM_GRP_CNT_V2_TR_OUT_SEL_OUT1, CY_TCPWM_CNT_TRIGGER_ON_OVERFLOW));
}
