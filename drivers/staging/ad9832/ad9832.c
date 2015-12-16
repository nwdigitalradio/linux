/*
 * AD9832 SPI DDS driver
 *
 * Copyright 2011 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/spi/spi.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/of.h>

#include "ad9832.h"


/* static ssize_t ad9832_write(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t len)
{
	struct ad9832_state *st = dev_get_drvdata(dev);
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		goto error_ret;

	switch (1) {
	case AD9832_PHASE0H:
	case AD9832_PHASE1H:
	case AD9832_PHASE2H:
	case AD9832_PHASE3H:
		ret = ad9832_write_phase(st, this_attr->address, val);
		break;
	case AD9832_PINCTRL_EN:
		if (val)
			st->ctrl_ss &= ~AD9832_SELSRC;
		else
			st->ctrl_ss |= AD9832_SELSRC;
		st->data = cpu_to_be16((AD9832_CMD_SYNCSELSRC << CMD_SHIFT) |
					st->ctrl_ss);
		ret = spi_sync(st->spi, &st->msg);
		break;
	case AD9832_FREQ_SYM:
		if (val == 1) {
			st->ctrl_fp |= AD9832_FREQ;
		} else if (val == 0) {
			st->ctrl_fp &= ~AD9832_FREQ;
		} else {
			ret = -EINVAL;
			break;
		}
		st->data = cpu_to_be16((AD9832_CMD_FPSELECT << CMD_SHIFT) |
					st->ctrl_fp);
		ret = spi_sync(st->spi, &st->msg);
		break;
	case AD9832_PHASE_SYM:
		if (val > 3) {
			ret = -EINVAL;
			break;
		}

		st->ctrl_fp &= ~AD9832_PHASE(3);
		st->ctrl_fp |= AD9832_PHASE(val);

		st->data = cpu_to_be16((AD9832_CMD_FPSELECT << CMD_SHIFT) |
					st->ctrl_fp);
		ret = spi_sync(st->spi, &st->msg);
		break;
	case AD9832_OUTPUT_EN:
		if (val)
			st->ctrl_src &= ~(AD9832_RESET | AD9832_SLEEP |
					AD9832_CLR);
		else
			st->ctrl_src |= AD9832_RESET;

		st->data = cpu_to_be16((AD9832_CMD_SLEEPRESCLR << CMD_SHIFT) |
					st->ctrl_src);
		ret = spi_sync(st->spi, &st->msg);
		break;
	default:
		ret = -ENODEV;
	}

error_ret:
	return ret ? ret : len;
} */

static int ad9832_write_frequency(struct ad9832_state *st, u32 addr, u32 regval) {
	st->freq_data[0] = cpu_to_be16((AD9832_CMD_FRE8BITSW << CMD_SHIFT) |
					(addr << ADD_SHIFT) |
					((regval >> 24) & 0xFF));
	st->freq_data[1] = cpu_to_be16((AD9832_CMD_FRE16BITSW << CMD_SHIFT) |
					((addr - 1) << ADD_SHIFT) |
					((regval >> 16) & 0xFF));
	st->freq_data[2] = cpu_to_be16((AD9832_CMD_FRE8BITSW << CMD_SHIFT) |
					((addr - 2) << ADD_SHIFT) |
					((regval >> 8) & 0xFF));
	st->freq_data[3] = cpu_to_be16((AD9832_CMD_FRE16BITSW << CMD_SHIFT) |
					((addr - 3) << ADD_SHIFT) |
					((regval >> 0) & 0xFF));

	return spi_sync(st->spi, &st->freq_msg);
}

static ssize_t ad9832_frequencyword_store(struct device *dev, const char *buf, 
                                          size_t len, u32 addr) {
    int ret;
	unsigned long regval;
	struct ad9832_state *st = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 10, &regval);
	if (ret)
		return ret;	

	ret = ad9832_write_frequency(st, addr, regval);
	if(ret)
		return ret;
		
	return len;
}

#define FREQUENCY_ENTRY(index) 																    \
static ssize_t frequencyword##index##_store(struct device *dev, struct device_attribute *attr,	\
			    		                    const char *buf, size_t len) {					    \
																								\
	return ad9832_frequencyword_store(dev, buf, len, AD9832_FREQ##index##HM);					\
}															   									\
static DEVICE_ATTR_WO(frequencyword##index)

FREQUENCY_ENTRY(0);
FREQUENCY_ENTRY(1);

static int ad9832_write_phase(struct ad9832_state *st, u32 addr, u32 phase) {
	if (phase > BIT(AD9832_PHASE_BITS))
		return -EINVAL;

	st->phase_data[0] = cpu_to_be16((AD9832_CMD_PHA8BITSW << CMD_SHIFT) |
					(addr << ADD_SHIFT) |
					((phase >> 8) & 0xFF));
	st->phase_data[1] = cpu_to_be16((AD9832_CMD_PHA16BITSW << CMD_SHIFT) |
					((addr - 1) << ADD_SHIFT) |
					(phase & 0xFF));

	return spi_sync(st->spi, &st->phase_msg);
}

static ssize_t ad9832_phaseword_store(struct device *dev, const char *buf,
                                      size_t len, u32 addr) {
    int ret;
    unsigned long phase;
	struct ad9832_state *st = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 10, &phase);
	if (ret)
		return ret;

	ret = ad9832_write_phase(st, addr, phase);
	if(ret)
		return ret;
		
	return len;              
}


#define PHASE_ENTRY(index)  															\
static ssize_t phaseword##index##_store(struct device *dev,								\
                                        struct device_attribute *attr, const char *buf,	\
			    		                size_t len) {									\
	return ad9832_phaseword_store(dev, buf, len, AD9832_PHASE##index##H);				\
}																						\
static DEVICE_ATTR_WO(phaseword##index)

PHASE_ENTRY(0);
PHASE_ENTRY(1);
PHASE_ENTRY(2);
PHASE_ENTRY(3);

static struct attribute *control_attrs[] = {
	&dev_attr_frequencyword0.attr,
	&dev_attr_frequencyword1.attr,
 	&dev_attr_phaseword0.attr,
	&dev_attr_phaseword1.attr,
	&dev_attr_phaseword2.attr,
	&dev_attr_phaseword3.attr,
	NULL
};

static struct attribute_group control_group = {
	.name = "control",
	.attrs = control_attrs,
};



/* static IIO_DEV_ATTR_FREQ(0, 0, S_IWUSR, NULL, ad9832_write, AD9832_FREQ0HM);
static IIO_DEV_ATTR_FREQ(0, 1, S_IWUSR, NULL, ad9832_write, AD9832_FREQ1HM);
static IIO_DEV_ATTR_FREQSYMBOL(0, S_IWUSR, NULL, ad9832_write, AD9832_FREQ_SYM);
static IIO_CONST_ATTR_FREQ_SCALE(0, "1");

static IIO_DEV_ATTR_PHASE(0, 0, S_IWUSR, NULL, ad9832_write, AD9832_PHASE0H);
static IIO_DEV_ATTR_PHASE(0, 1, S_IWUSR, NULL, ad9832_write, AD9832_PHASE1H);
static IIO_DEV_ATTR_PHASE(0, 2, S_IWUSR, NULL, ad9832_write, AD9832_PHASE2H);
static IIO_DEV_ATTR_PHASE(0, 3, S_IWUSR, NULL, ad9832_write, AD9832_PHASE3H);
static IIO_DEV_ATTR_PHASESYMBOL(0, S_IWUSR, NULL,
				ad9832_write, AD9832_PHASE_SYM);
static IIO_CONST_ATTR_PHASE_SCALE(0, "0.0015339808");

static IIO_DEV_ATTR_PINCONTROL_EN(0, S_IWUSR, NULL,
				ad9832_write, AD9832_PINCTRL_EN);
static IIO_DEV_ATTR_OUT_ENABLE(0, S_IWUSR, NULL,
				ad9832_write, AD9832_OUTPUT_EN); */


static int ad9832_probe(struct spi_device *spi)
{
	struct ad9832_platform_data *pdata = spi->dev.platform_data;
	struct ad9832_state *st;
	struct regulator *reg;
	struct device_node *np = spi->dev.of_node;
	int ret;
	unsigned long mclk = 0;
	unsigned long freq[2] = { 10000000, 11000000 };
	unsigned short phase[4] = { 0, 0, 0, 0 };
	
	if(pdata) {
		mclk = pdata->mclk;
		freq[0] = pdata->freq0;
		freq[1] = pdata->freq1;
		phase[0] = pdata->phase0;
		phase[1] = pdata->phase1;
		phase[2] = pdata->phase2;
		phase[3] = pdata->phase3;
	} else if(np) {
		mclk = clk_get_rate(devm_clk_get(&spi->dev, "mclk"));
		if(mclk == 0) {
			dev_err(&spi->dev, "No master clock defined\n");
			return -ENODEV;
		}
		of_property_read_u32_array(np, "adi,power-up-frequencies", (u32 *) freq, 2);
		of_property_read_u16_array(np, "adi,power-up-phases", (u16 *) phase, 4);
	} else {
		dev_err(&spi->dev, "No platform data.\n");
		return -ENODEV;
	}
	
	reg = devm_regulator_get(&spi->dev, "vcc");
	if (!IS_ERR(reg)) {
		ret = regulator_enable(reg);
		if (ret)
			return ret;
	}
	
	*spi->dev.groups = &control_group;
		
	st = devm_kzalloc(&spi->dev, sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;
		
	spi_set_drvdata(spi, st);

	st->mclk = mclk;
	st->reg = reg;
	st->spi = spi;

	/* Setup default messages */

	st->xfer.tx_buf = &st->data;
	st->xfer.len = 2;

	spi_message_init(&st->msg);
	spi_message_add_tail(&st->xfer, &st->msg);

	st->freq_xfer[0].tx_buf = &st->freq_data[0];
	st->freq_xfer[0].len = 2;
	st->freq_xfer[0].cs_change = 1;
	st->freq_xfer[1].tx_buf = &st->freq_data[1];
	st->freq_xfer[1].len = 2;
	st->freq_xfer[1].cs_change = 1;
	st->freq_xfer[2].tx_buf = &st->freq_data[2];
	st->freq_xfer[2].len = 2;
	st->freq_xfer[2].cs_change = 1;
	st->freq_xfer[3].tx_buf = &st->freq_data[3];
	st->freq_xfer[3].len = 2;

	spi_message_init(&st->freq_msg);
	spi_message_add_tail(&st->freq_xfer[0], &st->freq_msg);
	spi_message_add_tail(&st->freq_xfer[1], &st->freq_msg);
	spi_message_add_tail(&st->freq_xfer[2], &st->freq_msg);
	spi_message_add_tail(&st->freq_xfer[3], &st->freq_msg);

	st->phase_xfer[0].tx_buf = &st->phase_data[0];
	st->phase_xfer[0].len = 2;
	st->phase_xfer[0].cs_change = 1;
	st->phase_xfer[1].tx_buf = &st->phase_data[1];
	st->phase_xfer[1].len = 2;

	spi_message_init(&st->phase_msg);
	spi_message_add_tail(&st->phase_xfer[0], &st->phase_msg);
	spi_message_add_tail(&st->phase_xfer[1], &st->phase_msg);

	st->ctrl_src = AD9832_SLEEP | AD9832_RESET | AD9832_CLR;
	st->data = cpu_to_be16((AD9832_CMD_SLEEPRESCLR << CMD_SHIFT) |
					st->ctrl_src);
	ret = spi_sync(st->spi, &st->msg);
	if (ret) {
		dev_err(&spi->dev, "device init failed\n");
		goto error_disable_reg;
	}

	ret = ad9832_write_frequency(st, AD9832_FREQ0HM, freq[0]);
	if (ret)
		goto error_disable_reg;

	ret = ad9832_write_frequency(st, AD9832_FREQ1HM, freq[1]);
	if (ret)
		goto error_disable_reg;

	ret = ad9832_write_phase(st, AD9832_PHASE0H, phase[0]);
	if (ret)
		goto error_disable_reg;

	ret = ad9832_write_phase(st, AD9832_PHASE1H, phase[1]);
	if (ret)
		goto error_disable_reg;

	ret = ad9832_write_phase(st, AD9832_PHASE2H, phase[2]);
	if (ret)
		goto error_disable_reg;

	ret = ad9832_write_phase(st, AD9832_PHASE3H, phase[3]);
	if (ret)
		goto error_disable_reg;

	return 0;

error_disable_reg:
	if (!IS_ERR(reg))
		regulator_disable(reg);

	return ret;
}

static int ad9832_remove(struct spi_device *spi)
{
	struct ad9832_state *st = spi_get_drvdata(spi);

	if (!IS_ERR(st->reg))
		regulator_disable(st->reg);

	return 0;
}

static const struct of_device_id ad9832_of_id[] = {
	{ .compatible = "adi,ad9832", },
	{ .compatible = "adi,ad9835", },
	{ /* senitel */ }
};
MODULE_DEVICE_TABLE(of, ad9832_of_id);

static const struct spi_device_id ad9832_id[] = {
	{"ad9832", 0},
	{"ad9835", 0},
	{}
};
MODULE_DEVICE_TABLE(spi, ad9832_id);

static struct spi_driver ad9832_driver = {
	.driver = {
		.name	= "ad9832",
		.owner	= THIS_MODULE,
	},
	.probe		= ad9832_probe,
	.remove		= ad9832_remove,
	.id_table	= ad9832_id,
};
module_spi_driver(ad9832_driver);

MODULE_AUTHOR("Michael Hennerich <hennerich@blackfin.uclinux.org>");
MODULE_DESCRIPTION("Analog Devices AD9832/AD9835 DDS");
MODULE_LICENSE("GPL v2");
