// SPDX-License-Identifier: GPL-2.0-only
/*
 * w1-gpio - GPIO w1 bus master driver
 *
 * Copyright (C) 2007 Ville Syrjala <syrjala@sci.fi>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/w1-gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_platform.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/delay.h>

#include <linux/w1.h>

static u8 w1_gpio_set_pullup(void *data, int delay)
{
	struct w1_gpio_platform_data *pdata = data;

	if (delay) {
		pdata->strong_pullup_duration = delay;
	} else {
		if (pdata->strong_pullup_duration) {
			if (pdata->strong_pullup_gpiod) {
				/* Set STRONG PU, attn: the GPIO may need to be configured as ACTIVE_LOW in the DT */
				gpiod_set_value(pdata->strong_pullup_gpiod, 1);
				msleep(pdata->strong_pullup_duration);
				gpiod_set_value(pdata->strong_pullup_gpiod, 0);
			} else {
				printk("W1-GPIO: strong pull up requested, but not available");
			}
		}
		pdata->strong_pullup_duration = 0;
	}

	return 0;
}

static void w1_gpio_write_bit(void *data, u8 bit)
{
	struct w1_gpio_platform_data *pdata = data;

	if (pdata->pulldown_gpiod) {
		gpiod_set_value(pdata->pulldown_gpiod, !bit);
	} else {
		gpiod_set_value(pdata->gpiod, bit);
	}
}

static u8 w1_gpio_read_bit(void *data)
{
	struct w1_gpio_platform_data *pdata = data;

	return gpiod_get_value(pdata->gpiod) ? 1 : 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id w1_gpio_dt_ids[] = {
	{ .compatible = "w1-gpio" },
	{}
};
MODULE_DEVICE_TABLE(of, w1_gpio_dt_ids);
#endif

static int w1_gpio_probe(struct platform_device *pdev)
{
	struct w1_bus_master *master;
	struct w1_gpio_platform_data *pdata;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	/* Enforce open drain mode by default */
	enum gpiod_flags gflags = GPIOD_OUT_LOW_OPEN_DRAIN;
	int err;

	if (of_have_populated_dt()) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;

		/*
		 * This parameter means that something else than the gpiolib has
		 * already set the line into open drain mode, so we should just
		 * driver it high/low like we are in full control of the line and
		 * open drain will happen transparently.
		 */
		if (of_get_property(np, "linux,open-drain", NULL))
			gflags = GPIOD_OUT_LOW;

		pdev->dev.platform_data = pdata;
	}
	pdata = dev_get_platdata(dev);

	if (!pdata) {
		dev_err(dev, "No configuration data\n");
		return -ENXIO;
	}

	master = devm_kzalloc(dev, sizeof(struct w1_bus_master),
			GFP_KERNEL);
	if (!master) {
		dev_err(dev, "Out of memory\n");
		return -ENOMEM;
	}

	pdata->pulldown_gpiod = devm_gpiod_get_index_optional(dev, NULL, 2, GPIOD_OUT_LOW);
	if (IS_ERR(pdata->pulldown_gpiod)) {
		dev_err(dev, "gpio_request (pull down pin) failed\n");
		return PTR_ERR(pdata->pulldown_gpiod);
	}

	/* if we have a pulldown pin, the data pin is only used as an input */
	if (pdata->pulldown_gpiod)
		gflags = GPIOD_IN;

	pdata->gpiod = devm_gpiod_get_index(dev, NULL, 0, gflags);
	if (IS_ERR(pdata->gpiod)) {
		dev_err(dev, "gpio_request (data pin) failed\n");
		return PTR_ERR(pdata->gpiod);
	}

	pdata->strong_pullup_gpiod = devm_gpiod_get_index(dev, NULL, 1, GPIOD_OUT_LOW);
	if (IS_ERR(pdata->strong_pullup_gpiod)) {
		dev_err(dev, "gpio_request (strong pull up pin) failed\n");
		return PTR_ERR(pdata->strong_pullup_gpiod);
	}

	if (!pdata->pulldown_gpiod)
		gpiod_direction_output(pdata->gpiod, 1);

	master->data = pdata;
	master->read_bit = w1_gpio_read_bit;
	master->write_bit = w1_gpio_write_bit;

	if (gflags == GPIOD_OUT_LOW_OPEN_DRAIN || pdata->strong_pullup_gpiod)
		master->set_pullup = w1_gpio_set_pullup;

	err = w1_add_master_device(master);
	if (err) {
		dev_err(dev, "w1_add_master device failed\n");
		return err;
	}

	platform_set_drvdata(pdev, master);

	return 0;
}

static int w1_gpio_remove(struct platform_device *pdev)
{
	struct w1_bus_master *master = platform_get_drvdata(pdev);
	struct w1_gpio_platform_data *pdata = dev_get_platdata(&pdev->dev);

	if (pdata->strong_pullup_gpiod)
		gpiod_set_value(pdata->strong_pullup_gpiod, 0);

	w1_remove_master_device(master);

	return 0;
}

static int __maybe_unused w1_gpio_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused w1_gpio_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(w1_gpio_pm_ops, w1_gpio_suspend, w1_gpio_resume);

static struct platform_driver w1_gpio_driver = {
	.driver = {
		.name	= "w1-gpio",
		.pm	= &w1_gpio_pm_ops,
		.of_match_table = of_match_ptr(w1_gpio_dt_ids),
	},
	.probe = w1_gpio_probe,
	.remove = w1_gpio_remove,
};

module_platform_driver(w1_gpio_driver);

MODULE_DESCRIPTION("GPIO w1 bus master driver");
MODULE_AUTHOR("Ville Syrjala <syrjala@sci.fi>");
MODULE_LICENSE("GPL");
