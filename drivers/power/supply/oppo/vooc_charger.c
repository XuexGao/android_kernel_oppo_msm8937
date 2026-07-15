// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2008-2012, OPPO Mobile Comm Corp., Ltd
 * Copyright (c) 2025 Ricky Cheung <rcheung844@gmail.com>
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/of.h>

#include <linux/power/oppo_gauge.h>
#include <linux/power/oppo_vooc.h>

#define VOOC_POLL_INTERVAL 100

void vooc_set_fast_charge_enabled(struct oppo_vooc_device *vooc, bool enabled)
{
	int i;
	int values[vooc->switch_gpios->ndescs];

	for (i = 0; i < vooc->switch_gpios->ndescs; i++)
		values[i] = enabled;

	vooc->fast_charge_enabled = enabled;
	gpiod_set_array_value_cansleep(vooc->switch_gpios->ndescs,
				       vooc->switch_gpios->desc, values);
}

void vooc_chip_step_clock(struct oppo_vooc_device *vooc) {
	gpiod_set_value_cansleep(vooc->clock_gpio, 0);
	usleep_range(1000, 1000);

	gpiod_set_value_cansleep(vooc->clock_gpio, 1);
	usleep_range(19000, 19000);
}

void vooc_chip_send_reply(struct oppo_vooc_device *vooc, int reply_info)
{
	int device_type = 0;

#ifdef CONFIG_BATTERY_GAUGE_OPPO_BQ27541
	if (bq27541_device_type >= 0)
		device_type = bq27541_device_type;
#endif

	// Set data GPIOs to sleep (output)
	gpiod_direction_output(vooc->data_gpio, 0);

	// Send data through data GPIOs
	gpiod_set_value_cansleep(vooc->data_gpio, reply_info >> 1);
	vooc_chip_step_clock(vooc);
	gpiod_set_value_cansleep(vooc->data_gpio, reply_info & 0x1);
	vooc_chip_step_clock(vooc);
	gpiod_set_value_cansleep(vooc->data_gpio, device_type);
	vooc_chip_step_clock(vooc);

	// Set data GPIOs to active (input)
	gpiod_direction_input(vooc->data_gpio);
}

static void vooc_poll_func(struct work_struct *work)
{
	int i, data, bit;
	int chip_reply_info = 2; // FIXME: May also be returning 1
	struct delayed_work *dwork = to_delayed_work(work);
	struct oppo_vooc_device *vooc = container_of(dwork,
			struct oppo_vooc_device, vooc_poll_work);
	struct device *dev = vooc->dev;

	if (!mutex_trylock(&vooc->lock))
		return;

	// Check if data received start with "101"
	for (i = 0; i < 7; i++) {
		vooc_chip_step_clock(vooc);
		if (i != 2)
			continue;

		bit = gpiod_get_value_cansleep(vooc->data_gpio);
		data |= bit << (6 - i);
		if (data != 0x50 && !vooc->fw_update_required) {
			if (!vooc->fast_charge_enabled)
				goto out;

			dev_err(dev, "received errornous data: 0x%x\n", data);
			vooc_set_fast_charge_enabled(vooc, false);
			// TODO!
		}
	}

	dev_dbg(dev, "received data: 0x%x, fw version: 0x%x\n",
		 data, vooc->fw_data_version);

	switch (data) {
	case VOOC_NOTIFY_FAST_PRESENT:
		vooc_set_fast_charge_enabled(vooc, true);
		break;
	case VOOC_NOTIFY_FAST_ABSENT:
		vooc_set_fast_charge_enabled(vooc, false);
		break;
	case VOOC_NOTIFY_ALLOW_READING_IIC:
		// TODO: Investigate whatever this does
		dev_info(dev, "chip reported to allowing reading i2c\n",
			 data, vooc->fw_data_version);
		break;
	case VOOC_NOTIFY_LOW_TEMP_FULL:
	case VOOC_NOTIFY_BAD_CONNECTED:
	case VOOC_NOTIFY_TEMP_OVER:
	case VOOC_NOTIFY_BTB_TEMP_OVER:
		dev_err(dev, "chip reported error, data: 0x%x\n",
			data, vooc->fw_data_version);
	case VOOC_NOTIFY_NORMAL_TEMP_FULL:
		vooc_set_fast_charge_enabled(vooc, false);
		break;
	case VOOC_NOTIFY_FIRMWARE_UPDATE:
	case VOOC_NOTIFY_ADAPTER_FW_UPDATE:
		// TODO: Implement firmware update
		dev_info(dev, "chip reported for fw update\n",
			data, vooc->fw_data_version);
		break;
	default:
		dev_err(dev, "received errornous data: 0x%x\n", data);
		vooc_set_fast_charge_enabled(vooc, false);
		break;
	}

	vooc_chip_send_reply(vooc, chip_reply_info);

out:
	vooc_chip_step_clock(vooc);
	mutex_unlock(&vooc->lock);

	queue_delayed_work(vooc->vooc_poll_wq,
			   &vooc->vooc_poll_work,
			   msecs_to_jiffies(VOOC_POLL_INTERVAL));
}

static int vooc_probe(struct i2c_client *client,
		      const struct i2c_device_id *id)
{
	int rc = 0;

	struct oppo_vooc_device *vooc = devm_kzalloc(&client->dev, sizeof(*vooc), GFP_KERNEL);
	if (!vooc) {
		rc = -ENOMEM;
		goto exit;
	}

	vooc->client = client;
	vooc->dev = &client->dev;
	i2c_set_clientdata(client, vooc);

	vooc->switch_gpios = devm_gpiod_get_array(&client->dev,
			"switch", GPIOD_OUT_LOW);
	if (IS_ERR(vooc->switch_gpios)) {
		rc = PTR_ERR(vooc->switch_gpios);
		dev_err(&client->dev,
			"Failed to request GPIO switch pins, error %d\n", rc);
		goto exit;
	}

	vooc->reset_gpio = devm_gpiod_get(&client->dev,
				"reset", GPIOD_OUT_LOW);
	if (IS_ERR(vooc->reset_gpio)) {
		rc = PTR_ERR(vooc->reset_gpio);
		dev_err(&client->dev,
			"Failed to request GPIO reset pin, error %d\n", rc);
		goto exit;
	}

	vooc->clock_gpio = devm_gpiod_get(&client->dev,
				"clock", GPIOD_OUT_LOW);
	if (IS_ERR(vooc->clock_gpio)) {
		rc = PTR_ERR(vooc->clock_gpio);
		dev_err(&client->dev,
			"Failed to request GPIO clock pin, error %d\n", rc);
		goto exit;
	}

	vooc->data_gpio = devm_gpiod_get(&client->dev,
				"data", GPIOD_IN);
	if (IS_ERR(vooc->data_gpio)) {
		rc = PTR_ERR(vooc->data_gpio);
		dev_err(&client->dev,
			"Failed to request GPIO data pin, error %d\n", rc);
		goto exit;
	}

	mutex_init(&vooc->lock);

	INIT_DELAYED_WORK(&vooc->vooc_poll_work, vooc_poll_func);
	vooc->vooc_poll_wq = alloc_workqueue("vooc_poll_work",
				WQ_FREEZABLE | WQ_UNBOUND | WQ_MEM_RECLAIM, 0);
	if (!vooc->vooc_poll_wq) {
		dev_err(&client->dev, "Cannot create workqueue.\n");
		rc = -ENOMEM;
		goto exit;
	}

	queue_delayed_work(vooc->vooc_poll_wq,
			   &vooc->vooc_poll_work,
			   msecs_to_jiffies(VOOC_POLL_INTERVAL));

exit:
	return rc;
}

static int vooc_remove(struct i2c_client *client)
{
	struct oppo_vooc_device *vooc = i2c_get_clientdata(client);

	dev_dbg(&client->dev, "removed\n");
	if (vooc->vooc_poll_wq)
		destroy_workqueue(vooc->vooc_poll_wq);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int vooc_suspend(struct device *dev)
{
	struct oppo_vooc_device *vooc = dev_get_drvdata(dev);

	dev_dbg(dev, "suspended\n");
	cancel_delayed_work_sync(&vooc->vooc_poll_work);

	return 0;
}

static int vooc_resume(struct device *dev)
{
	struct oppo_vooc_device *vooc = dev_get_drvdata(dev);

	dev_dbg(dev, "resumed\n");
	queue_delayed_work(vooc->vooc_poll_wq,
			&vooc->vooc_poll_work,
			msecs_to_jiffies(VOOC_POLL_INTERVAL));

	return 0;
}
#endif

static const struct dev_pm_ops vooc_pm_ops = {
	.suspend = vooc_suspend,
	.resume = vooc_resume,
};

static const struct i2c_device_id vooc_i2c_ids[] = {
	{ "pic16f-vooc", 0 },
	{ "stm8s-vooc", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, vooc_i2c_ids);

static const struct of_device_id vooc_of_match[] = {
	{ .compatible = "oppo,pic16f-vooc", },
	{ .compatible = "oppo,stm8s-vooc", },
	{}
};
MODULE_DEVICE_TABLE(of, vooc_of_match);

static struct i2c_driver vooc_i2c_driver = {
	.driver = {
		.name	= "vooc-charger",
		.owner	= THIS_MODULE,
		.of_match_table = vooc_of_match,
		.pm = &vooc_pm_ops,
	},
	.probe	= vooc_probe,
	.remove	= vooc_remove,
	.id_table = vooc_i2c_ids,
};
module_i2c_driver(vooc_i2c_driver);

MODULE_DESCRIPTION("OPPO VOOC charging driver");
MODULE_LICENSE("GPL v2");
