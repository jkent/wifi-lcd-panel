#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "panel.h"
#include "lcd.h"

static lcd_state_t lcd_state = {0};

void lcd_init(void)
{
	lcd_command(0x38);
	vTaskDelay(10 / portTICK_PERIOD_MS);
	lcd_command(0x08);
	lcd_command(0x01);
	lcd_command(0x06);
	lcd_command(0x0C);
}

void lcd_data(uint8_t byte)
{
	if (lcd_state.address_counter >= 0x80) { /* CGRAM */
		lcd_state.cgram_data[lcd_state.address_counter & 0x3F] = byte;
		if (lcd_state.cursor_increase) {
			lcd_state.address_counter++;
		} else {
			lcd_state.address_counter--;
		}
		lcd_state.address_counter = 0x80 | (lcd_state.address_counter & 0x3F);
	} else if (lcd_state.address_counter >= 0x40) { /* DDRAM Line 2 */
		lcd_state.ddram_data[lcd_state.address_counter - 24] = byte;
		if (lcd_state.display_scroll) {
			if (lcd_state.cursor_increase) {
				lcd_state.display_shift++;
				if (lcd_state.display_shift == 40) {
					lcd_state.display_shift = 0;
				}
			} else {
				lcd_state.display_shift--;
				if (lcd_state.display_shift == 255) {
					lcd_state.display_shift = 39;
				}
			}
		}
		if (lcd_state.cursor_increase) {
			lcd_state.address_counter++;
			if (lcd_state.address_counter == 104) {
				lcd_state.address_counter = 0;
			}
		} else {
			lcd_state.address_counter--;
			if (lcd_state.address_counter == 63) {
				lcd_state.address_counter = 39;
			}
		}
	} else { /* DDRAM Line 1 */
		lcd_state.ddram_data[lcd_state.address_counter] = byte;
		if (lcd_state.display_scroll) {
			if (lcd_state.cursor_increase) {
				lcd_state.display_shift++;
				if (lcd_state.display_shift == 40) {
					lcd_state.display_shift = 0;
				}
			} else {
				lcd_state.display_shift--;
				if (lcd_state.display_shift == 255) {
					lcd_state.display_shift = 39;
				}
			}
		}
		if (lcd_state.cursor_increase) {
			lcd_state.address_counter++;
			if (lcd_state.address_counter == 40) {
				lcd_state.address_counter = 64;
			}
		} else {
			lcd_state.address_counter--;
			if (lcd_state.address_counter == 255) {
				lcd_state.address_counter = 103;
			}
		}
	}

	lcd_write(byte, false);
}

void lcd_data_str(const uint8_t *s)
{
	while (*s) {
		lcd_data(*s++);
	}
}

void lcd_command(uint8_t byte)
{
	bool delay = false;

	if (byte & 0x80) { /* DDRAM address */
		lcd_state.address_counter = byte & 0x7F;
		if (lcd_state.address_counter > 39 && lcd_state.address_counter < 64) {
			lcd_state.address_counter = 64;
		} else if (lcd_state.address_counter > 104) {
			lcd_state.address_counter = 0;
		}
	} else if (byte & 0x40) { /* CGRAM address */
		lcd_state.address_counter = 0x80 + (byte & 0x3F);
	} else if (byte & 0x20) { /* Function set */
		/* do nothing */
	} else if (byte & 0x10) { /* Cursor/display shift */
		if (byte & 0x08) { /* Shift display */
			if (byte & 0x04) { /* Shift right */
				lcd_state.display_shift--;
				if (lcd_state.display_shift == 255) {
					lcd_state.display_shift = 39;
				}
			} else { /* Shift left */
				lcd_state.display_shift++;
				if (lcd_state.display_shift == 40) {
					lcd_state.display_shift = 0;
				}
			}
		} else { /* Move cursor */
			if (byte & 0x04) { /* Move right */
				lcd_state.address_counter += 1;
			} else { /* Move left */
				lcd_state.address_counter -= 1;
			}
			if (lcd_state.address_counter > 39 && lcd_state.address_counter < 64) {
				lcd_state.address_counter = 64;
			} else if (lcd_state.address_counter > 104) {
				lcd_state.address_counter = 0;
			}
		}
	} else if (byte & 0x08) { /* Display ON/OFF control */
		lcd_state.display_on = !!(byte & 0x04);
		lcd_state.cursor_on = !!(byte & 0x02);
		lcd_state.cursor_blink = !!(byte & 0x01);
	} else if (byte & 0x04) { /* Entry mode set */
		lcd_state.cursor_increase = !!(byte & 0x02);
		lcd_state.display_scroll = !!(byte & 0x01);
	} else if (byte & 0x02) { /* Return home */
		lcd_state.address_counter = 0;
		lcd_state.display_shift = 0;
		delay = true;
	} else if (byte & 0x01) { /* Clear display */
		memset(lcd_state.ddram_data, ' ', sizeof(lcd_state.ddram_data));
		lcd_state.address_counter = 0;
		lcd_state.cursor_increase = 1;
		delay = true;
	}

	lcd_write(byte, true);
	if (delay) {
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}

void lcd_save(lcd_state_t *state)
{
	memcpy(state, &lcd_state, sizeof(lcd_state));
}

void lcd_restore(lcd_state_t *state)
{
	int i;

	memcpy(&lcd_state, state, sizeof(lcd_state));

	lcd_write(0x08, true); /* Turn off LCD */

	vTaskDelay(10 / portTICK_PERIOD_MS);

	lcd_write(0x40, true); /* CGRAM addr 0 */
	for (i = 0; i < sizeof(lcd_state.cgram_data); i++) {
		lcd_write(lcd_state.cgram_data[i], false);
	}

	lcd_write(0x80, true); /* DDRAM addr 0 */
	for (i = 0; i < sizeof(lcd_state.ddram_data); i++) {
		lcd_write(lcd_state.ddram_data[i], false);
	}

	if (lcd_state.display_shift < 20) {
		for (i = 0; i < lcd_state.display_shift; i++) {
			lcd_write(0x18, true); /* Shift display left */
		}
	} else {
		for (i = 39; i >= lcd_state.display_shift; i--) {
			lcd_write(0x1C, true); /* Shift display right */
		}
	}

	if (lcd_state.address_counter >= 128) { /* CGRAM address */
		lcd_write(0x40 | (lcd_state.address_counter & 0x3f), true);
	} else { /* DDRAM address */
		lcd_write(0x80 | lcd_state.address_counter, true);
	}

	lcd_write(0x04 | (lcd_state.cursor_increase << 1) |
			lcd_state.display_scroll, true);

	lcd_write(0x08 | lcd_state.display_on << 2 |
			lcd_state.cursor_on << 1 | lcd_state.cursor_blink, true);
}
