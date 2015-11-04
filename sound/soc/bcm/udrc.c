/*
 * ASoC Driver for the Universal Digital Radio Controller
 *
 * Author:	Jeremy McDermond <nh6z@nh6z.net>
 *		Copyright (c) 2015 Northwest Digtial Radio
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>

static int snd_rpi_udrc_init(struct snd_soc_pcm_runtime *rtd) {
    return 0;
}

static int snd_rpi_udrc_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params) {

    struct snd_soc_pcm_runtime *rtd = substream->private_data;
    struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
    struct snd_soc_dai *codec_dai = rtd->codec_dai;
    int err;
    unsigned int sample_bits = snd_pcm_format_physical_width(params_format(params)); //  XXX Is this what we really want?

    err = snd_soc_dai_set_sysclk(codec_dai, 0, 25000000, SND_SOC_CLOCK_IN);
    if(err < 0) {
        dev_err(rtd->card->dev, "codec_dai clock not set\n");
        return err;
    }

    //  XXX is this what we really want?

    printk(KERN_ERR "In UDRC hw_params, sample bits is %d\n", sample_bits);
    return snd_soc_dai_set_bclk_ratio(cpu_dai, sample_bits * 2);
}

static struct snd_soc_ops snd_rpi_udrc_ops = {
    .hw_params = snd_rpi_udrc_hw_params,
};

static struct snd_soc_dai_link snd_rpi_udrc_dai[] = {
{
    .name           = "Universal Digital Radio Controller",
    .stream_name    = "Universal Digital Radio Controller",
    .cpu_dai_name   = "bcm2708-i2s.0",
    .codec_dai_name = "tlv320aic32x4-hifi",
    .platform_name  = "bcm2708-i2s.0",
    .codec_name     = "tlv320aic32x4.1-0018",
    .dai_fmt        = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM,
    .ops            = &snd_rpi_udrc_ops,
    .init           = snd_rpi_udrc_init,
},
};

static struct snd_soc_card snd_rpi_udrc = {
    .name = "snd_rpi_udrc",
    .dai_link = snd_rpi_udrc_dai,
    .num_links = ARRAY_SIZE(snd_rpi_udrc_dai),
};

static int snd_rpi_udrc_probe(struct platform_device *pdev) {
    int ret = 0;
    snd_rpi_udrc.dev = &pdev->dev;

    if(pdev->dev.of_node) {
        struct device_node *i2s_node;
        struct snd_soc_dai_link *dai = &snd_rpi_udrc_dai[0];
        i2s_node = of_parse_phandle(pdev->dev.of_node, "i2s-controller", 0);

        if(i2s_node) {
            dai->cpu_dai_name = NULL;
            dai->cpu_of_node = i2s_node;
            dai->platform_name = NULL;
            dai->platform_of_node = i2s_node;
        }
    }

    printk(KERN_ERR "Attempting to register card\n");

    ret = snd_soc_register_card(&snd_rpi_udrc);
    if(ret)
        dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);

    return ret;
}

static int snd_rpi_udrc_remove(struct platform_device *pdev) {
    return snd_soc_unregister_card(&snd_rpi_udrc);
}

static const struct of_device_id snd_rpi_udrc_of_match[] = {
    { .compatible = "nwdr,udrc", },
    {},
};
MODULE_DEVICE_TABLE(of, snd_rpi_udrc_of_match);

static struct platform_driver snd_rpi_udrc_driver = {
    .driver = {
        .name           = "snd-udrc",
        .owner          = THIS_MODULE,
        .of_match_table = snd_rpi_udrc_of_match,
    },
    .probe  = snd_rpi_udrc_probe,
    .remove = snd_rpi_udrc_remove,
};

module_platform_driver(snd_rpi_udrc_driver);

MODULE_AUTHOR("Jeremy McDermond <nh6z@nh6z.net>");
MODULE_DESCRIPTION("ASoC Driver for the Universal Digital Radio Controller");
MODULE_LICENSE("GPL v2");
