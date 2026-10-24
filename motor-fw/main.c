/******************************************************************************
* File Name:   main.c
*
* Description: This code example demonstrates the implementation of PMSM sensorless
* field-oriented control (FOC) using the Infineon's MCUs.
*
* Related Document: See README.md
*
*******************************************************************************
* (c) 2024-2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "HardwareIface.h"
#include "cybsp.h"
#include "Controller.h"
#include "MotorCtrlHWConfig.h"
#include "door.h"

/*******************************************************************************
* Global variable
********************************************************************************/
/* XMC7x/T2G - GCC_ARM: EEPROM storage */
#if defined(APP_KIT_XMC7200_DC_V1) || defined(APP_KIT_TRAVEO_T2G_B_H_MC1)
uint8_t Em_Eeprom_Storage[srss_0_eeprom_0_PHYSICAL_SIZE] __attribute__ ((section(".cy_em_eeprom")));
#endif

/*******************************************************************************
* Function prototype
********************************************************************************/
#if defined(APP_KIT_PSC3M5_2GO)
void Motor_Control_POT_Control(void);
#endif

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    
    #if defined(APP_KIT_XMC7200_DC_V1) || defined(APP_KIT_TRAVEO_T2G_B_H_MC1) // Disabled the D-CACHE for XMC7200/T2G-B-H device.
    SCB_DisableDCache();
    #endif
    result = cybsp_init();                 /* Initialize the device and board peripherals */
    CY_ASSERT(result == CY_RSLT_SUCCESS);  /* Board init failed. Stop program execution   */

    // Initialize controller
    HW_IFACE_ConnectFcnPointers();         /* must be called before STATE_MACHINE_Init()  */
    STATE_MACHINE_Init();

#if defined(CTRL_METHOD_RFO)
    // Disable the drive for motors configured in position control mode.
    // MOTOR_CTRL_SetDriveEnable must be called after STATE_MACHINE_Init.
    for (uint8_t i = 0U; i < MOTOR_CTRL_NO_OF_MOTOR; i++)
    {
        if (params[i].ctrl.mode == Position_Mode_FOC_Encoder_Align_Startup)
        {
            MOTOR_CTRL_SetDriveEnable(&motor[i], false);
        }
    }
#endif

    DOOR_Init();

    // Enable global interrupts
    __enable_irq();

    (void) (result);
    for (;;)
    {
        DOOR_Run();

#if defined(APP_KIT_PSC3M5_2GO)
        Motor_Control_POT_Control();
#endif

#if (CPU_LOAD_CALC_ENABLED)
        MCU_CPULoadCalc();
#endif
    }
}

#if defined(APP_KIT_PSC3M5_2GO)
/*******************************************************************************
* Function Name: Motor_Control_POT_Control
********************************************************************************
* Summary: This function controls the pot and enable the gate drive.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void Motor_Control_POT_Control(void)
{
    static bool flag;
    static float t_min_configured;
    static bool state_check=false;
   /*Disable the driver when pot value is less than or equal to  5% */
    if(motor[0].params_ptr->sys.cmd.source == Internal)
    {
          if((motor[0].sensor_iface_ptr->pot.raw >= 0.05f)) /* flag is added to allow GUI control of driver ON/OFF*/
        {
              if(flag == true)
              {
                flag = false;
                motor[0].vars_ptr->en = true;
              }

        }
          else if(motor[0].sensor_iface_ptr->pot.raw <= 0.025f )
        {
              flag = true;
              motor[0].vars_ptr->en = false;

        }

    }
    else
    {
        flag = false;
    }
    /*Enable or disable Gate driver*/
    Cy_GPIO_Write(EN_IPM_PORT, EN_IPM_NUM, motor[0].vars_ptr->en);
    /* Reduce the minimum time for current measurement in profile mode*/
    if((Prof_Rot_Lock <= motor[0].sm_ptr->current) && (motor[0].sm_ptr->current <= Prof_Lq)&&(motor[0].params_ptr->ctrl.mode == Profiler_Mode))
    {
       if (state_check == false)
       {
          MCU_EnterCriticalSection();
          t_min_configured = motor[0].params_ptr->sys.analog.shunt.hyb_mod.adc_t_min;
          motor[0].params_ptr->sys.analog.shunt.hyb_mod.adc_t_min =  (t_min_configured <=3.0f)? 0.0f:t_min_configured-3.0f;
          motor[0].params_ptr->sys.analog.shunt.hyb_mod.adc_d_min = 2.0f * motor[0].params_ptr->sys.analog.shunt.hyb_mod.adc_t_min * motor[0].params_ptr->sys.samp.fpwm; // [%]
          MCU_ExitCriticalSection();
          state_check =true;
       }
    }
    else
    {
      if(state_check ==true)
      {
        MCU_EnterCriticalSection();
        motor[0].params_ptr->sys.analog.shunt.hyb_mod.adc_t_min =t_min_configured;
        motor[0].params_ptr->sys.analog.shunt.hyb_mod.adc_d_min = 2.0f * motor[0].params_ptr->sys.analog.shunt.hyb_mod.adc_t_min * motor[0].params_ptr->sys.samp.fpwm; // [%]
        MCU_ExitCriticalSection();
      }
      state_check =false;
    }
}
#endif
