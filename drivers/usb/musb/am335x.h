/*
 * TI's AM335x platform specific USB wrapper functions.
 *
 * Copyright (c) 2009 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * Author: Prashantha Krishna x0156546@ti.com
 */

#ifndef __AM335X_USB_H__
#define __AM335X_USB_H__

#include "musb_core.h"

/* Base address of USB subsystem */
#define USBSS_BASE 0x47400000

/* Base address of musb wrapper */
#define MUSB_BASE 0x47401000

/* Base address of musb core */
#define MENTOR_BASE (MUSB_BASE+0x400)

/* Base address of system control register used to program phy */
#define AM335X_CONF0	0x44E10620

/* Base address of USB interface clock control register */
#define AM335X_USB_ICLK    0x44E0001C

/* Base address of USB PHY clock control register */
#define AM335X_USB_PHYCLK  0x44E0047C

/*
 * AM335x platform USB wrapper register overlay. Note: Only the required
 * registers are included in this structure. It can be expanded as required.
 */
struct am335x_usb_regs {
	u32	revision;
	u32	reserved[0x4];
	u32	control;
	u32	status;
	u32	reserved0[0x1];
	u32	irq_mergestatus;
	u32	irq_eoi;
	u32	irq_statusraw0;
	u32	irq_statusraw1;
	u32	irq_status0;
	u32	irq_status1;
	u32	irq_enableset0;
	u32	irq_enableset1;
	u32	irq_enableclr0;
	u32	irq_enableclr1;
	u32	reserved1[0x5];
	u32	tx_mode;
	u32	rx_mode;
	u32	reserved2[0x2];
	u32	rndis_epnsize[0xF];
	u32	reserved3[0x5];
	u32	auto_req;
	u32	srp_fixtime;
	u32	teardown;
	u32	reserved4[0x1];
	u32	threshold_xdmaidle;
	u32	phy_utmi;
	u32	mgc_utmiloopback;
	u32	mode;
	u32	reserved5[0xC5];
	u32	core_fifo1[0x1A];
	u32	core_hwvers;
	u32	core_fifo2[0x4B];
};


/* USBSS EOI bits */
#define USBSS_EOI               0

/* DPLL PER clock enable */
#define DPLL_PER_CLKEN          (0x100)

/* Control register bits */
#define USB_SOFT_RESET_MASK     1

/* Timeout for MUSB module */
#define MUSB_TIMEOUT 0x3FFFFFF

/* AM335X USB interface and PHY clock control bits */
#define USB_ICLK_CTRL           (0x2)
#define USB_PHYCLK_CTRL         (0x300)

/* AM335X USB IRQ set bit */
#define USB_IRQENABLE_SET       (0xFFFE0000)

/* AM335X PHY controls register bits */
#define USBPHY_CM_PWRDN         (1 << 0)
#define USBPHY_OTG_PWRDN        (1 << 1)
#define USBPHY_CHGDET_DIS       (1 << 2)
#define USBPHY_CHGDET_RSTRT     (1 << 3)
#define USBPHY_SRCONDM          (1 << 4)
#define USBPHY_SINKONDP         (1 << 5)
#define USBPHY_CHGISINK_EN      (1 << 6)
#define USBPHY_CHGVSRC_EN       (1 << 7)
#define USBPHY_DMPULLUP         (1 << 8)
#define USBPHY_DPPULLUP         (1 << 9)
#define USBPHY_CDET_EXTCTL      (1 << 10)
#define USBPHY_GPIO_MODE        (1 << 12)
#define USBPHY_DPGPIO_PD        (1 << 17)
#define USBPHY_DMGPIO_PD        (1 << 18)
#define USBPHY_OTGVDET_EN       (1 << 19)
#define USBPHY_OTGSESSEND_EN    (1 << 20)
#define USBPHY_DATA_POLARITY    (1 << 23)

#endif	/* __AM335X_USB_H__ */
