// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Wetek NIM Tuner(s) Detection
 *
 * Copyright (C) 2014 Sasa Savic <sasa.savic.sr@gmail.com>
 *
 */

#ifndef __NIMDETECT_H
#define __NIMDETECT_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/dvb/version.h>
#include <linux/platform_device.h>
#include <media/dvb_frontend.h>

struct ts_input {
	int			mode;
	struct pinctrl		*pinctrl;
	int			control;
};

struct wetek_nims {
	struct dvb_frontend 	*fe[2];
	struct i2c_adapter 	*i2c[2];
	struct ts_input	   	ts[3];
	struct device       	*dev;
	struct platform_device  *pdev;
	struct pinctrl      	*card_pinctrl;
	u32 total_nims;
	int fec_reset;
	int power_ctrl;
};

void get_nims_infos(struct wetek_nims *p);
int set_external_vol_gpio(int *demod_id, int on);

#endif /* __NIMDETECT_H */
