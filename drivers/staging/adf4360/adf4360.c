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
	
	//  Set the control register settings
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
	st->reg[ADF4360_CONTROL_REG] = ADF4360_SET_REGISTER(reg);
	
	//  Set the N register settings
	reg = ADF4360_N_REG |
	      ADF4360_REG2_B_COUNTER(st->bcounter) |
	      ADF4360_REG2_A_COUNTER(st->acounter);
	if(st->divide_by_2)
		reg |= ADF4360_REG2_DIVIDE_BY_TWO;
	if(st->prescaler_input)
		reg |= ADF4360_REG2_PRESCALER_INPUT;
	if(st->cp_gain_perm)
		reg |= ADF4360_REG2_CP_GAIN_PERM;
	st->reg[ADF4360_N_REG] = ADF4360_SET_REGISTER(reg);
	      
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

static ssize_t bcounter_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct adf4360_state *st = dev_get_drvdata(dev);
	
	if(kstrtou16(buf, 0, &st->bcounter)) {
		dev_err(dev, "Invalid bcounter value\n");
		return -EFAULT;
	}
	                             
	adf4360_sync_config(st);
	
	return count;
}
static DEVICE_ATTR(bcounter, S_IWUSR, NULL, bcounter_store);

static ssize_t acounter_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
	struct adf4360_state *st = dev_get_drvdata(dev);
	
	if(kstrtou8(buf, 0, &st->acounter)) {
		dev_err(dev, "Invalid bcounter value\n");
		return -EFAULT;
	}
	                             
	adf4360_sync_config(st);
	
	return count;
}
static DEVICE_ATTR(acounter, S_IWUSR, NULL, acounter_store);

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
	st->xfer[0].tx_buf = &st->reg[1];
	st->xfer[1].tx_buf = &st->reg[0];
	st->xfer[2].tx_buf = &st->reg[2];
	
	for(i = 0; i < 3; ++i) {
		st->xfer[i].len = 3;
		st->xfer[i].cs_change = 1;
		spi_message_add_tail(&st->xfer[i], &st->message);
	}
	
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

	ret = device_create_file(&spi->dev, &dev_attr_rcounter);
	if(ret > 0)
		dev_err(&spi->dev, "Couldn't create rcounter device file"); 
	ret = device_create_file(&spi->dev, &dev_attr_bcounter);
	if(ret > 0)
		dev_err(&spi->dev, "Couldn't create bcounter device file"); 
	ret = device_create_file(&spi->dev, &dev_attr_acounter);
	if(ret > 0)
		dev_err(&spi->dev, "Couldn't create acounter device file"); 
		
	return adf4360_sync_config(st);
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

