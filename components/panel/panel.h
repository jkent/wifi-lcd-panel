#ifndef PANEL_H
#define PANEL_H

#include <stdbool.h>
#include <stdint.h>


#define PANEL_LCD_ROWS 2
#define PANEL_LCD_COLS 40

typedef enum {LED_BACKLIGHT, LED_1, LED_2, LED_3, LED_4, LED_5, LED_6, LED_7} led_t;
typedef enum {LED_OFF, LED_SLOW, LED_FAST, LED_ON} led_state_t;
typedef enum {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_ENTER} button_t;
typedef void (*button_cb_t)(button_t button, bool down, uint32_t time);

void panel_init(void);

void buzzer_play(uint32_t frequency, uint32_t duration);

void led_set(led_t led, led_state_t state);
led_state_t led_get(led_t led);
void backlight_set(bool enabled);

void button_set_cb(button_cb_t);
button_cb_t button_get_cb(void);

void lcd_write(uint8_t byte, bool command);
void set_contrast(uint8_t contrast);
uint8_t get_contrast(void);

#endif /* PANEL_H */
