/* ADF4360 SPI Synthesizer Driver
 * 
 * Copyright 2015 Northwest Digital Radio
 *
 * Licensed under the GPL-2.
 */
 
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

#include "adf4360.h"

#define ADF4360_SET_REGISTER(x)		cpu_to_be32((x) << 8);

struct adf4360_state {
	struct spi_device				*spi;
	struct adf4360_platform_data	*pdata;
	__be32							reg[3] ____cacheline_aligned;  // XXX be?
	
	struct spi_transfer				xfer[3];
	struct spi_message				message;
	
	u16								rcounter;
	u8								band_select;
	u8								anti_backlash;
	u8								lock_precision;
	
};

static int adf4360_sync_config(struct adf4360_state *st) {
	u32 reg = 0;
	
	//  Set the R register settings
	reg = ADF4360_R_REG | 
          ADF4360_REG1_BAND_SELECT_CLOCK(st->band_select) |
          ADF4360_REG1_ANTI_BACKLASH(st->anti_backlash) |
          ADF4360_REG1_R_COUNTER(st->rcounter);
	if(st->lock_precision)
		reg |= ADF4360_REG1_LOCK_PRECISION_5_CYCLES_EN;
	st->reg[ADF4360_R_REG] = ADF4360_SET_REGISTER(reg);

	return spi_sync(st->spi, &st->message);
}

static ssize_t rcounter_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct adf4360_state *st = dev_get_drvdata(dev);
	
	if(kstrtou16(buf, 0, &st->rcounter)) {
		dev_err(dev, "Invalid rcounter value\n");
		return -EFAULT;
	}
	                             
	adf4360_sync_config(st);
	
	return count;
}

static DEVICE_ATTR(rcounter, S_IWUSR, NULL, rcounter_store);

static int adf4360_probe(struct spi_device *spi) {
	struct adf4360_state *st;
	int i;
	int ret;
	
	// XXX need to deal with pdata here
	
	st = devm_kzalloc(&spi->dev, sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;
	
	spi_set_drvdata(spi, st);
	
	st->spi = spi;
	
	spi_message_init(&st->message);
	st->xfer[1].delay_usecs = 10000;
	for(i = 0; i < 3; ++i) {
		st->xfer[i].tx_buf = &st->reg[i];
		st->xfer[i].len = 3;
		st->xfer[i].cs_change = 1;
		spi_message_add_tail(&st->xfer[i], &st->message);
	}
	
	st->reg[0] = ADF4360_SET_REGISTER(0x00000001);
	st->reg[1] = ADF4360_SET_REGISTER(0x00800000);
	st->reg[2] = ADF4360_SET_REGISTER(0x00555555);
	
	// XXX Defaults here -- get from elsewhere probably.
	st->band_select = 2;
	st->anti_backlash = 0;
	st->lock_precision = 1;

	ret = device_create_file(&spi->dev, &dev_attr_rcounter);
	if(ret > 0)
		dev_err(&spi->dev, "Couldn't create rcounter device file"); 
		
	// XXX Need to do a sync at the end of stuff.
	
	return 0;
}

static int adf4360_remove(struct spi_device *spi) {
	// struct ad4360_state *st = spi_get_drvdata(spi);
	
	return 0;
}

static const struct of_device_id adf4360_of_id[] = {
	{ .compatible = "adi,adf4360", },
	{ /* senitel */ }
};
MODULE_DEVICE_TABLE(of, adf4360_of_id);

static const struct spi_device_id adf4360_id[] = {
	{"adf4360", 4360},
	{}
};
MODULE_DEVICE_TABLE(spi, adf4360_id);

static struct spi_driver adf4360_driver = {
	.driver = {
		.name 	= "adf4360",
		.owner	= THIS_MODULE,
		.of_match_table = adf4360_of_id,
	},
	.probe		= adf4360_probe,
	.remove		= adf4360_remove,
	.id_table	= adf4360_id,
};
module_spi_driver(adf4360_driver);

MODULE_DESCRIPTION("Analog Devices ADF4360 Driver");
MODULE_AUTHOR("Jeremy McDermond <nh6z@nh6z.net>");
MODULE_LICENSE("GPL");

