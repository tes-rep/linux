// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Linaro Ltd.
 */

#include <linux/bits.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
/* #include <linux/usb/typec_dp.h> */
#include <linux/usb/typec_mux.h>

struct gpio_ch482d_mux {
	struct gpio_desc *enable_gpio;
	struct gpio_desc *select_gpio;

	struct typec_switch_dev *sw;
	struct typec_mux_dev *mux;

	struct mutex lock; /* protect enabled and swapped */
	bool enabled;
	bool swapped;
};

static int gpio_ch482d_switch_set(struct typec_switch_dev *sw,
			       enum typec_orientation orientation)
{
	struct gpio_ch482d_mux *ch482d_mux = typec_switch_get_drvdata(sw);
	bool enabled;
	bool swapped;

	mutex_lock(&ch482d_mux->lock);

	enabled = ch482d_mux->enabled;

	switch (orientation) {
	case TYPEC_ORIENTATION_NONE:
		enabled = false;
		break;
	case TYPEC_ORIENTATION_NORMAL:
		swapped = false;
		break;
	case TYPEC_ORIENTATION_REVERSE:
		swapped = true;
		break;
	}

	if (enabled != ch482d_mux->enabled)
		gpiod_set_value(ch482d_mux->enable_gpio, enabled);

	if (swapped != ch482d_mux->swapped)
		gpiod_set_value(ch482d_mux->select_gpio, swapped);

	ch482d_mux->enabled = enabled;
	ch482d_mux->swapped = swapped;

	mutex_unlock(&ch482d_mux->lock);

	return 0;
}

/* static int gpio_ch482d_mux_set(struct ch482d_mux_dev *mux,
			    struct ch482d_mux_state *state)
{
	struct gpio_ch482d_mux *ch482d_mux = ch482d_mux_get_drvdata(mux);

	mutex_lock(&ch482d_mux->lock);

	switch (state->mode) {
	case TYPEC_STATE_SAFE:
	case TYPEC_STATE_USB:
		ch482d_mux->enabled = false;
		break;
	case TYPEC_DP_STATE_C:
	case TYPEC_DP_STATE_D:
	case TYPEC_DP_STATE_E:
		ch482d_mux->enabled = true;
		break;
	default:
		break;
	}

	gpiod_set_value(ch482d_mux->enable_gpio, ch482d_mux->enabled);

	mutex_unlock(&ch482d_mux->lock);

	return 0;
} */

static int gpio_ch482d_mux_probe(struct platform_device *pdev)
{
	struct typec_switch_desc sw_desc = { };
	/* struct typec_mux_desc mux_desc = { }; */
	struct device *dev = &pdev->dev;
	struct gpio_ch482d_mux *ch482d_mux;

	ch482d_mux = devm_kzalloc(dev, sizeof(*ch482d_mux), GFP_KERNEL);
	if (!ch482d_mux)
		return -ENOMEM;

	mutex_init(&ch482d_mux->lock);

	ch482d_mux->enable_gpio = devm_gpiod_get_optional(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(ch482d_mux->enable_gpio))
		return dev_err_probe(dev, PTR_ERR(ch482d_mux->enable_gpio),
				     "unable to acquire enable gpio\n");

	ch482d_mux->select_gpio = devm_gpiod_get(dev, "select", GPIOD_OUT_LOW);
	if (IS_ERR(ch482d_mux->select_gpio))
		return dev_err_probe(dev, PTR_ERR(ch482d_mux->select_gpio),
				     "unable to acquire select gpio\n");

	sw_desc.drvdata = ch482d_mux;
	sw_desc.fwnode = dev_fwnode(dev);
	sw_desc.set = gpio_ch482d_switch_set;

	ch482d_mux->sw = typec_switch_register(dev, &sw_desc);
	if (IS_ERR(ch482d_mux->sw))
		return dev_err_probe(dev, PTR_ERR(ch482d_mux->sw),
				     "failed to register ch482d switch\n");

	/* mux_desc.drvdata = ch482d_mux;
	mux_desc.fwnode = dev_fwnode(dev);
	mux_desc.set = gpio_ch482d_mux_set;

	ch482d_mux->mux = typec_mux_register(dev, &mux_desc);
	if (IS_ERR(ch482d_mux->mux)) {
		typec_switch_unregister(ch482d_mux->sw);
		return dev_err_probe(dev, PTR_ERR(ch482d_mux->mux),
				     "failed to register ch482d mux\n");
	} */

	platform_set_drvdata(pdev, ch482d_mux);

	return 0;
}

static int gpio_ch482d_mux_remove(struct platform_device *pdev)
{
	struct gpio_ch482d_mux *ch482d_mux = platform_get_drvdata(pdev);

	gpiod_set_value(ch482d_mux->enable_gpio, 0);

	typec_mux_unregister(ch482d_mux->mux);
	typec_switch_unregister(ch482d_mux->sw);

	return 0;
}

static const struct of_device_id gpio_ch482d_mux_match[] = {
	{ .compatible = "gpio-ch482d-mux", },
	{}
};
MODULE_DEVICE_TABLE(of, gpio_ch482d_mux_match);

static struct platform_driver gpio_ch482d_mux_driver = {
	.probe = gpio_ch482d_mux_probe,
	.remove = gpio_ch482d_mux_remove,
	.driver = {
		.name = "gpio_ch482d_mux",
		.of_match_table = gpio_ch482d_mux_match,
	},
};
module_platform_driver(gpio_ch482d_mux_driver);

MODULE_DESCRIPTION("CH482D GPIO based USB TYPE-C mux driver");
MODULE_LICENSE("GPL");
