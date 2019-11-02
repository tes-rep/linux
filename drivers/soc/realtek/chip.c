// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Realtek System-on-Chip info
 *
 * Copyright (c) 2017-2019 Andreas Färber
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sys_soc.h>

#define REG_CHIP_ID	0x0
#define REG_CHIP_REV	0x4

struct rtd_soc_revision {
	const char *name;
	u32 chip_rev;
};

static const struct rtd_soc_revision rtd1195_revisions[] = {
	{ "A", 0x00000000 },
	{ "B", 0x00010000 },
	{ "C", 0x00020000 },
	{ "D", 0x00030000 },
	{ }
};

static const struct rtd_soc_revision rtd1295_revisions[] = {
	{ "A00", 0x00000000 },
	{ "A01", 0x00010000 },
	{ "B00", 0x00020000 },
	{ "B01", 0x00030000 },
	{ }
};

static const struct rtd_soc_revision rtd1395_revisions[] = {
	{ "A00", 0x00000000 },
	{ "A01", 0x00010000 },
	{ "A02", 0x00020000 },
	{ }
};

struct rtd_soc {
	u32 chip_id;
	const char *family;
	const char *(*get_name)(struct device *dev, const struct rtd_soc *s);
	const struct rtd_soc_revision *revisions;
	const char *codename;
};

static const char *default_name(struct device *dev, const struct rtd_soc *s)
{
	return s->family;
}

static const char *rtd1295_name(struct device *dev, const struct rtd_soc *s)
{
	void __iomem *base;

	base = of_iomap(dev->of_node, 2);
	if (base) {
		u32 efuse = readl_relaxed(base);
		iounmap(base);
		if ((efuse & 0x3) == 0x1)
			return "RTD1294";
	}

	base = of_iomap(dev->of_node, 1);
	if (base) {
		u32 chipinfo1 = readl_relaxed(base);
		iounmap(base);
		if (chipinfo1 & BIT(11)) {
			if (chipinfo1 & BIT(4))
				return "RTD1293";
			return "RTD1296";
		}
	}

	return "RTD1295";
}

static const struct rtd_soc rtd_soc_families[] = {
	{ 0x00006329, "RTD1195", default_name, rtd1195_revisions, "Phoenix" },
	{ 0x00006421, "RTD1295", rtd1295_name, rtd1295_revisions, "Kylin" },
	{ 0x00006481, "RTD1395", default_name, rtd1395_revisions, "Hercules" },
};

static const struct rtd_soc *rtd_soc_by_chip_id(u32 chip_id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rtd_soc_families); i++) {
		const struct rtd_soc *family = &rtd_soc_families[i];

		if (family->chip_id == chip_id)
			return family;
	}
	return NULL;
}

static const char *rtd_soc_rev(const struct rtd_soc *family, u32 chip_rev)
{
	if (family) {
		const struct rtd_soc_revision *rev = family->revisions;

		while (rev && rev->name) {
			if (rev->chip_rev == chip_rev)
				return rev->name;
			rev++;
		}
	}
	return "unknown";
}

static int rtd_soc_probe(struct platform_device *pdev)
{
	const struct rtd_soc *s;
	struct soc_device_attribute *soc_dev_attr;
	struct soc_device *soc_dev;
	struct device_node *node;
	void __iomem *base;
	u32 chip_id, chip_rev;

	base = of_iomap(pdev->dev.of_node, 0);
	if (!base)
		return -ENODEV;

	soc_dev_attr = kzalloc(sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr)
		return -ENOMEM;

	chip_id  = readl_relaxed(base + REG_CHIP_ID);
	chip_rev = readl_relaxed(base + REG_CHIP_REV);

	node = of_find_node_by_path("/");
	of_property_read_string(node, "model", &soc_dev_attr->machine);
	of_node_put(node);

	s = rtd_soc_by_chip_id(chip_id);

	soc_dev_attr->family = kasprintf(GFP_KERNEL, "Realtek %s",
		(s && s->codename) ? s->codename :
		((s && s->family) ? s->family : "Digital Home Center"));

	if (likely(s && s->get_name))
		soc_dev_attr->soc_id = s->get_name(&pdev->dev, s);
	else
		soc_dev_attr->soc_id = "unknown";

	soc_dev_attr->revision = rtd_soc_rev(s, chip_rev);

	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR(soc_dev)) {
		kfree(soc_dev_attr->family);
		kfree(soc_dev_attr);
		return PTR_ERR(soc_dev);
	}

	platform_set_drvdata(pdev, soc_dev);

	pr_info("%s %s (0x%08x) rev %s (0x%08x) detected\n",
		soc_dev_attr->family, soc_dev_attr->soc_id, chip_id,
		soc_dev_attr->revision, chip_rev);

	return 0;
}

static int rtd_soc_remove(struct platform_device *pdev)
{
	struct soc_device *soc_dev = platform_get_drvdata(pdev);

	soc_device_unregister(soc_dev);

	return 0;
}

static const struct of_device_id rtd_soc_dt_ids[] = {
	 { .compatible = "realtek,rtd1195-chip" },
	 { }
};

static struct platform_driver rtd_soc_driver = {
	.probe = rtd_soc_probe,
	.remove = rtd_soc_remove,
	.driver = {
		.name = "rtd1195-soc",
		.of_match_table	= rtd_soc_dt_ids,
	},
};
module_platform_driver(rtd_soc_driver);

MODULE_DESCRIPTION("Realtek SoC identification");
MODULE_LICENSE("GPL");
