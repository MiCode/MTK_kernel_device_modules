// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
 #include <linux/acpi.h>
 #include <linux/thermal.h>
 #include <linux/platform_device.h>
 #include <mt-plat/aee.h>
 #include <linux/types.h>
 #include <linux/delay.h>
 #include <linux/proc_fs.h>
 #include <linux/syscalls.h>
 #include <linux/sched.h>
 #include <linux/writeback.h>
 #include <linux/uaccess.h>
 #include "mt-plat/mtk_thermal_monitor.h"
 #include "mach/mtk_thermal.h"
 #include "mtk_thermal_timer.h"
 #include "mt-plat/mtk_thermal_platform.h"
 #include <linux/uidgid.h>
 #include <tmp_bts.h>
 #include <linux/slab.h>
 #ifdef APPLY_PRECISE_CHARGEIC_TEMP
 #include <linux/iio/consumer.h>
 #endif
 /*=============================================================
  *Weak functions
  *=============================================================
  */
 #ifndef APPLY_PRECISE_CHARGEIC_TEMP
 int __attribute__ ((weak))
 IMM_IsAdcInitReady(void)
 {
 	pr_notice("E_WF: %s doesn't exist\n", __func__);
 	return 0;
 }
 int __attribute__ ((weak))
 IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata)
 {
 	pr_notice("E_WF: %s doesn't exist\n", __func__);
 	return -1;
 }
 #endif
 int __attribute__ ((weak))
 tsdctm_thermal_get_ttj_on(void)
 {
 	return 0;
 }
 /*=============================================================*/
 static DEFINE_SEMAPHORE(sem_mutex, 1);
 static unsigned int interval = 1;	/* seconds, 0 : no auto polling */
 static struct thermal_zone_device *thz_dev;
 static int num_trip = 1;
 static struct thermal_trip trips[10];
 /**
  * If curr_temp >= polling_trip_temp1, use interval
  * else if cur_temp >= polling_trip_temp2 && curr_temp < polling_trip_temp1,
  *	use interval*polling_factor1
  * else, use interval*polling_factor2
  */
 static int polling_trip_temp1 = 40000;
 static int polling_trip_temp2 = 20000;
 static int polling_factor1 = 5000;
 static int polling_factor2 = 10000;
 #define mtkts_chargeic_dprintk(fmt, args...)   \
 pr_err("[Thermal/TZ/CHARGEIC]" fmt, ##args)

 #define mtkts_chargeic_printk(fmt, args...) \
 pr_err("[Thermal/TZ/CHARGEIC]" fmt, ##args)
 struct CHARGEIC_TEMPERATURE {
 	__s32 CHARGEIC_Temp;
 	__s32 TemperatureR;
 };
 
 struct iio_channel *spark_thermistor_ch2;
 
 static int g_RAP_pull_up_R = CHARGEIC_RAP_PULL_UP_R;
 static int g_TAP_over_critical_low = BTCHARGEIC_TAP_OVER_CRITICAL_LOW;
 static int g_RAP_pull_up_voltage = BTCHARGEIC_RAP_PULL_UP_VOLTAGE;
 static int g_RAP_ntc_table = BTCHARGEIC_RAP_NTC_TABLE;
 static int g_btsctherm_TemperatureR;
 static struct CHARGEIC_TEMPERATURE *Temperature_Table;
 static int ntc_tbl_size;
 /* NCP15WF104F03RC(100K) */
 static struct CHARGEIC_TEMPERATURE CHARGEIC_Temperature_Table[] = {
 	{-40, 4397119},
 	{-35, 3088599},
 	{-30, 2197225},
 	{-25, 1581881},
 	{-20, 1151037},
	{-15, 846579},
 	{-10, 628988},
 	{-5, 471632},
 	{0, 357012},
 	{5, 272500},
 	{10, 209710},
 	{15, 162651},
 	{20, 127080},
 	{25, 100000},		/* 100K */
 	{30, 79222},
 	{35, 63167},
 #if defined(APPLY_PRECISE_NTC_TABLE)
 	{40, 50677},
 	{41, 48528},
 	{42, 46482},
 	{43, 44533},
 	{44, 42675},
 	{45, 40904},
 	{46, 39213},
 	{47, 37601},
 	{48, 36063},
 	{49, 34595},
 	{50, 33195},
 	{51, 31859},
 	{52, 30584},
 	{53, 29366},
 	{54, 28203},
 	{55, 27091},
 	{56, 26028},
 	{57, 25013},
 	{58, 24042},
 	{59, 23113},
 	{60, 22224},
 	{61, 21374},
 	{62, 20560},
 	{63, 19782},
 	{64, 19036},
 	{65, 18322},
 	{66, 17640},
 	{67, 16986},
 	{68, 16360},
 	{69, 15759},
 	{70, 15184},
 	{71, 14631},
 	{72, 14100},
 	{73, 13591},
 	{74, 13103},
 	{75, 12635},
 	{76, 12187},
 	{77, 11756},
 	{78, 11343},
 	{79, 10946},
 	{80, 10565},
 	{81, 10199},
 	{82,  9847},
 	{83,  9509},
 	{84,  9184},
 	{85,  8872},
 	{86,  8572},
 	{87,  8283},
 	{88,  8005},
 	{89,  7738},
 	{90,  7481},
 #else
 	{40, 50677},
 	{45, 40904},
 	{50, 33195},
 	{55, 27091},
 	{60, 22224},
 	{65, 18323},
 	{70, 15184},
 	{75, 12635},
 	{80, 10566},
 	{85, 8873},
 	{90, 7481},
 #endif
 	{95, 6337},
 	{100, 5384},
 	{105, 4594},
 	{110, 3934},
 	{115, 3380},
 	{120, 2916},
 	{125, 2522}
 };
 
 void mtkts_chargeic_prepare_table(int table_num)
 {
 	/* NCP15WF104F03RC */
 	Temperature_Table = CHARGEIC_Temperature_Table;
 	ntc_tbl_size = sizeof(CHARGEIC_Temperature_Table);
 	pr_notice("[Thermal/TZ/CHARGEIC] %s table_num=%d\n", __func__, table_num);
 }
 /* convert register to temperature  */
 static __s32 mtkts_chargeic_thermistor_conver_temp(__s32 Res)
 {
 	int i = 0;
 	int asize = 0;
 	__s32 RES1 = 0, RES2 = 0;
 	__s32 TAP_Value = -200, TMP1 = 0, TMP2 = 0;
 #ifdef APPLY_PRECISE_CHARGEIC_TEMP
 	TAP_Value = TAP_Value * 1000;
 #endif
 	asize = (ntc_tbl_size / sizeof(struct CHARGEIC_TEMPERATURE));
 	if (Res >= Temperature_Table[0].TemperatureR) {
 		TAP_Value = -40;	/* min */
 #ifdef APPLY_PRECISE_CHARGEIC_TEMP
 		TAP_Value = TAP_Value * 1000;
 #endif
 	} else if (Res <= Temperature_Table[asize - 1].TemperatureR) {
 		TAP_Value = 125;	/* max */
 #ifdef APPLY_PRECISE_CHARGEIC_TEMP
 		TAP_Value = TAP_Value * 1000;
 #endif
 	} else {
 		RES1 = Temperature_Table[0].TemperatureR;
 		TMP1 = Temperature_Table[0].CHARGEIC_Temp;
 		for (i = 0; i < asize; i++) {
 			if (Res >= Temperature_Table[i].TemperatureR) {
 				RES2 = Temperature_Table[i].TemperatureR;
 				TMP2 = Temperature_Table[i].CHARGEIC_Temp;
 				break;
 			}
 			RES1 = Temperature_Table[i].TemperatureR;
 			TMP1 = Temperature_Table[i].CHARGEIC_Temp;
 		}
 #ifdef APPLY_PRECISE_CHARGEIC_TEMP
 		TAP_Value = mult_frac((((Res - RES2) * TMP1) +
 			((RES1 - Res) * TMP2)), 1000, (RES1 - RES2));
 #else
 		TAP_Value = (((Res - RES2) * TMP1) + ((RES1 - Res) * TMP2))
 								/ (RES1 - RES2);
 #endif
 	}
 	pr_err("chargeic TAP_Value = %d\n", TAP_Value);
 	return TAP_Value;
 }
 /* convert ADC_AP_temp_volt to register */
 /*Volt to Temp formula same with 6589*/
 static __s32 mtk_ts_chargeic_volt_to_temp(__u32 dwVolt)
 {
 	__s32 TRes;
 	__u64 dwVCriAP = 0;
 	__s32 CHARGEIC_TMP = -100;
 	__u64 dwVCriAP2 = 0;
 	/* SW workaround-----------------------------------------------------
 	 * dwVCriAP = (TAP_OVER_CRITICAL_LOW * 1800) /
 	 *		(TAP_OVER_CRITICAL_LOW + 39000);
 	 * dwVCriAP = (TAP_OVER_CRITICAL_LOW * RAP_PULL_UP_VOLT) /
 	 *		(TAP_OVER_CRITICAL_LOW + RAP_PULL_UP_R);
 	 */
 	dwVCriAP = ((__u64)g_TAP_over_critical_low *
 		(__u64)g_RAP_pull_up_voltage);
 	dwVCriAP2 = (g_TAP_over_critical_low + g_RAP_pull_up_R);
 	do_div(dwVCriAP, dwVCriAP2);
 #ifdef APPLY_PRECISE_CHARGEIC_TEMP
 	if ((dwVolt / 100) > ((__u32)dwVCriAP)) {
 		TRes = g_TAP_over_critical_low;
 	} else {
 /* TRes = (39000*dwVolt) / (1800-dwVolt); */
 		/* TRes = (RAP_PULL_UP_R*dwVolt) / (RAP_PULL_UP_VOLT-dwVolt); */
 		TRes = ((long long)g_RAP_pull_up_R * dwVolt) /
 					(g_RAP_pull_up_voltage * 100 - dwVolt);
 	}
 #else
 	if (dwVolt > ((__u32)dwVCriAP)) {
 		TRes = g_TAP_over_critical_low;
 	} else {
 		/* TRes = (39000*dwVolt) / (1800-dwVolt); */
 		/* TRes = (RAP_PULL_UP_R*dwVolt) / (RAP_PULL_UP_VOLT-dwVolt); */
 		TRes = (g_RAP_pull_up_R * dwVolt) /
 					(g_RAP_pull_up_voltage - dwVolt);
 	}
 #endif
 	/* ------------------------------------------------------------------ */
 	g_btsctherm_TemperatureR = TRes;
 	pr_err("chargeic g_btsctherm_TemperatureR = %d\n", g_btsctherm_TemperatureR);
 	/* convert register to temperature */
 	CHARGEIC_TMP = mtkts_chargeic_thermistor_conver_temp(TRes);
 	return CHARGEIC_TMP;
 }
 static int get_hw_chargeic_temp(void)
 {
 	int val = 0;
 	int ret = 0, output;
 	ret = iio_read_channel_processed(spark_thermistor_ch2, &val);
 	if (ret < 0) {
 		mtkts_chargeic_printk("Busy/Timeout, IIO ch read failed %d\n", ret);
 		return ret;
 	}
 #ifdef APPLY_PRECISE_CHARGEIC_TEMP
 	ret = val * 100;
 #else
 	ret = val;
 #endif
 	/* ret = ret*1800/4096;//82's ADC power */
 	mtkts_chargeic_dprintk("APtery output mV = %d\n", ret);
 	output = mtk_ts_chargeic_volt_to_temp(ret);
 	mtkts_chargeic_dprintk("chargeic output temperature = %d\n", output);
 	return output;
 }
 static DEFINE_MUTEX(CHARGEIC_lock);
 /*int ts_AP_at_boot_time = 0;*/
 int mtkts_chargeic_get_hw_temp(void)
 {
 	int t_ret = 0;
 	int t_ret2 = 0;
 	mutex_lock(&CHARGEIC_lock);
 	t_ret = get_hw_chargeic_temp();
 #ifndef APPLY_PRECISE_CHARGEIC_TEMP
 	t_ret = t_ret * 1000;
 #endif
 	mutex_unlock(&CHARGEIC_lock);
 	if ((tsatm_thermal_get_catm_type() == 2) &&
 		(tsdctm_thermal_get_ttj_on() == 0))
 		t_ret2 = wakeup_ta_algo(TA_CATMPLUS_TTJ);
 	if (t_ret2 < 0)
 		pr_notice("[Thermal/TZ/CHARGEIC]wakeup_ta_algo out of memory\n");
 	if (t_ret > 40000)	/* abnormal high temp */
 		mtkts_chargeic_printk("T_AP=%d\n", t_ret);
 	mtkts_chargeic_dprintk("[%s] T_AP, %d\n", __func__, t_ret);
 	return t_ret;
 }
 static int mtkts_chargeic_get_temp(struct thermal_zone_device *thermal, int *t)
 {
 	*t = mtkts_chargeic_get_hw_temp();
 	if ((int)*t >= polling_trip_temp1)
 		thermal->polling_delay_jiffies = interval * 1000;
 	else if ((int)*t < polling_trip_temp2)
 		thermal->polling_delay_jiffies = interval * polling_factor2;
 	else
 		thermal->polling_delay_jiffies = interval * polling_factor1;
 	return 0;
 }
 /* bind callback functions to thermalzone */
 static struct thermal_zone_device_ops mtkts_CHARGEIC_dev_ops = {
 	.get_temp = mtkts_chargeic_get_temp,
 };
 static void mtkts_chargeic_unregister_thermal(void)
 {
 	mtkts_chargeic_dprintk("[%s]\n", __func__);
 	if (thz_dev) {
 		mtk_thermal_zone_device_unregister(thz_dev);
 		thz_dev = NULL;
 	}
 }
 static int mtkts_chargeic_register_thermal(void)
 {
 	mtkts_chargeic_dprintk("[%s]\n", __func__);
 	/* trips : trip 0~1 */
 	thz_dev = mtk_thermal_zone_device_register("wt_charger_therm", trips, num_trip, NULL,
 						&mtkts_CHARGEIC_dev_ops, 0, 0, 0,
 						interval * 1000);
 	return 0;
 }
 
 static int mtkts_chargeic_probe(struct platform_device *pdev)
 {
 	int err = 0;
 	int ret = 0;
 	mtkts_chargeic_dprintk("[%s]\n", __func__);
 	if (!pdev->dev.of_node) {
 		mtkts_chargeic_printk("[%s] Only DT based supported\n",
 			__func__);
 		return -ENODEV;
 }
 	spark_thermistor_ch2 = devm_kzalloc(&pdev->dev, sizeof(*spark_thermistor_ch2),
 		GFP_KERNEL);
 	if (!spark_thermistor_ch2)
 		return -ENOMEM;
 	spark_thermistor_ch2 = iio_channel_get(&pdev->dev, "thermistor-ch2");
 	ret = IS_ERR(spark_thermistor_ch2);
 	if (ret) {
 		mtkts_chargeic_printk("[%s] fail to get auxadc iio spark_thermistor_ch2: %d\n",
 			__func__, ret);
 		return ret;
 	}
 	/* setup default table */
 	mtkts_chargeic_prepare_table(g_RAP_ntc_table);
 	mtkts_chargeic_register_thermal();
 	return err;
 }
 #ifdef CONFIG_OF
 const struct of_device_id mt_thermistor_of_match5[2] = {
 	{.compatible = "mediatek,mtboard-thermistor5",},
 	{},
 };
 #endif
 #define THERMAL_THERMISTOR_NAME    "mtboard-thermistor5"
 static struct platform_driver mtk_thermal_chargeic_driver = {
 	.remove = NULL,
 	.shutdown = NULL,
 	.probe = mtkts_chargeic_probe,
 	.suspend = NULL,
 	.resume = NULL,
 	.driver = {
 		.name = THERMAL_THERMISTOR_NAME,
 #ifdef CONFIG_OF
 		.of_match_table = mt_thermistor_of_match5,
 #endif
 	},
 };
 int mtkts_chargeic_init(void)
 {
 	int err = 0;
 	mtkts_chargeic_dprintk("[%s]\n", __func__);
 	err = platform_driver_register(&mtk_thermal_chargeic_driver);
 	if (err) {
 		mtkts_chargeic_printk("thermal driver callback register failed.\n");
 		return err;
 	}
 	return 0;
 }
 void mtkts_chargeic_exit(void)
 {
 	mtkts_chargeic_dprintk("[%s]\n", __func__);
 	mtkts_chargeic_unregister_thermal();
 	/* mtkts_AP_unregister_cooler(); */
 }
 
