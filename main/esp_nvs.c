#include "esp_nvs.h"

/***************************************************************
 *     NODE CONFIGURATION STRUCT (NON-VOLATILE MEMORY) (NVM)   *
 ***************************************************************/
dwm_cfg_t node_cfg;

/***************************************************************
 *              ESP NON VOLATILE STORAGE FUNCTIONS             *
 ***************************************************************/
void nvs_get_node_dwm_cfg(dwm_cfg_t *nodecfg){
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    #ifdef NVS_DEBUG
    printf("\n");
    printf("Opening Non-Volatile Storage (NVS) handle... ");
    #endif // NVS_DEBUG
    nvs_handle my_handle;
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    
    // Open Successfull?
    if (err != ESP_OK) { // NO
        #ifdef NVS_DEBUG
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        #endif // NVS_DEBUG
    } else {             // YES
        nodecfg->bits = 0; // value will default to 0, if not set yet in NVS
        err = nvs_get_u64(my_handle, "dwm_cfg", &nodecfg->bits);
        switch (err) {
            case ESP_OK:
                #ifdef NVS_DEBUG
                printf("Retrieved dwm_cfg value: %llx \n", nodecfg->bits);        
                #endif // NVS_DEBUG
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                #ifdef NVS_DEBUG
                printf("The value is not initialized yet!\n");
                printf("Initializing dwm_cfg in NVS: 0x%llx ... ", nodecfg->bits);
                #endif // NVS_DEBUG
                err = nvs_set_u64(my_handle, "dwm_cfg", nodecfg->bits);
                #ifdef NVS_DEBUG
                printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
                printf("Committing updates in NVS ... ");
                #endif // NVS_DEBUG
                err = nvs_commit(my_handle);
                #ifdef NVS_DEBUG
                printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
                #endif // NVS_DEBUG
                break;
            default :
                #ifdef NVS_DEBUG
                printf("Error (%s) reading!\n", esp_err_to_name(err));
                #endif // NVS_DEBUG
        }
        // Close
        nvs_close(my_handle);
    }

}

void nvs_set_node_dwm_cfg(dwm_cfg_t *nodecfg){

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    #ifdef NVS_DEBUG
    printf("\n");
    printf("Opening Non-Volatile Storage (NVS) handle... ");
    #endif // NVS_DEBUG
    nvs_handle my_handle;
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    
    // Open Successfull?
    if (err != ESP_OK) { // NO
        #ifdef NVS_DEBUG
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        #endif // NVS_DEBUG
    } else {             // YES
        #ifdef NVS_DEBUG
        printf("Done\n");
        printf("Setting dwm_cfg in NVS: 0x%llx ... ", nodecfg->bits);
        #endif // NVS_DEBUG
        err = nvs_set_u64(my_handle, "dwm_cfg", nodecfg->bits);
        #ifdef NVS_DEBUG
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
        printf("Committing updates in NVS ... ");
        #endif // NVS_DEBUG
        err = nvs_commit(my_handle);
        #ifdef NVS_DEBUG
        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
        #endif // NVS_DEBUG
        nvs_close(my_handle);
    }

    

}