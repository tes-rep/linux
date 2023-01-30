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
#include <linux/usb/typec_dp.h>
#include <linux/usb/typec_mux.h>

struct ch482d {
	struct gpio_desc *enable_gpio;
	struct gpio_desc *select_gpio;

	struct typec_switch_dev *sw;
	struct typec_mux_dev *mux;

	struct mutex lock; /* protect enabled and swapped */
	bool enabled;
	bool swapped;
};

static int ch482d_switch_set(struct typec_switch_dev *sw,
			       enum typec_orientation orientation)
{
	struct ch482d *ch482d = typec_switch_get_drvdata(sw);
	bool enabled;
	bool swapped;

	mutex_lock(&ch482d->lock);

	enabled = ch482d->enabled;

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

	if (enabled != ch482d->enabled)
		gpiod_set_value(ch482d->enable_gpio, enabled);

	if (swapped != ch482d->swapped)
		gpiod_set_value(ch482d->select_gpio, swapped);

	ch482d->enabled = enabled;
	ch482d->swapped = swapped;

	mutex_unlock(&ch482d->lock);

	return 0;
}

/* static int ch482d_set(struct ch482d_dev *mux,
			 struct ch482d_state *state)
{
	struct ch482d *ch482d = ch482d_get_drvdata(mux);

	mutex_lock(&ch482d->lock);

	switch (state->mode) {
	case TYPEC_STATE_SAFE:
	case TYPEC_STATE_USB:
		ch482d->enabled = false;
		break;
	case TYPEC_DP_STATE_C:
	case TYPEC_DP_STATE_D:
	case TYPEC_DP_STATE_E:
		ch482d->enabled = true;
		break;
	default:
		break;
	}

	gpiod_set_value(ch482d->enable_gpio, ch482d->enabled);

	mutex_unlock(&ch482d->lock);

	return 0;
} */

static int ch482d_probe(struct platform_device *pdev)
{
	struct typec_switch_desc sw_desc = { };
	/* struct typec_mux_desc mux_desc = { }; */
	struct device *dev = &pdev->dev;
	struct ch482d *ch482d;

	ch482d = devm_kzalloc(dev, sizeof(*ch482d), GFP_KERNEL);
	if (!ch482d)
		return -ENOMEM;

	mutex_init(&ch482d->lock);

	/* implementations frequently use pull-down to enable */
	ch482d->enable_gpio = devm_gpiod_get_optional(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(ch482d->enable_gpio))
		return dev_err_probe(dev, PTR_ERR(ch482d->enable_gpio),
				     "unable to acquire enable gpio\n");

	ch482d->select_gpio = devm_gpiod_get(dev, "select", GPIOD_OUT_LOW);
	if (IS_ERR(ch482d->select_gpio))
		return dev_err_probe(dev, PTR_ERR(ch482d->select_gpio),
				     "unable to acquire select gpio\n");

	sw_desc.drvdata = ch482d;
	sw_desc.fwnode = dev_fwnode(dev);
	sw_desc.set = ch482d_switch_set;

	ch482d->sw = typec_switch_register(dev, &sw_desc);
	if (IS_ERR(ch482d->sw))
		return dev_err_probe(dev, PTR_ERR(ch482d->sw),
				     "failed to register ch482d switch\n");

	/* mux_desc.drvdata = ch482d;
	mux_desc.fwnode = dev_fwnode(dev);
	mux_desc.set = ch482d_set;

	ch482d = typec_mux_register(dev, &mux_desc);
	if (IS_ERR(ch482d->mux)) {
		typec_switch_unregister(ch482d->sw);
		return dev_err_probe(dev, PTR_ERR(ch482d->mux),
				     "failed to register ch482d mux\n");
	} */

	platform_set_drvdata(pdev, ch482d);

	return 0;
}

static int ch482d_remove(struct platform_device *pdev)
{
	struct ch482d *ch482d = platform_get_drvdata(pdev);

	gpiod_set_value(ch482d->enable_gpio, 0);

	typec_mux_unregister(ch482d->mux);
	typec_switch_unregister(ch482d->sw);

	return 0;
}

static const struct of_device_id ch482d_match[] = {
	{ .compatible = "ch482d-gpio-switch", },
	{}
};
MODULE_DEVICE_TABLE(of, ch482d_match);

static struct platform_driver ch482d_driver = {
	.probe = ch482d_probe,
	.remove = ch482d_remove,
	.driver = {
		.name = "ch482d",
		.of_match_table = ch482d_match,
	},
};
module_platform_driver(ch482d_driver);

MODULE_DESCRIPTION("CH482D GPIO based USB TYPE-C switch driver");
MODULE_LICENSE("GPL");
