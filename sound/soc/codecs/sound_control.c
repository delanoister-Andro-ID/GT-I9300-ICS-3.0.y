/*
 * sound_control.c  --  WM8994 Enhacement driver
 *
 * Sound Control V1 - Copyright 2012 Francisco Franco
 *
 * Based on Supercurio's original idea Voodoo Sound
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <trace/events/asoc.h>

#include <linux/mfd/wm8994/core.h>
#include <linux/mfd/wm8994/registers.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/mfd/wm8994/gpio.h>

#include <linux/i2c/fm34_we395.h>

#include "sound_control.h"

#include "wm8994.h"

bool bypass_write_hook = false;

unsigned short volume_left = 62;
unsigned short volume_right = 62;

struct snd_soc_codec *codec_;

void update_volume()
{
	unsigned short val;
	
	/* check this later */
	bypass_write_hook = true;
	
	if (volume_left > 62)
		volume_left = 62;
	if (volume_right > 62)
		volume_right = 62;
		
	val = (WM8994_HPOUT1L_MUTE_N | volume_left);
	val |= WM8994_HPOUT1L_ZC;
	wm8994_write(codec_, WM8994_LEFT_OUTPUT_VOLUME, val);
	
	val = (WM8994_HPOUT1_VU | WM8994_HPOUT1R_MUTE_N | volume_right);
	val |= WM8994_HPOUT1L_ZC;
	wm8994_write(codec_, WM8994_RIGHT_OUTPUT_VOLUME, val);
	bypass_write_hook = false;
	
}

static ssize_t show_volume_boost(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", (volume_left + volume_right) / 2);
}

static ssize_t store_volume_boost(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned short vol;
	if (sscanf(buf, "%hu", &vol) == 1)
	{
		volume_left = volume_right = vol;
		update_volume();
	}
	return size;
}

static DEVICE_ATTR(volume_boost, S_IRUGO | S_IWUGO , show_volume_boost, store_volume_boost);

static struct attribute *sound_control_attributes[] = {
	&dev_attr_volume_boost.attr,
	NULL
};

static struct attribute_group sound_control_group = {
	.attrs = sound_control_attributes,
};

static struct miscdevice sound_control_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sound_control",
};

unsigned int sound_control_wm8994_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int value)
{
	struct wm8994_priv *wm8994 = snd_soc_codec_get_drvdata(codec);
	
	if(!bypass_write_hook)
	{
		if (wm8994->jackdet)
		{
			if (reg == WM8994_LEFT_OUTPUT_VOLUME)
				value = (WM8994_HPOUT1_VU | WM8994_HPOUT1L_MUTE_N | volume_left);
			if (reg == WM8994_RIGHT_OUTPUT_VOLUME)
				value = (WM8994_HPOUT1_VU | WM8994_HPOUT1R_MUTE_N | volume_right);
		}
	}
	
	return value;
}

void sound_control_wm8994_pcm_probe(struct snd_soc_codec *codec)
{
	printk("Sound Control");
	misc_register(&sound_control_device);
	if (sysfs_create_group(&sound_control_device.this_device->kobj, &sound_control_group) < 0)
	{
		printk("%s sysfs_create_group fail\n", __FUNCTION__);
		pr_err("Failed to create sysfs group for device (%s)!\n", sound_control_device.name);
	}
	
	codec_ = codec;
}