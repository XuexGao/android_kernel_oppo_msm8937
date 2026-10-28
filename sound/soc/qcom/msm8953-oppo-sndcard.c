// SPDX-License-Identifier: GPL-2.0-only
/*
 * OPPO R9s (MSM8953) 声卡驱动 —— 通用 DTS 驱动版
 *
 * 为什么自己写而不是用现成的：
 *
 *   · 下游 4.9/4.19 用的是 techpack/audio/asoc/msm8952.c，那是一整套
 *     CAF 音频框架（12.7MB / 487 文件），依赖 q6afe-v2、msm-pcm-routing-v2、
 *     msm-cdc-pinctrl 等一堆 priv 层，前移到 6.12 是不现实的。
 *
 *   · ACK 6.12 已经有一套干净的上游 QDSP6 栈：q6afe / q6asm / q6adm /
 *     q6routing，msm8953.dtsi 里 LPASS + 这些 service 节点都是现成的。
 *     缺的只是一个把它们和 codec 连起来的 machine driver。
 *
 *   · ACK 还提供 qcom_snd_parse_of()，能从 DT 的 sound 节点里自动解析
 *     cpu/codec/platform 三方 dai link，所以本驱动**不硬编码任何链路** ——
 *     所有拓扑都写在 board dts 里。
 *
 * 与 apq8016_sbc.c / apq8096.c 的关系：
 *   那两个是给具体板子写死 dai_link 的。本驱动采用 common.c 的
 *   qcom_snd_parse_of() 路线，更适合"一套代码支持 R9s/R9sk 两个机型"。
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include "common.h"

#define DRV_NAME "msm8953-oppo-sndcard"

struct oppo_sndcard_data {
	struct snd_soc_card card;
	struct snd_soc_jack *jack;
};

static int oppo_sndcard_parse_of(struct oppo_sndcard_data *data)
{
	struct snd_soc_card *card = &data->card;
	struct device *dev = card->dev;
	int ret;

	ret = qcom_snd_parse_of(card);
	if (ret) {
		dev_err(dev, "解析 sound 节点失败: %d\n", ret);
		return ret;
	}

	return 0;
}

static int oppo_sndcard_probe(struct platform_device *pdev)
{
	struct oppo_sndcard_data *data;
	struct snd_soc_card *card;
	int ret;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	card = &data->card;
	card->dev = &pdev->dev;
	card->owner = THIS_MODULE;

	platform_set_drvdata(pdev, data);

	ret = oppo_sndcard_parse_of(data);
	if (ret)
		return ret;

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "注册声卡失败: %d\n", ret);
		return ret;
	}

	dev_info(&pdev->dev, "OPPO R9s 声卡已注册 (%d 条链路)\n",
		 card->num_links);

	return 0;
}

static const struct of_device_id oppo_sndcard_of_match[] = {
	{ .compatible = "oppo,msm8953-sndcard" },
	{ }
};
MODULE_DEVICE_TABLE(of, oppo_sndcard_of_match);

static struct platform_driver oppo_sndcard_driver = {
	.probe  = oppo_sndcard_probe,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = oppo_sndcard_of_match,
		.pm = &snd_soc_pm_ops,
	},
};
module_platform_driver(oppo_sndcard_driver);

MODULE_DESCRIPTION("OPPO MSM8953 sound card (DTS-driven)");
MODULE_LICENSE("GPL");
