# Motor control demo

This code example demonstrates the sensorless and sensored solutions using Infineon MCUs. Code example includes the following solutions.
- Sensorless PMSM FOC with 3-shunt
- Sensorless PMSM FOC with 1-shunt
- Hall sensor-based PMSM FOC
- Encoder-based PMSM FOC (supported for PSOC Control C3 MCU)
- Hall sensor-based Trapezoidal Block Commutation (TBC)


[View this README on GitHub.](https://github.com/Infineon/mtb-example-ce240614-motor-control-solutions)

[Provide feedback on this code example.](https://yourvoice.infineon.com/jfe/form/SV_1NTns53sK2yiljn?Q_EED=eyJVbmlxdWUgRG9jIElkIjoiQ0UyNDA2MTQiLCJTcGVjIE51bWJlciI6IjAwMi00MDYxNCIsIkRvYyBUaXRsZSI6Ik1vdG9yIGNvbnRyb2wgZGVtbyIsInJpZCI6ImFiaGlzaGVrLnByYXNoYW50QGluZmluZW9uLmNvbSIsIkRvYyB2ZXJzaW9uIjoiMi4yLjAiLCJEb2MgTGFuZ3VhZ2UiOiJFbmdsaXNoIiwiRG9jIERpdmlzaW9uIjoiTUNEIiwiRG9jIEJVIjoiSUNXIiwiRG9jIEZhbWlseSI6IlBTT0MifQ==)


## Requirements

- [ModusToolbox&trade;](https://www.infineon.com/modustoolbox) v3.8 or later
- ModusToolbox&trade; Motor Suite GUI: 2.10.0 or later
- Board support package (BSP) minimum required version:
  - KIT_XMC7200_DC_V1: 2.2.0
  - KIT_PSC3M5_CC2: 1.0.3
  - KIT_PSC3M5_2GO: 2.0.1
  - KIT_TRAVEO_T2G_B_H_MC1: 1.0.0
- Programming language: C
- Associated parts: [XMC7200D-F176K8384](https://www.infineon.com/cms/en/product/microcontroller/32-bit-industrial-microcontroller-based-on-arm-cortex-m/32-bit-xmc7000-industrial-microcontroller-arm-cortex-m7/xmc7200d-f176k8384aa/) [All PSOC&trade; Control C3 MCUs](https://www.infineon.com/cms/en/product/microcontroller/32-bit-psoc-arm-cortex-microcontroller/32-bit-psoc-control-arm-cortex-m33-mcu/) [TRAVEO&trade; T2G CYT4BF Series](https://www.infineon.com/cms/en/product/microcontroller/32-bit-traveo-t2g-arm-cortex-microcontroller/32-bit-traveo-t2g-arm-cortex-for-body/traveo-t2g-cyt4bf-series/)

## Supported toolchains (make variable 'TOOLCHAIN')

- GNU Arm&reg; Embedded Compiler v14.2.1 (`GCC_ARM`) – Default value of `TOOLCHAIN`
- IAR C/C++ Compiler v9.50.2 (`IAR`)


## Supported kits (make variable 'TARGET')

- [KIT_XMC7200_DC_V1 Motor Control Card](https://www.infineon.com/evaluation-board/KIT-XMC7200-DC-V1) (`KIT_XMC7200_DC_V1`) – Default value of `TARGET`
- [KIT_PSC3M5_CC2 Motor Control Card](https://www.infineon.com/evaluation-board/KIT-PSC3M5-CC2) (`KIT_PSC3M5_CC2`)
- [KIT_PSC3M5_2GO Compact Motor Control Kit](https://www.infineon.com/evaluation-board/KIT-PSC3M5-2GO) (`KIT_PSC3M5_2GO`)
- [KIT-TRAVEO-T2G-B-H-MC1 Motor Control Kit](https://www.infineon.com/evaluation-board/KIT-TRAVEO-T2G-B-H-MC1) (`KIT-TRAVEO-T2G-B-H-MC1`)

## Hardware setup

Refer to the respective kit quick start guide for the hardware setup information.

- [KIT_XMC7200_MC1 XMC7200 Complete System Motor Control Kit](https://www.infineon.com/evaluation-board/KIT-XMC7200-MC1) (`KIT_XMC7200_MC1`)<br>
  > **Note:** To enable single-shunt FOC on the KIT_XMC7200_MC1 with the new adaptor card, connect pins P8.3 and P10.5 with a jumper wire.
- [KIT_PSC3M5_MC1 PSOC&trade; Control C3M5 Complete System Motor Control Kit](https://www.infineon.com/evaluation-board/KIT-PSC3M5-MC1) (`KIT_PSC3M5_MC1`)
- [KIT_PSC3M5_2GO Compact Motor Control Kit](https://www.infineon.com/evaluation-board/KIT-PSC3M5-2GO) (`KIT_PSC3M5_2GO`)
- [KIT-TRAVEO-T2G-B-H-MC1 TRAVEO&trade; T2G CYT4BF Complete System Motor Control Kit](https://www.infineon.com/evaluation-board/KIT-TRAVEO-T2G-B-H-MC1) (`KIT-TRAVEO-T2G-B-H-MC1`)

## Software setup

See the [ModusToolbox&trade; tools package installation guide](https://www.infineon.com/ModusToolboxInstallguide) for information about installing and configuring the tools package.

This example requires ModusToolbox&trade; Motor Suite GUI to evaluate the GUI features. It can be installed from the [Infineon Developer Center](https://www.infineon.com/idc)


## Using the code example


### Create the project

The ModusToolbox&trade; tools package provides the Project Creator as both a GUI tool and a command line tool.

<details><summary><b>Use Project Creator GUI</b></summary>

1. Open the Project Creator GUI tool.

   There are several ways to do this, including launching it from the dashboard or from inside the Eclipse IDE. For more details, see the [Project Creator user guide](https://www.infineon.com/ModusToolboxProjectCreator) (locally available at *{ModusToolbox&trade; install directory}/tools_{version}/project-creator/docs/project-creator.pdf*).

2. On the **Choose Board Support Package (BSP)** page, select a kit supported by this code example. See [Supported kits](#supported-kits-make-variable-target).

   > **Note:** To use this code example for a kit not listed here, you may need to update the source files. If the kit does not have the required resources, the application may not work.

3. On the **Select Application** page:

   a. Select the **Applications(s) Root Path** and the **Target IDE**.

   > **Note:** Depending on how you open the Project Creator tool, these fields may be pre-selected for you.

   b. Select this code example from the list by enabling its check box.

   > **Note:** You can narrow the list of displayed examples by typing in the filter box.

   c. (Optional) Change the suggested **New Application Name** and **New BSP Name**.

   d. Click **Create** to complete the application creation process.

</details>


<details><summary><b>Use Project Creator CLI</b></summary>

The 'project-creator-cli' tool can be used to create applications from a CLI terminal or from within batch files or shell scripts. This tool is available in the *{ModusToolbox&trade; install directory}/tools_{version}/project-creator/* directory.

Use a CLI terminal to invoke the 'project-creator-cli' tool. On Windows, use the command-line 'modus-shell' program provided in the ModusToolbox&trade; installation instead of a standard Windows command-line application. This shell provides access to all ModusToolbox&trade; tools. You can access it by typing "modus-shell" in the search box in the Windows menu. In Linux and macOS, you can use any terminal application.

The following example clones the "[Motor control demo](https://github.com/Infineon/mtb-example-ce240614-motor-control-solutions)" application with the desired name "FOCMotorDemo" configured for the *KIT_XMC7200_DC_V1* BSP into the specified working directory, *C:/mtb_projects*:

   ```
   project-creator-cli --board-id KIT_XMC7200_DC_V1 --app-id mtb-example-motor-control-solutions --user-app-name FOCMotorDemo --target-dir "C:/mtb_projects"
   ```


The 'project-creator-cli' tool has the following arguments:

Argument | Description | Required/optional
---------|-------------|-----------
`--board-id` | Defined in the <id> field of the [BSP](https://github.com/Infineon?q=bsp-manifest&type=&language=&sort=) manifest | Required
`--app-id`   | Defined in the <id> field of the [CE](https://github.com/Infineon?q=ce-manifest&type=&language=&sort=) manifest | Required
`--target-dir`| Specify the directory in which the application is to be created if you prefer not to use the default current working directory | Optional
`--user-app-name`| Specify the name of the application if you prefer to have a name other than the example's default name | Optional



> **Note:** The project-creator-cli tool uses the `git clone` and `make getlibs` commands to fetch the repository and import the required libraries. For details, see the "Project creator tools" section of the [ModusToolbox&trade; tools package user guide](https://www.infineon.com/ModusToolboxUserGuide) (locally available at {ModusToolbox&trade; install directory}/docs_{version}/mtb_user_guide.pdf).

</details>


### Open the project

After the project has been created, you can open it in your preferred development environment.


<details><summary><b>Eclipse IDE</b></summary>

If you opened the Project Creator tool from the included Eclipse IDE, the project will open in Eclipse automatically.

For more details, see the [Eclipse IDE for ModusToolbox&trade; user guide](https://www.infineon.com/MTBEclipseIDEUserGuide) (locally available at *{ModusToolbox&trade; install directory}/docs_{version}/mt_ide_user_guide.pdf*).

</details>


<details><summary><b>Visual Studio (VS) Code</b></summary>

Launch VS Code manually, and then open the generated *{project-name}.code-workspace* file located in the project directory.

For more details, see the [Visual Studio Code for ModusToolbox&trade; user guide](https://www.infineon.com/MTBVSCodeUserGuide) (locally available at *{ModusToolbox&trade; install directory}/docs_{version}/mt_vscode_user_guide.pdf*).

</details>


<details><summary><b>Arm&reg; Keil&reg; µVision&reg;</b></summary>

Double-click the generated *{project-name}.cprj* file to launch the Keil&reg; µVision&reg; IDE.

For more details, see the [Arm&reg; Keil&reg; µVision&reg; for ModusToolbox&trade; user guide](https://www.infineon.com/MTBuVisionUserGuide) (locally available at *{ModusToolbox&trade; install directory}/docs_{version}/mt_uvision_user_guide.pdf*).

</details>


<details><summary><b>IAR Embedded Workbench</b></summary>

Open IAR Embedded Workbench manually, and create a new project. Then select the generated *{project-name}.ipcf* file located in the project directory.

For more details, see the [IAR Embedded Workbench for ModusToolbox&trade; user guide](https://www.infineon.com/MTBIARUserGuide) (locally available at *{ModusToolbox&trade; install directory}/docs_{version}/mt_iar_user_guide.pdf*).

</details>


<details><summary><b>Command line</b></summary>

If you prefer to use the CLI, open the appropriate terminal, and navigate to the project directory. On Windows, use the command-line 'modus-shell' program; on Linux and macOS, you can use any terminal application. From there, you can run various `make` commands.

For more details, see the [ModusToolbox&trade; tools package user guide](https://www.infineon.com/ModusToolboxUserGuide) (locally available at *{ModusToolbox&trade; install directory}/docs_{version}/mtb_user_guide.pdf*).

</details>


## Operation

1. Connect the kit as per [Hardware setup](#hardware-setup) section.

2. Program the board using one of the following:

   <details><summary><b>Using Eclipse IDE</b></summary>

      1. Select the application project in the Project Explorer.
	
      2. In the **Quick Panel**, scroll down, and click **\<Application Name> Program (JLink)**.
		
   </details>
   
   <details><summary><b>In other IDEs</b></summary>
   
   Follow the instructions in your preferred IDE.
	
   </details>
   
   <details><summary><b>Using CLI</b></summary>

     From the terminal, execute the `make program` command to build and program the application using the default toolchain to the default target. The default toolchain is specified in the application's Makefile but you can override this value manually:
      ```
      make program TOOLCHAIN=<toolchain>
      ```

      Example:
      ```
      make program TOOLCHAIN=GCC_ARM
      ```
   </details>

7. After programming, the application starts automatically.

8. Rotate the potentiometer (R6) to control the motor speed.

   - Set the motor speed to zero and press the user button. It will change the motor direction.

   - The user LED1 (yellow) shows the motor direction.


## ModusToolbox&trade; Motor Suite

**Launch the GUI**

To launch the GUI, double-click on **ModusToolbox&trade; Motor Suite** in the **Quick Panel** of ModusToolbox&trade; IDE. 
   
**Figure 2. Launch ModusToolbox&trade; Motor Suite**

![](images/launch-motor-suite.png)

**GUI - Getting started**

1. Select the **XMC7200/PSOC&trade; Control C3/TRAVEO&trade; T2G CYT4BF** kit with **RFO** from the dropdown menu. 
2. Select **New Project**, it will create a new GUI project for the selected device.

**Figure 3. Getting started**

![](images/open-new-project.png)
   
**GUI - Configurator**

1. In the GUI configurator, verify the establishment of J-Link connection in the right bottom corner indicated by the green LED.
2. Flash the *hex* and *elf* file by selecting the **Flash Firmware** option.
3. Select the **Test Bench** icon on the left panel to open the **Test Bench** window.

**Figure 4. GUI - Configurator**

![](images/gui-configurator-view.png)

Note that based on the selected build configuration, the configurator view shows the appropriate block diagrams and parameters.

By selecting each of the parameters in the parameter controls section, the corresponding block diagram for that specific parameter is shown to set that parameter.

To change the firmware parameters, stop the motor first from the test bench view.

There is a toggle switch on the right upper corner of the configurator view window to choose between seeing only basic parameters or advanced parameters.

The GUI can also invoke the firmware to auto-calculate the advanced parameters. See GUI's **Help** for more information.

**GUI - Test Bench**

Test Bench provides the option to control and monitor the motor parameters. Ensure the following:

- **Driver button:** Enable/disable the drive

- **Potentiometer button:**

   - Switch on for speed control of the motor by potentiometer (hardware).

   - Switch off for speed control of the motor by using the "Target Set" slider in the GUI (software).

**Figure 5. GUI - Test Bench**

![](images/gui-test-bench-view.png)
   
   
**GUI - Oscilloscope**

ModusToolbox&trade; Motor Suite supports a high-speed oscilloscope to monitor any firmware variable. There are four channels available to monitor four variables at a time. <br>
In the oscilloscope window, configure the **Divider** value and select **AutoScale** to get the optimum resolution.

**Figure 6. GUI - Oscilloscope**

![](images/oscilloscope.png)
   
   
## Debugging

You can debug the example to step through the code.


<details><summary><b>In Eclipse IDE</b></summary>

Use the **\<Application Name> Debug (JLink)** configuration in the **Quick Panel**. For details, see the "Program and debug" section in the [Eclipse IDE for ModusToolbox&trade; user guide](https://www.infineon.com/MTBEclipseIDEUserGuide).


</details>


<details><summary><b>In other IDEs</b></summary>

Follow the instructions in your preferred IDE.

</details>


## Design and implementation

This code example is created using the XMC7200D/TRAVEO&trade; T2G CYT4BF MCU resources and Infineon motor control library (middleware asset).

There are three major control types that are supported, namely:
- Rotor Field Oriented (RFO) control
- Stator Field Oriented (SFO) control
- Trapezoidal Block Commutation (TBC) control

These control types are selectable as build configurations in ModusToolbox&trade;, IAR, and Visual Studio.

By choosing a specific build configuration, only the code pertaining to that build configuration is compiled and included.
There are also common code blocks among all build configurations that are always included.

After selecting the control type through build configurations, choose the *controlled entity*, *feedback type*, and *startup method* by assigning the corresponding parameters either in the code before compilation or at runtime through the GUI:

**Figure 7. Control methods**

![](images/control-methods.png)

There are 23 different permutations of control type, control entity, feedback type, and startup methods that are supported as shown in **Figure 8**.
Additionally, both three-shunt and single-shunt configurations are supported, which result in more flexibility in supporting various applications.
Note that you can either include or bypass the current loop when using *TBC in TC* mode. Bypassing the current loop can address low-cost BLDC applications with no shunts or ADCs.


## Related resources

Resources  | Links
-----------|----------------------------------
Application notes  | [AN234334 - Getting started with XMC7200 MCU on ModusToolbox&trade;](https://www.infineon.com/dgdl/Infineon-Getting_started_with_XMC7000_MCU_on_ModusToolbox_software-ApplicationNotes-v06_00-EN.pdf?fileId=8ac78c8c850f4bee0185a53e84147437) <br> [AN240575 - PMSM FOC using XMC7200 MCU](https://www.infineon.com/dgdl/Infineon-AN240575_PMSM_FOC_using_XMC7200_MCU-ApplicationNotes-v01_00-EN.pdf?fileId=8ac78c8c92bcf0b001930d9d205504e1) <br> [AN238329 - Getting started with PSOC Control C3 MCU on ModusToolbox&trade; software](https://www.infineon.com/dgdl/Infineon-AN238329_Getting_started_PSOC_Control_C3_ModusToolbox-ApplicationNotes-v02_00-EN.pdf?fileId=8ac78c8c92bcf0b0019393f072d813b5) <br> [AN239646 - PMSM FOC using PSOC Control C3 MCU](https://www.infineon.com/dgdl/Infineon-AN239646_PMSM_FOC_PSOC_Control_C3_MCU-ApplicationNotes-v02_00-EN.pdf?fileId=8ac78c8c93956f5001939d61c8af4b9a) <br> [AN235305 - Getting started with TRAVEO&trade; T2G family MCUs in ModusToolbox&trade;](https://www.infineon.com/assets/row/public/documents/10/42/infineon-an235305-getting-started-with-traveo-t2g-family-mcus-in-modustoolbox-applicationnotes-en.pdf?fileId=8ac78c8c8b6555fe018c1fddd8a72801)  <br> [AN241827 - PMSM FOC using TRAVEO&trade; T2G CYT4BF MCU](https://www.infineon.com/assets/row/public/documents/10/42/infineon-an241827-pmsm-foc-using-traveo-tmt2g-cyt4bf-mcu-applicationnotes-en.pdf)
Code examples  | [Using ModusToolbox&trade;](https://github.com/Infineon/Code-Examples-for-ModusToolbox-Software) on GitHub
Device documentation | [XMC7200 MCU datasheets](https://www.infineon.com/sec/login?ret=https%3A%2F%2Fwww.infineon.com%2Fcms%2Fen%2Fproduct%2Fmicrocontroller%2F32-bit-industrial-microcontroller-based-on-arm-cortex-m%2F32-bit-xmc7000-industrial-microcontroller-arm-cortex-m7%2F%23!documents%2Fdocument-group-myInfineon-49) <br> [XMC7200 MCU reference manuals](https://www.infineon.com/sec/login?ret=https%3A%2F%2Fwww.infineon.com%2Fcms%2Fen%2Fproduct%2Fmicrocontroller%2F32-bit-industrial-microcontroller-based-on-arm-cortex-m%2F32-bit-xmc7000-industrial-microcontroller-arm-cortex-m7%2F%23!documents%2Fdocument-group-myInfineon-44) <br> [PSOC&trade; Control C3 MCU datasheets](https://www.infineon.com/sec/login?ret=https%3A%2F%2Fwww.infineon.com%2Fcms%2Fen%2Fproduct%2Fmicrocontroller%2F32-bit-psoc-arm-cortex-microcontroller%2F32-bit-psoc-control-arm-cortex-m33-mcu%2F%23!documents%2Fdocument-group-myInfineon-49) <br> [PSOC&trade; Control C3 MCU reference manuals](https://www.infineon.com/sec/login?ret=https%3A%2F%2Fwww.infineon.com%2Fcms%2Fen%2Fproduct%2Fmicrocontroller%2F32-bit-psoc-arm-cortex-microcontroller%2F32-bit-psoc-control-arm-cortex-m33-mcu%2F%23!documents%2Fdocument-group-myInfineon-57) <br> [TRAVEO&trade; T2G CYT4BF series datasheets](https://www.infineon.com/assets/row/public/documents/10/49/infineon-traveo-t2g-automotive-microcontroller-cyt4bf-32-bit-arm-cortex-m7-datasheet-en.pdf?fileId=5546d46275b79adb0175dc8387f93228) <br> [TRAVEO&trade; T2G MCU Body family architecture and registers reference manuals](https://www.infineon.com/cms/en/product/microcontroller/32-bit-traveo-t2g-arm-cortex-microcontroller/32-bit-traveo-t2g-arm-cortex-for-body/#!documents)
Development kits | Select your kits from the [Evaluation board finder](https://www.infineon.com/cms/en/design-support/finder-selection-tools/product-finder/evaluation-board).
Libraries on GitHub  | [mtb-pdl-cat1](https://github.com/Infineon/mtb-pdl-cat1) – Peripheral Driver Library (PDL) <br> [mtb-hal-cat1](https://github.com/Infineon/mtb-hal-cat1) – Hardware Abstraction Layer (HAL) library (XMC7200/TRAVEO&trade; T2G CYT4BF only) <br> [retarget-io](https://github.com/Infineon/retarget-io) – Utility library to retarget STDIO messages to a UART port
Tools  | [ModusToolbox&trade;](https://www.infineon.com/modustoolbox) – ModusToolbox&trade; software is a collection of easy-to-use libraries and tools enabling rapid development with Infineon MCUs for applications ranging from wireless and cloud-connected systems, edge AI/ML, embedded sense and control, to wired USB connectivity using PSOC&trade; Industrial/IoT MCUs, AIROC&trade; Wi-Fi and Bluetooth&reg; connectivity devices, XMC&trade; Industrial MCUs, and EZ-USB&trade;/EZ-PD&trade; wired connectivity controllers. ModusToolbox&trade; incorporates a comprehensive set of BSPs, HAL, libraries, configuration tools, and provides support for industry-standard IDEs to fast-track your embedded application development.

<br>


## Other resources

Infineon provides a wealth of data at [www.infineon.com](https://www.infineon.com) to help you select the right device, and quickly and effectively integrate it into your design.


## Document history

Document title: *CE240614* – *Motor control demo*

 Version | Description of change
 ------- | ---------------------
 1.0.0   | New code example supported XMC7200D MCU
 1.0.1   | Added support to PSOC&trade; Control C3 MCU
 1.0.2   | Added support to PSOC&trade; Control C3 Compact Kit
 1.0.3   | Added support to TRAVEO&trade; T2G CYT4BF MCU
 2.0.0   | Updated as per motor control library 3.1.0
 2.1.0   | Updated as per motor control library 3.2.0
 2.2.0   | Supported features of motor library 3.3.0 and mtb-oscilloscope
<br>


All referenced product or service names and trademarks are the property of their respective owners.

The Bluetooth&reg; word mark and logos are registered trademarks owned by Bluetooth SIG, Inc., and any use of such marks by Infineon is under license.


---------------------------------------------------------

© Cypress Semiconductor Corporation, 2024. This document is the property of Cypress Semiconductor Corporation, an Infineon Technologies company, and its affiliates ("Cypress").  This document, including any software or firmware included or referenced in this document ("Software"), is owned by Cypress under the intellectual property laws and treaties of the United States and other countries worldwide.  Cypress reserves all rights under such laws and treaties and does not, except as specifically stated in this paragraph, grant any license under its patents, copyrights, trademarks, or other intellectual property rights.  If the Software is not accompanied by a license agreement and you do not otherwise have a written agreement with Cypress governing the use of the Software, then Cypress hereby grants you a personal, non-exclusive, nontransferable license (without the right to sublicense) (1) under its copyright rights in the Software (a) for Software provided in source code form, to modify and reproduce the Software solely for use with Cypress hardware products, only internally within your organization, and (b) to distribute the Software in binary code form externally to end users (either directly or indirectly through resellers and distributors), solely for use on Cypress hardware product units, and (2) under those claims of Cypress's patents that are infringed by the Software (as provided by Cypress, unmodified) to make, use, distribute, and import the Software solely for use with Cypress hardware products.  Any other use, reproduction, modification, translation, or compilation of the Software is prohibited.
<br>
TO THE EXTENT PERMITTED BY APPLICABLE LAW, CYPRESS MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS DOCUMENT OR ANY SOFTWARE OR ACCOMPANYING HARDWARE, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  No computing device can be absolutely secure.  Therefore, despite security measures implemented in Cypress hardware or software products, Cypress shall have no liability arising out of any security breach, such as unauthorized access to or use of a Cypress product. CYPRESS DOES NOT REPRESENT, WARRANT, OR GUARANTEE THAT CYPRESS PRODUCTS, OR SYSTEMS CREATED USING CYPRESS PRODUCTS, WILL BE FREE FROM CORRUPTION, ATTACK, VIRUSES, INTERFERENCE, HACKING, DATA LOSS OR THEFT, OR OTHER SECURITY INTRUSION (collectively, "Security Breach").  Cypress disclaims any liability relating to any Security Breach, and you shall and hereby do release Cypress from any claim, damage, or other liability arising from any Security Breach.  In addition, the products described in these materials may contain design defects or errors known as errata which may cause the product to deviate from published specifications. To the extent permitted by applicable law, Cypress reserves the right to make changes to this document without further notice. Cypress does not assume any liability arising out of the application or use of any product or circuit described in this document. Any information provided in this document, including any sample design information or programming code, is provided only for reference purposes.  It is the responsibility of the user of this document to properly design, program, and test the functionality and safety of any application made of this information and any resulting product.  "High-Risk Device" means any device or system whose failure could cause personal injury, death, or property damage.  Examples of High-Risk Devices are weapons, nuclear installations, surgical implants, and other medical devices.  "Critical Component" means any component of a High-Risk Device whose failure to perform can be reasonably expected to cause, directly or indirectly, the failure of the High-Risk Device, or to affect its safety or effectiveness.  Cypress is not liable, in whole or in part, and you shall and hereby do release Cypress from any claim, damage, or other liability arising from any use of a Cypress product as a Critical Component in a High-Risk Device. You shall indemnify and hold Cypress, including its affiliates, and its directors, officers, employees, agents, distributors, and assigns harmless from and against all claims, costs, damages, and expenses, arising out of any claim, including claims for product liability, personal injury or death, or property damage arising from any use of a Cypress product as a Critical Component in a High-Risk Device. Cypress products are not intended or authorized for use as a Critical Component in any High-Risk Device except to the limited extent that (i) Cypress's published data sheet for the product explicitly states Cypress has qualified the product for use in a specific High-Risk Device, or (ii) Cypress has given you advance written authorization to use the product as a Critical Component in the specific High-Risk Device and you have signed a separate indemnification agreement.
<br>
Cypress, the Cypress logo, and combinations thereof, ModusToolbox, PSOC, CAPSENSE, EZ-USB, F-RAM, and TRAVEO are trademarks or registered trademarks of Cypress or a subsidiary of Cypress in the United States or in other countries. For a more complete list of Cypress trademarks, visit www.infineon.com. Other names and brands may be claimed as property of their respective owners.
