/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "../../../../drivers/video/msm/msm_fb.h"
#include "../../../../drivers/video/msm/mipi_dsi.h"
#include "../../../../drivers/video/msm/mdp4.h"

#include <mach/gpio.h>
#include <mach/panel_id.h>
#include <mach/msm_bus_board.h>
#include <linux/mfd/pmic8058.h>
#include <linux/pwm.h>
#include <linux/pmic8058-pwm.h>
#include <mach/debug_display.h>

#include "../devices.h"
#include "../board-ruby.h"

static int mipi_dsi_panel_power(const int on);
static int __devinit ruby_lcd_probe(struct platform_device *pdev);
static void ruby_lcd_shutdown(struct platform_device *pdev);
static int ruby_lcd_on(struct platform_device *pdev);
static int ruby_lcd_off(struct platform_device *pdev);
static void ruby_set_backlight(struct msm_fb_data_type *mfd);
static void ruby_display_on(struct msm_fb_data_type *mfd);
static int mipi_ruby_device_register(const char* dev_name, struct msm_panel_info *pinfo, u32 channel, u32 panel);

static struct dsi_buf panel_tx_buf;
static struct dsi_buf panel_rx_buf;
static struct msm_panel_info pinfo;
static int cur_bl_level = 0;
static int mipi_lcd_on = 1;
struct dcs_cmd_req cmdreq;

static char sw_reset[2] = {0x01, 0x00}; /* DTYPE_DCS_WRITE */
static char enter_sleep[2] = {0x10, 0x00}; 
static char exit_sleep[2] = {0x11, 0x00}; 
static char display_off[2] = {0x28, 0x00}; 
static char display_on[2] = {0x29, 0x00}; 
static char enable_te[2] = {0x35, 0x00}; 

static struct platform_driver this_driver = {
	.probe  = ruby_lcd_probe,
	.shutdown = ruby_lcd_shutdown,
};

struct msm_fb_panel_data ruby_panel_data = {
	.on			= ruby_lcd_on,
	.off		= ruby_lcd_off,
	.set_backlight = ruby_set_backlight,
	.display_on = ruby_display_on,
};

static struct mipi_dsi_platform_data mipi_dsi_pdata = {
	.vsync_gpio = GPIO_LCD_TE,
	.dsi_power_save = mipi_dsi_panel_power,
};

static char led_pwm1[] = {0x51, 0x0};
static char test_reg_ruy_shp[3] = {0x44, 0x01, 0x68};/* DTYPE_DCS_LWRITE */
static char set_width[5] = {0x2A, 0x00, 0x00, 0x02, 0x1B};
static char set_height[5] = {0x2B, 0x00, 0x00, 0x03, 0xBF};
static char novatek_e0[3] = {0xE0, 0x01, 0x03};
static char max_pktsize[2] = {MIPI_DSI_MRPS, 0x00};

static char ruy_shp_gamma1_d1[] = {
	0xD1, 0x00, 0x6D, 0x00,	0x76, 0x00, 0x88, 0x00,
	0x97, 0x00, 0xA5, 0x00,	0xBD, 0x00, 0xD0, 0x00,
	0xEE
};
static char ruy_shp_gamma1_d2[] = {
	0xD2, 0x01, 0x06, 0x01,	0x2B, 0x01, 0x46, 0x01,
	0x6B, 0x01, 0x83, 0x01,	0x84, 0x01, 0xA1, 0x01,
	0xC4
};
static char ruy_shp_gamma1_d3[] = {
	0xD3, 0x01, 0xD3, 0x01,	0xE4, 0x01, 0xF3, 0x02,
	0x0F, 0x02, 0x28, 0x02,	0x65, 0x02, 0x87, 0x02,
	0x95
};
static char ruy_shp_gamma1_d4[] = {
	0xD4, 0x02, 0x99, 0x02,	0x99
};
/* G+ */
static char ruy_shp_gamma1_d5[] = {
	0xD5, 0x00, 0xBE, 0x00,	0xC5, 0x00, 0xD2, 0x00,
	0xDD, 0x00, 0xE7, 0x00,	0xF9, 0x01, 0x09, 0x01,
	0x23
};
static char ruy_shp_gamma1_d6[] = {
	0xD6, 0x01, 0x35, 0x01,	0x4D, 0x01, 0x5F, 0x01,
	0x7A, 0x01, 0x93, 0x01,	0x93, 0x01, 0xA8, 0x01,
	0xC9
};
static char ruy_shp_gamma1_d7[] = {
	0xD7, 0x01, 0xD8, 0x01,	0xE8, 0x01, 0xF6, 0x02,
	0x11, 0x02, 0x29, 0x02,	0x49, 0x02, 0x71, 0x02,
	0x91
};
static char ruy_shp_gamma1_d8[] = {
	0xD8, 0x02, 0x99, 0x02,	0x99
};
/* B+ */
static char ruy_shp_gamma1_d9[] = {
	0xD9, 0x00, 0x84, 0x00,	0x93, 0x00, 0xAE, 0x00,
	0xC5, 0x00, 0xD6, 0x00,	0xF1, 0x01, 0x08, 0x01,
	0x29
};
static char ruy_shp_gamma1_dd[] = {
	0xDD, 0x01, 0x3D, 0x01,	0x5A, 0x01, 0x6D, 0x01,
	0x81, 0x01, 0x97, 0x01,	0x97, 0x01, 0xAC, 0x01,
	0xCD
};
static char ruy_shp_gamma1_de[] = {
	0xDE, 0x01, 0xDD, 0x01,	0xE7, 0x01, 0xF6, 0x02,
	0x0E, 0x02, 0x34, 0x02,	0x6A, 0x02, 0x75, 0x02,
	0x90
};
static char ruy_shp_gamma1_df[] = {
	0xDF, 0x02, 0x99, 0x02,	0x99
};
/* R- */
static char ruy_shp_gamma1_e0[] = {
	0xE0, 0x00, 0x8D, 0x00,	0x99, 0x00, 0xB0, 0x00,
	0xC5, 0x00, 0xD8, 0x00,	0xF9, 0x01, 0x14, 0x01,
	0x3E
};
static char ruy_shp_gamma1_e1[] = {
	0xE1, 0x01, 0x60, 0x01,	0x96, 0x01, 0xBF, 0x01,
	0xF8, 0x02, 0x20, 0x02,	0x22, 0x02, 0x57, 0x02,
	0x9C
};
static char ruy_shp_gamma1_e2[] = {
	0xE2, 0x02, 0xBE, 0x02,	0xE7, 0x03, 0x0C, 0x03,
	0x40, 0x03, 0x64, 0x03,	0xAB, 0x03, 0xD4, 0x03,
	0xE8
};
static char ruy_shp_gamma1_e3[] = {
	0xE3, 0x03, 0xED, 0x03,	0xEE
};
/* G- */
static char ruy_shp_gamma1_e4[] = {
	0xE4, 0x00, 0xFB, 0x01,	0x04, 0x01, 0x16, 0x01,
	0x26, 0x01, 0x34, 0x01,	0x4E, 0x01, 0x65, 0x01,
	0x8A
};
static char ruy_shp_gamma1_e5[] = {
	0xE5, 0x01, 0xA4, 0x01,	0xC9, 0x01, 0xE5, 0x02,
	0x11, 0x02, 0x3C, 0x02,	0x3D, 0x02, 0x63, 0x02,
	0xA9
};
static char ruy_shp_gamma1_e6[] = {
	0xE6, 0x02, 0xCB, 0x02,	0xF0, 0x03, 0x14, 0x03,
	0x44, 0x03, 0x64, 0x03,	0x8B, 0x03, 0xB9, 0x03,
	0xE2
};
static char ruy_shp_gamma1_e7[] = {
	0xE7, 0x03, 0xED, 0x03, 0xEE
};
/* B- */
static char ruy_shp_gamma1_e8[] = {
	0xE8, 0x00, 0xAB, 0x00,	0xC0, 0x00, 0xE5, 0x01,
	0x04, 0x01, 0x1C, 0x01,	0x43, 0x01, 0x62, 0x01,
	0x92
};
static char ruy_shp_gamma1_e9[] = {
	0xE9, 0x01, 0xB1, 0x01,	0xDD, 0x01, 0xFB, 0x02,
	0x1D, 0x02, 0x43, 0x02,	0x44, 0x02, 0x6C, 0x02,
	0xB0
};
static char ruy_shp_gamma1_ea[] = {
	0xEA, 0x02, 0xD5, 0x02,	0xED, 0x03, 0x12, 0x03,
	0x3F, 0x03, 0x73, 0x03,	0xB0, 0x03, 0xBD, 0x03,
	0xE0
};
static char ruy_shp_gamma1_eb[] = {
	0xEB, 0x03, 0xED, 0x03,	0xEE
};

/* Gamma for cut 2 */
/* R+ */
static char ruy_shp_gamma2_d1[] = {
	0xD1, 0x00, 0x6D, 0x00,	0x76, 0x00, 0x88, 0x00,
	0x97, 0x00, 0xA5, 0x00,	0xBD, 0x00, 0xD0, 0x00,
	0xEE
};
static char ruy_shp_gamma2_d2[] = {
	0xD2, 0x01, 0x06, 0x01,	0x2B, 0x01, 0x46, 0x01,
	0x6B, 0x01, 0x83, 0x01,	0x84, 0x01, 0xA1, 0x01,
	0xC4
};
static char ruy_shp_gamma2_d3[] = {
	0xD3, 0x01, 0xD3, 0x01,	0xE4, 0x01, 0xF3, 0x02,
	0x0F, 0x02, 0x28, 0x02,	0x65, 0x02, 0x87, 0x02,
	0x95
};
static char ruy_shp_gamma2_d4[] = {
	0xD4, 0x02, 0x99, 0x02,	0x99
};
/* G+ */
static char ruy_shp_gamma2_d5[] = {
	0xD5, 0x00, 0xBE, 0x00,	0xC5, 0x00, 0xD2, 0x00,
	0xDD, 0x00, 0xE7, 0x00,	0xF9, 0x01, 0x09, 0x01,
	0x23
};
static char ruy_shp_gamma2_d6[] = {
	0xD6, 0x01, 0x35, 0x01,	0x4D, 0x01, 0x5F, 0x01,
	0x7A, 0x01, 0x93, 0x01,	0x93, 0x01, 0xA8, 0x01,
	0xC9
};
static char ruy_shp_gamma2_d7[] = {
	0xD7, 0x01, 0xD8, 0x01,	0xE8, 0x01, 0xF6, 0x02,
	0x11, 0x02, 0x29, 0x02,	0x49, 0x02, 0x71, 0x02,
	0x91
};
static char ruy_shp_gamma2_d8[] = {
	0xD8, 0x02, 0x99, 0x02,	0x99
};
/* B+ */
static char ruy_shp_gamma2_d9[] = {
	0xD9, 0x00, 0x84, 0x00,	0x93, 0x00, 0xAE, 0x00,
	0xC5, 0x00, 0xD6, 0x00,	0xF1, 0x01, 0x08, 0x01,
	0x29
};
static char ruy_shp_gamma2_dd[] = {
	0xDD, 0x01, 0x3D, 0x01,	0x5A, 0x01, 0x6D, 0x01,
	0x81, 0x01, 0x97, 0x01,	0x97, 0x01, 0xAC, 0x01,
	0xCD
};
static char ruy_shp_gamma2_de[] = {
	0xDE, 0x01, 0xDD, 0x01,	0xE7, 0x01, 0xF6, 0x02,
	0x0E, 0x02, 0x34, 0x02,	0x6A, 0x02, 0x75, 0x02,
	0x90
};
static char ruy_shp_gamma2_df[] = {
	0xDF, 0x02, 0x99, 0x02,	0x99
};
/* R- */
static char ruy_shp_gamma2_e0[] = {
	0xE0, 0x00, 0x8D, 0x00,	0x99, 0x00, 0xB0, 0x00,
	0xC5, 0x00, 0xD8, 0x00,	0xF9, 0x01, 0x14, 0x01,
	0x3E
};
static char ruy_shp_gamma2_e1[] = {
	0xE1, 0x01, 0x60, 0x01,	0x96, 0x01, 0xBF, 0x01,
	0xF8, 0x02, 0x20, 0x02,	0x22, 0x02, 0x57, 0x02,
	0x9C
};
static char ruy_shp_gamma2_e2[] = {
	0xE2, 0x02, 0xBE, 0x02,	0xE7, 0x03, 0x0C, 0x03,
	0x40, 0x03, 0x64, 0x03,	0xAB, 0x03, 0xD4, 0x03,
	0xE8
};
static char ruy_shp_gamma2_e3[] = {
	0xE3, 0x03, 0xED, 0x03,	0xEE
};
/* G- */
static char ruy_shp_gamma2_e4[] = {
	0xE4, 0x00, 0xFB, 0x01,	0x04, 0x01, 0x16, 0x01,
	0x26, 0x01, 0x34, 0x01,	0x4E, 0x01, 0x65, 0x01,
	0x8A
};
static char ruy_shp_gamma2_e5[] = {
	0xE5, 0x01, 0xA4, 0x01,	0xC9, 0x01, 0xE5, 0x02,
	0x11, 0x02, 0x3C, 0x02,	0x3D, 0x02, 0x63, 0x02,
	0xA9
};
static char ruy_shp_gamma2_e6[] = {
	0xE6, 0x02, 0xCB, 0x02,	0xF0, 0x03, 0x14, 0x03,
	0x44, 0x03, 0x64, 0x03,	0x8B, 0x03, 0xB9, 0x03,
	0xE2
};
static char ruy_shp_gamma2_e7[] = {
	0xE7, 0x03, 0xED, 0x03, 0xEE
};
/* B- */
static char ruy_shp_gamma2_e8[] = {
	0xE8, 0x00, 0xAB, 0x00,	0xC0, 0x00, 0xE5, 0x01,
	0x04, 0x01, 0x1C, 0x01,	0x43, 0x01, 0x62, 0x01,
	0x92
};
static char ruy_shp_gamma2_e9[] = {
	0xE9, 0x01, 0xB1, 0x01,	0xDD, 0x01, 0xFB, 0x02,
	0x1D, 0x02, 0x43, 0x02,	0x44, 0x02, 0x6C, 0x02,
	0xB0
};
static char ruy_shp_gamma2_ea[] = {
	0xEA, 0x02, 0xD5, 0x02,	0xED, 0x03, 0x12, 0x03,
	0x3F, 0x03, 0x73, 0x03,	0xB0, 0x03, 0xBD, 0x03,
	0xE0
};
static char ruy_shp_gamma2_eb[] = {
	0xEB, 0x03, 0xED, 0x03,	0xEE
};

static struct dsi_cmd_desc ruy_shp_cmd_on_cmds[] = {
	/* added by our own */
	{ DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(sw_reset), sw_reset },

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		5, (char[]){ 0xFF, 0xAA, 0x55, 0x25, 0x01 } },

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		17, (char[]){ 0xFA, 0x00, 0x00, 0x00,
					  0x00, 0x00, 0x00, 0x00,
					  0x00, 0x00, 0x00, 0x00,
					  0x00, 0x03, 0x20, 0x12,
					  0x20, 0xFF, 0xFF, 0xFF } },/* 90Hz -> 60Hz */

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		5, (char[]){ 0xF3, 0x03, 0x03, 0x07, 0x14 } },/* vertical noise*/

	{ DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(exit_sleep), exit_sleep },

	/* page 0 */
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		6, (char[]){ 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00 } },/* select page 0 */

	{ DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		2, (char[]){ 0xB1, 0xFC } },/* display option */
	{ DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		2, (char[]){ 0xB6, 0x07 } },/* output data hold time */

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		3, (char[]){ 0xB7, 0x00, 0x00 } },/* EQ gate signal */
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		5, (char[]){ 0xB8, 0x00, 0x07, 0x07, 0x07 } },/* EQ source driver */

	{ DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		2, (char[]){ 0xBA, 0x02 } },/* vertical porch */
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		4, (char[]){ 0xBB, 0x83, 0x03, 0x83 } },/* source driver (vertical noise) */

	{ DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		2, (char[]){ 0xBC, 0x02 } },/* inversion driving */

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		5, (char[]){ 0xBD, 0x01, 0x4B, 0x08, 0x26 } },/* timing control (normal mode) */
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		12, (char[]){ 0xC7, 0x00, 0x0F, 0x0F, 0x06, 0x07, 0x09, 0x0A, 0x0A, 0x0A, 0xF0, 0xF0 } },/* timing control */

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		ARRAY_SIZE(novatek_e0), novatek_e0 },/* PWM frequency = 13kHz */

	/* page 1 */
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		6, (char[]){ 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01 } },/* select page 1 */

	{ DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){ 0xB0, 0x1F} },
	{ DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){ 0xB1, 0x1F} },
	{ DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){ 0xB3, 0x0D} },
	{ DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){ 0xB4, 0x0F} },
	{ DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){ 0xB6, 0x44} },
	{ DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){ 0xB7, 0x24} },
	{ DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){ 0xB9, 0x27} },
	{ DTYPE_DCS_WRITE1, 1, 0, 0, 0, 2, (char[]){ 0xBA, 0x24} },

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		4, (char[]){ 0xBC, 0x00, 0xC8, 0x00 } },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		4, (char[]){ 0xBD, 0x00, 0x78, 0x00 } },

	/* enter gamma table */
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_d1), ruy_shp_gamma1_d1 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_d2), ruy_shp_gamma1_d2 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_d3), ruy_shp_gamma1_d3 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_d4), ruy_shp_gamma1_d4 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_d5), ruy_shp_gamma1_d5 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_d6), ruy_shp_gamma1_d6 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_d7), ruy_shp_gamma1_d7 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_d8), ruy_shp_gamma1_d8 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_d9), ruy_shp_gamma1_d9 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_dd), ruy_shp_gamma1_dd },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_de), ruy_shp_gamma1_de },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_df), ruy_shp_gamma1_df },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_e0), ruy_shp_gamma1_e0 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_e1), ruy_shp_gamma1_e1 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_e2), ruy_shp_gamma1_e2 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_e3), ruy_shp_gamma1_e3 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_e4), ruy_shp_gamma1_e4 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_e5), ruy_shp_gamma1_e5 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_e6), ruy_shp_gamma1_e6 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_e7), ruy_shp_gamma1_e7 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_e8), ruy_shp_gamma1_e8 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_e9), ruy_shp_gamma1_e9 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_ea), ruy_shp_gamma1_ea },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma1_eb), ruy_shp_gamma1_eb },
	/* leave gamma table */

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		6, (char[]){ 0xF0, 0x55, 0xAA, 0x52, 0x00, 0x00 } },/* select page 0 */

	{ DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(enable_te), enable_te },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(test_reg_ruy_shp), test_reg_ruy_shp },

	{ DTYPE_MAX_PKTSIZE, 1, 0, 0, 0,
		sizeof(max_pktsize), max_pktsize },

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_width), set_width },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_height), set_height },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(display_on), display_on },
};

static struct dsi_cmd_desc ruy_shp_c2_cmd_on_cmds[] = {
	/* added by our own */
	{ DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(sw_reset), sw_reset },

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		5, (char[]){ 0xFF, 0xAA, 0x55, 0x25, 0x01 } },

	{ DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(exit_sleep), exit_sleep },

	/* page 0 */
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		6, (char[]){ 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00 } },/* select page 0 */

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		ARRAY_SIZE(novatek_e0), novatek_e0 },/* PWM frequency = 13kHz */

	/* page 1 */
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		6, (char[]){ 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01 } },/* select page 1 */

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		4, (char[]){ 0xBC, 0x00, 0xC0, 0x00 } },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		4, (char[]){ 0xBD, 0x00, 0x70, 0x00 } },

	/* enter gamma table */
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_d1), ruy_shp_gamma2_d1 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_d2), ruy_shp_gamma2_d2 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_d3), ruy_shp_gamma2_d3 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_d4), ruy_shp_gamma2_d4 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_d5), ruy_shp_gamma2_d5 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_d6), ruy_shp_gamma2_d6 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_d7), ruy_shp_gamma2_d7 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_d8), ruy_shp_gamma2_d8 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_d9), ruy_shp_gamma2_d9 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_dd), ruy_shp_gamma2_dd },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_de), ruy_shp_gamma2_de },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_df), ruy_shp_gamma2_df },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_e0), ruy_shp_gamma2_e0 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_e1), ruy_shp_gamma2_e1 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_e2), ruy_shp_gamma2_e2 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_e3), ruy_shp_gamma2_e3 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_e4), ruy_shp_gamma2_e4 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_e5), ruy_shp_gamma2_e5 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_e6), ruy_shp_gamma2_e6 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_e7), ruy_shp_gamma2_e7 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_e8), ruy_shp_gamma2_e8 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_e9), ruy_shp_gamma2_e9 },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_ea), ruy_shp_gamma2_ea },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0, ARRAY_SIZE(ruy_shp_gamma2_eb), ruy_shp_gamma2_eb },
	/* leave gamma table */

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		6, (char[]){ 0xF0, 0x55, 0xAA, 0x52, 0x00, 0x00 } },/* select page 0 */

	{ DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(enable_te), enable_te },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(test_reg_ruy_shp), test_reg_ruy_shp },

	{ DTYPE_MAX_PKTSIZE, 1, 0, 0, 0,
		sizeof(max_pktsize), max_pktsize },

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_width), set_width },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_height), set_height },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(display_on), display_on },
};

static struct dsi_cmd_desc ruy_shp_c2o_cmd_on_cmds[] = {
	/* added by our own */
	{ DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(sw_reset), sw_reset },

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		5, (char[]){ 0xFF, 0xAA, 0x55, 0x25, 0x01 } },

	{ DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(exit_sleep), exit_sleep },

	/* page 0 */
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		6, (char[]){ 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00 } },/* select page 0 */

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		ARRAY_SIZE(novatek_e0), novatek_e0 },/* PWM frequency = 13kHz */

	/* page 1 */
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		6, (char[]){ 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01 } },/* select page 1 */

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		6, (char[]){ 0xF0, 0x55, 0xAA, 0x52, 0x00, 0x00 } },/* select page 0 */

	{ DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(enable_te), enable_te },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(test_reg_ruy_shp), test_reg_ruy_shp },

	{ DTYPE_MAX_PKTSIZE, 1, 0, 0, 0,
		sizeof(max_pktsize), max_pktsize },

	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_width), set_width },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_height), set_height },
	{ DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(display_on), display_on },
};

static struct dsi_cmd_desc ruby_display_off_cmds[] = {
		{DTYPE_DCS_WRITE, 1, 0, 0, 0,
			sizeof(display_off), display_off},
		{DTYPE_DCS_WRITE, 1, 0, 0, 110,
			sizeof(enter_sleep), enter_sleep}
};

#define BRI_SETTING_MIN           30
#define BRI_SETTING_DEF           143
#define BRI_SETTING_MAX           255

#define PWM_MIN                   9       /* 3.5% of max pwm */
#define PWM_DEFAULT_AUO           83     /* 32.67% of max pwm */
#define PWM_DEFAULT_SHARP	  100	/* 39.2% of max pwm */
#define PWM_MAX                   255

#define PWM_DEFAULT	\
	(panel_type == PANEL_ID_PYD_AUO_NT ? PWM_DEFAULT_AUO:PWM_DEFAULT_SHARP)

static unsigned char ruby_shrink_pwm(int val)
{
	unsigned char shrink_br = BRI_SETTING_MAX;

	if (val <= 0) {
		shrink_br = 0;
	} else if (val > 0 && (val < BRI_SETTING_MIN)) {
			shrink_br = PWM_MIN;
	} else if ((val >= BRI_SETTING_MIN) && (val <= BRI_SETTING_DEF)) {
			shrink_br = (val - 30) * (PWM_DEFAULT - PWM_MIN) /
		(BRI_SETTING_DEF - BRI_SETTING_MIN) + PWM_MIN;
	} else if (val > BRI_SETTING_DEF && val <= BRI_SETTING_MAX) {
			shrink_br = (val - 143) * (PWM_MAX - PWM_DEFAULT) /
		(BRI_SETTING_MAX - BRI_SETTING_DEF) + PWM_DEFAULT;
	} else if (val > BRI_SETTING_MAX)
			shrink_br = PWM_MAX;

	PR_DISP_DEBUG("brightness orig=%d, transformed=%d\n", val, shrink_br);

	return shrink_br;
}

static struct mipi_dsi_phy_ctrl novatek_dsi_cmd_mode_phy_db = {
		{0x03, 0x01, 0x01, 0x00},
		{0x96, 0x1E, 0x1E, 0x00, 0x3C, 0x3C, 0x1E, 0x28,
		0x0b, 0x13, 0x04},
		{0x7f, 0x00, 0x00, 0x00},
		{0xee, 0x02, 0x86, 0x00},
		{0x41, 0x9c, 0xb9, 0xd6, 0x00, 0x50, 0x48, 0x63,0x01, 0x0f, 0x07,
		0x05, 0x14, 0x03, 0x03, 0x03, 0x54, 0x06, 0x10, 0x04, 0x03 },
};

static int mipi_cmd_novatek_blue_qhd_pt_init(void)
{
	int ret;

	PR_DISP_INFO("panel: mipi_cmd_novatek_qhd\n");

	pinfo.xres = 540;
	pinfo.yres = 960;
	pinfo.type = MIPI_CMD_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;
	pinfo.lcdc.h_back_porch = 64;
	pinfo.lcdc.h_front_porch = 96;
	pinfo.lcdc.h_pulse_width = 32;
	pinfo.lcdc.v_back_porch = 16;
	pinfo.lcdc.v_front_porch = 16;
	pinfo.lcdc.v_pulse_width = 4;
	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
	pinfo.lcdc.hsync_skew = 0;
	pinfo.bl_max = 255;
	pinfo.bl_min = 1;
	pinfo.fb_num = 2;
	pinfo.clk_rate = 482000000;
	pinfo.lcd.vsync_enable = TRUE;
	pinfo.lcd.hw_vsync_mode = TRUE;
	pinfo.lcd.refx100 = 6096; /* adjust refx100 to prevent tearing */

	pinfo.mipi.mode = DSI_CMD_MODE;
	pinfo.mipi.dst_format = DSI_CMD_DST_FORMAT_RGB888;
	pinfo.mipi.vc = 0;
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_BGR;
	pinfo.mipi.data_lane0 = TRUE;
	pinfo.mipi.data_lane1 = TRUE;
	pinfo.mipi.t_clk_post = 0x0a;
	pinfo.mipi.t_clk_pre = 0x1e;
	pinfo.mipi.stream = 0;	/* dma_p */
	pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.te_sel = 1; /* TE from vsycn gpio */
	pinfo.mipi.interleave_max = 1;
	pinfo.mipi.insert_dcs_cmd = TRUE;
	pinfo.mipi.wr_mem_continue = 0x3c;
	pinfo.mipi.wr_mem_start = 0x2c;
	pinfo.mipi.dsi_phy_db = &novatek_dsi_cmd_mode_phy_db;

	ret = mipi_ruby_device_register("mipi_novatek", &pinfo, MIPI_DSI_PRIM,
						MIPI_DSI_PANEL_QHD_PT);
	if (ret)
		PR_DISP_ERR("%s: failed to register device!\n", __func__);

	this_driver.driver.name = "mipi_novatek";
	return ret;
}

static struct dsi_cmd_desc backlight_cmd[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(led_pwm1), led_pwm1},
};

static inline void ruby_mipi_dsi_set_backlight(struct msm_fb_data_type *mfd)
{
	struct mipi_panel_info *mipi;

	mipi  = &mfd->panel_info.mipi;

	if (panel_type == PANEL_ID_RUY_SHARP_NT ||
		panel_type == PANEL_ID_RUY_SHARP_NT_C2 || panel_type == PANEL_ID_RUY_SHARP_NT_C2O) {
		ruby_shrink_pwm(mfd->bl_level);
	}

	if (panel_type == PANEL_ID_RUY_SHARP_NT ||
		panel_type == PANEL_ID_RUY_SHARP_NT_C2 || panel_type == PANEL_ID_RUY_SHARP_NT_C2O) {
		cmdreq.cmds = (struct dsi_cmd_desc*)&backlight_cmd;
		cmdreq.cmds_cnt = 1;
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
	}

	return;
}

static int ruby_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;

	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi  = &mfd->panel_info.mipi;

	if (mipi->mode == DSI_VIDEO_MODE) {
		
		PR_DISP_ERR("%s: not support DSI_VIDEO_MODE!(%d)", __func__, mipi->mode);
	} else {
		if (!mipi_lcd_on) {
			if (panel_type == PANEL_ID_RUY_SHARP_NT) {
				printk(KERN_INFO "ruby_lcd_on PANEL_ID_RUY_SHARP_NT\n");
				cmdreq.cmds = ruy_shp_cmd_on_cmds;
				cmdreq.cmds_cnt = ARRAY_SIZE(ruy_shp_cmd_on_cmds);
				cmdreq.flags = CMD_REQ_COMMIT;
				cmdreq.rlen = 0;
				cmdreq.cb = NULL;
				mipi_dsi_cmdlist_put(&cmdreq);
			} else if (panel_type == PANEL_ID_RUY_SHARP_NT_C2) {
				printk(KERN_INFO "ruby_lcd_on PANEL_ID_RUY_SHARP_NT\n");
				cmdreq.cmds = ruy_shp_c2_cmd_on_cmds;
				cmdreq.cmds_cnt = ARRAY_SIZE(ruy_shp_c2_cmd_on_cmds);
				cmdreq.flags = CMD_REQ_COMMIT;
				cmdreq.rlen = 0;
				cmdreq.cb = NULL;
				mipi_dsi_cmdlist_put(&cmdreq);
			} else if (panel_type == PANEL_ID_RUY_SHARP_NT_C2O) {
				printk(KERN_INFO "ruby_lcd_on PANEL_ID_RUY_SHARP_NT\n");
				cmdreq.cmds = ruy_shp_c2o_cmd_on_cmds;
				cmdreq.cmds_cnt = ARRAY_SIZE(ruy_shp_c2o_cmd_on_cmds);
				cmdreq.flags = CMD_REQ_COMMIT;
				cmdreq.rlen = 0;
				cmdreq.cb = NULL;
				mipi_dsi_cmdlist_put(&cmdreq);
			} else {
				PR_DISP_ERR("%s: panel_type is not supported!(%d)", __func__, panel_type);
			}
		}
	}
	mipi_lcd_on = 1;

	return 0;
}

static void ruby_display_on(struct msm_fb_data_type *mfd)
{
	if (panel_type == PANEL_ID_RUY_SHARP_NT) {
		printk(KERN_INFO "ruby_lcd_on PANEL_ID_RUY_SHARP_NT\n");
		cmdreq.cmds = ruy_shp_cmd_on_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(ruy_shp_cmd_on_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);
	} else if (panel_type == PANEL_ID_RUY_SHARP_NT_C2) {
		printk(KERN_INFO "ruby_lcd_on PANEL_ID_RUY_SHARP_NT\n");
		cmdreq.cmds = ruy_shp_cmd_on_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(ruy_shp_cmd_on_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);
	} else if (panel_type == PANEL_ID_RUY_SHARP_NT_C2O) {
		printk(KERN_INFO "ruby_lcd_on PANEL_ID_RUY_SHARP_NT\n");
		cmdreq.cmds = ruy_shp_cmd_on_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(ruy_shp_cmd_on_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);
	}
	cur_bl_level = 0;
}

static int ruby_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;
	if (!mipi_lcd_on)
		return 0;

	if (panel_type == PANEL_ID_RUY_SHARP_NT) {
		printk(KERN_INFO "ruby_lcd_on PANEL_ID_RUY_SHARP_NT\n");
		cmdreq.cmds = ruby_display_off_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(ruby_display_off_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);
	} else if (panel_type == PANEL_ID_RUY_SHARP_NT_C2) {
		printk(KERN_INFO "ruby_lcd_on PANEL_ID_RUY_SHARP_NT\n");
		cmdreq.cmds = ruby_display_off_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(ruby_display_off_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);
	} else if (panel_type == PANEL_ID_RUY_SHARP_NT_C2O) {
		printk(KERN_INFO "ruby_lcd_on PANEL_ID_RUY_SHARP_NT\n");
		cmdreq.cmds = ruby_display_off_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(ruby_display_off_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);
	}
	mipi_lcd_on = 0;
	return 0;
}

static void ruby_set_backlight(struct msm_fb_data_type *mfd)
{
	if (!mfd->panel_power_on || cur_bl_level == mfd->bl_level) {
		return;
	}

	ruby_mipi_dsi_set_backlight(mfd);

	cur_bl_level = mfd->bl_level;
	PR_DISP_DEBUG("%s- bl_level=%d\n", __func__, mfd->bl_level);
}

static int __devinit ruby_lcd_probe(struct platform_device *pdev)
{
	msm_fb_add_device(pdev);

	return 0;
}

static void ruby_lcd_shutdown(struct platform_device *pdev)
{
	mipi_dsi_panel_power(0);
}

static int mipi_ruby_device_register(const char* dev_name, struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	static int ch_used[3] = {0, 0, 0};
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	pdev = platform_device_alloc(dev_name, (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	ruby_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &ruby_panel_data,
		sizeof(ruby_panel_data));
	if (ret) {
		PR_DISP_ERR("%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		PR_DISP_ERR("%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}
	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static int mipi_dsi_panel_power(const int on)
{
	static bool dsi_power_on = false;
	static struct regulator *v_lcm, *v_dsivdd;
	static bool bPanelPowerOn = false;
	int rc;

	const char * const lcm_str    = "8058_l19";
	const char * const dsivdd_str = "8058_l20";

	PR_DISP_INFO("%s: state : %d\n", __func__, on);

	if (!dsi_power_on) {

		v_lcm = regulator_get(NULL,
				lcm_str);
		if (IS_ERR(v_lcm)) {
			PR_DISP_ERR("could not get %s, rc = %ld\n",
				lcm_str, PTR_ERR(v_lcm));
			return -ENODEV;
		}

		v_dsivdd = regulator_get(NULL,
				dsivdd_str);
		if (IS_ERR(v_dsivdd)) {
			PR_DISP_ERR("could not get %s, rc = %ld\n",
				dsivdd_str, PTR_ERR(v_dsivdd));
			return -ENODEV;
		}

		rc = regulator_set_voltage(v_lcm, 3000000, 3000000);
		if (rc) {
			PR_DISP_ERR("%s#%d: set_voltage %s failed, rc=%d\n", __func__, __LINE__, lcm_str, rc);
			return -EINVAL;
		}

		rc = regulator_set_voltage(v_dsivdd, 1800000, 1800000);
		if (rc) {
			PR_DISP_ERR("%s#%d: set_voltage %s failed, rc=%d\n", __func__, __LINE__, dsivdd_str, rc);
			return -EINVAL;
		}

		rc = gpio_request(GPIO_LCM_RST_N, "LCM_RST_N");
		if (rc) {
			PR_DISP_ERR("%s:LCM gpio %d request failed, rc=%d\n", __func__,  GPIO_LCM_RST_N, rc);
			return -EINVAL;
		}
		dsi_power_on = true;
	}

	if (on) {
		PR_DISP_INFO("%s: on\n", __func__);
		rc = regulator_set_optimum_mode(v_lcm, 100000);
		if (rc < 0) {
			PR_DISP_ERR("set_optimum_mode %s failed, rc=%d\n", lcm_str, rc);
			return -EINVAL;
		}

		rc = regulator_set_optimum_mode(v_dsivdd, 100000);
		if (rc < 0) {
			PR_DISP_ERR("set_optimum_mode %s failed, rc=%d\n", dsivdd_str, rc);
			return -EINVAL;
		}

		rc = regulator_enable(v_dsivdd);
		if (rc) {
			PR_DISP_ERR("enable regulator %s failed, rc=%d\n", dsivdd_str, rc);
			return -ENODEV;
		}

		hr_msleep(1);

		rc = regulator_enable(v_lcm);
		if (rc) {
			PR_DISP_ERR("enable regulator %s failed, rc=%d\n", lcm_str, rc);
			return -ENODEV;
		}

		if (!mipi_lcd_on) {
			hr_msleep(10);
			gpio_set_value(GPIO_LCM_RST_N, 1);
			hr_msleep(1);
			gpio_set_value(GPIO_LCM_RST_N, 0);
			hr_msleep(35);
			gpio_set_value(GPIO_LCM_RST_N, 1);
		}
		hr_msleep(60);
		bPanelPowerOn = true;
	} else {
		PR_DISP_INFO("%s: off\n", __func__);
		if (!bPanelPowerOn) return 0;
		hr_msleep(100);
		gpio_set_value(GPIO_LCM_RST_N, 0);
		hr_msleep(10);

		if (regulator_disable(v_lcm)) {
			PR_DISP_ERR("%s: Unable to enable the regulator: %s\n", __func__, lcm_str);
			return -EINVAL;
		}
		hr_msleep(5);

		if (panel_type == PANEL_ID_RUY_SHARP_NT ||
			panel_type == PANEL_ID_RUY_SHARP_NT_C2 || panel_type == PANEL_ID_RUY_SHARP_NT_C2O) {
			if (regulator_disable(v_dsivdd)) {
				PR_DISP_ERR("%s: Unable to enable the regulator: %s\n", __func__, dsivdd_str);
				return -EINVAL;
			}

			rc = regulator_set_optimum_mode(v_dsivdd, 100);
			if (rc < 0) {
				PR_DISP_ERR("%s: Unable to enable the regulator: %s\n", __func__, dsivdd_str);
				return -EINVAL;
			}
		}
		bPanelPowerOn = false;
	}
	return 0;
}

static int __init ruby_panel_init(void)
{
	if(panel_type != PANEL_ID_NONE)
		msm_fb_register_device("mipi_dsi", &mipi_dsi_pdata);
	else
		printk(KERN_INFO "[DISP]panel ID= NONE\n");

	return 0;
}

static int __init ruby_panel_late_init(void)
{
	mipi_dsi_buf_alloc(&panel_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&panel_rx_buf, DSI_BUF_SIZE);

	if(panel_type == PANEL_ID_NONE) {
		PR_DISP_INFO("%s panel ID = PANEL_ID_NONE\n", __func__);
		return 0;
	}

	if (panel_type == PANEL_ID_RUY_SHARP_NT ||
		panel_type == PANEL_ID_RUY_SHARP_NT_C2 || panel_type == PANEL_ID_RUY_SHARP_NT_C2O) {
		mipi_cmd_novatek_blue_qhd_pt_init();
	}

	return platform_driver_register(&this_driver);
}

module_init(ruby_panel_init);
late_initcall(ruby_panel_late_init);
