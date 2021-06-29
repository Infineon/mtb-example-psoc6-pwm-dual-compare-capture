/*******************************************************************************
* File Name:   main.c
*
* Description: The code example demonstrates the generation of asymmetric PWM
* signals using two compare/capture registers available in TCPWM block of
* PSoC 6 devices. Compared to the asymmetric PWM realized with only one
* compare function (where CPU is used to update the compare value two times in
* every PWM cycle), this solution uses two independent buffered compare values
* and generates less CPU load (where CPU is used to update the compare value once
* in every PWM cycle). The CE talks about these advantages and its application
* in the domain of field-oriented control applications.
*
* Related Document: See README.md
*
*******************************************************************************
* Copyright 2020-2021, Cypress Semiconductor Corporation (an Infineon company) or
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

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#define UART_IRQ_PRIORITY       (3)
#define COMPARE_VALUE_DELTA     (100)
#define DELAY_BETWEEN_READ_MS   (100)

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void uart_event_handler(void *handler_arg, cyhal_uart_event_t event);
void print_instructions(void);
void process_key_press(char);

/*******************************************************************************
* Global Variables
*******************************************************************************/
uint32_t period; /* Variable to store period value of TCPWM block */
int32_t compare0_value; /* Variable to store the CC0 value of TCPWM block */
int32_t compare1_value; /* Variable to store the CC1 value of TCPWM block */
volatile bool uart_read_flag = false;

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for CM4 CPU. Initializes the retarget-IO and sets
* up a callback to be triggered upon receiving data. It sets up the TCPWM in
* PWM mode. The infinite loop sets up asynchronous UART read and depending on
* the command read, the compare values are modified to change the duty cycle and
* phase.
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
    char uart_read_value; /* Variable to store the read command through UART */

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize retarget-io to use the debug UART port */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX,
                        CY_RETARGET_IO_BAUDRATE);

    /* The UART callback handler registration */
    cyhal_uart_register_callback(&cy_retarget_io_uart_obj, uart_event_handler,
                                 NULL);

    /* Enable UART events to get notified on receiving
     * RX data and on RX errors */
    cyhal_uart_enable_event(&cy_retarget_io_uart_obj,
                            (cyhal_uart_event_t)(CYHAL_UART_IRQ_RX_ERROR |
                            CYHAL_UART_IRQ_RX_DONE),
                            UART_IRQ_PRIORITY, true);

    /* Initialize and enable the TCPWM block */
    Cy_TCPWM_PWM_Init(TCPWM0_GRP1_CNT0_HW, TCPWM0_GRP1_CNT0_NUM,
                      &TCPWM0_GRP1_CNT0_config);
    Cy_TCPWM_PWM_Enable(TCPWM0_GRP1_CNT0_HW, TCPWM0_GRP1_CNT0_NUM);

    /* Fetch the initial values of period, CC0 and CC1 registers configured
     * through the design file */
    period = Cy_TCPWM_PWM_GetPeriod0(TCPWM0_GRP1_CNT0_HW, TCPWM0_GRP1_CNT0_NUM);
    compare1_value = Cy_TCPWM_PWM_GetCompare1Val(TCPWM0_GRP1_CNT0_HW,
                                                 TCPWM0_GRP1_CNT0_NUM);
    compare0_value = Cy_TCPWM_PWM_GetCompare0Val(TCPWM0_GRP1_CNT0_HW,
                                                 TCPWM0_GRP1_CNT0_NUM);

    /* Start the TCPWM block */
    Cy_TCPWM_TriggerStart_Single(TCPWM0_GRP1_CNT0_HW, TCPWM0_GRP1_CNT0_NUM);

    /* Enable global interrupts */
    __enable_irq();

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");

    printf("***********************************************************\r\n"
           "PSoC 6 MCU: TCPWM in PWM Mode with Dual Compare/Capture\r\n"
           "***********************************************************\r\n\n");

    print_instructions();

    for (;;)
    {
        /* Begin asynchronous RX read */
        cyhal_uart_read_async(&cy_retarget_io_uart_obj,
                              (void*) &uart_read_value,
                              sizeof(uart_read_value));

        /* Check if the read flag has been set by the callback */
        if(uart_read_flag)
        {
            /* Clear read flag */
            uart_read_flag = false;

            /* Process the command and modify the compare values to change the
             * duty cycle and phase.
             */
            process_key_press(uart_read_value);
        }

        /* Delay between next read */
        cyhal_system_delay_ms(DELAY_BETWEEN_READ_MS);
    }
}

/*******************************************************************************
* Function Name: uart_event_handler
********************************************************************************
* Summary:
* UART event handler callback function. Sets the read flag to true upon
* successful reception of data.
*
* Parameters:
*  handler_arg - argument for the handler provided during callback registration
*  event - interrupt cause flags
*
* Return:
*  void
*
*******************************************************************************/
void uart_event_handler(void *handler_arg, cyhal_uart_event_t event)
{
    (void)handler_arg;

    if (CYHAL_UART_IRQ_RX_DONE == (event & CYHAL_UART_IRQ_RX_DONE))
    {
        /* Set read flag */
        uart_read_flag = true;
    }
    else
    {
        CY_ASSERT(0);
    }
}

/*******************************************************************************
* Function Name: process_key_press
********************************************************************************
* Summary:
* Function to process the key pressed. Depending on the command passed as
* parameter, new compare values are calculated. The values are written to
* the respective buffer registers and a compare swap is issued.
*
* Parameters:
*  key_pressed - command read through terminal
*
* Return:
*  void
*
*******************************************************************************/
void process_key_press(char key_pressed)
{
    printf("Pressed key: %c\r\n", key_pressed);

    switch(key_pressed)
    {
        /* Increase duty cycle */
        case 's':
            compare0_value += COMPARE_VALUE_DELTA;
            compare1_value += COMPARE_VALUE_DELTA;
            if( compare0_value > period )
                compare0_value = period;
            if( compare1_value > period )
                compare1_value = period;
            break;
        /* Decrease duty cycle */
        case 'w':
            compare0_value -= COMPARE_VALUE_DELTA;
            compare1_value -= COMPARE_VALUE_DELTA;
            if( compare0_value < 0 )
                compare0_value = 0;
            if( compare1_value < 0 )
                compare1_value = 0;
            break;
        /* Shift waveform to left */
        case 'a':
            compare0_value -= COMPARE_VALUE_DELTA;
            compare1_value += COMPARE_VALUE_DELTA;
            if( compare0_value < 0 )
                compare0_value = 0;
            if( compare1_value > period )
                compare1_value = period;
            break;
        /* Shift waveform to right */
        case 'd':
            compare0_value += COMPARE_VALUE_DELTA;
            compare1_value -= COMPARE_VALUE_DELTA;
            if( compare0_value > period )
                compare0_value = period;
            if( compare1_value < 0 )
                compare1_value = 0;
            break;
        default:
            printf("Wrong key pressed !! See below instructions:\r\n");
            print_instructions();
            return;
    }

    printf("Period: %lu\tCompare0: %ld\tCompare1: %ld\r\n",
          (unsigned long)period, (long)compare0_value,
          (long)compare1_value);

    /* Set new values for CC0/1 compare buffers */
    Cy_TCPWM_PWM_SetCompare0BufVal(TCPWM0_GRP1_CNT0_HW,
                                   TCPWM0_GRP1_CNT0_NUM,
                                   compare0_value);
    Cy_TCPWM_PWM_SetCompare1BufVal(TCPWM0_GRP1_CNT0_HW,
                                   TCPWM0_GRP1_CNT0_NUM,
                                   compare1_value);

    /* Trigger compare swap with its buffer values */
    Cy_TCPWM_TriggerCaptureOrSwap_Single(TCPWM0_GRP1_CNT0_HW,
                                         TCPWM0_GRP1_CNT0_NUM);
}

/*******************************************************************************
* Function Name: print_instructions
********************************************************************************
* Summary:
* Prints set of instructions.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void print_instructions(void)
{
    printf("====================================================\r\n"
           "Instructions:\r\n"
           "====================================================\r\n"
           "Press 'w' : To increase the duty cycle\r\n"
           "Press 's' : To decrease the duty cycle\r\n"
           "Press 'a' : To shift waveform towards left\r\n"
           "Press 'd' : To shift waveform towards right\r\n"
           "====================================================\r\n");
}
