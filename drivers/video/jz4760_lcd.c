/*
 * linux/drivers/video/jz4760_lcd.c -- Ingenic Jz4760 LCD frame buffer device
 *
 * Copyright (C) 2005-2008, Ingenic Semiconductor Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * --------------------------------
 * NOTE:
 * This LCD driver support TFT16 TFT32 LCD, not support STN and Special TFT LCD
 * now.
 * 	It seems not necessory to support STN and Special TFT.
 * 	If it's necessary, update this driver in the future.
 * 	<Wolfgang Wang, Jun 10 2008>
 */
#include <linux/kthread.h>
#include <linux/proc_fs.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/pm.h>

#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/processor.h>
#include <asm/jzsoc.h>

#include "console/fbcon.h"

#include "jz4760_lcd.h"
#include "jz4760_tve.h"

#ifdef CONFIG_JZ4760_SLCD_KGM701A3_TFT_SPFD5420A
#include "jz_kgm_spfd5420a.h"
#endif

MODULE_DESCRIPTION("Jz4760 LCD Controller driver");
MODULE_AUTHOR("Wolfgang Wang, <lgwang@ingenic.cn>");
MODULE_LICENSE("GPL");
//#define RGB_TEST
#define D(fmt, args...) \
	//	printk(KERN_ERR "%s(): "fmt"\n", __func__, ##args)

#define E(fmt, args...) \
	printk(KERN_ERR "%s(): " fmt "\n", __func__, ##args)

#define JZ_FB_DEBUG 0
static int lcd_backlight_level = 102;
struct jz4760lcd_info jz4760_lcd_panel = {
#if defined(CONFIG_JZ4760_LCD_SAMSUNG_LTP400WQF02)
	.panel = {
		.cfg = LCD_CFG_LCDPIN_LCD | LCD_CFG_RECOVER | /* Underrun recover */
			   LCD_CFG_NEWDES |						  /* 8words descriptor */
			   LCD_CFG_MODE_GENERIC_TFT |			  /* General TFT panel */
			   LCD_CFG_MODE_TFT_18BIT |				  /* output 18bpp */
			   LCD_CFG_HSP |						  /* Hsync polarity: active low */
			   LCD_CFG_VSP,							  /* Vsync polarity: leading edge is falling edge */
		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16, /* 16words burst, enable out FIFO underrun irq */
		480,
		272,
		60,
		41,
		10,
		2,
		2,
		2,
		2,
	},
	.osd = {
		.osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
									//		 LCD_OSDC_ALPHAEN | /* enable alpha */
				   LCD_OSDC_F0EN,   /* enable Foreground0 */
		.osd_ctrl = 0,				/* disable ipu,  */
		.rgb_ctrl = 0,
		.bgcolor = 0x000000,		 /* set background color Black */
		.colorkey0 = 0,				 /* disable colorkey */
		.colorkey1 = 0,				 /* disable colorkey */
		.alpha = 0xA0,				 /* alpha value */
		.ipu_restart = 0x80001000,   /* ipu restart */
		.fg_change = FG_CHANGE_ALL,  /* change all initially */
		.fg0 = {32, 0, 0, 480, 272}, /* bpp, x, y, w, h */
		.fg1 = {32, 0, 0, 720, 573}, /* bpp, x, y, w, h */
	},
#elif defined(CONFIG_JZ4760_LCD_AUO_A043FL01V2)
	.panel = {
		.cfg = LCD_CFG_LCDPIN_LCD | LCD_CFG_RECOVER | /* Underrun recover */
			   LCD_CFG_NEWDES |						  /* 8words descriptor */
			   LCD_CFG_MODE_GENERIC_TFT |			  /* General TFT panel */
			   LCD_CFG_MODE_TFT_24BIT |				  /* output 18bpp */
			   LCD_CFG_HSP |						  /* Hsync polarity: active low */
			   LCD_CFG_VSP,							  /* Vsync polarity: leading edge is falling edge */
		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16, /* 16words burst, enable out FIFO underrun irq */
		480,
		272,
		60,
		41,
		10,
		8,
		4,
		4,
		2,
	},
	.osd = {
		.osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
									//		 LCD_OSDC_ALPHAEN | /* enable alpha */
									//		 LCD_OSDC_F1EN | /* enable Foreground1 */
				   LCD_OSDC_F0EN,   /* enable Foreground0 */
		.osd_ctrl = 0,				/* disable ipu,  */
		.rgb_ctrl = 0,
		.bgcolor = 0x000000,		 /* set background color Black */
		.colorkey0 = 0,				 /* disable colorkey */
		.colorkey1 = 0,				 /* disable colorkey */
		.alpha = 0xA0,				 /* alpha value */
		.ipu_restart = 0x80001000,   /* ipu restart */
		.fg_change = FG_CHANGE_ALL,  /* change all initially */
		.fg0 = {16, 0, 0, 480, 272}, /* bpp, x, y, w, h */
		.fg1 = {16, 0, 0, 720, 573}, /* bpp, x, y, w, h */
	},
#elif defined(CONFIG_JZ4760_LCD_TOPPOLY_TD043MGEB1)
	.panel = {
		.cfg = LCD_CFG_LCDPIN_LCD | LCD_CFG_RECOVER | /* Underrun recover */
			   LCD_CFG_NEWDES |						  /* 8words descriptor */
			   LCD_CFG_MODE_GENERIC_TFT |			  /* General TFT panel */
			   LCD_CFG_MODE_TFT_24BIT |				  /* output 18bpp */
			   LCD_CFG_HSP |						  /* Hsync polarity: active low */
			   LCD_CFG_VSP,							  /* Vsync polarity: leading edge is falling edge */
		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16, /* 16words burst, enable out FIFO underrun irq */
		800,
		480,
		60,
		1,
		1,
		40,
		215,
		10,
		34,
	},
	.osd = {
		.osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
									//		 LCD_OSDC_ALPHAEN | /* enable alpha */
									//		 LCD_OSDC_F1EN | /* enable Foreground1 */
				   LCD_OSDC_F0EN,   /* enable Foreground0 */
		.osd_ctrl = 0,				/* disable ipu,  */
		.rgb_ctrl = 0,
		.bgcolor = 0xff,			 /* set background color Black */
		.colorkey0 = 0,				 /* disable colorkey */
		.colorkey1 = 0,				 /* disable colorkey */
		.alpha = 0xA0,				 /* alpha value */
		.ipu_restart = 0x80001000,   /* ipu restart */
		.fg_change = FG_CHANGE_ALL,  /* change all initially */
		.fg0 = {32, 0, 0, 800, 480}, /* bpp, x, y, w, h */
		.fg1 = {32, 0, 0, 800, 480}, /* bpp, x, y, w, h */
	},
#elif defined(CONFIG_JZ4760_LCD_TRULY_TFT_GG1P0319LTSW_W)
	.panel = {
		.cfg = LCD_CFG_LCDPIN_SLCD | /* Underrun recover*/
			   LCD_CFG_NEWDES |		 /* 8words descriptor */
			   LCD_CFG_MODE_SLCD,	/* TFT Smart LCD panel */
		.slcd_cfg = SLCD_CFG_DWIDTH_16BIT | SLCD_CFG_CWIDTH_16BIT | SLCD_CFG_CS_ACTIVE_LOW | SLCD_CFG_RS_CMD_LOW | SLCD_CFG_CLK_ACTIVE_FALLING | SLCD_CFG_TYPE_PARALLEL,
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16, /* 16words burst, enable out FIFO underrun irq */
		240,
		320,
		60,
		0,
		0,
		0,
		0,
		0,
		0,
	},
	.osd = {
		.osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
									//		 LCD_OSDC_ALPHAEN | /* enable alpha */
									//		 LCD_OSDC_F1EN | /* enable Foreground0 */
				   LCD_OSDC_F0EN,   /* enable Foreground0 */
		.osd_ctrl = 0,				/* disable ipu,  */
		.rgb_ctrl = 0,
		.bgcolor = 0x000000,		 /* set background color Black */
		.colorkey0 = 0,				 /* disable colorkey */
		.colorkey1 = 0,				 /* disable colorkey */
		.alpha = 0xA0,				 /* alpha value */
		.ipu_restart = 0x80001000,   /* ipu restart */
		.fg_change = FG_CHANGE_ALL,  /* change all initially */
		.fg0 = {32, 0, 0, 240, 320}, /* bpp, x, y, w, h */
		.fg1 = {32, 0, 0, 240, 320}, /* bpp, x, y, w, h */
	},

#elif defined(CONFIG_JZ4760_LCD_FOXCONN_PT035TN01)
	.panel = {
		.cfg = LCD_CFG_LCDPIN_LCD | LCD_CFG_RECOVER | /* Underrun recover */
			   LCD_CFG_NEWDES |						  /* 8words descriptor */
			   LCD_CFG_MODE_GENERIC_TFT |			  /* General TFT panel */
													  //		LCD_CFG_MODE_TFT_18BIT | 	/* output 18bpp */
			   LCD_CFG_MODE_TFT_24BIT |				  /* output 24bpp */
			   LCD_CFG_HSP |						  /* Hsync polarity: active low */
			   LCD_CFG_VSP |						  /* Vsync polarity: leading edge is falling edge */
			   LCD_CFG_PCP,							  /* Pix-CLK polarity: data translations at falling edge */
		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16, /* 16words burst, enable out FIFO underrun irq */
		320,
		240,
		80,
		1,
		1,
		10,
		50,
		10,
		13},
	.osd = {
		.osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
									//		 LCD_OSDC_ALPHAEN | /* enable alpha */
									//		 LCD_OSDC_F1EN |	/* enable Foreground1 */
				   LCD_OSDC_F0EN,   /* enable Foreground0 */
		.osd_ctrl = 0,				/* disable ipu,  */
		.rgb_ctrl = 0,
		.bgcolor = 0x000000,		 /* set background color Black */
		.colorkey0 = 0,				 /* disable colorkey */
		.colorkey1 = 0,				 /* disable colorkey */
		.alpha = 0xA0,				 /* alpha value */
		.ipu_restart = 0x80001000,   /* ipu restart */
		.fg_change = FG_CHANGE_ALL,  /* change all initially */
		.fg0 = {32, 0, 0, 320, 240}, /* bpp, x, y, w, h */
		.fg1 = {32, 0, 0, 320, 240}, /* bpp, x, y, w, h */
	},
#elif defined(CONFIG_JZ4760_LCD_INNOLUX_PT035TN01_SERIAL)
	.panel = {
		.cfg = LCD_CFG_LCDPIN_LCD | LCD_CFG_RECOVER | /* Underrun recover */
			   LCD_CFG_NEWDES |						  /* 8words descriptor */
			   LCD_CFG_MODE_SERIAL_TFT |			  /* Serial TFT panel */
			   LCD_CFG_MODE_TFT_18BIT |				  /* output 18bpp */
			   LCD_CFG_HSP |						  /* Hsync polarity: active low */
			   LCD_CFG_VSP |						  /* Vsync polarity: leading edge is falling edge */
			   LCD_CFG_PCP,							  /* Pix-CLK polarity: data translations at falling edge */
		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16, /* 16words burst, enable out FIFO underrun irq */
		320,
		240,
		60,
		1,
		1,
		10,
		50,
		10,
		13},
	.osd = {
		.osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
									//		 LCD_OSDC_ALPHAEN | /* enable alpha */
				   LCD_OSDC_F0EN,   /* enable Foreground0 */
		.osd_ctrl = 0,				/* disable ipu,  */
		.rgb_ctrl = 0,
		.bgcolor = 0x000000,		 /* set background color Black */
		.colorkey0 = 0,				 /* disable colorkey */
		.colorkey1 = 0,				 /* disable colorkey */
		.alpha = 0xA0,				 /* alpha value */
		.ipu_restart = 0x80001000,   /* ipu restart */
		.fg_change = FG_CHANGE_ALL,  /* change all initially */
		.fg0 = {32, 0, 0, 320, 240}, /* bpp, x, y, w, h */
		.fg1 = {32, 0, 0, 320, 240}, /* bpp, x, y, w, h */
	},
#elif defined(CONFIG_JZ4760_SLCD_KGM701A3_TFT_SPFD5420A)
	.panel = {
		//		 .cfg = LCD_CFG_LCDPIN_SLCD | LCD_CFG_RECOVER | /* Underrun recover*/
		.cfg = LCD_CFG_LCDPIN_SLCD | /* Underrun recover*/
			   //		 LCD_CFG_DITHER | /* dither */
			   LCD_CFG_NEWDES |   /* 8words descriptor */
			   LCD_CFG_MODE_SLCD, /* TFT Smart LCD panel */
		.slcd_cfg = SLCD_CFG_DWIDTH_18BIT | SLCD_CFG_CWIDTH_18BIT | SLCD_CFG_CS_ACTIVE_LOW | SLCD_CFG_RS_CMD_LOW | SLCD_CFG_CLK_ACTIVE_FALLING | SLCD_CFG_TYPE_PARALLEL,
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16, /* 16words burst, enable out FIFO underrun irq */
		400,
		240,
		60,
		0,
		0,
		0,
		0,
		0,
		0,
	},
	.osd = {
		.osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
									//		 LCD_OSDC_ALPHAEN | /* enable alpha */
									//		 LCD_OSDC_ALPHAMD | /* alpha blending mode */
									//		 LCD_OSDC_F1EN | /* enable Foreground1 */
				   LCD_OSDC_F0EN,   /* enable Foreground0 */
		.osd_ctrl = 0,				/* disable ipu,  */
		.rgb_ctrl = 0,
		.bgcolor = 0x000000,		 /* set background color Black */
		.colorkey0 = 0,				 /* disable colorkey */
		.colorkey1 = 0,				 /* disable colorkey */
		.alpha = 0xA0,				 /* alpha value */
		.ipu_restart = 0x80001000,   /* ipu restart */
		.fg_change = FG_CHANGE_ALL,  /* change all initially */
									 //		 .fg0 = {32, 0, 0, 400, 240}, /* bpp, x, y, w, h */
		.fg0 = {32, 0, 0, 320, 240}, /* bpp, x, y, w, h */
		.fg1 = {32, 0, 0, 400, 240}, /* bpp, x, y, w, h */
	},
#elif defined(CONFIG_JZ4760_VGA_DISPLAY)
	.panel = {
		.cfg = LCD_CFG_LCDPIN_LCD | LCD_CFG_RECOVER | /* Underrun recover */
			   LCD_CFG_NEWDES |						  /* 8words descriptor */
			   LCD_CFG_MODE_GENERIC_TFT |			  /* General TFT panel */
			   LCD_CFG_MODE_TFT_24BIT |				  /* output 18bpp */
			   LCD_CFG_HSP |						  /* Hsync polarity: active low */
			   LCD_CFG_VSP,							  /* Vsync polarity: leading edge is falling edge */
		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16, /* 16words burst, enable out FIFO underrun irq */
												 //		800, 600, 60, 128, 4, 40, 88, 0, 23
		640,
		480,
		54,
		96,
		2,
		16,
		48,
		10,
		33
		//		1280, 720, 50, 152, 15, 22, 200, 14, 1
	},
	.osd = {
		.osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
									//		 LCD_OSDC_ALPHAEN | /* enable alpha */
									//		 LCD_OSDC_F1EN | /* enable Foreground1 */
				   LCD_OSDC_F0EN,   /* enable Foreground0 */
		.osd_ctrl = 0,				/* disable ipu,  */
		.rgb_ctrl = 0,
		.bgcolor = 0x000000,		 /* set background color Black */
		.colorkey0 = 0,				 /* disable colorkey */
		.colorkey1 = 0,				 /* disable colorkey */
		.alpha = 0xA0,				 /* alpha value */
		.ipu_restart = 0x80001000,   /* ipu restart */
		.fg_change = FG_CHANGE_ALL,  /* change all initially */
		.fg0 = {32, 0, 0, 640, 480}, /* bpp, x, y, w, h */
		.fg1 = {32, 0, 0, 640, 480}, /* bpp, x, y, w, h */
	},
#elif defined(CONFIG_JZ4760_LCD_RS97_V10)
    .panel = {
		.cfg = LCD_CFG_LCDPIN_LCD | LCD_CFG_RECOVER | /* Underrun recover */ 
			   LCD_CFG_MODE_SERIAL_TFT | /* General TFT panel */
			   LCD_CFG_MODE_TFT_16BIT |
			   LCD_CFG_PCP |
			   LCD_CFG_NEWDES, /* 8words descriptor */

		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16,	/* 16words burst, enable out FIFO underrun irq */
		320, 480,  60, 20, 1, 48, 40, 18,27,
	  //320, 240, 60, 50, 1, 10, 70, 5, 5,

	},

	.osd = {
		.osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
				   //LCD_OSDC_ALPHAEN | /* enable alpha */
				   LCD_OSDC_F1EN,	/* enable Foreground0 */
				   //LCD_OSDC_F0EN, /* enable Foreground0 */
		.osd_ctrl = LCD_OSDCTRL_IPU | LCD_OSDCTRL_OSDBPP_15_16,			  /* disable ipu,  */
		 .rgb_ctrl = LCD_RGBC_EVEN_GBR << LCD_RGBC_EVENRGB_BIT,
		 .bgcolor = 0x000000, /* set background color Black */
		 .colorkey0 = 0x80000000, /* disable colorkey */
		 .colorkey1 = 0x80000000, /* disable colorkey */
		 .alpha = 0xa0, /* alpha value */
		 .ipu_restart = 0x80001000, /* ipu restart */
		 .fg_change = FG_CHANGE_ALL, /* change all initially */
		 .fg0 = {16, 0, 0, 320,480}, /* bpp, x, y, w, h */
		 .fg1 = {16, 0, 0, 320,480}, /* bpp, x, y, w, h */
	 },
 
#elif defined(CONFIG_JZ4760_LCD_RS97_V21)
	.panel = {
		.cfg = LCD_CFG_LCDPIN_LCD | LCD_CFG_RECOVER | /* Underrun recover */
			   LCD_CFG_MODE_SERIAL_TFT |			  /* General TFT panel */
			   LCD_CFG_MODE_TFT_16BIT |
			   LCD_CFG_PCP,	/* Vsync polarity: leading edge is falling edge */

		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16, /* 16words burst, enable out FIFO underrun irq */
	/*  dw, dh, fclk, hsw, vsw, elw, blw, efw, bfw */
		320, 480, 60, 20, 1, 32, 40, 17, 27, //INNOLUX

},

	.osd = {
		.osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
				   //LCD_OSDC_ALPHAEN | /* enable alpha */
				   LCD_OSDC_F1EN,	/* enable Foreground0 */
				   //LCD_OSDC_F0EN, /* enable Foreground0 */
		.osd_ctrl = LCD_OSDCTRL_IPU | LCD_OSDCTRL_OSDBPP_15_16,			  /* disable ipu,  */
		.rgb_ctrl = LCD_RGBC_EVEN_GBR << LCD_RGBC_EVENRGB_BIT,
		.bgcolor = 0x000000,		 /* set background color Black */
		.colorkey0 = 0x80000000,	 /* disable colorkey */
		.colorkey1 = 0x80000000,	 /* disable colorkey */
		.alpha = 0xa0,				 /* alpha value */
		.ipu_restart = 0x80001000,   /* ipu restart */
		.fg_change = FG_CHANGE_ALL,  /* change all initially */
		.fg0 = {16, 0, 0, 320, 480}, /* bpp, x, y, w, h */
		.fg1 = {16, 0, 0, 320, 480}, /* bpp, x, y, w, h */
	},

#elif defined(CONFIG_JZ4760_LCD_RS97_V30)
	.panel = {
		.cfg = LCD_CFG_LCDPIN_LCD | LCD_CFG_RECOVER | /* Underrun recover */
			   LCD_CFG_MODE_SERIAL_TFT |			  /* General TFT panel */
			   LCD_CFG_MODE_TFT_16BIT |
			   LCD_CFG_PCP |
			   LCD_CFG_NEWDES, /* 8words descriptor */

		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16, /* 16words burst, enable out FIFO underrun irq */
		320, 480, 120, 20, 1, 48, 40, 10, 42, //ILI8965
	},

	.osd = {
		.osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
				   //LCD_OSDC_ALPHAEN | /* enable alpha */
				   LCD_OSDC_F1EN,	/* enable Foreground0 */
				   //LCD_OSDC_F0EN, /* enable Foreground0 */
		.osd_ctrl = LCD_OSDCTRL_IPU | LCD_OSDCTRL_OSDBPP_15_16,			  /* disable ipu,  */
		.rgb_ctrl = LCD_RGBC_ODD_GBR << LCD_RGBC_ODDRGB_BIT,
		.bgcolor = 0x000000,		 /* set background color Black */
		.colorkey0 = 0x80000000,	 /* disable colorkey */
		.colorkey1 = 0x80000000,	 /* disable colorkey */
		.alpha = 0xa0,				 /* alpha value */
		.ipu_restart = 0x80001000,   /* ipu restart */
		.fg_change = FG_CHANGE_ALL,  /* change all initially */
		.fg0 = {16, 0, 0, 320, 480}, /* bpp, x, y, w, h */
		.fg1 = {16, 0, 0, 320, 480}, /* bpp, x, y, w, h */
	},

#else
#error "Select LCD panel first!!!"
#endif
};

#if defined(CONFIG_JZ4760_HDMI_DISPLAY)
#define AIC_FR_TFTH_BIT 16
#define AIC_FR_RFTH_BIT 24

#define PANEL_MODE_HDMI_480P 3
#define PANEL_MODE_HDMI_576P 4
#define PANEL_MODE_HDMI_720P50 5
#define PANEL_MODE_HDMI_720P60 6

struct jz4760lcd_info jz4760_info_hdmi_480p = {
	.panel = {
		.cfg = LCD_CFG_MODE_GENERIC_TFT | LCD_CFG_MODE_TFT_24BIT |
			   LCD_CFG_NEWDES | LCD_CFG_RECOVER |
			   LCD_CFG_PCP | LCD_CFG_HSP | LCD_CFG_VSP,
		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_BST_32,
		//  width,height,freq,hsync,vsync,elw,blw,efw,bfw
		640,
		480,
		60,
		96,
		2,
		48,
		16,
		33,
		10, //HDMI-480P
			//800,600,58,128,4,88,40,23,1,i
			//1024,768,60,136,6,160,24,29,3,
	},
	.osd = {
		.osd_cfg = LCD_OSDC_OSDEN |   // Use OSD mode
				   LCD_OSDC_ALPHAEN | // enable alpha
				   LCD_OSDC_F0EN,	 // enable Foreground0
									  // LCD_OSDC_F1EN,                                         // enable Foreground1

		.osd_ctrl = 0, // disable ipu,
		.rgb_ctrl = 0,
		.bgcolor = 0x000000,		 // set background color Black
		.colorkey0 = 0,				 // disable colorkey
		.colorkey1 = 0,				 // disable colorkey
		.alpha = 0xa0,				 // alpha value
		.ipu_restart = 0x8000085d,   // ipu restart
		.fg_change = FG_CHANGE_ALL,  // change all initially
		.fg0 = {32, 0, 0, 640, 480}, // bpp, x, y, w, h
		.fg1 = {32, 0, 0, 640, 480}, // bpp, x, y, w, h
	},
};
struct jz4760lcd_info jz4760_info_hdmi_576p = {
	.panel = {
		.cfg = LCD_CFG_MODE_GENERIC_TFT | LCD_CFG_MODE_TFT_24BIT |
			   LCD_CFG_NEWDES | LCD_CFG_RECOVER |
			   LCD_CFG_PCP | LCD_CFG_HSP | LCD_CFG_VSP,
		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_BST_32,
		//  width,height,freq,hsync,vsync,elw,blw,efw,bfw
		720,
		576,
		50,
		64,
		5,
		68,
		12,
		40,
		4,
		//800,600,58,128,4,88,40,23,1,i
		//1024,768,60,136,6,160,24,29,3,
	},
	.osd = {
		.osd_cfg = LCD_OSDC_OSDEN |   // Use OSD mode
				   LCD_OSDC_ALPHAEN | // enable alpha
				   LCD_OSDC_F0EN,	 // enable Foreground0
									  // LCD_OSDC_F1EN,                                         // enable Foreground1

		.osd_ctrl = 0, // disable ipu,
		.rgb_ctrl = 0,
		.bgcolor = 0x000000,		 // set background color Black
		.colorkey0 = 0,				 // disable colorkey
		.colorkey1 = 0,				 // disable colorkey
		.alpha = 0xa0,				 // alpha value
		.ipu_restart = 0x8000085d,   // ipu restart
		.fg_change = FG_CHANGE_ALL,  // change all initially
		.fg0 = {32, 0, 0, 720, 576}, // bpp, x, y, w, h
		.fg1 = {32, 0, 0, 720, 576}, // bpp, x, y, w, h
	},
};
struct jz4760lcd_info jz4760_info_hdmi_720p50 = {
	.panel = {
		.cfg = LCD_CFG_MODE_GENERIC_TFT | LCD_CFG_MODE_TFT_24BIT |
			   LCD_CFG_NEWDES | LCD_CFG_RECOVER |
			   LCD_CFG_PCP | LCD_CFG_HSP | LCD_CFG_VSP,
		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_BST_32,
		//  width,height,freq,hsync,vsync,elw,blw,efw,bfw
		1280,
		720,
		50,
		40,
		5,
		440,
		220,
		20,
		5,
		//	1280, 720, 50, 152, 15, 22, 200, 14, 1
		//	1280,720,46,1,1,121,259,6,19,
		//800,600,58,128,4,88,40,23,1,i
		//1024,768,60,136,6,160,24,29,3,
	},
	.osd = {
		.osd_cfg = LCD_OSDC_OSDEN |   // Use OSD mode
				   LCD_OSDC_ALPHAEN | // enable alpha
				   LCD_OSDC_F0EN,	 // enable Foreground0
									  // LCD_OSDC_F1EN,                                         // enable Foreground1

		.osd_ctrl = 0, // disable ipu,
		.rgb_ctrl = 0,
		.bgcolor = 0x000000,		  // set background color Black
		.colorkey0 = 0,				  // disable colorkey
		.colorkey1 = 0,				  // disable colorkey
		.alpha = 0xa0,				  // alpha value
		.ipu_restart = 0x8000085d,	// ipu restart
		.fg_change = FG_CHANGE_ALL,   // change all initially
		.fg0 = {32, 0, 0, 1280, 720}, // bpp, x, y, w, h
		.fg1 = {32, 0, 0, 1280, 720}, // bpp, x, y, w, h
	},
};
struct jz4760lcd_info jz4760_info_hdmi_720p60 = {
	.panel = {
		.cfg = LCD_CFG_MODE_GENERIC_TFT | LCD_CFG_MODE_TFT_24BIT |
			   LCD_CFG_NEWDES | LCD_CFG_RECOVER |
			   LCD_CFG_PCP | LCD_CFG_HSP | LCD_CFG_VSP,
		.slcd_cfg = 0,
		.ctrl = LCD_CTRL_BST_32,
		//  width,height,freq,hsync,vsync,elw,blw,efw,bfw
		1280,
		720,
		60,
		40,
		5,
		110,
		220,
		20,
		5, //74250000
		//800,600,58,128,4,88,40,23,1,i
		//1024,768,60,136,6,160,24,29,3,
	},
	.osd = {
		.osd_cfg = LCD_OSDC_OSDEN |   // Use OSD mode
				   LCD_OSDC_ALPHAEN | // enable alpha
				   LCD_OSDC_F0EN,	 // enable Foreground0
									  // LCD_OSDC_F1EN,                                         // enable Foreground1

		.osd_ctrl = 0, // disable ipu,
		.rgb_ctrl = 0,
		.bgcolor = 0x000000,		  // set background color Black
		.colorkey0 = 0,				  // disable colorkey
		.colorkey1 = 0,				  // disable colorkey
		.alpha = 0xa0,				  // alpha value
		.ipu_restart = 0x8000085d,	// ipu restart
		.fg_change = FG_CHANGE_ALL,   // change all initially
		.fg0 = {32, 0, 0, 1280, 720}, // bpp, x, y, w, h
		.fg1 = {32, 0, 0, 1280, 720}, // bpp, x, y, w, h
	},
};

static void set_i2s_external_codec(void)
{
#if defined(CONFIG_JZ4760_CYGNUS) || defined(CONFIG_JZ4760B_CYGNUS)
	/* gpio defined based on CYGNUS board */
	__gpio_as_func1(3 * 32 + 12); //blck
	__gpio_as_func0(3 * 32 + 13); //sync
	__gpio_as_func0(4 * 32 + 7);  //sd0
	__gpio_as_func0(4 * 32 + 11); //sd1
	__gpio_as_func0(4 * 32 + 12); //sd2
	__gpio_as_func0(4 * 32 + 13); //sd3
#endif

	__i2s_external_codec();

	__aic_select_i2s();
	__i2s_select_i2s();
	__i2s_as_master();

	REG_AIC_I2SCR |= AIC_I2SCR_ESCLK;

	__i2s_disable_record();
	__i2s_disable_replay();
	__i2s_disable_loopback();

	REG_AIC_FR &= ~AIC_FR_TFTH_MASK;
	REG_AIC_FR |= ((8) << AIC_FR_TFTH_BIT);
	REG_AIC_FR &= ~AIC_FR_RFTH_MASK;
	REG_AIC_FR |= ((8) << AIC_FR_RFTH_BIT);

	__i2s_enable();
}
#endif

struct jz4760lcd_info jz4760_info_tve = {
	.panel = {
		.cfg = LCD_CFG_TVEN |					 /* output to tve */
			   LCD_CFG_NEWDES |					 /* 8words descriptor */
			   LCD_CFG_RECOVER |				 /* underrun protect */
			   LCD_CFG_MODE_INTER_CCIR656,		 /* Interlace CCIR656 mode */
		.ctrl = LCD_CTRL_OFUM | LCD_CTRL_BST_16, /* 16words burst */
		TVE_WIDTH_PAL,
		TVE_HEIGHT_PAL,
		TVE_FREQ_PAL,
		0,
		0,
		0,
		0,
		0,
		0,
	},
	.osd = {
		.osd_cfg = LCD_OSDC_OSDEN | /* Use OSD mode */
									//		 LCD_OSDC_ALPHAEN | /* enable alpha */
				   LCD_OSDC_F0EN,   /* enable Foreground0 */
		.osd_ctrl = 0,				/* disable ipu,  */
		.rgb_ctrl = LCD_RGBC_YCC,   /* enable RGB => YUV */
		.bgcolor = 0x00000000,		/* set background color Black */
		.colorkey0 = 0,				/* disable colorkey */
		.colorkey1 = 0,				/* disable colorkey */
		.alpha = 0xA0,				/* alpha value */
		.ipu_restart = 0x80000100,  /* ipu restart */
		.fg_change = FG_CHANGE_ALL, /* change all initially */
		.fg0 = {
			32,
		}, /*  */
		.fg0 = {
			32,
		},
	},
};

struct jz4760lcd_info *jz4760_lcd_info = &jz4760_lcd_panel; /* default output to lcd panel */

#if 0 //JZ_FB_DEBUG
static void print_lcdc_registers(void)	/* debug */
{
	/* LCD Controller Resgisters */
	printk("REG_LCD_CFG:\t0x%08x\n", REG_LCD_CFG);
	printk("REG_LCD_CTRL:\t0x%08x\n", REG_LCD_CTRL);
	printk("REG_LCD_STATE:\t0x%08x\n", REG_LCD_STATE);
	printk("REG_LCD_OSDC:\t0x%08x\n", REG_LCD_OSDC);
	printk("REG_LCD_OSDCTRL:\t0x%08x\n", REG_LCD_OSDCTRL);
	printk("REG_LCD_OSDS:\t0x%08x\n", REG_LCD_OSDS);
	printk("REG_LCD_BGC:\t0x%08x\n", REG_LCD_BGC);
	printk("REG_LCD_KEK0:\t0x%08x\n", REG_LCD_KEY0);
	printk("REG_LCD_KEY1:\t0x%08x\n", REG_LCD_KEY1);
	printk("REG_LCD_ALPHA:\t0x%08x\n", REG_LCD_ALPHA);
	printk("REG_LCD_IPUR:\t0x%08x\n", REG_LCD_IPUR);
	printk("REG_LCD_VAT:\t0x%08x\n", REG_LCD_VAT);
	printk("REG_LCD_DAH:\t0x%08x\n", REG_LCD_DAH);
	printk("REG_LCD_DAV:\t0x%08x\n", REG_LCD_DAV);
	printk("REG_LCD_XYP0:\t0x%08x\n", REG_LCD_XYP0);
	printk("REG_LCD_XYP1:\t0x%08x\n", REG_LCD_XYP1);
	printk("REG_LCD_SIZE0:\t0x%08x\n", REG_LCD_SIZE0);
	printk("REG_LCD_SIZE1:\t0x%08x\n", REG_LCD_SIZE1);
	printk("REG_LCD_RGBC\t0x%08x\n", REG_LCD_RGBC);
	printk("REG_LCD_VSYNC:\t0x%08x\n", REG_LCD_VSYNC);
	printk("REG_LCD_HSYNC:\t0x%08x\n", REG_LCD_HSYNC);
	printk("REG_LCD_PS:\t0x%08x\n", REG_LCD_PS);
	printk("REG_LCD_CLS:\t0x%08x\n", REG_LCD_CLS);
	printk("REG_LCD_SPL:\t0x%08x\n", REG_LCD_SPL);
	printk("REG_LCD_REV:\t0x%08x\n", REG_LCD_REV);
	printk("REG_LCD_IID:\t0x%08x\n", REG_LCD_IID);
	printk("REG_LCD_DA0:\t0x%08x\n", REG_LCD_DA0);
	printk("REG_LCD_SA0:\t0x%08x\n", REG_LCD_SA0);
	printk("REG_LCD_FID0:\t0x%08x\n", REG_LCD_FID0);
	printk("REG_LCD_CMD0:\t0x%08x\n", REG_LCD_CMD0);
	printk("REG_LCD_OFFS0:\t0x%08x\n", REG_LCD_OFFS0);
	printk("REG_LCD_PW0:\t0x%08x\n", REG_LCD_PW0);
	printk("REG_LCD_CNUM0:\t0x%08x\n", REG_LCD_CNUM0);
	printk("REG_LCD_DESSIZE0:\t0x%08x\n", REG_LCD_DESSIZE0);
	printk("REG_LCD_DA1:\t0x%08x\n", REG_LCD_DA1);
	printk("REG_LCD_SA1:\t0x%08x\n", REG_LCD_SA1);
	printk("REG_LCD_FID1:\t0x%08x\n", REG_LCD_FID1);
	printk("REG_LCD_CMD1:\t0x%08x\n", REG_LCD_CMD1);
	printk("REG_LCD_OFFS1:\t0x%08x\n", REG_LCD_OFFS1);
	printk("REG_LCD_PW1:\t0x%08x\n", REG_LCD_PW1);
	printk("REG_LCD_CNUM1:\t0x%08x\n", REG_LCD_CNUM1);
	printk("REG_LCD_DESSIZE1:\t0x%08x\n", REG_LCD_DESSIZE1);
	printk("==================================\n");
	printk("REG_LCD_VSYNC:\t%d:%d\n", REG_LCD_VSYNC>>16, REG_LCD_VSYNC&0xfff);
	printk("REG_LCD_HSYNC:\t%d:%d\n", REG_LCD_HSYNC>>16, REG_LCD_HSYNC&0xfff);
	printk("REG_LCD_VAT:\t%d:%d\n", REG_LCD_VAT>>16, REG_LCD_VAT&0xfff);
	printk("REG_LCD_DAH:\t%d:%d\n", REG_LCD_DAH>>16, REG_LCD_DAH&0xfff);
	printk("REG_LCD_DAV:\t%d:%d\n", REG_LCD_DAV>>16, REG_LCD_DAV&0xfff);
	printk("==================================\n");

	/* Smart LCD Controller Resgisters */
	printk("REG_SLCD_CFG:\t0x%08x\n", REG_SLCD_CFG);
	printk("REG_SLCD_CTRL:\t0x%08x\n", REG_SLCD_CTRL);
	printk("REG_SLCD_STATE:\t0x%08x\n", REG_SLCD_STATE);
	printk("==================================\n");

	/* TVE Controller Resgisters */
	printk("REG_TVE_CTRL:\t0x%08x\n", REG_TVE_CTRL);
	printk("REG_TVE_FRCFG:\t0x%08x\n", REG_TVE_FRCFG);
	printk("REG_TVE_SLCFG1:\t0x%08x\n", REG_TVE_SLCFG1);
	printk("REG_TVE_SLCFG2:\t0x%08x\n", REG_TVE_SLCFG2);
	printk("REG_TVE_SLCFG3:\t0x%08x\n", REG_TVE_SLCFG3);
	printk("REG_TVE_LTCFG1:\t0x%08x\n", REG_TVE_LTCFG1);
	printk("REG_TVE_LTCFG2:\t0x%08x\n", REG_TVE_LTCFG2);
	printk("REG_TVE_CFREQ:\t0x%08x\n", REG_TVE_CFREQ);
	printk("REG_TVE_CPHASE:\t0x%08x\n", REG_TVE_CPHASE);
	printk("REG_TVE_CBCRCFG:\t0x%08x\n", REG_TVE_CBCRCFG);
	printk("REG_TVE_WSSCR:\t0x%08x\n", REG_TVE_WSSCR);
	printk("REG_TVE_WSSCFG1:\t0x%08x\n", REG_TVE_WSSCFG1);
	printk("REG_TVE_WSSCFG2:\t0x%08x\n", REG_TVE_WSSCFG2);
	printk("REG_TVE_WSSCFG3:\t0x%08x\n", REG_TVE_WSSCFG3);

	printk("==================================\n");
}
#else
#define print_lcdc_registers()
#endif

struct lcd_cfb_info
{
	struct fb_info fb;
	struct
	{
		u16 red, green, blue;
	} palette[NR_PALETTE];

	int b_lcd_display;
	int b_lcd_pwm;
	int backlight_level;
};

static struct lcd_cfb_info *jz4760fb_info;


static unsigned char *lcd_palette;
unsigned char *lcd_frame0;

//TONY IPU
bool clear_fb;
#ifdef VSYNC_OPTION
uint32_t vsync_on;
#endif
uint32_t vsync_count;
spinlock_t lock;
wait_queue_head_t wait_vsync;
unsigned int delay_flush;

#define MAX_XRES 640
#define MAX_YRES 480

unsigned int backlight_value = 80;

//unsigned char *ipu_buffer;
static struct task_struct *resize_task;

#define LCD_SCREEN_W 320
#define LCD_SCREEN_H 480
int get_lcd_width(void)
{
	return LCD_SCREEN_W;
}
int get_lcd_hight(void)
{
	return LCD_SCREEN_H;
}
EXPORT_SYMBOL(get_lcd_width);
EXPORT_SYMBOL(get_lcd_hight);

typedef enum
{
	RAW = 0,
	IPU,
	SW_HALF_DOUBLE,
	SW_FULL_DOUBLE
} lcd_output_mode;

unsigned int frame_yoffset = 0;

extern void ipu_driver_close_tv(void);
extern void ipu_driver_open_tv(int, int, int, int);
extern void ipu_driver_flush_tv(void);
extern void ipu_update_address();
extern void ipu_driver_wait_end();

//END OF TONY IPU

static unsigned char *lcd_cmdbuf;

static void jz4760fb_set_mode(struct jz4760lcd_info *lcd_info);
static void jz4760fb_deep_set_mode(struct jz4760lcd_info *lcd_info);

static int jz4760fb_set_backlight_level(int n);

static int screen_on(void);
static int screen_off(void);

static void ctrl_enable(void)
{
	REG_LCD_STATE = 0; /* clear lcdc status */
	__lcd_slcd_special_on();
	__lcd_clr_dis();
	__lcd_set_ena(); /* enable lcdc */

	return;
}

static void ctrl_disable(void)
{
	if (jz4760_lcd_info->panel.cfg & LCD_CFG_LCDPIN_SLCD ||
		jz4760_lcd_info->panel.cfg & LCD_CFG_TVEN) /*  */
		__lcd_clr_ena();						   /* Smart lcd and TVE mode only support quick disable */
	else
	{
		int cnt;
		/* when CPU main freq is 336MHz,wait for 30ms */
		cnt = 528000 * 30;
		__lcd_set_dis(); /* regular disable */
		//__lcd_clr_ena();
		while (!__lcd_disable_done() && cnt)
		{
			cnt--;
		}
		if (cnt == 0)
			printk("LCD disable timeout! REG_LCD_STATE=0x%08xx\n", REG_LCD_STATE);
		REG_LCD_STATE &= ~LCD_STATE_LDD;
	}

	return;
}

static inline u_int chan_to_field(u_int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

static int jz4760fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
							  u_int transp, struct fb_info *info)
{
	struct lcd_cfb_info *cfb = (struct lcd_cfb_info *)info;
	unsigned short *ptr, ctmp;

	//	D("regno:%d,RGBt:(%d,%d,%d,%d)\t", regno, red, green, blue, transp);
	if (regno >= NR_PALETTE)
		return 1;

	cfb->palette[regno].red = red;
	cfb->palette[regno].green = green;
	cfb->palette[regno].blue = blue;
	if (cfb->fb.var.bits_per_pixel <= 16)
	{
		red >>= 8;
		green >>= 8;
		blue >>= 8;

		red &= 0xff;
		green &= 0xff;
		blue &= 0xff;
	}
	switch (cfb->fb.var.bits_per_pixel)
	{
	case 1:
	case 2:
	case 4:
	case 8:
		if (((jz4760_lcd_info->panel.cfg & LCD_CFG_MODE_MASK) == LCD_CFG_MODE_SINGLE_MSTN) ||
			((jz4760_lcd_info->panel.cfg & LCD_CFG_MODE_MASK) == LCD_CFG_MODE_DUAL_MSTN))
		{
			ctmp = (77L * red + 150L * green + 29L * blue) >> 8;
			ctmp = ((ctmp >> 3) << 11) | ((ctmp >> 2) << 5) |
				   (ctmp >> 3);
		}
		else
		{
			/* RGB 565 */
			if (((red >> 3) == 0) && ((red >> 2) != 0))
				red = 1 << 3;
			if (((blue >> 3) == 0) && ((blue >> 2) != 0))
				blue = 1 << 3;
			ctmp = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
		}

		ptr = (unsigned short *)lcd_palette;
		ptr = (unsigned short *)(((u32)ptr) | 0xa0000000);
		ptr[regno] = ctmp;

		break;

	case 15:
		if (regno < 16)
			((u32 *)cfb->fb.pseudo_palette)[regno] =
				((red >> 3) << 10) |
				((green >> 3) << 5) |
				(blue >> 3);
		break;
	case 16:
		if (regno < 16)
		{
			((u32 *)cfb->fb.pseudo_palette)[regno] =
				((red >> 3) << 11) |
				((green >> 2) << 5) |
				(blue >> 3);
		}
		break;
	case 17 ... 32:
		if (regno < 16)
			((u32 *)cfb->fb.pseudo_palette)[regno] =
				(red << 16) |
				(green << 8) |
				(blue << 0);

		/*		if (regno < 16) {
			unsigned val;
                        val  = chan_to_field(red, &cfb->fb.var.red);
                        val |= chan_to_field(green, &cfb->fb.var.green);
                        val |= chan_to_field(blue, &cfb->fb.var.blue);
			((u32 *)cfb->fb.pseudo_palette)[regno] = val;
		}
*/

		break;
	}
	return 0;
}

/*
 * switch to tve mode from lcd mode
 * mode:
 * 	PANEL_MODE_TVE_PAL: switch to TVE_PAL mode
 * 	PANEL_MODE_TVE_NTSC: switch to TVE_NTSC mode
 */
static void jz4760lcd_info_switch_to_TVE(int mode)
{
	struct jz4760lcd_info *info;
	struct jz4760lcd_osd_t *osd_lcd;
	int x, y, w, h;

	info = jz4760_lcd_info = &jz4760_info_tve;
	osd_lcd = &jz4760_lcd_panel.osd;

	switch (mode)
	{
	case PANEL_MODE_TVE_PAL:
		info->panel.cfg |= LCD_CFG_TVEPEH; /* TVE PAL enable extra halfline signal */
		info->panel.w = TVE_WIDTH_PAL;
		info->panel.h = TVE_HEIGHT_PAL;
		info->panel.fclk = TVE_FREQ_PAL;
		w = (osd_lcd->fg0.w < TVE_WIDTH_PAL) ? osd_lcd->fg0.w : TVE_WIDTH_PAL;
		h = (osd_lcd->fg0.h < TVE_HEIGHT_PAL) ? osd_lcd->fg0.h : TVE_HEIGHT_PAL;
		x = ((TVE_WIDTH_PAL - w) >> 2) << 1;
		y = ((TVE_HEIGHT_PAL - h) >> 2) << 1;
		//		x = 0;
		//		y = 0;

		info->osd.fg0.bpp = osd_lcd->fg0.bpp;
		info->osd.fg0.x = x;
		info->osd.fg0.y = y;
		info->osd.fg0.w = w;
		info->osd.fg0.h = h;
		w = (osd_lcd->fg1.w < TVE_WIDTH_PAL) ? osd_lcd->fg1.w : TVE_WIDTH_PAL;
		h = (osd_lcd->fg1.h < TVE_HEIGHT_PAL) ? osd_lcd->fg1.h : TVE_HEIGHT_PAL;
		x = ((TVE_WIDTH_PAL - w) >> 2) << 1;
		y = ((TVE_HEIGHT_PAL - h) >> 2) << 1;
		//		x = 0;
		//		y = 0;

		info->osd.fg1.bpp = 32; /* use RGB888 in TVE mode*/
		info->osd.fg1.x = x;
		info->osd.fg1.y = y;
		info->osd.fg1.w = w;
		info->osd.fg1.h = h;
		break;
	case PANEL_MODE_TVE_NTSC:
		info->panel.cfg &= ~LCD_CFG_TVEPEH; /* TVE NTSC disable extra halfline signal */
		info->panel.w = TVE_WIDTH_NTSC;
		info->panel.h = TVE_HEIGHT_NTSC;
		info->panel.fclk = TVE_FREQ_NTSC;
		w = (osd_lcd->fg0.w < TVE_WIDTH_NTSC) ? osd_lcd->fg0.w : TVE_WIDTH_NTSC;
		h = (osd_lcd->fg0.h < TVE_HEIGHT_NTSC) ? osd_lcd->fg0.h : TVE_HEIGHT_NTSC;
		x = ((TVE_WIDTH_NTSC - w) >> 2) << 1;
		y = ((TVE_HEIGHT_NTSC - h) >> 2) << 1;
		//		x = 0;
		//		y = 0;
		info->osd.fg0.bpp = osd_lcd->fg0.bpp;
		info->osd.fg0.x = x;
		info->osd.fg0.y = y;
		info->osd.fg0.w = w;
		info->osd.fg0.h = h;
		w = (osd_lcd->fg1.w < TVE_WIDTH_NTSC) ? osd_lcd->fg1.w : TVE_WIDTH_NTSC;
		h = (osd_lcd->fg1.h < TVE_HEIGHT_NTSC) ? osd_lcd->fg1.h : TVE_HEIGHT_NTSC;
		x = ((TVE_WIDTH_NTSC - w) >> 2) << 1;
		y = ((TVE_HEIGHT_NTSC - h) >> 2) << 1;
		info->osd.fg1.bpp = 32; /* use RGB888 int TVE mode */
		info->osd.fg1.x = x;
		info->osd.fg1.y = y;
		info->osd.fg1.w = w;
		info->osd.fg1.h = h;
		break;
	default:
		printk("%s, %s: Unknown tve mode\n", __FILE__, __FUNCTION__);
	}
}

static int jz4760fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	void __user *argp = (void __user *)arg;

	switch (cmd)
	{
	case FBIOSETBACKLIGHT:
		jz4760fb_set_backlight_level(arg);

		break;

	case FBIODISPON:
		ctrl_enable();
		screen_on();

		break;

	case FBIODISPOFF:
		screen_off();
		ctrl_disable();

		break;

	case FBIOPRINT_REG:
		print_lcdc_registers();

		break;

	case FBIO_GET_MODE:
		D("fbio get mode\n");

		if (copy_to_user(argp, jz4760_lcd_info, sizeof(struct jz4760lcd_info)))
			return -EFAULT;

		break;

	case FBIO_SET_MODE:
		D("fbio set mode\n");

		if (copy_from_user(jz4760_lcd_info, argp, sizeof(struct jz4760lcd_info)))
			return -EFAULT;

		/* set mode */
		jz4760fb_set_mode(jz4760_lcd_info);

		break;

	case FBIO_DEEP_SET_MODE:
		D("fbio deep set mode\n");

		if (copy_from_user(jz4760_lcd_info, argp, sizeof(struct jz4760lcd_info)))
			return -EFAULT;

		jz4760fb_deep_set_mode(jz4760_lcd_info);

		break;

	case FBIO_MODE_SWITCH:
		D("FBIO_MODE_SWITCH");
		switch (arg)
		{
#ifdef CONFIG_FB_JZ4760_TVE
		case PANEL_MODE_TVE_PAL:  /* switch to TVE_PAL mode */
		case PANEL_MODE_TVE_NTSC: /* switch to TVE_NTSC mode */
			jz4760lcd_info_switch_to_TVE(arg);
			jz4760tve_init(arg); /* tve controller init */
			udelay(100);
			cpm_start_clock(CGM_TVE);
			jz4760tve_enable_tve();
			/* turn off lcd backlight */
			screen_off();
			break;
#endif
#if defined(CONFIG_JZ4760_HDMI_DISPLAY)
		case PANEL_MODE_HDMI_480P:
			set_i2s_external_codec();
			/* turn off TVE, turn off DACn... */
			//jz4760tve_disable_tve();
			jz4760_lcd_info = &jz4760_info_hdmi_480p;
			/* turn on lcd backlight */
			screen_off();
			break;

		case PANEL_MODE_HDMI_576P:
			set_i2s_external_codec();
			/* turn off TVE, turn off DACn... */
			//jz4760tve_disable_tve();
			jz4760_lcd_info = &jz4760_info_hdmi_576p;
			/* turn on lcd backlight */
			screen_off();
			break;

		case PANEL_MODE_HDMI_720P50:
#if defined(CONFIG_SOC_JZ4760B)
			REG_LCD_PCFG = 0xc0000888;
			REG_GPIO_PXSLC(2) = 0;
			REG_GPIO_PXDS1S(2) |= 1 << 8;
			REG_GPIO_PXDS0S(2) = 0x0fffffff;

#endif
			set_i2s_external_codec();
			/* turn off TVE, turn off DACn... */
			//jz4760tve_disable_tve();
			jz4760_lcd_info = &jz4760_info_hdmi_720p50;
			/* turn on lcd backlight */
			screen_off();
			break;

		case PANEL_MODE_HDMI_720P60:
#if defined(CONFIG_SOC_JZ4760B)
			REG_LCD_PCFG = 0xc0000888;
			REG_GPIO_PXSLC(2) = 0;
			REG_GPIO_PXDS1S(2) |= 1 << 8;
			REG_GPIO_PXDS0S(2) = 0x0fffffff;
#endif
			set_i2s_external_codec();
			/* turn off TVE, turn off DACn... */
			//jz4760tve_disable_tve();
			jz4760_lcd_info = &jz4760_info_hdmi_720p60;
			/* turn on lcd backlight */
			screen_off();
			break;
#endif							   //CONFIG_JZ4760_HDMI_DISPLAY
		case PANEL_MODE_LCD_PANEL: /* switch to LCD mode */
		default:
			/* turn off TVE, turn off DACn... */
#ifdef CONFIG_FB_JZ4760_TVE
			jz4760tve_disable_tve();
			cpm_stop_clock(CGM_TVE);
#endif
			jz4760_lcd_info = &jz4760_lcd_panel;
			/* turn on lcd backlight */
			screen_on();
			break;
		}
		jz4760fb_deep_set_mode(jz4760_lcd_info);
		break;

#ifdef CONFIG_FB_JZ4760_TVE
	case FBIO_GET_TVE_MODE:
		D("fbio get TVE mode\n");
		if (copy_to_user(argp, jz4760_tve_info, sizeof(struct jz4760tve_info)))
			return -EFAULT;
		break;
	case FBIO_SET_TVE_MODE:
		D("fbio set TVE mode\n");
		if (copy_from_user(jz4760_tve_info, argp, sizeof(struct jz4760tve_info)))
			return -EFAULT;
		/* set tve mode */
		jz4760tve_set_tve_mode(jz4760_tve_info);
		break;
#endif
	default:
		printk("%s, unknown command(0x%x)", __FILE__, cmd);
		break;
	}

	return ret;
}

/* Use mmap /dev/fb can only get a non-cacheable Virtual Address. */
static int jz4760fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct lcd_cfb_info *cfb = (struct lcd_cfb_info *)info;
	unsigned long start;
	unsigned long off;
	u32 len;
	D("%s, %s, %d\n", __FILE__, __FUNCTION__, __LINE__);
	off = vma->vm_pgoff << PAGE_SHIFT;
	//fb->fb_get_fix(&fix, PROC_CONSOLE(info), info);

	/* frame buffer memory */
	start = cfb->fb.fix.smem_start;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + cfb->fb.fix.smem_len);
	start &= PAGE_MASK;

	if ((vma->vm_end - vma->vm_start + off) > len)
		return -EINVAL;
	off += start;

	vma->vm_pgoff = off >> PAGE_SHIFT;
	vma->vm_flags |= VM_IO;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); /* Uncacheable */

#if 1
	pgprot_val(vma->vm_page_prot) &= ~_CACHE_MASK;
	pgprot_val(vma->vm_page_prot) |= _CACHE_UNCACHED; /* Uncacheable */
//	pgprot_val(vma->vm_page_prot) |= _CACHE_CACHABLE_NONCOHERENT;	/* Write-Back */
#endif

	if (io_remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
						   vma->vm_end - vma->vm_start,
						   vma->vm_page_prot))
	{
		return -EAGAIN;
	}
	return 0;
}

/* checks var and eventually tweaks it to something supported,
 * DO NOT MODIFY PAR */
static int jz4760fb_check_var(struct fb_var_screeninfo *var, struct fb_info *fb)
{
	clear_fb = var->bits_per_pixel != fb->var.bits_per_pixel ||
		var->xres != fb->var.xres || var->yres != fb->var.yres;
	return 0;
}

static int jzfb_wait_for_vsync()
{
	uint32_t count = vsync_count;
	long t = wait_event_interruptible_timeout(wait_vsync,
						  count != vsync_count,
						  HZ / 10);
	return t > 0 ? 0 : (t < 0 ? (int)t : -ETIMEDOUT);
}


/*
 * set the video mode according to info->var
 */
static int jz4760fb_set_par(struct fb_info *info)
{	
	struct fb_var_screeninfo *var = &info->var;
	struct fb_fix_screeninfo *fix = &info->fix;
	
	uint32_t xoffset = (320 - (var->xres))/2;
	uint32_t yoffset = var->yres > 240 ? 480 - (var->yres) : 240 - (var->yres);
	uint32_t result = yoffset << 16 | xoffset;

	spin_lock_irq(&lock);
	//struct lcd_cfb_info *jzfb = info->par;
	fix->line_length = var->xres_virtual * (var->bits_per_pixel >> 3);
		
	ipu_driver_close_tv();
	// printk("TONY RESULT = %d %d %d\n",xoffset,yoffset,result);
	if (var->yres > 240)
	{
		REG_LCD_XYP1 = 0;	
		REG_LCD_OSDCTRL |= LCD_OSDCTRL_CHANGES;
		while (REG_LCD_OSDS & LCD_OSDS_READY != 1);
		
		REG_LCD_SIZE1 = var->yres << 16 | var->xres;
		REG_LCD_OSDCTRL |= LCD_OSDCTRL_CHANGES;
		while (REG_LCD_OSDS & LCD_OSDS_READY != 1);
		
		ipu_driver_open_tv(var->xres,var->yres,var->xres,var->yres);		
	}
	else
	{
		REG_LCD_XYP1 = result;	
		REG_LCD_OSDCTRL |= LCD_OSDCTRL_CHANGES;
		while (REG_LCD_OSDS & LCD_OSDS_READY != 1);

		REG_LCD_SIZE1 = var->yres*2 << 16 | var->xres;
		REG_LCD_OSDCTRL |= LCD_OSDCTRL_CHANGES;
		while (REG_LCD_OSDS & LCD_OSDS_READY != 1);
		
		ipu_driver_open_tv(var->xres,var->yres,var->xres,var->yres * 2);
	}
	
	if (clear_fb) 
	{
		void *page_virt = lcd_frame0;
		unsigned int size = fix->line_length * var->yres * 3;

		spin_unlock_irq(&lock);

		for (; page_virt < lcd_frame0 + size; page_virt += PAGE_SIZE)
			clear_page(page_virt);
		dma_cache_wback_inv((unsigned long) lcd_frame0, size);

		spin_lock_irq(&lock);
	}
	
	return 0;
}

/*
 * (Un)Blank the display.
 * Fix me: should we use VESA value?
 */
static int jz4760fb_blank(int blank_mode, struct fb_info *info)
{
	D("jz4760 fb_blank %d %p", blank_mode, info);
	switch (blank_mode)
	{
	case FB_BLANK_UNBLANK:
		//case FB_BLANK_NORMAL:
		/* Turn on panel */
		__lcd_set_ena();
		screen_on();

		break;

	case FB_BLANK_NORMAL:
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_POWERDOWN:
#if 0
		/* Turn off panel */
		__lcd_display_off();
		__lcd_set_dis();
#endif
		break;
	default:
		break;
	}
	return 0;
}

/*
 * pan display
 */
static int jz4760fb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct lcd_cfb_info *cfb = (struct lcd_cfb_info *)info;
	if (!var || !cfb)
	{
		return -EINVAL;
	}
#ifdef VSYNC_OPTION
	if (vsync_on)
#endif
	{
		spin_lock_irq(&lock);
		frame_yoffset = var->yoffset * cfb->fb.fix.line_length;
		delay_flush = 8;
		dma_cache_wback_inv((unsigned long)(lcd_frame0 + frame_yoffset),
					cfb->fb.fix.line_length * cfb->fb.var.yres);
		spin_unlock_irq(&lock);
		jzfb_wait_for_vsync();
	}
#ifdef VSYNC_OPTION
	else
	{
		frame_yoffset = var->yoffset * cfb->fb.fix.line_length;
		delay_flush = 8;
		dma_cache_wback_inv((unsigned long)(lcd_frame0 + frame_yoffset),
					cfb->fb.fix.line_length * cfb->fb.var.yres);
		ipu_update_address();
	}
#endif
	return 0;
}

/* use default function cfb_fillrect, cfb_copyarea, cfb_imageblit */
static struct fb_ops jz4760fb_ops = {
	.owner = THIS_MODULE,
	.fb_setcolreg = jz4760fb_setcolreg,
	.fb_check_var = jz4760fb_check_var,
	.fb_set_par = jz4760fb_set_par,
	.fb_blank = jz4760fb_blank,
	.fb_pan_display = jz4760fb_pan_display,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_mmap = jz4760fb_mmap,
	.fb_ioctl = jz4760fb_ioctl,
};

static int jz4760fb_set_var(struct fb_var_screeninfo *var, int con,
							struct fb_info *info)
{
	struct lcd_cfb_info *cfb = (struct lcd_cfb_info *)info;
	struct jz4760lcd_info *lcd_info = jz4760_lcd_info;
	int chgvar = 0;

	var->height = lcd_info->osd.fg0.h; /* tve mode */
	var->width = lcd_info->osd.fg0.w;
	var->bits_per_pixel = lcd_info->osd.fg0.bpp;

	var->vmode = FB_VMODE_NONINTERLACED;
	var->activate = cfb->fb.var.activate;
	var->xres = var->width;
	var->yres = var->height;
	var->xres_virtual = var->width;
	var->yres_virtual = var->height;
	var->xoffset = 0;
	var->yoffset = 0;
	var->pixclock = 0;
	var->left_margin = 0;
	var->right_margin = 0;
	var->upper_margin = 0;
	var->lower_margin = 0;
	var->hsync_len = 0;
	var->vsync_len = 0;
	var->sync = 0;
	var->activate &= ~FB_ACTIVATE_TEST;

	/*
	 * CONUPDATE and SMOOTH_XPAN are equal.  However,
	 * SMOOTH_XPAN is only used internally by fbcon.
	 */
	if (var->vmode & FB_VMODE_CONUPDATE)
	{
		var->vmode |= FB_VMODE_YWRAP;
		var->xoffset = cfb->fb.var.xoffset;
		var->yoffset = cfb->fb.var.yoffset;
	}

	if (var->activate & FB_ACTIVATE_TEST)
		return 0;

	if ((var->activate & FB_ACTIVATE_MASK) != FB_ACTIVATE_NOW)
		return -EINVAL;

	if (cfb->fb.var.xres != var->xres)
		chgvar = 1;
	if (cfb->fb.var.yres != var->yres)
		chgvar = 1;
	if (cfb->fb.var.xres_virtual != var->xres_virtual)
		chgvar = 1;
	if (cfb->fb.var.yres_virtual != var->yres_virtual)
		chgvar = 1;
	if (cfb->fb.var.bits_per_pixel != var->bits_per_pixel)
		chgvar = 1;

	//display = fb_display + con;

	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;

	switch (var->bits_per_pixel)
	{
	case 1: /* Mono */
		cfb->fb.fix.visual = FB_VISUAL_MONO01;
		cfb->fb.fix.line_length = (var->xres * var->bits_per_pixel) / 8;
		break;
	case 2: /* Mono */
		var->red.offset = 0;
		var->red.length = 2;
		var->green.offset = 0;
		var->green.length = 2;
		var->blue.offset = 0;
		var->blue.length = 2;

		cfb->fb.fix.visual = FB_VISUAL_PSEUDOCOLOR;
		cfb->fb.fix.line_length = (var->xres * var->bits_per_pixel) / 8;
		break;
	case 4: /* PSEUDOCOLOUR*/
		var->red.offset = 0;
		var->red.length = 4;
		var->green.offset = 0;
		var->green.length = 4;
		var->blue.offset = 0;
		var->blue.length = 4;

		cfb->fb.fix.visual = FB_VISUAL_PSEUDOCOLOR;
		cfb->fb.fix.line_length = var->xres / 2;
		break;
	case 8: /* PSEUDOCOLOUR, 256 */
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 0;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;

		cfb->fb.fix.visual = FB_VISUAL_PSEUDOCOLOR;
		cfb->fb.fix.line_length = var->xres;
		break;
	case 15: /* DIRECTCOLOUR, 32k */
		var->bits_per_pixel = 15;
		var->red.offset = 10;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 5;
		var->blue.offset = 0;
		var->blue.length = 5;

		cfb->fb.fix.visual = FB_VISUAL_DIRECTCOLOR;
		cfb->fb.fix.line_length = var->xres_virtual * 2;
		break;
	case 16: /* DIRECTCOLOUR, 64k */
		var->bits_per_pixel = 16;
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;

		cfb->fb.fix.visual = FB_VISUAL_TRUECOLOR;
		cfb->fb.fix.line_length = var->xres_virtual * 2;
		break;
	case 17 ... 32:
		/* DIRECTCOLOUR, 256 */
		var->bits_per_pixel = 32;

		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 24;
		var->transp.length = 8;

		cfb->fb.fix.visual = FB_VISUAL_TRUECOLOR;
		cfb->fb.fix.line_length = var->xres_virtual * 4;
		break;

	default: /* in theory this should never happen */
		printk(KERN_WARNING "%s: don't support for %dbpp\n",
			   cfb->fb.fix.id, var->bits_per_pixel);
		break;
	}

	cfb->fb.var = *var;
	cfb->fb.var.activate &= ~FB_ACTIVATE_ALL;

	/*
	 * Update the old var.  The fbcon drivers still use this.
	 * Once they are using cfb->fb.var, this can be dropped.
	 *					--rmk
	 */
	//display->var = cfb->fb.var;
	/*
	 * If we are setting all the virtual consoles, also set the
	 * defaults used to create new consoles.
	 */
	fb_set_cmap(&cfb->fb.cmap, &cfb->fb);

	return 0;
}

static struct lcd_cfb_info *jz4760fb_alloc_fb_info(void)
{
	struct lcd_cfb_info *cfb;

	cfb = kmalloc(sizeof(struct lcd_cfb_info) + sizeof(u32) * 16, GFP_KERNEL);

	if (!cfb)
		return NULL;

	jz4760fb_info = cfb;

	memset(cfb, 0, sizeof(struct lcd_cfb_info));

	cfb->backlight_level = LCD_DEFAULT_BACKLIGHT;

	strcpy(cfb->fb.fix.id, "jz-lcd");
	cfb->fb.fix.type = FB_TYPE_PACKED_PIXELS;
	cfb->fb.fix.type_aux = 0;
	cfb->fb.fix.xpanstep = 1;
	cfb->fb.fix.ypanstep = 1;
	cfb->fb.fix.ywrapstep = 0;
	cfb->fb.fix.accel = FB_ACCEL_NONE;

	cfb->fb.var.nonstd = 0;
	cfb->fb.var.activate = FB_ACTIVATE_NOW;
	cfb->fb.var.height = -1;
	cfb->fb.var.width = -1;
	cfb->fb.var.accel_flags = FB_ACCELF_TEXT;

	cfb->fb.fbops = &jz4760fb_ops;
	cfb->fb.flags = FBINFO_FLAG_DEFAULT;

	cfb->fb.pseudo_palette = (void *)(cfb + 1);

	switch (jz4760_lcd_info->osd.fg0.bpp)
	{
	case 1:
		fb_alloc_cmap(&cfb->fb.cmap, 4, 0);
		break;
	case 2:
		fb_alloc_cmap(&cfb->fb.cmap, 8, 0);
		break;
	case 4:
		fb_alloc_cmap(&cfb->fb.cmap, 32, 0);
		break;
	case 8:

	default:
		fb_alloc_cmap(&cfb->fb.cmap, 256, 0);
		break;
	}
	D("fb_alloc_cmap,fb.cmap.len:%d....\n", cfb->fb.cmap.len);

	return cfb;
}

static int bpp_to_data_bpp(int bpp)
{
	switch (bpp)
	{
	case 32:
	case 16:
		break;

	case 15:
		bpp = 16;
		break;

	default:
		bpp = -EINVAL;
	}

	return bpp;
}

/*
 * Map screen memory
 */
static int jz4760fb_map_smem(struct lcd_cfb_info *cfb)
{
	/* Compute space for max res at 32bpp, double buffered + ipu buffer. */
	unsigned int size = PAGE_ALIGN(MAX_XRES * MAX_YRES * 4 * 3);
	void *page_virt;

	lcd_palette = (unsigned char *)__get_free_pages(GFP_KERNEL, 0);
	lcd_frame0 = alloc_pages_exact(size, GFP_KERNEL);
	//ipu_buffer = lcd_frame0 + (MAX_XRES * MAX_YRES * 4 * 2);
	//TONY IPU

	if ((!lcd_palette) || (!lcd_frame0))
		return -ENOMEM;


#if defined(CONFIG_FB_JZ4760_SLCD)
	lcd_cmdbuf = (unsigned char *)__get_free_pages(GFP_KERNEL, 0);
	memset((void *)lcd_cmdbuf, 0, PAGE_SIZE);

	{
		int data, i, *ptr;
		ptr = (unsigned int *)lcd_cmdbuf;
		data = WR_GRAM_CMD;
		data = ((data & 0xff) << 1) | ((data & 0xff00) << 2);
		for (i = 0; i < 3; i++)
		{
			ptr[i] = data;
		}
	}
#endif

	/*
	 * Set page reserved so that mmap will work. This is necessary
	 * since we'll be remapping normal memory.
	 */
	page_virt = (unsigned long)lcd_palette;
	SetPageReserved(virt_to_page((void *)page_virt));

	for (page_virt = lcd_frame0;
	     page_virt < lcd_frame0 + size; page_virt += PAGE_SIZE) {
		SetPageReserved(virt_to_page(page_virt));
		clear_page(page_virt);
	}

	cfb->fb.fix.smem_start = virt_to_phys((void *)lcd_frame0);
	cfb->fb.fix.smem_len = (size); /* page_shift/2 ??? */
	cfb->fb.screen_base =
		(unsigned char *)(((unsigned int)lcd_frame0 & 0x1fffffff) | 0xa0000000);

	if (!cfb->fb.screen_base)
	{
		printk("jz4760fb, %s: unable to map screen memory\n", cfb->fb.fix.id);
		return -ENOMEM;
	}

	return 0;
}

static void jz4760fb_free_fb_info(struct lcd_cfb_info *cfb)
{
	if (cfb)
	{
		fb_alloc_cmap(&cfb->fb.cmap, 0, 0);
		kfree(cfb);
	}
}

static void jz4760fb_unmap_smem(struct lcd_cfb_info *cfb)
{
	struct page *map = NULL;
	unsigned char *tmp;
	unsigned int page_shift, needroom, bpp, w, h;

	bpp = jz4760_lcd_info->osd.fg0.bpp;
	if (bpp == 18 || bpp == 24)
		bpp = 32;
	if (bpp == 15)
		bpp = 16;
	w = jz4760_lcd_info->osd.fg0.w;
	h = jz4760_lcd_info->osd.fg0.h;
	needroom = ((w * bpp + 7) >> 3) * h;
#if defined(CONFIG_FB_JZ4760_LCD_USE_2LAYER_FRAMEBUFFER)
	bpp = jz4760_lcd_info->osd.fg1.bpp;
	if (bpp == 18 || bpp == 24)
		bpp = 32;
	if (bpp == 15)
		bpp = 16;
	w = jz4760_lcd_info->osd.fg1.w;
	h = jz4760_lcd_info->osd.fg1.h;
	needroom += ((w * bpp + 7) >> 3) * h;
#endif

	for (page_shift = 0; page_shift < 12; page_shift++)
		if ((PAGE_SIZE << page_shift) >= needroom)
			break;

	if (cfb && cfb->fb.screen_base)
	{
		iounmap(cfb->fb.screen_base);
		cfb->fb.screen_base = NULL;
		release_mem_region(cfb->fb.fix.smem_start,
						   cfb->fb.fix.smem_len);
	}

	if (lcd_palette)
	{
		map = virt_to_page(lcd_palette);
		clear_bit(PG_reserved, &map->flags);
		free_pages((int)lcd_palette, 0);
	}

	if (lcd_frame0)
	{
		for (tmp = (unsigned char *)lcd_frame0;
			 tmp < lcd_frame0 + (PAGE_SIZE << page_shift);
			 tmp += PAGE_SIZE)
		{
			map = virt_to_page(tmp);
			clear_bit(PG_reserved, &map->flags);
		}
		free_pages((int)lcd_frame0, page_shift);
	}
}


static void jz4760fb_set_panel_mode(struct jz4760lcd_info *lcd_info)
{
	struct jz4760lcd_panel_t *panel = &lcd_info->panel;
#ifdef CONFIG_JZ4760_VGA_DISPLAY
	REG_TVE_CTRL |= TVE_CTRL_DAPD;
	REG_TVE_CTRL &= ~(TVE_CTRL_DAPD1 | TVE_CTRL_DAPD2 | TVE_CTRL_DAPD3);
#endif
	/* set bpp */
	lcd_info->panel.ctrl &= ~LCD_CTRL_BPP_MASK;
	if (lcd_info->osd.fg0.bpp == 1)
		lcd_info->panel.ctrl |= LCD_CTRL_BPP_1;
	else if (lcd_info->osd.fg0.bpp == 2)
		lcd_info->panel.ctrl |= LCD_CTRL_BPP_2;
	else if (lcd_info->osd.fg0.bpp == 4)
		lcd_info->panel.ctrl |= LCD_CTRL_BPP_4;
	else if (lcd_info->osd.fg0.bpp == 8)
		lcd_info->panel.ctrl |= LCD_CTRL_BPP_8;
	else if (lcd_info->osd.fg0.bpp == 15)
		lcd_info->panel.ctrl |= LCD_CTRL_BPP_16 | LCD_CTRL_RGB555;
	else if (lcd_info->osd.fg0.bpp == 16)
		lcd_info->panel.ctrl |= LCD_CTRL_BPP_16 | LCD_CTRL_RGB565;
	else if (lcd_info->osd.fg0.bpp > 16 && lcd_info->osd.fg0.bpp < 32 + 1)
	{
		lcd_info->osd.fg0.bpp = 32;
		lcd_info->panel.ctrl |= LCD_CTRL_BPP_18_24;
	}
	else
	{
		printk("The BPP %d is not supported\n", lcd_info->osd.fg0.bpp);
		lcd_info->osd.fg0.bpp = 32;
		lcd_info->panel.ctrl |= LCD_CTRL_BPP_18_24;
	}

	lcd_info->panel.cfg |= LCD_CFG_NEWDES; /* use 8words descriptor always */

	REG_LCD_CTRL = lcd_info->panel.ctrl;	 /* LCDC Controll Register */
	REG_LCD_CFG = lcd_info->panel.cfg;		 /* LCDC Configure Register */
	REG_SLCD_CFG = lcd_info->panel.slcd_cfg; /* Smart LCD Configure Register */

	if (lcd_info->panel.cfg & LCD_CFG_LCDPIN_SLCD) /* enable Smart LCD DMA */
		REG_SLCD_CTRL = SLCD_CTRL_DMA_EN;

	switch (lcd_info->panel.cfg & LCD_CFG_MODE_MASK)
	{
	case LCD_CFG_MODE_GENERIC_TFT:
	case LCD_CFG_MODE_INTER_CCIR656:
	case LCD_CFG_MODE_NONINTER_CCIR656:
	case LCD_CFG_MODE_SLCD:
	default: /* only support TFT16 TFT32, not support STN and Special TFT by now(10-06-2008)*/
		REG_LCD_VAT = (((panel->blw + panel->w + panel->elw + panel->hsw)) << 16) | (panel->vsw + panel->bfw + panel->h + panel->efw);
		REG_LCD_DAH = ((panel->hsw + panel->blw) << 16) | (panel->hsw + panel->blw + panel->w);
		REG_LCD_DAV = ((panel->vsw + panel->bfw) << 16) | (panel->vsw + panel->bfw + panel->h);
		REG_LCD_HSYNC = (0 << 16) | panel->hsw;
		REG_LCD_VSYNC = (0 << 16) | panel->vsw;
		break;
	}
}

static void jz4760fb_set_osd_mode(struct jz4760lcd_info *lcd_info)
{
	struct jz4760lcd_panel_t *panel = &lcd_info->panel;
	D("%s, %d\n", __FILE__, __LINE__);
	lcd_info->osd.osd_ctrl &= ~(LCD_OSDCTRL_OSDBPP_MASK);
	if (lcd_info->osd.fg1.bpp == 15)
		lcd_info->osd.osd_ctrl |= LCD_OSDCTRL_OSDBPP_15_16 | LCD_OSDCTRL_RGB555;
	else if (lcd_info->osd.fg1.bpp == 16)
		lcd_info->osd.osd_ctrl |= LCD_OSDCTRL_OSDBPP_15_16 | LCD_OSDCTRL_RGB565;
	else
	{
		lcd_info->osd.fg1.bpp = 32;
		lcd_info->osd.osd_ctrl |= LCD_OSDCTRL_OSDBPP_18_24;
	}

	REG_LCD_OSDC = lcd_info->osd.osd_cfg; /* F0, F1, alpha, */

	REG_LCD_OSDCTRL = lcd_info->osd.osd_ctrl; /* IPUEN, bpp */
	REG_LCD_RGBC = lcd_info->osd.rgb_ctrl;
	REG_LCD_BGC = lcd_info->osd.bgcolor;
	REG_LCD_KEY0 = lcd_info->osd.colorkey0;
	REG_LCD_KEY1 = lcd_info->osd.colorkey1;
	REG_LCD_ALPHA = lcd_info->osd.alpha;

	REG_LCD_IPUR = LCD_IPUR_IPUREN |
			(panel->blw + panel->w + panel->elw) * panel->vsw / 3;
}

static void jz4760fb_foreground_resize(struct jz4760lcd_info *lcd_info)
{
	int fg0_line_size, fg0_frm_size, fg1_line_size, fg1_frm_size;
	/*
	 * NOTE:
	 * Foreground change sequence:
	 * 	1. Change Position Registers -> LCD_OSDCTL.Change;
	 * 	2. LCD_OSDCTRL.Change -> descripter->Size
	 * Foreground, only one of the following can be change at one time:
	 * 	1. F0 size;
	 *	2. F0 position
	 * 	3. F1 size
	 *	4. F1 position
	 */

	/*
	 * The rules of f0, f1's position:
	 * 	f0.x + f0.w <= panel.w;
	 * 	f0.y + f0.h <= panel.h;
	 *
	 * When output is LCD panel, fg.y and fg.h can be odd number or even number.
	 * When output is TVE, as the TVE has odd frame and even frame,
	 * to simplified operation, fg.y and fg.h should be even number always.
	 *
	 */

	/* Foreground 0  */
	if (lcd_info->osd.fg0.x >= lcd_info->panel.w)
		lcd_info->osd.fg0.x = lcd_info->panel.w;
	if (lcd_info->osd.fg0.y >= lcd_info->panel.h)
		lcd_info->osd.fg0.y = lcd_info->panel.h;
	if (lcd_info->osd.fg0.x + lcd_info->osd.fg0.w > lcd_info->panel.w)
		lcd_info->osd.fg0.w = lcd_info->panel.w - lcd_info->osd.fg0.x;
	if (lcd_info->osd.fg0.y + lcd_info->osd.fg0.h > lcd_info->panel.h)
		lcd_info->osd.fg0.h = lcd_info->panel.h - lcd_info->osd.fg0.y;

	//	fg0_line_size = lcd_info->osd.fg0.w*((lcd_info->osd.fg0.bpp+7)/8);
	fg0_line_size = (lcd_info->osd.fg0.w * (lcd_info->osd.fg0.bpp) / 8);
	fg0_line_size = ((fg0_line_size + 3) >> 2) << 2; /* word aligned */
	fg0_frm_size = fg0_line_size * lcd_info->osd.fg0.h;

	fg1_line_size = lcd_info->osd.fg1.w * ((lcd_info->osd.fg1.bpp + 7) / 8);
	fg1_line_size = ((fg1_line_size + 3) >> 2) << 2; /* word aligned */
	fg1_frm_size = fg1_line_size * lcd_info->osd.fg1.h;

	if (lcd_info->osd.fg_change)
	{
		if (lcd_info->osd.fg_change & FG0_CHANGE_POSITION)
		{ /* F1 change position */
			REG_LCD_XYP0 = lcd_info->osd.fg0.y << 16 | lcd_info->osd.fg0.x;
		}
		if (lcd_info->osd.fg_change & FG1_CHANGE_POSITION)
		{ /* F1 change position */
			REG_LCD_XYP1 = lcd_info->osd.fg1.y << 16 | lcd_info->osd.fg1.x;
		}

		/* set change */
		if (!(lcd_info->osd.osd_ctrl & LCD_OSDCTRL_IPU) &&
			(lcd_info->osd.fg_change != FG_CHANGE_ALL))
			REG_LCD_OSDCTRL |= LCD_OSDCTRL_CHANGES;

		/* wait change ready??? */
		//		while ( REG_LCD_OSDS & LCD_OSDS_READY )	/* fix in the future, Wolfgang, 06-20-2008 */
		D("wait LCD_OSDS_READY\n");

		if (lcd_info->osd.fg_change & FG0_CHANGE_SIZE)
		{ /* change FG0 size */
			REG_LCD_SIZE0 = (lcd_info->osd.fg0.h << 16) | lcd_info->osd.fg0.w;
		}

		if (lcd_info->osd.fg_change & FG1_CHANGE_SIZE)
		{ /* change FG1 size*/
			REG_LCD_SIZE1 = lcd_info->osd.fg1.h << 16 | lcd_info->osd.fg1.w;
		}

		lcd_info->osd.fg_change = FG_NOCHANGE; /* clear change flag */
	}
}

static void jz4760fb_change_clock(struct jz4760lcd_info *lcd_info)
{

#if defined(CONFIG_FPGA)
	REG_LCD_REV = 0x00000004;
	printk("Fuwa test, pixclk divide REG_LCD_REV=0x%08x\n", REG_LCD_REV);
	printk("Fuwa test, pixclk %d\n", JZ_EXTAL / (((REG_LCD_REV & 0xFF) + 1) * 2));
#else
	unsigned int val = 0;
	unsigned int pclk;
	/* Timing setting */
	__cpm_stop_lcd();

	val = lcd_info->panel.fclk; /* frame clk */

	if ((lcd_info->panel.cfg & LCD_CFG_MODE_MASK) != LCD_CFG_MODE_SERIAL_TFT)
	{
		pclk = val * (lcd_info->panel.w + lcd_info->panel.hsw + lcd_info->panel.elw + lcd_info->panel.blw) * (lcd_info->panel.h + lcd_info->panel.vsw + lcd_info->panel.efw + lcd_info->panel.bfw); /* Pixclk */
	}
	else
	{
		/* serial mode: Hsync period = 3*Width_Pixel */
		pclk = val * (lcd_info->panel.w * 3 + lcd_info->panel.hsw + lcd_info->panel.elw + lcd_info->panel.blw) * (lcd_info->panel.h + lcd_info->panel.vsw + lcd_info->panel.efw + lcd_info->panel.bfw); /* Pixclk */
	}

#ifdef CONFIG_FB_JZ4760_TVE
	/********* In TVE mode PCLK = 27MHz ***********/
	if (lcd_info->panel.cfg & LCD_CFG_TVEN)
	{ /* LCDC output to TVE */
		//	   	__cpm_stop_tve();
		OUTREG32(CPM_CPPCR0, ((90 << CPPCR0_PLLM_LSB) | (2 << CPPCR0_PLLN_LSB) | (1 << CPPCR0_PLLOD_LSB) | (0x20 << CPPCR0_PLLST_LSB) | CPPCR0_PLLEN));
		REG_CPM_LPCDR |= LPCDR_LTCS;
		pclk = 27000000;
		val = __cpm_get_pllout2() / pclk; /* pclk */
		val--;
		__cpm_set_pixdiv(val);
		//		__cpm_start_tve();

		D("REG_CPM_LPCDR = 0x%08x\n", REG_CPM_LPCDR);
#if 0
#if defined(CONFIG_SOC_JZ4760) /* Jz4760D don't use LCLK */
		val = pclk * 3 ;	/* LCDClock > 2.5*Pixclock */

		val =(__cpm_get_pllout()) / val;
		if ( val > 0x1f ) {
			printk("lcd clock divide is too large, set it to 0x1f\n");
			val = 0x1f;
		}
		__cpm_set_ldiv( val );
#endif
#endif
		__cpm_select_pixclk_tve();

		REG_CPM_CPCCR |= CPCCR_CE; /* update divide */
	}
	else
#endif
	{									  /* LCDC output to  LCD panel */
		val = __cpm_get_pllout2() / pclk; /* pclk */
		val--;
		D("ratio: val = %d\n", val);
		if (val > 0x7ff)
		{
			printk("pixel clock divid is too large, set it to 0x7ff\n");
			val = 0x7ff;
		}

		__cpm_set_pixdiv(val);
		D("REG_CPM_LPCDR = 0x%08x\n", REG_CPM_LPCDR);
#if 0
#if defined(CONFIG_SOC_JZ4760) /* Jz4760D don't use LCLK */
		val = pclk * 3 ;	/* LCDClock > 2.5*Pixclock */
		val =__cpm_get_pllout2() / val;
		if ( val > 0x1f ) {
			printk("lcd clock divide is too large, set it to 0x1f\n");
			val = 0x1f;
		}
		__cpm_set_ldiv( val );
#endif
#endif
		__cpm_select_pixclk_lcd();
		REG_CPM_CPCCR |= CPCCR_CE; /* update divide */
	}

	D("REG_CPM_LPCDR=0x%08x\n", REG_CPM_LPCDR);
	D("REG_CPM_CPCCR=0x%08x\n", REG_CPM_CPCCR);

	jz_clocks.pixclk = __cpm_get_pixclk();
	printk("LCDC: PixClock:%d\n", jz_clocks.pixclk);
#if 0
#if defined(CONFIG_SOC_JZ4760) /* Jz4760D don't use LCLK */
	jz_clocks.lcdclk = __cpm_get_lcdclk();
	printk("LCDC: LcdClock:%d\n", jz_clocks.lcdclk);
#endif
#endif
	__cpm_start_lcd();
	udelay(1000);
	/*
	 * set lcd device clock and lcd pixel clock.
	 * what about TVE mode???
	 *
	 */
#endif
}

/*
 * jz4760fb_set_mode(), set osd configure, resize foreground
 *
 */
static void jz4760fb_set_mode(struct jz4760lcd_info *lcd_info)
{
	struct lcd_cfb_info *cfb = jz4760fb_info;

	jz4760fb_set_osd_mode(lcd_info);
	//jz4760fb_foreground_resize(lcd_info);
	jz4760fb_set_var(&cfb->fb.var, -1, &cfb->fb);
}

/*
 * jz4760fb_deep_set_mode,
 *
 */
static void jz4760fb_deep_set_mode(struct jz4760lcd_info *lcd_info)
{
	/* configurate sequence:
	 * 1. disable lcdc.
	 * 2. init frame descriptor.
	 * 3. set panel mode
	 * 4. set osd mode
	 * 5. start lcd clock in CPM
	 * 6. enable lcdc.
	 */

	__lcd_clr_ena();						 /* Quick Disable */
	lcd_info->osd.fg_change = FG_CHANGE_ALL; /* change FG0, FG1 size, postion??? */
	//jz4760fb_descriptor_init(lcd_info);
	jz4760fb_set_panel_mode(lcd_info);
	jz4760fb_set_mode(lcd_info);
	jz4760fb_change_clock(lcd_info);
	__lcd_set_ena(); /* enable lcdc */
}

static irqreturn_t jz4760fb_interrupt_handler(int irq, void *dev_id)
{
	struct lcd_cfb_info *cfb = dev_id;


	spin_lock(&lock);
	//SETREG32(IPU_STATUS,0);
	//writel(0, jzfb->ipu_base + IPU_STATUS);

	if (delay_flush == 0) {
	dma_cache_wback_inv((unsigned long)(lcd_frame0 + frame_yoffset),
				cfb->fb.fix.line_length * cfb->fb.var.yres);
	} else {
		delay_flush--;
	}

	ipu_update_address();
	vsync_count++;

	spin_unlock(&lock);

	wake_up_interruptible_all(&wait_vsync);
	return IRQ_HANDLED;
}

#ifdef CONFIG_PM

/*
 * Suspend the LCDC.
 */
static int jz4760_fb_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk("%s(): called.\n", __func__);

	screen_off();
	//ctrl_disable();
	__lcd_clr_ena();

	__cpm_stop_lcd();

	return 0;
}

/*
 * Resume the LCDC.
 */
static int jz4760_fb_resume(struct platform_device *pdev)
{
	struct lcd_cfb_info *cfb = jz4760fb_info;

	printk("%s(): called.\n", __func__);

	__cpm_start_lcd();
	screen_on();
	__lcd_set_ena();

	return 0;
}

#else
#define jzfb_suspend NULL
#define jzfb_resume NULL
#endif /* CONFIG_PM */

/* The following routine is only for test */

#if JZ_FB_DEBUG
static void test_gpio(int gpio_num, int delay)
{
	__gpio_as_output(gpio_num);
	while (1)
	{
		__gpio_set_pin(gpio_num);
		udelay(delay);
		__gpio_clear_pin(gpio_num);
		udelay(delay);
	}
}
static void display_v_color_bar(int w, int h, int bpp)
{
	int i, j, wpl, data = 0;
	int *ptr;
	ptr = (int *)lcd_frame0;
	//	ptr = (int *)lcd_frame1;
	wpl = w * bpp / 32;
	if (!(bpp > 8))
		switch (bpp)
		{
		case 1:
			for (j = 0; j < h; j++)
				for (i = 0; i < wpl; i++)
				{
					*ptr++ = 0x00ff00ff;
				}
			break;
		case 2:
			for (j = 0; j < h; j++)
				for (i = 0; i < wpl; i++)
				{
					data = (i % 4) * 0x55555555;
					*ptr++ = data;
				}
			break;
		case 4:
			for (j = 0; j < h; j++)
				for (i = 0; i < wpl; i++)
				{
					data = (i % 16) * 0x11111111;
					*ptr++ = data;
				}
			break;
		case 8:
			for (j = 0; j < h; j++)
				for (i = 0; i < wpl; i += 2)
				{
					data = (i % (256)) * 0x01010101;
					*ptr++ = data;
					*ptr++ = data;
				}
			break;
		}
	else
	{
		switch (bpp)
		{
		case 16:
			for (j = 0; j < h; j++)
				for (i = 0; i < wpl; i++)
				{
					if ((i / 4) % 8 == 0)
						*ptr++ = 0xffffffff;
					else if ((i / 4) % 8 == 1)
						*ptr++ = 0xf800f800;
					else if ((i / 4) % 8 == 2)
						*ptr++ = 0xffe0ffe0;
					else if ((i / 4) % 8 == 3)
						*ptr++ = 0x07e007e0;
					else if ((i / 4) % 8 == 4)
						*ptr++ = 0x07ff07ff;
					else if ((i / 4) % 8 == 5)
						*ptr++ = 0x001f001f;
					else if ((i / 4) % 8 == 6)
						*ptr++ = 0xf81ff81f;
					else if ((i / 4) % 8 == 7)
						*ptr++ = 0x00000000;
				}
			break;
		case 18:
		case 24:
		case 32:
		default:
#if 1
			for (j = 0; j < h; j++)
				for (i = 0; i < wpl; i++)
				{
					if ((i / 8) % 8 == 7)
						*ptr++ = 0xffffff;
					else if ((i / 8) % 8 == 1)
						*ptr++ = 0xff0000;
					else if ((i / 8) % 8 == 2)
						*ptr++ = 0xffff00;
					else if ((i / 8) % 8 == 3)
						*ptr++ = 0x00ff00;
					else if ((i / 8) % 8 == 4)
						*ptr++ = 0x00ffff;
					else if ((i / 8) % 8 == 5)
						*ptr++ = 0x0000ff;
					else if ((i / 8) % 8 == 6)
						*ptr++ = 0xff00ff;
					else if ((i / 8) % 8 == 0)
						*ptr++ = 0x000000;
				}
#else
			for (j = 0; j < h; j++)
				for (i = 0; i < wpl; i++)
				{
					if ((i / 8) % 8 == 7)
						*ptr++ = 0x00ff0000;
					else if ((i / 8) % 8 == 1)
						*ptr++ = 0xffff0000;
					else if ((i / 8) % 8 == 2)
						*ptr++ = 0x20ff0000;
					else if ((i / 8) % 8 == 3)
						*ptr++ = 0x40ff0000;
					else if ((i / 8) % 8 == 4)
						*ptr++ = 0x60ff0000;
					else if ((i / 8) % 8 == 5)
						*ptr++ = 0x80ff0000;
					else if ((i / 8) % 8 == 6)
						*ptr++ = 0xa0ff0000;
					else if ((i / 8) % 8 == 0)
						*ptr++ = 0xc0ff0000;
				}
#endif
			break;
		}
	}
}
static void display_h_color_bar(int w, int h, int bpp)
{
	int i, data = 0;
	int *ptr;
	int wpl; //word_per_line
	ptr = (int *)lcd_frame0;
	//	ptr = (int *)lcd_frame1;
	wpl = w * bpp / 32;
	if (!(bpp > 8))
		for (i = 0; i < wpl * h; i++)
		{
			switch (bpp)
			{
			case 1:
				if (i % (wpl * 8) == 0)
					data = ((i / (wpl * 8)) % 2) * 0xffffffff;
				*ptr++ = data;
				break;
			case 2:
				if (i % (wpl * 8) == 0)
					data = ((i / (wpl * 8)) % 4) * 0x55555555;
				*ptr++ = data;
				break;
			case 4:
				if (i % (wpl * 8) == 0)
					data = ((i / (wpl * 8)) % 16) * 0x11111111;
				*ptr++ = data;
				break;
			case 8:
				if (i % (wpl * 8) == 0)
					data = ((i / (wpl * 8)) % 256) * 0x01010101;
				*ptr++ = data;
				break;
			}
		}
	else
	{

		switch (bpp)
		{
		case 15:
		case 16:
			for (i = 0; i < wpl * h; i++)
			{
				if (((i / (wpl * 8)) % 8) == 0)
					*ptr++ = 0xffffffff;
				else if (((i / (wpl * 8)) % 8) == 1)
					*ptr++ = 0xf800f800;
				else if (((i / (wpl * 8)) % 8) == 2)
					*ptr++ = 0xffe0ffe0;
				else if (((i / (wpl * 8)) % 8) == 3)
					*ptr++ = 0x07e007e0;
				else if (((i / (wpl * 8)) % 8) == 4)
					*ptr++ = 0x07ff07ff;
				else if (((i / (wpl * 8)) % 8) == 5)
					*ptr++ = 0x001f001f;
				else if (((i / (wpl * 8)) % 8) == 6)
					*ptr++ = 0xf81ff81f;
				else if (((i / (wpl * 8)) % 8) == 7)
					*ptr++ = 0x00000000;
			}
			break;
		case 18:
		case 24:
		case 32:
		default:
			for (i = 0; i < wpl * h; i++)
			{
				if (((i / (wpl * 8)) % 8) == 7)
					*ptr++ = 0xffffff;
				else if (((i / (wpl * 8)) % 8) == 2)
					*ptr++ = 0xff0000;
				else if (((i / (wpl * 8)) % 8) == 4)
					*ptr++ = 0xffff00;
				else if (((i / (wpl * 8)) % 8) == 6)
					*ptr++ = 0x00ff00;
				else if (((i / (wpl * 8)) % 8) == 1)
					*ptr++ = 0x00ffff;
				else if (((i / (wpl * 8)) % 8) == 3)
					*ptr++ = 0x0000ff;
				else if (((i / (wpl * 8)) % 8) == 5)
					*ptr++ = 0x000000;
				else if (((i / (wpl * 8)) % 8) == 0)
					*ptr++ = 0xff00ff;
			}
			break;
		}
	}
}
#endif

/* Backlight Control Interface via sysfs
 *
 * LCDC:
 * Enabling LCDC when LCD backlight is off will only affects cfb->display.
 *
 * Backlight:
 * Changing the value of LCD backlight when LCDC is off will only affect the cfb->backlight_level.
 *
 * - River.
 */
static int screen_off(void)
{
	struct lcd_cfb_info *cfb = jz4760fb_info;

	__lcd_close_backlight();
	__lcd_display_off();

#ifdef HAVE_LCD_PWM_CONTROL
	if (cfb->b_lcd_pwm)
	{
		__lcd_pwm_stop();
		cfb->b_lcd_pwm = 0;
	}
#endif

	cfb->b_lcd_display = 0;

	return 0;
}

static int screen_on(void)
{
	struct lcd_cfb_info *cfb = jz4760fb_info;

	__lcd_display_on();

	/* Really restore LCD backlight when LCD backlight is turned on. */
	if (cfb->backlight_level)
	{
#ifdef HAVE_LCD_PWM_CONTROL
		if (!cfb->b_lcd_pwm)
		{
			__lcd_pwm_start();
			cfb->b_lcd_pwm = 1;
		}
#endif
		__lcd_set_backlight_level(cfb->backlight_level);
	}

	cfb->b_lcd_display = 1;

	return 0;
}

static int jz4760fb_set_backlight_level(int n)
{
	struct lcd_cfb_info *cfb = jz4760fb_info;

	if (n)
	{
		if (n > LCD_MAX_BACKLIGHT)
			n = LCD_MAX_BACKLIGHT;

		if (n < LCD_MIN_BACKLIGHT)
			n = LCD_MIN_BACKLIGHT;

		/* Really change the value of backlight when LCDC is enabled. */
		if (cfb->b_lcd_display)
		{
#ifdef HAVE_LCD_PWM_CONTROL
			if (!cfb->b_lcd_pwm)
			{
				__lcd_pwm_start();
				cfb->b_lcd_pwm = 1;
			}
#endif
			__lcd_set_backlight_level(n);
		}
	}
	else
	{
		/* Turn off LCD backlight. */
		__lcd_close_backlight();

#ifdef HAVE_LCD_PWM_CONTROL
		if (cfb->b_lcd_pwm)
		{
			__lcd_pwm_stop();
			cfb->b_lcd_pwm = 0;
		}
#endif
	}

	cfb->backlight_level = n;

	return 0;
}

static ssize_t show_bl_level(struct device *device,
							 struct device_attribute *attr, char *buf)
{
	struct lcd_cfb_info *cfb = jz4760fb_info;

	return snprintf(buf, PAGE_SIZE, "%d\n", cfb->backlight_level);
}

static ssize_t store_bl_level(struct device *device,
							  struct device_attribute *attr,
							  const char *buf, size_t count)
{
	int n;
	char *ep;

	n = simple_strtoul(buf, &ep, 0);
	if (*ep && *ep != '\n')
		return -EINVAL;

	jz4760fb_set_backlight_level(n);

	return count;
}

static struct device_attribute device_attrs[] = {
	__ATTR(backlight_level, S_IRUGO | S_IWUSR, show_bl_level, store_bl_level),
};

static int jz4760fb_device_attr_register(struct fb_info *fb_info)
{
	int error = 0;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(device_attrs); i++)
	{
		error = device_create_file(fb_info->dev, &device_attrs[i]);

		if (error)
			break;
	}

	if (error)
	{
		while (--i >= 0)
			device_remove_file(fb_info->dev, &device_attrs[i]);
	}

	return 0;
}

static int jz4760fb_device_attr_unregister(struct fb_info *fb_info)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(device_attrs); i++)
		device_remove_file(fb_info->dev, &device_attrs[i]);

	return 0;
}
/* End */

static void gpio_init(void)
{
	__lcd_display_pin_init();  //LCD REST
#if defined(CONFIG_JZ4760_LCD_RS97_V10)
		REG_GPIO_PXFUNS(2) = 0x000c31fc;
		REG_GPIO_PXTRGC(2) = 0x000c31fc;
		REG_GPIO_PXSELC(2) = 0x000c31fc;
		REG_GPIO_PXPES(2)  = 0x000c31fc;
		return;
#endif
	/* gpio init __gpio_as_lcd */
	if (jz4760_lcd_info->panel.cfg & LCD_CFG_MODE_TFT_16BIT)
		__gpio_as_lcd_16bit();
	else if (jz4760_lcd_info->panel.cfg & LCD_CFG_MODE_TFT_24BIT)
		__gpio_as_lcd_24bit();
	else
 		__gpio_as_lcd_18bit();

	/* In special mode, we only need init special pin,
	 * as general lcd pin has init in uboot */
#if defined(CONFIG_SOC_JZ4760)
	switch (jz4760_lcd_info->panel.cfg & LCD_CFG_MODE_MASK) {
	case LCD_CFG_MODE_SPECIAL_TFT_1:
	case LCD_CFG_MODE_SPECIAL_TFT_2:
	case LCD_CFG_MODE_SPECIAL_TFT_3:
		__gpio_as_lcd_special();
		break;
	default:
		;
	}
#endif

	return;

	return;
}

static void set_bpp_to_ctrl_bpp(void)
{
	switch (jz4760_lcd_info->osd.fg0.bpp)
	{
	case 15:
	case 16:
		break;

	case 17 ... 32:
		jz4760_lcd_info->osd.fg0.bpp = 32;
		break;

	default:
		E("FG0: BPP (%d) not support, Set BPP 32.\n",
		  jz4760_lcd_info->osd.fg0.bpp);

		jz4760_lcd_info->osd.fg0.bpp = 32;
		break;
	}

	switch (jz4760_lcd_info->osd.fg1.bpp)
	{
	case 15:
	case 16:
		break;

	case 17 ... 32:
		jz4760_lcd_info->osd.fg1.bpp = 32;
		break;

	default:
		E("FG1: BPP (%d) not support, Set BPP 32.\n",
		  jz4760_lcd_info->osd.fg1.bpp);

		jz4760_lcd_info->osd.fg1.bpp = 32;
		break;
	}

	return;
}

static void slcd_init(void)
{
	/* Configure SLCD module for setting smart lcd control registers */
#if defined(CONFIG_FB_JZ4760_SLCD)
	__lcd_as_smart_lcd();
	__slcd_disable_dma();
	__init_slcd_bus(); /* Note: modify this depend on you lcd */

#endif
	return;
}

static int proc_lcd_backlight_read_proc(
	char *page, char **start, off_t off,
	int count, int *eof, void *data)
{
	return sprintf(page, "%u\n", backlight_value);
}

static int proc_lcd_backlight_write_proc(
	struct file *file, const char *buffer,
	unsigned long count, void *data)
{

	backlight_value = simple_strtoul(buffer, 0, 10);
	//printk("backlight_value is %d \n",backlight_value);
	if (backlight_value == 0)
	{
		__gpio_clear_pin(GPIO_LCD_VCC_EN_N);
		__lcd_close_backlight();

		//__gpio_clear_pin(GPIO_TP_VCC_EN); //allen add for touch
	}
	else
	{
		__gpio_set_pin(GPIO_LCD_VCC_EN_N);
		__lcd_set_backlight_level(backlight_value);

		//__gpio_set_pin(GPIO_TP_VCC_EN); //allen add for touch
	}
	return count;
}
#ifdef VSYNC_OPTION
static int proc_vsync_mode_read_proc(
	char *page, char **start, off_t off,
	int count, int *eof, void *data)
{
	return sprintf(page, "%lu\n", vsync_on);
}

static int proc_vsync_mode_write_proc(
	struct file *file, const char *buffer,
	unsigned long count, void *data)
{
	int tmp_output_flag = simple_strtoul(buffer, 0, 10);
	vsync_on = tmp_output_flag
	;
	return count;
}
#endif
static int __devinit jz4760_fb_probe(struct platform_device *dev)
{
	struct lcd_cfb_info *cfb;
	int ret;
	int rv = 0;
	cpm_start_clock(CGM_IPU);
	cfb = jz4760fb_alloc_fb_info();
	if (!cfb)
		goto failed;

	screen_off();
	ctrl_disable();

	gpio_init();
	slcd_init();

	set_bpp_to_ctrl_bpp();
	init_waitqueue_head(&wait_vsync);
	spin_lock_init(&lock);
	delay_flush = 0;
#ifdef VSYNC_OPTION	
	vsync_on = 1;
#endif
	/* init clk */
	//jz4760fb_change_clock(jz4760_lcd_info);
	jz4760fb_device_attr_register(&cfb->fb);
	if (devm_request_irq(&dev->dev, IRQ_IPU,
				jz4760fb_interrupt_handler, 0, "ipu", cfb)) 
	{
		dev_err(&dev->dev, "Failed to request IRQ.\n");
		goto failed;
	}

	rv = jz4760fb_map_smem(cfb);
	if (rv)
		goto failed;

	
	
	
	ipu_driver_open_tv(320, 240, 320, 480);
	ipu_update_address();

	jz4760fb_deep_set_mode(jz4760_lcd_info);
		
	rv = register_framebuffer(&cfb->fb);
	if (rv < 0)
	{
		D("Failed to register framebuffer device.");
		goto failed;
	}

	printk("fb%d: %s frame buffer device, using %dK of video memory\n",
		   cfb->fb.node, cfb->fb.fix.id, cfb->fb.fix.smem_len >> 10);


	// if (request_irq(IRQ_LCD, jz4760fb_interrupt_handler, IRQF_DISABLED,
					// "lcd", 0))
	// {
		// D("Faield to request LCD IRQ.\n");
		// rv = -EBUSY;
		// goto failed;
	// }

	ctrl_enable();
	
	__lcd_display_on();
		
#if defined(RGB_TEST)
	int i = 0;
	unsigned short*ptr;
	ptr = (unsigned short*)lcd_frame0;

	for(i = 0 ; i < LCD_SCREEN_W*LCD_SCREEN_H/3; i++)
	{
		*ptr++ = 0xf800;
	}
	
	for(i = 0 ; i < LCD_SCREEN_W*LCD_SCREEN_H/3; i++)
	{
		*ptr++ = 0x07e0;
	}
	
	for(i = 0 ; i < LCD_SCREEN_W*LCD_SCREEN_H/3; i++)
	{
		*ptr++ = 0x001f;
	}
	
	dma_cache_wback((unsigned int)lcd_frame0, LCD_SCREEN_W * LCD_SCREEN_H * 2);
	mdelay(3000);
#else
	memset(lcd_frame0, 0x00, LCD_SCREEN_W * LCD_SCREEN_H * 6);
	dma_cache_wback_inv((unsigned int)lcd_frame0, LCD_SCREEN_W * LCD_SCREEN_H * 6);

#endif
	//Avoid whie flash
	
	
	mdelay(120);

	/* Really restore LCD backlight when LCD backlight is turned on. */
	if (cfb->backlight_level) {
#ifdef HAVE_LCD_PWM_CONTROL
		if (!cfb->b_lcd_pwm) {
			__lcd_pwm_start();
			cfb->b_lcd_pwm = 1;
		}
#endif
		__lcd_set_backlight_level(cfb->backlight_level);
	}

	cfb->b_lcd_display = 1;

	//screen_on(); //avoid white flash


#if JZ_FB_DEBUG
	display_h_color_bar(jz4760_lcd_info->osd.fg0.w, jz4760_lcd_info->osd.fg0.h, jz4760_lcd_info->osd.fg0.bpp);

	print_lcdc_registers();
#endif

	return 0;

failed:
	jz4760fb_unmap_smem(cfb);
	jz4760fb_free_fb_info(cfb);

	return rv;
}

static int __devexit jz4760_fb_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver jz4760_fb_driver = {
	.probe = jz4760_fb_probe,
	.remove = jz4760_fb_remove,
	.suspend = jz4760_fb_suspend,
	.resume = jz4760_fb_resume,
	.driver = {
		.name = "jz-lcd",
		.owner = THIS_MODULE,
	},
};


static int __init jz4760_fb_init(void)
{
	struct proc_dir_entry *res;

	res = create_proc_entry("jz/lcd_backlight", 0, NULL);
	if(res)
	{
		res->read_proc = proc_lcd_backlight_read_proc;
		res->write_proc = proc_lcd_backlight_write_proc;
	}
#ifdef VSYNC_OPTION
	
	res = create_proc_entry("jz/vsync", 0, NULL);
	if(res)
	{
		res->read_proc = proc_vsync_mode_read_proc;
		res->write_proc = proc_vsync_mode_write_proc;
		res->data = NULL;
	}
#endif	
	return platform_driver_register(&jz4760_fb_driver);
}

static void __exit jz4760_fb_cleanup(void)
{
	platform_driver_unregister(&jz4760_fb_driver);
}

module_init(jz4760_fb_init);
module_exit(jz4760_fb_cleanup);
