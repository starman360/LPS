#ifndef ESPNVM_H_
#define ESPNVM_H_

// -- GENERAL C INCLUDES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// -- SYSTEM INCLUDES
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
// -- MEMORY RELATED INCLUDES
#include "nvs_flash.h"
#include "nvs.h"

// -- DESIRED CONFIGURATION VALUES
#define DWM1_CFG_BYTE0     0x9A
#define DWM1_CFG_BYTE1     0x00
#define DWM1_INTCFG_BYTE0  0x10
#define DWM2_CFG_BYTE0     0x1A
#define DWM2_CFG_BYTE1     0x30
#define DWM2_INTCFG_BYTE0  0x10

// -- DEBUG
#define NVS_DEBUG

/***************************************************************
 *     NODE CONFIGURATION STRUCT (NON-VOLATILE MEMORY) (NVM)   *
 * ----------------------------------------------------------- *
 *  dwm_cfg_t will have its 'bits' loaded with the node        *
 *  configuration stored in the ESP32 NVM. the values can then *
 *  be accessed through the inner struct as follows:           *
 *      // cast from uint64 'x' to struct                      *
 *      dwm_cfg_t mycfg = {.bits = x};                         *
 *                                                             *
 *      // cast to uint64 'x'                                  *
 *      uint64_t x = mycfg.bits;                               *
 ***************************************************************/
union _dwm_cfg{
    struct {
        uint8_t dwm1_cfg_byte0    : 8;
        uint8_t dwm1_cfg_byte1    : 8;
        uint8_t dwm1_intcfg_byte0 : 8;
        uint8_t dwm2_cfg_byte0    : 8;
        uint8_t dwm2_cfg_byte1    : 8;
        uint8_t dwm2_intcfg_byte0 : 8;
    } cfg;
    uint64_t bits;    
};
typedef union _dwm_cfg dwm_cfg_t;

/***************************************************************
 *              ESP NON VOLATILE STORAGE FUNCTIONS             *
 ***************************************************************/
void nvs_get_node_dwm_cfg(dwm_cfg_t *);
void nvs_set_node_dwm_cfg(dwm_cfg_t *);


#endif // ESPNVM_H_