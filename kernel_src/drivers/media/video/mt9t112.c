/*
 * mt9t112 Camera Driver
 *
 * Copyright (C) 2009 Renesas Solutions Corp.
 * Kuninori Morimoto <morimoto.kuninori@renesas.com>
 *
 * Based on ov772x driver, mt9m111 driver,
 *
 * Copyright (C) 2008 Kuninori Morimoto <morimoto.kuninori@renesas.com>
 * Copyright (C) 2008, Robert Jarzmik <robert.jarzmik@free.fr>
 * Copyright 2006-7 Jonathan Corbet <corbet@lwn.net>
 * Copyright (C) 2008 Magnus Damm
 * Copyright (C) 2008, Guennadi Liakhovetski <kernel@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <media/mt9t112.h>
#include <media/soc_camera.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-common.h>

/* you can check PLL/clock info */
/* #define EXT_CLOCK 24000000 */

//#define TEST_PATTERN

/************************************************************************
			macro
************************************************************************/
/*
 * frame size
 */
#define MAX_WIDTH   2048
#define MAX_HEIGHT  1536

#define VGA_WIDTH   640
#define VGA_HEIGHT  480

/*
 * macro of read/write
 */
#define ECHECKER(ret, x)		\
	do {				\
		(ret) = (x);		\
		if ((ret) < 0)		\
			return (ret);	\
	} while (0)

#define mt9t112_reg_write(ret, client, a, b) \
	ECHECKER(ret, __mt9t112_reg_write(client, a, b))
#define mt9t112_mcu_write(ret, client, a, b) \
	ECHECKER(ret, __mt9t112_mcu_write(client, a, b))

#define mt9t112_reg_mask_set(ret, client, a, b, c) \
	ECHECKER(ret, __mt9t112_reg_mask_set(client, a, b, c))
#define mt9t112_mcu_mask_set(ret, client, a, b, c) \
	ECHECKER(ret, __mt9t112_mcu_mask_set(client, a, b, c))

#define mt9t112_reg_read(ret, client, a) \
	ECHECKER(ret, __mt9t112_reg_read(client, a))
#define mt9t112_mcu_read(ret, client, a) \
	ECHECKER(ret, __mt9t112_mcu_read(client, a))

/*
 * Logical address
 */
#define _VAR(id, offset, base)	(base | (id & 0x1f) << 10 | (offset & 0x3ff))
#define VAR(id, offset)  _VAR(id, offset, 0x0000)
#define VAR8(id, offset) _VAR(id, offset, 0x8000)

/************************************************************************
			struct
************************************************************************/
struct mt9t112_format {
	enum v4l2_mbus_pixelcode code;
	enum v4l2_colorspace colorspace;
	u16 fmt;
	u16 order;
};

struct mt9t112_resolution_param {
	u16 col_strt;
	u16 row_end;
	u16 col_end;
	u16 read_mode;
	u16 fine_cor;
	u16 fine_min;
	u16 fine_max;
	u16 base_lines;
	u16 min_lin_len;
	u16 line_len;
	u16 con_width;
	u16 con_height;
	u16 s_f1_50;
	u16 s_f2_50;
	u16 s_f1_60;
	u16 s_f2_60;
	u16 per_50;
	u16 per_50_M;
	u16 per_60;
	u16 fd_w_height;
	u16 tx_water;
	u16 max_fd_50;
	u16 max_fd_60;
	u16 targ_fd;
};

struct mt9t112_priv {
	struct v4l2_subdev		 subdev;
	struct mt9t112_camera_info	*info;
	struct i2c_client		*client;
	struct v4l2_rect		 frame;
	const struct mt9t112_format	*format;
	int				 model;
	u32				 flags;
/* for flags */
#define INIT_DONE	(1 << 0)
#define PCLK_RISING	(1 << 1)
	struct mt9t112_resolution_param	 resolution;
};

/************************************************************************
			supported format
************************************************************************/

static const struct mt9t112_format mt9t112_cfmts[] = {
	{
		.code		= V4L2_MBUS_FMT_UYVY8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.fmt		= 1,
		.order		= 0,
	}, {
		.code		= V4L2_MBUS_FMT_VYUY8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.fmt		= 1,
		.order		= 1,
	}, {
		.code		= V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.fmt		= 1,
		.order		= 2,
	}, {
		.code		= V4L2_MBUS_FMT_YVYU8_2X8,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.fmt		= 1,
		.order		= 3,
	}, {
		.code		= V4L2_MBUS_FMT_RGB555_2X8_PADHI_LE,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.fmt		= 8,
		.order		= 2,
	}, {
		.code		= V4L2_MBUS_FMT_RGB565_2X8_LE,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.fmt		= 4,
		.order		= 2,
	},
};

/************************************************************************
			general function
************************************************************************/
static struct mt9t112_priv *to_mt9t112(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client),
			    struct mt9t112_priv,
			    subdev);
}

static int __mt9t112_reg_read(const struct i2c_client *client, u16 command)
{
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

	command = swab16(command);

	msg[0].addr  = client->addr;
	msg[0].flags = 0;
	msg[0].len   = 2;
	msg[0].buf   = (u8 *)&command;

	msg[1].addr  = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len   = 2;
	msg[1].buf   = buf;

	/*
	 * if return value of this function is < 0,
	 * it mean error.
	 * else, under 16bit is valid data.
	 */
	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0)
		return ret;

	memcpy(&ret, buf, 2);
	return swab16(ret);
}

static int __mt9t112_reg_write(const struct i2c_client *client,
			       u16 command, u16 data)
{
	struct i2c_msg msg;
	u8 buf[4];
	int ret;

	command = swab16(command);
	data = swab16(data);

	memcpy(buf + 0, &command, 2);
	memcpy(buf + 2, &data,    2);

	msg.addr  = client->addr;
	msg.flags = 0;
	msg.len   = 4;
	msg.buf   = buf;

	/*
	 * i2c_transfer return message length,
	 * but this function should return 0 if correct case
	 */
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret >= 0)
		ret = 0;

	return ret;
}

static int __mt9t112_reg_mask_set(const struct i2c_client *client,
				  u16  command,
				  u16  mask,
				  u16  set)
{
	int val = __mt9t112_reg_read(client, command);
	if (val < 0)
		return val;

	val &= ~mask;
	val |= set & mask;

	return __mt9t112_reg_write(client, command, val);
}

/* mcu access */
static int __mt9t112_mcu_read(const struct i2c_client *client, u16 command)
{
	int ret;

	ret = __mt9t112_reg_write(client, 0x098E, command);
	if (ret < 0)
		return ret;

	return __mt9t112_reg_read(client, 0x0990);
}

static int __mt9t112_mcu_write(const struct i2c_client *client,
			       u16 command, u16 data)
{
	int ret;

	ret = __mt9t112_reg_write(client, 0x098E, command);
	if (ret < 0)
		return ret;

	return __mt9t112_reg_write(client, 0x0990, data);
}

static int __mt9t112_mcu_mask_set(const struct i2c_client *client,
				  u16  command,
				  u16  mask,
				  u16  set)
{
	int val = __mt9t112_mcu_read(client, command);
	if (val < 0)
		return val;

	val &= ~mask;
	val |= set & mask;

	return __mt9t112_mcu_write(client, command, val);
}

static int mt9t112_reset(const struct i2c_client *client)
{
	int ret;

	mt9t112_reg_mask_set(ret, client, 0x001a, 0x0001, 0x0001);
	msleep(1);
	mt9t112_reg_mask_set(ret, client, 0x001a, 0x0001, 0x0000);

	return ret;
}

#ifndef EXT_CLOCK
#define CLOCK_INFO(a, b)
#else
#define CLOCK_INFO(a, b) mt9t112_clock_info(a, b)
static int mt9t112_clock_info(const struct i2c_client *client, u32 ext)
{
	int m, n, p1, p2, p3, p4, p5, p6, p7;
	u32 vco, clk;
	char *enable;

	ext /= 1000; /* kbyte order */

	mt9t112_reg_read(n, client, 0x0012);
	p1 = n & 0x000f;
	n = n >> 4;
	p2 = n & 0x000f;
	n = n >> 4;
	p3 = n & 0x000f;

	mt9t112_reg_read(n, client, 0x002a);
	p4 = n & 0x000f;
	n = n >> 4;
	p5 = n & 0x000f;
	n = n >> 4;
	p6 = n & 0x000f;

	mt9t112_reg_read(n, client, 0x002c);
	p7 = n & 0x000f;

	mt9t112_reg_read(n, client, 0x0010);
	m = n & 0x00ff;
	n = (n >> 8) & 0x003f;

	enable = ((6000 > ext) || (54000 < ext)) ? "X" : "";
	dev_dbg(&client->dev, "EXTCLK          : %10u K %s\n", ext, enable);

	vco = 2 * m * ext / (n+1);
	enable = ((384000 > vco) || (768000 < vco)) ? "X" : "";
	dev_dbg(&client->dev, "VCO             : %10u K %s\n", vco, enable);

	clk = vco / (p1+1) / (p2+1);
	enable = (96000 < clk) ? "X" : "";
	dev_dbg(&client->dev, "PIXCLK          : %10u K %s\n", clk, enable);

	clk = vco / (p3+1);
	enable = (768000 < clk) ? "X" : "";
	dev_dbg(&client->dev, "MIPICLK         : %10u K %s\n", clk, enable);

	clk = vco / (p6+1);
	enable = (96000 < clk) ? "X" : "";
	dev_dbg(&client->dev, "MCU CLK         : %10u K %s\n", clk, enable);

	clk = vco / (p5+1);
	enable = (54000 < clk) ? "X" : "";
	dev_dbg(&client->dev, "SOC CLK         : %10u K %s\n", clk, enable);

	clk = vco / (p4+1);
	enable = (70000 < clk) ? "X" : "";
	dev_dbg(&client->dev, "Sensor CLK      : %10u K %s\n", clk, enable);

	clk = vco / (p7+1);
	dev_dbg(&client->dev, "External sensor : %10u K\n", clk);

	clk = ext / (n+1);
	enable = ((2000 > clk) || (24000 < clk)) ? "X" : "";
	dev_dbg(&client->dev, "PFD             : %10u K %s\n", clk, enable);

	return 0;
}
#endif

static void mt9t112_frame_check(u32 *width, u32 *height, u32 *left, u32 *top)
{
	soc_camera_limit_side(left, width, 0, 0, MAX_WIDTH);
	soc_camera_limit_side(top, height, 0, 0, MAX_HEIGHT);
}

static int mt9t112_set_a_frame_size(const struct i2c_client *client,
				   u16 width,
				   u16 height)
{
	int ret;
	u16 wstart = (MAX_WIDTH - width) / 2;
	u16 hstart = (MAX_HEIGHT - height) / 2;

	/* (Context A) Image Width/Height */
	mt9t112_mcu_write(ret, client, VAR(26, 0), width);
	mt9t112_mcu_write(ret, client, VAR(26, 2), height);

	/* (Context A) Output Width/Height */
	mt9t112_mcu_write(ret, client, VAR(18, 43), 8 + width);
	mt9t112_mcu_write(ret, client, VAR(18, 45), 8 + height);

	/* (Context A) Start Row/Column */
	mt9t112_mcu_write(ret, client, VAR(18, 2), 4 + hstart);
	mt9t112_mcu_write(ret, client, VAR(18, 4), 4 + wstart);

	/* (Context A) End Row/Column */
	mt9t112_mcu_write(ret, client, VAR(18, 6), 11 + height + hstart);
	mt9t112_mcu_write(ret, client, VAR(18, 8), 11 + width  + wstart);

	mt9t112_mcu_write(ret, client, VAR8(1, 0), 0x06);

	return ret;
}

static int mt9t112_set_pll_dividers(const struct i2c_client *client,
				    u8 m, u8 n,
				    u8 p1, u8 p2, u8 p3,
				    u8 p4, u8 p5, u8 p6,
				    u8 p7)
{
	int ret;
	u16 val;

	/* N/M */
	val = (n << 8) |
	      (m << 0);
	mt9t112_reg_mask_set(ret, client, 0x0010, 0x3fff, val);

	/* P1/P2/P3 */
	val = ((p3 & 0x0F) << 8) |
	      ((p2 & 0x0F) << 4) |
	      ((p1 & 0x0F) << 0);
	mt9t112_reg_mask_set(ret, client, 0x0012, 0x0fff, val);

	/* P4/P5/P6 */
	val = (0x7         << 12) |
	      ((p6 & 0x0F) <<  8) |
	      ((p5 & 0x0F) <<  4) |
	      ((p4 & 0x0F) <<  0);
	mt9t112_reg_mask_set(ret, client, 0x002A, 0x7fff, val);

	/* P7 */
	val = (0x1         << 12) |
	      ((p7 & 0x0F) <<  0);
	mt9t112_reg_mask_set(ret, client, 0x002C, 0x100f, val);

	return ret;
}

static int mt9t112_set_resolution_params(const struct i2c_client *client)
{
	int ret = 1;
	struct mt9t112_priv *priv = to_mt9t112(client);
	struct mt9t112_resolution_param *resolution = &priv->resolution;

	if ((priv->frame.width == 1280) && (priv->frame.height == 720)) {
		resolution->col_strt    = 0x0004;
		resolution->row_end     = 0x05AD;
		resolution->col_end     = 0x050B;
		resolution->read_mode   = 0x002C;
		resolution->fine_cor    = 0x008C;
		resolution->fine_min    = 0x01F1;
		resolution->fine_max    = 0x00FF;
		resolution->base_lines  = 0x032D;
		resolution->min_lin_len = 0x0378;
		resolution->con_width   = 0x0508;
		resolution->con_height  = 0x02D8;
		resolution->per_50_M    = 0x00;
		resolution->tx_water    = 0x0210;
		resolution->max_fd_60   = 0x0004;
		if (priv->info->flags & MT9T112_FLAG_HISPEED) {
			resolution->line_len    = 0x0833;
			resolution->s_f1_50     = 0x20;
			resolution->s_f2_50     = 0x22;
			resolution->s_f1_60     = 0x27;
			resolution->s_f2_60     = 0x29;
			resolution->per_50      = 0xF4;
			resolution->per_60      = 0xCB;
			resolution->fd_w_height = 0x06;
			resolution->max_fd_50   = 0x0003;
			resolution->targ_fd     = 0x0003;
		} else {
			resolution->line_len    = 0x091C;
			resolution->s_f1_50     = 0x23;
			resolution->s_f2_50     = 0x25;
			resolution->s_f1_60     = 0x2B;
			resolution->s_f2_60     = 0x2D;
			resolution->per_50      = 0xDC;
			resolution->per_60      = 0xB7;
			resolution->fd_w_height = 0x05;
			resolution->max_fd_50   = 0x0004;
			resolution->targ_fd     = 0x0004;
		}
	} else if ((priv->frame.width <= 1024) && (priv->frame.height <= 768) &&
		   (priv->frame.width != priv->frame.height)) {
		resolution->col_strt    = 0x000;
		resolution->row_end     = 0x60D;
		resolution->col_end     = 0x80D;
		resolution->read_mode   = 0x046C;
		resolution->fine_cor    = 0x00CC;
		resolution->fine_min    = 0x0381;
		resolution->fine_max    = 0x024F;
		resolution->base_lines  = 0x0364;
		resolution->min_lin_len = 0x05D0;
		resolution->line_len    = 0x07AC;
		resolution->con_width   = 0x0408;
		resolution->con_height  = 0x0308;
		resolution->s_f1_50     = 0x23;
		resolution->s_f2_50     = 0x25;
		resolution->s_f1_60     = 0x2A;
		resolution->s_f2_60     = 0x2C;
		resolution->per_50      = 0x05;
		resolution->per_50_M    = 0x01;
		resolution->per_60      = 0xD9;
		resolution->fd_w_height = 0x06;
		resolution->max_fd_50   = 0x0003;
		resolution->max_fd_60   = 0x0004;
		resolution->targ_fd     = 0x0003;
		if ((priv->frame.width == 1024) && (priv->frame.height == 768)) {
			resolution->tx_water = 0x0218;
		} else if ((priv->frame.width == 800) && (priv->frame.height == 480)) {
			resolution->tx_water = 0x02DA;
		} else { // 640 x 480 but use it with everything else until we figure out how to calc it
			resolution->tx_water = 0x0352;
		}
	} else {
		ret = 0;
	}

	return ret;
}

static int mt9t112_pll_setup_custom_pll(const struct i2c_client *client)
{
/*
; Bypass PLL: Unchecked
; Input Frequency: 32.000
; Target Pads Frequency: 96.000
; Target I2C Clock Frequency: 100.000
; Target VCO Frequency: Unspecified
; For Parallel Output: Checked
; "M" Value: Unspecified
; "N" Value: Unspecified
;
; Target Pads Clock Frequency: 96 MHz
; Input Clock Frequency: 32 MHz
;
; Actual Pads Clock Frequency: 96 MHz
; Sensor Core Clock Frequency: 54.857 MHz
; SOC Clock Frequency: 54.857 MHz
; MCU Clock Frequency: 96 MHz
; I2C Master Clock Frequency: 99.740 KHz
;
; M = 24
; N = 1
; Fpdf = 16 MHz
; Fvco = 768 MHz
; P2 = 8
; P4 = 14
; P5 = 14
; P6 = 8
*/
	int data, i, ret;

	mt9t112_reg_mask_set(ret, client, 0x14, 1, 1);	// Bypass PLL
	mt9t112_reg_mask_set(ret, client, 0X14, 2, 0);	// Power-down PLL
	mt9t112_reg_write(ret, client, 0x0014, 0x2145);	// PLL control: BYPASS PLL = 8517
	mt9t112_reg_write(ret, client, 0x0010, 0x0118);	// PLL Dividers = 280
	mt9t112_reg_write(ret, client, 0x0012, 0x0070);	// PLL P Dividers = 112
	mt9t112_reg_write(ret, client, 0x002A, 0x77EE);	// PLL P Dividers 4-5-6 = 30685
	mt9t112_reg_write(ret, client, 0x001A, 0x218);	// Reset Misc. Control = 536
	mt9t112_reg_write(ret, client, 0x0014, 0x2545);	// PLL control: TEST_BYPASS on = 9541
	mt9t112_reg_write(ret, client, 0x0014, 0x2547);	// PLL control: PLL_ENABLE on = 9543
	mt9t112_reg_write(ret, client, 0x0014, 0x2447);	// PLL control: SEL_LOCK_DET on = 9287
	mt9t112_reg_write(ret, client, 0x0014, 0x2047);	// PLL control: TEST_BYPASS off = 8263

	// Wait for the PLL to lock
	for (i=0; i<1000; i++) {
		mt9t112_reg_read(data, client, 0x0014);
		if (0x8000 & data)
			break;

		mdelay(10);
	}

	mt9t112_reg_write(ret, client, 0x0014, 0x2046);	// PLL control: PLL_BYPASS off = 8262
	mt9t112_reg_write(ret, client, 0x0022, 0x0280);	// Reference clock count for 20 us = 640
	mt9t112_reg_write(ret, client, 0x001E, 0x0777);	// Pad Slew Rate = 1911
	mt9t112_reg_write(ret, client, 0x0016, 0x0400);	// JPEG Clock = 1024

	return ret;
}

static int mt9t112_sysctl_startup_K26A_rev_3(const struct i2c_client *client)
{
	int ret;

	// reset
	mt9t112_reset(client);

	// Setup PLL
	mt9t112_pll_setup_custom_pll(client);

	// crank up the output slew rate (don't forget to enable these bits in TX_SS)
	mt9t112_reg_write(ret, client, 0x001E, 0x0777);

	return ret;
}

static int mt9t112_high_speed_overrides(const struct i2c_client *client)
{
	int ret;

// Use this section to apply settings that are specific to this revision of SOC
// or for any other specialized settings

// clear the "Output Buffer Enable Adaptive Clock" bit to enable the SYSCTL
// slew rate settings, change this in the variables and register

	// PRI_A_CONFIG_JPEG_OB_TX_CONTROL_VAR
	mt9t112_mcu_write(ret, client, VAR(26, 160), 0x082E);
	// PRI_B_CONFIG_JPEG_OB_TX_CONTROL_VAR
	mt9t112_mcu_write(ret, client, VAR(27, 160), 0x082E);
	//SEC_A_CONFIG_JPEG_OB_TX_CONTROL_VAR
	mt9t112_mcu_write(ret, client, VAR(28, 160), 0x082E);
	//SEC_B_CONFIG_JPEG_OB_TX_CONTROL_VAR
	mt9t112_mcu_write(ret, client, VAR(29, 160), 0x082E);
	mt9t112_reg_mask_set(ret, client, 0x3C52, 0x0040, 0);         // set this value in HW

	// Set correct values for Context B FIFO control
	// CAM1_CTX_B_RX_FIFO_TRIGGER_MARK
	mt9t112_mcu_write(ret, client, VAR(18, 142), 32);
	// PRI_B_CONFIG_IO_OB_MANUAL_FLAG
	mt9t112_mcu_write(ret, client, VAR(27, 172), 0);

	return ret;
}

static int mt9t112_go(const struct i2c_client *client)
{
	int data, i, ret;

	// release MCU from standby
	mt9t112_reg_mask_set(ret, client, 0x0018, 0x0001, 0);

	// wait for K26A to come out of standby
	for (i=0; i<100; i++) {
		mt9t112_reg_read(data, client, 0x0018);
		if (!(0x4000 & data))
			break;

		mdelay(10);
	}

	return ret;
}

static int mt9t112_continue(const struct i2c_client *client)
{
	int data, i, ret;

	// clear powerup stop bit
	mt9t112_reg_mask_set(ret, client, 0x0018, 0x0004, 0);

	// wait for sequencer to enter preview state
	for (i=0; i<100; i++) {
		mt9t112_mcu_read(data, client, VAR8(1, 1));
		if (data == 3)
			break;

		mdelay(10);
	}

	return ret;
}

static int mt9t112_mcu_powerup_stop_enable(const struct i2c_client *client)
{
	int ret;

	// set powerup stop bit
	mt9t112_reg_mask_set(ret, client, 0x0018, 0x0004, 1);

	return ret;
}

static int mt9t112_custom_setup(const struct i2c_client *client)
{
	struct mt9t112_priv *priv = to_mt9t112(client);
	struct mt9t112_resolution_param *resolution = &priv->resolution;
	int ret;

	//I2C Master Clock Divider
	mt9t112_mcu_write(ret, client, 0x6006, 0x0100);	//      = 275
	//Output Width (A)
	mt9t112_mcu_write(ret, client, 0x6800, priv->frame.width);
	//Output Height (A)
	mt9t112_mcu_write(ret, client, 0x6802, priv->frame.height);
	//JPEG (A)
	mt9t112_mcu_write(ret, client, 0xE88E, 0x00);	//      = 0
	//Adaptive Output Clock (A)
	mt9t112_mcu_mask_set(ret, client, 0x68A0, 0x0040, 0x0000);	//      = 0
	//Row Start (A)
	mt9t112_mcu_write(ret, client, 0x4802, 0x000);	//      = 0
	//Column Start (A)
	mt9t112_mcu_write(ret, client, 0x4804, resolution->col_strt);
	//Row End (A)
	mt9t112_mcu_write(ret, client, 0x4806, resolution->row_end);
	//Column End (A)
	mt9t112_mcu_write(ret, client, 0x4808, resolution->col_end);
	//Row Speed (A)
	mt9t112_mcu_write(ret, client, 0x480A, 0x0111);	//      = 273
	//Read Mode (A)
	mt9t112_mcu_write(ret, client, 0x480C, resolution->read_mode);
	//Fine Correction (A)
	mt9t112_mcu_write(ret, client, 0x480F, resolution->fine_cor);
	//Fine IT Min (A)
	mt9t112_mcu_write(ret, client, 0x4811, resolution->fine_min);
	//Fine IT Max Margin (A)
	mt9t112_mcu_write(ret, client, 0x4813, resolution->fine_max);
	//Base Frame Lines (A)
	mt9t112_mcu_write(ret, client, 0x481D, resolution->base_lines);
	//Min Line Length (A)
	mt9t112_mcu_write(ret, client, 0x481F, resolution->min_lin_len);
	//Line Length (A)
	mt9t112_mcu_write(ret, client, 0x4825, resolution->line_len);
	//Contex Width (A)
	mt9t112_mcu_write(ret, client, 0x482B, resolution->con_width);
	//Context Height (A)
	mt9t112_mcu_write(ret, client, 0x482D, resolution->con_height);
	//Output Width (B)
	mt9t112_mcu_write(ret, client, 0x6C00, 0x0800);	//      = 2048
	//Output Height (B)
	mt9t112_mcu_write(ret, client, 0x6C02, 0x0600);	//      = 1536
	//JPEG (B)
	mt9t112_mcu_write(ret, client, 0xEC8E, 0x01);	//      = 1
	//Adaptive Output Clock (B)
	mt9t112_mcu_mask_set(ret, client, 0x6CA0, 0x0040, 0x0000);	//      = 0
	//Row Start (B)
	mt9t112_mcu_write(ret, client, 0x484A, 0x004);	//      = 4
	//Column Start (B)
	mt9t112_mcu_write(ret, client, 0x484C, 0x004);	//      = 4
	//Row End (B)
	mt9t112_mcu_write(ret, client, 0x484E, 0x60B);	//      = 1547
	//Column End (B)
	mt9t112_mcu_write(ret, client, 0x4850, 0x80B);	//      = 2059
	//Row Speed (B)
	mt9t112_mcu_write(ret, client, 0x4852, 0x0111);	//      = 273
	//Read Mode (B)
	mt9t112_mcu_write(ret, client, 0x4854, 0x0024);	//      = 36
	//Fine Correction (B)
	mt9t112_mcu_write(ret, client, 0x4857, 0x008C);	//      = 140
	//Fine IT Min (B)
	mt9t112_mcu_write(ret, client, 0x4859, 0x01F1);	//      = 497
	//Fine IT Max Margin (B)
	mt9t112_mcu_write(ret, client, 0x485B, 0x00FF);	//      = 255
	//Base Frame Lines (B)
	mt9t112_mcu_write(ret, client, 0x4865, 0x06AE);	//      = 1710
	//Min Line Length (B)
	mt9t112_mcu_write(ret, client, 0x4867, 0x0378);	//      = 888
	//Line Length (B)
	mt9t112_mcu_write(ret, client, 0x486D, 0x0A3A);	//      = 2618
	//Contex Width (B)
	mt9t112_mcu_write(ret, client, 0x4873, 0x0808);	//      = 2056
	//Context Height (B)
	mt9t112_mcu_write(ret, client, 0x4875, 0x0608);	//      = 1544
	//search_f1_50
	mt9t112_mcu_write(ret, client, 0xC8A5, resolution->s_f1_50);
	//search_f2_50
	mt9t112_mcu_write(ret, client, 0xC8A6, resolution->s_f2_50);
	//search_f1_60
	mt9t112_mcu_write(ret, client, 0xC8A7, resolution->s_f1_60);
	//search_f2_60
	mt9t112_mcu_write(ret, client, 0xC8A8, resolution->s_f2_60);
	//period_50Hz (A)
	mt9t112_mcu_write(ret, client, 0xC844, resolution->per_50);
	//period_50Hz (A MSB)
	mt9t112_mcu_write(ret, client, 0xC92F, resolution->per_50_M);
	//period_60Hz (A)
	mt9t112_mcu_write(ret, client, 0xC845, resolution->per_60);
	//period_60Hz (A MSB)
	mt9t112_mcu_write(ret, client, 0xC92D, 0x00);	//      = 0
	//period_50Hz (B)
	mt9t112_mcu_write(ret, client, 0xC88C, 0xD2);	//      = 210
	//period_50Hz (B) MSB
	mt9t112_mcu_write(ret, client, 0xC930, 0x00);	//      = 0
	//period_60Hz (B)
	mt9t112_mcu_write(ret, client, 0xC88D, 0xAF);	//      = 175
	//period_60Hz (B) MSB
	mt9t112_mcu_write(ret, client, 0xC92E, 0x00);	//      = 0
	//FD Window Height
	mt9t112_mcu_write(ret, client, 0xB825, resolution->fd_w_height);
	//Stat_min
	mt9t112_mcu_write(ret, client, 0xA009, 0x02);	//      = 2
	//Stat_max
	mt9t112_mcu_write(ret, client, 0xA00A, 0x03);	//      = 3
	//Min_amplitude
	mt9t112_mcu_write(ret, client, 0xA00C, 0x0A);	//      = 10
	//RX FIFO Watermark (A)
	mt9t112_mcu_write(ret, client, 0x4846, 0x0080);	//      = 128
	//TX FIFO Watermark (A)
	mt9t112_mcu_write(ret, client, 0x68AA, resolution->tx_water);
	//Max FD Zone 50 Hz
	mt9t112_mcu_write(ret, client, 0x6815, resolution->max_fd_50);
	//Max FD Zone 60 Hz
	mt9t112_mcu_write(ret, client, 0x6817, resolution->max_fd_60);
	//AE Target FD Zone
	mt9t112_mcu_write(ret, client, 0x682D, resolution->targ_fd);
	//RX FIFO Watermark (B)
	mt9t112_mcu_write(ret, client, 0x488E, 0x0080);	//      = 128
	//TX FIFO Watermark (B)
	mt9t112_mcu_write(ret, client, 0x6CAA, 0x01D0);	//      = 464
	//Refresh Sequencer Mode
	mt9t112_mcu_write(ret, client, 0x8400, 0x06);	//      = 6
	//Refresh Sequencer
	mt9t112_mcu_write(ret, client, 0x8400, 0x05);	//      = 5

#ifdef TEST_PATTERN
	mt9t112_mcu_write(ret, client, VAR(24, 0x03), 0x100);
	mt9t112_mcu_write(ret, client, VAR(24, 0x25), 0x0B);            // B - Color Bar Test Pattern (supposed to be 6 ?)
#endif

	return ret;
}

static int mt9t112_optimal_power_consumption(const struct i2c_client *client)
{
	int ret;

	// Analog setting B

	mt9t112_reg_write(ret, client, 0x3084, 0x2409);
	mt9t112_reg_write(ret, client, 0x3092, 0x0A49);
	mt9t112_reg_write(ret, client, 0x3094, 0x4949);
	mt9t112_reg_write(ret, client, 0x3096, 0x4950);

	return ret;
}

static int mt9t112_blooming_row_pattern(const struct i2c_client *client)
{
	int ret;

	// Improve high light image quality
	// [CAM1_CTX_A_COARSE_ITMIN]
	mt9t112_mcu_write(ret, client, 0x4815, 0x0004);
	// [CAM1_CTX_B_COARSE_ITMIN]
	mt9t112_mcu_write(ret, client, 0x485D, 0x0004);

	return ret;
}

static int mt9t112_set_orientation(const struct i2c_client *client, int flip)
{
	int ret;

	// if flip = 0 set [normal]
	// if flip = 1 set [horizontal mirror]
	// if flip = 2 set [vertical flip]
	// if flip = 3 set [rotate 180]
	flip &= 0x3;

	// [CAM1_CTX_A_READ_MODE]
	mt9t112_mcu_mask_set(ret, client, VAR(18, 0x000C), 0x0003, flip);
	// [CAM1_CTX_A_PIXEL_ORDER]
	mt9t112_mcu_write(ret, client, VAR8(18, 0x000E), flip);

	// [CAM1_CTX_B_READ_MODE]
	mt9t112_mcu_mask_set(ret, client, VAR(18, 0x0054), 0x0003, flip);
	// [CAM1_CTX_B_PIXEL_ORDER]
	mt9t112_mcu_write(ret, client, VAR8(18, 0x0056), flip);

	// [SEQ_CMD]
	mt9t112_mcu_write(ret, client, VAR8(1, 0), 0x06);

	return ret;
}

static int mt9t112_init_camera_optimized(const struct i2c_client *client)
{
	int ret;

	// basic startup
	ECHECKER(ret, mt9t112_sysctl_startup_K26A_rev_3(client));

	// enable powerup stop
	ECHECKER(ret, mt9t112_mcu_powerup_stop_enable(client));

	// start MCU
	ECHECKER(ret, mt9t112_go(client));

	// customize the configuration
	ECHECKER(ret, mt9t112_custom_setup(client));

	// enable operation with fastest clocks
	ECHECKER(ret, mt9t112_high_speed_overrides(client));

	// Optimal power consumption setting
	ECHECKER(ret, mt9t112_optimal_power_consumption(client));

	// load anti-blooming settings
	ECHECKER(ret, mt9t112_blooming_row_pattern(client));

	// continue after powerup stop
	ECHECKER(ret, mt9t112_continue(client));

	return ret;
}

static int mt9t112_init_pll(const struct i2c_client *client)
{
	struct mt9t112_priv *priv = to_mt9t112(client);
	int data, i, ret;

	mt9t112_reg_mask_set(ret, client, 0x0014, 0x003, 0x0001);

	/* PLL control: BYPASS PLL = 8517 */
	mt9t112_reg_write(ret, client, 0x0014, 0x2145);

	/* Replace these registers when new timing parameters are generated */
	mt9t112_set_pll_dividers(client,
				 priv->info->divider.m,
				 priv->info->divider.n,
				 priv->info->divider.p1,
				 priv->info->divider.p2,
				 priv->info->divider.p3,
				 priv->info->divider.p4,
				 priv->info->divider.p5,
				 priv->info->divider.p6,
				 priv->info->divider.p7);

	/*
	 * TEST_BYPASS  on
	 * PLL_ENABLE   on
	 * SEL_LOCK_DET on
	 * TEST_BYPASS  off
	 */
	mt9t112_reg_write(ret, client, 0x0014, 0x2525);
	mt9t112_reg_write(ret, client, 0x0014, 0x2527);
	mt9t112_reg_write(ret, client, 0x0014, 0x3427);
	mt9t112_reg_write(ret, client, 0x0014, 0x3027);

	mdelay(10);

	/*
	 * PLL_BYPASS off
	 * Reference clock count
	 * I2C Master Clock Divider
	 */
	mt9t112_reg_write(ret, client, 0x0014, 0x3046);
	mt9t112_reg_write(ret, client, 0x0022, 0x0190);
	mt9t112_reg_write(ret, client, 0x3B84, 0x0212);

	/* External sensor clock is PLL bypass */
	mt9t112_reg_write(ret, client, 0x002E, 0x0500);

	mt9t112_reg_mask_set(ret, client, 0x0018, 0x0002, 0x0002);
	mt9t112_reg_mask_set(ret, client, 0x3B82, 0x0004, 0x0004);

	/* MCU disabled */
	mt9t112_reg_mask_set(ret, client, 0x0018, 0x0004, 0x0004);

	/* out of standby */
	mt9t112_reg_mask_set(ret, client, 0x0018, 0x0001, 0);

	mdelay(50);

	/*
	 * Standby Workaround
	 * Disable Secondary I2C Pads
	 */
	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);
	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);
	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);
	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);
	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);
	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);

	/* poll to verify out of standby. Must Poll this bit */
	for (i = 0; i < 100; i++) {
		mt9t112_reg_read(data, client, 0x0018);
		if (!(0x4000 & data))
			break;

		mdelay(10);
	}

	return ret;
}

static int mt9t112_init_setting(const struct i2c_client *client)
{

	int ret;

	/* Adaptive Output Clock (A) */
	mt9t112_mcu_mask_set(ret, client, VAR(26, 160), 0x0040, 0x0000);

	/* Read Mode (A) */
	mt9t112_mcu_write(ret, client, VAR(18, 12), 0x0024);

	/* Fine Correction (A) */
	mt9t112_mcu_write(ret, client, VAR(18, 15), 0x00CC);

	/* Fine IT Min (A) */
	mt9t112_mcu_write(ret, client, VAR(18, 17), 0x01f1);

	/* Fine IT Max Margin (A) */
	mt9t112_mcu_write(ret, client, VAR(18, 19), 0x00fF);

	/* Base Frame Lines (A) */
	mt9t112_mcu_write(ret, client, VAR(18, 29), 0x032D);

	/* Min Line Length (A) */
	mt9t112_mcu_write(ret, client, VAR(18, 31), 0x073a);

	/* Line Length (A) */
	mt9t112_mcu_write(ret, client, VAR(18, 37), 0x07d0);

	/* Adaptive Output Clock (B) */
	mt9t112_mcu_mask_set(ret, client, VAR(27, 160), 0x0040, 0x0000);

	/* Row Start (B) */
	mt9t112_mcu_write(ret, client, VAR(18, 74), 0x004);

	/* Column Start (B) */
	mt9t112_mcu_write(ret, client, VAR(18, 76), 0x004);

	/* Row End (B) */
	mt9t112_mcu_write(ret, client, VAR(18, 78), 0x60B);

	/* Column End (B) */
	mt9t112_mcu_write(ret, client, VAR(18, 80), 0x80B);

	/* Fine Correction (B) */
	mt9t112_mcu_write(ret, client, VAR(18, 87), 0x008C);

	/* Fine IT Min (B) */
	mt9t112_mcu_write(ret, client, VAR(18, 89), 0x01F1);

	/* Fine IT Max Margin (B) */
	mt9t112_mcu_write(ret, client, VAR(18, 91), 0x00FF);

	/* Base Frame Lines (B) */
	mt9t112_mcu_write(ret, client, VAR(18, 101), 0x0668);

	/* Min Line Length (B) */
	mt9t112_mcu_write(ret, client, VAR(18, 103), 0x0AF0);

	/* Line Length (B) */
	mt9t112_mcu_write(ret, client, VAR(18, 109), 0x0AF0);

	/*
	 * Flicker Dectection registers
	 * This section should be replaced whenever new Timing file is generated
	 * All the following registers need to be replaced
	 * Following registers are generated from Register Wizard but user can
	 * modify them. For detail see auto flicker detection tuning
	 */

	/* FD_FDPERIOD_SELECT */
	mt9t112_mcu_write(ret, client, VAR8(8, 5), 0x01);

	/* PRI_B_CONFIG_FD_ALGO_RUN */
	mt9t112_mcu_write(ret, client, VAR(27, 17), 0x0003);

	/* PRI_A_CONFIG_FD_ALGO_RUN */
	mt9t112_mcu_write(ret, client, VAR(26, 17), 0x0003);

	/*
	 * AFD range detection tuning registers
	 */

	/* search_f1_50 */
	mt9t112_mcu_write(ret, client, VAR8(18, 165), 0x25);

	/* search_f2_50 */
	mt9t112_mcu_write(ret, client, VAR8(18, 166), 0x28);

	/* search_f1_60 */
	mt9t112_mcu_write(ret, client, VAR8(18, 167), 0x2C);

	/* search_f2_60 */
	mt9t112_mcu_write(ret, client, VAR8(18, 168), 0x2F);

	/* period_50Hz (A) */
	mt9t112_mcu_write(ret, client, VAR8(18, 68), 0xBA);

	/* secret register by aptina */
	/* period_50Hz (A MSB) */
	mt9t112_mcu_write(ret, client, VAR8(18, 303), 0x00);

	/* period_60Hz (A) */
	mt9t112_mcu_write(ret, client, VAR8(18, 69), 0x9B);

	/* secret register by aptina */
	/* period_60Hz (A MSB) */
	mt9t112_mcu_write(ret, client, VAR8(18, 301), 0x00);

	/* period_50Hz (B) */
	mt9t112_mcu_write(ret, client, VAR8(18, 140), 0x82);

	/* secret register by aptina */
	/* period_50Hz (B) MSB */
	mt9t112_mcu_write(ret, client, VAR8(18, 304), 0x00);

	/* period_60Hz (B) */
	mt9t112_mcu_write(ret, client, VAR8(18, 141), 0x6D);

	/* secret register by aptina */
	/* period_60Hz (B) MSB */
	mt9t112_mcu_write(ret, client, VAR8(18, 302), 0x00);

	/* FD Mode */
	mt9t112_mcu_write(ret, client, VAR8(8, 2), 0x10);

	/* Stat_min */
	mt9t112_mcu_write(ret, client, VAR8(8, 9), 0x02);

	/* Stat_max */
	mt9t112_mcu_write(ret, client, VAR8(8, 10), 0x03);

	/* Min_amplitude */
	mt9t112_mcu_write(ret, client, VAR8(8, 12), 0x0A);

	/* RX FIFO Watermark (A) */
	mt9t112_mcu_write(ret, client, VAR(18, 70), 0x0014);

	/* RX FIFO Watermark (B) */
	mt9t112_mcu_write(ret, client, VAR(18, 142), 0x0014);

	/* MCLK: 16MHz
	 * PCLK: 73MHz
	 * CorePixCLK: 36.5 MHz
	 */
	mt9t112_mcu_write(ret, client, VAR8(18, 0x0044), 133);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x0045), 110);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x008c), 130);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x008d), 108);

	mt9t112_mcu_write(ret, client, VAR8(18, 0x00A5), 27);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x00a6), 30);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x00a7), 32);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x00a8), 35);

	return ret;
}

static int mt9t112_auto_focus_setting(const struct i2c_client *client)
{
	int ret;

	mt9t112_mcu_write(ret, client, VAR(12, 13),	0x000F);
	mt9t112_mcu_write(ret, client, VAR(12, 23),	0x0F0F);
	mt9t112_mcu_write(ret, client, VAR8(1, 0),	0x06);

	mt9t112_reg_write(ret, client, 0x0614, 0x0000);

	mt9t112_mcu_write(ret, client, VAR8(1, 0),	0x05);
	mt9t112_mcu_write(ret, client, VAR8(12, 2),	0x02);
	mt9t112_mcu_write(ret, client, VAR(12, 3),	0x0002);
	mt9t112_mcu_write(ret, client, VAR(17, 3),	0x8001);
	mt9t112_mcu_write(ret, client, VAR(17, 11),	0x0025);
	mt9t112_mcu_write(ret, client, VAR(17, 13),	0x0193);
	mt9t112_mcu_write(ret, client, VAR8(17, 33),	0x18);
	mt9t112_mcu_write(ret, client, VAR8(1, 0),	0x05);

	return ret;
}

static int mt9t112_auto_focus_trigger(const struct i2c_client *client)
{
	int ret;

	mt9t112_mcu_write(ret, client, VAR8(12, 25), 0x01);

	return ret;
}

static int mt9t112_init_camera(const struct i2c_client *client)
{
	int ret;

	ECHECKER(ret, mt9t112_reset(client));

	ECHECKER(ret, mt9t112_init_pll(client));

	ECHECKER(ret, mt9t112_init_setting(client));

	ECHECKER(ret, mt9t112_auto_focus_setting(client));

	mt9t112_reg_mask_set(ret, client, 0x0018, 0x0004, 0);

	/* Analog setting B */
	mt9t112_reg_write(ret, client, 0x3084, 0x2409);
	mt9t112_reg_write(ret, client, 0x3092, 0x0A49);
	mt9t112_reg_write(ret, client, 0x3094, 0x4949);
	mt9t112_reg_write(ret, client, 0x3096, 0x4950);

	/*
	 * Disable adaptive clock
	 * PRI_A_CONFIG_JPEG_OB_TX_CONTROL_VAR
	 * PRI_B_CONFIG_JPEG_OB_TX_CONTROL_VAR
	 */
	mt9t112_mcu_write(ret, client, VAR(26, 160), 0x0A2E);
	mt9t112_mcu_write(ret, client, VAR(27, 160), 0x0A2E);

	/* Configure STatus in Status_before_length Format and enable header */
	/* PRI_B_CONFIG_JPEG_OB_TX_CONTROL_VAR */
	mt9t112_mcu_write(ret, client, VAR(27, 144), 0x0CB4);

	/* Enable JPEG in context B */
	/* PRI_B_CONFIG_JPEG_OB_TX_CONTROL_VAR */
	mt9t112_mcu_write(ret, client, VAR8(27, 142), 0x01);

	/* Disable Dac_TXLO */
	mt9t112_reg_write(ret, client, 0x316C, 0x350F);

	/* Set max slew rates */
	mt9t112_reg_write(ret, client, 0x1E, 0x777);

	return ret;
}

/************************************************************************
			v4l2_subdev_core_ops
************************************************************************/
static int mt9t112_g_chip_ident(struct v4l2_subdev *sd,
				struct v4l2_dbg_chip_ident *id)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);

	id->ident    = priv->model;
	id->revision = 0;

	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int mt9t112_g_register(struct v4l2_subdev *sd,
			      struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int                ret;

	reg->size = 2;
	mt9t112_reg_read(ret, client, reg->reg);

	reg->val = (__u64)ret;

	return 0;
}

static int mt9t112_s_register(struct v4l2_subdev *sd,
			      struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	mt9t112_reg_write(ret, client, reg->reg, reg->val);

	return ret;
}
#endif

static struct v4l2_subdev_core_ops mt9t112_subdev_core_ops = {
	.g_chip_ident	= mt9t112_g_chip_ident,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= mt9t112_g_register,
	.s_register	= mt9t112_s_register,
#endif
};


/************************************************************************
			v4l2_subdev_video_ops
************************************************************************/
static int mt9t112_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);
	int ret = 0;
	int optimize = 0;

	if (!enable) {
		/* FIXME
		 *
		 * If user selected large output size,
		 * and used it long time,
		 * mt9t112 camera will be very warm.
		 *
		 * But current driver can not stop mt9t112 camera.
		 * So, set small size here to solve this problem.
		 */
		mt9t112_set_a_frame_size(client, VGA_WIDTH, VGA_HEIGHT);
		return ret;
	}

	// fill the structure with new resolution parameters
	optimize = mt9t112_set_resolution_params(client);

	if (optimize)
		ECHECKER(ret, mt9t112_init_camera_optimized(client));
	else
		ECHECKER(ret, mt9t112_init_camera(client));

	/* Invert PCLK (Data sampled on falling edge of pixclk) */
	mt9t112_reg_write(ret, client, 0x3C20, (PCLK_RISING & priv->flags ? 0x0001 : 0x0000));
	mdelay(5);

	mt9t112_mcu_write(ret, client, VAR(26, 7), priv->format->fmt);
	mt9t112_mcu_write(ret, client, VAR(26, 9), priv->format->order);
	mt9t112_mcu_write(ret, client, VAR8(1, 0), 0x06);

	if (!optimize) {
		mt9t112_set_a_frame_size(client,
				 priv->frame.width,
				 priv->frame.height);

		ECHECKER(ret, mt9t112_auto_focus_trigger(client));
	}

	if (priv->info->flags & MT9T112_FLAG_VFLIP) {
		ECHECKER(ret, mt9t112_set_orientation(client, 2));
	}

	dev_dbg(&client->dev, "format : %d\n", priv->format->code);
	dev_dbg(&client->dev, "size   : %d x %d\n",
		priv->frame.width,
		priv->frame.height);

	CLOCK_INFO(client, EXT_CLOCK);

	return ret;
}

static int mt9t112_set_params(struct mt9t112_priv *priv,
			      const struct v4l2_rect *rect,
			      enum v4l2_mbus_pixelcode code)
{
	int i;

	/*
	 * get color format
	 */
	for (i = 0; i < ARRAY_SIZE(mt9t112_cfmts); i++)
		if (mt9t112_cfmts[i].code == code)
			break;

	if (i == ARRAY_SIZE(mt9t112_cfmts))
		return -EINVAL;

	priv->frame  = *rect;

	/*
	 * frame size check
	 */
	mt9t112_frame_check(&priv->frame.width, &priv->frame.height,
			    &priv->frame.left, &priv->frame.top);

	priv->format = mt9t112_cfmts + i;

	return 0;
}

static int mt9t112_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left			= 0;
	a->bounds.top			= 0;
	a->bounds.width			= MAX_WIDTH;
	a->bounds.height		= MAX_HEIGHT;
	a->defrect.left			= 0;
	a->defrect.top			= 0;
	a->defrect.width		= VGA_WIDTH;
	a->defrect.height		= VGA_HEIGHT;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int mt9t112_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);

	a->c	= priv->frame;
	a->type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int mt9t112_s_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);
	struct v4l2_rect *rect = &a->c;

	return mt9t112_set_params(priv, rect, priv->format->code);
}

static int mt9t112_g_fmt(struct v4l2_subdev *sd,
			 struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);

	mf->width	= priv->frame.width;
	mf->height	= priv->frame.height;
	mf->colorspace	= priv->format->colorspace;
	mf->code	= priv->format->code;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int mt9t112_s_fmt(struct v4l2_subdev *sd,
			 struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);
	struct v4l2_rect rect = {
		.width = mf->width,
		.height = mf->height,
		.left = priv->frame.left,
		.top = priv->frame.top,
	};
	int ret;

	ret = mt9t112_set_params(priv, &rect, mf->code);

	if (!ret)
		mf->colorspace = priv->format->colorspace;

	return ret;
}

static int mt9t112_try_fmt(struct v4l2_subdev *sd,
			   struct v4l2_mbus_framefmt *mf)
{
	unsigned int top, left;
	int i;

	for (i = 0; i < ARRAY_SIZE(mt9t112_cfmts); i++)
		if (mt9t112_cfmts[i].code == mf->code)
			break;

	if (i == ARRAY_SIZE(mt9t112_cfmts)) {
		mf->code = V4L2_MBUS_FMT_UYVY8_2X8;
		mf->colorspace = V4L2_COLORSPACE_JPEG;
	} else {
		mf->colorspace	= mt9t112_cfmts[i].colorspace;
	}

	mt9t112_frame_check(&mf->width, &mf->height, &left, &top);

	mf->field = V4L2_FIELD_NONE;

	return 0;
}

static int mt9t112_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(mt9t112_cfmts))
		return -EINVAL;

	*code = mt9t112_cfmts[index].code;

	return 0;
}

static int mt9t112_g_mbus_config(struct v4l2_subdev *sd,
				 struct v4l2_mbus_config *cfg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_link *icl = soc_camera_i2c_to_link(client);

	cfg->flags = V4L2_MBUS_MASTER | V4L2_MBUS_VSYNC_ACTIVE_HIGH |
		V4L2_MBUS_HSYNC_ACTIVE_HIGH | V4L2_MBUS_DATA_ACTIVE_HIGH |
		V4L2_MBUS_PCLK_SAMPLE_RISING | V4L2_MBUS_PCLK_SAMPLE_FALLING;
	cfg->type = V4L2_MBUS_PARALLEL;
	cfg->flags = soc_camera_apply_board_flags(icl, cfg);

	return 0;
}

static int mt9t112_s_mbus_config(struct v4l2_subdev *sd,
				 const struct v4l2_mbus_config *cfg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_link *icl = soc_camera_i2c_to_link(client);
	struct mt9t112_priv *priv = to_mt9t112(client);

	if (soc_camera_apply_board_flags(icl, cfg) & V4L2_MBUS_PCLK_SAMPLE_RISING)
		priv->flags |= PCLK_RISING;

	return 0;
}

static struct v4l2_subdev_video_ops mt9t112_subdev_video_ops = {
	.s_stream	= mt9t112_s_stream,
	.g_mbus_fmt	= mt9t112_g_fmt,
	.s_mbus_fmt	= mt9t112_s_fmt,
	.try_mbus_fmt	= mt9t112_try_fmt,
	.cropcap	= mt9t112_cropcap,
	.g_crop		= mt9t112_g_crop,
	.s_crop		= mt9t112_s_crop,
	.enum_mbus_fmt	= mt9t112_enum_fmt,
	.g_mbus_config	= mt9t112_g_mbus_config,
	.s_mbus_config	= mt9t112_s_mbus_config,
};

/************************************************************************
			i2c driver
************************************************************************/
static struct v4l2_subdev_ops mt9t112_subdev_ops = {
	.core	= &mt9t112_subdev_core_ops,
	.video	= &mt9t112_subdev_video_ops,
};

static int mt9t112_camera_probe(struct i2c_client *client)
{
	struct mt9t112_priv *priv = to_mt9t112(client);
	const char          *devname;
	int                  chipid;

	/*
	 * check and show chip ID
	 */
	mt9t112_reg_read(chipid, client, 0x0000);

	switch (chipid) {
	case 0x2680:
		devname = "mt9t111";
		priv->model = V4L2_IDENT_MT9T111;
		break;
	case 0x2682:
		devname = "mt9t112";
		priv->model = V4L2_IDENT_MT9T112;
		break;
	default:
		dev_err(&client->dev, "Product ID error %04x\n", chipid);
		return -ENODEV;
	}

	dev_info(&client->dev, "%s chip ID %04x\n", devname, chipid);

	return 0;
}

static int mt9t112_probe(struct i2c_client *client,
			 const struct i2c_device_id *did)
{
	struct mt9t112_priv *priv;
	struct soc_camera_link *icl = soc_camera_i2c_to_link(client);
	struct v4l2_rect rect = {
		.width = VGA_WIDTH,
		.height = VGA_HEIGHT,
		.left = (MAX_WIDTH - VGA_WIDTH) / 2,
		.top = (MAX_HEIGHT - VGA_HEIGHT) / 2,
	};
	int ret;

	if (!icl || !icl->priv) {
		dev_err(&client->dev, "mt9t112: missing platform data!\n");
		return -EINVAL;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->info = icl->priv;

	v4l2_i2c_subdev_init(&priv->subdev, client, &mt9t112_subdev_ops);

	ret = mt9t112_camera_probe(client);
	if (ret) {
		kfree(priv);
		return ret;
	}

	/* Cannot fail: using the default supported pixel code */
	mt9t112_set_params(priv, &rect, V4L2_MBUS_FMT_UYVY8_2X8);

	return ret;
}

static int mt9t112_remove(struct i2c_client *client)
{
	struct mt9t112_priv *priv = to_mt9t112(client);

	kfree(priv);
	return 0;
}

static const struct i2c_device_id mt9t112_id[] = {
	{ "mt9t112", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mt9t112_id);

static struct i2c_driver mt9t112_i2c_driver = {
	.driver = {
		.name = "mt9t112",
	},
	.probe    = mt9t112_probe,
	.remove   = mt9t112_remove,
	.id_table = mt9t112_id,
};

/************************************************************************
			module function
************************************************************************/
static int __init mt9t112_module_init(void)
{
	return i2c_add_driver(&mt9t112_i2c_driver);
}

static void __exit mt9t112_module_exit(void)
{
	i2c_del_driver(&mt9t112_i2c_driver);
}

module_init(mt9t112_module_init);
module_exit(mt9t112_module_exit);

MODULE_DESCRIPTION("SoC Camera driver for mt9t112");
MODULE_AUTHOR("Kuninori Morimoto");
MODULE_LICENSE("GPL v2");
