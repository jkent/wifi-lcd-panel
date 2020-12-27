#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <freertos/event_groups.h>

#include <driver/gpio.h>
#include <esp_event.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <tcpip_adapter.h>

#include "dialog.h"
#include "lcd.h"
#include "panel.h"

#include "menu.h"


#define arraysize(a) \
	(sizeof(a) / sizeof(a[0]))

typedef struct wifi_status_t {
	char status[16];
	char ip[16];
	char rssi[9];
} wifi_status_t;

static lcd_state_t lcd_state;

static wifi_status_t s_wifi_status;
static wifi_config_t s_wifi_config;
static int s_retry_num = 0;

static void show_main_dialog(void);
static void show_wifi_status_dialog(view_t *view);
static void show_wifi_config_dialog(view_t *view);

static void show_main_dialog(void)
{
	dialog_t *dialog = dialog_new();

	control_button2x_t button2x = {
		.type = CONTROL_TYPE_BUTTON2X,
		.label = "WiFi Status",
		.label2 = "WiFi Config",
		.action = show_wifi_status_dialog,
		.action2 = show_wifi_config_dialog,
	};
	dialog_append(&dialog, &button2x);
	dialog_enter(dialog);
}

static void event_handler(void* arg, esp_event_base_t event_base,
		int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
		strcpy(s_wifi_status.status, "Connecting");
		s_retry_num = 0;
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < 3) {
			esp_wifi_connect();
			s_retry_num++;
		} else {
			wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
			if (event->reason == WIFI_REASON_ASSOC_LEAVE) {
				strcpy(s_wifi_status.status, "Idle");
			} else if (event->reason == WIFI_REASON_AUTH_FAIL) {
				strcpy(s_wifi_status.status, "Wrong password");
			} else if (event->reason == WIFI_REASON_NO_AP_FOUND) {
				strcpy(s_wifi_status.status, "AP not found");
			} else {
				strcpy(s_wifi_status.status, "Connect fail");
			}
		}
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		sprintf(s_wifi_status.ip, IPSTR, IP2STR(&event->ip_info.ip));
		strcpy(s_wifi_status.status, "Connected");
		s_retry_num = 0;
	}
}

static void wifi_status_update(void)
{
	wifi_ap_record_t ap;
	esp_wifi_sta_get_ap_info(&ap);
	sprintf(s_wifi_status.rssi, "%d dBm", ap.rssi);
}

static void wifi_status_back_action(view_t *view)
{
	dialog_t *dialog = view->dialog;
	dialog_exit();
	dialog->free(dialog);
	dialog_redraw();
}

static void show_wifi_status_dialog(view_t *view)
{
	dialog_t *dialog = dialog_new();

	wifi_status_update();

	control_static_t static_ = {
		.type = CONTROL_TYPE_STATIC,
		.label = "WiFi Status:",
		.value = s_wifi_status.status,
	};
	dialog_append(&dialog, &static_);

	static_.label = "WiFi IP:";
	static_.value = s_wifi_status.ip;
	dialog_append(&dialog, &static_);

	static_.label = "WiFi RSSI:";
	static_.value = s_wifi_status.rssi;
	dialog_append(&dialog, &static_);

	control_button_t button = {
		.type = CONTROL_TYPE_BUTTON,
		.label = "Back",
		.action = wifi_status_back_action,
	};
	dialog_append(&dialog, &button);

	dialog_enter(dialog);
}

static void wifi_config_back_action(view_t *view)
{
	dialog_t *dialog = view->dialog;

	esp_wifi_disconnect();
	esp_wifi_set_config(ESP_IF_WIFI_STA, &s_wifi_config);
	esp_wifi_connect();
    strcpy(s_wifi_status.status, "Connecting");

	dialog_exit();
	dialog->free(dialog);
	dialog_redraw();
}

static void show_wifi_config_dialog(view_t *view)
{
	dialog_t *dialog = dialog_new();

	esp_wifi_get_config(ESP_IF_WIFI_STA, &s_wifi_config);

	control_text_t text = {
		.type = CONTROL_TYPE_TEXT,
		.label = "WiFi SSID:",
		.value = (char*)s_wifi_config.sta.ssid,
		.size = sizeof(s_wifi_config.sta.ssid),
	};
	dialog_append(&dialog, &text);

	text.label = "WiFi Key:";
	text.value = (char*)s_wifi_config.sta.password;
	text.size = sizeof(s_wifi_config.sta.password);
	dialog_append(&dialog, &text);

	control_button_t button = {
		.type = CONTROL_TYPE_BUTTON,
		.label = "Back",
		.action = wifi_config_back_action,
	};
	dialog_append(&dialog, &button);

	dialog_enter(dialog);
}

static xTaskHandle *main_task_handle_ptr;
static xTaskHandle menu_task_handle;

static void menu_task(void *pvParameters)
{
	while (1) {
		vTaskSuspend(NULL);
		vTaskDelay(50 / portTICK_PERIOD_MS);
		if (!gpio_get_level(0)) {
			if (dialog_active()) {
				dialog_terminate();
				lcd_restore(&lcd_state);
				if (*main_task_handle_ptr) {
					vTaskResume(*main_task_handle_ptr);
				}
			} else {
				if (*main_task_handle_ptr) {
					vTaskSuspend(*main_task_handle_ptr);
				}
				lcd_save(&lcd_state);
				show_main_dialog();
			}
		}
	}
}

static void gpio0_interrupt_handler(void *arg)
{
	xTaskResumeFromISR(menu_task_handle);
}

void menu_init(xTaskHandle *task_handle_ptr)
{
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

	main_task_handle_ptr = task_handle_ptr;

	gpio_config_t gpio_cfg = {
		.intr_type = GPIO_INTR_NEGEDGE,
		.mode = GPIO_MODE_INPUT,
		.pin_bit_mask = 1<<GPIO_NUM_0,
	};
	gpio_config(&gpio_cfg);

	memset(&s_wifi_status, 0, sizeof(s_wifi_status));
	strcpy(s_wifi_status.status, "Idle");
	strcpy(s_wifi_status.ip, "0.0.0.0");

	esp_wifi_start();

	xTaskCreate(menu_task, "menu", 384, NULL, tskIDLE_PRIORITY, &menu_task_handle);

	gpio_install_isr_service(0);
	gpio_isr_handler_add(GPIO_NUM_0, gpio0_interrupt_handler, NULL);
}
