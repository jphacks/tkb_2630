### KIT_PSC3M5_CC2 BSP

### PSOC™ Control C3M5 Motor Control Card - This board has the PSOC™ Control C3M5 device with debugger and 100 pin HD connector.     Equipped with an Arm® Cortex®-M33 based PSOC™ Control microcontroller from Infineon Technologies AG,     the PSoC™ Control C3M5 Motor Control Card is designed to use with different power boards to demonstrate the capabilities of     Infineon's PSOC™ Control C3M5 Microcontroller for motor control applications.     
**Note:**
Programming this kit requires installing 
[SEGGER J-Link software](https://www.segger.com/downloads/jlink/#J-LinkSoftwareAndDocumentationPack)

NOTE: BSPs are versioned by family. This means that version 1.2.0 of any BSP in a family (eg: PSoC™ 6) will have the same software maturity level. However, not all updates are necessarily applicable for each BSP in the family so not all version numbers will exist for each board. Additionally, new BSPs may not start at version 1.0.0. In the event of adding a common feature across all BSPs, the libraries are assigned the same version number. For example if BSP_A is at v1.3.0 and BSP_B is at v1.2.0, the event will trigger a version update to v1.4.0 for both BSP_A and BSP_B. This allows the common feature to be tracked in a consistent way.

### What's Included?
The KIT_PSC3M5_CC2 library includes the following:
* BSP specific makefile to configure the build process for the board
* cybsp.c/h files to initialize the board and any system peripherals
* cybsp_types.h file describing basic board setup
* Linker script & startup code for GCC, IAR, and ARM toolchains
* Configurator design files (and generated code) to setup board specific peripherals
* .lib file references for all dependent libraries
* API documentation

### What Changed?
#### v3.3.0
* Update BSP template to v1.12.0
#### v3.2.0
* Update BSP template to v1.11.0
#### v3.1.0
* Added the BSP for REF_80VDC_3_5_KW_OPE2 
#### v3.0.1
* Update BSP template to v1.7.6
#### v3.0.0
* B1 MPN update from CYW20829B0LKML to CYW20829B1010 and CYW89829B0232 to CYW89829B1232
#### v2.0.2
* DSRAM and secure silicon fix release
#### v2.0.1
* Revert support from MTB3.2 to MTB3.0 and above
#### v2.0.0
* BTFW as separate bsp dependent asset and MTB3.2 support
#### v1.0.4
* MPN CYW89829B01MKSBG to CYW89829B0232 update release for CYW89829
#### v1.0.3
* Initial production release for CYW89829
#### v1.0.2
* Initial production release for CYW20829
#### v1.0.1
* MTB 3.0 and pre-production release
#### v0.5.0
* Initial pre-production release

### Supported Software and Tools
This version of the KIT_PSC3M5_CC2 BSP was validated for compatibility with the following Software and Tools:

| Software and Tools                        | Version |
| :---                                      | :----:  |
| ModusToolbox™ Software Environment        | 3.8.0   |
| GCC Compiler                              | 14.2.1  |
| IAR Compiler                              | 9.50.2  |
| ARM Compiler                              | 6.22    |

Minimum required ModusToolbox™ Software Environment: v3.0.0

### More information
* [KIT_PSC3M5_CC2 BSP API Reference Manual][api]
* [KIT_PSC3M5_CC2 Documentation](http://www.infineon.com/KIT_PSC3M5_CC2)
* [Infineon Technologies AG](http://www.infineon.com)
* [Infineon GitHub](https://github.com/infineon)
* [ModusToolbox™](https://www.infineon.com/design-resources/development-tools/sdk/modustoolbox-software)

[api]: https://infineon.github.io/TARGET_KIT_PSC3M5_CC2/html/modules.html

---
© Infineon Technologies AG or an affiliate of Infineon Technologies AG, 2019-2026.