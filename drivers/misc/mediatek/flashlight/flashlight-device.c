// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "flashlight-core.h"

const struct flashlight_device_id flashlight_id[] = {
        /* {TYPE, CT, PART, "NAME", CHANNEL, DECOUPLE} */
        {0, 0, 0, "flashlights-lm3644", 0, 0},
	{0, 0, 0, "aw36518", 0, 0},
};

const int flashlight_device_num =
	sizeof(flashlight_id) / sizeof(struct flashlight_device_id);

