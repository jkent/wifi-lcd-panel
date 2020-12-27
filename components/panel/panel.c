#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/semphr.h>

#include <driver/gpio.h>
#include <driver/hw_timer.h>
#include <driver/spi.h>
#include <esp_err.h>
#include <esp8266/gpio_register.h>


#include "panel.h"


#define WDEV_NOW() REG_READ(0x3ff20c00)

#define GPIO_SPK 5
#define GPIO_SS0 15
#define GPIO_SS1 4
#define GPIO_MOSI 13
#define GPIO_MISO 12
#define GPIO_SCLK 14

static uint32_t buzzer_end;

static xSemaphoreHandle spi_lock = NULL;

static uint16_t leds = 0;
static uint8_t loop_count = 0;

static button_cb_t button_cb = NULL;
volatile uint8_t buttons = 0;
static xTimerHandle button_timer = NULL;
static uint8_t button_last_down = 0;
static bool button_first_press = false;

static uint8_t contrast = 0x1f;

static void buzzer_func(void* arg);
static uint8_t poll_buttons(void);
static void button_repeat_cb(xTimerHandle pxTimer);
static void button_led_task(void *pvParameters);

static void udelay(uint32_t t)
{
	uint32_t start = WDEV_NOW();
	while (WDEV_NOW() - start < t);
}

void panel_init(void)
{
	/* Buzzer */
    gpio_set_level(GPIO_SPK, 0);
	gpio_set_direction(GPIO_SPK, GPIO_MODE_OUTPUT);

	hw_timer_init(buzzer_func, NULL);
	hw_timer_set_clkdiv(TIMER_CLKDIV_1);
	hw_timer_set_intr_type(TIMER_EDGE_INT);
	hw_timer_set_reload(true);

	/* SPI */
    gpio_set_level(GPIO_SS0, 1);
	gpio_set_level(GPIO_SS1, 1);

	gpio_config_t config = {
		.mode = GPIO_MODE_OUTPUT,
		.pin_bit_mask = 1<<GPIO_SS0 | 1<<GPIO_SS1 | 1<<GPIO_MOSI | 1<<GPIO_SCLK,
	};
	gpio_config(&config);

	config.mode = GPIO_MODE_INPUT;
	config.pin_bit_mask = 1<<GPIO_MISO;
	gpio_config(&config);

	spi_lock = xSemaphoreCreateMutex();

	/* Buttons/LEDs */
	backlight_set(true);
	poll_buttons();
	xTaskCreate(button_led_task, "panel", 2048, NULL, 2, NULL);
	button_timer = xTimerCreate("button", 250 / portTICK_PERIOD_MS, pdTRUE, NULL, button_repeat_cb);
}

static void buzzer_func(void* arg)
{
	if ((long)(WDEV_NOW() - buzzer_end) > 0) {
		hw_timer_disarm();
		gpio_set_level(GPIO_SPK, 0);
	} else {
        gpio_set_level(GPIO_SPK, !(GPIO_REG_READ(GPIO_OUT_ADDRESS) & BIT(GPIO_SPK)));
	}
}

void buzzer_play(uint32_t frequency, uint32_t duration)
{
	buzzer_end = WDEV_NOW() + (duration * 1000);
	hw_timer_disarm();
	if (frequency == 0) {
		gpio_set_level(GPIO_SPK, 0);
	} else {
		hw_timer_alarm_us(1000000 / frequency, true);
		gpio_set_level(GPIO_SPK, 1);
	}
}

static uint8_t leds_get_raw(void)
{
	uint8_t raw = 0xff;

	for (led_t led = LED_BACKLIGHT; led <= LED_7; led++) {
		switch ((leds >> (led * 2)) & 0x03) {
		case LED_OFF:
			raw |= 1 << led;
			break;

		case LED_SLOW:
			if (loop_count & 128) {
				raw &= ~(1 << led);
			} else {
				raw |= 1 << led;
			}
			break;

		case LED_FAST:
			if (loop_count & 16) {
				raw &= ~(1 << led);
			} else {
				raw |= 1 << led;
			}
			break;

		case LED_ON:
			raw &= ~(1 << led);
			break;
		}
	}

	return raw;
}

void led_set(led_t led, led_state_t state)
{
	leds &= ~(0x03 << (led * 2));
	leds |= state << (led * 2);
}

led_state_t led_get(led_t led)
{
	return (leds >> (led * 2)) & 0x03;
}

void backlight_set(bool enabled)
{
	led_set(LED_BACKLIGHT, enabled ? LED_ON : LED_OFF);
}

void button_set_cb(button_cb_t cb)
{
	button_cb = cb;
}

button_cb_t button_get_cb()
{
	return button_cb;
}

static uint8_t poll_buttons(void)
{
	static uint8_t cnt0, cnt1;
	static bool first = true;
	uint8_t sample = 0;
	uint8_t delta, toggle;

	uint8_t leds = leds_get_raw();
	xSemaphoreTake(spi_lock, portMAX_DELAY);
	gpio_set_level(GPIO_SS1, 0);
	for (int i = 0; i < 8; i++) {
		gpio_set_level(GPIO_MOSI, leds & 0x80);
		leds <<= 1;
		gpio_set_level(GPIO_SCLK, 1);
		sample <<= 1;
		sample |= gpio_get_level(GPIO_MISO);
		gpio_set_level(GPIO_SCLK, 0);
	}
	gpio_set_level(GPIO_SS1, 1);
	xSemaphoreGive(spi_lock);

	sample >>= 3;

	if (first) {
		toggle = 0;
		buttons = sample;
		first = false;
	} else {
		/* vertical counter for debouncing */
		delta = sample ^ buttons;
		cnt1 = (cnt1 ^ cnt0) & delta;
		cnt0 = ~cnt0 & delta;
		toggle = delta & ~(cnt0 | cnt1);
		buttons ^= toggle;
	}

	return toggle;
}

static void button_led_task(void *pvParameters)
{
	uint8_t toggle;

	while (true) {
		toggle = poll_buttons();

		if (button_cb) {
			for (int n = BTN_UP; n <= BTN_ENTER; n++) {
				if (toggle & BIT(n)) {
					if (buttons & BIT(n)) {
						button_last_down = n;
						xTimerChangePeriod(button_timer, 250 / portTICK_PERIOD_MS, portMAX_DELAY);
						xTimerStart(button_timer, portMAX_DELAY);
						button_first_press = true;
					} else if (button_last_down == n) {
						xTimerStop(button_timer, portMAX_DELAY);
					}
					button_cb(n, !!(buttons & BIT(n)), WDEV_NOW());
				}
			}
		}
		vTaskDelay(10 / portTICK_PERIOD_MS);
		loop_count++;
	}
}

static void button_repeat_cb(xTimerHandle pxTimer)
{
	if (button_first_press) {
		xTimerChangePeriod(button_timer, 100 / portTICK_PERIOD_MS, portMAX_DELAY);
		button_first_press = false;
	}
	if (button_cb) {
		button_cb(button_last_down, true, WDEV_NOW());
	}
}

void lcd_write(uint8_t byte, bool command)
{
	xSemaphoreTake(spi_lock, portMAX_DELAY);
	gpio_set_level(GPIO_SS0, false);
	uint32_t data = 0x8000 | (~contrast & 0x1f) << 10 | byte;
	if (!command) {
		data |= 0x0100;
	}
	for (int i = 0; i < 16; i++) {
		gpio_set_level(GPIO_MOSI, data & 0x8000);
		data <<= 1;
		gpio_set_level(GPIO_SCLK, 1);
		udelay(2);
		gpio_set_level(GPIO_SCLK, 0);
		udelay(2);
	}
	gpio_set_level(GPIO_SS0, true);
	xSemaphoreGive(spi_lock);
}

void set_contrast(uint8_t n)
{
	contrast = n;
	xSemaphoreTake(spi_lock, portMAX_DELAY);
	gpio_set_level(GPIO_SS0, false);
	uint32_t data = 0x0000 | (~contrast & 0x1f) << 10;
	for (int i = 0; i < 16; i++) {
		gpio_set_level(GPIO_MOSI, data & 0x8000);
		data <<= 1;
		gpio_set_level(GPIO_SCLK, 1);
		udelay(2);
		gpio_set_level(GPIO_SCLK, 0);
		udelay(2);
	}
	gpio_set_level(GPIO_SS0, true);
	xSemaphoreGive(spi_lock);
}

uint8_t get_contrast(void)
{
	return contrast;
}
