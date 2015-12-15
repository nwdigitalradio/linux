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

struct adf4360_state {
	struct spi_device				*spi;
	struct adf4360_platform_data	*pdata;
	__be32							regs[3] ____cacheline_aligned;  // XXX be?
	
	struct spi_transfer				xfers[3];
	struct spi_message				message;
	
};

static int adf4360_sync_config(struct adf4360_state *st) {
	return spi_sync(&st->spi, &st->message);
}

static ssize_t rcounter_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct ad4360_state *st = dev_get_drvdata(dev);
	
	adf4360_sync_config(st);
	
	return count;
}

static DEVICE_ATTR(r-counter, S_IWUSR, NULL, rcounter_store);

static int adf4360_probe(struct spi_device *spi) {
	struct adf4360_state *st;
	int i;
	
	// XXX need to deal with pdata here
	
	st = devm_kzalloc(dev, sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;
	
	spi_set_drvdata(spi, st);
	
	st->spi = spi;
	
	spi_message_init(&st->message);
	st->xfer[1].delay_usecs = 10000;
	for(i = 0; i < 3; ++i) {
		st->xfer[i].tx_buf = &st->regs[i];
		st->xfer[i].len = 3;
		st->xfer[i].cs_change = 1;
		spi_message_add_tail(&st->xfer[i], &st->message);
	}
	
	regs[0] = 0x00000001;

	sysfs_create_file(&spi->dev->kobj, &dev_attr_r-counter_name);
}

static int adf4360_remove(struct spi_device *spi) {
	struct ad4360_state *st = spi_get_drvdata(spi);
	
	kfree(st);
}

static const struct of_device_id adf4360_of_id[] = {
	{ .compatible = "adi,adf4360", },
	{ /* senitel */ }
};
MODULE_DEVICE_TABLE(of, ad9832_of_id);

static const struct spi_device_id adf4360_id[] = {
	{"adf4360", 4360},
	{}
};

static struct spi_driver adf4360_driver = {
	.driver = {
		.name 	= "adf4360",
		.owner	= THIS_MODULE,
		.of_match_table = ad4360_of_id,
	}
	.probe		= adf4360_probe,
	.remove		= adf4360_remove,
	.id_table	= adf4360_id,
};
module_spi_driver(adf4360_driver);

MODULE_DESCRIPTION("Analog Devices ADF4360 Driver");
MODULE_AUTHOR("Jeremy McDermond <nh6z@nh6z.net>");
MODULE_LICENSE("GPL");

