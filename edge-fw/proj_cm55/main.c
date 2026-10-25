/******************************************************************************
* File Name:   main.c
*
* Description: This is the main file for mtb-example-psoc-edge-ml-deepcraft-deploy-radar
* Code Example.
*
* Related Document: See README.md
*
*******************************************************************************
* (c) 2025-2026, Infineon Technologies AG, or an affiliate of Infineon
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

#include <stdlib.h>
#include "cybsp.h"
#include "retarget_io_init.h"
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include "mtb_hal.h"
#include "cybsp.h"
#include "retarget_io_init.h"
#include "xensiv_bgt60trxx_mtb.h"
#include "xensiv_bgt60trxx_platform.h"
#include "radar_settings.h"
#include "cy_scb_spi.h"
#include "model.h"

/*******************************************************************************
* Macros
********************************************************************************/
/* SPI controller */
#ifdef USE_KIT_PSE84_HMI
#define RADAR_SPI_CONTROLLER_HW     CYBSP_SPI_RADAR_CONTROLLER_HW
#define RADAR_SPI_CONTROLLER_IRQ    CYBSP_SPI_RADAR_CONTROLLER_IRQ
#define RADAR_SPI_CONTROLLER_config CYBSP_SPI_RADAR_CONTROLLER_config
#define RADAR_SPI_SLAVE_SELECT      CY_SCB_SPI_SLAVE_SELECT0
#else
#define RADAR_SPI_CONTROLLER_HW     CYBSP_SPI_CONTROLLER_HW
#define RADAR_SPI_CONTROLLER_IRQ    CYBSP_SPI_CONTROLLER_IRQ
#define RADAR_SPI_CONTROLLER_config CYBSP_SPI_CONTROLLER_config
#define RADAR_SPI_SLAVE_SELECT      CY_SCB_SPI_SLAVE_SELECT1
#endif

/* These sizes and masks must be aligned. */
#define DEFAULT_FIFO_SETTING                            6144

#define RING_BUFFER_MASK                                0x0000FFFF
#define RING_BUFFER_MASK32                              0x00007FFF /* Mask for 32-bits */
#define XENSIV_BGT60TRXX_SPI_BURST_MODE_CMD             (0xFF000000UL)  /* Write addr 7f<<1 | 0x01 */
#define XENSIV_BGT60TRXX_SPI_BURST_MODE_SADR_POS        (17U)
#define XENSIV_BGT60TRXX_SPI_BURST_MODE_RWB_POS         (16U)
#define XENSIV_BGT60TRXX_SPI_BURST_MODE_LEN_POS         (9U)
#define RING_BUFFER_SIZE                                0x00010000 /* 64k samples */

#define XENSIV_BGT60TRXX_IRQ_PRIORITY                   (1U) 
#define SPI_INTR_NUM                                    ((IRQn_Type) RADAR_SPI_CONTROLLER_IRQ)
#define SPI_INTR_PRIORITY                               (2U)


/*******************************************************************************
* Ringbuffer array
*******************************************************************************/
uint16_t bgt60_tensor_ring[ RING_BUFFER_SIZE ];  /* This is the size of the internal fifo. */
uint32_t* ring32;
int bgt60_ring_next_to_write = 0;
int bgt60_ring_next_to_read = 0;
int bgt60_ring_level=0;


cy_stc_scb_spi_context_t SPI_context;
mtb_hal_spi_t CYBSP_SPI_CONTROLLER_hal_obj;

cy_stc_sysint_t irq_cfg;

static volatile bool data_available = false;
/*******************************************************************************
* Preset configuration variables and structs.
*******************************************************************************/
struct xensiv_bgt60trxx_type
{
    uint32_t fifo_addr;
    uint16_t fifo_size;
    xensiv_bgt60trxx_device_t device;
};

struct radar_config radar_configs;

enum spi_state {
    NONE = 0,
    IDLE,
    BURST_PENDING,
    FIFO_READ_PENDING,
    FIFO_READ_DONE
};

enum spi_state sp_state = IDLE;

static void load_presets(void);
static void spi_set_data_width(CySCB_Type* base, uint32_t data_width);


static void cm55_ml_deepcraft_task(void);

#define RING_BUFFER_SIZE 0x00010000 /* 64k samples */

/*******************************************************************************
* Types
*******************************************************************************/
typedef struct {
    xensiv_bgt60trxx_mtb_t bgt60_obj;
    uint16_t bgt60_buffer0[16384];
    uint16_t bgt60_buffer1[16384];
    float bgt60_send_buffer[RING_BUFFER_SIZE];
    bool have_data;
    int skipped_frames;
} dev_bgt60trxx_t;

__attribute__ ((section(".cy_socmem_data"), used)) dev_bgt60trxx_t radar = {0}; 

/*******************************************************************************
* Function Name: mSPI_Interrupt
********************************************************************************
* Summary:
*   SPI_Interrupt
* Parameters:
*   
* Return:
*   
*******************************************************************************/
void mSPI_Interrupt(void)
{
    Cy_SCB_SPI_Interrupt(RADAR_SPI_CONTROLLER_HW, &SPI_context);
}


/*******************************************************************************
* Function Name: xensiv_bgt60trxx_interrupt_handler
********************************************************************************
* Summary:
* This is the interrupt handler to react on sensor indicating the availability
* of new data
*    1. Triggers the radar data manager for buffering radar data into software buffer.
*
* Parameters:
*  void
*
* Return:
*  none
*
*******************************************************************************/
void xensiv_bgt60trxx_interrupt_handler(void)
{
    data_available = true;
    Cy_GPIO_ClearInterrupt(CYBSP_RADAR_INT_PORT, CYBSP_RADAR_INT_NUM);
    NVIC_ClearPendingIRQ(irq_cfg.intrSrc);
}

/*******************************************************************************
* Function Name: radar_data_process
********************************************************************************
* Summary:
* This function processes the radar sensor data and feeds the data to DEEPCRAFT
* pre-processor model and returns the results.
*
* Parameters:
*  void
*
* Return:
*  CY_RSLT_SUCCESS if successful, otherwise an error code indicating failure.
*
*******************************************************************************/
cy_rslt_t radar_data_process(void)
{
    const char* class_map[] = IMAI_DATA_OUT_SYMBOLS;
    int a;
    int words_a_24;
    int fifo_size_32;
    int imai_result_enqueue;
    float model_out[IMAI_DATA_OUT_COUNT];

    xensiv_bgt60trxx_t* p_device = &radar.bgt60_obj.dev;

    void* iface = p_device->iface;
    xensiv_bgt60trxx_mtb_iface_t* mtb_iface = iface;

    /* Check if SPI transfer is active. If it is active then skip everything. */
    if (0UL != (CY_SCB_SPI_TRANSFER_ACTIVE &
                Cy_SCB_SPI_GetTransferStatus(mtb_iface->scb_inst, mtb_iface->spi)))
    {
        return 1;
    }

    switch (sp_state)
    {
        case IDLE:
            a = Cy_GPIO_Read(CYBSP_RADAR_INT_PORT,CYBSP_RADAR_INT_NUM);
            if ( a )
            {
                /* Make Chip select */
                xensiv_bgt60trxx_platform_spi_cs_set(iface, false);

                /* Read current fifo state */
                uint32_t* head = (uint32_t*)radar.bgt60_buffer0;
                const xensiv_bgt60trxx_mtb_iface_t* mtb_iface = iface;

                *head = XENSIV_BGT60TRXX_SPI_BURST_MODE_CMD |
                        (p_device->type->fifo_addr << XENSIV_BGT60TRXX_SPI_BURST_MODE_SADR_POS) |   /* Addr 0x60.. */
                        (0 << XENSIV_BGT60TRXX_SPI_BURST_MODE_RWB_POS) |    /* Read mode */
                        (0 << XENSIV_BGT60TRXX_SPI_BURST_MODE_LEN_POS);     /* Read until termination. */

                /* Ensure correct byte order for sending the command */
                *head = xensiv_bgt60trxx_platform_word_reverse(*head);

                spi_set_data_width(mtb_iface->scb_inst, 8U);
                Cy_SCB_SetByteMode(mtb_iface->scb_inst, true);

                Cy_SCB_SPI_Transfer(mtb_iface->scb_inst, (uint8_t*)radar.bgt60_buffer0,
                                                                    (uint8_t*)radar.bgt60_buffer1, /* Can be set to NULL, Recieved data is discarded. */
                                                                    4,
                                                                    mtb_iface->spi);

                sp_state = BURST_PENDING;
            }
            break;

        case BURST_PENDING:

            words_a_24 = radar_configs.fifo_int_level & 0xFFFFFFC0;

            spi_set_data_width(mtb_iface->scb_inst, 12U);

            Cy_SCB_SetByteMode(mtb_iface->scb_inst, false);


            Cy_SCB_SPI_Transfer(mtb_iface->scb_inst,
                                            (uint8_t*)radar.bgt60_buffer0, /* Can be set to NULL, Recieved data is discarded. */
                                            (uint8_t*)radar.bgt60_buffer1,
                                            words_a_24,
                                            mtb_iface->spi);

        /* At this point it might be a good idea to check if there is another interrupt from the radar pending. */
        /* If there is it should be possible to swap buffers and issue a second read of data. */
        /* In this state the radar burst mode is still active. */
            sp_state = FIFO_READ_PENDING;
            break;

        case FIFO_READ_PENDING:
            /* Clear the Chip Select. Done when the next polling allows for it. */
            xensiv_bgt60trxx_platform_spi_cs_set(iface, true);
            sp_state = FIFO_READ_DONE;
            break;

        case FIFO_READ_DONE:
            sp_state = IDLE;
            break;

        default:
            break;
    }

    
    if(sp_state == FIFO_READ_DONE)
    {
        radar.have_data = false;

/*********************************************************************************
**  Data copying is now made as a 32-bit data copying to reduce the number of
**  clock cycles needed.
*/
        ring32 = (uint32_t*)bgt60_tensor_ring; /* This will point to a 32k buffer. */
        fifo_size_32 = radar_configs.fifo_int_level>>1;

        for ( int x = 0; x<fifo_size_32; x++ )
        {
            ring32[bgt60_ring_next_to_write] = ((uint32_t*)(radar.bgt60_buffer1))[x]; /* radar->full_buffer[x]; */
            bgt60_ring_level+=2;        /* Copying 2 samples at a time. */
            bgt60_ring_next_to_write++;
            bgt60_ring_next_to_write&=RING_BUFFER_MASK32;
        }

        int t=0;
        while ( bgt60_ring_level >= radar_configs.num_samples_per_frame )
        {
            for ( int i=0; i < radar_configs.num_samples_per_frame; i+=1 )
            {
                (radar.bgt60_send_buffer)[t] = ((int16_t*)bgt60_tensor_ring)[bgt60_ring_next_to_read]*(1.0f);
                t++;
                bgt60_ring_level-=1;
                bgt60_ring_next_to_read++;
                bgt60_ring_next_to_read&=RING_BUFFER_MASK;
            }

            t = 0;

            /* Move cursor home */
            printf("\033[H");
            printf("DEEPCRAFT Radar Model Example\r\n\n");

            imai_result_enqueue = IMAI_enqueue(radar.bgt60_send_buffer);
            if (IMAI_RET_SUCCESS != imai_result_enqueue)
            {
                CY_ASSERT(0);
            }
            if (xensiv_bgt60trxx_start_frame(&radar.bgt60_obj.dev, false) != XENSIV_BGT60TRXX_STATUS_OK)
            {

            }
            int imai_result = IMAI_dequeue(model_out);
            if (xensiv_bgt60trxx_start_frame(&radar.bgt60_obj.dev, true) != XENSIV_BGT60TRXX_STATUS_OK)
            {

            }
            int16_t best_label = 0;
            float max_score = -1000.0f;

            switch (imai_result)
            {
                case IMAI_RET_SUCCESS:
                    for (uint8_t i = 0; i < IMAI_DATA_OUT_COUNT; i++)
                    {
                        printf("label: %-10s: score: %.2f\r\n", class_map[i], model_out[i]);
                        if (model_out[i] > max_score)
                        {
                            max_score = model_out[i];
                            best_label = i;
                        }
                    }
                    printf("\r\n");
                    printf("Output: %-30s\r\n", class_map[best_label]);
                    break;
                case IMAI_RET_NODATA:
                    break;
                case IMAI_RET_ERROR:
                    /* Something went wrong, stop the program */
                    printf("Unable to perform inference. Unknown error occurred.\n");
                    break;
                }

        }
    }
    return 0;
}
    


/*******************************************************************************
* Function Name: radar_init
********************************************************************************
* Summary:
* This function configures the SPI interface, initializes radar and interrupt
* service routine to indicate the availability of radar data.
*
* Parameters:
*  void
*
* Return:
*  Success or error
*
*******************************************************************************/
cy_rslt_t radar_init(void)
{
    cy_rslt_t result;

    result = Cy_SCB_SPI_Init(RADAR_SPI_CONTROLLER_HW, &RADAR_SPI_CONTROLLER_config, &SPI_context);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    
    if (result == CY_RSLT_SUCCESS)
    {
        cy_stc_sysint_t spiIntrConfig =
        {
            .intrSrc      = SPI_INTR_NUM,
            .intrPriority = SPI_INTR_PRIORITY,
        };

        Cy_SysInt_Init(&spiIntrConfig, &mSPI_Interrupt);
        NVIC_EnableIRQ(SPI_INTR_NUM);

        /* Set active target select to line 0 */
        Cy_SCB_SPI_SetActiveSlaveSelect(RADAR_SPI_CONTROLLER_HW, RADAR_SPI_SLAVE_SELECT);
        /* Enable SPI Controller block. */
        Cy_SCB_SPI_Enable(RADAR_SPI_CONTROLLER_HW);
    }


    if (CY_RSLT_SUCCESS != result)
    {
        CY_ASSERT(0);
    }

    load_presets();


    radar.have_data = false;
    radar.skipped_frames = 0;
    
    /* Enable the RADAR. */
    radar.bgt60_obj.iface.scb_inst = RADAR_SPI_CONTROLLER_HW;
    radar.bgt60_obj.iface.spi = &SPI_context;
    radar.bgt60_obj.iface.sel_port = CYBSP_RSPI_CS_PORT;
    radar.bgt60_obj.iface.sel_pin = CYBSP_RSPI_CS_PIN;
    radar.bgt60_obj.iface.rst_port = CYBSP_RADAR_RESET_PORT;
    radar.bgt60_obj.iface.rst_pin = CYBSP_RADAR_RESET_PIN;
    radar.bgt60_obj.iface.irq_port = CYBSP_RADAR_INT_PORT;
    radar.bgt60_obj.iface.irq_pin = CYBSP_RADAR_INT_PIN;
    radar.bgt60_obj.iface.irq_num = CYBSP_RADAR_INT_IRQ;


    /* Reduce drive strength to improve EMI */
    Cy_GPIO_SetSlewRate(CYBSP_RSPI_MOSI_PORT, CYBSP_RSPI_MOSI_PIN, CY_GPIO_SLEW_FAST);
    Cy_GPIO_SetDriveSel(CYBSP_RSPI_MOSI_PORT, CYBSP_RSPI_MOSI_PIN, CY_GPIO_DRIVE_1_8);
    Cy_GPIO_SetSlewRate(CYBSP_RSPI_CLK_PORT, CYBSP_RSPI_CLK_PIN, CY_GPIO_SLEW_FAST);
    Cy_GPIO_SetDriveSel(CYBSP_RSPI_CLK_PORT, CYBSP_RSPI_CLK_PIN, CY_GPIO_DRIVE_1_8);

    irq_cfg.intrSrc = (IRQn_Type)radar.bgt60_obj.iface.irq_num;
    irq_cfg.intrPriority = XENSIV_BGT60TRXX_IRQ_PRIORITY;

    result = xensiv_bgt60trxx_mtb_init(&radar.bgt60_obj, radar_configs.register_list, radar_configs.number_of_regs);
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    /*Initialization uses preset 0*/
    result = xensiv_bgt60trxx_mtb_interrupt_init(&radar.bgt60_obj, radar_configs.num_samples_per_frame);
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    Cy_SysInt_Init(&irq_cfg, xensiv_bgt60trxx_interrupt_handler);

    NVIC_ClearPendingIRQ(irq_cfg.intrSrc);
    NVIC_EnableIRQ(irq_cfg.intrSrc);

    Cy_GPIO_ClearInterrupt(CYBSP_RADAR_INT_PORT, CYBSP_RADAR_INT_NUM);
    NVIC_ClearPendingIRQ(irq_cfg.intrSrc);

    Cy_SysLib_Delay(1000);

    if (result != CY_RSLT_SUCCESS)
    {
        printf("ERROR: xensiv_bgt60trxx_mtb_init failed\n");
        return false;
    }
  
    if (result == CY_RSLT_SUCCESS)
    {
        xensiv_bgt60trxx_set_fifo_limit(&radar.bgt60_obj.dev, radar_configs.fifo_int_level);
    }
    printf("Radar: Initialized device.\r\n");

    if (xensiv_bgt60trxx_soft_reset(&radar.bgt60_obj.dev, XENSIV_BGT60TRXX_RESET_FIFO) != XENSIV_BGT60TRXX_STATUS_OK)
    {
        printf("Fifo reset error error.\r\n");
    }

    /* Reset the local fifo. */
    bgt60_ring_next_to_write = 0;
    bgt60_ring_next_to_read = 0;
    bgt60_ring_level=0;

    radar.have_data = false;
    if (xensiv_bgt60trxx_start_frame(&radar.bgt60_obj.dev, true) != XENSIV_BGT60TRXX_STATUS_OK)
    {
        printf("Start frame error.\r\n");
    }

    return CY_RSLT_SUCCESS;

}
/*******************************************************************************
 * Platform functions implementation
 ******************************************************************************/
static void spi_set_data_width(CySCB_Type* base, uint32_t data_width)
{
    CY_ASSERT(CY_SCB_SPI_IS_DATA_WIDTH_VALID(data_width));

    CY_REG32_CLR_SET(SCB_TX_CTRL(base),
                     SCB_TX_CTRL_DATA_WIDTH,
                     (uint32_t)data_width - 1U);
    CY_REG32_CLR_SET(SCB_RX_CTRL(base),
                     SCB_RX_CTRL_DATA_WIDTH,
                     (uint32_t)data_width - 1U);
}

/*******************************************************************************
* Summary:
*   This just loads preset from header file constants to enable selection of
*   various radar presets.
* See radar_settings.h for information of the sources of radar settings.
*
*******************************************************************************/
static void load_presets()
{
    radar_configs.start_freq =            P0_XENSIV_BGT60TRXX_CONF_START_FREQ_HZ;
    radar_configs.end_freq =              P0_XENSIV_BGT60TRXX_CONF_END_FREQ_HZ;
    radar_configs.samples_per_chirp =     P0_XENSIV_BGT60TRXX_CONF_NUM_SAMPLES_PER_CHIRP;
    radar_configs.chirps_per_frame =      P0_XENSIV_BGT60TRXX_CONF_NUM_CHIRPS_PER_FRAME;
    radar_configs.rx_antennas =           P0_XENSIV_BGT60TRXX_CONF_NUM_RX_ANTENNAS;
    radar_configs.tx_antennas =           P0_XENSIV_BGT60TRXX_CONF_NUM_TX_ANTENNAS;
    radar_configs.sample_rate =           P0_XENSIV_BGT60TRXX_CONF_SAMPLE_RATE;
    radar_configs.chirp_repetition_time = P0_XENSIV_BGT60TRXX_CONF_CHIRP_REPETITION_TIME_S;
    radar_configs.frame_repetition_time = P0_XENSIV_BGT60TRXX_CONF_FRAME_REPETITION_TIME_S;

    radar_configs.frame_rate = (int)(0.5 + 1.0/radar_configs.frame_repetition_time);
    radar_configs.num_samples_per_frame =
            radar_configs.samples_per_chirp *
            radar_configs.chirps_per_frame *
            radar_configs.rx_antennas;

    radar_configs.fifo_int_level = DEFAULT_FIFO_SETTING; /* This will equal 75% of the fifo. 12288 samples */

    radar_configs.number_of_regs = P0_XENSIV_BGT60TRXX_CONF_NUM_REGS;
    radar_configs.register_list = register_list_p0;

}

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for CM55 CPU. It initializes BSP, radar, SPI and the
* ML model. It reads data from radar sensor continuously, processes it within
* the model and displays the output.
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

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();
    
    cm55_ml_deepcraft_task();

}



/*******************************************************************************
* Function Name: system_init
********************************************************************************
* Summary:
*  Initializes the neural network based on the DEEPCRAFT model and the
*  DEEPCRAFT pre-processor and initializes the Radar sensor.
*
* Parameters:
*  None
*
* Returns:
*  The status of the initialization.
*
*******************************************************************************/
static cy_rslt_t system_init(void)
{
    cy_rslt_t result;

    /* Initialize DEEPCRAFT pre-processing library */
    IMAI_init();

    result = radar_init();

    /* ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H\x1b[?25l;");
    
    return result;
}

/*******************************************************************************
* Function Name: cm55_ml_deepcraft_task
********************************************************************************
* Summary:
*  Contains the main loop for the application. It sets up the UART for
*  logs and initialises the system (DEEPCRAFT pre-processor and Radar sensor
*  for gesture input). It then invokes the Radar Data Processing function that
*  sends the data for pre-processing, inferencing, and prints in the results
*  when enough data is received.
*
* Parameters:
*  None
*
* Returns:
*  None
*
*******************************************************************************/

static void cm55_ml_deepcraft_task(void)
{
    cy_rslt_t result;
    /* Initializes retarget-io middleware. */
    init_retarget_io();

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");    
    
    /* Initialize inference engine and sensors */
    result = system_init();

    /* Initialization failed */
    if(CY_RSLT_SUCCESS != result)
    {
        /* Failed to initialize properly */
        printf("System initialization fail\r\n");
        while(1);
    }

    for (;;)
    {
        /* Invoke the PDM Data Processing function that sends the data for
         * pre-processing, inferencing, and print the results when enough data
         * is received.
         */
        radar_data_process();
    }
}
