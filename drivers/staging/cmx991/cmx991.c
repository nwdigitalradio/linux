#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/regmap.h>

#include "cmx991.h"

#define DEVICE_ATTR_WOGRP(_name) DEVICE_ATTR(_name, S_IWUSR | S_IWGRP, NULL, 		\
											 _name##_store)
#define DEVICE_ATTR_RWGRP(_name) DEVICE_ATTR(_name, 								\
                                             S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP, \
                                             _name##_show, _name##_store)

static const struct regmap_config cmx991_general_regmap = {
	.name = "general",
	.reg_bits = 8,
	.val_bits = 8,
	
	.max_register = 0x06,
	.read_flag_mask = 0xE0,
	.write_flag_mask = 0x10,
};

enum cmx991_general_fields {
	F_BIAS_EN,
	F_IFH,
	F_CHAN_SEL,
	F_RX_MODE,
	F_VCO_NR,
	F_VCO_BUFF_EN,
	F_VCO_NR_EN,
	F_RX_MIX_PWR,
	F_RX_IQ_PWR,
	F_RX_AMP_PWR,
	F_RX_SLI_PWR,
	F_RX_LNA_PWR,
	F_RX_MIXLO_DIV,
	F_RX_VBIAS_EN,
	F_RX_IF_IN,
	F_RX_MIX_OUT,
	F_RX_IQ_FILTER_BW,
	F_RX_CAL_EN,
	F_RX_VGA,
	F_TX_MIXPWR_EN,
	F_TX_IQMODPWR_EN,
	F_TX_FREQRANGE,
	F_TX_IQOUT,
	F_TX_IF_BW,
	F_TX_HILO,
	F_TX_RFDIV,
	F_TX_IFDIV,
	F_TX_GAIN,
	F_MAX_FIELDS
};

static const struct reg_field cmx991_general_reg_fields[] = {
	[F_BIAS_EN]			= REG_FIELD(CMX991_GENERAL_CONTROL, 7, 7),
	[F_IFH]				= REG_FIELD(CMX991_GENERAL_CONTROL, 6, 6),
	[F_CHAN_SEL]		= REG_FIELD(CMX991_GENERAL_CONTROL, 5, 5),
	[F_RX_MODE]			= REG_FIELD(CMX991_GENERAL_CONTROL, 4, 4),
	[F_VCO_NR]			= REG_FIELD(CMX991_GENERAL_CONTROL, 2, 3),
	[F_VCO_BUFF_EN]		= REG_FIELD(CMX991_GENERAL_CONTROL, 1, 1),
	[F_VCO_NR_EN]		= REG_FIELD(CMX991_GENERAL_CONTROL, 0, 0),
	[F_RX_MIX_PWR]		= REG_FIELD(CMX991_RX_CONTROL, 7, 7),
	[F_RX_IQ_PWR]		= REG_FIELD(CMX991_RX_CONTROL, 6, 6),
	[F_RX_AMP_PWR]		= REG_FIELD(CMX991_RX_CONTROL, 5, 5),
	[F_RX_SLI_PWR]		= REG_FIELD(CMX991_RX_CONTROL, 4, 4),
	[F_RX_LNA_PWR]		= REG_FIELD(CMX991_RX_CONTROL, 3, 3),
	[F_RX_MIXLO_DIV]	= REG_FIELD(CMX991_RX_CONTROL, 1, 2),
	[F_RX_VBIAS_EN]		= REG_FIELD(CMX991_RX_CONTROL, 0, 0),
	[F_RX_IF_IN]		= REG_FIELD(CMX991_RX_MODE, 7, 7),
	[F_RX_MIX_OUT]		= REG_FIELD(CMX991_RX_MODE, 6, 6),
	[F_RX_IQ_FILTER_BW]	= REG_FIELD(CMX991_RX_MODE, 5, 5),
	[F_RX_CAL_EN]		= REG_FIELD(CMX991_RX_MODE, 4, 4),
	[F_RX_VGA]			= REG_FIELD(CMX991_RX_MODE, 0, 3),
	[F_TX_MIXPWR_EN]	= REG_FIELD(CMX991_TX_MODE, 6, 6),
	[F_TX_IQMODPWR_EN]	= REG_FIELD(CMX991_TX_MODE, 4, 4),
	[F_TX_FREQRANGE]	= REG_FIELD(CMX991_TX_MODE, 1, 1),
	[F_TX_IQOUT]		= REG_FIELD(CMX991_TX_MODE, 0, 0),
	[F_TX_IF_BW]		= REG_FIELD(CMX991_TX_MODE, 4, 5),
	[F_TX_HILO]			= REG_FIELD(CMX991_TX_MODE, 2, 2),
	[F_TX_RFDIV]		= REG_FIELD(CMX991_TX_MODE, 1, 1),
	[F_TX_IFDIV]		= REG_FIELD(CMX991_TX_MODE, 0, 0),
	[F_TX_GAIN]			= REG_FIELD(CMX991_TX_GAIN, 6, 7),
	
};

static const struct regmap_config cmx991_pll_regmap = {
	.name = "pll",
	.reg_bits = 8,
	.val_bits = 8,
	
	.max_register = 0x03,
	.read_flag_mask = 0xD0,
	.write_flag_mask = 0x20,
};

enum cmx991_pll_fields {
	F_PLL_M_LSB,
	F_PLL_M_MSB,
	F_PLL_CP_EN,
	F_PLL_LD_SYNTH,
	F_PLL_EN,
	F_PLL_N_LSB,
	F_PLL_N_MSB,
	F_PLL_MAX_FIELDS
};

static const struct reg_field cmx991_pll_reg_fields[] = {
	[F_PLL_M_LSB]		= REG_FIELD(CMX991_PLL_M_LSB, 0, 7),
	[F_PLL_M_MSB]		= REG_FIELD(CMX991_PLL_M_MSB, 0, 4),
	[F_PLL_CP_EN]		= REG_FIELD(CMX991_PLL_M_MSB, 5, 5),
	[F_PLL_LD_SYNTH]	= REG_FIELD(CMX991_PLL_M_MSB, 6, 6),
	[F_PLL_EN]			= REG_FIELD(CMX991_PLL_M_MSB, 7, 7),
	[F_PLL_N_LSB]		= REG_FIELD(CMX991_PLL_N_LSB, 0, 7),
	[F_PLL_N_MSB]		= REG_FIELD(CMX991_PLL_N_MSB, 0, 6),
};

static const __be32 reset_buf[] ____cacheline_aligned = { 0x10 };

struct cmx991_state {
	struct spi_device *spi;
	struct regmap *general_regmap;
	struct regmap *pll_regmap;
	
	struct regmap_field *general_fields[F_MAX_FIELDS];
	struct regmap_field *pll_fields[F_PLL_MAX_FIELDS];
	
	bool power;
	bool rx_power;
	bool tx_power;
	bool pll_enable;
};

static void cmx991_set_n_divider(struct cmx991_state *st, u16 divider)
{
	regmap_write(st->pll_regmap, CMX991_PLL_N_LSB, divider);
	regmap_write(st->pll_regmap, CMX991_PLL_N_MSB, (divider >> 8) & 0x7F);
}

static int cmx991_get_n_divider(struct cmx991_state *st, unsigned int *val)
{
	unsigned int lsb, msb;
	int ret;
	
	ret = regmap_read(st->pll_regmap, CMX991_PLL_N_LSB, &lsb);
	if(ret)
		return ret;
	ret = regmap_read(st->pll_regmap, CMX991_PLL_N_MSB, &msb);
	if(ret)
		return ret;
	
	*val = ((msb & 0x7F) << 8) | lsb;
	
	return 0;
}

static void cmx991_set_m_divider(struct cmx991_state *st, u16 divider)
{
	regmap_write(st->pll_regmap, CMX991_PLL_M_LSB, divider);
	regmap_update_bits(st->pll_regmap, CMX991_PLL_M_MSB, 0x5F, 
	                   (divider >> 8) & 0x1F);
}

static int cmx991_get_m_divider(struct cmx991_state *st, unsigned int *val)
{
	unsigned int lsb, msb;
	int ret;
	
	ret = regmap_read(st->pll_regmap, CMX991_PLL_M_LSB, &lsb);
	if(ret)
		return ret;
	ret = regmap_read(st->pll_regmap, CMX991_PLL_M_MSB, &msb);
	if(ret)
		return ret;
	
	*val = ((msb & 0x1F) << 8) | lsb;
	
	return 0;
}


static void cmx991_enable_pll(struct cmx991_state *st, bool enable)
{
	st->pll_enable = enable;
	
	regmap_update_bits(st->pll_regmap, CMX991_PLL_M_MSB, 0xE0, 
	                   st->pll_enable ? 0xA0 : 0x00);

}

static void cmx991_enable_tx(struct cmx991_state *st, bool power)
{
	st->tx_power = power;
	
	if(!st->pll_enable && st->tx_power)
		cmx991_enable_pll(st, true);
		
	regmap_update_bits(st->general_regmap, CMX991_TX_CONTROL, 0x50,
	                   st->tx_power ? 0x50 : 0x00);
	                   
	//  Turn off the LNA bit when we're transmitting
	regmap_update_bits(st->general_regmap, CMX991_RX_CONTROL, 0x08,
	                   st->tx_power ? 0x00 : 0x08);
}

static void cmx991_set_power(struct cmx991_state *st, bool power)
{
	st->power = power;
		
	//  Power up the chip
	regmap_update_bits(st->general_regmap, CMX991_GENERAL_CONTROL, 0x83,
	                   st->power ? 0x83 : 0x00);
	
	//  Power up and enable the PLL                   
	regmap_update_bits(st->pll_regmap, CMX991_PLL_M_MSB, 0xE0, 
	                   st->power ? 0xA0 : 0x00);

	//  Turn on the receiver
	regmap_update_bits(st->general_regmap, CMX991_RX_CONTROL, 0xF9,
	                   st->power ? 0xF9 : 0x00);
	                   
	//  Make sure the transmitter is off
	cmx991_enable_tx(st, false);
}


static inline int cmx991_reset(struct cmx991_state *st)
{
	return spi_write(st->spi, reset_buf, 1);
}

static ssize_t power_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	u8 enable;
	
	if(kstrtou8(buf, 0, &enable)) {
		dev_err(dev, "Invalid power value\n");
		return -EFAULT;
	}
	
	if(enable == 0) {
		cmx991_set_power(st, false);
	} else if(enable == 1) {
		cmx991_set_power(st, true);
	} else {
		dev_err(dev, "Invalid value for power state");
		return -EFAULT;
	}
	                             	
	return count;
}
static ssize_t power_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cmx991_state *st = dev_get_drvdata(dev);

	if(st->power)
		*buf = '1';
	else
		*buf = '0';
	
	return 1;
}
static DEVICE_ATTR_RWGRP(power);

static ssize_t tx_power_store(struct device *dev, struct device_attribute *attr,
                              const char *buf, size_t count)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	u8 enable;
	
	if(kstrtou8(buf, 0, &enable)) {
		dev_err(dev, "Invalid power value\n");
		return -EFAULT;
	}
	
	if(enable == 0) {
		cmx991_enable_tx(st, false);
	} else if(enable == 1) {
		cmx991_enable_tx(st, true);
	} else {
		dev_err(dev, "Invalid value for rx power state");
		return -EFAULT;
	}
	
	//  Notify people that our TX state has changed
	//  XXX This should probably be modified so that the strings are
	//  XXX derived from device_attribute and friends.
	sysfs_notify(&dev->kobj, "control", attr->attr.name);
	                             	
	return count;
}
static ssize_t tx_power_show(struct device *dev, struct device_attribute *attr,
                             char *buf)
{
	struct cmx991_state *st = dev_get_drvdata(dev);

	if(st->tx_power)
		*buf = '1';
	else
		*buf = '0';
	
	return 1;
}
static DEVICE_ATTR_RWGRP(tx_power);


static ssize_t pll_m_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count) {
	struct cmx991_state *st = dev_get_drvdata(dev);
	u16 m;
	
	if(kstrtou16(buf, 0, &m)) {
		dev_err(dev, "Invalid pll_m value\n");
		return -EFAULT;
	}
	
	if(m < 1 || m > 0x1FFF) {
		dev_err(dev, "Invalid pll_m value\n");
		return -EFAULT;
	}
	
	cmx991_set_m_divider(st, m);

	return count;
}
static ssize_t pll_m_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	unsigned int val;
	int ret;
	
	ret = cmx991_get_m_divider(st, &val);
	if(ret) {
		dev_err(dev, "Error retrieving M divider: %d", ret);
		return -EFAULT;
	}
	
	return scnprintf(buf, PAGE_SIZE, "%d", val);
}
static DEVICE_ATTR_RWGRP(pll_m);

static ssize_t pll_n_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count) {
	struct cmx991_state *st = dev_get_drvdata(dev);
	u16 n;
	
	if(kstrtou16(buf, 0, &n)) {
		dev_err(dev, "Invalid pll_m value\n");
		return -EFAULT;
	}
	
	cmx991_set_n_divider(st, n);

	return count;
}
static ssize_t pll_n_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	unsigned int val;
	int ret;
	
	ret = cmx991_get_n_divider(st, &val);
	if(ret) {
		dev_err(dev, "Error retrieving N divider: %d", ret);
		return -EFAULT;
	}
	
	return scnprintf(buf, PAGE_SIZE, "%d", val);
}
static DEVICE_ATTR_RWGRP(pll_n);

static ssize_t pll_locked_show(struct device *dev, struct device_attribute *attr,
                               char *buf)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	unsigned int val;
	
	regmap_field_read(st->pll_fields[F_PLL_LD_SYNTH], &val);
	return scnprintf(buf, PAGE_SIZE, "%d", val);
}
static DEVICE_ATTR_RO(pll_locked);

static ssize_t vga_gain_store(struct device *dev, struct device_attribute *attr, 
                              const char *buf, size_t count)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	s8 val;
	int ret;
	
	//  This should probably be giving values in dB and parsing them.
	if(kstrtos8(buf, 0, &val)) {
		dev_err(dev, "Invalid VGA gain value\n");
		return -EFAULT;
	}
	
	if(val > 0 || val < -48 || val % 6 != 0) {
		dev_err(dev, 
		        "Invalid VGA gain value.  Valid values between 0 and -48 in steps of 6\n");
		return -EFAULT;
	}
	
	ret = regmap_field_write(st->general_fields[F_RX_VGA], -(val / 6));
	if(ret) {
		dev_err(dev, "Couldn't write F_RX_VGA regmap field\n");
		return ret;
	}
	
	return count;
}

static ssize_t vga_gain_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	unsigned int val;
	
	regmap_field_read(st->general_fields[F_RX_VGA], &val);
	return scnprintf(buf, PAGE_SIZE, "%d", -(val * 6));
}
DEVICE_ATTR_RWGRP(vga_gain);

static ssize_t tx_gain_store(struct device *dev, struct device_attribute *attr,
                             const char *buf, size_t count)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	s8 val;
	int ret;
	int writeval;
	
	if(kstrtos8(buf, 0, &val)) {
		dev_err(dev, "Invalid TX Gain");
		return -EFAULT;
	}
	
	switch(val) {
		case -6:
			writeval = 1;
			break;
		case 0:
			writeval = 0;
			break;
		case 6:
			writeval = 2;
			break;
		default:
			dev_err(dev, "Invalid TX Gain");
			return -EFAULT;
			break;
	}
	
	ret = regmap_field_write(st->general_fields[F_TX_GAIN], writeval);
	if(ret) {
		dev_err(dev, "Couldn't write F_RX_GAIN regmap field");
		return ret;
	}
	
	return count;
}

static ssize_t tx_gain_show(struct device *dev, struct device_attribute *attr,
                            char *buf)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	unsigned int readval;
	s8 val;
	
	regmap_field_read(st->general_fields[F_TX_GAIN], &readval);
	switch(readval) {
		case 0:
			val = 0;
			break;
		case 1:
			val = -6;
			break;
		case 2:
			val = 6;
			break;
		default:
			dev_err(dev, "Got weird value %d in F_RX_GAIN register", readval);
			return -EFAULT;
	}
	
	return scnprintf(buf, PAGE_SIZE, "%d", val);	
}
DEVICE_ATTR_RWGRP(tx_gain);

static ssize_t mixout_store(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	u8 val;
	int ret;
	
	if(kstrtou8(buf, 0, &val)) {
		dev_err(dev, "Invalid mixer out routing\n");
		return -EFAULT;
	}
	
	if(val < 1 || val > 2) {
		dev_err(dev, "Invalid mixe rout routing\n");
		return -EFAULT;
	}
	
	ret = regmap_field_write(st->general_fields[F_RX_MIX_OUT], val - 1);
	if(ret) {
		dev_err(dev, "Couldn't write F_RX_MIX_OUT regmap field\n");
		return ret;
	}
	
	return count;
}

static ssize_t mixout_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	unsigned int val;
	
	regmap_field_read(st->general_fields[F_RX_MIX_OUT], &val);
	return scnprintf(buf, PAGE_SIZE, "%d", val + 1);
}
DEVICE_ATTR_RWGRP(mixout);

static ssize_t ifin_store(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	u8 val;
	int ret;
	
	if(kstrtou8(buf, 0, &val)) {
		dev_err(dev, "Invalid IF in routing\n");
		return -EFAULT;
	}
	
	if(val < 1 || val > 2) {
		dev_err(dev, "Invalid IF in routing\n");
		return -EFAULT;
	}
	
	ret = regmap_field_write(st->general_fields[F_RX_IF_IN], val - 1);
	if(ret) {
		dev_err(dev, "Couldn't write F_RX_IF_IN regmap field\n");
		return ret;
	}
	
	return count;
}

static ssize_t ifin_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	unsigned int val;
	
	regmap_field_read(st->general_fields[F_RX_IF_IN], &val);
	return scnprintf(buf, PAGE_SIZE, "%d", val + 1);
}
DEVICE_ATTR_RWGRP(ifin);

static ssize_t iqbandwidth_store(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	u16 val;
	int ret;
	
	if(kstrtou16(buf, 0, &val)) {
		dev_err(dev, "Invalid IQ Bandwidth\n");
		return -EFAULT;
	}
	
	if(val != 1000 || val != 100) {
		dev_err(dev, "Invalid IQ Bandwidth\n");
		return -EFAULT;
	}
	
	ret = regmap_field_write(st->general_fields[F_RX_IQ_FILTER_BW], val == 1000 ? 1 : 0);
	if(ret) {
		dev_err(dev, "Couldn't write F_RX_IQ_FILTER_BW regmap field\n");
		return ret;
	}
	
	return count;
}

static ssize_t iqbandwidth_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	unsigned int val;
	
	regmap_field_read(st->general_fields[F_RX_IQ_FILTER_BW], &val);
	return scnprintf(buf, PAGE_SIZE, "%d", val == 1 ? 1000 : 100);
}
DEVICE_ATTR_RWGRP(iqbandwidth);

static ssize_t calibrate_store(struct device *dev, struct device_attribute *attr,
                               const char *buf, size_t count)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	u8 val;
	int ret;
	
	if(kstrtou8(buf, 0, &val)) {
		dev_err(dev, "Invalid calibration value\n");
		return -EFAULT;
	}
	
	if(val > 1) {
		dev_err(dev, "Invalid calibration value\n");
		return -EFAULT;
	}
	
	ret = regmap_field_write(st->general_fields[F_RX_CAL_EN], val);
	if(ret) {
		dev_err(dev, "Couldn't write F_RX_CAL_EN regmap field\n");
		return ret;
	}
	
	return count;
}

static ssize_t calibrate_show(struct device *dev, struct device_attribute *attr,
                              char *buf)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	unsigned int val;
	
	regmap_field_read(st->general_fields[F_RX_CAL_EN], &val);
	return scnprintf(buf, PAGE_SIZE, "%d", val);
}
DEVICE_ATTR_RWGRP(calibrate);

static ssize_t tx_if_bandwidth_store(struct device *dev, struct device_attribute *attr,
                                     const char *buf, size_t count)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	u8 val;
	int ret;
	int writeval;
	
	if(kstrtou8(buf, 0, &val)) {
		dev_err(dev, "Invalid TX IF Bandwidth");
		return -EFAULT;
	}
	
	switch(val) {
		case 45:
			writeval = 0;
			break;
		case 60:
			writeval = 1;
			break;
		case 90:
			writeval = 2;
			break;
		case 120:
			writeval = 3;
			break;
		default:
			dev_err(dev, "Invalid TX IF Bandwidth");
			return -EFAULT;
			break;
	}
	
	ret = regmap_field_write(st->general_fields[F_TX_IF_BW], writeval);
	if(ret) {
		dev_err(dev, "Couldn't write F_TX_IF_BW\n");
		return ret;
	}
	
	return count;
}

static ssize_t tx_if_bandwidth_show(struct device *dev, struct device_attribute *attr,
                                    char *buf)
{
	struct cmx991_state *st = dev_get_drvdata(dev);
	unsigned int readval;
	u8 val;
	
	regmap_field_read(st->general_fields[F_TX_IF_BW], &readval);
	switch(readval) {
		case 0:
			val = 45;
			break;
		case 1:
			val = 60;
			break;
		case 2:
			val = 90;
			break;
		case 3:
			val = 120;
			break;
		default:
			dev_err(dev, "Got weird value %d in F_TX_IF_BW register", readval);
			return -EFAULT;
	}
	
	return scnprintf(buf, PAGE_SIZE, "%d", val);	
}

DEVICE_ATTR_RWGRP(tx_if_bandwidth);

static struct attribute *control_attrs[] = {
	&dev_attr_power.attr,
	&dev_attr_tx_power.attr,
	&dev_attr_pll_m.attr,
	&dev_attr_pll_n.attr,
	&dev_attr_pll_locked.attr,
	&dev_attr_vga_gain.attr,
	&dev_attr_mixout.attr,
	&dev_attr_ifin.attr,
	&dev_attr_iqbandwidth.attr,
	&dev_attr_calibrate.attr,
	&dev_attr_tx_gain.attr,
	&dev_attr_tx_if_bandwidth.attr,
	NULL
};

static const struct attribute_group control_group = {
	.name = "control",
	.attrs = control_attrs,
};

static const struct attribute_group *device_groups[] = {
	&control_group,
	NULL
};

static int cmx991_probe(struct spi_device *spi) {
	struct cmx991_state *st;
	int ret;
	int i;
	
	// XXX need to deal with pdata here
	
	st = devm_kzalloc(&spi->dev, sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;
			
	spi_set_drvdata(spi, st);
	
	st->spi = spi;
	
	st->general_regmap = devm_regmap_init_spi(spi, &cmx991_general_regmap);
	if(IS_ERR(st->general_regmap))
		return PTR_ERR(st->general_regmap);
		
	st->pll_regmap = devm_regmap_init_spi(spi, &cmx991_pll_regmap);
	if(IS_ERR(st->pll_regmap))
		return PTR_ERR(st->pll_regmap);
		
	for(i = 0; i < F_MAX_FIELDS; ++i) {
		st->general_fields[i] = devm_regmap_field_alloc(&spi->dev, st->general_regmap,
		                                                cmx991_general_reg_fields[i]);
		if(IS_ERR(st->general_fields[i])) {
			dev_err(&spi->dev, "Failed to allocate general regmap field = %d\n", i);
			return PTR_ERR(st->general_fields[i]);
		}
	}
		
	for(i = 0; i < F_PLL_MAX_FIELDS; ++i) {
		st->pll_fields[i] = devm_regmap_field_alloc(&spi->dev, st->pll_regmap,
		                                                cmx991_pll_reg_fields[i]);
		if(IS_ERR(st->pll_fields[i])) {
			dev_err(&spi->dev, "Failed to allocate pll regmap field = %d\n", i);
			return PTR_ERR(st->pll_fields[i]);
		}
	}
			
	if(sysfs_create_groups(&spi->dev.kobj, device_groups))
		dev_err(&spi->dev, "Couldn't add device groups");
		
	cmx991_reset(st);
		
	// XXX These should come out of platform structure
	regmap_field_write(st->general_fields[F_VCO_NR], 0x01);

	//  RX Parameters
	//XXX These should be in defaults
	regmap_field_write(st->general_fields[F_RX_MIXLO_DIV], 0);
	regmap_field_write(st->general_fields[F_RX_VGA], 5);
	regmap_field_write(st->general_fields[F_RX_IF_IN], 0);
	regmap_field_write(st->general_fields[F_RX_MIX_OUT], 0);
	regmap_field_write(st->general_fields[F_RX_IQ_FILTER_BW], 0);
	regmap_field_write(st->general_fields[F_RX_CAL_EN], 0);
	
	//  TX Parameters
	//  XXX These should be in defaults
	regmap_field_write(st->general_fields[F_TX_IF_BW], 0);
	regmap_field_write(st->general_fields[F_TX_HILO], 1);
	regmap_field_write(st->general_fields[F_TX_RFDIV], 0);
	regmap_field_write(st->general_fields[F_TX_IFDIV], 0);
	regmap_field_write(st->general_fields[F_TX_GAIN], 0);
	
	//  PLL Parameters
	//  XXX These should be in defaults
	cmx991_set_m_divider(st, 0x00FA);
	cmx991_set_n_divider(st, 0x0AF0);
	
	cmx991_set_power(st, false);

	return 0;
}

static int cmx991_remove(struct spi_device *spi) {
	//struct cmx991_state *st = spi_get_drvdata(spi);
	
	return 0;
}

static const struct of_device_id cmx991_of_id[] = {
	{ .compatible = "cml,cmx991", },
	{ /* senitel */ }
};
MODULE_DEVICE_TABLE(of, cmx991_of_id);

static const struct spi_device_id cmx991_id[] = {
	{"cmx991", 0},
	{}
};
MODULE_DEVICE_TABLE(spi, cmx991_id);

static struct spi_driver cmx991_driver = {
	.driver = {
		.name 	= "cmx991",
		.owner	= THIS_MODULE,
		.of_match_table = cmx991_of_id,
	},
	.probe		= cmx991_probe,
	.remove		= cmx991_remove,
	.id_table	= cmx991_id,
};
module_spi_driver(cmx991_driver);

MODULE_DESCRIPTION("CML Microcircuits CMX991 Driver");
MODULE_AUTHOR("Jeremy McDermond <nh6z@nh6z.net>");
MODULE_LICENSE("GPL");
