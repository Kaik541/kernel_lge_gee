/* Copyright (c) 2012, LGE Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/regulator/consumer.h>
#include "devices.h"

#include "board-8064.h"
#ifdef CONFIG_SND_SOC_TPA2028D
#include <sound/tpa2028d.h>
#endif

#include "board-j1.h"
#ifdef CONFIG_SWITCH_FSA8008
#include <linux/platform_data/hds_fsa8008.h>
#include <mach/board_lge.h>
#endif
#include "../../../../sound/soc/codecs/wcd9310.h"


#define TPA2028D_ADDRESS (0xB0>>1)
#define MSM_AMP_EN (PM8921_GPIO_PM_TO_SYS(19))
#define AGC_COMPRESIION_RATE        0
#define AGC_OUTPUT_LIMITER_DISABLE  1
#if defined(CONFIG_MACH_APQ8064_J1KD) || defined(CONFIG_MACH_APQ8064_J1D)
#define AGC_FIXED_GAIN              14
#else
#define AGC_FIXED_GAIN              12
#endif

#define GPIO_EAR_MIC_BIAS_EN        PM8921_GPIO_PM_TO_SYS(20)
#define GPIO_EAR_SENSE_N            82

#if defined(CONFIG_MACH_APQ8064_GKKT)||defined(CONFIG_MACH_APQ8064_GKSK)||defined(CONFIG_MACH_APQ8064_GKU)||defined(CONFIG_MACH_APQ8064_GKATT)
#define GPIO_EAR_SENSE_N_REV11      38
#else
#define GPIO_EAR_SENSE_N_REV11      82
#endif
#define GPIO_EAR_MIC_EN             PM8921_GPIO_PM_TO_SYS(31)
#define GPIO_EARPOL_DETECT          PM8921_GPIO_PM_TO_SYS(32)
#define GPIO_EAR_KEY_INT            83

#define I2C_SURF 1
#define I2C_FFA  (1 << 1)
#define I2C_RUMI (1 << 2)
#define I2C_SIM  (1 << 3)
#define I2C_LIQUID (1 << 4)
/* LGE_UPDATE_S. 02242012. jihyun.lee@lge.com
   Add mach_mask for I2C */
#define I2C_J1V (1 << 5)
/* LGE_UPDATE_E */

struct i2c_registry {
	u8                     machs;
	int                    bus;
	struct i2c_board_info *info;
	int                    len;
};

#ifdef CONFIG_SND_SOC_TPA2028D
int amp_enable(int on_state)
{
	int err = 0;
	static int init_status = 0;
	struct pm_gpio param = {
		.direction      = PM_GPIO_DIR_OUT,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value   = 1,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel	= PM_GPIO_VIN_S4,
		.out_strength   = PM_GPIO_STRENGTH_MED,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	if (init_status == 0) {
		err = gpio_request(MSM_AMP_EN, "AMP_EN");
		if (err)
			pr_err("%s: Error requesting GPIO %d\n",
					__func__, MSM_AMP_EN);

		err = pm8xxx_gpio_config(MSM_AMP_EN, &param);
		if (err)
			pr_err("%s: Failed to configure gpio %d\n",
					__func__, MSM_AMP_EN);
		else
			init_status++;
	}

	switch (on_state) {
	case 0:
		err = gpio_direction_output(MSM_AMP_EN, 0);
		printk(KERN_INFO "%s: AMP_EN is set to 0\n", __func__);
		break;
	case 1:
		err = gpio_direction_output(MSM_AMP_EN, 1);
		printk(KERN_INFO "%s: AMP_EN is set to 1\n", __func__);
		break;
	case 2:
#ifdef CONFIG_MACH_MSM8960_D1L
			return 0;
#else
		printk(KERN_INFO "%s: amp enable bypass(%d)\n", __func__, on_state);
		err = 0;
#endif
		break;

	default:
		pr_err("amp enable fail\n");
		err = 1;
		break;
	}
	return err;
}

static struct audio_amp_platform_data amp_platform_data =  {
	.enable = amp_enable,
	.agc_compression_rate = AGC_COMPRESIION_RATE,
	.agc_output_limiter_disable = AGC_OUTPUT_LIMITER_DISABLE,
	.agc_fixed_gain = AGC_FIXED_GAIN,
};
#endif

static struct i2c_board_info msm_i2c_audiosubsystem_info[] = {
#ifdef CONFIG_SND_SOC_TPA2028D
	{
		I2C_BOARD_INFO("tpa2028d_amp", TPA2028D_ADDRESS),
		.platform_data = &amp_platform_data,
	}
#endif
};

static struct i2c_registry msm_i2c_audiosubsystem __initdata = {
	/* Add the I2C driver for Audio Amp, ehgrace.kim@lge.cim, 06/13/2011 */
		 I2C_SURF | I2C_FFA | I2C_RUMI | I2C_SIM | I2C_LIQUID | I2C_J1V,
		APQ_8064_GSBI1_QUP_I2C_BUS_ID,
		msm_i2c_audiosubsystem_info,
		ARRAY_SIZE(msm_i2c_audiosubsystem_info),
};


static void __init lge_add_i2c_tpa2028d_devices(void)
{
	/* Run the array and install devices as appropriate */
	i2c_register_board_info(msm_i2c_audiosubsystem.bus,
				msm_i2c_audiosubsystem.info,
				msm_i2c_audiosubsystem.len);
}

static void enable_external_mic_bias(int status)
{
        static struct regulator *reg_mic_bias = NULL;
        static int prev_on;
        int rc = 0;

        if (status == prev_on)
                return;

        if (lge_get_board_revno() > HW_REV_C) {
                if (!reg_mic_bias) {
                        reg_mic_bias = regulator_get(NULL, "mic_bias");
                        if (IS_ERR(reg_mic_bias)) {
                                pr_err("%s: could not regulator_get\n",
                                                __func__);
                                reg_mic_bias = NULL;
                                return;
                        }
                }

                if (status) {
                        rc = regulator_enable(reg_mic_bias);
                        if (rc)
                                pr_err("%s: regulator enable failed\n",
                                                __func__);
                        pr_debug("%s: mic_bias is on\n", __func__);
                } else {
                        rc = regulator_disable(reg_mic_bias);
                        if (rc)
                                pr_warn("%s: regulator disable failed\n",
                                                __func__);
                        pr_debug("%s: mic_bias is off\n", __func__);
                }
        }

        if (lge_get_board_revno() < HW_REV_1_0)
                gpio_set_value_cansleep(GPIO_EAR_MIC_BIAS_EN, status);
        prev_on = status;
}

static struct fsa8008_platform_data lge_hs_pdata = {
        .switch_name = "h2w",
        .keypad_name = "hs_detect",

        .key_code = KEY_MEDIA,

        .gpio_detect = GPIO_EAR_SENSE_N,
        .gpio_mic_en = GPIO_EAR_MIC_EN,
        .gpio_mic_bias_en = GPIO_EAR_MIC_BIAS_EN,
        .gpio_jpole  = GPIO_EARPOL_DETECT,
        .gpio_key    = GPIO_EAR_KEY_INT,

        .latency_for_detection = 75,
        .set_headset_mic_bias = enable_external_mic_bias,
};

static __init void j1_fixed_audio(void)
{
        if (lge_get_board_revno() >= HW_REV_1_0)
                lge_hs_pdata.gpio_mic_bias_en = -1;
        if (lge_get_board_revno() > HW_REV_1_0) {
                lge_hs_pdata.gpio_detect = GPIO_EAR_SENSE_N_REV11;
                lge_hs_pdata.gpio_detect_can_wakeup = 1;
        }
}

static struct platform_device lge_hsd_device = {
        .name = "fsa8008",
        .id   = -1,
        .dev = {
                .platform_data = &lge_hs_pdata,
        },
};

static struct platform_device *sound_devices[] __initdata = {
        &lge_hsd_device,
};

void __init lge_add_sound_devices(void)
{
        j1_fixed_audio();
        lge_add_i2c_tpa2028d_devices();
        platform_add_devices(sound_devices, ARRAY_SIZE(sound_devices));
}
