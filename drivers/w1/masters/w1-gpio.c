/*
 * w1-gpio - GPIO w1 bus master driver
 *
 * Copyright (C) 2007 Ville Syrjala <syrjala@sci.fi>
 * Copyright (C) 2015 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#define pr_fmt(fmt)	"w1-gpio: %s:" fmt , __func__
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/w1-gpio.h>
#include <linux/gpio.h>

#include "../w1.h"
#include "../w1_int.h"

#include <linux/of.h>
#include <linux/err.h>

static struct of_device_id w1_gpio_match_table[] = {
       { .compatible = "qcom,w1-gpio" },
       {}
};

static void w1_gpio_write_bit_dir(void *data, u8 bit)
{
	struct w1_gpio_platform_data *pdata = data;

	if (bit)
		gpio_direction_input(pdata->pin);
	else
		gpio_direction_output(pdata->pin, 0);
}

static void w1_gpio_write_bit_val(void *data, u8 bit)
{
	struct w1_gpio_platform_data *pdata = data;

	gpio_set_value(pdata->pin, bit);
}

static u8 w1_gpio_read_bit(void *data)
{
	struct w1_gpio_platform_data *pdata = data;

	return gpio_get_value(pdata->pin) ? 1 : 0;
}

static int w1_gpio_probe_dt(struct device_node *node, 
			struct w1_gpio_platform_data *pdata)
{
	char *key;
	int ret;
	unsigned int is_open_drain;
	
	key = "qcom,gpio-pin";
	ret = of_property_read_u32(node, key, &(pdata->pin));
	if (ret) {
                pr_err("%s: missing DT key '%s'\n", __func__, key);
                return ret;
	}
	pr_info("gpio %d\n", pdata->pin);
	
	key = "qcom,is-open-drain";
	ret = of_property_read_u32(node, key, &(is_open_drain));
	if (ret) {
                pr_err("%s: missing DT key '%s'\n", __func__, key);
                return ret;
	}
	pdata->is_open_drain = is_open_drain?1:0;
	
	return 0;
}

static int __init w1_gpio_probe(struct platform_device *pdev)
{
	struct w1_bus_master *master;
	struct w1_gpio_platform_data *pdata = pdev->dev.platform_data;
	int err = 0;
	struct device_node *node;

	if (!pdata) {
                pdata = kzalloc(sizeof(struct w1_gpio_platform_data), GFP_KERNEL);
                if (!pdata)
                        return -ENOMEM;
                pdev->dev.platform_data =(void *) pdata;
	}
	
	node = pdev->dev.of_node;
	/* parse dt */
	err = w1_gpio_probe_dt(node, pdata);
	if (err) {
		pr_err("%s: failed to parse DT \n", __func__);
		goto free_pdata;
	}

	master = kzalloc(sizeof(struct w1_bus_master), GFP_KERNEL);
	if (!master)
		return -ENOMEM;

	err = gpio_request(pdata->pin, "w1");
	if (err)
		goto free_master;

	master->data = pdata;
	master->read_bit = w1_gpio_read_bit;

	if (pdata->is_open_drain) {
		gpio_direction_output(pdata->pin, 1);
		master->write_bit = w1_gpio_write_bit_val;
	} else {
		gpio_direction_input(pdata->pin);
		master->write_bit = w1_gpio_write_bit_dir;
	}

	err = w1_add_master_device(master);
	if (err)
		goto free_gpio;

	if (pdata->enable_external_pullup)
		pdata->enable_external_pullup(1);

	platform_set_drvdata(pdev, master);

	return 0;

 free_gpio:
	gpio_free(pdata->pin);
 free_master:
	kfree(master);
 free_pdata:
 	kfree(pdata);

	return err;
}

static int __exit w1_gpio_remove(struct platform_device *pdev)
{
	struct w1_bus_master *master = platform_get_drvdata(pdev);
	struct w1_gpio_platform_data *pdata = pdev->dev.platform_data;

	if (pdata->enable_external_pullup)
		pdata->enable_external_pullup(0);

	w1_remove_master_device(master);
	gpio_free(pdata->pin);
	kfree(master);

	return 0;
}

#ifdef CONFIG_PM

static int w1_gpio_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct w1_gpio_platform_data *pdata = pdev->dev.platform_data;

	if (pdata->enable_external_pullup)
		pdata->enable_external_pullup(0);

	return 0;
}

static int w1_gpio_resume(struct platform_device *pdev)
{
	struct w1_gpio_platform_data *pdata = pdev->dev.platform_data;

	if (pdata->enable_external_pullup)
		pdata->enable_external_pullup(1);

	return 0;
}

#else
#define w1_gpio_suspend	NULL
#define w1_gpio_resume	NULL
#endif

static struct platform_driver w1_gpio_driver = {
	.driver = {
		.name	= "w1-gpio",
		.owner	= THIS_MODULE,
		.of_match_table = w1_gpio_match_table,
	},
	.remove	= __exit_p(w1_gpio_remove),
	.suspend = w1_gpio_suspend,
	.resume = w1_gpio_resume,
};

static int __init w1_gpio_init(void)
{
	return platform_driver_probe(&w1_gpio_driver, w1_gpio_probe);
}

static void __exit w1_gpio_exit(void)
{
	platform_driver_unregister(&w1_gpio_driver);
}

module_init(w1_gpio_init);
module_exit(w1_gpio_exit);

MODULE_DESCRIPTION("GPIO w1 bus master driver");
MODULE_AUTHOR("Ville Syrjala <syrjala@sci.fi>");
MODULE_LICENSE("GPL");
