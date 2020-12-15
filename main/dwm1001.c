#include "dwm1001.h"

/* ----- ESP32 NECESSARY SPI STRUCT INITIALIZATION -----*/
spi_bus_config_t buscfg={
    .miso_io_num=PIN_NUM_MISO,
    .mosi_io_num=PIN_NUM_MOSI,
    .sclk_io_num=PIN_NUM_CLK,
    .quadwp_io_num=-1,
    .quadhd_io_num=-1
};
spi_device_interface_config_t devcfg1={
    .clock_speed_hz= 1*1000*1000, //1*1000*1000,            //Clock out at 1 MHz
    .mode=0,                                //SPI mode 0
    .spics_io_num=PIN_NUM_CS1,               //CS pin
    .queue_size=1,                          //We want to be able to queue 7 transactions at a time
};
spi_device_interface_config_t devcfg2={
    .clock_speed_hz= 1*1000*1000, //1*1000*1000,            //Clock out at 1 MHz
    .mode=0,                                //SPI mode 0
    .spics_io_num=PIN_NUM_CS2,               //CS pin
    .queue_size=1,                          //We want to be able to queue 7 transactions at a time
};

/* ----- DWM RESPONSE ERROR CODE DECLARATION & INITIALIZATION -----*/
char *dwm_err_codes[6] = {
    "OK",
    "Unknown command or broken TLV frame",
    "Internal error",
    "Invalid Parameter",
    "Busy",
    "Incoherent SPI, debug please"};
    
/***************************************************************
 *                    DWM SPI DATA REDAY ISR                   *
 ***************************************************************/
void data_ready_isr( void * intpin){
    uint8_t pin = (uint8_t) intpin;

    switch(pin){
        case PIN_NUM_INT1:
            DATA_READY_FLAG1 = 1;
            break;
        case PIN_NUM_INT2:
            DATA_READY_FLAG2 = 1;
            break;          
    }
    printf("Pin %d went high, Data Ready\n", pin);
}

/***************************************************************
 *                SPI INITIALIZATION FUNCTIONS                 *
 ***************************************************************/
void spi_init(){    
    ret=spi_bus_initialize(VSPI_HOST, &buscfg, 1);  //Initialize the SPI bus
    ESP_ERROR_CHECK(ret);
    
    ret=spi_bus_add_device(VSPI_HOST, &devcfg1, &spi1);  //Attach the DWM TAG to the SPI bus
    ESP_ERROR_CHECK(ret);

    ret=spi_bus_add_device(VSPI_HOST, &devcfg2, &spi2);  //Attach the DWM ANC to the SPI bus
    ESP_ERROR_CHECK(ret);

}

/***************************************************************
 *                  SPI TRANSACTION FUNCTIONS                  *
 ***************************************************************/
void spi_write(dwm_write_tlv *tlv){
    uint8_t write_len=2+tlv->length; //Capture transaction array length (accounting for type & length)
    esp_err_t ret;
    spi_transaction_t t = {};
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8*write_len;                   //Len is in bytes, transaction length is in bits.
    t.rxlength = 8*write_len;
    t.tx_buffer=tlv;              //Data
    t.rx_buffer=tlv;
    
    if(DWM_NUM == NODE_TYPE_TAG)
        ret=spi_device_polling_transmit(spi1, &t);  // Transmit to TAG!
    else
        ret=spi_device_polling_transmit(spi2, &t);  // Transmit to ANC!
    assert(ret==ESP_OK);      
}

void spi_wait(dwm_response *tlv){
    uint32_t txdummy = 0x000000FF;
    uint32_t rxdummy = 0x00000000;
    tlv->rxsize = 0x00;
    
    if(DATA_READY_EN == 1){
        while((!DATA_READY_FLAG1));
        DATA_READY_FLAG1 = 0;
        DATA_READY_FLAG2 = 0;

        esp_err_t ret;
        spi_transaction_t t = {};
        memset(&t, 0, sizeof(t));     //Zero out the transaction
        t.length=8;                   //Len is in bytes, transaction length is in bits.
        t.rxlength = 8;
        t.tx_buffer= &txdummy;       //Data
        t.rx_buffer= &rxdummy;

        if(DWM_NUM == NODE_TYPE_TAG)
            ret=spi_device_polling_transmit(spi1, &t);  // Transmit to TAG!
        else
            ret=spi_device_polling_transmit(spi2, &t);  // Transmit to ANC!
        assert(ret==ESP_OK);
    } else {
        while(rxdummy == 0x00){
            esp_err_t ret;
            spi_transaction_t t = {};
            memset(&t, 0, sizeof(t));     //Zero out the transaction
            t.length=8;                   //Len is in bytes, transaction length is in bits.
            t.rxlength = 8;
            t.tx_buffer= &txdummy;        //Data
            t.rx_buffer= &rxdummy;  
            vTaskDelay(1/ portTICK_RATE_MS);

            if(DWM_NUM == NODE_TYPE_TAG)
                ret=spi_device_polling_transmit(spi1, &t);  // Transmit to TAG!
            else
                ret=spi_device_polling_transmit(spi2, &t);  // Transmit to ANC!
            assert(ret==ESP_OK);
        } 
    }
    tlv->rxsize = rxdummy & 0x0000007F;
}

void spi_receive(dwm_response *tlv){
    memset(tlv->response, 0xFF, tlv->rxsize); // Initialize dummy transaction with 0xFFs

    esp_err_t ret;
    spi_transaction_t t = {};
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8*tlv->rxsize;                   //Len is in bytes, transaction length is in bits.
    t.rxlength = 8*tlv->rxsize;
    t.tx_buffer= tlv->response;              //Data
    t.rx_buffer= tlv->response;
    
    if(DWM_NUM == NODE_TYPE_TAG)
        ret=spi_device_polling_transmit(spi1, &t);  // Transmit to TAG!
    else
        ret=spi_device_polling_transmit(spi2, &t);  // Transmit to ANC!
    assert(ret==ESP_OK);
}

void dwm_tlv(dwm_write_tlv *tlv, dwm_response *response, bool node_type){
    DWM_NUM = (node_type == 1) ? 1 : 0; 
    spi_write(tlv);
    spi_wait(response);
    spi_receive(response);

    #ifdef TLV_DEBUG
    printf("[DWM %d]\n", node_type);
    printf("Write Response: ");
    printf("%02X, ", tlv->type);
    printf("%02X \n", tlv->length);
    printf("Rxsize: %d\n", response->rxsize);
    printf("Transaction Response: ");
    for(uint8_t i=0; i<response->rxsize; i++)
        printf("%02X, ", response->response[i]);
    printf("\n");
    #endif // DEBUG
}

/***************************************************************
 *                    DWM SPI API FUNCTIONS                    *
 ***************************************************************/
dwm_resp_err dwm_tag_set(uint8_t *tagcfg, bool DWM_NUM){
    /* --- Decalre & Initialize transaction variables --- */
    dwm_write_tlv tlv = {0x05, 0x02, {tagcfg[0],tagcfg[1]}};
    dwm_response response;
    dwm_resp_err err;
    /* --- Do the transaction --- */
    dwm_tlv(&tlv, &response, DWM_NUM);
    /* --- Error check the TLV response --- */
    err.err_code = response.response[2]; // Retrieve error code of transaction
    // if(err.err_code < 5)
    //     err.err_string = dwm_err_codes[err.err_code]; // Retrieve error string corresponding to the code
    // else
    //     err.err_string = dwm_err_codes[5]; // Retrieve error string corresponding to the code
    /* --- Debug Info --- */
    #ifdef TLV_DEBUG
    if(err.err_code != 0x00){
        printf("[ERROR]: Oops, something happened when executing 'dwm_tag_set()'"); 
        // printf("\n\t-ERROR CODE: %02X\n\t-ERROR INFO: %s\n",err.err_code,err.err_string);
        printf("---------------------------------------------------------------------\n");
    }
    #endif // DEBUG

    return err;
}

dwm_resp_err dwm_anc_set(uint8_t anccfg, bool DWM_NUM){
    /* --- Decalre & Initialize transaction variables --- */
    dwm_write_tlv tlv = {0x07, 0x01, {anccfg}};
    dwm_response response;
    dwm_resp_err err;
    /* --- Do the transaction --- */
    dwm_tlv(&tlv, &response, DWM_NUM);
    /* --- Error check the TLV response --- */
    err.err_code = response.response[2]; // Retrieve error code of transaction
    // if(err.err_code < 5)
    //     err.err_string = dwm_err_codes[err.err_code]; // Retrieve error string corresponding to the code
    // else
    //     err.err_string = dwm_err_codes[5]; // Retrieve error string corresponding to the code
    /* --- Debug Info --- */
    #ifdef FILTERED_RESPONSE_DEBUG
    if(err.err_code != 0x00){
        printf("[ERROR]: Oops, something happened when executing 'dwm_anc_set()'"); 
        // printf("\n\t-ERROR CODE: %02X\n\t-ERROR INFO: %s\n",err.err_code,err.err_string);
        printf("---------------------------------------------------------------------\n");
    }
    #endif // DEBUG
    
    return err;
}

dwm_resp_err dwm_rst(bool DWM_NUM){
    /* --- Decalre & Initialize transaction variables --- */
    dwm_write_tlv tlv = {0x14, 0x00, {0x00}};
    dwm_response response;
    dwm_resp_err err;
    /* --- Do the transaction --- */
    dwm_tlv(&tlv, &response, DWM_NUM);
    /* --- Error check the TLV response --- */
    err.err_code = response.response[2]; // Retrieve error code of transaction
    // if(err.err_code < 5)
    //     err.err_string = dwm_err_codes[err.err_code]; // Retrieve error string corresponding to the code
    // else
    //     err.err_string = dwm_err_codes[5]; // Retrieve error string corresponding to the code
    /* --- Debug Info --- */
    #ifdef FILTERED_RESPONSE_DEBUG
    if(err.err_code != 0x00){
        printf("[ERROR]: Oops, something happened when executing 'dwm_rst()'"); 
        // printf("\n\t-ERROR CODE: %02X\n\t-ERROR INFO: %s\n",err.err_code,err.err_string);
        printf("---------------------------------------------------------------------\n");
    }
    #endif // DEBUG

    return err;
}

dwm_resp_cfg_get dwm_cfg_get(bool DWM_NUM){
    /* --- Decalre & Initialize transaction variables --- */
    dwm_write_tlv tlv = {0x08, 0x00, {0x00}};
    dwm_response response;
    dwm_resp_cfg_get mycfg;
    /* --- Do the transaction --- */
    dwm_tlv(&tlv, &response, DWM_NUM);
    /* --- Error check the TLV response --- */
    mycfg.err.err_code = response.response[2]; // Retrieve error code of transaction
    // if(mycfg.err.err_code < 5)
    //     mycfg.err.err_string = dwm_err_codes[mycfg.err.err_code]; // Retrieve error string corresponding to the code
    // else
    //     mycfg.err.err_string = dwm_err_codes[5]; // Retrieve error string corresponding to the code
    /* --- Extract Config Info--- */
    mycfg.cfg.cfg_bytes[0] = response.response[5];
    mycfg.cfg.cfg_bytes[1] = response.response[6];
    /* --- Debug Info --- */
    #ifdef FILTERED_RESPONSE_DEBUG
    if(mycfg.err.err_code == 0x00){
        printf("\nDWM Configuration: [DWM %d]\n", DWM_NUM);
        printf("\tlow_power_en - %d\n", mycfg.cfg.cfg_vals.low_power_en);
        printf("\tloc_engine_en - %d\n", mycfg.cfg.cfg_vals.loc_engine_en);
        printf("\tled_en - %d\n", mycfg.cfg.cfg_vals.led_en);
        printf("\tble_en - %d\n", mycfg.cfg.cfg_vals.ble_en);
        printf("\tfw_update_en - %d\n", mycfg.cfg.cfg_vals.fw_update_en);
        printf("\tuwb_mode - %d\t(0 - offline, 1 - passive, 2 - active)\n", mycfg.cfg.cfg_vals.uwb_mode);
        printf("\tmode - %d\t(0 - tag, 1 - anchor)\n", mycfg.cfg.cfg_vals.mode);
        printf("\tinitiator - %d\n", mycfg.cfg.cfg_vals.initiator);
        printf("\tbridge - %d\n", mycfg.cfg.cfg_vals.bridge);
        printf("\taccel_en - %d\n", mycfg.cfg.cfg_vals.accel_en);
        printf("\tmeas_mode - %d\t(0 - TWR, 1|2|3 - reserved)\n", mycfg.cfg.cfg_vals.meas_mode);
        printf("---------------------------------------------------------------------\n");
    } else  {
        printf("[DWM %d][ERROR]: Oops, something happened when executing 'dwm_cfg_get()'", DWM_NUM); 
        // printf("\n\t-ERROR CODE: %02X\n\t-ERROR INFO: %s\n",mycfg.err.err_code,mycfg.err.err_string);
        printf("---------------------------------------------------------------------\n");

    }
    #endif // DEBUG

    return mycfg;
}

int dwm_tag_loc_get(uint8_t *buf, bool DWM_NUM){
    /* --- Decalre & Initialize transaction variables --- */
    dwm_write_tlv tlv = {0x0C, 0x00, {0x00}};
    dwm_response response;
    dwm_resp_tag_loc_get loc;
    /* --- Do the transaction --- */
    dwm_tlv(&tlv, &response, DWM_NUM);
    /* --- Error check the TLV response --- */
    loc.err.err_code = response.response[2]; // Retrieve error code of transaction
    // if(loc.err.err_code < 5)
    //     loc.err.err_string = dwm_err_codes[loc.err.err_code]; // Retrieve error string corresponding to the code
    // else
    //     loc.err.err_string = dwm_err_codes[5]; // Retrieve error string corresponding to the code
    /* --- Grab # of distances seen by Tag --- */    
    loc.num_dists = ((response.response[20]>4)? 1: response.response[20]);

    /* --- Allocate # slots of memory for each distance seen --- */    
    loc.id = (uint16_t*)malloc(sizeof(uint16_t)*NODE_LIMIT); // Allocate # of slots for distance id's
    loc.dist = (uint32_t*)malloc(sizeof(uint32_t)*NODE_LIMIT);   // Allocate # of slots for distances
    loc.qf = (uint8_t*)malloc(sizeof(uint8_t)*NODE_LIMIT);   // Allocate # of slots for quality factor of distances
    /* --- Populate the distances, their ids, & quality factor  --- */
    if((response.response[18] == 0x49) && (loc.err.err_code == 0x00)){  // If Node is a Tag
        for(uint8_t i=0; i<loc.num_dists; i++){
            // Retrieve the id from the response (little endian)
            loc.id[i] = response.response[21+(20*i)] | (response.response[22+(20*i)] << 8); 
            // Retrieve the dist from the response (little endian)
            loc.dist[i] = response.response[23+(20*i)] | (response.response[24+(20*i)] << 8) | (response.response[25+(20*i)] << 16) | (response.response[26+(20*i)] << 24); 
            // Retrieve the qf from the response (little endian)
            loc.qf[i] = response.response[27+(20*i)]; 
        }        
    }

    uint8_t mindex = 0;
    uint32_t mindist = loc.dist[mindex];
    for (uint8_t i = 0; i < loc.num_dists; i++){
        if (loc.dist[i] < mindist){ 
            mindex = i;
            mindist = loc.dist[mindex];
        }
    }
    uint16_t minid = loc.id[mindex];

    /* --- copy loc data to buffer --- */
    printf("Pre-Buf: ");
    for(uint8_t i=0; i<30; i++){
        printf("%02X, ", *(buf + i));
    }
    dists2buf(&loc, buf, minid);
    
    /* --- Debug Info --- */
    #ifdef FILTERED_RESPONSE_DEBUG
    if(loc.err.err_code == 0x00){
        printf("\n# of distances: %d\n",loc.num_dists);
        for(uint8_t i=0; i<loc.num_dists; i++){
            printf("- Dist %d:\n\tID: %d\n\tDISTANCE: %d mm\n\tQUALITY FACTOR: %d\n",(i+1),loc.id[i],loc.dist[i],loc.qf[i]);
        }
        printf("---------------------------------------------------------------------\n");
    } else  {
        free(loc.id);
        free(loc.dist);
        free(loc.qf);
        printf("\n[ERROR]: Oops, something happened when executing 'dwm_tag_loc_get()'"); 
        // printf("\n\t-ERROR CODE: %02X\n\t-ERROR INFO: %s\n",loc.err.err_code,loc.err.err_string);
        printf("\n---------------------------------------------------------------------\n");
        return -1;
    }
    #endif // DEBUG

    free(loc.id);
    free(loc.dist);
    free(loc.qf);
    return 0;
}

dwm_resp_err dwm_int_cfg(uint8_t intcfg, bool DWM_NUM){
    /* --- Decalre & Initialize transaction variables --- */
    dwm_write_tlv tlv = {0x34, 0x01, {intcfg}};
    dwm_response response;
    dwm_resp_err err;
    /* --- Do the transaction --- */
    dwm_tlv(&tlv, &response, DWM_NUM);
    /* --- Error check the TLV response --- */
    err.err_code = response.response[2]; // Retrieve error code of transaction
    // if(err.err_code < 5)
    //     err.err_string = dwm_err_codes[err.err_code]; // Retrieve error string corresponding to the code
    // else
    //     err.err_string = dwm_err_codes[5]; // Retrieve error string corresponding to the code
    /* --- Debug Info --- */
    #ifdef FILTERED_RESPONSE_DEBUG
    if(err.err_code != 0x00){
        printf("[ERROR]: Oops, something happened when executing 'dwm_int_cfg()'"); 
        // printf("\n\t-ERROR CODE: %02X\n\t-ERROR INFO: %s\n",err.err_code,err.err_string);
        printf("---------------------------------------------------------------------\n");
    }
    #endif // DEBUG
    
    return err;
}


/***************************************************************
 *              DWM RESPONSE FILTERING FUNCTIONS               *
 ***************************************************************/
void dists2buf(dwm_resp_tag_loc_get *loc, uint8_t *buf, uint16_t minid){
    // buf[0] = ((loc->num_dists>4)? 1: loc->num_dists);
    // printf("%d\n",buf[0]);
    // for(uint8_t i=0; i<buf[0]; i++){
    //     // Load the id from the struct to the buffer (little endian)
    //     buf[1+7*i] = loc->id[i];
    //     buf[2+7*i] = (loc->id[i] << 8); 
    //     // Load the dist from the struct to the buffer (little endian)
    //     buf[3+7*i] = loc->dist[i];  
    //     buf[4+7*i] = (loc->dist[i] << 8);
    //     buf[5+7*i] = (loc->dist[i] << 16);
    //     buf[6+7*i] = (loc->dist[i] << 24); 
    //     // Load the qf from the struct to the buffer (little endian)
    //     buf[7+7*i] = loc->qf[i]; 
    // }
    // printf("\n# of distances: %d\n",loc->num_dists);
    // printf("\nsize of dist: %d\n",sizeof(loc->dist)*sizeof(uint32_t));
    // memcpy(buf, &(loc->num_dists), sizeof(loc->num_dists));

    uint8_t currentsize = 0;
    // printf("\ncurrentsize: %d\n",currentsize);
    // memcpy(buf + currentsize, &(loc->err), sizeof(loc->err));
    // currentsize += sizeof(loc->err);
    // printf("\ncurrentsize: %d\n",currentsize);
    memcpy(buf + currentsize, &(loc->num_dists), sizeof(loc->num_dists));
    currentsize += sizeof(loc->num_dists);
    memcpy(buf + currentsize, &minid, sizeof(minid));
    currentsize += sizeof(minid);
    for(uint8_t i = 0;i < loc->num_dists; i++) {
        memcpy(buf + currentsize, &(loc->id[i]), sizeof(loc->id[i]));
        currentsize += sizeof(loc->id[i]);
        memcpy(buf + currentsize, &(loc->dist[i]), sizeof(loc->dist[i]));
        currentsize += sizeof(loc->dist[i]);
        memcpy(buf + currentsize, &(loc->qf[i]), sizeof(loc->qf[i]));
        currentsize += sizeof(loc->qf[i]);
    }
    printf("\ncurrentsize: %d\n",currentsize);
    
    // printf("\nPost-Buf: ");
    // for(uint8_t i=0; i<30; i++){
    //     printf("%02X, ", *(buf + i));
    // }
    // printf("\n\n");
}

// void dists2struct(dwm_resp_tag_loc_get *loc, uint8_t *buf){
//     loc->num_dists = buf[0];
//     for(uint8_t i=0; i<buf[0]; i++){
//         // Load the id from the buffer to the struct (little endian)
//         loc->id[i] = buf[1+7*i] | (buf[2+7*i] << 8);
//         // Load the dist from the buffer to the struct (little endian)
//         loc->dist[i] = buf[3+7*i] | (buf[4+7*i] << 8) | (buf[5+7*i] << 16) | (buf[6+7*i] << 24); 
//         // Load the qf from the buffer to the struct (little endian)
//         loc->qf[i] = buf[7+7*i]; 
//     }
// }