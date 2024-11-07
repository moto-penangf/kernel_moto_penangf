/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef CHOPDETECTHUB_H
#define CHOPDETECTHUB_H

#include <linux/ioctl.h>
#include <linux/init.h>

int __init chopdetecthub_init(void);
void __exit chopdetecthub_exit(void);

#endif
