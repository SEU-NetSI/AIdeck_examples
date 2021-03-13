#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
 
 
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

static const char *TAG = "scan";
 
esp_err_t event_handler(void *ctx,system_event_t *event)
{
  switch (event->event_id)
  {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s",
                  ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG,"Disconnected from access point");
        break;
    default:
    break;
  }
    return ESP_OK;
}
 
static void initialise_wifi(void)
{
  wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler,NULL));
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  wifi_config_t wifi_config = {
    .sta = {
        .ssid = CONFIG_EXAMPLE_SSID,
        .password = CONFIG_EXAMPLE_PASSWORD,
    },
  };
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG,"wifi sta init");
}
 
static void scan_task(void *pvParameters)
{
  while(1)
  {
    xEventGroupWaitBits(wifi_event_group,WIFI_CONNECTED_BIT,0,1,portMAX_DELAY);
    ESP_LOGI(TAG,"WIFI CONNECT DONE");
    xEventGroupClearBits(wifi_event_group,WIFI_CONNECTED_BIT);
    wifi_ap_record_t ap_info;
    esp_wifi_sta_get_ap_info(&ap_info);
    ESP_LOGI(TAG,"SSID: %s,RSSI: %d",ap_info.ssid,ap_info.rssi);
    ESP_ERROR_CHECK(esp_wifi_stop());
    vTaskDelay(2000/portTICK_PERIOD_MS);  //2s扫描一次
    ESP_ERROR_CHECK(esp_wifi_start());
  }
}
 
 
void app_main()
{
  nvs_flash_init();
  tcpip_adapter_init();
  initialise_wifi();
  xTaskCreate(&scan_task,"scan_task",4096,NULL,5,NULL);
}
 
 