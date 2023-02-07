// SPDX-License-Identifier: GPL-2.0
/*
 *  Driver for WeTek Play2 Frontend
 *
 *  Copyright (2023): Christian Hewitt <christianshewitt@gmail.com>
 */

#ifndef DVB_WETEK_FE_H
#define DVB_WETEK_FE_H

#include <linux/dvb/frontend.h>
#include <media/dvb_frontend.h>

#if IS_REACHABLE(CONFIG_DVB_WETEK)
struct dvb_frontend *dvb_wetek_fe_ofdm_attach(void);
struct dvb_frontend *dvb_wetek_fe_qpsk_attach(void);
struct dvb_frontend *dvb_wetek_fe_qam_attach(void);
#else
static inline struct dvb_frontend *dvb_wetek_fe_ofdm_attach(void)
{
	pr_warn("%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
static inline struct dvb_frontend *dvb_wetek_fe_qpsk_attach(void)
{
	pr_warn("%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
static inline struct dvb_frontend *dvb_wetek_fe_qam_attach(void)
{
	pr_warn("%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif /* CONFIG_DVB_WETEK_FE */

#endif // DVB_WETEK_FE_H
