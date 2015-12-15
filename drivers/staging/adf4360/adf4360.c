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

struct adf4360_state {
	struct spi_device				*spi;
	struct adf4360_platform_data	*pdata;
	u32								regs[3];
	
	__be32							val ____cacheline_aligned;
};

static int adf4360_write_reg(struct adf4360_state *st, int reg) {
	int ret;
	
	st->val = cpu_to_be32(st->regs[i] | (i & 0x03));
	ret = spi_write(st->spi, &st->val, 3);
	if(ret < 0)
		return ret;
	dev_err(&st->spi->dev, "[%d] 0x%X\n", i, (u32)st->regs[i] | (i & 0x03));  //  XXX Should be dev_dbg
	
	return 0;
}

static int adf4360_sync_config(struct adf4360_state *st) {
	int ret;
	
	ret = adf4360_write_reg(st, AD4360_R_REG);
	if(ret < 0)
		return ret;
		
	ret = adf4360_write_reg(st, AD4360_CONTROL_REG);
	if(ret < 0)
		return ret;
		
	msleep(10);
	
	ret = adf4360_write_reg(st, AD4360_N_REG);
	if(ret < 0)
		return ret;
		
	return 0;
}

static int adf4360_probe(struct spi_device *spi) {
	struct *adf4360_platform_data pdata;
	
	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	strncpy(&pdata->name[0], "ad4360", 32);
	
	spi_set_drvdata(spi, pdata);
	
	
}

static int adf4360_remove(struct spi_device *spi) {
}

static const struct of_device_id adf4360_of_id[] = {
	{ .compatible = "adi,adf4360", },
	{ /* senitel */ }
};
MODULE_DEVICE_TABLE(of, ad9832_of_id);

static const struct spi_device_id ad4360_id[] = {
	{"adf4360", 4360},
	{}
};

static struct spi_driver adf_4350_driver = {
	.driver = {
		.name 	= "adf4350",
		.owner	= THIS_MODULE,
	}
	.probe		= adf4350_probe,
	.remove		= adf4350_remove,
	.id_table	= adf4350_id,
};
module_spi_driver(adf4350_driver);