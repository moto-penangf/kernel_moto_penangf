// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

/*Penang Code for EKPENAN4GU-3013 by chenxiaoyong at 20240321 start*/
extern unsigned int gc02m1_read_otpdata(struct i2c_client *client, unsigned int addr, unsigned char *data, unsigned int size);
extern unsigned int ov02b10_read_otpdata(struct i2c_client *client, unsigned int addr, unsigned char *data, unsigned int size);
extern unsigned int s5k4h7yx03_read_region(struct i2c_client *client, unsigned int addr, unsigned char *data, unsigned int size);
/*Penang Code for EKPENAN4GU-3013 by chenxiaoyong at 20240321 end*/

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */
	{IMX519_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2T7SP_SENSOR_ID, 0xA4, Common_read_region},
	{IMX338_SENSOR_ID, 0xA0, Common_read_region},
	{S5K4E6_SENSOR_ID, 0xA8, Common_read_region},
	{IMX386_SENSOR_ID, 0xA0, Common_read_region},
	{S5K3M3_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2L7_SENSOR_ID, 0xA0, Common_read_region},
	{IMX398_SENSOR_ID, 0xA0, Common_read_region},
	{IMX350_SENSOR_ID, 0xA0, Common_read_region},
	{IMX318_SENSOR_ID, 0xA0, Common_read_region},
	{IMX386_MONO_SENSOR_ID, 0xA0, Common_read_region},
	/*B+B. No Cal data for main2 OV8856*/
	{S5K2P7_SENSOR_ID, 0xA0, Common_read_region},
	/*Penang Code OTP bringup，modify by wangyaohua 20240304 start*/
	{S5KJN1SQ03_SENSOR_ID, 0xA0, Common_read_region},
	{HI1634Q_SENSOR_ID, 0xA2, Common_read_region},
	/*Penang Code for EKPENAN4GU-3013 by chenxiaoyong at 20240321 start*/
	{S5K4H7YX03_SENSOR_ID, 0x5A, s5k4h7yx03_read_region},
	{GC02M1_SENSOR_ID, 0x6E, gc02m1_read_otpdata},
	{OV02B10_SENSOR_ID, 0x78, ov02b10_read_otpdata},
	/*Penang Code for EKPENAN4GU-3013 by chenxiaoyong at 20240321 end*/
	{HI556_SENSOR_ID, 0xA4, Common_read_region},
	{OV05A10_SENSOR_ID, 0xA4, Common_read_region},
	/*Penang Code OTP bringup，modify by wangyaohua 20240304 end*/
#ifdef SUPPORT_S5K4H7
	{S5K4H7_SENSOR_ID, 0xA0, zte_s5k4h7_read_region},
	{S5K4H7SUB_SENSOR_ID, 0xA0, zte_s5k4h7_sub_read_region},
#endif
	/*  ADD before this line */
	{0, 0, 0}       /*end of list */
};

unsigned int cam_cal_get_sensor_list(
	struct stCAM_CAL_LIST_STRUCT **ppCamcalList)
{
	if (ppCamcalList == NULL)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}


