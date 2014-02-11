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

#include <common.h>
#include "am335x.h"

/* MUSB platform configuration */
struct musb_config musb_cfg = {
	(struct	musb_regs *)MENTOR_BASE,
	MUSB_TIMEOUT,
	0,
	0
};

/* MUSB module register overlay */
struct am335x_usb_regs *regs;

/*
 * Enable the USB phy
 */
static u8 phy_on(void)
{
	u32 usbphycfg;

	/* Inititlaize the DPLL PER */
	writel(DPLL_PER_CLKEN, AM335X_USB_PHYCLK);

	/* Inititalize USB PHY clock control register*/
	writel(USB_PHYCLK_CTRL, AM335X_USB_PHYCLK);

	/* Inititlaize USB interface clock control register */
	writel(USB_ICLK_CTRL, AM335X_USB_ICLK);

	/*
	 * Start the on-chip PHY and its PLL.
	 */
	usbphycfg = readl(AM335X_CONF0);

	usbphycfg &= ~(USBPHY_CM_PWRDN | USBPHY_OTG_PWRDN);
	usbphycfg |= (USBPHY_OTGVDET_EN | USBPHY_OTGSESSEND_EN);

	writel(usbphycfg, AM335X_CONF0);

	/* TODO: Wait until the USB phy is turned ON
	 * Currently set some delay
	 */
	udelay(5000);

	return 1;
}

/*
 * Disable the USB phy
 */
static void phy_off(void)
{
	u32 usbphycfg;

	/*
	 * Power down the on-chip PHY.
	 */
	usbphycfg = readl(AM335X_CONF0);

	usbphycfg |= USBPHY_CM_PWRDN
		| USBPHY_OTG_PWRDN;

	writel(usbphycfg, AM335X_CONF0);
}

/*
 * This function performs platform specific initialization for usb0.
 */
int musb_platform_init(void)
{
	if (!phy_on())	{
		printf("phy_on failed");
		return -1;
	}

	regs = (struct am335x_usb_regs *)MUSB_BASE;

	/* Enable the IRQ on USB0 wrapper register instead of MUSB register */
	writel(USB_IRQENABLE_SET, &regs->irq_enableset0);

	return 0;
}

/*
 * This function performs platform specific deinitialization for usb0.
 */
void musb_platform_deinit(void)
{
	/* Turn of the phy */
	phy_off();
}
