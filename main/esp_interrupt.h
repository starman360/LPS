#ifndef ESPINT_H_
#define ESPINT_H_

// -- GENERAL C INCLUDES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// -- SYSTEM INCLUDES
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
// -- INTERRUPT RELATED INCLUDES
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

// -- INTERRUPT PINS
#define GPIO_OUTPUT_IO_0    2
#define GPIO_OUTPUT_IO_1    4
#define GPIO_OUTPUT_PIN_SEL ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))
#define GPIO_INPUT_IO_0     15
#define GPIO_INPUT_IO_1     0
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1))
#define ESP_INTR_FLAG_DEFAULT 0

// -- ESP ISR QUEU HANDLER
static xQueueHandle gpio_evt_queue = NULL;

/***************************************************************
 *               ESP ISR INITIALIZATION FUNCTION               *
 ***************************************************************/
void interrupt_init();

/***************************************************************
 *                      ESP ISR HANDLER                        *
 * ------------------------------------------------------------*
 * This func will queue the task that the ISR triggered        *
 ***************************************************************/
static void IRAM_ATTR gpio_isr_handler(void*);


/***************************************************************
 *                    DATA READY ISR TASK                      *
 ***************************************************************/
static void data_ready_task(void*);

#endif // ESPINT_H_