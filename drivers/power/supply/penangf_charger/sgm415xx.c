// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/init.h>     /* For init/exit macros */
#include <linux/module.h>   /* For MODULE_ marcros  */
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif
//#include <mt-plat/mtk_boot.h>
//#include <mt-plat/upmu_common.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include "sgm415xx.h"
#include <charger_class.h>
#include <mtk_battery.h>
/**********************************************************
 *
 *   [I2C Slave Setting]
 *
 *********************************************************/

#define SGM4154x_REG_NUM    (0xF)

extern void Charger_Detect_Release(void);
extern void Charger_Detect_Init(void);

extern int g_quick_charging_flag;
extern int g_charger_status;
extern void hvdcp_chgstat_notify(void);
extern bool g_qc3_type_set;
/* SGM4154x REG06 BOOST_LIM[5:4], uV */
static const unsigned int BOOST_VOLT_LIMIT[] = {
    4850000, 5000000, 5150000, 5300000
};
/* SGM4154x REG02 BOOST_LIM[7:7], uA */
#if (defined(__SGM41542_CHIP_ID__) || defined(__SGM41541_CHIP_ID__)|| defined(__SGM41543_CHIP_ID__)|| defined(__SGM41543D_CHIP_ID__))
static const unsigned int BOOST_CURRENT_LIMIT[] = {
    1200000, 2000000
};
#else
static const unsigned int BOOST_CURRENT_LIMIT[] = {
    500000, 1200000
};
#endif

#if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))

static const unsigned int IPRECHG_CURRENT_STABLE[] = {
    5000, 10000, 15000, 20000, 30000, 40000, 50000, 60000,
    80000, 100000, 120000, 140000, 160000, 180000, 200000, 240000
};

static const unsigned int ITERM_CURRENT_STABLE[] = {
    5000, 10000, 15000, 20000, 30000, 40000, 50000, 60000,
    80000, 100000, 120000, 140000, 160000, 180000, 200000, 240000
};
#endif

static enum power_supply_usb_type sgm4154x_usb_type[] = {
    POWER_SUPPLY_USB_TYPE_UNKNOWN,
    POWER_SUPPLY_USB_TYPE_SDP,
    POWER_SUPPLY_USB_TYPE_DCP,
    POWER_SUPPLY_USB_TYPE_CDP,
    POWER_SUPPLY_USB_TYPE_ACA,
};

static const struct charger_properties sgm4154x_chg_props = {
    .alias_name = SGM4154x_NAME,
};


enum {
    SGM_DP_DM_VOL_HIZ,
    SGM_DP_DM_VOL_0P0,
    SGM_DP_DM_VOL_0P6,
    SGM_DP_DM_VOL_3P3,
};

/**********************************************************
 *
 *   [Global Variable]
 *
 *********************************************************/
static struct power_supply_desc sgm4154x_power_supply_desc;
static struct charger_device *s_chg_dev_otg;
struct sgm4154x_chargeing_type g_sgm_type;
static int sgm4154x_set_vreg_tunning(struct sgm4154x_device *sgm, int tunning);
static int g_num = 0;
static bool up_vol_test = true;
static bool start_count_work = true;
static int error_times = 0;
/**********************************************************
 *
 *   [I2C Function For Read/Write sgm4154x]
 *
 *********************************************************/
static int __sgm4154x_read_byte(struct sgm4154x_device *sgm, u8 reg, u8 *data)
{
    s32 ret;

    ret = i2c_smbus_read_byte_data(sgm->client, reg);
    if (ret < 0) {
        pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
        return ret;
    }

    *data = (u8) ret;

    return 0;
}

static int __sgm4154x_write_byte(struct sgm4154x_device *sgm, int reg, u8 val)
{
    s32 ret;

    ret = i2c_smbus_write_byte_data(sgm->client, reg, val);
    if (ret < 0) {
        pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
                val, reg, ret);
        return ret;
    }
    return 0;
}

static int sgm4154x_read_reg(struct sgm4154x_device *sgm, u8 reg, u8 *data)
{
    int ret;

    mutex_lock(&sgm->i2c_rw_lock);
    ret = __sgm4154x_read_byte(sgm, reg, data);
    mutex_unlock(&sgm->i2c_rw_lock);

    return ret;
}
#if 0
static int sgm4154x_write_reg(struct sgm4154x_device *sgm, u8 reg, u8 val)
{
    int ret;

    mutex_lock(&sgm->i2c_rw_lock);
    ret = __sgm4154x_write_byte(sgm, reg, val);
    mutex_unlock(&sgm->i2c_rw_lock);

    if (ret)
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

    return ret;
}
#endif
static int sgm4154x_update_bits(struct sgm4154x_device *sgm, u8 reg,
        u8 mask, u8 val)
{
    int ret;
    u8 tmp;

    mutex_lock(&sgm->i2c_rw_lock);
    ret = __sgm4154x_read_byte(sgm, reg, &tmp);
    if (ret) {
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
        goto out;
    }

    tmp &= ~mask;
    tmp |= val & mask;

    ret = __sgm4154x_write_byte(sgm, reg, tmp);
    if (ret)
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
    mutex_unlock(&sgm->i2c_rw_lock);
    return ret;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static int sgm4154x_set_watchdog_timer(struct sgm4154x_device *sgm, int time)
{
    int ret;
    u8 reg_val;

    if (time == 0)
        reg_val = SGM4154x_WDT_TIMER_DISABLE;
    else if (time == 40)
        reg_val = SGM4154x_WDT_TIMER_40S;
    else if (time == 80)
        reg_val = SGM4154x_WDT_TIMER_80S;
    else
        reg_val = SGM4154x_WDT_TIMER_160S;

    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_5,
            SGM4154x_WDT_TIMER_MASK, reg_val);

    return ret;
}

#if 0
static int sgm4154x_get_term_curr(struct sgm4154x_device *sgm)
{
    int ret;
    u8 reg_val;
    int curr;
    int offset = SGM4154x_TERMCHRG_I_MIN_uA;

    ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_3, &reg_val);
    if (ret)
        return ret;

    reg_val &= SGM4154x_TERMCHRG_CUR_MASK;
    curr = reg_val * SGM4154x_TERMCHRG_CURRENT_STEP_uA + offset;
    return curr;
}

static int sgm4154x_get_prechrg_curr(struct sgm4154x_device *sgm)
{
    int ret;
    u8 reg_val;
    int curr;
    int offset = SGM4154x_PRECHRG_I_MIN_uA;

    ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_3, &reg_val);
    if (ret)
        return ret;

    reg_val = (reg_val&SGM4154x_PRECHRG_CUR_MASK)>>4;
    curr = reg_val * SGM4154x_PRECHRG_CURRENT_STEP_uA + offset;
    return curr;
}
#endif

static int sgm4154x_get_ichg_curr(struct charger_device *chg_dev, u32 *uA)
{
    int ret;
    u8 ichg;
    unsigned int curr;
    struct sgm4154x_device *sgm = charger_get_data(chg_dev);

    ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_2, &ichg);
    if (ret)
        return ret;

    ichg &= SGM4154x_ICHRG_I_MASK;
#if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
    if (ichg <= 0x8)
        curr = ichg * 5000;
    else if (ichg <= 0xF)
        curr = 40000 + (ichg - 0x8) * 10000;
    else if (ichg <= 0x17)
        curr = 110000 + (ichg - 0xF) * 20000;
    else if (ichg <= 0x20)
        curr = 270000 + (ichg - 0x17) * 30000;
    else if (ichg <= 0x30)
        curr = 540000 + (ichg - 0x20) * 60000;
    else if (ichg <= 0x3C)
        curr = 1500000 + (ichg - 0x30) * 120000;
    else
        curr = 3000000;
#else
    curr = ichg * SGM4154x_ICHRG_I_STEP_uA;
#endif
    *uA = curr;
    pr_info("%s:get curr %d uA\n", __func__, *uA);
    return curr;
}

static int sgm4154x_set_term_curr(struct sgm4154x_device *sgm, int uA)
{
    u8 reg_val;
#if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))

    for(reg_val = 1; reg_val < 16 && uA >= ITERM_CURRENT_STABLE[reg_val]; reg_val++)
        ;
    reg_val--;
#else
    if (uA < SGM4154x_TERMCHRG_I_MIN_uA)
        uA = SGM4154x_TERMCHRG_I_MIN_uA;
    else if (uA > SGM4154x_TERMCHRG_I_MAX_uA)
        uA = SGM4154x_TERMCHRG_I_MAX_uA;

    reg_val = (uA - 1) / SGM4154x_TERMCHRG_CURRENT_STEP_uA;// -1 in order to seam protection
#endif

    return sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_3,
            SGM4154x_TERMCHRG_CUR_MASK, reg_val);
}

static int sgm4154x_set_prechrg_curr(struct sgm4154x_device *sgm, int uA)
{
    u8 reg_val;

#if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
    for(reg_val = 1; reg_val < 16 && uA >= IPRECHG_CURRENT_STABLE[reg_val]; reg_val++)
        ;
    reg_val--;
#else
    if (uA < SGM4154x_PRECHRG_I_MIN_uA)
        uA = SGM4154x_PRECHRG_I_MIN_uA;
    else if (uA > SGM4154x_PRECHRG_I_MAX_uA)
        uA = SGM4154x_PRECHRG_I_MAX_uA;

    reg_val = (uA - SGM4154x_PRECHRG_I_MIN_uA) / SGM4154x_PRECHRG_CURRENT_STEP_uA;
#endif
    reg_val = reg_val << 4;
    return sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_3,
            SGM4154x_PRECHRG_CUR_MASK, reg_val);
}

static int sgm4154x_set_ichrg_curr(struct charger_device *chg_dev, unsigned int uA)
{
    int ret;
    u8 reg_val;
    struct sgm4154x_device *sgm = charger_get_data(chg_dev);
    pr_info("%s: set curr %d uA\n", __func__, uA);
    if (uA < SGM4154x_ICHRG_I_MIN_uA)
        uA = SGM4154x_ICHRG_I_MIN_uA;
    else if ( uA > sgm->init_data.max_ichg)
        uA = sgm->init_data.max_ichg;
#if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
    if (uA <= 40000)
        reg_val = uA / 5000;
    else if (uA <= 110000)
        reg_val = 0x08 + (uA -40000) / 10000;
    else if (uA <= 270000)
        reg_val = 0x0F + (uA -110000) / 20000;
    else if (uA <= 540000)
        reg_val = 0x17 + (uA -270000) / 30000;
    else if (uA <= 1500000)
        reg_val = 0x20 + (uA -540000) / 60000;
    else if (uA <= 2940000)
        reg_val = 0x30 + (uA -1500000) / 120000;
    else
        reg_val = 0x3d;
#else

    reg_val = uA / SGM4154x_ICHRG_I_STEP_uA;
#endif
    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_2,
            SGM4154x_ICHRG_I_MASK, reg_val);

    return ret;
}

static int sgm4154x_set_chrg_volt(struct charger_device *chg_dev, unsigned int chrg_volt)
{
    int ret;
    u8 reg_val;
    u32 charg_volt_tunning = 0;
    struct sgm4154x_device *sgm = charger_get_data(chg_dev);

    pr_info("into %s, chrg_volt is %d\n",__func__, chrg_volt);
    if (chrg_volt == 4450000) {  //Default battery cv
        chrg_volt = 4464000;
        charg_volt_tunning = 3;
    } else if (chrg_volt == 4496000) {   //qc battery cv
        charg_volt_tunning = 0;
    } else if (chrg_volt == 4488000) {   //recovery to sw battery cv
        chrg_volt = 4496000;
        charg_volt_tunning = 2;
    } else if (chrg_volt == 4480000) {   //recovery to sw battery cv
        chrg_volt = 4496000;
        charg_volt_tunning = 3;
    }

    if (chrg_volt < SGM4154x_VREG_V_MIN_uV)
        chrg_volt = SGM4154x_VREG_V_MIN_uV;
    else if (chrg_volt > sgm->init_data.max_vreg)
        chrg_volt = sgm->init_data.max_vreg;


    reg_val = (chrg_volt-SGM4154x_VREG_V_MIN_uV) / SGM4154x_VREG_V_STEP_uV;
    reg_val = reg_val<<3;
    pr_err("%s:reg_val is %x, charg_volt_tunning is %d\n",__func__, reg_val, charg_volt_tunning);
    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_4,
            SGM4154x_VREG_V_MASK, reg_val);
    ret = sgm4154x_set_vreg_tunning(sgm, charg_volt_tunning);

    return ret;
}

static int sgm4154x_get_chrg_volt(struct charger_device *chg_dev,unsigned int *volt)
{
    int ret;
    u8 vreg_val;
    struct sgm4154x_device *sgm = charger_get_data(chg_dev);

    ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_4, &vreg_val);
    if (ret)
        return ret;

    vreg_val = (vreg_val & SGM4154x_VREG_V_MASK)>>3;

    if (15 == vreg_val)
        *volt = 4352000; //default
    else if (vreg_val < 25)
        *volt = vreg_val*SGM4154x_VREG_V_STEP_uV + SGM4154x_VREG_V_MIN_uV;

    return 0;
}
#if 0
static int sgm4154x_get_vindpm_offset_os(struct sgm4154x_device *sgm)
{
    int ret;
    u8 reg_val;

    ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_f, &reg_val);
    if (ret)
        return ret;

    reg_val = reg_val & SGM4154x_VINDPM_OS_MASK;

    return reg_val;
}
#endif
static int sgm4154x_set_vindpm_offset_os(struct sgm4154x_device *sgm,u8 offset_os)
{
    int ret;

    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_f,
            SGM4154x_VINDPM_OS_MASK, offset_os);

    if (ret){
        pr_err("%s fail\n",__func__);
        return ret;
    }

    return ret;
}
static int sgm4154x_set_input_volt_lim(struct charger_device *chg_dev, unsigned int vindpm)
{
    int ret;
    unsigned int offset;
    u8 reg_val;
    u8 os_val;
    struct sgm4154x_device *sgm = charger_get_data(chg_dev);

    if (vindpm < SGM4154x_VINDPM_V_MIN_uV ||
            vindpm > SGM4154x_VINDPM_V_MAX_uV)
        return -EINVAL;

    if (vindpm < 5900000){
        os_val = 0;
        offset = 3900000;
    }
    else if (vindpm >= 5900000 && vindpm < 7500000){
        os_val = 1;
        offset = 5900000; //uv
    }
    else if (vindpm >= 7500000 && vindpm < 10500000){
        os_val = 2;
        offset = 7500000; //uv
    }
    else{
        os_val = 3;
        offset = 10500000; //uv
    }

    sgm4154x_set_vindpm_offset_os(sgm,os_val);
    reg_val = (vindpm - offset) / SGM4154x_VINDPM_STEP_uV;

    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_6,
            SGM4154x_VINDPM_V_MASK, reg_val);

    return ret;
}
#if 0
static int sgm4154x_get_input_volt_lim(struct sgm4154x_device *sgm)
{
    int ret;
    int offset;
    u8 vlim;
    int temp;

    ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_6, &vlim);
    if (ret)
        return ret;

    temp = sgm4154x_get_vindpm_offset_os(sgm);
    if (0 == temp)
        offset = 3900000; //uv
    else if (1 == temp)
        offset = 5900000;
    else if (2 == temp)
        offset = 7500000;
    else if (3 == temp)
        offset = 10500000;

    temp = offset + (vlim & 0x0F) * SGM4154x_VINDPM_STEP_uV;
    return temp;
}
#endif

static int sgm4154x_set_input_curr_lim(struct charger_device *chg_dev, unsigned int iindpm)
{
    int ret;
    u8 reg_val;
    struct sgm4154x_device *sgm = charger_get_data(chg_dev);

    if (iindpm < SGM4154x_IINDPM_I_MIN_uA ||
            iindpm > SGM4154x_IINDPM_I_MAX_uA)
        return -EINVAL;

#if (defined(__SGM41513_CHIP_ID__) || defined(__SGM41513A_CHIP_ID__) || defined(__SGM41513D_CHIP_ID__))
    reg_val = (iindpm-SGM4154x_IINDPM_I_MIN_uA) / SGM4154x_IINDPM_STEP_uA;
#else
    if (iindpm >= SGM4154x_IINDPM_I_MIN_uA && iindpm <= 3100000)//default
        reg_val = (iindpm-SGM4154x_IINDPM_I_MIN_uA) / SGM4154x_IINDPM_STEP_uA;
    else if (iindpm > 3100000 && iindpm < SGM4154x_IINDPM_I_MAX_uA)
        reg_val = 0x1E;
    else
        reg_val = SGM4154x_IINDPM_I_MASK;
#endif
    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_0,
            SGM4154x_IINDPM_I_MASK, reg_val);
    return ret;
}

static int sgm4154x_get_input_curr_lim(struct charger_device *chg_dev,unsigned int *ilim)
{
    int ret;
    u8 reg_val;
    struct sgm4154x_device *sgm = charger_get_data(chg_dev);

    ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_0, &reg_val);
    if (ret)
        return ret;
    if (SGM4154x_IINDPM_I_MASK == (reg_val & SGM4154x_IINDPM_I_MASK))
        *ilim =  SGM4154x_IINDPM_I_MAX_uA;
    else
        *ilim = (reg_val & SGM4154x_IINDPM_I_MASK)*SGM4154x_IINDPM_STEP_uA + SGM4154x_IINDPM_I_MIN_uA;

    return 0;
}


static int32_t sgm4154x_set_dpdm(
        struct sgm4154x_device *sgm, uint8_t dp_val, uint8_t dm_val)
{
    uint8_t data_reg = 0;

    uint8_t mask = SGM4154x_DP_VSEL_MASK|SGM4154x_DM_VSEL_MASK;

    data_reg  = (dp_val << SGM4154x_DP_VOLT_SHIFT) & SGM4154x_DP_VSEL_MASK;
    data_reg |= (dm_val << SGM4154x_DM_VOLT_SHIFT) & SGM4154x_DM_VSEL_MASK;

    return sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_d,
            mask, data_reg);

}
#if 0
static int sgm4154x_charging_set_hvdcp20(
        struct sgm4154x_device *sgm, uint32_t vbus_target)
{
    int32_t ret = 0;

    pr_err("Set vbus target %dv\n", vbus_target);
    switch (vbus_target) {
        case 5:
            ret = sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_0P0);
            sgm4154x_set_input_volt_lim(sgm->chg_dev, 4500000);
            break;
        case 9:
            ret = sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_3P3, SGM_DP_DM_VOL_0P6);
            sgm4154x_set_input_volt_lim(sgm->chg_dev, 8000000);
            break;
        case 12:
            ret = sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_0P6);
            sgm4154x_set_input_volt_lim(sgm->chg_dev, 11000000);
            break;
        default:
            ret = sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_HIZ, SGM_DP_DM_VOL_HIZ);
            sgm4154x_set_input_volt_lim(sgm->chg_dev, 4500000);
            break;
    }

    return (ret < 0) ? ret : 0;
}
#endif

int32_t sgm4154x_charging_enable_hvdcp30(struct sgm4154x_device *sgm, bool enable)
{
    //  sgm4154x_dbg("enable %d\n", enable);
    //enter continuous mode DP 0.6, DM 3.3
    return sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_3P3);
}

int32_t sgm4154x_charging_set_hvdcp30(struct sgm4154x_device *sgm, bool increase)
{
    int32_t ret = 0;

    //  sgm4154x_dbg("increase %d\n", increase);
    if (increase) {
        //DP 3.3, DM 3.3
        ret = sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_3P3, SGM_DP_DM_VOL_3P3);
        if (ret < 0)
            return ret;
        //need test 3ms
        mdelay(3);
        //DP 0.6, DM 3.3
        ret = sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_3P3);
        if (ret < 0)
            return ret;
        mdelay(3);
    } else {
        //DP 0.6, DM 3.3
        ret = sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_0P6);
        if (ret < 0)
            return ret;
        //need test
        mdelay(3);
        //DP 0.6, DM 3.3
        ret = sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_3P3);
        if (ret < 0)
            return ret;
        mdelay(3);
    }
    return 0;
}

static int sgm4154x_get_state(struct sgm4154x_device *sgm,
        struct sgm4154x_state *state)
{
    u8 chrg_stat;
    u8 fault;
    u8 chrg_param_0,chrg_param_1,chrg_param_2;
    int ret;

    ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_STAT, &chrg_stat);
    if (ret){
        ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_STAT, &chrg_stat);
        if (ret){
            pr_err("%s chrg_type =%d,chrg_stat =%d online = %d\n", __func__, sgm->state.chrg_type, sgm->state.chrg_stat, sgm->state.online);
            pr_err("%s read SGM4154x_CHRG_STAT fail\n",__func__);
            return ret;
        }
    }
    state->chrg_type = chrg_stat & SGM4154x_VBUS_STAT_MASK;
    state->chrg_stat = chrg_stat & SGM4154x_CHG_STAT_MASK;
    state->online = !!(chrg_stat & SGM4154x_PG_STAT);
    state->therm_stat = !!(chrg_stat & SGM4154x_THERM_STAT);
    state->vsys_stat = !!(chrg_stat & SGM4154x_VSYS_STAT);

    pr_err("%s chrg_type =%d,chrg_stat =%d online = %d\n",__func__,state->chrg_type,state->chrg_stat,state->online);


    ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_FAULT, &fault);
    if (ret){
        pr_err("%s read SGM4154x_CHRG_FAULT fail\n",__func__);
        return ret;
    }
    state->chrg_fault = fault;
    state->ntc_fault = fault & SGM4154x_TEMP_MASK;
    state->health = state->ntc_fault;
    ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_0, &chrg_param_0);
    if (ret){
        pr_err("%s read SGM4154x_CHRG_CTRL_0 fail\n",__func__);
        return ret;
    }
    state->hiz_en = !!(chrg_param_0 & SGM4154x_HIZ_EN);

    ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_5, &chrg_param_1);
    if (ret){
        pr_err("%s read SGM4154x_CHRG_CTRL_5 fail\n",__func__);
        return ret;
    }
    state->term_en = !!(chrg_param_1 & SGM4154x_TERM_EN);

    ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_a, &chrg_param_2);
    if (ret){
        pr_err("%s read SGM4154x_CHRG_CTRL_a fail\n",__func__);
        return ret;
    }
    state->vbus_gd = !!(chrg_param_2 & SGM4154x_VBUS_GOOD);

    if(state->chrg_stat == 0 || state->chrg_stat == SGM4154x_TERM_CHRG) {
        state->charge_enabled = false;
    } else {
        state->charge_enabled = true;
    }

    return 0;
}

static int sgm4154x_set_hiz(struct charger_device *chg_dev, bool hiz_en)
{
    u8 reg_val;
    struct sgm4154x_device *sgm = charger_get_data(chg_dev);

    dev_notice(sgm->dev, "%s:%d", __func__, hiz_en);
    reg_val = hiz_en ? SGM4154x_HIZ_EN : 0;

    return sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_0,
            SGM4154x_HIZ_EN, reg_val);
}

static int sgm4154x_enable_power_path(struct charger_device *chg_dev, bool en)
{
    struct sgm4154x_device *sgm = charger_get_data(chg_dev);

    dev_notice(sgm->dev, "%s en = %d\n", __func__, en);

    return sgm4154x_set_hiz(chg_dev, !en);
}

static int sgm4154x_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
    struct sgm4154x_device *sgm = charger_get_data(chg_dev);

    dev_notice(sgm->dev, "%s en = %d\n", __func__, en);

    if(en) {
        schedule_delayed_work(&sgm->apsd_work, 10);
    }
    return 0;
}

static int sgm4154x_enable_charger(struct sgm4154x_device *sgm)
{
    int ret;

    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_1, SGM4154x_CHRG_EN,
            SGM4154x_CHRG_EN);
    sgm->charge_enabled = true;
    return ret;
}

static int sgm4154x_disable_charger(struct sgm4154x_device *sgm)
{
    int ret;

    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_1, SGM4154x_CHRG_EN,
            0);
    sgm->charge_enabled = false;
    return ret;
}

static int sgm4154x_charging_switch(struct charger_device *chg_dev,bool enable)
{
    int ret;
    struct sgm4154x_device *sgm = charger_get_data(chg_dev);

    if (enable)
        ret = sgm4154x_enable_charger(sgm);
    else
        ret = sgm4154x_disable_charger(sgm);
    return ret;
}

static int sgm4154x_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct sgm4154x_device *sgm = dev_get_drvdata(&chg_dev->dev);

	*en = sgm->charge_enabled;

	return 0;
}

static int sgm4154x_set_recharge_volt(struct sgm4154x_device *sgm, int mV)
{
    u8 reg_val;

    reg_val = (mV - SGM4154x_VRECHRG_OFFSET_mV) / SGM4154x_VRECHRG_STEP_mV;

    return sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_4,
            SGM4154x_VRECHARGE, reg_val);
}

static int sgm4154x_set_wdt_rst(struct sgm4154x_device *sgm, bool is_rst)
{
    u8 val;

    if (is_rst)
        val = SGM4154x_WDT_RST_MASK;
    else
        val = 0;
    return sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_1,
            SGM4154x_WDT_RST_MASK, val);
}
static int sgm4154x_enable_apsd_bc12(struct sgm4154x_device *sgm)
{
    int ret;

    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_7, SGM4154x_ADSP_BC12,
            SGM4154x_ADSP_BC12);

    pr_err("%s, reg7\n",__func__);
    return ret;
}

static int sgm4154x_get_apsd_status(struct sgm4154x_device *sgm)
{
    int ret;
    u8 reg_val;
    int flag;

    ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_7, &reg_val);
    if (ret)
        return ret;
    flag = (reg_val & SGM4154x_ADSP_STAT_MASK) >> SGM4154x_ADSP_STAT_SHIFT;
    pr_err("input detection reg_val = 0x%x\n", reg_val);
    return flag;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static int sgm4154x_dump_register(struct charger_device *chg_dev)
{
	unsigned char i = 0;
	unsigned int ret = 0;
	unsigned char sgm4154x_reg[SGM4154x_REG_NUM+1] = { 0 };
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);
	char buffer[1024] = {0};
	char *ptr = buffer;

	for (i = 0; i < SGM4154x_REG_NUM + 1; i++) {
		ret = sgm4154x_read_reg(sgm,i, &sgm4154x_reg[i]);
		if (ret == 0)
			ptr += sprintf(ptr, "[0x%0x:0x%0x]",
			       i, sgm4154x_reg[i]);
	}
	pr_info("[%s]%s\n",__func__, buffer);
    return 0;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static int sgm4154x_hw_chipid_detect(struct sgm4154x_device *sgm)
{
    int ret = 0;
    u8 val = 0;
    ret = sgm4154x_read_reg(sgm,SGM4154x_CHRG_CTRL_b,&val);
    if (ret < 0)
    {
        pr_info("[%s] read SGM4154x_CHRG_CTRL_b fail\n", __func__);
        return ret;
    }
    val = val & SGM4154x_PN_MASK;
    pr_info("[%s] Reg[0x0B]=0x%x\n", __func__,val);

    return val;
}

static int sgm4154x_reset_watch_dog_timer(struct charger_device
        *chg_dev)
{
    int ret;
    struct sgm4154x_device *sgm = charger_get_data(chg_dev);

    pr_info("charging_reset_watch_dog_timer\n");

    ret = sgm4154x_set_wdt_rst(sgm,0x1);    /* RST watchdog */

    return ret;
}

static int sgm4154x_get_charging_status(struct charger_device *chg_dev,
        bool *is_done)
{
    //struct sgm4154x_state state;
    struct sgm4154x_device *sgm = charger_get_data(chg_dev);
    //sgm4154x_get_state(sgm, &state);

    if (sgm->state.chrg_stat == SGM4154x_TERM_CHRG)
        *is_done = true;
    else
        *is_done = false;

    return 0;
}

static int sgm4154x_set_vreg_tunning(struct sgm4154x_device *sgm, int tunning)
{
    int ret;
    u8 reg_val;

    switch(tunning) {
        case SGM4154x_TUNNING_NONE:
            reg_val = 0 << SGM4154x_VREG_TUNNING_SHITF;
            break;
        case SGM4154x_TUNNING_P8MV:
            reg_val = 1 << SGM4154x_VREG_TUNNING_SHITF;
            break;
        case SGM4154x_TUNNING_M8MV:
            reg_val = 2 << SGM4154x_VREG_TUNNING_SHITF;
            break;
        case SGM4154x_TUNNING_M16MV:
            reg_val = 3 << SGM4154x_VREG_TUNNING_SHITF;
            break;
        default:
            reg_val = 0 << SGM4154x_VREG_TUNNING_SHITF;
            break;

    }
    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_f,
            SGM4154x_VREG_TUNNIG_MASK, reg_val);
    return ret;
}

static int sgm4154x_set_en_timer(struct sgm4154x_device *sgm)
{
    int ret;

    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_5,
            SGM4154x_SAFETY_TIMER_EN, SGM4154x_SAFETY_TIMER_EN);

    return ret;
}

static int sgm4154x_set_disable_timer(struct sgm4154x_device *sgm)
{
    int ret;

    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_5,
            SGM4154x_SAFETY_TIMER_EN, 0);

    return ret;
}

static int sgm4154x_enable_safetytimer(struct charger_device *chg_dev,bool en)
{
    struct sgm4154x_device *sgm = charger_get_data(chg_dev);
    int ret = 0;

    if (en)
        ret = sgm4154x_set_en_timer(sgm);
    else
        ret = sgm4154x_set_disable_timer(sgm);
    return ret;
}

static int sgm4154x_get_is_safetytimer_enable(struct charger_device
        *chg_dev,bool *en)
{
    int ret = 0;
    u8 val = 0;

    struct sgm4154x_device *sgm = charger_get_data(chg_dev);

    ret = sgm4154x_read_reg(sgm,SGM4154x_CHRG_CTRL_5,&val);
    if (ret < 0)
    {
        pr_info("[%s] read SGM4154x_CHRG_CTRL_5 fail\n", __func__);
        return ret;
    }
    *en = !!(val & SGM4154x_SAFETY_TIMER_EN);
    return 0;
}

#if (defined(__SGM41542_CHIP_ID__)|| defined(__SGM41516D_CHIP_ID__)|| defined(__SGM41543D_CHIP_ID__))
static int sgm4154x_en_pe_current_partern(struct charger_device
        *chg_dev,bool is_up)
{
    int ret = 0;

    struct sgm4154x_device *sgm = charger_get_data(chg_dev);

    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_d,
            SGM4154x_EN_PUMPX, SGM4154x_EN_PUMPX);
    if (ret < 0)
    {
        pr_info("[%s] read SGM4154x_CHRG_CTRL_d fail\n", __func__);
        return ret;
    }
    if (is_up)
        ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_d,
                SGM4154x_PUMPX_UP, SGM4154x_PUMPX_UP);
    else
        ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_d,
                SGM4154x_PUMPX_DN, SGM4154x_PUMPX_DN);
    return ret;
}
#endif

static int sgm4154x_set_boost_current_limit(struct charger_device *chg_dev, u32 uA)
{
    int ret = 0;
    struct sgm4154x_device *sgm = charger_get_data(chg_dev);

    if (uA < BOOST_CURRENT_LIMIT[1]) {
        ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_2, SGM4154x_BOOST_LIM,
                                    0);
    } else {
        ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_2, SGM4154x_BOOST_LIM,
                                    BIT(7));
    }

    return ret;
}

static enum power_supply_property sgm4154x_power_supply_props[] = {
    POWER_SUPPLY_PROP_MANUFACTURER,
    POWER_SUPPLY_PROP_MODEL_NAME,
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_TYPE,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CURRENT_NOW,
    POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
    POWER_SUPPLY_PROP_CHARGE_TYPE,
    POWER_SUPPLY_PROP_USB_TYPE,
    POWER_SUPPLY_PROP_ENERGY_NOW,
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_VOLTAGE_MAX,
    POWER_SUPPLY_PROP_CURRENT_MAX
};

static int sgm4154x_property_is_writeable(struct power_supply *psy,
        enum power_supply_property prop)
{
    switch (prop) {
        case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
        case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
        case POWER_SUPPLY_PROP_ENERGY_NOW://POWER_SUPPLY_PROP_CHARGING_ENABLED
            return true;
        default:
            return false;
    }
}
static int sgm4154x_charger_set_property(struct power_supply *psy,
        enum power_supply_property prop,
        const union power_supply_propval *val)
{
    //struct sgm4154x_device *sgm = power_supply_get_drvdata(psy);
    int ret = -EINVAL;

    switch (prop) {
        case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
            ret = sgm4154x_set_input_curr_lim(s_chg_dev_otg, val->intval);
            break;
        case POWER_SUPPLY_PROP_ENERGY_NOW:
            sgm4154x_charging_switch(s_chg_dev_otg,val->intval);
            break;
        /*  case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
            ret = sgm4154x_set_input_volt_lim(s_chg_dev_otg, val->intval);
            break;*/
        default:
            return -EINVAL;
    }

    return ret;
}

static int sgm4154x_charger_get_property(struct power_supply *psy,
        enum power_supply_property psp,
        union power_supply_propval *val)
{
    struct sgm4154x_device *sgm = power_supply_get_drvdata(psy);
    struct sgm4154x_state state;
    int ret = 0;

    mutex_lock(&sgm->lock);
    //ret = sgm4154x_get_state(sgm, &state);
    state = sgm->state;
    mutex_unlock(&sgm->lock);
    if (ret)
        return ret;

    switch (psp) {
        case POWER_SUPPLY_PROP_STATUS:
            if (!state.chrg_type || (state.chrg_type == SGM4154x_OTG_MODE))
                val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
            else if (state.chrg_stat == SGM4154x_TERM_CHRG)
                val->intval = POWER_SUPPLY_STATUS_FULL;
            else if (!state.chrg_stat || !sgm->charge_enabled)
                val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
            else
                val->intval = POWER_SUPPLY_STATUS_CHARGING;
            break;
        case POWER_SUPPLY_PROP_CHARGE_TYPE:
            switch (state.chrg_stat) {
                case SGM4154x_PRECHRG:
                    val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
                    break;
                case SGM4154x_FAST_CHRG:
                    val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
                    break;
                case SGM4154x_TERM_CHRG:
                    val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
                    break;
                case SGM4154x_NOT_CHRGING:
                    val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
                    break;
                default:
                    val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
            }
            break;

        case POWER_SUPPLY_PROP_USB_TYPE:
            val->intval = sgm->psy_usb_type;
            break;

        case POWER_SUPPLY_PROP_MANUFACTURER:
            val->strval = SGM4154x_MANUFACTURER;
            break;

        case POWER_SUPPLY_PROP_MODEL_NAME:
            val->strval = SGM4154x_NAME;
            break;

        case POWER_SUPPLY_PROP_ONLINE:
            val->intval = state.online;
            break;
        case POWER_SUPPLY_PROP_PRESENT:
            val->intval = state.vbus_gd;
            break;
        case POWER_SUPPLY_PROP_TYPE:
            val->intval = sgm4154x_power_supply_desc.type;
            break;

        case POWER_SUPPLY_PROP_HEALTH:
            if (state.chrg_fault & 0xF8)
                val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
            else
                val->intval = POWER_SUPPLY_HEALTH_GOOD;

            switch (state.health) {
                case SGM4154x_TEMP_HOT:
                    val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
                    break;
                case SGM4154x_TEMP_WARM:
                    val->intval = POWER_SUPPLY_HEALTH_GOOD;
                    break;
                case SGM4154x_TEMP_COOL:
                    val->intval = POWER_SUPPLY_HEALTH_GOOD;
                    break;
                case SGM4154x_TEMP_COLD:
                    val->intval = POWER_SUPPLY_HEALTH_COLD;
                    break;
            }
            break;

        case POWER_SUPPLY_PROP_VOLTAGE_NOW:
            val->intval = battery_get_vbus();
            break;

        case POWER_SUPPLY_PROP_CURRENT_NOW:
            //val->intval = state.ibus_adc;
            break;
            /*  case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
                ret = sgm4154x_get_input_volt_lim(sgm);
                if (ret < 0)
                return ret;

                val->intval = ret;
                break;*/

        case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
            break;

        case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
            break;

        case POWER_SUPPLY_PROP_ENERGY_NOW:
            val->intval = state.charge_enabled;
            break;

        case POWER_SUPPLY_PROP_VOLTAGE_MAX:
            val->intval = battery_get_vbus() * 1000; /* uv */
            break;

        case POWER_SUPPLY_PROP_CURRENT_MAX:
            switch (sgm->psy_usb_type) {
                case POWER_SUPPLY_USB_TYPE_SDP: /* 500mA */
                    val->intval = 500000;
                    break;
                case POWER_SUPPLY_USB_TYPE_CDP: /* 1500mA */
                case POWER_SUPPLY_USB_TYPE_ACA:
                    val->intval = 1500000;
                    break;
                case POWER_SUPPLY_USB_TYPE_DCP: /* 2000mA */
                    val->intval = 2000000;
                    break;
                default :
                    break;
            }
            break;

        default:
            return -EINVAL;
    }

    return ret;
}

#if 0
static bool sgm4154x_state_changed(struct sgm4154x_device *sgm,
        struct sgm4154x_state *new_state)
{
    struct sgm4154x_state old_state;

    mutex_lock(&sgm->lock);
    old_state = sgm->state;
    mutex_unlock(&sgm->lock);

    return (old_state.chrg_type != new_state->chrg_type ||
            old_state.chrg_stat != new_state->chrg_stat     ||
            old_state.online != new_state->online           ||
            old_state.therm_stat != new_state->therm_stat   ||
            old_state.vsys_stat != new_state->vsys_stat     ||
            old_state.chrg_fault != new_state->chrg_fault
           );
}
#endif
static void sgm4154x_chg_type_hvdcp_work(struct work_struct *work)
{
    struct sgm4154x_device * sgm =
        container_of(work, struct sgm4154x_device, hvdcp_work.work);
    int vbus;
    int i;
    /*Set cur is 500ma before do BC1.2 & QC*/
    charger_manager_set_qc_input_current_limit( 500000);
    charger_manager_set_qc_charging_current_limit(500000);
    sgm4154x_set_input_curr_lim(sgm->chg_dev, 500000);
    sgm4154x_set_ichrg_curr(sgm->chg_dev, 500000);

    g_sgm_type.sgm_is_hvdcp = false;
    sgm4154x_charging_enable_hvdcp30(sgm,true);
    msleep(50);
    if (start_count_work) {
        pr_err("%s start count work\n",__func__);
        schedule_delayed_work(&sgm->time_test_work, 1);
        start_count_work = false;
    }
    if (g_num <= 5 && up_vol_test) {
        pr_err("%s counting time<5s, error_times = %d, g_num = %d\n",__func__, error_times, g_num);
        if (error_times++ > 4) {
            error_times = 0;
            g_num = 0;
            pr_err("%s charging times>5, begin low vol charge\n", __func__);
            up_vol_test = false;
            start_count_work = true;
            msleep(100);
        }
    }
    /* HVDCP raises the voltage  */
    if (up_vol_test) {
        for (i=0; i<5; i++) {
            sgm4154x_charging_set_hvdcp30(sgm,true);
        }
    /* Determine whether the voltage is raised successfully */
        for (i = 0; i < 5; i++) {
            vbus = battery_get_vbus();
            if(vbus > 5500) {
                break;
            }
            msleep(200);
        }
    }
    vbus = battery_get_vbus();
    if (vbus >= 5500) {
        sgm4154x_set_input_curr_lim(sgm->chg_dev, 3250000);
        sgm4154x_set_ichrg_curr(sgm->chg_dev, 3600000);
        g_sgm_type.sgm_is_hvdcp = true;
        hvdcp_chgstat_notify();
        pr_err("%s THE ADAPTER IS HVDCP %d",__func__,vbus);
    }
    /* set curr default after BC1.2*/
    charger_manager_set_qc_input_current_limit(-1);
    charger_manager_set_qc_charging_current_limit(-1);
    pr_err("%s HVDCP %d",__func__,vbus);
    return ;
}

static void time_test_work_func(struct work_struct *work)
{
    struct sgm4154x_device * sgm = NULL;
    struct delayed_work *time_test_work = NULL;
    struct sgm4154x_state state;
    int ret = 0;

    time_test_work = container_of(work, struct delayed_work, work);
    if(time_test_work == NULL) {
        pr_err("Cann't get time_test_work\n");
        return ;
    }
    sgm = container_of(time_test_work, struct sgm4154x_device, time_test_work);
    if(sgm == NULL) {
        pr_err("Cann't get sgm \n");
        return ;
    }

    ret = sgm4154x_get_state(sgm, &state);
    if (ret) {
        pr_err("%s: get reg state error\n", __func__);
        return ;
    }

    mutex_lock(&sgm->lock);
    sgm->state = state;
    mutex_unlock(&sgm->lock);
    g_num++;
    if (g_num < 5) {
        schedule_delayed_work(&sgm->time_test_work, 1000);
    } else {
        g_num = 0;
        pr_err("%s counting time>5s,restart count num\n", __func__);
        start_count_work = true;
        error_times = 0;
        up_vol_test = true;
    }
}

static int sgm4154x_get_hvdcp_type(struct charger_device *chg_dev, bool *state)
{
    if (((g_sgm_type.sgm_is_hvdcp == true) &&
            (g_sgm_type.sgm_advanced ==  CHARGERING_18W)) ||
            ((g_qc3_type_set == true) && (g_sgm_type.sgm_advanced ==  CHARGERING_33W))) {
        *state = true;
    } else {
        *state = false;
    }

    return 0;
}
extern void wt6670f_charger_reset_reg(void);

static void sgm4154x_chg_type_apsd_work(struct work_struct *work)
{
    int ret;
    int i = 0;
    struct sgm4154x_device * sgm =
        container_of(work, struct sgm4154x_device, apsd_work.work);

    pr_err("%s enter",__func__);

    /*Set cur is 500ma before do BC1.2 & QC*/
    charger_manager_set_qc_input_current_limit(500000);
    charger_manager_set_qc_charging_current_limit(500000);
    sgm4154x_set_input_curr_lim(sgm->chg_dev, 500000);
    sgm4154x_set_ichrg_curr(sgm->chg_dev, 500000);

    msleep(1000);
    Charger_Detect_Init();
    if (sgm->advanced == CHARGERING_33W) {
        wt6670f_charger_reset_reg();
    }
    sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_HIZ, SGM_DP_DM_VOL_HIZ);
    msleep(100);

    ret = sgm4154x_enable_apsd_bc12(sgm);
    /*BC12 input detection is completed at most 1s*/
    for (i = 0; i < 10; i++){
        msleep(100);
        ret = sgm4154x_get_apsd_status(sgm);
        if (ret){
            pr_err("input detection not completed error ret = %d\n", ret);
        }else {
            pr_err("input detection completed ret = %d\n", ret);
            break;
        }
    }
    Charger_Detect_Release();

    return ;
}

static void charger_monitor_work_func(struct work_struct *work)
{
    int ret = 0;
    struct sgm4154x_device * sgm = NULL;
    struct delayed_work *charge_monitor_work = NULL;
    //static u8 last_chg_method = 0;
    struct sgm4154x_state state;

    charge_monitor_work = container_of(work, struct delayed_work, work);
    if(charge_monitor_work == NULL) {
        pr_err("Cann't get charge_monitor_work\n");
        return ;
    }
    sgm = container_of(charge_monitor_work, struct sgm4154x_device, charge_monitor_work);
    if(sgm == NULL) {
        pr_err("Cann't get sgm \n");
        return ;
    }

    ret = sgm4154x_get_state(sgm, &state);
    if (ret) {
        pr_err("%s: get reg state error\n", __func__);
        goto OUT;
    }

    mutex_lock(&sgm->lock);
    sgm->state = state;
    mutex_unlock(&sgm->lock);

    if (!state.chrg_type || (state.chrg_type == SGM4154x_OTG_MODE)) {
        g_charger_status = POWER_SUPPLY_STATUS_DISCHARGING;
    } else if (!state.chrg_stat) {
        g_charger_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
    } else if (state.chrg_stat == SGM4154x_TERM_CHRG) {
        g_charger_status = POWER_SUPPLY_STATUS_FULL;
    } else {
        g_charger_status = POWER_SUPPLY_STATUS_CHARGING;
    }

    if(!sgm->state.vbus_gd) {
        dev_err(sgm->dev, "Vbus not present, disable charge\n");
        sgm4154x_disable_charger(sgm);
        goto OUT;
    }
    if(!state.online)
    {
        dev_err(sgm->dev, "Vbus not online\n");
        goto OUT;
    }
    sgm4154x_dump_register(sgm->chg_dev);

OUT:
    schedule_delayed_work(&sgm->charge_monitor_work, 10*HZ);
}

extern void wt6670f_get_charger_type_func_work(struct work_struct *work);
extern int wt6670f_reset_charger_type(void);
static void charger_detect_work_func(struct work_struct *work)
{
    static bool type_dect_again = 1;
    struct delayed_work *charge_detect_delayed_work = NULL;
    struct sgm4154x_device * sgm = NULL;
    //static int charge_type_old = 0;
    int curr_in_limit = 0;
    unsigned int non_stand_current_limit = 1000000;
    struct sgm4154x_state state;
    enum charger_type chrg_type = CHARGER_UNKNOWN;
    int ret;
    static bool fullStatus = false;
    g_sgm_type.sgm_is_hvdcp = false;

    pr_err("%s\n",__func__);
    charge_detect_delayed_work = container_of(work, struct delayed_work, work);
    if(charge_detect_delayed_work == NULL) {
        pr_err("Cann't get charge_detect_delayed_work\n");
        return ;
    }

    sgm = container_of(charge_detect_delayed_work, struct sgm4154x_device, charge_detect_delayed_work);
    if(sgm == NULL) {
        pr_err("Cann't get sgm4154x_device\n");
        return ;
    }

    if (!sgm->charger_wakelock->active)
        __pm_stay_awake(sgm->charger_wakelock);

    ret = sgm4154x_get_state(sgm, &state);
    mutex_lock(&sgm->lock);
    sgm->state = state;
    mutex_unlock(&sgm->lock);

    if(!sgm->state.vbus_gd) {
        sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_HIZ);
        dev_err(sgm->dev, "Vbus not present, disable charge\n");
        g_quick_charging_flag = 0;
        type_dect_again = 1;
        fullStatus = false;
        g_qc3_type_set = false;
        Charger_Detect_Init();
        sgm4154x_disable_charger(sgm);
        wt6670f_reset_charger_type();
        chrg_type = CHARGER_UNKNOWN;
        if (sgm->charger_wakelock->active)
            __pm_relax(sgm->charger_wakelock);
        goto err;
    }

    if(!state.online) {
        dev_err(sgm->dev, "vbus is present, but not online, doing poor source detecting\n");
        g_quick_charging_flag = 0;
        type_dect_again = 1;
        fullStatus = false;
        g_qc3_type_set = false;
        Charger_Detect_Init();
        sgm4154x_disable_charger(sgm);
        wt6670f_reset_charger_type();
        chrg_type = CHARGER_UNKNOWN;
        dev_err(sgm->dev, "Relax wakelock\n");
        if (sgm->charger_wakelock->active)
            __pm_relax(sgm->charger_wakelock);
        goto err;
    }

    /* set curr default after BC1.2*/
    if (sgm->state.chrg_type != SGM4154x_NON_STANDARD) {
        charger_manager_set_qc_input_current_limit(-1);
        charger_manager_set_qc_charging_current_limit(-1);
    }

    if(state.online && state.chrg_stat == SGM4154x_TERM_CHRG) {
        dev_err(sgm->dev, "charging terminated\n");
    }

#if (defined(__SGM41542_CHIP_ID__)|| defined(__SGM41516D_CHIP_ID__)|| defined(__SGM41543D_CHIP_ID__))
    switch(sgm->state.chrg_type) {
        case SGM4154x_USB_SDP:
            sgm4154x_power_supply_desc.type = POWER_SUPPLY_TYPE_USB;
            sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
            chrg_type = STANDARD_HOST;
            pr_err("SGM4154x charger type: SDP\n");
            curr_in_limit = 500000;
            break;

        case SGM4154x_USB_CDP:
            sgm4154x_power_supply_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
            sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
            chrg_type = CHARGING_HOST;
            pr_err("SGM4154x charger type: CDP\n");
            curr_in_limit = 1500000;
            break;

        case SGM4154x_USB_DCP:
            sgm4154x_power_supply_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
            sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
            chrg_type = STANDARD_CHARGER;
            pr_err("SGM4154x charger type: DCP\n");
            /* in terminated status forbiden QC charge */
            if(state.online && state.chrg_stat == SGM4154x_TERM_CHRG && !fullStatus) {
                fullStatus = true;
            } else if (fullStatus) {
                goto err;
            }

            if (sgm->advanced == CHARGERING_18W) {
                sgm4154x_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_HIZ);
                schedule_delayed_work(&sgm->hvdcp_work, msecs_to_jiffies(1400));
                g_sgm_type.sgm_advanced = sgm->advanced;
            } else if(sgm->advanced == CHARGERING_33W) {
                schedule_delayed_work(&sgm->psy_work, 0);
                g_sgm_type.sgm_advanced = sgm->advanced;
            } else {
                //do nothing
            }
            curr_in_limit = 2000000;
            break;

        case SGM4154x_NON_STANDARD:
            sgm4154x_power_supply_desc.type = POWER_SUPPLY_TYPE_USB_ACA;
            chrg_type = NONSTANDARD_CHARGER;
            curr_in_limit = 1000000;
            ret = sgm4154x_get_input_curr_lim(sgm->chg_dev,&non_stand_current_limit);
            if (!ret) {
                curr_in_limit = non_stand_current_limit;
                switch(curr_in_limit) {
                case SGM4154x_APPLE_1A:
                   sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID;
                   chrg_type = APPLE_1_0A_CHARGER;
                   break;
                case SGM4154x_APPLE_2A:
                case SGM4154x_APPLE_2P1A:
                case SGM4154x_APPLE_2P4A:
                   sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_ACA;
                   chrg_type = APPLE_2_1A_CHARGER;
                   break;
                default:
                   sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID;
                   chrg_type = APPLE_1_0A_CHARGER;
                   break;
               }
            }
            charger_manager_set_qc_input_current_limit(-1);
            charger_manager_set_qc_charging_current_limit( -1);
            pr_err("SGM4154x charger type: NON_STANDARD_ADAPTER, curr_in_limit = %d \n", curr_in_limit);
            break;

        case SGM4154x_UNKNOWN:
            sgm4154x_power_supply_desc.type = POWER_SUPPLY_TYPE_USB;
            sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
            chrg_type = NONSTANDARD_CHARGER;
            pr_err("SGM4154x charger type: UNKNOWN\n");
            curr_in_limit = 500000;
            if (type_dect_again) {
                type_dect_again = 0;
                schedule_delayed_work(&sgm->apsd_work, 1000);
                pr_err("[%s]: chrg_type = unknown, try again \n", __func__);
                goto err;
            }
            break;

        default:
            pr_err("SGM4154x charger type: default\n");
            //curr_in_limit = 500000;
            //break;
            //__pm_relax(sgm->charger_wakelock);
            return;
    }

    if ( sgm->state.chrg_type != SGM4154x_USB_DCP){
        Charger_Detect_Release();
    }

    //set charge parameters
    dev_err(sgm->dev, "Update: curr_in_limit = %d\n", curr_in_limit);
    sgm4154x_set_input_curr_lim(sgm->chg_dev, curr_in_limit);

#endif
    //enable charge
    sgm4154x_enable_charger(sgm);
    //    sgm4154x_dump_register(sgm->chg_dev);

err:
    //release wakelock
    power_supply_changed(sgm->charger);
    //dev_err(sgm->dev, "Relax wakelock\n");
    //__pm_relax(sgm->charger_wakelock);
    return;
}

static irqreturn_t sgm4154x_irq_handler_thread(int irq, void *private)
{
    struct sgm4154x_device *sgm = private;
    pr_err("%s\n",__func__);
    charger_detect_work_func(&sgm->charge_detect_delayed_work.work);
    return IRQ_HANDLED;
}
static char *sgm4154x_charger_supplied_to[] = {
    "battery",
    /*Penang code for EKPENAN4GU-2057 by hq_zhangjiangbin_tmp at 2024/3/18 start*/
    "mtk-master-charger",
    /*Penang code for EKPENAN4GU-2057 by hq_zhangjiangbin_tmp at 2024/3/18 end*/
};

static struct power_supply_desc sgm4154x_power_supply_desc = {
    .name = "sgm4154x-charger",
    .type = POWER_SUPPLY_TYPE_USB,
    .usb_types = sgm4154x_usb_type,
    .num_usb_types = ARRAY_SIZE(sgm4154x_usb_type),
    .properties = sgm4154x_power_supply_props,
    .num_properties = ARRAY_SIZE(sgm4154x_power_supply_props),
    .get_property = sgm4154x_charger_get_property,
    .set_property = sgm4154x_charger_set_property,
    .property_is_writeable = sgm4154x_property_is_writeable,
};

static int sgm4154x_power_supply_init(struct sgm4154x_device *sgm,
        struct device *dev)
{
    struct power_supply_config psy_cfg = { .drv_data = sgm,
        .of_node = dev->of_node, };

    psy_cfg.supplied_to = sgm4154x_charger_supplied_to;
    psy_cfg.num_supplicants = ARRAY_SIZE(sgm4154x_charger_supplied_to);
    pr_err("%s:num_supplicants is %d\n", __func__, psy_cfg.num_supplicants);
    sgm->charger = devm_power_supply_register(sgm->dev,
            &sgm4154x_power_supply_desc,
            &psy_cfg);
    if (IS_ERR(sgm->charger))
        return -EINVAL;

    return 0;
}

static int sgm4154x_hw_init(struct sgm4154x_device *sgm)
{
    int ret = 0;
    struct power_supply_battery_info bat_info = { };

    bat_info.constant_charge_current_max_ua =
        SGM4154x_ICHRG_I_DEF_uA;

    bat_info.constant_charge_voltage_max_uv =
        SGM4154x_VREG_V_DEF_uV;

    bat_info.precharge_current_ua =
        SGM4154x_PRECHRG_I_DEF_uA;

    bat_info.charge_term_current_ua =
        SGM4154x_TERMCHRG_I_DEF_uA;

    sgm->init_data.max_ichg =
        SGM4154x_ICHRG_I_MAX_uA;

    sgm->init_data.max_vreg =
        SGM4154x_VREG_V_MAX_uV;

    sgm4154x_set_watchdog_timer(sgm,0);

    ret = sgm4154x_set_ichrg_curr(s_chg_dev_otg,
            bat_info.constant_charge_current_max_ua);
    if (ret)
        goto err_out;

    ret = sgm4154x_set_prechrg_curr(sgm, bat_info.precharge_current_ua);
    if (ret)
        goto err_out;

    ret = sgm4154x_set_chrg_volt(s_chg_dev_otg,
            bat_info.constant_charge_voltage_max_uv);
    if (ret)
        goto err_out;

    ret = sgm4154x_set_term_curr(sgm, bat_info.charge_term_current_ua);
    if (ret)
        goto err_out;

    ret = sgm4154x_set_input_volt_lim(s_chg_dev_otg, sgm->init_data.vlim);
      if (ret)
      goto err_out;

    ret = sgm4154x_set_input_curr_lim(s_chg_dev_otg, sgm->init_data.ilim);
    if (ret)
        goto err_out;

    ret = sgm4154x_set_boost_current_limit(s_chg_dev_otg, 1200000);//default boost current limit
    if(ret)
        goto err_out;
#if 0
    ret = sgm4154x_set_vac_ovp(sgm);//14V
    if (ret)
        goto err_out;
#endif
    ret = sgm4154x_set_recharge_volt(sgm, 200);//100~200mv
    if (ret)
        goto err_out;

    ret = sgm4154x_set_vreg_tunning(sgm, 0);
    if(ret)
        goto err_out;

    dev_notice(sgm->dev, "ichrg_curr:%d prechrg_curr:%d chrg_vol:%d"
            " term_curr:%d input_curr_lim:%d",
            bat_info.constant_charge_current_max_ua,
            bat_info.precharge_current_ua,
            bat_info.constant_charge_voltage_max_uv,
            bat_info.charge_term_current_ua,
            sgm->init_data.ilim);

    /*VINDPM*/
    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_a,
            SGM4154x_VINDPM_V_MASK, 0x3);

    /*ts error-- temp disable jeita */
    ret = __sgm4154x_write_byte(sgm, SGM4154x_CHRG_CTRL_d, 0x0);

    return 0;

err_out:
    return ret;

}

static int sgm4154x_parse_dt(struct sgm4154x_device *sgm)
{
    int ret;
    int irq_gpio = 0, irqn = 0;
    int chg_en_gpio = 0;

    ret = device_property_read_u32(sgm->dev,
            "input-voltage-limit-microvolt",
            &sgm->init_data.vlim);
    if (ret)
        sgm->init_data.vlim = SGM4154x_VINDPM_DEF_uV;

    if (sgm->init_data.vlim > SGM4154x_VINDPM_V_MAX_uV ||
            sgm->init_data.vlim < SGM4154x_VINDPM_V_MIN_uV) {
        pr_err("sgm4154x parse dts fail\n");
        return -EINVAL;
    }

    ret = device_property_read_u32(sgm->dev,
            "input-current-limit-microamp",
            &sgm->init_data.ilim);
    if (ret)
        sgm->init_data.ilim = SGM4154x_IINDPM_DEF_uA;

    if (sgm->init_data.ilim > SGM4154x_IINDPM_I_MAX_uA ||
            sgm->init_data.ilim < SGM4154x_IINDPM_I_MIN_uA)
        return -EINVAL;

    ret = device_property_read_u32(sgm->dev, "sgm,is-advanced",
            &sgm->advanced);
    if (ret) {
        pr_err("no type of machine %d\n",&sgm->advanced);
    }

    irq_gpio = of_get_named_gpio(sgm->dev->of_node, "sgm,irq-gpio", 0);
    if (!gpio_is_valid(irq_gpio))
    {
        dev_err(sgm->dev, "%s: %d gpio get failed\n", __func__, irq_gpio);
        return -EINVAL;
    }
    ret = gpio_request(irq_gpio, "sgm4154x irq pin");
    if (ret) {
        dev_err(sgm->dev, "%s: %d gpio request failed\n", __func__, irq_gpio);
        return ret;
    }
    gpio_direction_input(irq_gpio);
    irqn = gpio_to_irq(irq_gpio);
    if (irqn < 0) {
        dev_err(sgm->dev, "%s:%d gpio_to_irq failed\n", __func__, irqn);
        return irqn;
    }
    sgm->client->irq = irqn;

    chg_en_gpio = of_get_named_gpio(sgm->dev->of_node, "sgm,chg-en-gpio", 0);
    if (!gpio_is_valid(chg_en_gpio))
    {
        dev_err(sgm->dev, "%s: %d gpio get failed\n", __func__, chg_en_gpio);
        return -EINVAL;
    }
    ret = gpio_request(chg_en_gpio, "sgm chg en pin");
    if (ret) {
        dev_err(sgm->dev, "%s: %d gpio request failed\n", __func__, chg_en_gpio);
        return ret;
    }
    gpio_direction_output(chg_en_gpio,0);//default enable charge
    return 0;
}

int sgm4154x_set_ieoc(struct charger_device *chg_dev, u32 uA)
{
    int ret = 0;
    struct sgm4154x_device *sgm = charger_get_data(s_chg_dev_otg);

    pr_err("%s sgm4154x_set_ieoc %d\n", __func__, uA);

    ret = sgm4154x_set_term_curr(sgm, uA);

    return ret;
}

static int sgm4154x_enable_vbus(struct regulator_dev *rdev)
{
    int ret = 0;
    struct sgm4154x_device *sgm = charger_get_data(s_chg_dev_otg);

    pr_err("%s enable_vbus\n", __func__);

    Charger_Detect_Release();
    sgm4154x_set_hiz(sgm->chg_dev, false);
    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_1, SGM4154x_OTG_EN,
            SGM4154x_OTG_EN);
    return ret;
}

static int sgm4154x_disable_vbus(struct regulator_dev *rdev)
{
    int ret = 0;
    struct sgm4154x_device *sgm = charger_get_data(s_chg_dev_otg);
    pr_err("%s disable_vbus\n", __func__);

    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_1, SGM4154x_OTG_EN,
            0);

    return ret;
}

static int sgm4154x_is_enabled_vbus(struct regulator_dev *rdev)
{
    u8 temp = 0;
    int ret = 0;
    struct sgm4154x_device *sgm = charger_get_data(s_chg_dev_otg);
    pr_err("%s -----\n", __func__);

    ret = sgm4154x_read_reg(sgm, SGM4154x_CHRG_CTRL_1, &temp);
    return (temp&SGM4154x_OTG_EN)? 1 : 0;
}

static int sgm4154x_enable_otg(struct charger_device *chg_dev, bool en)
{
    int ret = 0;

    pr_info("%s en = %d\n", __func__, en);
    if (en) {
        ret = sgm4154x_enable_vbus(NULL);
    } else {
        ret = sgm4154x_disable_vbus(NULL);
    }
    return ret;
}

#if 0
static int sgm4154x_set_boost_voltage_limit(struct charger_device
        *chg_dev, u32 uV)
{
    int ret = 0;
    char reg_val = -1;
    int i = 0;
    struct sgm4154x_device *sgm = charger_get_data(chg_dev);

    while(i<4){
        if (uV == BOOST_VOLT_LIMIT[i]){
            reg_val = i;
            break;
        }
        i++;
    }
    if (reg_val < 0)
        return reg_val;
    reg_val = reg_val << 4;
    ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_6,
            SGM4154x_BOOSTV, reg_val);

    return ret;
}
#endif

static struct regulator_ops sgm4154x_vbus_ops = {
    .enable = sgm4154x_enable_vbus,
    .disable = sgm4154x_disable_vbus,
    .is_enabled = sgm4154x_is_enabled_vbus,
};

static const struct regulator_desc sgm4154x_otg_rdesc = {
    .of_match = "usb-otg-vbus",
    .name = "usb-otg-vbus",
    .ops = &sgm4154x_vbus_ops,
    .owner = THIS_MODULE,
    .type = REGULATOR_VOLTAGE,
    .fixed_uV = 5000000,
    .n_voltages = 1,
};

static int sgm4154x_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	struct sgm4154x_device *sgm = charger_get_data(chg_dev);

	switch (event) {
	case EVENT_FULL:
	case EVENT_RECHARGE:
	case EVENT_DISCHARGE:
		power_supply_changed(sgm->charger);
		break;
	default:
		break;
	}

	return 0;
}

static int sgm4154x_vbus_regulator_register(struct sgm4154x_device *sgm)
{
    struct regulator_config config = {};
    int ret = 0;
    /* otg regulator */
    config.dev = sgm->dev;
    config.driver_data = sgm;
    sgm->otg_rdev = devm_regulator_register(sgm->dev,
            &sgm4154x_otg_rdesc, &config);
    sgm->otg_rdev->constraints->valid_ops_mask |= REGULATOR_CHANGE_STATUS;
    if (IS_ERR(sgm->otg_rdev)) {
        ret = PTR_ERR(sgm->otg_rdev);
        pr_info("%s: register otg regulator failed (%d)\n", __func__, ret);
    }
    return ret;
}

static int sgm4154x_enable_termination(struct charger_device *chg_dev,bool en)
{
    struct sgm4154x_device *sgm = charger_get_data(chg_dev);
    int ret = 0;
    if (en){
        ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_5,
                SGM4154x_TERM_EN, SGM4154x_TERM_EN);
        pr_info("[%s] enable termination success!\n", __func__);
    } else {
        ret = sgm4154x_update_bits(sgm, SGM4154x_CHRG_CTRL_5,
                SGM4154x_TERM_EN, 0);
        pr_info("[%s] disable termination success!\n", __func__);
    }
    return ret;
}

static struct charger_ops sgm4154x_chg_ops = {
    .enable_hz = sgm4154x_set_hiz,

    /* Normal charging */
    .dump_registers = sgm4154x_dump_register,
    .enable = sgm4154x_charging_switch,
    .is_enabled = sgm4154x_is_charging_enable,
    .get_charging_current = sgm4154x_get_ichg_curr,
    .get_tchg_adc = NULL,
    .set_charging_current = sgm4154x_set_ichrg_curr,
    .get_input_current = sgm4154x_get_input_curr_lim,
    .set_input_current = sgm4154x_set_input_curr_lim,
    .get_constant_voltage = sgm4154x_get_chrg_volt,
    .set_constant_voltage = sgm4154x_set_chrg_volt,
    .kick_wdt = sgm4154x_reset_watch_dog_timer,
    .set_mivr = sgm4154x_set_input_volt_lim,
    .is_charging_done = sgm4154x_get_charging_status,
    .enable_termination = sgm4154x_enable_termination,

    .get_charger_type_hvdcp = sgm4154x_get_hvdcp_type,
    .set_eoc_current = sgm4154x_set_ieoc,
    /* Safety timer */
    .enable_safety_timer = sgm4154x_enable_safetytimer,
    .is_safety_timer_enabled = sgm4154x_get_is_safetytimer_enable,
    .event = sgm4154x_do_event,

    /* Power path */
    .enable_powerpath = sgm4154x_enable_power_path,
    /*.is_powerpath_enabled = sgm4154x_get_is_power_path_enable, */

    /* Charger type detection */
    .enable_chg_type_det = sgm4154x_enable_chg_type_det,

    /* OTG */
    .enable_otg = sgm4154x_enable_otg,
    .set_boost_current_limit = sgm4154x_set_boost_current_limit,
    //.event = sgm4154x_do_event,
    /* PE+/PE+20 */
#if (defined(__SGM41542_CHIP_ID__)|| defined(__SGM41516D_CHIP_ID__)|| defined(__SGM41543D_CHIP_ID__))
    .send_ta_current_pattern = sgm4154x_en_pe_current_partern,
#else
    .send_ta_current_pattern = NULL,
#endif
    /*.set_pe20_efficiency_table = NULL,*/
    /*.send_ta20_current_pattern = NULL,*/
    /*.set_ta20_reset = NULL,*/
    /*.enable_cable_drop_comp = NULL,*/
};

static int sgm4154x_driver_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
    int ret = 0;
    struct device *dev = &client->dev;
    struct sgm4154x_device *sgm;

    char *name = NULL;

    sgm = devm_kzalloc(dev, sizeof(*sgm), GFP_KERNEL);
    if (!sgm) {
        pr_err("sgm4154x alloc fail\n");
        return -ENOMEM;
    }

    sgm->client = client;
    sgm->dev = dev;

    mutex_init(&sgm->lock);
    mutex_init(&sgm->i2c_rw_lock);

    i2c_set_clientdata(client, sgm);

    ret = sgm4154x_hw_chipid_detect(sgm);
    if (ret != SGM4154x_PN_ID){
        pr_err("[%s] device not found !!!\n", __func__);
        return ret;
    }

    ret = sgm4154x_parse_dt(sgm);
    if (ret) {
        pr_err("sgm4154x parse dts fail\n");
        return ret;
    }

    name = devm_kasprintf(sgm->dev, GFP_KERNEL, "%s","sgm4154x suspend wakelock");
    sgm->charger_wakelock = wakeup_source_register(sgm->dev, name);

    /* Register charger device */
    sgm4154x_chg_ops.get_tchg_adc = NULL;
    sgm->chg_dev = charger_device_register("primary_chg",
            &client->dev, sgm,
            &sgm4154x_chg_ops,
            &sgm4154x_chg_props);
    if (IS_ERR_OR_NULL(sgm->chg_dev)) {
        pr_info("%s: register charger device  failed\n", __func__);
        ret = PTR_ERR(sgm->chg_dev);
        return ret;
    }

    /* otg regulator */
    s_chg_dev_otg=sgm->chg_dev;

    INIT_DELAYED_WORK(&sgm->charge_detect_delayed_work, charger_detect_work_func);
    INIT_DELAYED_WORK(&sgm->charge_monitor_work, charger_monitor_work_func);
    INIT_DELAYED_WORK(&sgm->hvdcp_work, sgm4154x_chg_type_hvdcp_work);
    INIT_DELAYED_WORK(&sgm->apsd_work, sgm4154x_chg_type_apsd_work);
    INIT_DELAYED_WORK(&sgm->psy_work, wt6670f_get_charger_type_func_work);//add by chan
    INIT_DELAYED_WORK(&sgm->time_test_work, time_test_work_func);

    if (client->irq) {
        ret = devm_request_threaded_irq(dev, client->irq, NULL,
                sgm4154x_irq_handler_thread,
                IRQF_TRIGGER_FALLING |
                IRQF_ONESHOT,
                dev_name(&client->dev), sgm);
        if (ret) {
            pr_err("%d:sgm4154x irq register fail\n", __LINE__);
            return ret;
        }
        enable_irq_wake(client->irq);
    }

    ret = sgm4154x_power_supply_init(sgm, dev);
    if (ret) {
        pr_err("Failed to register power supply\n");
        return ret;
    }

    ret = sgm4154x_hw_init(sgm);
    if (ret) {
        dev_err(dev, "Cannot initialize the chip.\n");
        return ret;
    }

    //OTG setting
    //sgm4154x_set_otg_voltage(s_chg_dev_otg, 5000000); //5V
    //sgm4154x_set_otg_current(s_chg_dev_otg, 1200000); //1.2A
    ret = sgm4154x_vbus_regulator_register(sgm);
    schedule_delayed_work(&sgm->charge_monitor_work, 100);
    schedule_delayed_work(&sgm->charge_detect_delayed_work, 10*HZ);

    if (sgm->advanced == CHARGERING_18W) {
        pr_err("%d:sgm4154x probe TO 4G\n", __LINE__);
    }
    pr_err("%d:sgm4154x probe done\n", __LINE__);
    return ret;
}

static int sgm4154x_charger_remove(struct i2c_client *client)
{
    struct sgm4154x_device *sgm = i2c_get_clientdata(client);

    cancel_delayed_work_sync(&sgm->charge_monitor_work);

    regulator_unregister(sgm->otg_rdev);

    power_supply_unregister(sgm->charger);

    mutex_destroy(&sgm->lock);
    mutex_destroy(&sgm->i2c_rw_lock);

    return 0;
}

static void sgm4154x_charger_shutdown(struct i2c_client *client)
{
    int ret = 0;
    struct sgm4154x_device *sgm = i2c_get_clientdata(client);

    pr_info("sgm4154x_charger_shutdown\n");

    ret =  __sgm4154x_write_byte(sgm, SGM4154x_CHRG_CTRL_b, 0x80);
    if (ret < 0) {
        pr_err("sgm4154x register reset fail\n");
    }

    ret = sgm4154x_disable_charger(sgm);
    if (ret) {
        pr_err("Failed to disable charger, ret = %d\n", ret);
    }
}

static const struct i2c_device_id sgm4154x_i2c_ids[] = {
    { "sgm41541", 0 },
    { "sgm41542", 1 },
    { "sgm41543", 2 },
    { "sgm41543D", 3 },
    { "sgm41513", 4 },
    { "sgm41513A", 5 },
    { "sgm41513D", 6 },
    { "sgm41516", 7 },
    { "sgm41516D", 8 },
    {},
};
MODULE_DEVICE_TABLE(i2c, sgm4154x_i2c_ids);

static const struct of_device_id sgm4154x_of_match[] = {
    { .compatible = "sgm,sgm41541", },
    { .compatible = "sgm,sgm41542", },
    { .compatible = "sgm,sgm41543", },
    { .compatible = "sgm,sgm41543D", },
    { .compatible = "sgm,sgm41513", },
    { .compatible = "sgm,sgm41513A", },
    { .compatible = "sgm,sgm41513D", },
    { .compatible = "sgm,sgm41516", },
    { .compatible = "sgm,sgm41516D", },
    { },
};
MODULE_DEVICE_TABLE(of, sgm4154x_of_match);


static struct i2c_driver sgm4154x_driver = {
    .driver = {
        .name = "sgm4154x-charger",
        .of_match_table = sgm4154x_of_match,
    },
    .probe = sgm4154x_driver_probe,
    .remove = sgm4154x_charger_remove,
    .shutdown = sgm4154x_charger_shutdown,
    .id_table = sgm4154x_i2c_ids,
};
module_i2c_driver(sgm4154x_driver);

MODULE_AUTHOR(" qhq <Allen_qin@sg-micro.com>");
MODULE_DESCRIPTION("sgm4154x charger driver");
MODULE_LICENSE("GPL v2");

