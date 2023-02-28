/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <arch/arm.h>
#include <arch/ops.h>
#include <kernel/thread.h>
#include <platform/iomap.h>
#include <platform/timer.h>
#include <sys/types.h>

#include <app.h>
#include <bits.h>
#include <compiler.h>
#include <debug.h>
#include <err.h>
#include <hsusb.h>
#include <platform.h>
#include <reg.h>
#include <smem.h>
#include <stdlib.h>
#include <string.h>

#include <dev/flash.h>
#include <dev/fbcon.h>
#include <dev/udc.h>
#include <dev/keys.h>

#include <lib/ptable.h>

#include "bootimg.h"
#include "fastboot.h"
#include "recovery.h"

#define MENU_FONT_8X16

#include "ui_font.h"

#ifdef MENU_FONT_8X16
#define FONT_WIDTH		8
#define FONT_HEIGHT		16
#define FONT_PPCHAR		16
static unsigned *font = ui_font_8x16;
#endif

#define FASTBOOT_MODE   0x77665500

#define RGB565_RED		    0xf800
#define RGB565_GREEN		0x07e0
#define RGB565_BLUE		    0x001f
#define RGB565_YELLOW		0xffe0
#define RGB565_CYAN		    0x07ff
#define RGB565_MAGENTA		0xf81f
#define RGB565_WHITE		0xffff
#define RGB565_BLACK		0x0000

#define MENU_REDRAW_ALL		0x00
#define MENU_HIGHLIGHT_UP 	0x01
#define MENU_HIGHLIGHT_DOWN  0x02

#define FBCON_FOREGROUND		RGB565_BLACK
#define FBCON_BACKGROUND		RGB565_WHITE

#define KEY_ERROR 99

uint16_t keyp = KEY_ERROR;

uint16_t keys[] = {
	KEY_VOLUMEUP,
	KEY_VOLUMEDOWN,
	KEY_SOFT1,
	KEY_SEND,
	KEY_CLEAR,
	KEY_BACK,
	KEY_HOME
};

#define MENU_START_LINE 	(2)

#ifdef MENU_FONT_8X16
#define MENU_MAX_HEIGHT 	(11*16)
#endif

struct menu_item {
	char menu_title[64];
	char menu_cmd[64];
};

struct menu_item menu_main_items[] = {
	{ "  - BOOT FROM NAND"  		, "boot_nand" },
	{ "  - BOOT FROM SD CARD"  		, "boot_sbot" },
	{ "  - RECOVERY"  				, "boot_recv" },
	{ "  - PARTITION TABLE"  		, "prnt_part" },
	{ "  - CLEAR CONSOLE"  			, "prnt_clrs" },
	{ "  - REBOOT"  				, "acpu_ggwp" },
	{ "  - REBOOT BOOTLOADER"		, "acpu_bgwp" },
	{ "  - POWER OFF"  				, "acpu_pawn" },
};

static unsigned menu_chosen_item = 0;
static unsigned menu_item_number = sizeof(menu_main_items)/sizeof(struct menu_item);

static uint16_t text_color 				= RGB565_BLACK;
static uint16_t background_color 		= RGB565_WHITE;
static uint16_t highlight_color 		= RGB565_GREEN;
static uint16_t text_hightlight_color 	= RGB565_WHITE;

extern int  boot_linux_from_flash(void); // in aboot.c
extern void reboot_device(unsigned reboot_reason);  // in init.c
extern void shutdown(void); // in proc_comm.c
extern void htcleo_ptable_dump(struct ptable *ptable); // in init.c

void ui_key_listener_start(void);
int  ui_key_listener_thread(void *arg);
void ui_handle_keyup(void);
void ui_handle_keydown(void);
void ui_menu_redraw(unsigned mode);

extern unsigned boot_into_sboot; // in aboot.c
extern struct ptable flash_ptable;  // in init.c

static struct menu_item* ui_get_menu_selection()
{
	if (menu_chosen_item >= menu_item_number)
		return NULL;
	return (&menu_main_items[menu_chosen_item]);
}

static void ui_handle_command(void)
{
    char command[32];

    struct menu_item *chosen_item = ui_get_menu_selection();
    if (chosen_item == NULL) {
    	return;
    }
	strcpy(command, chosen_item->menu_cmd);

	if (!memcmp(command,"boot_recv", strlen(command)))
	{
		boot_into_sboot = 0;
        boot_into_recovery = 1;
        boot_linux_from_flash();
	}
	else if (!memcmp(command,"prnt_part", strlen(command)))
	{
		htcleo_ptable_dump(&flash_ptable);
	}
	else if (!memcmp(command,"prnt_clrs", strlen(command)))
	{
		fbcon_reset();
	}
	else if (!memcmp(command,"boot_sbot", strlen(command)))
	{
        boot_into_sboot = 1;
        boot_into_recovery = 0;
        boot_linux_from_flash();
	}
	else if (!memcmp(command,"boot_nand", strlen(command)))
	{
        boot_into_sboot = 0;
        boot_into_recovery = 0;
        boot_linux_from_flash();
	}
	else if (!memcmp(command,"acpu_ggwp", strlen(command)))
	{
        reboot_device(0);
	}
	else if (!memcmp(command,"acpu_bgwp", strlen(command)))
	{
		reboot_device(FASTBOOT_MODE);
	}
	else if (!memcmp(command,"acpu_pawn", strlen(command)))
	{
        shutdown();
	}
	else
	{
		dprintf(CRITICAL, "Unimplemented command...\n");
	}
}

void ui_menu_clear(void)
{
	struct fbcon_config* fbconfig = fbcon_display();
	unsigned bytes_per_bpp = ((fbconfig->bpp) / 8);
	unsigned count = fbconfig->width * MENU_MAX_HEIGHT;

	memset(fbconfig->base, background_color, count * bytes_per_bpp);
}

static unsigned reverse_font(unsigned x)
{
	unsigned y = 0;
	for (uint8_t i = 0; i < 9; ++i) {
		y <<= 1;
		y |= (x & 1);
		x >>= 1;
	}
	return y;
}

static void ui_draw_char(uint16_t *pixels, uint16_t paint, unsigned stride,
	    					unsigned *glyph)
{
	unsigned x, y;
	stride -= FONT_WIDTH;
	for (unsigned i = 0; i < FONT_PPCHAR; i++) {
		unsigned data = glyph[i];
		data = reverse_font(data);
		for (y = 0; y < (FONT_HEIGHT / FONT_PPCHAR); ++y) {
			for (x = 0; x < FONT_WIDTH; ++x) {
				if (data & 1)
					*pixels = paint;
				data >>= 1;
				pixels++;
			}
			pixels += stride;
		}
	}
}

static void ui_write_line(char* text, unsigned lineno, uint16_t color)
{
	struct fbcon_config* fbconfig = fbcon_display();
	uint16_t *pixels;
	unsigned bytes_per_bpp = ((fbconfig->bpp) / 8);

	if (fbconfig == NULL) {
		return;
	}
	pixels = fbconfig->base + fbconfig->width * (lineno * FONT_HEIGHT) * bytes_per_bpp;

	while(*text != 0) {
		char c = *text++;
		if(((unsigned char)c > 127) || ((unsigned char)c < 32))
			continue;
		ui_draw_char(pixels, color, fbconfig->stride, (font + (c - 32) * FONT_PPCHAR));
		pixels += FONT_WIDTH;
	}
}

static void ui_hightlight_line(unsigned lineno, uint16_t color)
{
	struct fbcon_config* fbconfig = fbcon_display();
	unsigned bytes_per_bpp = ((fbconfig->bpp) / 8);

	if (fbconfig == NULL) {
		return;
	}
	memset(fbconfig->base + fbconfig->width * (lineno * FONT_HEIGHT) * bytes_per_bpp, color,
			fbconfig->width * FONT_HEIGHT * bytes_per_bpp);
}

static void ui_draw_horizontal_line(unsigned y, uint16_t color, unsigned thickness)
{
	struct fbcon_config* fbconfig = fbcon_display();
	unsigned bytes_per_bpp = ((fbconfig->bpp) / 8);

	if (fbconfig == NULL) {
		return;
	}

	memset(fbconfig->base + fbconfig->width * y * bytes_per_bpp, color,
			fbconfig->width * thickness * bytes_per_bpp);
}

static void ui_menu_draw_item(unsigned index, unsigned highlight)
{
	if (index >= menu_item_number)
		return;

	struct menu_item *item = &menu_main_items[index];
	if (highlight) {
		ui_hightlight_line(MENU_START_LINE + index, highlight_color);
		ui_write_line(item->menu_title, MENU_START_LINE + index, text_hightlight_color);
	} else {
		ui_hightlight_line(MENU_START_LINE + index, background_color);
		ui_write_line(item->menu_title, MENU_START_LINE + index, text_color);
	}
}

void ui_menu_redraw(unsigned mode)
{
	if (mode == MENU_REDRAW_ALL) {
		ui_menu_clear();
		/* redraw the header */
		char* header = "LK2ND-" LK2ND_VERSION;
		ui_hightlight_line(0, RGB565_BLACK);
		ui_write_line(header, 0, RGB565_WHITE);
		ui_draw_horizontal_line(FONT_HEIGHT, RGB565_BLACK, 11);
		/* redraw the menu items */
		for (uint8_t i = 0; i < menu_item_number; i++)
		{
			if (i == menu_chosen_item) {
				ui_menu_draw_item(i, 1); // highlight
			} else {
				ui_menu_draw_item(i, 0); // not highlight
			}
		}
		ui_draw_horizontal_line((menu_item_number + MENU_START_LINE)*FONT_HEIGHT+3, RGB565_BLACK, 1);
	} else if (mode == MENU_HIGHLIGHT_UP) {
		if (menu_chosen_item == (menu_item_number - 1)) {
			ui_menu_draw_item(0, 0);
		} else {
			ui_menu_draw_item(menu_chosen_item + 1, 0);
		}
		ui_menu_draw_item(menu_chosen_item, 1);

	} else if (mode == MENU_HIGHLIGHT_DOWN) {
		if (menu_chosen_item == 0) {
			ui_menu_draw_item(menu_item_number - 1, 0);
		} else {
			ui_menu_draw_item(menu_chosen_item - 1, 0);
		}
		ui_menu_draw_item(menu_chosen_item, 1);
	}
}

static int ui_menu_item_down()
{
	thread_set_priority(HIGHEST_PRIORITY);

	if (menu_chosen_item == (menu_item_number - 1)) {
		menu_chosen_item = 0;
	} else {
		menu_chosen_item++;
	}

	/* redraw the menu with new highlight item */
	ui_menu_redraw(MENU_HIGHLIGHT_DOWN);

    thread_set_priority(DEFAULT_PRIORITY);
	return 0;
}

static int ui_menu_item_up()
{
	thread_set_priority(HIGHEST_PRIORITY);

	if (menu_chosen_item == 0) {
		menu_chosen_item = menu_item_number - 1;
	} else {
		menu_chosen_item--;
	}

	/* redraw the menu with new highlight item */
	ui_menu_redraw(MENU_HIGHLIGHT_UP);

	thread_set_priority(DEFAULT_PRIORITY);
	return 0;
}

static void ui_menu_init()
{
	/* redraw the menu */
	ui_menu_redraw(MENU_REDRAW_ALL);
}

void ui_handle_keydown(void)
{
	if (keyp == KEY_ERROR)
		return;

    switch (keys[keyp])
	{
        case KEY_VOLUMEUP:
			ui_menu_item_up();
			break;

        case KEY_VOLUMEDOWN:
			ui_menu_item_down();
			break;

        case KEY_SEND: // dial
            ui_handle_command();
			break;

        case KEY_CLEAR:  // hang up
        case KEY_BACK: // go back
			break;
	}
}

void ui_handle_keyup(void)
{
	if (keyp == KEY_ERROR)
		return;

    switch (keys[keyp])
	{
        case KEY_VOLUMEUP:
			break;
        case KEY_VOLUMEDOWN:
			break;
        case KEY_SEND: // dial
			break;
        case KEY_CLEAR: //hang up
			break;
        case KEY_BACK: // go back
			break;
	}
}

static int ui_key_repeater(void *arg)
{
	uint16_t last_key = keyp;
	uint8_t cnt = 0;

	for(;;)
	{
		if ((keyp == KEY_ERROR || (last_key != keyp)))
		{
			thread_exit(0);
			return 0;
		}
		else
		{
			thread_sleep(10);
			cnt++;
			if(cnt > 50) {
				cnt=0;
				break;
			}
		}
	}

	while((keyp != KEY_ERROR) && (last_key == keyp)
			&& (keys_get_state(keys[keyp])!=0)) {
		ui_handle_keydown();
		thread_sleep(100);
	}

	thread_exit(0);
	return 0;
}

int ui_key_listener_thread(void *arg)
{
	for (;;)
	{
        for(uint16_t i = 0; i < sizeof(keys)/sizeof(uint16_t); i++)
		{
			if (keys_get_state(keys[i]) != 0) {
				keyp = i;
				ui_handle_keydown();
				thread_resume(thread_create("ui_key_repeater", &ui_key_repeater, NULL, DEFAULT_PRIORITY, 4096));
				while (keys_get_state(keys[keyp]) !=0)
					thread_sleep(1);
				ui_handle_keyup();
				keyp = KEY_ERROR;
			}
		}
	}
	thread_exit(0);
	return 0;
}

void ui_key_listener_start(void)
{
	thread_resume(thread_create("key_listener", &ui_key_listener_thread, 0, LOW_PRIORITY, 4096));
}

void ui_clear_all(void)
{
	struct fbcon_config* fbconfig = fbcon_display();
	unsigned bytes_per_bpp = ((fbconfig->bpp) / 8);
	unsigned count = fbconfig->width * fbconfig->height;

	memset(fbconfig->base, background_color, count * bytes_per_bpp);
}

void init_ui(void)
{
	ui_clear_all();

	/* config fb console, it has been set up in display_init() */
	fbcon_set_colors(FBCON_BACKGROUND, FBCON_FOREGROUND);
	fbcon_set_top_margin(MENU_MAX_HEIGHT);
	fbcon_set_bottom_margin(0);
	fbcon_reset();

	/* init the menu */
	ui_menu_init();

	/* start keypad listener */
	ui_key_listener_start();

	dprintf(INFO, "init_ui()\n");
}
