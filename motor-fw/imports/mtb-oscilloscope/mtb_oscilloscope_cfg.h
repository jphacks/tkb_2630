/*
*********************************************************************************************************
*                                       uC/Probe Communication
*
*                    Copyright 2007-2020 Silicon Laboratories Inc. www.silabs.com
*
*                                 SPDX-License-Identifier: APACHE-2.0
*
*               This software is subject to an open source license and is distributed by
*                Silicon Laboratories Inc. pursuant to the terms of the Apache License,
*                    Version 2.0 available at www.apache.org/licenses/LICENSE-2.0.
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            Oscilloscope
*
* File    : MTB_OSCILLOSCOPE_CFG.H
* Version : V2.30
*********************************************************************************************************
*/


#ifndef MTB_OSCILLOSCOPE_CFG_H
#define MTB_OSCILLOSCOPE_CFG_H

/*
*********************************************************************************************************
*                                            CONFIGURATION
*********************************************************************************************************
*/

#define  MTB_OSCILLOSCOPE_MAX_CH                       (4U)    /* The maximum number of channels: [1,8].                      */
#define  MTB_OSCILLOSCOPE_MAX_SAMPLES               (1000U)    /* The maximum number of samples per channel.                  */
#define  MTB_OSCILLOSCOPE_16_BIT_EN                    (1U)    /* The maximum size of each sample is 16-bits: [0,1].          */
#define  MTB_OSCILLOSCOPE_32_BIT_EN                    (1U)    /* The maximum size of each sample is 32-bits: [0,1].          */
#define  MTB_OSCILLOSCOPE_SAMPLING_CLK_HZ_DFLT      (1000U)    /* Default freq (Hz) to configure the timer at init.           */

#endif /* MTB_OSCILLOSCOPE_CFG_H */
