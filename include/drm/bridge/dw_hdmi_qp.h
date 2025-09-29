/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2021-2022 Rockchip Electronics Co., Ltd.
 * Copyright (c) 2024 Collabora Ltd.
 */

#ifndef __DW_HDMI_QP__
#define __DW_HDMI_QP__

#include <linux/types.h>

struct device;
struct drm_encoder;
struct dw_hdmi_qp;
struct platform_device;

struct dw_hdmi_qp_link_config {
	bool frl_enabled;
	u8 frl_rate_forced;
	u8 frl_rate_per_lane;
	u8 frl_lanes;
};

struct dw_hdmi_qp_phy_ops {
	int (*init)(struct dw_hdmi_qp *hdmi, void *data);
	void (*disable)(struct dw_hdmi_qp *hdmi, void *data);
	enum drm_connector_status (*read_hpd)(struct dw_hdmi_qp *hdmi, void *data);
	void (*setup_hpd)(struct dw_hdmi_qp *hdmi, void *data);
	struct dw_hdmi_qp_link_config *(*get_link_cfg)(struct dw_hdmi_qp *hdmi, void *data);
	int (*force_link_rate)(struct dw_hdmi_qp *hdmi, void *data, u8 frl_rate);
};

struct dw_hdmi_qp_plat_data {
	const struct dw_hdmi_qp_phy_ops *phy_ops;
	void *phy_data;
	int main_irq;
	int cec_irq;
	unsigned long ref_clk_rate;
	/* Supported output formats: bitmask of @hdmi_colorspace */
	unsigned int supported_formats;
	/* Maximum bits per color channel: 8, 10 or 12 */
	unsigned int max_bpc;
};

struct dw_hdmi_qp *dw_hdmi_qp_bind(struct platform_device *pdev,
				   struct drm_encoder *encoder,
				   const struct dw_hdmi_qp_plat_data *plat_data);
void dw_hdmi_qp_resume(struct device *dev, struct dw_hdmi_qp *hdmi);
#endif /* __DW_HDMI_QP__ */
