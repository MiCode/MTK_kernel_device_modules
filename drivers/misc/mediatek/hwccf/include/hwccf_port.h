// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _HWCCF_PORT_H
#define _HWCCF_PORT_H

#include <linux/types.h> /* Kernel only */
#include <linux/kernel.h> /* Kernel only */
#include <linux/time.h> /* Kernel only */
#include <linux/delay.h> /* Kernel only */
#include <linux/regmap.h> /* Kernel only */
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

//#define HWCCF_TEST_MODE
#define HWCCF_LOG_EN            1

/* should disable becasue outside loop with spin lock */
#define HWCCF_DBG_LOG_EN        0
#define HWCCF_PROFILE_EN        0
#define HWCCF_STEP_RECORD_EN    1

#define HWCCF_TAG           "[HWV] "
#define HWCCF_ERR_TAG       "[HWV_E] "
#define HWCCF_DBG_TAG       "[HWV_D] "

#define HWCCF_ERR(fmt, arg...)												\
	pr_err(HWCCF_ERR_TAG "<%s(), %d> " fmt, __func__, __LINE__, ##arg)

#define HWCCF_DBG_ISR(fmt, arg...)											\
	pr_notice(HWCCF_DBG_TAG "<%s(), %d> " fmt, __func__, __LINE__, ##arg)

#if HWCCF_LOG_EN
	#define HWCCF_LOG(fmt, arg...)      pr_notice(HWCCF_TAG fmt, ##arg) /* Kernel only */
	#define HWCCF_LOG_ISR(fmt, arg...)  pr_notice(HWCCF_TAG fmt, ##arg) /* Kernel only */
#else
	#define HWCCF_LOG(...)
	#define HWCCF_LOG_ISR(...)
#endif

#if HWCCF_DBG_LOG_EN
	#define HWCCF_DBG(fmt, arg...)											\
		pr_notice(HWCCF_DBG_TAG "<%s(), %d> " fmt, __func__, __LINE__, ##arg) /* Kernel only */
	#define HWCCF_WARN(fmt, arg...)										\
		pr_notice(HWCCF_TAG "<%s(), %d> " fmt, __func__, __LINE__, ##arg) /* warning log */
#else
	#define HWCCF_DBG(...)
	#define HWCCF_WARN(fmt, arg...)										\
		pr_notice(HWCCF_TAG "<%s(), %d> " fmt, __func__, __LINE__, ##arg) /* warning log */
#endif

/* Kernel only */
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	#define configASSERT(x)                                               \
			aee_kernel_warning_api(__FILE__, __LINE__,                    \
					DB_OPT_DEFAULT | DB_OPT_FTRACE, HWCCF_ERR_TAG,        \
					"HWCCF error occured\n")
#else
	#define configASSERT(x)              WARN_ON(!(x)) /* Kernel only */
#endif

#ifndef BIT
#define BIT(nr)     (1U << (nr))
#endif
#ifndef GENMASK
#define GENMASK(h, l) (((~0U >> (31 - (h))) >> (l)) << (l))
#endif
#ifndef IS_BIT_SET
#define IS_BIT_SET(value, bit) (((value) >> (bit)) & 1U)
#endif
#ifndef IS_MASK_SET
#define IS_MASK_SET(value, mask) (((value) & (mask)) == (mask))
#endif
#ifndef IS_MASK_CLR
#define IS_MASK_CLR(value, mask) (((value) & (mask)) == 0)
#endif

#define PLATFORM_Kernel  /* Kernel only */

#if defined(PLATFORM_LK2)
    typedef lk_time_t hwccf_time_t;
#elif defined(PLATFORM_Kernel)
    typedef ktime_t hwccf_time_t;
#else
    typedef uint64_t hwccf_time_t;
    #error "Unsupported platform"
#endif

/*profile ISR performance*/
#if HWCCF_PROFILE_EN
	#define HWCCF_PROFILE_DECLARE(name) \
		static hwccf_time_t name##_start_us; \
		static hwccf_time_t name##_end_us; \
		static hwccf_time_t name##_sum; \
		static hwccf_time_t name##_count

	#define HWCCF_PROFILE_START(name) \
		(name##_start_us = hwccf_get_time_us())

	#define HWCCF_PROFILE_END(name) do { \
		name##_end_us = hwccf_get_time_us(); \
		hwccf_time_t name##_duration = name##_end_us - name##_start_us; \
		if (name##_sum + name##_duration >= name##_sum) { \
			name##_sum += name##_duration; \
			name##_count++; \
		} else { \
			/* reset to prevent overflow */ \
			name##_sum = name##_duration; \
			name##_count = 1; \
		} \
	} while (0)

	#define HWCCF_PROFILE_RESET(name) do { \
		name##_start_us = 0; \
		name##_end_us = 0; \
		name##_sum = 0; \
		name##_count = 0; \
	} while (0)

	#define HWCCF_PROFILE_PRINT(name) \
		HWCCF_DBG_ISR(#name " - Time AVG/start/end = %llu/%llu/%llu us\n", \
			name##_count > 0 ? (name##_sum / name##_count) : 0, \
			name##_start_us, name##_end_us)

#else
	#define HWCCF_PROFILE_DECLARE(name)
	#define HWCCF_PROFILE_START(name)
	#define HWCCF_PROFILE_END(name)
	#define HWCCF_PROFILE_RESET(name)
	#define HWCCF_PROFILE_PRINT(name)
#endif

/*ex. HWCCF_STEPS_DECLARE(DEBUG_BIT, dummy_RG_addr, start_LSB = 0, 0xffffffff)*/

#if HWCCF_STEP_RECORD_EN
	#define HWCCF_STEPS_DECLARE(name, reg, reg_bit, reg_msk) \
		static uint32_t name##_step_addr = (reg); \
		static uint8_t  name##_step_bit  = (reg_bit); \
		static uint32_t name##_step_msk  = (reg_msk)

	#define HWCCF_STEPS_UPDATE(name, step) \
		hwccf_remap_write(name##_step_addr, hwccf_remap_read(name##_step_addr) \
			| BIT(step) << name##_step_bit)

	#define HWCCF_STEPS_CLEAR(name, step) \
		hwccf_remap_write(name##_step_addr, hwccf_remap_read(name##_step_addr) \
			& (~BIT(step) << name##_step_bit))

	#define HWCCF_STEPS_RESET(name) \
		hwccf_remap_write(name##_step_addr, hwccf_remap_read(name##_step_addr)\
			& ~(name##_step_msk << name##_step_bit))

	#define HWCCF_STEPS_PRINT(name) \
		HWCCF_DBG(#name " -STEPs= 0x%x\n", (hwccf_remap_read(name##_step_addr)\
			>> name##_step_bit) & name##_step_msk)
#else
	#define HWCCF_STEPS_DECLARE(name, reg, reg_bit, reg_msk)
	#define HWCCF_STEPS_UPDATE(name, step)
	#define HWCCF_STEPS_CLEAR(name, step)
	#define HWCCF_STEPS_RESET(name)
	#define HWCCF_STEPS_PRINT(name)
#endif

#define hwccf_remap_read(addr)          hwccf_read(hwccf_remap(addr))
#define hwccf_remap_write(addr, value)  hwccf_write(hwccf_remap(addr), value)

uint32_t hwccf_remap(uint32_t addr);
void hwccf_write(struct regmap *regmap, uint32_t addr, uint32_t value);
void hwccf_update_bit(struct regmap* regmap, uint32_t ofs, uint32_t mask, uint32_t value);
uint32_t hwccf_read(struct regmap *regmap, uint32_t addr);
hwccf_time_t hwccf_get_time_us(void);

#endif /* _HWCCF_PORT_H */
