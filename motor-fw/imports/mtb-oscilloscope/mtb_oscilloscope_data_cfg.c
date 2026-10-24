/*******************************************************************************
* File Name:   mtb_oscilloscope_data_cfg.c
*
* Description: Application-specific configuration of the oscilloscope variable
*              list. Customize the contents of the mtb_oscilloscope_data_list[]
*              array below to expose the variables monitored by the
*              oscilloscope. The mtb_oscilloscope_data_registry is built from
*              this list at compile time and consumed by the oscilloscope
*              data module.
*
********************************************************************************
* \copyright
* (c) 2026, Infineon Technologies AG or an affiliate of
* Infineon Technologies AG.
*
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

/*******************************************************************************
*       INCLUDES
*******************************************************************************/
#include "mtb_oscilloscope_data.h"
#include "mtb_oscilloscope.h"


/*******************************************************************************
*       EXPORTED VARIABLES
*******************************************************************************/
// Application-specific list of variables to be exposed via the oscilloscope.
// Replace the placeholder entry with the variables of your application.
/* coverity[misra_c_2012_rule_5_9_violation] Caused by testing tools implementation specific, not a code defect */
static const SCOPEDATA_VAR_t mtb_oscilloscope_data_list[] = {
    {"Var_tmpl   ", (uint8_t*)0, MTB_OSCILLOSCOPE_FP32},
};


// Variable registry consumed by the oscilloscope data module.
// Initialized at compile time from the mtb_oscilloscope_data_list[] above.
const SCOPEDATA_t mtb_oscilloscope_data_registry = {
    .var_count = sizeof(mtb_oscilloscope_data_list) / sizeof(SCOPEDATA_VAR_t),
    .var_list  = mtb_oscilloscope_data_list
};
