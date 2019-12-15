// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Titan Micro Electronics TM1628 LED controller
 * Also compatible:
 * Fuda Hisi Microelectronics FD628
 * Fude Microelectronics AiP1618
 *
 * Copyright (c) 2019 Andreas Färber
 */

#include <linux/bitops.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/pwm.h>
#include <linux/spi/spi.h>

#define TM1628_CMD_MASK			GENMASK(7, 6)
#define TM1628_CMD_DISPLAY_MODE		(0x0 << 6)
#define TM1628_CMD_DATA_SETTING		(0x1 << 6)
#define TM1628_CMD_DISPLAY_CTRL		(0x2 << 6)
#define TM1628_CMD_ADDRESS_SETTING	(0x3 << 6)

#define TM1628_DISPLAY_MODE_MODE_MASK	GENMASK(1, 0)

#define TM1628_DATA_SETTING_MODE_MASK	GENMASK(1, 0)
#define TM1628_DATA_SETTING_WRITE_DATA	0x0
#define TM1628_DATA_SETTING_READ_DATA	0x2
#define TM1628_DATA_SETTING_FIXED_ADDR	BIT(2)
#define TM1628_DATA_SETTING_TEST_MODE	BIT(3)

#define TM1628_DISPLAY_CTRL_PW_MASK	GENMASK(2, 0)

#define TM1628_DISPLAY_CTRL_DISPLAY_ON	BIT(3)

struct tm1628_mode {
	u8	grid_mask;
	u16	seg_mask;
};

struct tm1628_info {
	u8				grid_mask;
	u16				seg_mask;
	const struct tm1628_mode	*modes;
	int				default_mode;
	const struct pwm_capture	*pwm_map;
	int				default_pwm;
};

struct tm1628_segment {
	u32 grid;
	u32 seg;
};

struct tm1628_display {
	struct tm1628_segment segments[8];
};

struct tm1628_led {
	struct led_classdev	leddev;
	struct tm1628		*ctrl;
	u32			grid;
	u32			seg;
};

struct tm1628 {
	struct spi_device		*spi;
	const struct tm1628_info	*info;
	u32				grids;
	unsigned int			segments;
	int				mode_index;
	int				pwm_index;
	u8				data[14];
	unsigned int			num_displays;
	struct tm1628_display		*displays;
	unsigned int			num_leds;
	struct tm1628_led		leds[];
};

/* Command 1: Display Mode Setting */
static int tm1628_set_display_mode(struct spi_device *spi, u8 grid_mode)
{
	u8 cmd = TM1628_CMD_DISPLAY_MODE;

	if (unlikely(grid_mode & ~TM1628_DISPLAY_MODE_MODE_MASK))
		return -EINVAL;

	cmd |= grid_mode;

	return spi_write(spi, &cmd, 1);
}

/* Command 2: Data Setting */
static int tm1628_write_data(struct spi_device *spi, const u8 *data, unsigned int len)
{
	u8 cmd = TM1628_CMD_DATA_SETTING | TM1628_DATA_SETTING_WRITE_DATA;
	struct spi_transfer xfers[] = {
		{
			.tx_buf = &cmd,
			.len = 1,
		},
		{
			.tx_buf = data,
			.len = len,
		},
	};

	if (len > 14)
		return -EINVAL;

	return spi_sync_transfer(spi, xfers, ARRAY_SIZE(xfers));
}

/* Command 3: Address Setting */
static int tm1628_set_address(struct spi_device *spi, u8 addr)
{
	u8 cmd = TM1628_CMD_ADDRESS_SETTING;

	cmd |= (addr & GENMASK(3, 0));

	return spi_write(spi, &cmd, 1);
}

/* Command 4: Display Control */
static int tm1628_set_display_ctrl(struct spi_device *spi, bool on, u8 pwm_index)
{
	u8 cmd = TM1628_CMD_DISPLAY_CTRL;

	if (on)
		cmd |= TM1628_DISPLAY_CTRL_DISPLAY_ON;

	if (pwm_index & ~TM1628_DISPLAY_CTRL_PW_MASK)
		return -EINVAL;

	cmd |= pwm_index;

	return spi_write(spi, &cmd, 1);
}

static inline bool tm1628_is_valid_grid(struct tm1628 *s, unsigned int grid)
{
	return s->info->modes[s->mode_index].grid_mask & BIT(grid);
}

static inline bool tm1628_is_valid_seg(struct tm1628 *s, unsigned int seg)
{
	return s->info->modes[s->mode_index].seg_mask & BIT(seg);
}

static int tm1628_get_led_offset(struct tm1628 *s,
	unsigned int grid, unsigned int seg, int *poffset, int *pbit)
{
	int offset, bit;

	if (grid == 0 || grid > 7 || seg == 0 || seg > 16)
		return -EINVAL;

	offset = (grid - 1) * 2;
	bit = seg - 1;
	if (bit >= 8) {
		bit -= 8;
		offset++;
	}

	*poffset = offset;
	if (pbit)
		*pbit = bit;

	return 0;
}

static int tm1628_get_led(struct tm1628 *s,
	unsigned int grid, unsigned int seg, bool *on)
{
	int offset, bit;
	int ret;

	ret = tm1628_get_led_offset(s, grid, seg, &offset, &bit);
	if (ret)
		return ret;

	*on = !!(s->data[offset] & BIT(bit));

	return 0;
}

static int tm1628_set_led(struct tm1628 *s,
	unsigned int grid, unsigned int seg, bool on)
{
	int offset, bit;
	int ret;

	ret = tm1628_get_led_offset(s, grid, seg, &offset, &bit);
	if (ret)
		return ret;

	if (on)
		s->data[offset] |=  BIT(bit);
	else
		s->data[offset] &= ~BIT(bit);

	return 0;
}

static int tm1628_led_set_brightness(struct led_classdev *led_cdev,
	enum led_brightness brightness)
{
	struct tm1628_led *led = container_of(led_cdev, struct tm1628_led, leddev);
	struct tm1628 *s = led->ctrl;
	int ret, offset;

	ret = tm1628_set_led(s, led->grid, led->seg, brightness != LED_OFF);
	if (ret)
		return ret;

	ret = tm1628_get_led_offset(s, led->grid, led->seg, &offset, NULL);
	if (unlikely(ret))
		return ret;

	ret = tm1628_set_address(s->spi, offset);
	if (ret)
		return ret;

	return tm1628_write_data(s->spi, s->data + offset, 1);
}

static enum led_brightness tm1628_led_get_brightness(struct led_classdev *led_cdev)
{
	struct tm1628_led *led = container_of(led_cdev, struct tm1628_led, leddev);
	struct tm1628 *s = led->ctrl;
	bool on;
	int ret;

	ret = tm1628_get_led(s, led->grid, led->seg, &on);
	if (ret)
		return ret;

	return on ? LED_ON : LED_OFF;
}

static int tm1628_register_led(struct tm1628 *s,
	struct fwnode_handle *node, u32 grid, u32 seg, struct tm1628_led *led)
{
	struct device *dev = &s->spi->dev;
	struct led_init_data init_data = {0};

	if (!tm1628_is_valid_grid(s, grid) || !tm1628_is_valid_seg(s, seg)) {
		dev_warn(dev, "%s reg out of range\n", fwnode_get_name(node));
		return -EINVAL;
	}

	led->ctrl = s;
	led->grid = grid;
	led->seg  = seg;
	led->leddev.max_brightness = LED_ON;
	led->leddev.brightness_set_blocking = tm1628_led_set_brightness;
	led->leddev.brightness_get = tm1628_led_get_brightness;

	fwnode_property_read_string(node, "linux,default-trigger", &led->leddev.default_trigger);

	init_data.fwnode = node;
	init_data.devicename = "tm1628";

	return devm_led_classdev_register_ext(dev, &led->leddev, &init_data);
}

#define SSD_TOP			BIT(0)
#define SSD_TOP_RIGHT		BIT(1)
#define SSD_BOTTOM_RIGHT	BIT(2)
#define SSD_BOTTOM		BIT(3)
#define SSD_BOTTOM_LEFT		BIT(4)
#define SSD_TOP_LEFT		BIT(5)
#define SSD_MIDDLE		BIT(6)
#define SSD_DOT			BIT(7)

struct tm1628_ssd_char {
	char ch;
	u8 segs;
};

static const struct tm1628_ssd_char tm1628_char_ssd_map[] = {
	{ '0', SSD_TOP | SSD_TOP_LEFT | SSD_TOP_RIGHT |
	       SSD_BOTTOM_LEFT | SSD_BOTTOM_RIGHT | SSD_BOTTOM },
	{ '1', SSD_TOP_RIGHT | SSD_BOTTOM_RIGHT },
	{ '2', SSD_TOP | SSD_TOP_RIGHT | SSD_MIDDLE | SSD_BOTTOM_LEFT | SSD_BOTTOM },
	{ '3', SSD_TOP | SSD_TOP_RIGHT | SSD_MIDDLE | SSD_BOTTOM_RIGHT | SSD_BOTTOM },
	{ '4', SSD_TOP_LEFT | SSD_TOP_RIGHT | SSD_MIDDLE | SSD_BOTTOM_RIGHT },
	{ '5', SSD_TOP | SSD_TOP_LEFT | SSD_MIDDLE | SSD_BOTTOM_RIGHT | SSD_BOTTOM },
	{ '6', SSD_TOP | SSD_TOP_LEFT | SSD_MIDDLE | SSD_BOTTOM_LEFT | SSD_BOTTOM_RIGHT | SSD_BOTTOM },
	{ '7', SSD_TOP | SSD_TOP_RIGHT | SSD_BOTTOM_RIGHT },
	{ '8', SSD_TOP | SSD_TOP_LEFT | SSD_TOP_RIGHT | SSD_MIDDLE |
	       SSD_BOTTOM_LEFT | SSD_BOTTOM_RIGHT | SSD_BOTTOM },
	{ '9', SSD_TOP | SSD_TOP_LEFT | SSD_TOP_RIGHT | SSD_MIDDLE |
	       SSD_BOTTOM_RIGHT | SSD_BOTTOM },
	{ 'A', SSD_TOP | SSD_TOP_LEFT | SSD_TOP_RIGHT | SSD_MIDDLE |
	       SSD_BOTTOM_LEFT | SSD_BOTTOM_RIGHT },
	{ 'B', SSD_TOP | SSD_TOP_LEFT | SSD_TOP_RIGHT | SSD_MIDDLE |
	       SSD_BOTTOM_LEFT | SSD_BOTTOM_RIGHT | SSD_BOTTOM },
	{ 'C', SSD_TOP | SSD_TOP_LEFT | SSD_BOTTOM_LEFT | SSD_BOTTOM },
	{ 'D', SSD_TOP | SSD_TOP_LEFT | SSD_TOP_RIGHT |
	       SSD_BOTTOM_LEFT | SSD_BOTTOM_RIGHT | SSD_BOTTOM },
	{ 'E', SSD_TOP | SSD_TOP_LEFT | SSD_MIDDLE | SSD_BOTTOM_LEFT | SSD_BOTTOM },
	{ 'F', SSD_TOP | SSD_TOP_LEFT | SSD_MIDDLE | SSD_BOTTOM_LEFT },
	{ 'G', SSD_TOP | SSD_TOP_LEFT | SSD_BOTTOM_LEFT | SSD_BOTTOM | SSD_BOTTOM_RIGHT },
	{ 'H', SSD_TOP_LEFT | SSD_TOP_RIGHT | SSD_MIDDLE |
	       SSD_BOTTOM_LEFT | SSD_BOTTOM_RIGHT },
	{ 'I', SSD_TOP_LEFT | SSD_BOTTOM_LEFT },
	{ 'J', SSD_TOP | SSD_TOP_RIGHT | SSD_BOTTOM_RIGHT | SSD_BOTTOM },
	{ 'K', SSD_TOP_LEFT | SSD_TOP_RIGHT | SSD_MIDDLE |
	       SSD_BOTTOM_LEFT | SSD_BOTTOM_RIGHT },
	{ 'L', SSD_TOP_LEFT | SSD_BOTTOM_LEFT | SSD_BOTTOM },
	{ 'O', SSD_TOP | SSD_TOP_LEFT | SSD_TOP_RIGHT |
	       SSD_BOTTOM_LEFT | SSD_BOTTOM_RIGHT | SSD_BOTTOM },
	{ 'P', SSD_TOP | SSD_TOP_LEFT | SSD_TOP_RIGHT | SSD_MIDDLE |
	       SSD_BOTTOM_LEFT },
	{ 'Q', SSD_TOP | SSD_TOP_LEFT | SSD_TOP_RIGHT |
	       SSD_BOTTOM_LEFT | SSD_BOTTOM_RIGHT | SSD_BOTTOM },
	{ 'R', SSD_TOP | SSD_TOP_LEFT | SSD_TOP_RIGHT | SSD_MIDDLE |
	       SSD_BOTTOM_LEFT | SSD_BOTTOM_RIGHT },
	{ 'S', SSD_TOP | SSD_TOP_LEFT | SSD_MIDDLE | SSD_BOTTOM_RIGHT | SSD_BOTTOM },
	{ 'T', SSD_TOP | SSD_TOP_LEFT | SSD_BOTTOM_LEFT },
	{ 'U', SSD_TOP_LEFT | SSD_BOTTOM_LEFT | SSD_BOTTOM | SSD_BOTTOM_RIGHT | SSD_TOP_RIGHT },
	{ 'V', SSD_TOP_LEFT | SSD_BOTTOM_LEFT | SSD_BOTTOM | SSD_BOTTOM_RIGHT | SSD_TOP_RIGHT },
	{ 'W', SSD_TOP_LEFT | SSD_BOTTOM_LEFT | SSD_BOTTOM | SSD_BOTTOM_RIGHT | SSD_TOP_RIGHT },
	{ 'Z', SSD_TOP | SSD_TOP_RIGHT | SSD_MIDDLE | SSD_BOTTOM_LEFT | SSD_BOTTOM },
	{ 'b', SSD_TOP_LEFT | SSD_MIDDLE | SSD_BOTTOM_LEFT | SSD_BOTTOM | SSD_BOTTOM_RIGHT },
	{ 'l', SSD_TOP_LEFT | SSD_BOTTOM_LEFT },
	{ 'o', SSD_MIDDLE | SSD_BOTTOM_LEFT | SSD_BOTTOM | SSD_BOTTOM_RIGHT },
	{ 't', SSD_TOP_LEFT | SSD_MIDDLE | SSD_BOTTOM_LEFT | SSD_BOTTOM },
	{ 'u', SSD_BOTTOM_LEFT | SSD_BOTTOM | SSD_BOTTOM_RIGHT },
	{ 'v', SSD_BOTTOM_LEFT | SSD_BOTTOM | SSD_BOTTOM_RIGHT },
	{ 'w', SSD_BOTTOM_LEFT | SSD_BOTTOM | SSD_BOTTOM_RIGHT },
	{ '-', SSD_MIDDLE },
	{ '_', SSD_BOTTOM },
	{ '.', SSD_DOT },
};

static u8 tm1628_get_char_ssd_map(char ch)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tm1628_char_ssd_map); i++) {
		if (tm1628_char_ssd_map[i].ch == ch)
			return tm1628_char_ssd_map[i].segs;
	}

	return 0x0;
}

static int tm1628_display_apply_map(struct tm1628 *s,
	struct tm1628_display *display, u8 map)
{
	struct tm1628_segment *segment;
	int i;

	for (i = 0; i < 8; i++) {
		segment = &display->segments[i];
		tm1628_set_led(s, segment->grid, segment->seg, map & BIT(i));
	}

	return 0;
}

static ssize_t text_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct tm1628 *s = dev_get_drvdata(dev);
	size_t offset, len = count;
	u8 map;
	int i, ret;

	if (len > 0 && buf[len - 1] == '\n')
		len--;

	for (i = 0, offset = 0; i < s->num_displays; i++) {
		if (offset < len) {
			map = tm1628_get_char_ssd_map(buf[offset]);
			offset++;
		} else
			map = 0x0;

		tm1628_display_apply_map(s, &s->displays[i], map);
	}

	ret = tm1628_set_address(s->spi, 0x0);
	if (ret)
		return ret;

	ret = tm1628_write_data(s->spi, s->data, 14);
	if (ret)
		return ret;

	return count;
}

static struct device_attribute tm1628_attr =
	__ATTR_WO(text);

static int tm1628_register_display(struct tm1628 *s,
	struct fwnode_handle *node)
{
	struct device *dev = &s->spi->dev;
	struct tm1628_display *display;
	u32 *reg;
	u32 grid, seg;
	int i, j, ret, reg_count;

	reg_count = fwnode_property_count_u32(node, "reg");
	if (reg_count < 0)
		return reg_count;

	if (reg_count % 2) {
		dev_warn(dev, "Ignoring extra cell in %s reg property\n",
			 fwnode_get_name(node));
		reg_count--;
	}

	if (s->displays) {
		dev_warn(dev, "Only one display supported\n");
		return -EINVAL;
	}

	s->num_displays = reg_count >> 1;

	reg = devm_kzalloc(dev, reg_count * sizeof(*reg), GFP_KERNEL);
	if (!reg)
		return -ENOMEM;

	ret = fwnode_property_read_u32_array(node, "reg", reg, reg_count);
	if (ret) {
		dev_err(dev, "Reading %s reg property failed (%d)\n",
			fwnode_get_name(node), ret);
		return ret;
	}

	s->displays = devm_kzalloc(dev, s->num_displays * sizeof(*s->displays), GFP_KERNEL);
	if (!s->displays)
		return -ENOMEM;

	for (i = 0; i < s->num_displays; i++) {
		display = &s->displays[i];
		grid = reg[i * 2];
		seg  = reg[i * 2 + 1];
		if (grid == 0 && seg != 0) {
			if (!tm1628_is_valid_seg(s, seg)) {
				dev_warn(dev, "%s reg out of range\n", fwnode_get_name(node));
				return -EINVAL;
			}
			grid = s->info->modes[s->mode_index].grid_mask;
			for (j = 0; grid && j < 7; j++) {
				display->segments[j].seg = seg;
				display->segments[j].grid = __ffs(grid);
				grid &= ~BIT(display->segments[j].grid);
			}
		} else if (grid != 0 && seg == 0) {
			if (!tm1628_is_valid_grid(s, grid)) {
				dev_warn(dev, "%s reg out of range\n", fwnode_get_name(node));
				return -EINVAL;
			}
			seg = s->info->modes[s->mode_index].seg_mask;
			for (j = 0; seg && j < 8; j++) {
				display->segments[j].grid = grid;
				display->segments[j].seg = __ffs(seg);
				seg &= ~BIT(display->segments[j].seg);
			}
		}
	}

	devm_kfree(dev, reg);

	device_create_file(dev, &tm1628_attr);

	return 0;
}

/* Work around __builtin_popcount() */
static u32 tm1628_grid_popcount(u8 grid_mask)
{
	int i, n = 0;

	while (grid_mask) {
		i = __ffs(grid_mask);
		grid_mask &= ~BIT(i);
		n++;
	}

	return n;
}

static int tm1628_spi_probe(struct spi_device *spi)
{
	struct tm1628 *s;
	struct fwnode_handle *child;
	u32 grids;
	u32 reg[2];
	size_t leds;
	int ret, i;

	leds = device_get_child_node_count(&spi->dev);

	s = devm_kzalloc(&spi->dev, struct_size(s, leds, leds), GFP_KERNEL);
	if (!s)
		return -ENOMEM;

	s->spi = spi;

	s->info = device_get_match_data(&spi->dev);
	if (!s->info)
		return -EINVAL;

	s->pwm_index = s->info->default_pwm;

	ret = tm1628_set_display_ctrl(spi, false, s->pwm_index);
	if (ret) {
		dev_err(&spi->dev, "Turning display off failed (%d)\n", ret);
		return ret;
	}

	ret = device_property_read_u32(&spi->dev, "#grids", &grids);
	if (ret && ret != -EINVAL) {
		dev_err(&spi->dev, "Error reading #grids property (%d)\n", ret);
		return ret;
	}

	s->mode_index = -1;
	for (i = 0; i < 4; i++) {
		if (tm1628_grid_popcount(s->info->modes[i].grid_mask) != grids)
			continue;
		s->mode_index = i;
		break;
	}
	if (s->mode_index == -1) {
		dev_err(&spi->dev, "#grids out of range (%u)\n", grids);
		return -EINVAL;
	}

	spi_set_drvdata(spi, s);

	device_for_each_child_node(&spi->dev, child) {
		ret = fwnode_property_read_u32_array(child, "reg", reg, 2);
		if (ret) {
			dev_err(&spi->dev, "Reading %s reg property failed (%d)\n",
				fwnode_get_name(child), ret);
			fwnode_handle_put(child);
			return ret;
		}

		if (reg[0] != 0 && reg[1] != 0 && fwnode_property_count_u32(child, "reg") == 2) {
			ret = tm1628_register_led(s, child, reg[0], reg[1], &s->leds[i++]);
			if (ret && ret != -EINVAL) {
				dev_err(&spi->dev, "Failed to register LED %s (%d)\n",
					fwnode_get_name(child), ret);
				fwnode_handle_put(child);
				return ret;
			}
			s->num_leds++;
		} else {
			ret = tm1628_register_display(s, child);
			if (ret) {
				dev_err(&spi->dev, "Failed to register display %s (%d)\n",
					fwnode_get_name(child), ret);
				fwnode_handle_put(child);
				return ret;
			}
		}
	}

	ret = tm1628_set_address(spi, 0x0);
	if (ret) {
		dev_err(&spi->dev, "Setting address failed (%d)\n", ret);
		return ret;
	}

	ret = tm1628_write_data(spi, s->data, sizeof(s->data));
	if (ret) {
		dev_err(&spi->dev, "Writing data failed (%d)\n", ret);
		return ret;
	}

	ret = tm1628_set_display_mode(spi, s->mode_index);
	if (ret) {
		dev_err(&spi->dev, "Setting display mode failed (%d)\n", ret);
		return ret;
	}

	ret = tm1628_set_display_ctrl(spi, true, s->pwm_index);
	if (ret) {
		dev_err(&spi->dev, "Turning display on failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

static const struct pwm_capture tm1628_pwm_map[8] = {
	{ .duty_cycle =  1, .period = 16 },
	{ .duty_cycle =  2, .period = 16 },
	{ .duty_cycle =  4, .period = 16 },
	{ .duty_cycle = 10, .period = 16 },
	{ .duty_cycle = 11, .period = 16 },
	{ .duty_cycle = 12, .period = 16 },
	{ .duty_cycle = 13, .period = 16 },
	{ .duty_cycle = 14, .period = 16 },
};

static const struct tm1628_mode tm1628_modes[4] = {
	{
		.grid_mask = GENMASK(4, 1),
		.seg_mask = GENMASK(14, 12) | GENMASK(10, 1),
	},
	{
		.grid_mask = GENMASK(5, 1),
		.seg_mask = GENMASK(13, 12) | GENMASK(10, 1),
	},
	{
		.grid_mask = GENMASK(6, 1),
		.seg_mask = BIT(12) | GENMASK(10, 1),
	},
	{
		.grid_mask = GENMASK(7, 1),
		.seg_mask = GENMASK(10, 1),
	},
};

static const struct tm1628_info tm1628_info = {
	.grid_mask = GENMASK(7, 1),
	.seg_mask = GENMASK(14, 12) | GENMASK(10, 1),
	.modes = tm1628_modes,
	.default_mode = 3,
	.pwm_map = tm1628_pwm_map,
	.default_pwm = 0,
};

static const struct tm1628_info fd628_info = {
	.grid_mask = GENMASK(7, 1),
	.seg_mask = GENMASK(14, 12) | GENMASK(10, 1),
	.modes = tm1628_modes,
	.default_mode = 3,
	.pwm_map = tm1628_pwm_map,
	.default_pwm = 0,
};

static const struct tm1628_mode aip1618_modes[4] = {
	{
		.grid_mask = GENMASK(4, 1),
		.seg_mask = GENMASK(8, 1),
	},
	{
		.grid_mask = GENMASK(5, 1),
		.seg_mask = GENMASK(7, 1),
	},
	{
		.grid_mask = GENMASK(6, 1),
		.seg_mask = GENMASK(6, 1),
	},
	{
		.grid_mask = GENMASK(7, 1),
		.seg_mask = GENMASK(5, 1),
	},
};

static const struct tm1628_info aip1618_info = {
	.grid_mask = GENMASK(7, 1),
	.seg_mask = GENMASK(14, 12) | GENMASK(5, 1),
	.modes = aip1618_modes,
	.default_mode = 3,
	.pwm_map = tm1628_pwm_map,
	.default_pwm = 0,
};

static const struct of_device_id tm1628_spi_of_matches[] = {
	{ .compatible = "titanmec,tm1628", .data = &tm1628_info },
	{ .compatible = "fdhisi,fd628", .data = &fd628_info },
	{ .compatible = "szfdwdz,aip1618", .data = &aip1618_info },
	{}
};
MODULE_DEVICE_TABLE(of, tm1628_spi_of_matches);

static struct spi_driver tm1628_spi_driver = {
	.probe = tm1628_spi_probe,
	.driver = {
		.name = "tm1628",
		.of_match_table = tm1628_spi_of_matches,
	},
};
module_spi_driver(tm1628_spi_driver);

MODULE_DESCRIPTION("TM1628 LED controller driver");
MODULE_AUTHOR("Andreas Färber");
MODULE_LICENSE("GPL");
