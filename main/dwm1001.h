#ifndef DWM1001_H_
#define DWM1001_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"

/* ----- ESP32 SPI PIN DEFINES -----*/
#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
/* ----- TAG DWM PIN DEFINES -----*/
#define PIN_NUM_CS1   5
#define PIN_NUM_INT1 21
/* ----- ANC DWM PIN DEFINES -----*/
#define PIN_NUM_CS2   17
#define PIN_NUM_INT2  3

/* ----- NODE TYPE DEFINE -----*/
#define NODE_TYPE_TAG 0
#define NODE_TYPE_ANC 1

/* --- LIMITS --- */
#define NODE_LIMIT 4

/* ----- DEBUG DEFINE -----*/
#define TLV_DEBUG
#define FILTERED_RESPONSE_DEBUG
// #define NODE_DEBUG

/* ----- ESP32 NECESSARY SPI STRUCT DECLARATION -----*/
esp_err_t ret;
spi_device_handle_t spi1;
spi_device_handle_t spi2;

/* --- SPI INTERRUPT FLAGS --- */
static bool DATA_READY_EN;
volatile bool DATA_READY_FLAG1;
volatile bool DATA_READY_FLAG2;

/* ------- NODE TYPE TRANSMIT -------*/
/* - This global is written to      -*/
/* - EVERYTIME the ESP talks to DWM -*/
static bool DWM_NUM; // 0 - NODE_TYPE_TAG, 1 - NODE_TYPE_ANC

/***************************************************************
 *     NODE CONFIGURATION VARS (NON-VOLATILE MEMORY) (NVM)     *
 * ----------------------------------------------------------- *
 *  These vars are loaded with the configuration of both DWM   *
 *  modules & interrupt capability. The values of the configs  *
 *  are stored in NVM and loaded into this struct on init      *
 ***************************************************************/
static uint8_t dwm1_cfg[2], dwm2_cfg[2];
static bool DATA_READY_EN1, DATA_READY_EN2;

/***************************************************************
 *                   SPI TRANSACTION STRUCTS                   *
 * ----------------------------------------------------------- *
 *  These are the hidden structs that are passed around        *
 *  the SPI TRANSACTION FUNCTIONS (i.e. structs that pass the  *
 *  raw, unflitered spi transaction data)                      *
 ***************************************************************/
/*
 The DWM needs a specific format for a message, in the form of type, length, & value.
 - type: specifies the specific API call to the DWM
 - length: specifies the length of the to-be transmitted values (if not applicable, then 0)
 - value: an array that holds teh values to be written (if not applicable then {0})
*/
struct _dwm_write_tlv{
    uint8_t type;
    uint8_t length;
    uint8_t value[16]; 
};
typedef struct _dwm_write_tlv dwm_write_tlv;
/*
 The DWM responds with a message, in the form of type, length, & value. However, depending on the API call
 the response may consist of multiple tlv structures, therefore we store the response in an array. This response 
 struct will be fed into a post-processing function to get usable data.
 Rxsize is the length of the response.
*/
struct _dwm_response{
    uint8_t rxsize;
    uint8_t response[256];
};
typedef struct _dwm_response dwm_response;

/***************************************************************
 *                SPI FILTERED RESPONSE STRUCTS                *
 * ----------------------------------------------------------- *
 *  These are the structs that will be passed around the mesh  *
 *  (i.e. structs that hold the filtered spi responses)        *
 ***************************************************************/
struct _dwm_resp_err{
    uint8_t err_code;
    // char *err_string;
};
typedef struct _dwm_resp_err dwm_resp_err;

struct _dwm_resp_cfg_get{
    dwm_resp_err err;
    union _cfg{
        struct _cfg_vals{
            uint8_t uwb_mode      : 2; // Byte 0 - bits 1-0
            uint8_t fw_update_en  : 1; // Byte 0 - bit  2
            uint8_t ble_en        : 1; // Byte 0 - bit  3
            uint8_t led_en        : 1; // Byte 0 - bit  4
            uint8_t reserved_b0   : 1; // Byte 0 - bit  5
            uint8_t loc_engine_en : 1; // Byte 0 - bit  6
            uint8_t low_power_en  : 1; // Byte 0 - bit  7
            uint8_t meas_mode     : 2; // Byte 1 - bits 1-0
            uint8_t accel_en      : 1; // Byte 1 - bit  2
            uint8_t bridge        : 1; // Byte 1 - bit  3
            uint8_t initiator     : 1; // Byte 1 - bit  4
            uint8_t mode          : 1; // Byte 1 - bit  5
            uint8_t reserved_b1   : 2; // Byte 1 - bits 7-6
        } cfg_vals;
        uint8_t cfg_bytes[2]; 
    }cfg;
};
typedef struct _dwm_resp_cfg_get dwm_resp_cfg_get;

struct _dwm_resp_tag_loc_get{
    dwm_resp_err err;
    uint8_t num_dists;
    uint16_t *id;
    uint32_t *dist; 
    uint8_t *qf;
};
typedef struct _dwm_resp_tag_loc_get dwm_resp_tag_loc_get;

/***************************************************************
 *                    DWM SPI DATA READY ISR                   *
 ***************************************************************/
void data_ready_isr(void *);

/***************************************************************
 *                SPI INITIALIZATION FUNCTIONS                 *
 ***************************************************************/
void spi_init();

/***************************************************************
 *                 SPI TRANSACTION FUNCTIONS                   *
 ***************************************************************/
void spi_write(dwm_write_tlv *);
void spi_wait(dwm_response *);
void spi_receive(dwm_response *);
void dwm_tlv(dwm_write_tlv *, dwm_response *, bool);

/***************************************************************
 *                    DWM SPI API FUNCTIONS                    *
 ***************************************************************/
dwm_resp_err dwm_tag_set(uint8_t *, bool);
dwm_resp_err dwm_anc_set(uint8_t, bool);
dwm_resp_err dwm_rst(bool);
dwm_resp_cfg_get dwm_cfg_get(bool);
int dwm_tag_loc_get(uint8_t *, bool);
dwm_resp_err dwm_int_cfg(uint8_t, bool);

/***************************************************************
 *              DWM RESPONSE FILTERING FUNCTIONS               *
 ***************************************************************/
void dists2buf(dwm_resp_tag_loc_get *, uint8_t*, uint16_t minid);
void dists2struct(dwm_resp_tag_loc_get *, uint8_t*);



#endif // DWM1001_H_