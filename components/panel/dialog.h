#ifndef MENU_H
#define MENU_H

#include <stdbool.h>
#include <stdint.h>
#include <lwip/ip4_addr.h>

#include "panel.h"


typedef enum control_type_t {
	CONTROL_TYPE_STATIC,
	CONTROL_TYPE_BUTTON,
	CONTROL_TYPE_BUTTON2X,
	CONTROL_TYPE_TEXT,
	CONTROL_TYPE_TOGGLE,
	CONTROL_TYPE_SELECT,
	CONTROL_TYPE_IP,
} control_type_t;

typedef struct view_t view_t;
typedef struct dialog_t dialog_t;
typedef struct control_head_t control_head_t;

typedef struct view_t {
	view_t *parent;
	button_cb_t old_button_func;
	uint8_t window_row;
	uint8_t window_row_last;
	uint8_t row;
	uint8_t col;
	uint8_t edit_offset;
	uint8_t edit_cursor;
	bool is_active;
	dialog_t *dialog;
} view_t;

typedef void (*event_cb_t)(view_t *view);

typedef struct dialog_t {
	void (*free)(dialog_t *dialog);
	uint8_t count;
	control_head_t *controls[];
} dialog_t;

typedef struct control_head_t {
	control_type_t type;
} control_head_t;

typedef struct control_static_t {
	control_type_t type;
	char *label;
	char *value;
} control_static_t;

typedef struct control_button_t {
	control_type_t type;
	char *label;
	event_cb_t action;
} control_button_t;

typedef struct control_button2x_t {
	control_type_t type;
	char *label;
	char *label2;
	event_cb_t action;
	event_cb_t action2;
} control_button2x_t;

typedef struct control_text_t {
	control_type_t type;
	char *label;
	char *value;
	uint8_t size;
	event_cb_t change;
} control_text_t;

typedef struct control_toggle_t {
	control_type_t type;
	char *label;
	const char **list;
	uint8_t size;
	uint8_t *index;
	event_cb_t change;
} control_toggle_t;

typedef struct control_select_t {
	control_type_t type;
	char *label;
	const char **list;
	uint8_t size;
	uint8_t *index;
	event_cb_t change;
} control_select_t;

typedef struct control_ip_t {
	control_type_t type;
	char *label;
	ip4_addr_t *addr;
} control_ip_t;

void dialog_redraw(void);
dialog_t *dialog_new(void);
void dialog_insert(dialog_t **dialog, const void *control, int pos);
void dialog_append(dialog_t **dialog, const void *control);
void dialog_remove(dialog_t **dialog, int pos);
void dialog_default_free(dialog_t *dialog);
void dialog_enter(dialog_t *controls);
void dialog_exit(void);
void dialog_terminate(void);
bool dialog_active(void);

#endif /* MENU_H */
