#include <stdbool.h>
#include <string.h>
#include <time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "clock.h"
#include "lcd.h"


void clock_task(void *pvParameters);

static const uint8_t cgram[] = {
	0b00000011, /* 0 */
	0b00000111,
	0b00000111,
	0b00000111,
	0b00000111,
	0b00000111,
	0b00000111,
	0b00000011,

	0b00011000, /* 1 */
	0b00011100,
	0b00011100,
	0b00011100,
	0b00011100,
	0b00011100,
	0b00011100,
	0b00011000,

	0b00011111, /* 2 */
	0b00011111,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,

	0b00000000, /* 3 */
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00011111,
	0b00011111,

	0b00011111, /* 4 */
	0b00011111,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00011111,
	0b00011111,

	0b00000001, /* 5 */
	0b00000011,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,

	0b00000000, /* 6 */
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000001,
	0b00000011,

	0b00000000, /* 7 */
	0b00000000,
	0b00000000,
	0b00001110,
	0b00001110,
	0b00000000,
	0b00000000,
	0b00000000,
};

struct digitmap_t {
	uint8_t top[3];
	uint8_t bottom[3];
};

static const struct digitmap_t digitmap[] = {
	{ /* 0 */
		{0, 2, 1},
		{0, 3, 1}
	},
	{ /* 1 */
		{32, 0, 32},
		{32, 0, 32}
	},
	{ /* 2 */
		{5, 4, 1},
		{0, 3, 32}
	},
	{ /* 3 */
		{5, 4, 1},
		{6, 3, 1},
	},
	{ /* 4 */
		{0, 3, 1},
		{32, 32, 1},
	},
	{ /* 5 */
		{0, 4, 32},
		{6, 3, 1},
	},
	{ /* 6 */
		{0, 4, 32},
		{0, 3, 1},
	},
	{ /* 7 */
		{5, 2, 1},
		{32, 0, 32},
	},
	{ /* 8 */
		{0, 4, 1},
		{0, 3, 1},
	},
	{ /* 9 */
		{0, 4, 1},
		{6, 3, 1},
	},
	{ /* 10 - blank */
		{32, 32, 32},
		{32, 32, 32},
	}
};

const char *wday[] = {
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday",
};

const char *mon[] = {
	"January",
	"Feburary",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December"
};

xTaskHandle clock_start(void)
{
	xTaskHandle task;

	setenv("TZ", "CST6CDT", 1);

	xTaskCreate(clock_task, "clock", 1536, NULL, tskIDLE_PRIORITY, &task);

	return task;
}

static void draw_big_digit(uint8_t n, uint8_t pos)
{
	if (n > 10 || pos > 36) {
		return;
	}

	lcd_command(0x80 + pos);
	for (int i = 0; i < 3; i++) {
		lcd_data(digitmap[n].top[i]);
	}
	lcd_command(0xC0 + pos);
	for (int i = 0; i < 3; i++) {
		lcd_data(digitmap[n].bottom[i]);
	}
}

static void draw_big_colon(uint8_t pos, bool visible)
{
	if (pos > 39) {
		return;
	}

	lcd_command(0x80 + pos);
	lcd_data(visible ? '\x07' : ' ');
	lcd_command(0xC0 + pos);
	lcd_data(visible ? '\x07' : ' ');
}

static void draw_big_time(struct tm *tm, uint8_t pos, bool colon_visible)
{
	const bool millitary_time = false;

	if (colon_visible) {
		if (millitary_time) {
			draw_big_digit(tm->tm_hour / 10, pos);
			draw_big_digit(tm->tm_hour % 10, pos + 3);
		} else {
			uint8_t hour = tm->tm_hour % 12;
			if (hour == 0) {
				draw_big_digit(1, pos);
				draw_big_digit(2, pos + 3);
			} else {
				draw_big_digit(hour / 10 == 0 ? 10 : hour / 10, pos);
				draw_big_digit(hour % 10, pos + 3);
			}

			lcd_command(0xC0 + pos + 13);
			if (tm->tm_hour < 12) {
				lcd_data_str((const uint8_t*)"am");
			} else {
				lcd_data_str((const uint8_t*)"pm");
			}
		}

		draw_big_digit(tm->tm_min / 10, pos + 7);
		draw_big_digit(tm->tm_min % 10, pos + 10);
	}

	draw_big_colon(pos + 6, colon_visible);
}

static void draw_date(struct tm *tm, uint8_t pos)
{
	char mday[3];

	lcd_command(0x80 + pos);
	lcd_data_str((const uint8_t*)wday[tm->tm_wday]);
	lcd_data(',');
	lcd_data(' ');
	lcd_data_str((const uint8_t*)mon[tm->tm_mon]);
	lcd_data(' ');

	sprintf(mday, "%d", tm->tm_mday);
	lcd_data_str((const uint8_t*)mday);

	int n = strlen(wday[tm->tm_wday]) + 2 + strlen(mon[tm->tm_mon]) + 1 + strlen(mday);
	while (n++ < 23) {
		lcd_data(' ');
	}
}

void clock_task(void *pvParameters)
{
	time_t ts;
	static struct tm *tm;

	lcd_command(0x01); /* Clear display */
	lcd_command(0x02); /* Return home */
	lcd_command(0x40); /* CGRAM addr 0 */

	for (int i = 0; i < sizeof(cgram); i++) {
		lcd_data(cgram[i]);
	}

	portTickType xLastWakeTime = xTaskGetTickCount();

	while (true) {
		time(&ts);
		setenv("TZ", "CST6CDT", 1);
		tm = localtime(&ts);

		draw_big_time(tm, 0, true);
		draw_date(tm, 16);

		vTaskDelayUntil(&xLastWakeTime, 500 / portTICK_PERIOD_MS);
		draw_big_time(tm, 0, false);

		vTaskDelayUntil(&xLastWakeTime, 500 / portTICK_PERIOD_MS);

	}
}
