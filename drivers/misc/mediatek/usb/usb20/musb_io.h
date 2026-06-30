/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2017 MediaTek Inc.
 */

#ifndef __MUSB_LINUX_PLATFORM_ARCH_H__
#define __MUSB_LINUX_PLATFORM_ARCH_H__

#include <linux/io.h>
#include <linux/spinlock.h>
#include <usb20.h>
#include <musb_debug.h>

extern bool mtk_usb_power;
#if IS_ENABLED(CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE)
extern void mt_usb_clock_prepare(void);
extern void mt_usb_clock_unprepare(void);
#endif
extern spinlock_t usb_io_lock;
extern spinlock_t usb_power_lock;

static inline u16 musb_readw(const void __iomem *addr, unsigned int offset)
{
	u16 rc = 0;
	unsigned long flags = 0, usb_power_flags = 0;

	spin_lock_irqsave(&usb_power_lock, usb_power_flags);
	if (likely(mtk_usb_power)) {
		rc = readw(addr + offset);
	} else {
		#if IS_ENABLED(CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE)
		mt_usb_clock_prepare();
		#endif
		spin_lock_irqsave(&usb_io_lock, flags);
		if (usb_enable_clock(true) < 0) {
			WARN_ON_ONCE(1);
			spin_unlock_irqrestore(&usb_io_lock, flags);
			spin_unlock_irqrestore(&usb_power_lock, usb_power_flags);
			DBG_LIMIT(5, "[MUSB]: clk enable fail, reject access register\n");
			return 0;
		}
		DBG(1, "[MUSB]:access %s function when usb clock is off 0x%X\n",
			__func__, offset);
		rc = readw(addr + offset);
		usb_enable_clock(false);
		spin_unlock_irqrestore(&usb_io_lock, flags);
		#if IS_ENABLED(CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE)
		mt_usb_clock_unprepare();
		#endif
	}
	spin_unlock_irqrestore(&usb_power_lock, usb_power_flags);
	return rc;
}

static inline u32
	musb_readl(const void __iomem *addr, unsigned int offset)
{
	u32 rc = 0;
	unsigned long flags = 0, usb_power_flags = 0;

	spin_lock_irqsave(&usb_power_lock, usb_power_flags);
	if (likely(mtk_usb_power)) {
		rc = readl(addr + offset);
	} else {
		#if IS_ENABLED(CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE)
		mt_usb_clock_prepare();
		#endif
		spin_lock_irqsave(&usb_io_lock, flags);
		if (usb_enable_clock(true) < 0) {
			WARN_ON_ONCE(1);
			spin_unlock_irqrestore(&usb_io_lock, flags);
			spin_unlock_irqrestore(&usb_power_lock, usb_power_flags);
			DBG_LIMIT(5, "[MUSB]: clk enable fail, reject access register\n");
			return 0;
		}
		DBG(1, "[MUSB]:access %s function when usb clock is off 0x%X\n",
			__func__, offset);
		rc = readl(addr + offset);
		usb_enable_clock(false);
		spin_unlock_irqrestore(&usb_io_lock, flags);
		#if IS_ENABLED(CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE)
		mt_usb_clock_unprepare();
		#endif
	}
	spin_unlock_irqrestore(&usb_power_lock, usb_power_flags);
	return rc;
}


static inline void
	musb_writew(void __iomem *addr, unsigned int offset, u16 data)
{
	unsigned long flags = 0, usb_power_flags = 0;

	spin_lock_irqsave(&usb_power_lock, usb_power_flags);
	if (likely(mtk_usb_power)) {
		writew(data, addr + offset);
	} else {
		#if IS_ENABLED(CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE)
		mt_usb_clock_prepare();
		#endif
		spin_lock_irqsave(&usb_io_lock, flags);
		if (usb_enable_clock(true) < 0) {
			WARN_ON_ONCE(1);
			spin_unlock_irqrestore(&usb_io_lock, flags);
			spin_unlock_irqrestore(&usb_power_lock, usb_power_flags);
			DBG_LIMIT(5, "[MUSB]: clk enable fail, reject access register\n");
			return;
		}
		DBG(1, "[MUSB]:access %s function when usb clock is off 0x%X\n",
			__func__, offset);
		writew(data, addr + offset);
		usb_enable_clock(false);
		spin_unlock_irqrestore(&usb_io_lock, flags);
		#if IS_ENABLED(CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE)
		mt_usb_clock_unprepare();
		#endif
	}
	spin_unlock_irqrestore(&usb_power_lock, usb_power_flags);
}

static inline void
	musb_writel(void __iomem *addr, unsigned int offset, u32 data)
{
	unsigned long flags = 0, usb_power_flags = 0;

	spin_lock_irqsave(&usb_power_lock, usb_power_flags);
	if (likely(mtk_usb_power)) {
		writel(data, addr + offset);
	} else {
		#if IS_ENABLED(CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE)
		mt_usb_clock_prepare();
		#endif
		spin_lock_irqsave(&usb_io_lock, flags);
		if (usb_enable_clock(true) < 0) {
			WARN_ON_ONCE(1);
			spin_unlock_irqrestore(&usb_io_lock, flags);
			spin_unlock_irqrestore(&usb_power_lock, usb_power_flags);
			DBG_LIMIT(5, "[MUSB]: clk enable fail, reject access register\n");
			return;
		}
		DBG(1, "[MUSB]:access %s function when usb clock is off 0x%X\n",
			__func__, offset);
		writel(data, addr + offset);
		usb_enable_clock(false);
		spin_unlock_irqrestore(&usb_io_lock, flags);
		#if IS_ENABLED(CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE)
		mt_usb_clock_unprepare();
		#endif
	}
	spin_unlock_irqrestore(&usb_power_lock, usb_power_flags);
}

static inline u8 musb_readb(const void __iomem *addr, unsigned int offset)
{
	u8 rc = 0;
	unsigned long flags = 0, usb_power_flags = 0;

	spin_lock_irqsave(&usb_power_lock, usb_power_flags);
	if (likely(mtk_usb_power)) {
		rc = readb(addr + offset);
	} else {
		#if IS_ENABLED(CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE)
		mt_usb_clock_prepare();
		#endif
		spin_lock_irqsave(&usb_io_lock, flags);
		if (usb_enable_clock(true) < 0) {
			WARN_ON_ONCE(1);
			spin_unlock_irqrestore(&usb_io_lock, flags);
			spin_unlock_irqrestore(&usb_power_lock, usb_power_flags);
			DBG_LIMIT(5, "[MUSB]: clk enable fail, reject access register\n");
			return 0;
		}
		DBG(1, "[MUSB]:access %s function when usb clock is off 0x%X\n",
			__func__, offset);
		rc = readb(addr + offset);
		usb_enable_clock(false);
		spin_unlock_irqrestore(&usb_io_lock, flags);
		#if IS_ENABLED(CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE)
		mt_usb_clock_unprepare();
		#endif
	}
	spin_unlock_irqrestore(&usb_power_lock, usb_power_flags);
	return rc;
}

static inline void musb_writeb
	(void __iomem *addr, unsigned int offset, u8 data)
{
	unsigned long flags = 0, usb_power_flags = 0;

	spin_lock_irqsave(&usb_power_lock, usb_power_flags);
	if (likely(mtk_usb_power)) {
		writeb(data, addr + offset);
	} else {
		#if IS_ENABLED(CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE)
		mt_usb_clock_prepare();
		#endif
		spin_lock_irqsave(&usb_io_lock, flags);
		if (usb_enable_clock(true) < 0) {
			WARN_ON_ONCE(1);
			spin_unlock_irqrestore(&usb_io_lock, flags);
			spin_unlock_irqrestore(&usb_power_lock, usb_power_flags);
			DBG_LIMIT(5, "[MUSB]: clk enable fail, reject access register\n");
			return;
		}
		DBG(1, "[MUSB]:access %s function when usb clock is off 0x%X\n",
			__func__, offset);
		writeb(data, addr + offset);
		usb_enable_clock(false);
		spin_unlock_irqrestore(&usb_io_lock, flags);
		#if IS_ENABLED(CONFIG_MTK_MUSB_PORT0_LOWPOWER_MODE)
		mt_usb_clock_unprepare();
		#endif
	}
	spin_unlock_irqrestore(&usb_power_lock, usb_power_flags);
}

#endif
