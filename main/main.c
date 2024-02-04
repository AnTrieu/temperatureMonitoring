/* Ethernet Basic Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "cJSON.h"

#include "mqtt_client.h"
#include "ethernet_init.h"
#include "sdkconfig.h"

#define ACTIVE_ETHERNET             0
#define TAG                         "app_main"

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t           event_group;
static char                         clientID[64];
static char                         sub_topic[128];
static esp_mqtt_client_handle_t     client = NULL;

extern const uint8_t mqtt_ca_certificate_pem_start[]    asm("_binary_mqtt_ca_certificate_pem_start");
extern const uint8_t http_certificate_pem_start[]       asm("_binary_http_certificate_pem_start");

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT          BIT0
#define WIFI_FAIL_BIT               BIT1
#define MQTT_CONNECTED_BIT          BIT2
#define MQTT_SUBSCRIBED_BIT         BIT3
#define REGISTER_DONE_BIT           BIT4

#if !ACTIVE_ETHERNET
/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      "Quoc An"
#define EXAMPLE_ESP_WIFI_PASS      "0902301580"
#define EXAMPLE_ESP_MAXIMUM_RETRY  5
#endif

#if ACTIVE_ETHERNET
/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);


        sprintf(clientID, "dev_%02x_%02x_%02x_%02x_%02x_%02x",
                mac_addr[0], mac_addr[1], mac_addr[2],
                mac_addr[3], mac_addr[4], mac_addr[5]);
        sprintf(sub_topic, "%s/command", clientID);

        ESP_LOGI(TAG, "ClientID: %s", clientID);                 
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        xEventGroupClearBits(event_group, WIFI_CONNECTED_BIT);
        xEventGroupSetBits(event_group, WIFI_FAIL_BIT);
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");

    xEventGroupSetBits(event_group, WIFI_CONNECTED_BIT);
    xEventGroupClearBits(event_group, WIFI_FAIL_BIT);
}

#else

static void event_wifi_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    static int s_retry_num = 0;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupClearBits(event_group, WIFI_CONNECTED_BIT);
            xEventGroupSetBits(event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;

        // Retrieve and log MAC address
        uint8_t mac_address[6];
        esp_wifi_get_mac(ESP_IF_WIFI_STA, mac_address);


        sprintf(clientID, "dev_%02x_%02x_%02x_%02x_%02x_%02x",
                mac_address[0], mac_address[1], mac_address[2],
                mac_address[3], mac_address[4], mac_address[5]);
        sprintf(sub_topic, "%s/command", clientID);
        ESP_LOGI(TAG, "ClientID: %s", clientID);
        
        xEventGroupSetBits(event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(event_group, WIFI_FAIL_BIT);
    }
}
#endif

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static int content_length = 1;
    static int content_raw = 0;
    static int percentageK1 = 0;

    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        if (strcmp(evt->header_key, "Content-Length") == 0)
        {
            content_length = atoi(evt->header_value);
            content_raw = 0;
            percentageK1 = 0;
        }
        break;
    case HTTP_EVENT_ON_DATA:
        content_raw += evt->data_len;
        int percentage = (content_raw * 100) / content_length;
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d, progress=%d%%", evt->data_len, percentage);

        if (((percentage - percentageK1) >= 10) && ((percentage - percentageK1) < 100))
        {
            // Publish data to register device
            char payload[128] = {0};

            sprintf(payload, "{\"Type\":\"OTA\",\"clientID\":\"%s\",\"process\":\"%d%%\"}", clientID, percentage);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", esp_mqtt_client_publish(client, "data_topic", payload, strlen(payload), 0, 0));

            percentageK1 = percentage;
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}
/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        xEventGroupSetBits(event_group, MQTT_CONNECTED_BIT);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        xEventGroupSetBits(event_group, MQTT_SUBSCRIBED_BIT);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");

        if (memcmp(event->topic, sub_topic, event->topic_len) == 0)
        {
            cJSON *json = cJSON_Parse(event->data);
            cJSON *command = cJSON_GetObjectItem(json, "command");

            if (strcmp(command->valuestring, "get_data") == 0)
            {
                // Publish data to register device
                char payload[128] = {0};

                // TODO: dummy data
                float temperature = 30.12f;

                sprintf(payload, "{\"Type\":\"Response\",\"clientID\":\"%s\",\"raw_data\":\"%.2f\"}", clientID, temperature);
                ESP_LOGI(TAG, "sent publish successful, msg_id=%d", esp_mqtt_client_publish(client, "data_topic", payload, strlen(payload), 0, 0));                
            }
            else if (strcmp(command->valuestring, "register_done") == 0)
            {
                xEventGroupSetBits(event_group, REGISTER_DONE_BIT);
            }
            else if (strcmp(command->valuestring, "ota") == 0)
            {
                cJSON *link = cJSON_GetObjectItem(json, "link");

                esp_http_client_config_t config = {
                    .url                            = link->valuestring,
                    .event_handler                  = _http_event_handler,
                    .cert_pem                       = (const char *) http_certificate_pem_start,
                    .buffer_size                    = 4096,
                    .skip_cert_common_name_check    = true,
                };

                esp_https_ota_config_t ota_config = {
                    .http_config = &config,
                };

                esp_err_t err = esp_https_ota(&ota_config);
                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "OTA Begin failed (%s)", esp_err_to_name(err));
                }
                else
                {
                    ESP_LOGI(TAG, "OTA Begin succeeded");

                    // Publish data to register device
                    char payload[128] = {0};

                    sprintf(payload, "{\"Type\":\"OTA\",\"clientID\":\"%s\",\"process\":\"Restart...\"}", clientID);
                    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", esp_mqtt_client_publish(client, "data_topic", payload, strlen(payload), 0, 0));

                    vTaskDelay(1000 / portTICK_PERIOD_MS); // 1000ms
                    esp_restart();
                }
            }

            cJSON_Delete(json);
        }

        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        }
        else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        }
        else
        {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void main_task(void *pvParameter)
{
    static int8_t cnt_retry = 0;

    for(;;)
    {
        if (WIFI_CONNECTED_BIT & xEventGroupGetBits(event_group))
        {
            // Connect MQTT
            if (!(MQTT_CONNECTED_BIT & xEventGroupGetBits(event_group)))
            {
                const esp_mqtt_client_config_t mqtt_cfg = {
                    .broker.address.uri                                 = "mqtts://toantrungcloud.com:8883",
                    .broker.verification.certificate                    = (const char *) mqtt_ca_certificate_pem_start,
                    .broker.verification.skip_cert_common_name_check    = true,
                    .session.protocol_ver                               = MQTT_PROTOCOL_V_3_1_1,
                    .credentials = {                
                        .username                                       = "Device",
                        .client_id                                      = "dev_test",
                        .authentication = {                 
                            .password                                   = "Device@12345",
                        },
                    },
                    .network.disable_auto_reconnect                     = false
                };

                client = esp_mqtt_client_init(&mqtt_cfg);

                /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
                esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
                esp_mqtt_client_start(client);

                // Timeout 30s
                for (int i = 0; i < 300; i++)
                {
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    if (MQTT_CONNECTED_BIT & xEventGroupGetBits(event_group))
                    {
                        cnt_retry = 0;
                        break;
                    }
                }
            }
            else if (!(MQTT_SUBSCRIBED_BIT & xEventGroupGetBits(event_group)))
            {
                int msg_id = esp_mqtt_client_subscribe(client, sub_topic, 1);
                ESP_LOGI(TAG, "Sent subscribe successful with \"%s\" topic, msg_id=%d", sub_topic, msg_id);

                if (msg_id < 0 && cnt_retry++ >= 3)
                {
                    // Not success
                    esp_restart();
                }
                else
                {
                    // Timeout 30s
                    for (int i = 0; i < 300; i++)
                    {
                        vTaskDelay(100 / portTICK_PERIOD_MS);
                        if (MQTT_SUBSCRIBED_BIT & xEventGroupGetBits(event_group))
                        {
                            cnt_retry = 0;
                            break;
                        }
                    }
                }
            }
            else if (!(REGISTER_DONE_BIT & xEventGroupGetBits(event_group)))
            {
                // Publish data to register device
                char payload[128] = {0};

                // Delay 
                vTaskDelay(2900 / portTICK_PERIOD_MS); // 2900ms

                sprintf(payload, "{\"Type\":\"Register\",\"clientID\":\"%s\"}", clientID);
                ESP_LOGI(TAG, "sent publish successful, msg_id=%d", esp_mqtt_client_publish(client, "register_topic", payload, strlen(payload), 0, 0));
            }
        }
        else if (WIFI_FAIL_BIT & xEventGroupGetBits(event_group))
        {
            esp_restart();
        }

        vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    event_group = xEventGroupCreate();

#if ACTIVE_ETHERNET
    // Initialize Ethernet driver
    uint8_t eth_port_cnt = 0;
    esp_eth_handle_t *eth_handles;
    ESP_ERROR_CHECK(example_eth_init(&eth_handles, &eth_port_cnt));

    // Initialize TCP/IP network interface aka the esp-netif (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create instance(s) of esp-netif for Ethernet(s)
    // Use ESP_NETIF_DEFAULT_ETH when just one Ethernet interface is used and you don't need to modify
    // default esp-netif configuration parameters.
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);
    // Attach Ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));

    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    ESP_ERROR_CHECK(esp_eth_start(eth_handles[0]));
#else

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_wifi_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_wifi_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );
#endif

    // Reset buffer
    memset(clientID, 0, sizeof(clientID));

    xEventGroupClearBits(event_group, MQTT_CONNECTED_BIT | MQTT_SUBSCRIBED_BIT | REGISTER_DONE_BIT);

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    xEventGroupWaitBits(event_group,
                        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                        pdFALSE,
                        pdFALSE,
                        portMAX_DELAY);

    xTaskCreate(&main_task, "main_task", 1024 * 20, NULL, 5, NULL);
    vTaskDelete(NULL);  
}
