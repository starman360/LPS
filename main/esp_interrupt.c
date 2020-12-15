#include "esp_interrupt.h"

/***************************************************************
 *               ESP ISR INITIALIZATION FUNCTION               *
 ***************************************************************/
void interrupt_init(){
    // -- Declare the struct that holds GPIO pin configs
    gpio_config_t io_conf;
    
    // -- Configure output pin
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;  // disable interrupt
    io_conf.mode = GPIO_MODE_OUTPUT;  // set as output mode
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;  // bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pull_down_en = 0;  // disable pull-down mode
    io_conf.pull_up_en = 0;  // disable pull-up mode
    gpio_config(&io_conf);  // configure GPIO with the given settings

    // -- Configure input pin
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;  // interrupt of rising edge
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;  // bit mask of the pins, use GPIO4/5 here
    io_conf.mode = GPIO_MODE_INPUT;  // set as input mode    
    io_conf.pull_up_en = 1;  // enable pull-up mode
    gpio_config(&io_conf);  // configure GPIO with the given settings

    // -- Ex of changing interrupt type of gpio input pin after config above
    gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

    // -- Create a queue to hold up to 10 items of uint23_t size
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    
    // -- Create a task that holds a function, a task name, size of task stack, & more
    xTaskCreate(data_ready_task, "data_ready_task", 2048, NULL, 10, NULL);

    // -- Install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void*) GPIO_INPUT_IO_1);

    // remove isr handler for gpio number.
    // gpio_isr_handler_remove(GPIO_INPUT_IO_0);
    // hook isr handler for specific gpio pin again
    // gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);
}

/***************************************************************
 *                      ESP ISR HANDLER                        *
 * ------------------------------------------------------------*
 * This func will queue the task that the ISR triggered        *
 ***************************************************************/
static void IRAM_ATTR gpio_isr_handler(void* arg){
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

/***************************************************************
 *                    DATA READY ISR TASK                      *
 ***************************************************************/
static void data_ready_task(void* arg){
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}

