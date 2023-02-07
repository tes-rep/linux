// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Driver for WeTek Play2 Frontend
 *
 *  Copyright (2023): Christian Hewitt <christianshewitt@gmail.com>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/slab.h>

#include <linux/reset.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/usb.h>

#include <media/dvb_frontend.h>
#include "wetek.h"

struct dvb_wetek_fe_state {
	struct dvb_frontend frontend;
};


static int dvb_wetek_fe_read_status(struct dvb_frontend *fe,
				    enum fe_status *status)
{
	*status = FE_HAS_SIGNAL
		| FE_HAS_CARRIER
		| FE_HAS_VITERBI
		| FE_HAS_SYNC
		| FE_HAS_LOCK;

	return 0;
}

static int dvb_wetek_fe_read_ber(struct dvb_frontend *fe, u32 *ber)
{
	*ber = 0;
	return 0;
}

static int dvb_wetek_fe_read_signal_strength(struct dvb_frontend *fe,
					     u16 *strength)
{
	*strength = 0;
	return 0;
}

static int dvb_wetek_fe_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	*snr = 0;
	return 0;
}

static int dvb_wetek_fe_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	*ucblocks = 0;
	return 0;
}

/*
 * Should only be implemented if it actually reads something from the hardware.
 * Also, it should check for the locks, in order to avoid report wrong data
 * to userspace.
 */
static int dvb_wetek_fe_get_frontend(struct dvb_frontend *fe,
				     struct dtv_frontend_properties *p)
{
	return 0;
}

static int dvb_wetek_fe_set_frontend(struct dvb_frontend *fe)
{
	if (fe->ops.tuner_ops.set_params) {
		fe->ops.tuner_ops.set_params(fe);
		if (fe->ops.i2c_gate_ctrl)
			fe->ops.i2c_gate_ctrl(fe, 0);
	}

	return 0;
}

static int dvb_wetek_fe_sleep(struct dvb_frontend *fe)
{
	return 0;
}

static int dvb_wetek_fe_init(struct dvb_frontend *fe)
{
	return 0;
}

static int dvb_wetek_fe_set_tone(struct dvb_frontend *fe,
				 enum fe_sec_tone_mode tone)
{
	return 0;
}

static int dvb_wetek_fe_set_voltage(struct dvb_frontend *fe,
				    enum fe_sec_voltage voltage)
{
	return 0;
}

static void dvb_wetek_fe_release(struct dvb_frontend *fe)
{
	struct dvb_wetek_fe_state *state = fe->demodulator_priv;

	kfree(state);
}

static const struct dvb_frontend_ops dvb_wetek_fe_ofdm_ops;

struct dvb_frontend *dvb_wetek_fe_ofdm_attach(void)
{
	struct dvb_wetek_fe_state *state = NULL;

	/* allocate memory for the internal state */
	state = kzalloc(sizeof(struct dvb_wetek_fe_state), GFP_KERNEL);
	if (!state)
		return NULL;

	/* create dvb_frontend */
	memcpy(&state->frontend.ops,
	       &dvb_wetek_fe_ofdm_ops,
	       sizeof(struct dvb_frontend_ops));

	state->frontend.demodulator_priv = state;
	return &state->frontend;
}
EXPORT_SYMBOL(dvb_wetek_fe_ofdm_attach);

static const struct dvb_frontend_ops dvb_wetek_fe_qpsk_ops;

struct dvb_frontend *dvb_wetek_fe_qpsk_attach(void)
{
	struct dvb_wetek_fe_state *state = NULL;

	/* allocate memory for the internal state */
	state = kzalloc(sizeof(struct dvb_wetek_fe_state), GFP_KERNEL);
	if (!state)
		return NULL;

	/* create dvb_frontend */
	memcpy(&state->frontend.ops,
	       &dvb_wetek_fe_qpsk_ops,
	       sizeof(struct dvb_frontend_ops));

	state->frontend.demodulator_priv = state;
	return &state->frontend;
}
EXPORT_SYMBOL(dvb_wetek_fe_qpsk_attach);

static const struct dvb_frontend_ops dvb_wetek_fe_qam_ops;

struct dvb_frontend *dvb_wetek_fe_qam_attach(void)
{
	struct dvb_wetek_fe_state *state = NULL;

	/* allocate memory for the internal state */
	state = kzalloc(sizeof(struct dvb_wetek_fe_state), GFP_KERNEL);
	if (!state)
		return NULL;

	/* create dvb_frontend */
	memcpy(&state->frontend.ops,
	       &dvb_wetek_fe_qam_ops,
	       sizeof(struct dvb_frontend_ops));

	state->frontend.demodulator_priv = state;
	return &state->frontend;
}
EXPORT_SYMBOL(dvb_wetek_fe_qam_attach);

static const struct dvb_frontend_ops dvb_wetek_fe_ofdm_ops = {
	.delsys = { SYS_DVBT },
	.info = {
		.name			= "WeTek Play2 DVB-T",
		.frequency_min_hz	= 0,
		.frequency_max_hz	= 863250 * kHz,
		.frequency_stepsize_hz	= 62500,
		.caps = FE_CAN_FEC_1_2 |
			FE_CAN_FEC_2_3 |
			FE_CAN_FEC_3_4 |
			FE_CAN_FEC_4_5 |
			FE_CAN_FEC_5_6 |
			FE_CAN_FEC_6_7 |
			FE_CAN_FEC_7_8 |
			FE_CAN_FEC_8_9 |
			FE_CAN_FEC_AUTO |
			FE_CAN_QAM_16 |
			FE_CAN_QAM_64 |
			FE_CAN_QAM_AUTO |
			FE_CAN_TRANSMISSION_MODE_AUTO |
			FE_CAN_GUARD_INTERVAL_AUTO |
			FE_CAN_HIERARCHY_AUTO,
	},

	.release = dvb_wetek_fe_release,

	.init = dvb_wetek_fe_init,
	.sleep = dvb_wetek_fe_sleep,

	.set_frontend = dvb_wetek_fe_set_frontend,
	.get_frontend = dvb_wetek_fe_get_frontend,

	.read_status = dvb_wetek_fe_read_status,
	.read_ber = dvb_wetek_fe_read_ber,
	.read_signal_strength = dvb_wetek_fe_read_signal_strength,
	.read_snr = dvb_wetek_fe_read_snr,
	.read_ucblocks = dvb_wetek_fe_read_ucblocks,
};

static const struct dvb_frontend_ops dvb_wetek_fe_qam_ops = {
	.delsys = { SYS_DVBC_ANNEX_A },
	.info = {
		.name			= "WeTek Play2 DVB-C",
		.frequency_min_hz	=  51 * MHz,
		.frequency_max_hz	= 858 * MHz,
		.frequency_stepsize_hz	= 62500,
		/* symbol_rate_min: SACLK/64 == (XIN/2)/64 */
		.symbol_rate_min	= (57840000 / 2) / 64,
		.symbol_rate_max	= (57840000 / 2) / 4,   /* SACLK/4 */
		.caps = FE_CAN_QAM_16 |
			FE_CAN_QAM_32 |
			FE_CAN_QAM_64 |
			FE_CAN_QAM_128 |
			FE_CAN_QAM_256 |
			FE_CAN_FEC_AUTO |
			FE_CAN_INVERSION_AUTO
	},

	.release = dvb_wetek_fe_release,

	.init = dvb_wetek_fe_init,
	.sleep = dvb_wetek_fe_sleep,

	.set_frontend = dvb_wetek_fe_set_frontend,
	.get_frontend = dvb_wetek_fe_get_frontend,

	.read_status = dvb_wetek_fe_read_status,
	.read_ber = dvb_wetek_fe_read_ber,
	.read_signal_strength = dvb_wetek_fe_read_signal_strength,
	.read_snr = dvb_wetek_fe_read_snr,
	.read_ucblocks = dvb_wetek_fe_read_ucblocks,
};

static const struct dvb_frontend_ops dvb_wetek_fe_qpsk_ops = {
	.delsys = { SYS_DVBS },
	.info = {
		.name			= "WeTek Play2 DVB-S",
		.frequency_min_hz	=  950 * MHz,
		.frequency_max_hz	= 2150 * MHz,
		.frequency_stepsize_hz	= 250 * kHz,
		.frequency_tolerance_hz	= 29500 * kHz,
		.symbol_rate_min	= 1000000,
		.symbol_rate_max	= 45000000,
		.caps = FE_CAN_INVERSION_AUTO |
			FE_CAN_FEC_1_2 |
			FE_CAN_FEC_2_3 |
			FE_CAN_FEC_3_4 |
			FE_CAN_FEC_5_6 |
			FE_CAN_FEC_7_8 |
			FE_CAN_FEC_AUTO |
			FE_CAN_QPSK
	},

	.release = dvb_wetek_fe_release,

	.init = dvb_wetek_fe_init,
	.sleep = dvb_wetek_fe_sleep,

	.set_frontend = dvb_wetek_fe_set_frontend,
	.get_frontend = dvb_wetek_fe_get_frontend,

	.read_status = dvb_wetek_fe_read_status,
	.read_ber = dvb_wetek_fe_read_ber,
	.read_signal_strength = dvb_wetek_fe_read_signal_strength,
	.read_snr = dvb_wetek_fe_read_snr,
	.read_ucblocks = dvb_wetek_fe_read_ucblocks,

	.set_voltage = dvb_wetek_fe_set_voltage,
	.set_tone = dvb_wetek_fe_set_tone,
};

MODULE_DESCRIPTION("WeTek Play2 DVB Frontend");
MODULE_AUTHOR("Christian Hewitt <christianshewitt@gmail.com>");
MODULE_LICENSE("GPL");
