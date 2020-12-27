#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "panel.h"
#include "dialog.h"
#include "lcd.h"

#define max(a,b) \
	({ __typeof__ (a) _a = (a); \
	  __typeof__ (b) _b = (b); \
	  _a > _b ? _a : _b; })

#define min(a,b) \
	({ __typeof__ (a) _a = (a); \
	  __typeof__ (b) _b = (b); \
	  _a < _b ? _a : _b; })

static view_t *view = NULL;

static void dialog_draw(void);
static void dialog_button_func(button_t button, bool down, uint32_t time);
static int dialog_find_control(button_t button);
static void trim_inplace(char *s);

static void dialog_button_func(button_t button, _Bool down, uint32_t time)
{
	control_head_t* control = view->dialog->controls[view->row];
	uint8_t len;
	if (!down) {
		return;
	}

	if (!view->is_active) {
		if (button == BTN_ENTER) {
			switch (control->type) {
			case CONTROL_TYPE_BUTTON: {
				control_button_t *button = (control_button_t *)control;
				if (button->action) {
					button->action(view);
				}
				break;
			}

			case CONTROL_TYPE_BUTTON2X: {
				control_button2x_t *button2x = (control_button2x_t *)control;
				if (view->col == 0) {
					if (button2x->action) {
						button2x->action(view);
					}
				} else if (view->col == 1) {
					if (button2x->action2) {
						button2x->action2(view);
					}
				}
				break;
			}

			case CONTROL_TYPE_TEXT: {
				control_text_t *text = (control_text_t *)control;
				len = strlen(text->value);
				uint8_t width = PANEL_LCD_COLS / 2;
				view->edit_cursor =
						len < text->size - 1 ? len - 1 : text->size - 2;
				if (view->edit_cursor == 0xFF) {
					view->edit_cursor = 0;
				}
				view->edit_offset =	view->edit_cursor >= width ?
								view->edit_cursor - width + 1 : 0;
				view->is_active = true;
				dialog_draw();
				break;
			}

			case CONTROL_TYPE_TOGGLE: {
				control_toggle_t *toggle = (control_toggle_t *)control;
				*toggle->index += 1;
				if (*toggle->index >= toggle->size) {
					*toggle->index = 0;
				}
				if (toggle->change) {
					toggle->change(view);
				}
				dialog_draw();
				break;
			}

			case CONTROL_TYPE_SELECT:
				view->is_active = true;
				dialog_draw();
				break;

			case CONTROL_TYPE_IP:
				view->is_active = true;
				view->edit_cursor = 0;
				dialog_draw();
				break;

			default:
				break;
			}
		} else {
			int i = dialog_find_control(button);
			if (i >= 0) {
				view->row = i & 0xFFFF;
				view->col = (i >> 16) & 0xFFFF;

				if (view->row < view->window_row) {
					view->window_row = view->row;
				} else if (view->row >= view->window_row + (PANEL_LCD_ROWS - 1)) {
					view->window_row = max(0, view->row - (PANEL_LCD_ROWS - 1));
				}
				dialog_draw();
			}
		}
		return;
	}

	switch (button) {
	case BTN_UP:
		if (control->type == CONTROL_TYPE_TEXT) {
			control_text_t *text = (control_text_t *) control;
			switch (text->value[view->edit_cursor]) {
			case '\0':
			case ' ':
				text->value[view->edit_cursor] = 'a';
				break;

			case 'z':
				text->value[view->edit_cursor] = 'A';
				break;

			case 'Z':
				text->value[view->edit_cursor] = '0';
				break;

			case '9':
				text->value[view->edit_cursor] = '!';
				break;

			case '.':
				text->value[view->edit_cursor] = ':';
				break;

			case '@':
				text->value[view->edit_cursor] = '[';
				break;

			case '_':
				text->value[view->edit_cursor] = '{';
				break;

			case '~':
				text->value[view->edit_cursor] = ' ';
				break;

			default:
				text->value[view->edit_cursor] += 1;
			}
			dialog_draw();
		} else if (control->type == CONTROL_TYPE_SELECT) {
			control_select_t *select = (control_select_t *) control;
			if (*select->index > 0) {
				*select->index -= 1;
				dialog_draw();
			}
		} else if (control->type == CONTROL_TYPE_IP) {
			control_ip_t *ip = (control_ip_t *)control;
			int octet = ip->addr->addr >> (24 - (view->edit_cursor / 3 * 8)) & 0xFF;
			int digit = view->edit_cursor % 3;

			if (digit == 0) {
				octet += 100;
			} else if (digit == 1) {
				octet += 10;
			} else if (digit == 2) {
				octet += 1;
			}

			if (octet > 255) {
				octet = 255;
			}
			ip->addr->addr &= ~(0xFF << (24 - (view->edit_cursor / 3 * 8)));
			ip->addr->addr |= octet << (24 - (view->edit_cursor / 3 * 8));
			dialog_draw();
		}
		break;

	case BTN_DOWN:
		if (control->type == CONTROL_TYPE_TEXT) {
			control_text_t *text = (control_text_t *) control;
			switch (text->value[view->edit_cursor]) {
			case 'a':
				text->value[view->edit_cursor] = ' ';
				break;

			case 'A':
				text->value[view->edit_cursor] = 'z';
				break;

			case '0':
				text->value[view->edit_cursor] = 'Z';
				break;

			case '!':
				text->value[view->edit_cursor] = '9';
				break;

			case ':':
				text->value[view->edit_cursor] = '.';
				break;

			case '[':
				text->value[view->edit_cursor] = '@';
				break;

			case '{':
				text->value[view->edit_cursor] = '_';
				break;

			case '\0':
			case ' ':
				text->value[view->edit_cursor] = '~';
				break;

			default:
				text->value[view->edit_cursor] -= 1;
			}
			dialog_draw();
		} else if (control->type == CONTROL_TYPE_SELECT) {
			control_select_t *select = (control_select_t *)control;
			if (*select->index < select->size - 1) {
				*select->index += 1;
				dialog_draw();
			}
		} else if (control->type == CONTROL_TYPE_IP) {
			control_ip_t *ip = (control_ip_t *)control;
			int octet = ip->addr->addr >> (24 - (view->edit_cursor / 3 * 8)) & 0xFF;
			int digit = view->edit_cursor % 3;

			if (digit == 0) {
				octet -= 100;
			} else if (digit == 1) {
				octet -= 10;
			} else if (digit == 2) {
				octet -= 1;
			}

			if (octet < 0) {
				octet = 0;
			}
			ip->addr->addr &= ~(0xFF << (24 - (view->edit_cursor / 3 * 8)));
			ip->addr->addr |= octet << (24 - (view->edit_cursor / 3 * 8));
			dialog_draw();
		}
		break;

	case BTN_LEFT:
		if (control->type == CONTROL_TYPE_TEXT) {
			if (view->edit_cursor > 0) {
				view->edit_cursor -= 1;
				if (view->edit_offset > 0
						&& view->edit_cursor < view->edit_offset + 1) {
					view->edit_offset -= 1;
				}
				dialog_draw();
			}
		} else if (control->type == CONTROL_TYPE_IP) {
			if (view->edit_cursor > 0) {
				view->edit_cursor -= 1;
				dialog_draw();
			}
		}
		break;

	case BTN_RIGHT:
		if (control->type == CONTROL_TYPE_TEXT) {
			control_text_t *text = (control_text_t *) control;
			uint8_t len = strlen(text->value);
			uint8_t width = PANEL_LCD_COLS / 2;
			if (view->edit_cursor >= text->size - 2) {
				break;
			}
			if (view->edit_cursor <= len) {
				view->edit_cursor += 1;
				if (view->edit_cursor > view->edit_offset + width - 1
						|| (view->edit_cursor < len - 1
								&& view->edit_cursor
										> view->edit_offset + width - 2)) {
					view->edit_offset += 1;
				}

				if (view->edit_cursor == len) {
					text->value[view->edit_cursor] = ' ';
					text->value[view->edit_cursor + 1] = '\0';
				}
				dialog_draw();
			}
		} else if (control->type == CONTROL_TYPE_IP) {
			if (view->edit_cursor < 11) {
				view->edit_cursor += 1;
				dialog_draw();
			}
		}
		break;

	case BTN_ENTER:
		lcd_command(0x0C); /* Hide cursor */
		view->window_row_last = -1;
		view->is_active = false;
		if (control->type == CONTROL_TYPE_TEXT) {
			control_text_t *text = (control_text_t *)control;
			trim_inplace(text->value);
			if (text->change) {
				text->change(view);
			}
		} else if (control->type == CONTROL_TYPE_SELECT) {
			control_select_t *select = (control_select_t *)control;
			if (select->change) {
				select->change(view);
			}
		}
		dialog_draw();
		break;
	}
}

static void dialog_field(const char *s, int field_len)
{
	while (*s && field_len-- > 0) {
		lcd_data(*s++);
	}
	while (field_len-- > 0) {
		lcd_data(' ');
	}
}

static int get_cols(control_head_t *control)
{
	switch (control->type) {
	case CONTROL_TYPE_BUTTON2X:
		return 2;

	default:
		return 1;
	}
}

static int dialog_find_control(button_t button)
{
	dialog_t *dialog = view->dialog;
	int row = view->row;
	int col = view->col;
	int cols;

	switch (button) {
	case BTN_UP:
		if (view->row > 0) {
			row = view->row - 1;
			cols = get_cols(dialog->controls[row]);
			col = min(view->col, cols - 1);
		}
		break;

	case BTN_DOWN:
		if (view->row < dialog->count - 1) {
			row = view->row + 1;
			cols = get_cols(dialog->controls[row]);
			col = min(view->col, cols - 1);
		}
		break;

	case BTN_LEFT:
		col = max(view->col - 1, 0);
		break;

	case BTN_RIGHT:
		cols = get_cols(dialog->controls[row]);
		col = min(view->col + 1, cols - 1);
		break;

	default:
		break;
	}

	return row | (col << 16);
}

static void dialog_draw_static(int row)
{
	control_static_t *static_ = (control_static_t *)view->dialog->controls[row];
	int width = PANEL_LCD_COLS / 2;

	if (row == view->row) {
		lcd_data('\x01');
	} else {
		lcd_data(' ');
	}
	dialog_field(static_->label, width - 1);
	dialog_field(static_->value, width);
}

static void dialog_draw_button(int row)
{
	control_button_t *button = (control_button_t *)view->dialog->controls[row];
	int width = PANEL_LCD_COLS;

	if (row == view->row) {
		lcd_data('>');
	} else {
		lcd_data(' ');
	}
	dialog_field(button->label, width - 1);
}

static void dialog_draw_button2x(int row)
{
	control_button2x_t *button2x = (control_button2x_t *)view->dialog->controls[row];
	int width = PANEL_LCD_COLS / 2;

	if (row == view->row && view->col == 0) {
		lcd_data('\x01');
	} else {
		lcd_data(' ');
	}
	dialog_field(button2x->label, width - 1);

	if (row == view->row && view->col == 1) {
		lcd_data('\x01');
	} else {
		lcd_data(' ');
	}
	dialog_field(button2x->label2, width - 1);
}

static void dialog_draw_text(int row)
{
	control_text_t *text = (control_text_t *)view->dialog->controls[row];
	int width = PANEL_LCD_COLS / 2;

	if (!view->is_active) {
		if (row == view->row) {
			lcd_data('\x01');
		} else {
			lcd_data(' ');
		}
		dialog_field(text->label, width - 1);
		dialog_field(text->value, width);
	} else {
		int lcd_row = view->row - view->window_row;
		lcd_command(0x80 | (lcd_row << 6) | PANEL_LCD_COLS / 2);
		int offset = 0;
		int right_arrow = 0;
		if (view->edit_offset > 0) {
			width -= 1;
			offset += 1;
			lcd_data('\x00');
		}
		if (strlen(text->value) > view->edit_offset + width + offset) {
			width -= 1;
			right_arrow = 1;
		}
		dialog_field(text->value + view->edit_offset + offset, width);
		if (right_arrow) {
			lcd_data('\x01');
		}
		lcd_command(0x80 | (lcd_row << 6) | (PANEL_LCD_COLS / 2 + view->edit_cursor - view->edit_offset));
		lcd_command(0x0E); /* Show cursor */
	}
}

static void dialog_draw_toggle(int row)
{
	control_toggle_t *toggle = (control_toggle_t *)view->dialog->controls[row];
	int width = PANEL_LCD_COLS;

	if (row == view->row) {
		lcd_data('\x01');
	} else {
		lcd_data(' ');
	}
	width /= 2;
	dialog_field(toggle->label, width - 1);
	dialog_field(toggle->list[*toggle->index], width);
}

static void dialog_draw_select(int row)
{
	control_select_t *select = (control_select_t *)view->dialog->controls[row];
	int width = PANEL_LCD_COLS;

	if (!view->is_active) {
		if (row == view->row) {
			lcd_data('\x01');
		} else {
			lcd_data(' ');
		}
		width /= 2;
		dialog_field(select->label, width - 1);
		dialog_field(select->list[*select->index], width - 1);
	} else {
		int lcd_row = view->row - view->window_row;
		lcd_command(0x80 | (lcd_row << 6) | PANEL_LCD_COLS / 2);
		if (*select->index == 0) {
			lcd_data('\x03');
		} else if (*select->index == select->size - 1){
			lcd_data('\x02');
		} else {
			lcd_data('\x04');
		}
		const char *s = select->list[*select->index];
		dialog_field(s, PANEL_LCD_COLS / 2 - 1);
	}
}

static void dialog_draw_ip(int row)
{
	control_ip_t *ip = (control_ip_t *)view->dialog->controls[row];
	uint32_t addr = ip->addr->addr;
	int width = PANEL_LCD_COLS;
	char tmp[16];

	if (!view->is_active) {
		if (row == view->row) {
			lcd_data('\x01');
		} else {
			lcd_data(' ');
		}
		width /= 2;
		dialog_field(ip->label, (PANEL_LCD_COLS / 2) - 1);
		sprintf(tmp, "%3hhu.%3hhu.%3hhu.%3hhu", (addr & 0xFF000000) >> 24,
				(addr & 0x00FF0000) >> 16, (addr & 0x0000FF00) >> 8,
				(addr & 0x000000FF) >> 0);
		dialog_field(tmp, PANEL_LCD_COLS / 2);
	} else {
		int lcd_row = view->row - view->window_row;
		lcd_command(0x80 | (lcd_row << 6) | PANEL_LCD_COLS / 2);
		sprintf(tmp, "%3hhu.%3hhu.%3hhu.%3hhu", (addr & 0xFF000000) >> 24,
				(addr & 0x00FF0000) >> 16, (addr & 0x0000FF00) >> 8,
				(addr & 0x000000FF) >> 0);
		dialog_field(tmp, PANEL_LCD_COLS / 2);

		int col = PANEL_LCD_COLS / 2 + view->edit_cursor;
		if (view->edit_cursor >= 9) {
			col += 3;
		} else if (view->edit_cursor >= 6) {
			col += 2;
		} else if (view->edit_cursor >= 3) {
			col += 1;
		}
		lcd_command(0x80 | (lcd_row << 6) | col);
		lcd_command(0x0E); /* Show cursor */
	}
}

static void dialog_draw(void)
{
	control_head_t *control = view->dialog->controls[view->row];

	if (view->is_active) {
		int lcd_row = view->row - view->window_row;

		lcd_command(0x80 | (lcd_row << 6));
		lcd_data(' ');

		switch (control->type) {
		case CONTROL_TYPE_TEXT:
			dialog_draw_text(view->row);
			break;

		case CONTROL_TYPE_SELECT:
			dialog_draw_select(view->row);
			break;

		case CONTROL_TYPE_IP:
			dialog_draw_ip(view->row);
			break;

		default:
			break;
		}
		return;
	}

	if (view->window_row != view->window_row_last) {
		lcd_command(0x01); /* Clear display */
	}

	for (int row = view->window_row; row < min(view->dialog->count, view->window_row + PANEL_LCD_ROWS); row++) {
		control_head_t *control = view->dialog->controls[row];
		lcd_command(0x80 | ((row - view->window_row) << 6));
		switch (control->type) {
		case CONTROL_TYPE_STATIC:
			dialog_draw_static(row);
			break;

		case CONTROL_TYPE_BUTTON:
			dialog_draw_button(row);
			break;

		case CONTROL_TYPE_BUTTON2X:
			dialog_draw_button2x(row);
			break;

		case CONTROL_TYPE_TEXT:
			dialog_draw_text(row);
			break;

		case CONTROL_TYPE_TOGGLE:
			dialog_draw_toggle(row);
			break;

		case CONTROL_TYPE_SELECT:
			dialog_draw_select(row);
			break;

		case CONTROL_TYPE_IP:
			dialog_draw_ip(row);
			break;
		}
	}

	view->window_row_last = view->window_row;
}

void dialog_redraw(void)
{
	if (view) {
		view->window_row_last = -1;
		dialog_draw();
	}
}

dialog_t *dialog_new(void)
{
	dialog_t *dialog = malloc(sizeof(dialog_t));
	dialog->count = 0;
	dialog->free = dialog_default_free;
	return dialog;
}

void dialog_insert(dialog_t **dialog, const void *control, int pos)
{
	int count = (*dialog)->count;
	if (pos < 0) {
		pos = count + pos;
	}
	int newsize = sizeof(dialog_t) + sizeof(void *) * (count + 1);
	*dialog = realloc(*dialog, newsize);
	if (pos < count) {
		memmove((*dialog)->controls + pos + 1, (*dialog)->controls + pos, sizeof(void *) * (count - pos));
	}

	int control_size;

	switch (((control_head_t *)control)->type) {
	case CONTROL_TYPE_STATIC:
		control_size = sizeof(control_static_t);
		break;
	case CONTROL_TYPE_BUTTON:
		control_size = sizeof(control_button_t);
		break;
	case CONTROL_TYPE_BUTTON2X:
		control_size = sizeof(control_button2x_t);
		break;
	case CONTROL_TYPE_TEXT:
		control_size = sizeof(control_text_t);
		break;
	case CONTROL_TYPE_TOGGLE:
		control_size = sizeof(control_toggle_t);
		break;
	case CONTROL_TYPE_SELECT:
		control_size = sizeof(control_select_t);
		break;
	case CONTROL_TYPE_IP:
		control_size = sizeof(control_ip_t);
		break;
	default:
		/* TODO: panic! */
		control_size = sizeof(control_head_t);
	}

	control_head_t *new_control = malloc(control_size);
	memcpy(new_control, control, control_size);
	(*dialog)->controls[pos] = new_control;
	(*dialog)->count = count + 1;
}

void dialog_append(dialog_t **dialog, const void *control)
{
	dialog_insert(dialog, control, (*dialog)->count);
}

void dialog_remove(dialog_t **dialog, int pos)
{
	if (pos < 0) {
		pos = (*dialog)->count + pos;
	}

	free((*dialog)->controls[pos]);
	if (pos < (*dialog)->count - 1) {
		memmove((*dialog)->controls + pos, (*dialog)->controls + pos + 1,
				sizeof(void *) * ((*dialog)->count - pos - 1));
	}
	(*dialog)->count--;
}

void dialog_default_free(dialog_t *dialog)
{
	for (int i = 0; i < dialog->count; i++) {
		free(dialog->controls[i]);
	}
	free(dialog);
}

void dialog_enter(dialog_t *dialog)
{
	view_t *new_view = malloc(sizeof(view_t));
	bzero(new_view, sizeof(view_t));

	new_view->parent = view;
	view = new_view;
	view->old_button_func = button_get_cb();
	view->window_row_last = -1;
	view->dialog = dialog;
	view->row = 0;

	button_set_cb(dialog_button_func);

	if (!view->parent) {
		lcd_command(0x40); /* Set CGRAM address */

		/* 0 */
		lcd_data(0b00000010);
		lcd_data(0b00000110);
		lcd_data(0b00001110);
		lcd_data(0b00011110);
		lcd_data(0b00001110);
		lcd_data(0b00000110);
		lcd_data(0b00000010);
		lcd_data(0b00000000);

		/* 1 */
		lcd_data(0b00001000);
		lcd_data(0b00001100);
		lcd_data(0b00001110);
		lcd_data(0b00001111);
		lcd_data(0b00001110);
		lcd_data(0b00001100);
		lcd_data(0b00001000);
		lcd_data(0b00000000);

		/* 2 */
		lcd_data(0b00000100);
		lcd_data(0b00001110);
		lcd_data(0b00011111);
		lcd_data(0b00000000);
		lcd_data(0b00000000);
		lcd_data(0b00000000);
		lcd_data(0b00000000);
		lcd_data(0b00000000);

		/* 3 */
		lcd_data(0b00000000);
		lcd_data(0b00000000);
		lcd_data(0b00000000);
		lcd_data(0b00000000);
		lcd_data(0b00011111);
		lcd_data(0b00001110);
		lcd_data(0b00000100);
		lcd_data(0b00000000);

		/* 4 */
		lcd_data(0b00000100);
		lcd_data(0b00001110);
		lcd_data(0b00011111);
		lcd_data(0b00000000);
		lcd_data(0b00011111);
		lcd_data(0b00001110);
		lcd_data(0b00000100);
		lcd_data(0b00000000);
	}
	dialog_draw();
}

void dialog_exit()
{
	view_t *old_view = view;
	button_set_cb(view->old_button_func);
	view = view->parent;
	free(old_view);
}

void dialog_terminate(void)
{
	while (view) {
		dialog_t *dialog = view->dialog;
		dialog->free(dialog);
		dialog_exit();
	}
}

bool dialog_active(void)
{
	return !!view;
}

static void trim_inplace(char *s)
{
        int i;
        int begin = 0;
        int end = strlen(s) - 1;

        while (isspace(s[begin])) {
                begin += 1;
        }

        while ((end >= begin) && isspace(s[end])) {
                end -= 1;
        }

        for (i = begin; i <= end; i++) {
                s[i - begin] = s[i];
        }

        s[i - begin] = '\0';
}
