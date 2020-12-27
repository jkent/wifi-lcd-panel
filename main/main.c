#include <stdbool.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <esp_sntp.h>

#include "clock.h"
#include "lcd.h"
#include "menu.h"
#include "panel.h"


static xTaskHandle main_task_handle = NULL;

static void wifi_init()
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

void app_main(void)
{
    nvs_flash_init();
    panel_init();
    lcd_init();
    wifi_init();
    menu_init(&main_task_handle);

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    lcd_data_str((uint8_t*)"Hello world!");
    buzzer_play(440, 100);

    main_task_handle = clock_start();
}
