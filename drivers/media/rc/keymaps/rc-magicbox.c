// SPDX-License-Identifier: GPL-2.0+
//
// Copyright (C) 2022 Zhang Ning <zhangn1985@qq.com>

/*
 * Keytable for the Tmall MagicBox M16S Android Set-Top Box
 * Remote control. The key labelled 'M' is used as 'Magic'
 * in the vendor OS (online functions) and mapped to MUTE.
 */

#include <media/rc-map.h>
#include <linux/module.h>

static struct rc_map_table magicbox[] = {
	{ 0x9f57, KEY_POWER },
	{ 0x9f8a, KEY_MUTE }, // M (MAGIC)

	{ 0x9f43, KEY_UP },
	{ 0x9f0a, KEY_DOWN },
	{ 0x9f06, KEY_LEFT },
	{ 0x9f0e, KEY_RIGHT },
	{ 0x9f02, KEY_OK },

	{ 0x9f47, KEY_HOME },
	{ 0x9f4f, KEY_BACK },
	{ 0x9f16, KEY_MENU },

	{ 0x9fff, KEY_VOLUMEUP },
	{ 0x9f5d, KEY_VOLUMEDOWN },
};

static struct rc_map_list magicbox_map = {
	.map = {
		.scan     = magicbox,
		.size     = ARRAY_SIZE(magicbox),
		.rc_proto = RC_PROTO_NEC,
		.name     = RC_MAP_MAGICBOX,
	}
};

static int __init init_rc_map_magicbox(void)
{
	return rc_map_register(&magicbox_map);
}

static void __exit exit_rc_map_magicbox(void)
{
	rc_map_unregister(&magicbox_map);
}

module_init(init_rc_map_magicbox)
module_exit(exit_rc_map_magicbox)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhang Ning <zhangn1985@qq.com>");
