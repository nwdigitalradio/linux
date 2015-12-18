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
	__be32							reg[3] ____cacheline_aligned;
	
	struct spi_transfer				sync_xfers[3];
	struct spi_transfer				n_xfer;
	struct spi_message				sync_message;
	struct spi_message				n_message;
	
	//  R latch values
	u16								rcounter;
	u8								band_select;
	u8								anti_backlash;
	u8								lock_precision;
	
	//  Control latch values
	u8 								prescaler;
	u8								powerdown;
	u16								cp_current[2];
	u8								output_power;
	u8								muxout;
	u8								core_power;
	u8								mute_til_lock;
	u8								cp_gain;
	u8								threestate;
	u8								pd_polarity_pos;
	u8								counter_reset;
	
	// N latch values
	u16								bcounter;
	u8								acounter;
	u8								divide_by_2;
	u8								prescaler_input;
	u8								cp_gain_perm;
};

static u32 adf4360_construct_reg(struct adf4360_state *st, char regnum) {
	u32 reg = 0;
	
	switch(regnum) {
		case ADF4360_N_REG:
			reg = ADF4360_N_REG |
				  ADF4360_REG2_B_COUNTER(st->bcounter) |
				  ADF4360_REG2_A_COUNTER(st->acounter);
			if(st->divide_by_2)
				reg |= ADF4360_REG2_DIVIDE_BY_TWO;
			if(st->prescaler_input)
				reg |= ADF4360_REG2_PRESCALER_INPUT;
			if(st->cp_gain_perm)
				reg |= ADF4360_REG2_CP_GAIN_PERM;
			break;
		case ADF4360_R_REG:
			reg = ADF4360_R_REG | 
				  ADF4360_REG1_BAND_SELECT_CLOCK(st->band_select) |
				  ADF4360_REG1_ANTI_BACKLASH(st->anti_backlash) |
				  ADF4360_REG1_R_COUNTER(st->rcounter);
			if(st->lock_precision)
				reg |= ADF4360_REG1_LOCK_PRECISION_5_CYCLES_EN;
			break;
		case ADF4360_CONTROL_REG:
			reg = ADF4360_CONTROL_REG |
				  ADF4360_REG0_PRESCALER(st->prescaler) |
				  ADF4360_REG0_POWERDOWN(st->powerdown) |
				  ADF4360_REG0_CHARGE_PUMP_CURR1_uA(st->cp_current[0]) |
				  ADF4360_REG0_CHARGE_PUMP_CURR2_uA(st->cp_current[1]) |
				  ADF4360_REG0_OUTPUT_PWR(st->output_power) |
				  ADF4360_REG0_MUXOUT(st->muxout) |
				  ADF4360_REG0_CORE_POWER_mA(st->core_power);
			if(st->mute_til_lock)
				reg |= ADF4360_REG0_MUTE_TIL_LOCK_EN;
			if(st->cp_gain)
				reg |= ADF4360_REG0_CP_GAIN;
			if(st->threestate)
				reg |= ADF4360_REG0_CP_THREESTATE_EN;
			if(st->pd_polarity_pos)
				reg |= ADF4360_REG0_PD_POLARITY_POS;
			if(st->counter_reset)
				reg |= ADF4360_REG0_COUNTER_RESET;
			break;
		default:
			dev_err(&st->spi->dev, "Invalid register value: %d", reg);
			return 0;
	}
	
	return cpu_to_be32(reg << 8);
}

static int adf4360_sync_config(struct adf4360_state *st) {
	int i;
	
	for(i = 0; i < 3; ++i) {
		st->reg[i] = adf4360_construct_reg(st, i);
	}
		      
	return spi_sync(st->spi, &st->sync_message);
}

static int adf4360_write_n_register(struct adf4360_state *st) {
	st->reg[ADF4360_N_REG] = adf4360_construct_reg(st, ADF4360_N_REG);
	return spi_sync(st->spi, &st->n_message);
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
static DEVICE_ATTR_WO(rcounter);

static ssize_t bcounter_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct adf4360_state *st = dev_get_drvdata(dev);
	
	if(kstrtou16(buf, 0, &st->bcounter)) {
		dev_err(dev, "Invalid bcounter value\n");
		return -EFAULT;
	}
	                             
	adf4360_write_n_register(st);
	
	return count;
}
static DEVICE_ATTR_WO(bcounter);

static ssize_t acounter_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct adf4360_state *st = dev_get_drvdata(dev);
	
	if(kstrtou8(buf, 0, &st->acounter)) {
		dev_err(dev, "Invalid bcounter value\n");
		return -EFAULT;
	}
	                             
	adf4360_write_n_register(st);
	
	return count;
}
static DEVICE_ATTR_WO(acounter);

static struct attribute *control_attrs[] = {
	&dev_attr_rcounter.attr,
	&dev_attr_bcounter.attr,
	&dev_attr_acounter.attr,
	NULL
};

static struct attribute_group control_group = {
	.name = "control",
	.attrs = control_attrs,
};

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
	
	spi_message_init(&st->sync_message);
	st->sync_xfers[1].delay_usecs = 10000;
	st->sync_xfers[0].tx_buf = &st->reg[ADF4360_R_REG];
	st->sync_xfers[1].tx_buf = &st->reg[ADF4360_CONTROL_REG];
	st->sync_xfers[2].tx_buf = &st->reg[ADF4360_N_REG];
	
	for(i = 0; i < 3; ++i) {
		st->sync_xfers[i].len = 3;
		st->sync_xfers[i].cs_change = 1;
		spi_message_add_tail(&st->sync_xfers[i], &st->sync_message);
	}
	
	spi_message_init(&st->n_message);
	st->n_xfer.tx_buf = &st->reg[ADF4360_N_REG];
	st->n_xfer.len = 3;
	st->n_xfer.cs_change = 1;
	spi_message_add_tail(&st->n_xfer, &st->n_message);
	
	// XXX Defaults here -- get from elsewhere probably.
	st->band_select = 2;
	st->anti_backlash = 0;
	st->lock_precision = 1;
	
	st->prescaler = 0;
	st->powerdown = 0;
	st->cp_current[0] = 2500;
	st->cp_current[1] = 2500;
	st->output_power = 3;
	st->muxout = ADF4360_MUXOUT_DIGITAL_LOCK_DETECT;
	st->core_power = 5;
	st->mute_til_lock = 0;
	st->cp_gain = 0;
	st->threestate = 0;
	st->pd_polarity_pos = 1;
	st->counter_reset = 0;
	
	st->bcounter = 88;
	st->acounter = 6;
	st->divide_by_2 = 0;
	st->prescaler_input = 0;
	st->cp_gain_perm = 0;
	
	ret = adf4360_sync_config(st);
	if(ret)
		dev_err(&spi->dev, "Couldn't sync configuration");
		
	if(sysfs_create_group(&spi->dev.kobj, &control_group))
		dev_err(&spi->dev, "Couldn't register control group");
			
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
	{"adf4360", 0},
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

