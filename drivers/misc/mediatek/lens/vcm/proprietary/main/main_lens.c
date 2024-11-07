// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * MAIN AF voice coil motor driver
 *
 *
 */

#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

/* kernel standard */
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>

/* OIS/EIS Timer & Workqueue */
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/ktime.h>
/* ------------------------- */

#include "lens_info.h"
#include "lens_list.h"

/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 start*/
#include "cam_vcm_notify.h"
#include "vibrator_notify.h"
#include <linux/pm_wakeup.h>
/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 end*/

#define AF_DRVNAME "MAINAF"

#if defined(CONFIG_MTK_LEGACY)
#define I2C_CONFIG_SETTING 1
#elif defined(CONFIG_OF)
#define I2C_CONFIG_SETTING 2 /* device tree */
#else

#define I2C_CONFIG_SETTING 1
#endif

#if I2C_CONFIG_SETTING == 1
#define LENS_I2C_BUSNUM 0
#define I2C_REGISTER_ID 0x28
#endif

#define PLATFORM_DRIVER_NAME "lens_actuator_main_af"
#define AF_DRIVER_CLASS_NAME "actuatordrv_main_af"

#if I2C_CONFIG_SETTING == 1
static struct i2c_board_info kd_lens_dev __initdata = {
	I2C_BOARD_INFO(AF_DRVNAME, I2C_REGISTER_ID)};
#endif

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...)                                               \
	pr_info(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

/* OIS/EIS Timer & Workqueue */
static struct workqueue_struct *ois_workqueue;
static struct work_struct ois_work;
static struct hrtimer ois_timer;

static DEFINE_MUTEX(ois_mutex);
static int g_EnableTimer;
static int g_GetOisInfoCnt;
static int g_OisPosIdx;
static struct stAF_OisPosInfo OisPosInfo;
/* ------------------------- */
/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 start*/
static struct work_struct vib_off_work;
static void vib_off_work_callback(struct work_struct *data);
static struct work_struct vib_on_work;
static void vib_on_work_callback(struct work_struct *data);
static struct hrtimer vcm_timer;
static int g_EnableVcmTimer;
static enum hrtimer_restart vcm_timer_func(struct hrtimer *timer);
static struct wakeup_source *vcm_wakelock;
/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 end*/

static struct stAF_DrvList g_stAF_DrvList[MAX_NUM_OF_LENS] = {
	{1, AFDRV_DW9718TAF, DW9718TAF_SetI2Cclient, DW9718TAF_Ioctl,
	 DW9718TAF_Release, DW9718TAF_GetFileName, NULL},
	{1, AFDRV_AK7371AF, AK7371AF_SetI2Cclient, AK7371AF_Ioctl,
	 AK7371AF_Release, AK7371AF_GetFileName, NULL},
	{1, AFDRV_BU6424AF, BU6424AF_SetI2Cclient, BU6424AF_Ioctl,
	 BU6424AF_Release, BU6424AF_GetFileName, NULL},
	{1, AFDRV_BU6429AF, BU6429AF_SetI2Cclient, BU6429AF_Ioctl,
	 BU6429AF_Release, BU6429AF_GetFileName, NULL},
	{1, AFDRV_BU64748AF, bu64748af_SetI2Cclient_Main, bu64748af_Ioctl_Main,
	 bu64748af_Release_Main, bu64748af_GetFileName_Main, NULL},
	{1, AFDRV_BU64253GWZAF, BU64253GWZAF_SetI2Cclient, BU64253GWZAF_Ioctl,
	 BU64253GWZAF_Release, BU64253GWZAF_GetFileName, NULL},
	{1,
#ifdef CONFIG_MTK_LENS_BU63165AF_SUPPORT
	 AFDRV_BU63165AF, BU63165AF_SetI2Cclient, BU63165AF_Ioctl,
	 BU63165AF_Release, BU63165AF_GetFileName, NULL
#else
	 AFDRV_BU63169AF, BU63169AF_SetI2Cclient, BU63169AF_Ioctl,
	 BU63169AF_Release, BU63169AF_GetFileName, NULL
#endif
	},
	{1, AFDRV_DW9714AF, DW9714AF_SetI2Cclient, DW9714AF_Ioctl,
	 DW9714AF_Release, DW9714AF_GetFileName, NULL},
	{1, AFDRV_DW9718SAF, DW9718SAF_SetI2Cclient, DW9718SAF_Ioctl,
	 DW9718SAF_Release, DW9718SAF_GetFileName, NULL},
	{1, AFDRV_DW9719TAF, DW9719TAF_SetI2Cclient, DW9719TAF_Ioctl,
	 DW9719TAF_Release, DW9719TAF_GetFileName, NULL},
	{1, AFDRV_DW9763AF, DW9763AF_SetI2Cclient, DW9763AF_Ioctl,
	 DW9763AF_Release, DW9763AF_GetFileName, NULL},
	{1, AFDRV_LC898212XDAF, LC898212XDAF_SetI2Cclient, LC898212XDAF_Ioctl,
	 LC898212XDAF_Release, LC898212XDAF_GetFileName, NULL},
	{1, AFDRV_DW9800WAF, DW9800WAF_SetI2Cclient, DW9800WAF_Ioctl,
	DW9800WAF_Release, DW9800WAF_GetFileName, NULL},
	{1, AFDRV_DW9814AF, DW9814AF_SetI2Cclient, DW9814AF_Ioctl,
	 DW9814AF_Release, DW9814AF_GetFileName, NULL},
	{1, AFDRV_DW9839AF, DW9839AF_SetI2Cclient, DW9839AF_Ioctl,
	 DW9839AF_Release, DW9839AF_GetFileName, NULL},
	{1, AFDRV_FP5510E2AF, FP5510E2AF_SetI2Cclient, FP5510E2AF_Ioctl,
	 FP5510E2AF_Release, FP5510E2AF_GetFileName, NULL},
	{1, AFDRV_DW9718AF, DW9718AF_SetI2Cclient, DW9718AF_Ioctl,
	 DW9718AF_Release, DW9718AF_GetFileName, NULL},
	{1, AFDRV_GT9764AF, GT9764AF_SetI2Cclient, GT9764AF_Ioctl,
	GT9764AF_Release, GT9764AF_GetFileName, NULL},
	{1, AFDRV_GT9768AF, GT9768AF_SetI2Cclient, GT9768AF_Ioctl,
	GT9768AF_Release, GT9768AF_GetFileName, NULL},
	{1, AFDRV_LC898212AF, LC898212AF_SetI2Cclient, LC898212AF_Ioctl,
	 LC898212AF_Release, LC898212AF_GetFileName, NULL},
	{1, AFDRV_LC898214AF, LC898214AF_SetI2Cclient, LC898214AF_Ioctl,
	 LC898214AF_Release, LC898214AF_GetFileName, NULL},
	{1, AFDRV_LC898217AF, LC898217AF_SetI2Cclient, LC898217AF_Ioctl,
	 LC898217AF_Release, LC898217AF_GetFileName, NULL},
	{1, AFDRV_LC898217AFA, LC898217AFA_SetI2Cclient, LC898217AFA_Ioctl,
	 LC898217AFA_Release, LC898217AFA_GetFileName, NULL},
	{1, AFDRV_LC898217AFB, LC898217AFB_SetI2Cclient, LC898217AFB_Ioctl,
	 LC898217AFB_Release, LC898217AFB_GetFileName, NULL},
	{1, AFDRV_LC898217AFC, LC898217AFC_SetI2Cclient, LC898217AFC_Ioctl,
	 LC898217AFC_Release, LC898217AFC_GetFileName, NULL},
	{1, AFDRV_LC898229AF, LC898229AF_SetI2Cclient, LC898229AF_Ioctl,
	 LC898229AF_Release, LC898229AF_GetFileName, NULL},
	{1, AFDRV_LC898122AF, LC898122AF_SetI2Cclient, LC898122AF_Ioctl,
	 LC898122AF_Release, LC898122AF_GetFileName, NULL},
	{1, AFDRV_WV511AAF, WV511AAF_SetI2Cclient, WV511AAF_Ioctl,
	 WV511AAF_Release, WV511AAF_GetFileName, NULL},
};

static struct stAF_DrvList *g_pstAF_CurDrv;

static spinlock_t g_AF_SpinLock;

static int g_s4AF_Opened;

static struct i2c_client *g_pstAF_I2Cclient;

static dev_t g_AF_devno;
static struct cdev *g_pAF_CharDrv;
static struct class *actuator_class;
static struct device *lens_device;

static struct regulator *vcamaf_ldo;
static struct pinctrl *vcamaf_pio;
static struct pinctrl_state *vcamaf_pio_on;
static struct pinctrl_state *vcamaf_pio_off;

#define CAMAF_PMIC     "camaf_m1_pmic"
#define CAMAF_GPIO_ON  "camaf_m1_gpio_on"
#define CAMAF_GPIO_OFF "camaf_m1_gpio_off"

static void camaf_power_init(void)
{
	int ret;
	struct device_node *node, *kd_node;

	/* check if customer camera node defined */
	node = of_find_compatible_node(
		NULL, NULL, "mediatek,camera_af_lens");

	if (node) {
		kd_node = lens_device->of_node;
		lens_device->of_node = node;

		if (vcamaf_ldo == NULL) {
			vcamaf_ldo = regulator_get(lens_device, CAMAF_PMIC);
			if (IS_ERR(vcamaf_ldo)) {
				ret = PTR_ERR(vcamaf_ldo);
				vcamaf_ldo = NULL;
				LOG_INF("cannot get regulator\n");
			}
		}

		if (vcamaf_pio == NULL) {
			vcamaf_pio = devm_pinctrl_get(lens_device);
			if (IS_ERR(vcamaf_pio)) {
				ret = PTR_ERR(vcamaf_pio);
				vcamaf_pio = NULL;
				pr_info("cannot get pinctrl\n");
			} else {
				vcamaf_pio_on = pinctrl_lookup_state(
					vcamaf_pio, CAMAF_GPIO_ON);

				if (IS_ERR(vcamaf_pio_on)) {
					ret = PTR_ERR(vcamaf_pio_on);
					vcamaf_pio_on = NULL;
					LOG_INF("cannot get vcamaf_pio_on\n");
				}

				vcamaf_pio_off = pinctrl_lookup_state(
					vcamaf_pio, CAMAF_GPIO_OFF);

				if (IS_ERR(vcamaf_pio_off)) {
					ret = PTR_ERR(vcamaf_pio_off);
					vcamaf_pio_off = NULL;
					LOG_INF("cannot get vcamaf_pio_off\n");
				}
			}
		}

		lens_device->of_node = kd_node;
	}
}

/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 start*/
static int g_AF_Open_flag;
static int g_AfPowerStatus;
static void camera_af_power_off(void);
static void camaf_power_on(void)
{
	int ret;
	g_AF_Open_flag = 1;

    vibrator_notifier_call_chain(CAM_EVENT_ON, NULL);
/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 end*/

	if (vcamaf_ldo) {
		ret = regulator_enable(vcamaf_ldo);
		LOG_INF("regulator enable (%d)\n", ret);
	}

	if (vcamaf_pio && vcamaf_pio_on) {
		ret = pinctrl_select_state(vcamaf_pio, vcamaf_pio_on);
		LOG_INF("pinctrl enable (%d)\n", ret);
	}
}

static void camaf_power_off(void)
{
	int ret;

/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 start*/
    g_AF_Open_flag = 0;
    vibrator_notifier_call_chain(CAM_EVENT_OFF, NULL);
	if (g_AfPowerStatus) {
		camera_af_power_off();
		g_AfPowerStatus = 0;
		LOG_INF("camera af is on,power off\n");
	}
/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 end*/

	if (vcamaf_ldo) {
		ret = regulator_disable(vcamaf_ldo);
		LOG_INF("regulator disable (%d)\n", ret);
	}

	if (vcamaf_pio && vcamaf_pio_off) {
		ret = pinctrl_select_state(vcamaf_pio, vcamaf_pio_off);
		LOG_INF("pinctrl disable (%d)\n", ret);
	}
}

/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 start*/
static int g_AfPowerStatus;
static struct regulator *lens_ldo;
static void camera_af_power_on(void)
{
	int ret = 0;
	struct device_node *node, *kd_node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,camera_hw");
	if (node) {
		kd_node = lens_device->of_node;
		lens_device->of_node = node;

		lens_ldo = regulator_get(lens_device, "cam0_vcamaf");
		if (IS_ERR(lens_ldo)) { /* handle return value */
			ret = PTR_ERR(lens_ldo);
			LOG_INF("get cam0_vcamaf fail, error: %d\n", ret);
		}
	}
        if (lens_ldo) {
                ret = regulator_set_voltage(lens_ldo, 2800000, 2800000);
                if (ret < 0) {
                        LOG_INF("set voltage lens_ldo fail, ret = %d\n", ret);
                }
                /* enable regulator */
                ret = regulator_enable(lens_ldo);
                if (ret < 0) {
                        LOG_INF("enable regulator lens_ldo fail, ret = %d\n",ret);
                }
        }
}

static void camera_af_power_off(void)
{
	int ret = 0;

	ret = regulator_disable(lens_ldo);
	if (ret < 0) {
                LOG_INF("disable regulator vcamaf_ldo fail, ret = %d\n",ret);
	}
}
/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 end*/
#ifdef CONFIG_MACH_MT6765
static int DrvPwrDn1 = 1;
static int DrvPwrDn2 = 1;
static int DrvPwrDn3 = 1;
#endif

void AF_PowerDown(void)
{
	if (g_pstAF_I2Cclient != NULL) {
		LOG_INF("+\n");
		LOG_INF("-\n");
	}
}
EXPORT_SYMBOL(AF_PowerDown);

static long AF_SetMotorName(__user struct stAF_MotorName *pstMotorName)
{
	long i4RetValue = -1;
	int i;
	struct stAF_MotorName stMotorName;

	if (copy_from_user(&stMotorName, pstMotorName,
			   sizeof(struct stAF_MotorName))) {
		LOG_INF("copy to user failed when getting motor information\n");
		return i4RetValue;
	}

	for (i = 0; i < MAX_NUM_OF_LENS; i++) {
		if (g_stAF_DrvList[i].uEnable != 1)
			break;

		LOG_INF("Search Motor Name : %s\n", g_stAF_DrvList[i].uDrvName);
		stMotorName.uMotorName[STRUCT_MOTOR_NAME - 1] = '\0';
		if (strcmp(stMotorName.uMotorName,
			   g_stAF_DrvList[i].uDrvName) == 0) {
			LOG_INF("Motor Name : %s\n", stMotorName.uMotorName);
			g_pstAF_CurDrv = &g_stAF_DrvList[i];
			i4RetValue = g_pstAF_CurDrv->pAF_SetI2Cclient(
				g_pstAF_I2Cclient, &g_AF_SpinLock,
				&g_s4AF_Opened);
			break;
		}
	}
	return i4RetValue;
}

static inline int64_t getCurNS(void)
{
	int64_t ns;
	struct timespec64 time;

	time.tv_sec = time.tv_nsec = 0;
	/* get_monotonic_boottime(&time); */
	ns = time.tv_sec * 1000000000LL + time.tv_nsec;

	return ns;
}

/* OIS/EIS Timer & Workqueue */
static void ois_pos_polling(struct work_struct *data)
{
	mutex_lock(&ois_mutex);
	if (g_pstAF_CurDrv) {
		if (g_pstAF_CurDrv->pAF_OisGetHallPos) {
			int PosX = 0, PosY = 0;

			g_pstAF_CurDrv->pAF_OisGetHallPos(&PosX, &PosY);
			if (g_OisPosIdx >= 0) {
				OisPosInfo.TimeStamp[g_OisPosIdx] = getCurNS();
				OisPosInfo.i4OISHallPosX[g_OisPosIdx] = PosX;
				OisPosInfo.i4OISHallPosY[g_OisPosIdx] = PosY;
				g_OisPosIdx++;
				g_OisPosIdx &= OIS_DATA_MASK;
			}
		}
	}
	mutex_unlock(&ois_mutex);
}

static enum hrtimer_restart ois_timer_func(struct hrtimer *timer)
{
	g_GetOisInfoCnt--;

	if (ois_workqueue != NULL && g_GetOisInfoCnt > 11)
		queue_work(ois_workqueue, &ois_work);

	if (g_GetOisInfoCnt < 10) {
		g_EnableTimer = 0;
		return HRTIMER_NORESTART;
	}

	hrtimer_forward_now(timer, ktime_set(0, 5000000));
	return HRTIMER_RESTART;
}
/* ------------------------- */
/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 start*/
unsigned long g_Last_Param = 0;
/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 end*/
/* ////////////////////////////////////////////////////////////// */
static long AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
		     unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_S_SETDRVNAME:
		i4RetValue = AF_SetMotorName(
			(__user struct stAF_MotorName *)(a_u4Param));
		break;

	case AFIOC_G_GETDRVNAME:
		{
	/* Set Driver Name */
	int i;
	struct stAF_MotorName stMotorName = {'\0'};
	struct stAF_DrvList *pstAF_CurDrv = NULL;
	__user struct stAF_MotorName *pstMotorName =
			(__user struct stAF_MotorName *)a_u4Param;

	if (copy_from_user(&stMotorName, pstMotorName,
			   sizeof(struct stAF_MotorName))) {
		LOG_INF("copy to user failed when getting motor information\n");
		break;
	}

	for (i = 0; i < MAX_NUM_OF_LENS; i++) {
		if (g_stAF_DrvList[i].uEnable != 1)
			break;

		LOG_INF("Search Motor Name : %s\n", g_stAF_DrvList[i].uDrvName);
		stMotorName.uMotorName[STRUCT_MOTOR_NAME - 1] = '\0';
		if (strcmp(stMotorName.uMotorName,
			   g_stAF_DrvList[i].uDrvName) == 0) {
			LOG_INF("Motor Name : %s\n", stMotorName.uMotorName);
			pstAF_CurDrv = &g_stAF_DrvList[i];
			break;
		}
	}

	/* Get File Name */
	if (pstAF_CurDrv) {
		if (pstAF_CurDrv->pAF_GetFileName) {
			__user struct stAF_MotorName *pstMotorName =
			(__user struct stAF_MotorName *)a_u4Param;
			struct stAF_MotorName MotorFileName;

			pstAF_CurDrv->pAF_GetFileName(
					MotorFileName.uMotorName);
			i4RetValue = 1;
			LOG_INF("GETDRVNAME : get file name(%s)\n",
				MotorFileName.uMotorName);
			if (copy_to_user(
				    pstMotorName, &MotorFileName,
				    sizeof(struct stAF_MotorName)))
				LOG_INF("copy to user failed\n");
		}
	}
		}
		break;

	case AFIOC_S_SETDRVINIT:
		spin_lock(&g_AF_SpinLock);
		g_s4AF_Opened = 1;
		spin_unlock(&g_AF_SpinLock);
		break;

	case AFIOC_S_SETPOWERDOWN:
		AF_PowerDown();
		i4RetValue = 1;
		break;

	case AFIOC_G_OISPOSINFO:
		if (g_pstAF_CurDrv) {
			if (g_pstAF_CurDrv->pAF_OisGetHallPos) {
				__user struct stAF_OisPosInfo *pstOisPosInfo =
					(__user struct stAF_OisPosInfo *)
						a_u4Param;

				mutex_lock(&ois_mutex);

				if (copy_to_user(
					    pstOisPosInfo, &OisPosInfo,
					    sizeof(struct stAF_OisPosInfo)))
					LOG_INF("copy to user failed\n");

				g_OisPosIdx = 0;
				g_GetOisInfoCnt = 100;
				memset(&OisPosInfo, 0, sizeof(OisPosInfo));
				mutex_unlock(&ois_mutex);

				if (g_EnableTimer == 0) {
					/* Start Timer */
					hrtimer_start(&ois_timer,
						      ktime_set(0, 50000000),
						      HRTIMER_MODE_REL);
					g_EnableTimer = 1;
				}
			}
		}
		break;

	default:
/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 start*/
		if (a_u4Param < 1024)
            g_Last_Param = a_u4Param;
/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 end*/
		if (g_pstAF_CurDrv) {
			if (g_pstAF_CurDrv->pAF_Ioctl)
				i4RetValue = g_pstAF_CurDrv->pAF_Ioctl(
					a_pstFile, a_u4Command, a_u4Param);
		}
		break;
	}

	return i4RetValue;
}

#ifdef CONFIG_COMPAT
static long AF_Ioctl_Compat(struct file *a_pstFile, unsigned int a_u4Command,
			    unsigned long a_u4Param)
{
	long i4RetValue = 0;

	i4RetValue = AF_Ioctl(a_pstFile, a_u4Command,
			      (unsigned long)compat_ptr(a_u4Param));

	return i4RetValue;
}
#endif

/* Main jobs: */
/* 1.check for device-specified errors, device not ready. */
/* 2.Initialize the device if it is opened for the first time. */
/* 3.Update f_op pointer. */
/* 4.Fill data structures into private_data */
/* CAM_RESET */
static int AF_Open(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");
/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 start*/
        if (g_EnableVcmTimer) {
                hrtimer_cancel(&vcm_timer);
                g_EnableVcmTimer = 0;
        }
        if (g_pstAF_CurDrv) {
                DW9800WAF_Release(NULL,NULL);
                g_pstAF_CurDrv = NULL;
        } else {
                spin_lock(&g_AF_SpinLock);
                g_s4AF_Opened = 0;
                spin_unlock(&g_AF_SpinLock);
        }
/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 end*/
	spin_lock(&g_AF_SpinLock);
	if (g_s4AF_Opened) {
		spin_unlock(&g_AF_SpinLock);
		LOG_INF("The device is opened\n");
		return -EBUSY;
	}
	g_s4AF_Opened = 1;
	spin_unlock(&g_AF_SpinLock);

	camaf_power_init();
	camaf_power_on();

	/* OIS/EIS Timer & Workqueue */
	/* init work queue */
	INIT_WORK(&ois_work, ois_pos_polling);

	/* init timer */
	hrtimer_init(&ois_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ois_timer.function = ois_timer_func;

	g_EnableTimer = 0;
	/* ------------------------- */

	LOG_INF("End\n");

	return 0;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
static int AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");

	if (g_pstAF_CurDrv) {
		g_pstAF_CurDrv->pAF_Release(a_pstInode, a_pstFile);
		g_pstAF_CurDrv = NULL;
	} else {
		spin_lock(&g_AF_SpinLock);
		g_s4AF_Opened = 0;
		spin_unlock(&g_AF_SpinLock);
	}

	camaf_power_off();

	/* OIS/EIS Timer & Workqueue */
	/* Cancel Timer */
	hrtimer_cancel(&ois_timer);

	/* flush work queue */
	flush_work(&ois_work);

	if (ois_workqueue) {
		flush_workqueue(ois_workqueue);
		destroy_workqueue(ois_workqueue);
		ois_workqueue = NULL;
	}
	/* ------------------------- */

	LOG_INF("End\n");

	return 0;
}

static const struct file_operations g_stAF_fops = {
	.owner = THIS_MODULE,
	.open = AF_Open,
	.release = AF_Release,
	.unlocked_ioctl = AF_Ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = AF_Ioctl_Compat,
#endif
};

static inline int Register_AF_CharDrv(void)
{
	LOG_INF("Start\n");

	/* Allocate char driver no. */
	if (alloc_chrdev_region(&g_AF_devno, 0, 1, AF_DRVNAME)) {
		LOG_INF("Allocate device no failed\n");

		return -EAGAIN;
	}
	/* Allocate driver */
	g_pAF_CharDrv = cdev_alloc();

	if (g_pAF_CharDrv == NULL) {
		unregister_chrdev_region(g_AF_devno, 1);

		LOG_INF("Allocate mem for kobject failed\n");

		return -ENOMEM;
	}
	/* Attatch file operation. */
	cdev_init(g_pAF_CharDrv, &g_stAF_fops);

	g_pAF_CharDrv->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(g_pAF_CharDrv, g_AF_devno, 1)) {
		LOG_INF("Attatch file operation failed\n");

		unregister_chrdev_region(g_AF_devno, 1);

		return -EAGAIN;
	}

	actuator_class = class_create(THIS_MODULE, AF_DRIVER_CLASS_NAME);
	if (IS_ERR(actuator_class)) {
		int ret = PTR_ERR(actuator_class);

		LOG_INF("Unable to create class, err = %d\n", ret);
		return ret;
	}

	lens_device = device_create(actuator_class, NULL, g_AF_devno, NULL,
				    AF_DRVNAME);

	if (lens_device == NULL)
		return -EIO;

	LOG_INF("End\n");
	return 0;
}

static inline void Unregister_AF_CharDrv(void)
{
	LOG_INF("Start\n");

	/* Release char driver */
	cdev_del(g_pAF_CharDrv);

	unregister_chrdev_region(g_AF_devno, 1);

	device_destroy(actuator_class, g_AF_devno);

	class_destroy(actuator_class);

	LOG_INF("End\n");
}

/* //////////////////////////////////////////////////////////////////// */

static int AF_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id);
static int AF_i2c_remove(struct i2c_client *client);
static const struct i2c_device_id AF_i2c_id[] = {{AF_DRVNAME, 0}, {} };

/* TOOL : kernel-3.10\tools\dct */
/* PATH : vendor\mediatek\proprietary\custom\#project#\kernel\dct\dct */
#if I2C_CONFIG_SETTING == 2
static const struct of_device_id MAINAF_of_match[] = {
	{.compatible = "mediatek,CAMERA_MAIN_AF"}, {},
};
#endif

static struct i2c_driver AF_i2c_driver = {
	.probe = AF_i2c_probe,
	.remove = AF_i2c_remove,
	.driver.name = AF_DRVNAME,
#if I2C_CONFIG_SETTING == 2
	.driver.of_match_table = MAINAF_of_match,
#endif
	.id_table = AF_i2c_id,
};

static int AF_i2c_remove(struct i2c_client *client)
{
	Unregister_AF_CharDrv();
	return 0;
}

/* Kirby: add new-style driver {*/
static int AF_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int i4RetValue = 0;

	LOG_INF("Start\n");

	/* Kirby: add new-style driver { */
	g_pstAF_I2Cclient = client;

	/* Register char driver */
	i4RetValue = Register_AF_CharDrv();

	if (i4RetValue) {

		LOG_INF(" register char device failed!\n");

		return i4RetValue;
	}

	spin_lock_init(&g_AF_SpinLock);

/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 start*/
	INIT_WORK(&vib_off_work, vib_off_work_callback);
	INIT_WORK(&vib_on_work, vib_on_work_callback);
//init vcm timer
	hrtimer_init(&vcm_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vcm_timer.function = vcm_timer_func;
	g_EnableVcmTimer = 0;
	g_AfPowerStatus = 0;
    g_AF_Open_flag = 0;
//init wakelock
	vcm_wakelock = wakeup_source_register(NULL, "vcm-wake-lock");
/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 end*/
	LOG_INF("Attached!!\n");

	return 0;
}
/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 start*/
extern void dw9800MoveAFToZero(unsigned long a_u4Position);
static void cam_vcm_event_vib_on(void)
{
        int i;
        struct stAF_MotorName stMotorName;
        if (g_EnableVcmTimer) {
                hrtimer_cancel(&vcm_timer);
                g_EnableVcmTimer = 0;
        }
        if (g_AfPowerStatus) {
                LOG_INF("camera af power is on \n");
                return;
        }
        camera_af_power_on();
        g_AfPowerStatus = 1;
        //if (g_pstInode && g_pstFile)
        {
//open AF
                spin_lock(&g_AF_SpinLock);
                if (g_s4AF_Opened) {
                        spin_unlock(&g_AF_SpinLock);
                        LOG_INF("The device is opened\n");
                        return;
                }
                g_s4AF_Opened = 1;
                spin_unlock(&g_AF_SpinLock);
//seach Name
                strcpy(stMotorName.uMotorName, AFDRV_DW9800WAF);
                stMotorName.uMotorName[sizeof(stMotorName.uMotorName) - 1] = '\0';
                for (i = 0; i < MAX_NUM_OF_LENS; i++) {
                        if (g_stAF_DrvList[i].uEnable != 1)
                                break;
                        LOG_INF("Search Motor Name : %s\n", g_stAF_DrvList[i].uDrvName);
                        if (strcmp(stMotorName.uMotorName,g_stAF_DrvList[i].uDrvName) == 0) {
                                LOG_INF("Motor Name : %s\n", stMotorName.uMotorName);
                                g_pstAF_CurDrv = &g_stAF_DrvList[i];
                                g_pstAF_CurDrv->pAF_SetI2Cclient(g_pstAF_I2Cclient, &g_AF_SpinLock,&g_s4AF_Opened);
                                break;
                        }
                }
//move AF
                dw9800MoveAFToZero(0); // move dw9800 to zero position
        }
}

static void cam_vcm_power_off(void)
{
        LOG_INF("cam_vcm_power_off start\n");
        //if (g_pstInode && g_pstFile)
        {
                if (g_pstAF_CurDrv) {
                        DW9800WAF_Release(NULL,NULL);
                        //g_pstAF_CurDrv->pAF_Release(g_pstInode, g_pstFile);
                        g_pstAF_CurDrv = NULL;
                } else {
                        spin_lock(&g_AF_SpinLock);
                        g_s4AF_Opened = 0;
                        spin_unlock(&g_AF_SpinLock);
                }
        }

        camera_af_power_off();
        g_AfPowerStatus = 0;
}

static enum hrtimer_restart vcm_timer_func(struct hrtimer *timer)
{
	LOG_INF("vcm_timer_func \n");
	schedule_work(&vib_off_work);
	return HRTIMER_NORESTART;
}

static void cam_vcm_event_vib_off(void)
{
	ktime_t ktime;

	g_EnableVcmTimer = 1;
	ktime = ktime_set(2, 0); //2s
	hrtimer_start(&vcm_timer, ktime, HRTIMER_MODE_REL);
}

static int cam_vcm_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
    LOG_INF("cam_vcm_notifier_callback \n");
	if (event == CAM_VCM_EVENT_VIB_ON) {
			LOG_INF("CAM_VCM_EVENT_VIB_ON \n");
			if (!g_AF_Open_flag) {
					schedule_work(&vib_on_work);
			}
	} else if(event == CAM_VCM_EVENT_VIB_OFF) {
			LOG_INF("CAM_VCM_EVENT_VIB_OFF \n");
			if (!g_AF_Open_flag) {
					__pm_wakeup_event(vcm_wakelock, 3500); //wakelock 3.5 secs
					cam_vcm_event_vib_off();
			}
        }
	return 0;
}

static void vib_off_work_callback(struct work_struct *data)
{
	LOG_INF("vib_off_work_callback\n");
	cam_vcm_power_off();
}

static void vib_on_work_callback(struct work_struct *data)
{
	LOG_INF("vib_on_work_callback\n");
	cam_vcm_event_vib_on();
}

#include <linux/proc_fs.h>
#define PROC_BUF_SIZE 256
static ssize_t lens_debug_write(struct file *filp, const char __user *buff,
			       size_t count, loff_t *ppos)
{
	u8 *writebuf = NULL;
	u8 tmpbuf[PROC_BUF_SIZE] = {0};
	int buflen = count;
	int ret = 0;

	if ((buflen < 1) || (buflen > PAGE_SIZE)) {
		LOG_INF("apk proc write count(%d>%d) fail", buflen,
			  (int)PAGE_SIZE);
		return -EINVAL;
	}

	if (buflen > PROC_BUF_SIZE) {
		writebuf = kcalloc(buflen, sizeof(u8), GFP_KERNEL);
		if (writebuf == NULL) {
			LOG_INF("apk proc write buf zalloc fail");
			return -ENOMEM;
		}
	} else {
		writebuf = tmpbuf;
	}

	if (copy_from_user(writebuf, buff, buflen)) {
		LOG_INF("[APK]: copy from user error!!");
		ret = -EFAULT;
	}
	LOG_INF("lens_test writebuf[0] = %d",writebuf[0]);
	if(writebuf[0] == '1')
		cam_vcm_notifier_call_chain(CAM_VCM_EVENT_VIB_ON, NULL);
	else if (writebuf[0] == '0')
		cam_vcm_notifier_call_chain(CAM_VCM_EVENT_VIB_OFF, NULL);

	ret = buflen;

	return ret;
}

static const struct file_operations lens_proc_fops = {
  	.owner = THIS_MODULE,
	.write = lens_debug_write,
};

struct notifier_block cam_vcm_notif;
/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 end*/

static int AF_probe(struct platform_device *pdev)
{
/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 start*/
    int ret = 0;

	cam_vcm_notif.notifier_call = cam_vcm_notifier_callback;
	ret = cam_vcm_register_client(&cam_vcm_notif);
	if (ret) {
		LOG_INF("Unable to register cam_vcm_notifier: %d\n",ret);
	}
	// proc_create("lens_test", 0777, NULL, &lens_proc_fops);
/*Penang Code for EKPENAN4GU-2875 by changqi at 20240321 end*/
	return i2c_add_driver(&AF_i2c_driver);
}

static int AF_remove(struct platform_device *pdev)
{
	i2c_del_driver(&AF_i2c_driver);
	return 0;
}

static int AF_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int AF_resume(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id gaf_of_device_id[] = {
	{.compatible = "mediatek,camera_af_lens",},
	{}
};
#endif

/* platform structure */
static struct platform_driver g_stAF_Driver = {
	.probe = AF_probe,
	.remove = AF_remove,
	.suspend = AF_suspend,
	.resume = AF_resume,
	.driver = {
		.name = PLATFORM_DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = gaf_of_device_id,
#endif
	} };

static struct platform_device g_stAF_device = {
	.name = PLATFORM_DRIVER_NAME, .id = 0, .dev = {} };

static int __init MAINAF_i2C_init(void)
{
#if I2C_CONFIG_SETTING == 1
	i2c_register_board_info(LENS_I2C_BUSNUM, &kd_lens_dev, 1);
#endif

	if (platform_device_register(&g_stAF_device)) {
		LOG_INF("failed to register AF driver\n");
		return -ENODEV;
	}

	if (platform_driver_register(&g_stAF_Driver)) {
		LOG_INF("Failed to register AF driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit MAINAF_i2C_exit(void)
{
	platform_driver_unregister(&g_stAF_Driver);
	platform_device_unregister(&g_stAF_device);
}
module_init(MAINAF_i2C_init);
module_exit(MAINAF_i2C_exit);

MODULE_DESCRIPTION("MAINAF lens module driver");
MODULE_AUTHOR("KY Chen <ky.chen@Mediatek.com>");
MODULE_LICENSE("GPL");
