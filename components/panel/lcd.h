#ifndef _LCD_H
#define _LCD_H

#include <stdint.h>

typedef struct lcd_state_t {
	uint8_t ddram_data[80];
	uint8_t cgram_data[64];
	uint8_t address_counter;
	uint8_t display_shift;
	uint8_t display_on : 1;
	uint8_t cursor_on : 1;
	uint8_t cursor_blink : 1;
	uint8_t cursor_increase : 1;
	uint8_t display_scroll : 1;
} lcd_state_t;

void lcd_init(void);
void lcd_command(uint8_t byte);
void lcd_data(uint8_t byte);
void lcd_data_str(const uint8_t *s);
void lcd_save(lcd_state_t *state);
void lcd_restore(lcd_state_t *state);

#endif /* _LCD_H */
