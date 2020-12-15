/* 
Mesh Main
Anmol Modur
3/15/19
*/

/* --- Include Libraries --- */
#include <string.h>
// Mesh Libraries
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_mesh.h"
#include "esp_mesh_internal.h"
// #include "mesh_light.h"
#include "nvs_flash.h"
// Decawave Libraries
#include "dwm1001.h"
#include "esp_nvs.h"
#include "esp_interrupt.h"
#include "triangulation.h"
// Lwip Libraries
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "ssd1366.h"
#include "font8x8_basic.h"
/*******************************************************
 *                Macros
 *******************************************************/
//#define MESH_P2P_TOS_OFF

/*******************************************************
 *                Constants
 *******************************************************/
#define RX_SIZE (1500)
#define TX_SIZE (1460)
#define MESH_SERVER_IP "192.168.0.103"
#define MESH_PORT 3000
#define SDA_PIN GPIO_NUM_21
#define SCL_PIN GPIO_NUM_22
#define tag "SSD1306"
/*******************************************************
 *                Variable Definitions
 *******************************************************/
static const char *MESH_TAG = "mesh_main";
static const uint8_t MESH_ID[6] = {0x77, 0x77, 0x77, 0x77, 0x77, 0x77};
static uint8_t tx_buf[TX_SIZE] = {
    0,
};
static uint8_t rx_buf[RX_SIZE] = {
    0,
};
static bool is_running = true;
static bool is_mesh_connected = false;
static mesh_addr_t mesh_parent_addr;
static int mesh_layer = -1;
static mesh_addr_t mesh_root_addr;
static dwm_resp_tag_loc_get loc;
static bool gotIP = false;

/*******************************************************
 *                Function Declarations
 *******************************************************/

/*******************************************************
 *                Function Definitions
 *******************************************************/

void i2c_master_init()
{
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 1000000};
    i2c_param_config(I2C_NUM_0, &i2c_config);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

void ssd1306_init()
{
    esp_err_t espRc;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);

    i2c_master_write_byte(cmd, OLED_CMD_SET_CHARGE_PUMP, true);
    i2c_master_write_byte(cmd, 0x14, true);

    i2c_master_write_byte(cmd, OLED_CMD_SET_SEGMENT_REMAP, true); // reverse left-right mapping
    i2c_master_write_byte(cmd, OLED_CMD_SET_COM_SCAN_MODE, true); // reverse up-bottom mapping

    i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_ON, true);
    i2c_master_stop(cmd);

    espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    if (espRc == ESP_OK)
    {
        ESP_LOGI(tag, "OLED configured successfully");
    }
    else
    {
        ESP_LOGE(tag, "OLED configuration failed. code: 0x%.2X", espRc);
    }
    i2c_cmd_link_delete(cmd);
}

void task_ssd1306_display_text(const void *arg_text)
{
    char *text = (char *)arg_text;
    uint8_t text_len = strlen(text);

    i2c_cmd_handle_t cmd;

    uint8_t cur_page = 0;

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

    i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
    i2c_master_write_byte(cmd, 0x00, true); // reset column
    i2c_master_write_byte(cmd, 0x10, true);
    i2c_master_write_byte(cmd, 0xB0 | cur_page, true); // reset page

    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    for (uint8_t i = 0; i < text_len; i++)
    {
        if (text[i] == '\n')
        {
            cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

            i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
            i2c_master_write_byte(cmd, 0x00, true); // reset column
            i2c_master_write_byte(cmd, 0x10, true);
            i2c_master_write_byte(cmd, 0xB0 | ++cur_page, true); // increment page

            i2c_master_stop(cmd);
            i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
            i2c_cmd_link_delete(cmd);
        }
        else
        {
            cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

            i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
            i2c_master_write(cmd, font8x8_basic_tr[(uint8_t)text[i]], 8, true);

            i2c_master_stop(cmd);
            i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
            i2c_cmd_link_delete(cmd);
        }
    }

    vTaskDelete(NULL);
}

void display_clear()
{
    xTaskCreate(&task_ssd1306_display_text, "ssd1306_display_text", 2048,
                (void *)"                \n                \n                \n                \n                \n                \n                \n                ", 6, NULL);
}

void disp_info()
{
    char str[128] = {
        0,
    };
    display_clear();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    if (esp_mesh_is_root())
    {
        strcat(str, "MASTER NODE\n");
    }
    else
    {
        strcat(str, "\n");
    }
    strcat(str, "MESH: ");
    if (is_mesh_connected)
    {
        strcat(str, "CONNECT\n");
    }
    else
    {
        strcat(str, "NOT CONN\n");
    }
    printf("PRINTING... %s\n", str);
    xTaskCreate(&task_ssd1306_display_text, "ssd1306_display_text", 2048,
                (void *)str, 6, NULL);
}

void esp_mesh_p2p_tx_main(void *arg)
{
    int i;
    esp_err_t err;
    int send_count = 0;
    mesh_addr_t route_table[CONFIG_MESH_ROUTE_TABLE_SIZE];
    int route_table_size = 0;
    mesh_data_t data;
    data.data = tx_buf;
    data.size = sizeof(tx_buf);
    data.proto = MESH_PROTO_BIN;

    is_running = true;
    while (is_running)
    {
        // If root: gets routing table and updates local
        if (esp_mesh_is_root())
            esp_mesh_get_routing_table((mesh_addr_t *)&route_table, CONFIG_MESH_ROUTE_TABLE_SIZE * 6, &route_table_size);

        // Everyone does this
        int errr = dwm_tag_loc_get(&tx_buf, NODE_TYPE_TAG);
        printf("err = %d\n", errr);

        // if (err == -1) {
        //     printf("---Reseting Node Order---");
        //     nodeorder[0] = 0;
        //     nodeorder[1] = 0;
        //     nodeorder[2] = 0;
        //     nodeorder[3] = 0;
        // }

        // memcpy(tx_buf, "{b:'ho'}", sizeof("{b:'ho'}"));
        if (errr != -1)
        {
            err = esp_mesh_send(&mesh_root_addr, &data, MESH_DATA_P2P, NULL, 0);
            if (err)
                ESP_LOGE(MESH_TAG, "ERROR ON TRANSMIT");
        }
        vTaskDelay(1 * 100 / portTICK_RATE_MS);

        if (esp_mesh_is_root())
        {
            ESP_LOGI(MESH_TAG, "*** I AM ROOT ***");
            /* if route_table_size is less than 10, add delay to avoid watchdog in this task. */
            if (route_table_size < 10)
            {
                vTaskDelay(1 * 100 / portTICK_RATE_MS);
            }
        }
    }
    vTaskDelete(NULL);
}

void send2Server(mesh_addr_t *mAddr, mesh_data_t *mData)
{
    if (!gotIP)
        return;

    ESP_LOGE(MESH_TAG, "Send to server");
    esp_mesh_send(mAddr, mData, MESH_DATA_TODS, NULL, 0);
}

void esp_mesh_p2p_rx_main(void *arg)
{
    printf("RX START\n");
    int recv_count = 0;
    esp_err_t err;
    mesh_addr_t from;
    int send_count = 0;
    mesh_data_t data;
    int flag = 0;
    data.data = rx_buf;
    data.size = RX_SIZE;

    mesh_addr_t serverAddr;
    IP4_ADDR(&serverAddr.mip.ip4, 192, 168, 0, 103); // 192.168.0.51 is my pc
    serverAddr.mip.port = 3000;                      // Server Port

    struct sockaddr_in tcpServerAddr;
    tcpServerAddr.sin_addr.s_addr = inet_addr(MESH_SERVER_IP);
    tcpServerAddr.sin_family = AF_INET;
    tcpServerAddr.sin_port = htons(MESH_PORT);

    is_running = true;

    int s;
    while (is_running)
    {
        printf("RX PHASE\n");
        data.size = RX_SIZE;
        err = esp_mesh_recv(&from, &data, portMAX_DELAY, &flag, NULL, 0);
        if (err != ESP_OK || !data.size)
        {
            ESP_LOGE(MESH_TAG, "err:0x%x, size:%d", err, data.size);
            continue;
        }
        if (esp_mesh_is_root())
        {
            ESP_LOGI(MESH_TAG, "*** I AM ROOT ***");
            uint8_t numOfDists = data.data[0];
            uint16_t minid = data.data[1] | (data.data[2] << 8);
            // Find minid in node order
            uint8_t idloc = getnodeorder(minid);

            for (uint8_t i = 0; i < numOfDists; i++)
            {
                setDistance(idloc, data.data[3 + 7 * i] | (data.data[4 + 7 * i] << 8), data.data[5 + 7 * i] | (data.data[6 + 7 * i] << 8) | (data.data[7 + 7 * i] << 16) | (data.data[8 + 7 * i] << 24));
            }
            get_coords();
            // Print Dist Table
            printf("Distance Table\n");
            for (uint8_t i = 0; i < 4; i++)
            {
                for (uint8_t j = 0; j < 4; j++)
                {
                    printf(" %f,\t ", dist[i][j]);
                }
                printf("\n");
            }
            printf("XYZ Size = %d\n", xyz.size);
            // Print Coords
            printf("Node 1: x = %f ,y = %f, z = %f\n", xyz.n1.x, xyz.n1.y, xyz.n1.z);
            printf("Node 2: x = %f ,y = %f, z = %f\n", xyz.n2.x, xyz.n2.y, xyz.n2.z);
            printf("Node 3: x = %f ,y = %f, z = %f\n", xyz.n3.x, xyz.n3.y, xyz.n3.z);
            printf("Node 4: x = %f ,y = %f, z = %f\n", xyz.n4.x, xyz.n4.y, xyz.n4.z);

            // send2Server(&serverAddr, &guidata);
            s = socket(AF_INET, SOCK_STREAM, 0);
            connect(s, (struct sockaddr *)&tcpServerAddr, sizeof(tcpServerAddr));

            char str[110];
            sprintf(str, "%f,%f,%f;%f,%f,%f;%f,%f,%f;%f,%f,%f", xyz.n1.x, xyz.n1.y, xyz.n1.z, xyz.n2.x, xyz.n2.y, xyz.n2.z, xyz.n3.x, xyz.n3.y, xyz.n3.z, xyz.n4.x, xyz.n4.y, xyz.n4.z);

            write(s, str, strlen(str));
            close(s);
            // err = esp_mesh_send(&serverAddr, &data, MESH_DATA_TODS, NULL , 0);
            // if (err)
            //     ESP_LOGE(MESH_TAG, "ERROR ON TRANSMIT");

            // Print Node Order
            printf("\nNode Order\n");
            for (uint8_t i = 0; i < 4; i++)
            {
                printf(" %d,\t ", nodeorder[i]);
            }
            printf("\n");
        }
        disp_info();
    }
    vTaskDelete(NULL);
}

esp_err_t esp_mesh_comm_p2p_start(void)
{
    static bool is_comm_p2p_started = false;
    if (!is_comm_p2p_started)
    {
        is_comm_p2p_started = true;
        xTaskCreate(esp_mesh_p2p_tx_main, "MPTX", 3072, NULL, 5, NULL);
        xTaskCreate(esp_mesh_p2p_rx_main, "MPRX", 3072, NULL, 5, NULL);
    }
    return ESP_OK;
}

void mesh_event_handler(mesh_event_t event)
{
    mesh_addr_t id = {
        0,
    };
    static uint8_t last_layer = 0;
    ESP_LOGD(MESH_TAG, "esp_event_handler:%d", event.id);

    switch (event.id)
    {
    case MESH_EVENT_STARTED:
        esp_mesh_get_id(&id);
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_STARTED>ID:" MACSTR "", MAC2STR(id.addr));
        is_mesh_connected = false;
        mesh_layer = esp_mesh_get_layer();
        break;
    case MESH_EVENT_STOPPED:
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_STOPPED>");
        is_mesh_connected = false;
        mesh_layer = esp_mesh_get_layer();
        break;
    case MESH_EVENT_CHILD_CONNECTED:
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHILD_CONNECTED>aid:%d, " MACSTR "",
                 event.info.child_connected.aid,
                 MAC2STR(event.info.child_connected.mac));
        break;
    case MESH_EVENT_CHILD_DISCONNECTED:
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHILD_DISCONNECTED>aid:%d, " MACSTR "",
                 event.info.child_disconnected.aid,
                 MAC2STR(event.info.child_disconnected.mac));
        break;
    case MESH_EVENT_ROUTING_TABLE_ADD:
        ESP_LOGW(MESH_TAG, "<MESH_EVENT_ROUTING_TABLE_ADD>add %d, new:%d",
                 event.info.routing_table.rt_size_change,
                 event.info.routing_table.rt_size_new);
        break;
    case MESH_EVENT_ROUTING_TABLE_REMOVE:
        ESP_LOGW(MESH_TAG, "<MESH_EVENT_ROUTING_TABLE_REMOVE>remove %d, new:%d",
                 event.info.routing_table.rt_size_change,
                 event.info.routing_table.rt_size_new);
        break;
    case MESH_EVENT_NO_PARENT_FOUND:
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_NO_PARENT_FOUND>scan times:%d",
                 event.info.no_parent.scan_times);
        /* TODO handler for the failure */
        break;
    case MESH_EVENT_PARENT_CONNECTED:
        esp_mesh_get_id(&id);
        mesh_layer = event.info.connected.self_layer;
        memcpy(&mesh_parent_addr.addr, event.info.connected.connected.bssid, 6);
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_PARENT_CONNECTED>layer:%d-->%d, parent:" MACSTR "%s, ID:" MACSTR "",
                 last_layer, mesh_layer, MAC2STR(mesh_parent_addr.addr),
                 esp_mesh_is_root() ? "<ROOT>" : (mesh_layer == 2) ? "<layer2>" : "", MAC2STR(id.addr));
        last_layer = mesh_layer;
        // mesh_connected_indicator(mesh_layer);
        is_mesh_connected = true;
        if (esp_mesh_is_root())
        {
            tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA);
        }
        esp_mesh_comm_p2p_start();
        break;
    case MESH_EVENT_PARENT_DISCONNECTED:
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_PARENT_DISCONNECTED>reason:%d",
                 event.info.disconnected.reason);
        is_mesh_connected = false;
        // mesh_disconnected_indicator();
        mesh_layer = esp_mesh_get_layer();
        break;
    case MESH_EVENT_LAYER_CHANGE:
        mesh_layer = event.info.layer_change.new_layer;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_LAYER_CHANGE>layer:%d-->%d%s",
                 last_layer, mesh_layer,
                 esp_mesh_is_root() ? "<ROOT>" : (mesh_layer == 2) ? "<layer2>" : "");
        last_layer = mesh_layer;
        // mesh_connected_indicator(mesh_layer);
        break;
    case MESH_EVENT_ROOT_ADDRESS:
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_ADDRESS>root address:" MACSTR "",
                 MAC2STR(event.info.root_addr.addr));
        mesh_root_addr = event.info.root_addr;

        break;
    case MESH_EVENT_ROOT_GOT_IP:
        /* root starts to connect to server */
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_ROOT_GOT_IP>sta ip: " IPSTR ", mask: " IPSTR ", gw: " IPSTR,
                 IP2STR(&event.info.got_ip.ip_info.ip),
                 IP2STR(&event.info.got_ip.ip_info.netmask),
                 IP2STR(&event.info.got_ip.ip_info.gw));
        gotIP = true;
        break;
    case MESH_EVENT_ROOT_LOST_IP:
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_LOST_IP>");
        break;
    case MESH_EVENT_VOTE_STARTED:
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_VOTE_STARTED>attempts:%d, reason:%d, rc_addr:" MACSTR "",
                 event.info.vote_started.attempts,
                 event.info.vote_started.reason,
                 MAC2STR(event.info.vote_started.rc_addr.addr));
        break;
    case MESH_EVENT_VOTE_STOPPED:
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_VOTE_STOPPED>");
        break;
    case MESH_EVENT_ROOT_SWITCH_REQ:
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_ROOT_SWITCH_REQ>reason:%d, rc_addr:" MACSTR "",
                 event.info.switch_req.reason,
                 MAC2STR(event.info.switch_req.rc_addr.addr));
        break;
    case MESH_EVENT_ROOT_SWITCH_ACK:
        /* new root */
        mesh_layer = esp_mesh_get_layer();
        esp_mesh_get_parent_bssid(&mesh_parent_addr);
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_SWITCH_ACK>layer:%d, parent:" MACSTR "", mesh_layer, MAC2STR(mesh_parent_addr.addr));
        break;
    case MESH_EVENT_TODS_STATE:
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_TODS_REACHABLE>state:%d",
                 event.info.toDS_state);
        break;
    case MESH_EVENT_ROOT_FIXED:
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_FIXED>%s",
                 event.info.root_fixed.is_fixed ? "fixed" : "not fixed");
        break;
    case MESH_EVENT_ROOT_ASKED_YIELD:
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_ROOT_ASKED_YIELD>" MACSTR ", rssi:%d, capacity:%d",
                 MAC2STR(event.info.root_conflict.addr),
                 event.info.root_conflict.rssi,
                 event.info.root_conflict.capacity);
        break;
    case MESH_EVENT_CHANNEL_SWITCH:
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHANNEL_SWITCH>new channel:%d", event.info.channel_switch.channel);
        break;
    case MESH_EVENT_SCAN_DONE:
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_SCAN_DONE>number:%d",
                 event.info.scan_done.number);
        break;
    case MESH_EVENT_NETWORK_STATE:
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_NETWORK_STATE>is_rootless:%d",
                 event.info.network_state.is_rootless);
        break;
    case MESH_EVENT_STOP_RECONNECTION:
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_STOP_RECONNECTION>");
        break;
    case MESH_EVENT_FIND_NETWORK:
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_FIND_NETWORK>new channel:%d, router BSSID:" MACSTR "",
                 event.info.find_network.channel, MAC2STR(event.info.find_network.router_bssid));
        break;
    case MESH_EVENT_ROUTER_SWITCH:
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROUTER_SWITCH>new router:%s, channel:%d, " MACSTR "",
                 event.info.router_switch.ssid, event.info.router_switch.channel, MAC2STR(event.info.router_switch.bssid));
        break;
    default:
        ESP_LOGI(MESH_TAG, "unknown id:%d", event.id);
        break;
    }
}

void app_main(void)
{

    /*****************************************************************************/
    /* ---------- ----------       Global Initialization   ---------- ---------- */
    /*****************************************************************************/
    loc.id = (uint16_t *)malloc(sizeof(uint16_t) * NODE_LIMIT);   // Allocate # of slots for distance id's
    loc.dist = (uint32_t *)malloc(sizeof(uint32_t) * NODE_LIMIT); // Allocate # of slots for distances
    loc.qf = (uint8_t *)malloc(sizeof(uint8_t) * NODE_LIMIT);     // Allocate # of slots for quality factor of distances

    /*****************************************************************************/
    /* ---------- ----------       SPI Initialization      ---------- ---------- */
    /*****************************************************************************/
    /* --- SPI Initializaion Start --- */
    spi_init();
    // -- Add slight delay to let all systems comfortably boot up
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    /*****************************************************************************/
    /* ---------- ----------       I2C Initialization      ---------- ---------- */
    /*****************************************************************************/

    i2c_master_init();
    ssd1306_init();

    disp_info();

    /*****************************************************************************/
    /* ---------- ----------    Node Configuration Check   ---------- ---------- */
    /*****************************************************************************/
    dwm_resp_cfg_get dwm1cfg, dwm2cfg;
    dwm_resp_err err1, err2;
    bool rst = false;
    // -- Check if designated Anchor DWM is configured as desired
    dwm2cfg = dwm_cfg_get(NODE_TYPE_ANC);
    if ((dwm2cfg.cfg.cfg_bytes[0] != DWM2_CFG_BYTE0) && (dwm2cfg.cfg.cfg_bytes[1] != DWM2_CFG_BYTE1))
    {
        uint8_t anccfg_arg = 0x9A, intcfg_arg = 0x02;
        err2 = dwm_anc_set(anccfg_arg, NODE_TYPE_ANC);
        err2 = dwm_rst(NODE_TYPE_ANC);
        rst = true;
    }
    // -- Check if designated Anchor DWM is configured as desired
    dwm1cfg = dwm_cfg_get(NODE_TYPE_TAG);
    if ((dwm1cfg.cfg.cfg_bytes[0] != DWM1_CFG_BYTE0) && (dwm1cfg.cfg.cfg_bytes[1] != DWM1_CFG_BYTE1))
    {
        uint8_t tagcfg_arg[] = {0x9A, 0x00}, intcfg_arg = 0x02;
        err1 = dwm_tag_set(tagcfg_arg, NODE_TYPE_TAG);
        err1 = dwm_int_cfg(intcfg_arg, NODE_TYPE_TAG); // Does not seem to work, will ignore interrupts for now
        err1 = dwm_rst(NODE_TYPE_TAG);
        rst = true;
    }
    // -- If a DWM module had to reconfigure, we wait a few moments for it to reboot & connect to other DWMs
    if (rst == true)
        vTaskDelay(5000 / portTICK_PERIOD_MS);

    /*****************************************************************************/
    /* ---------- ----------    Mesh Initializaion Start   ---------- ---------- */
    /*****************************************************************************/
    // ESP_ERROR_CHECK(mesh_light_init());
    ESP_ERROR_CHECK(nvs_flash_init());
    /*  tcpip initialization */
    tcpip_adapter_init();
    /* for mesh
     * stop DHCP server on softAP interface by default
     * stop DHCP client on station interface by default
     * */
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
    ESP_ERROR_CHECK(tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA));
#if 0
    /* static ip settings */
    tcpip_adapter_ip_info_t sta_ip;
    sta_ip.ip.addr = ipaddr_addr("192.168.1.102");
    sta_ip.gw.addr = ipaddr_addr("192.168.1.1");
    sta_ip.netmask.addr = ipaddr_addr("255.255.255.0");
    tcpip_adapter_set_ip_info(WIFI_IF_STA, &sta_ip);
#endif
    /*  wifi initialization */
    ESP_ERROR_CHECK(esp_event_loop_init(NULL, NULL));
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_start());
    /*  mesh initialization */
    ESP_ERROR_CHECK(esp_mesh_init());
    ESP_ERROR_CHECK(esp_mesh_set_max_layer(CONFIG_MESH_MAX_LAYER));
    ESP_ERROR_CHECK(esp_mesh_set_vote_percentage(1));
    ESP_ERROR_CHECK(esp_mesh_set_ap_assoc_expire(10));
#ifdef MESH_FIX_ROOT
    ESP_ERROR_CHECK(esp_mesh_fix_root(1));
#endif
    mesh_cfg_t cfg = MESH_INIT_CONFIG_DEFAULT();
    /* mesh ID */
    memcpy((uint8_t *)&cfg.mesh_id, MESH_ID, 6);
    /* mesh event callback */
    cfg.event_cb = &mesh_event_handler;
    /* router */
    cfg.channel = CONFIG_MESH_CHANNEL;
    cfg.router.ssid_len = strlen(CONFIG_MESH_ROUTER_SSID);
    memcpy((uint8_t *)&cfg.router.ssid, CONFIG_MESH_ROUTER_SSID, cfg.router.ssid_len);
    memcpy((uint8_t *)&cfg.router.password, CONFIG_MESH_ROUTER_PASSWD,
           strlen(CONFIG_MESH_ROUTER_PASSWD));
    /* mesh softAP */
    ESP_ERROR_CHECK(esp_mesh_set_ap_authmode(CONFIG_MESH_AP_AUTHMODE));
    cfg.mesh_ap.max_connection = CONFIG_MESH_AP_CONNECTIONS;
    memcpy((uint8_t *)&cfg.mesh_ap.password, CONFIG_MESH_AP_PASSWD,
           strlen(CONFIG_MESH_AP_PASSWD));
    ESP_ERROR_CHECK(esp_mesh_set_config(&cfg));
    /* mesh start */
    ESP_ERROR_CHECK(esp_mesh_start());
    ESP_LOGI(MESH_TAG, "mesh starts successfully, heap:%d, %s\n", esp_get_free_heap_size(),
             esp_mesh_is_root_fixed() ? "root fixed" : "root not fixed");
}
