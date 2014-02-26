/*
 * evm.c
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <common.h>
#include <asm/cache.h>
#include <asm/omap_common.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/cpu.h>
#include <asm/arch/ddr_defs.h>
#include <asm/arch/hardware.h>
#include <asm/arch/mmc_host_def.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/mem.h>
#include <asm/arch/nand.h>
#include <asm/arch/clock.h>
#include <asm/arch/gpio.h>
#include <linux/mtd/nand.h>
#include <nand.h>
#include <net.h>
#include <miiphy.h>
#include <netdev.h>
#include <spi_flash.h>
#include "common_def.h"
#include "pmic.h"
#include "tps65217.h"
#include <i2c.h>
#include <serial.h>
#include <asm/errno.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/musb.h>
#include <asm/omap_musb.h>
#include <fastboot.h>
#include <environment.h>

DECLARE_GLOBAL_DATA_PTR;

/* UART Defines */
#define UART_SYSCFG_OFFSET	(0x54)
#define UART_SYSSTS_OFFSET	(0x58)

#define UART_RESET		(0x1 << 1)
#define UART_CLK_RUNNING_MASK	0x1
#define UART_SMART_IDLE_EN	(0x1 << 0x3)

/* Timer Defines */
#define TSICR_REG		0x54
#define TIOCP_CFG_REG		0x10
#define TCLR_REG		0x38

/* DDR defines */
#define MDDR_SEL_DDR2		0xefffffff		/* IOs set for DDR2-STL Mode */
#define CKE_NORMAL_OP		0x00000001		/* Normal Op:CKE controlled by EMIF */
#define GATELVL_INIT_MODE_SEL	0x1	/* Selects a starting ratio value based
					on DATA0/1_REG_PHY_GATELVL_INIT_RATIO_0
					value programmed by the user */
#define WRLVL_INIT_MODE_SEL	0x1	/* Selects a starting ratio value based
					on DATA0/1_REG_PHY_WRLVL_INIT_RATIO_0
					value programmed by the user */

/* CPLD registers */
#define CFG_REG			0x10

/*
 * I2C Address of various board
 */
#define I2C_BASE_BOARD_ADDR	0x50
#define I2C_DAUGHTER_BOARD_ADDR 0x51
#define I2C_LCD_BOARD_ADDR	0x52

#define I2C_CPLD_ADDR		0x35

/* RGMII mode define */
#define RGMII_MODE_ENABLE	0xA
#define RMII_MODE_ENABLE	0x5
#define MII_MODE_ENABLE		0x0

/* TLK110 PHY registers */
#define TLK110_COARSEGAIN_REG	0x00A3
#define TLK110_LPFHPF_REG	0x00AC
#define TLK110_SPAREANALOG_REG	0x00B9
#define TLK110_VRCR_REG		0x00D0
#define TLK110_SETFFE_REG	(unsigned char)0x0107
#define TLK110_FTSP_REG		(unsigned char)0x0154
#define TLK110_ALFATPIDL_REG	0x002A
#define TLK110_PSCOEF21_REG	0x0096
#define TLK110_PSCOEF3_REG	0x0097
#define TLK110_ALFAFACTOR1_REG	0x002C
#define TLK110_ALFAFACTOR2_REG	0x0023
#define TLK110_CFGPS_REG	0x0095
#define TLK110_FTSPTXGAIN_REG	(unsigned char)0x0150
#define TLK110_SWSCR3_REG	0x000B
#define TLK110_SCFALLBACK_REG	0x0040
#define TLK110_PHYRCR_REG	0x001F

/* TLK110 register writes values */
#define TLK110_COARSEGAIN_VAL	0x0000
#define TLK110_LPFHPF_VAL	0x8000
#define TLK110_SPAREANALOG_VAL	0x0000
#define TLK110_VRCR_VAL		0x0008
#define TLK110_SETFFE_VAL	0x0605
#define TLK110_FTSP_VAL		0x0255
#define TLK110_ALFATPIDL_VAL	0x7998
#define TLK110_PSCOEF21_VAL	0x3A20
#define TLK110_PSCOEF3_VAL	0x003F
#define TLK110_ALFAFACTOR1_VAL	0xFF80
#define TLK110_ALFAFACTOR2_VAL	0x021C
#define TLK110_CFGPS_VAL	0x0000
#define TLK110_FTSPTXGAIN_VAL	0x6A88
#define TLK110_SWSCR3_VAL	0x0000
#define TLK110_SCFALLBACK_VAL	0xC11D
#define TLK110_PHYRCR_VAL	0x4000
#define TLK110_PHYIDR1		0x2000
#define TLK110_PHYIDR2		0xA201

#define NO_OF_MAC_ADDR          3
#define ETH_ALEN		6

#define RTC_KICK0_REG		0x6c
#define RTC_KICK1_REG		0x70
#define RTC_OSC_REG		0x54

struct am335x_baseboard_id {
	unsigned int  magic;
	char name[8];
	char version[4];
	char serial[12];
	char config[32];
	char mac_addr[NO_OF_MAC_ADDR][ETH_ALEN];
};

#ifdef CONFIG_CMD_FASTBOOT
#ifdef FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING

#define MAX_PTN                 5

/* Initialize the name of fastboot flash name mappings */
fastboot_ptentry ptn[MAX_PTN] = {
	{
		.name   = "spl",
		.start  = 0x0000000,
		.length = 0x0020000, /* 128 K */
		/* Written into the first 4 0x20000 blocks
		   Use HW ECC */
		.flags  = FASTBOOT_PTENTRY_FLAGS_WRITE_I |
			  FASTBOOT_PTENTRY_FLAGS_WRITE_HW_BCH8_ECC |
			  FASTBOOT_PTENTRY_FLAGS_REPEAT_4,
	},
	{
		.name   = "uboot",
		.start  = 0x0080000,
		.length = 0x01E0000, /* 1.875 M */
		/* Skip bad blocks on write
		   Use HW ECC */
		.flags  = FASTBOOT_PTENTRY_FLAGS_WRITE_I |
			  FASTBOOT_PTENTRY_FLAGS_WRITE_HW_BCH8_ECC,
	},
	{
		.name   = "environment",
		.start  = MNAND_ENV_OFFSET,  /* set in config file */
		.length = 0x0020000,
		.flags  = FASTBOOT_PTENTRY_FLAGS_WRITE_ENV |
			  FASTBOOT_PTENTRY_FLAGS_WRITE_HW_ECC,
	},
	{
		.name   = "kernel",
		.start  = 0x0280000,
		.length = 0x0500000, /* 5 M */
		.flags  = FASTBOOT_PTENTRY_FLAGS_WRITE_I |
			  FASTBOOT_PTENTRY_FLAGS_WRITE_HW_BCH8_ECC,
	},
	{
		.name   = "filesystem",
		.start  = 0x0780000,
		.length = 0xF880000, /* 248.5 M */
		.flags  = FASTBOOT_PTENTRY_FLAGS_WRITE_I |
			  FASTBOOT_PTENTRY_FLAGS_WRITE_HW_BCH8_ECC,
	},
};
#endif /* FASTBOOT_PORT_OMAPZOOM_NAND_FLASHING */
#endif /* CONFIG_FASTBOOT */

static struct am335x_baseboard_id __attribute__((section (".data"))) header;
extern void cpsw_eth_set_mac_addr(const u_int8_t *addr);
static
unsigned char __attribute__((section (".data"))) daughter_board_connected = 1;
static volatile int __attribute__((section (".data"))) board_id = SK_BOARD;

/* GPIO base address table */
static const struct gpio_bank gpio_bank_am335x[4] = {
	{ (void *)AM335X_GPIO0_BASE, METHOD_GPIO_24XX },
	{ (void *)AM335X_GPIO1_BASE, METHOD_GPIO_24XX },
	{ (void *)AM335X_GPIO2_BASE, METHOD_GPIO_24XX },
	{ (void *)AM335X_GPIO3_BASE, METHOD_GPIO_24XX },
};
const struct gpio_bank *const omap_gpio_bank = gpio_bank_am335x;

int boot_mmc = 0;
/*
 * dram_init:
 * At this point we have initialized the i2c bus and can read the
 * EEPROM which will tell us what board and revision we are on.
 */
int dram_init(void)
{
	gd->ram_size = get_ram_size(
			(void *)CONFIG_SYS_SDRAM_BASE,
			CONFIG_MAX_RAM_BANK_SIZE);

	return 0;
}

void dram_init_banksize (void)
{
	/* Fill up board info */
	gd->bd->bi_dram[0].start = PHYS_DRAM_1;
	gd->bd->bi_dram[0].size = PHYS_DRAM_1_SIZE;
}



#ifdef CONFIG_SPL_BUILD
static void Data_Macro_Config_ddr2(int dataMacroNum)
{
        u32 BaseAddrOffset = 0x00;;

        if (dataMacroNum == 1)
                BaseAddrOffset = 0xA4;

        writel(((DDR2_RD_DQS<<30)|(DDR2_RD_DQS<<20)
                        |(DDR2_RD_DQS<<10)|(DDR2_RD_DQS<<0)),
                        (DATA0_RD_DQS_SLAVE_RATIO_0 + BaseAddrOffset));
        writel(DDR2_RD_DQS>>2, (DATA0_RD_DQS_SLAVE_RATIO_1 + BaseAddrOffset));
        writel(((DDR2_WR_DQS<<30)|(DDR2_WR_DQS<<20)
                        |(DDR2_WR_DQS<<10)|(DDR2_WR_DQS<<0)),
                        (DATA0_WR_DQS_SLAVE_RATIO_0 + BaseAddrOffset));
        writel(DDR2_WR_DQS>>2, (DATA0_WR_DQS_SLAVE_RATIO_1 + BaseAddrOffset));
        writel(((DDR2_PHY_WRLVL<<30)|(DDR2_PHY_WRLVL<<20)
                        |(DDR2_PHY_WRLVL<<10)|(DDR2_PHY_WRLVL<<0)),
                        (DATA0_WRLVL_INIT_RATIO_0 + BaseAddrOffset));
        writel(DDR2_PHY_WRLVL>>2, (DATA0_WRLVL_INIT_RATIO_1 + BaseAddrOffset));
        writel(((DDR2_PHY_GATELVL<<30)|(DDR2_PHY_GATELVL<<20)
                        |(DDR2_PHY_GATELVL<<10)|(DDR2_PHY_GATELVL<<0)),
                        (DATA0_GATELVL_INIT_RATIO_0 + BaseAddrOffset));
        writel(DDR2_PHY_GATELVL>>2,
                        (DATA0_GATELVL_INIT_RATIO_1 + BaseAddrOffset));
        writel(((DDR2_PHY_FIFO_WE<<30)|(DDR2_PHY_FIFO_WE<<20)
                        |(DDR2_PHY_FIFO_WE<<10)|(DDR2_PHY_FIFO_WE<<0)),
                        (DATA0_FIFO_WE_SLAVE_RATIO_0 + BaseAddrOffset));
        writel(DDR2_PHY_FIFO_WE>>2,
                        (DATA0_FIFO_WE_SLAVE_RATIO_1 + BaseAddrOffset));
        writel(((DDR2_PHY_WR_DATA<<30)|(DDR2_PHY_WR_DATA<<20)
                        |(DDR2_PHY_WR_DATA<<10)|(DDR2_PHY_WR_DATA<<0)),
                        (DATA0_WR_DATA_SLAVE_RATIO_0 + BaseAddrOffset));
        writel(DDR2_PHY_WR_DATA>>2,
                        (DATA0_WR_DATA_SLAVE_RATIO_1 + BaseAddrOffset));
        writel(DDR2_PHY_DLL_LOCK_DIFF,
                        (DATA0_DLL_LOCK_DIFF_0 + BaseAddrOffset));
}

static void config_vtp(void)
{
        writel(readl(VTP0_CTRL_REG) | VTP_CTRL_ENABLE, VTP0_CTRL_REG);
        writel(readl(VTP0_CTRL_REG) & (~VTP_CTRL_START_EN), VTP0_CTRL_REG);
        writel(readl(VTP0_CTRL_REG) | VTP_CTRL_START_EN, VTP0_CTRL_REG);

        /* Poll for READY */
        while ((readl(VTP0_CTRL_REG) & VTP_CTRL_READY) != VTP_CTRL_READY);
}

static void phy_config_cmd(void)
{
	writel(DDR3_RATIO, CMD0_CTRL_SLAVE_RATIO_0);
	writel(DDR3_INVERT_CLKOUT, CMD0_INVERT_CLKOUT_0);
	writel(DDR3_RATIO, CMD1_CTRL_SLAVE_RATIO_0);
	writel(DDR3_INVERT_CLKOUT, CMD1_INVERT_CLKOUT_0);
	writel(DDR3_RATIO, CMD2_CTRL_SLAVE_RATIO_0);
	writel(DDR3_INVERT_CLKOUT, CMD2_INVERT_CLKOUT_0);
}

static void phy_config_cmd_bbb(void)
{
	writel(MT41K256M16HA125E_RATIO, CMD0_CTRL_SLAVE_RATIO_0);
	writel(MT41K256M16HA125E_INVERT_CLKOUT, CMD0_INVERT_CLKOUT_0);
	writel(MT41K256M16HA125E_RATIO, CMD1_CTRL_SLAVE_RATIO_0);
	writel(MT41K256M16HA125E_INVERT_CLKOUT, CMD1_INVERT_CLKOUT_0);
	writel(MT41K256M16HA125E_RATIO, CMD2_CTRL_SLAVE_RATIO_0);
	writel(MT41K256M16HA125E_INVERT_CLKOUT, CMD2_INVERT_CLKOUT_0);
}

static void phy_config_data(void)
{
	writel(DDR3_RD_DQS, DATA0_RD_DQS_SLAVE_RATIO_0);
	writel(DDR3_WR_DQS, DATA0_WR_DQS_SLAVE_RATIO_0);
	writel(DDR3_PHY_FIFO_WE, DATA0_FIFO_WE_SLAVE_RATIO_0);
	writel(DDR3_PHY_WR_DATA, DATA0_WR_DATA_SLAVE_RATIO_0);

	writel(DDR3_RD_DQS, DATA1_RD_DQS_SLAVE_RATIO_0);
	writel(DDR3_WR_DQS, DATA1_WR_DQS_SLAVE_RATIO_0);
	writel(DDR3_PHY_FIFO_WE, DATA1_FIFO_WE_SLAVE_RATIO_0);
	writel(DDR3_PHY_WR_DATA, DATA1_WR_DATA_SLAVE_RATIO_0);
}

static void phy_config_data_bbb(void)
{
	writel(MT41K256M16HA125E_RD_DQS, DATA0_RD_DQS_SLAVE_RATIO_0);
	writel(MT41K256M16HA125E_WR_DQS, DATA0_WR_DQS_SLAVE_RATIO_0);
	writel(MT41K256M16HA125E_PHY_FIFO_WE, DATA0_FIFO_WE_SLAVE_RATIO_0);
	writel(MT41K256M16HA125E_PHY_WR_DATA, DATA0_WR_DATA_SLAVE_RATIO_0);

	writel(MT41K256M16HA125E_RD_DQS, DATA1_RD_DQS_SLAVE_RATIO_0);
	writel(MT41K256M16HA125E_WR_DQS, DATA1_WR_DQS_SLAVE_RATIO_0);
	writel(MT41K256M16HA125E_PHY_FIFO_WE, DATA1_FIFO_WE_SLAVE_RATIO_0);
	writel(MT41K256M16HA125E_PHY_WR_DATA, DATA1_WR_DATA_SLAVE_RATIO_0);
}

static void config_emif_ddr3(void)
{
	/*Program EMIF0 CFG Registers*/
	writel(DDR3_EMIF_READ_LATENCY, EMIF4_0_DDR_PHY_CTRL_1);
	writel(DDR3_EMIF_READ_LATENCY, EMIF4_0_DDR_PHY_CTRL_1_SHADOW);
	writel(DDR3_EMIF_READ_LATENCY, EMIF4_0_DDR_PHY_CTRL_2);
	writel(DDR3_EMIF_TIM1, EMIF4_0_SDRAM_TIM_1);
	writel(DDR3_EMIF_TIM1, EMIF4_0_SDRAM_TIM_1_SHADOW);
	writel(DDR3_EMIF_TIM2, EMIF4_0_SDRAM_TIM_2);
	writel(DDR3_EMIF_TIM2, EMIF4_0_SDRAM_TIM_2_SHADOW);
	writel(DDR3_EMIF_TIM3, EMIF4_0_SDRAM_TIM_3);
	writel(DDR3_EMIF_TIM3, EMIF4_0_SDRAM_TIM_3_SHADOW);

	writel(DDR3_EMIF_SDREF, EMIF4_0_SDRAM_REF_CTRL);
	writel(DDR3_EMIF_SDREF, EMIF4_0_SDRAM_REF_CTRL_SHADOW);
	writel(DDR3_ZQ_CFG, EMIF0_0_ZQ_CONFIG);

	writel(DDR3_EMIF_SDCFG, EMIF4_0_SDRAM_CONFIG);

	/*
	 * Write contents of SDRAM_CONFIG to SECURE_EMIF_SDRAM_CONFIG.
	 * SDRAM_CONFIG will be reconfigured with this value during resume
	 */
	writel(DDR3_EMIF_SDCFG, SECURE_EMIF_SDRAM_CONFIG);
}

static void config_emif_ddr3_bbb(void)
{
	/*Program EMIF0 CFG Registers*/
	writel(MT41K256M16HA125E_EMIF_READ_LATENCY, EMIF4_0_DDR_PHY_CTRL_1);
	writel(MT41K256M16HA125E_EMIF_READ_LATENCY, EMIF4_0_DDR_PHY_CTRL_1_SHADOW);
	writel(MT41K256M16HA125E_EMIF_READ_LATENCY, EMIF4_0_DDR_PHY_CTRL_2);
	writel(MT41K256M16HA125E_EMIF_TIM1, EMIF4_0_SDRAM_TIM_1);
	writel(MT41K256M16HA125E_EMIF_TIM1, EMIF4_0_SDRAM_TIM_1_SHADOW);
	writel(MT41K256M16HA125E_EMIF_TIM2, EMIF4_0_SDRAM_TIM_2);
	writel(MT41K256M16HA125E_EMIF_TIM2, EMIF4_0_SDRAM_TIM_2_SHADOW);
	writel(MT41K256M16HA125E_EMIF_TIM3, EMIF4_0_SDRAM_TIM_3);
	writel(MT41K256M16HA125E_EMIF_TIM3, EMIF4_0_SDRAM_TIM_3_SHADOW);

	writel(MT41K256M16HA125E_EMIF_SDREF, EMIF4_0_SDRAM_REF_CTRL);
	writel(MT41K256M16HA125E_EMIF_SDREF, EMIF4_0_SDRAM_REF_CTRL_SHADOW);
	writel(MT41K256M16HA125E_ZQ_CFG, EMIF0_0_ZQ_CONFIG);

	writel(MT41K256M16HA125E_EMIF_SDCFG, EMIF4_0_SDRAM_CONFIG);

	/*
	 * Write contents of SDRAM_CONFIG to SECURE_EMIF_SDRAM_CONFIG.
	 * SDRAM_CONFIG will be reconfigured with this value during resume
	 */
	writel(MT41K256M16HA125E_EMIF_SDCFG, SECURE_EMIF_SDRAM_CONFIG);
}

static void config_am335x_ddr3(void)
{
	enable_ddr3_clocks();

	config_vtp();

	phy_config_cmd();
	phy_config_data();

	/* set IO control registers */
	writel(DDR3_IOCTRL_VALUE, DDR_CMD0_IOCTRL);
	writel(DDR3_IOCTRL_VALUE, DDR_CMD1_IOCTRL);
	writel(DDR3_IOCTRL_VALUE, DDR_CMD2_IOCTRL);
	writel(DDR3_IOCTRL_VALUE, DDR_DATA0_IOCTRL);
	writel(DDR3_IOCTRL_VALUE, DDR_DATA1_IOCTRL);

	/* IOs set for DDR3 */
	writel(readl(DDR_IO_CTRL) & MDDR_SEL_DDR2, DDR_IO_CTRL);
	/* CKE controlled by EMIF/DDR_PHY */
	writel(readl(DDR_CKE_CTRL) | CKE_NORMAL_OP, DDR_CKE_CTRL);

	config_emif_ddr3();
}

static void config_am335x_ddr3_bbb(void)
{
	enable_ddr3_clocks();

	config_vtp();

	phy_config_cmd_bbb();
	phy_config_data_bbb();

	/* set IO control registers */
	writel(MT41K256M16HA125E_IOCTRL_VALUE, DDR_CMD0_IOCTRL);
	writel(MT41K256M16HA125E_IOCTRL_VALUE, DDR_CMD1_IOCTRL);
	writel(MT41K256M16HA125E_IOCTRL_VALUE, DDR_CMD2_IOCTRL);
	writel(MT41K256M16HA125E_IOCTRL_VALUE, DDR_DATA0_IOCTRL);
	writel(MT41K256M16HA125E_IOCTRL_VALUE, DDR_DATA1_IOCTRL);

	/* IOs set for DDR3 */
	writel(readl(DDR_IO_CTRL) & MDDR_SEL_DDR2, DDR_IO_CTRL);
	/* CKE controlled by EMIF/DDR_PHY */
	writel(readl(DDR_CKE_CTRL) | CKE_NORMAL_OP, DDR_CKE_CTRL);

	config_emif_ddr3_bbb();
}

static void Cmd_Macro_Config_ddr2(void)
{
	writel(DDR2_RATIO, CMD0_CTRL_SLAVE_RATIO_0);
	writel(DDR2_CMD_FORCE, CMD0_CTRL_SLAVE_FORCE_0);
	writel(DDR2_CMD_DELAY, CMD0_CTRL_SLAVE_DELAY_0);
	writel(DDR2_DLL_LOCK_DIFF, CMD0_DLL_LOCK_DIFF_0);
	writel(DDR2_INVERT_CLKOUT, CMD0_INVERT_CLKOUT_0);

	writel(DDR2_RATIO, CMD1_CTRL_SLAVE_RATIO_0);
	writel(DDR2_CMD_FORCE, CMD1_CTRL_SLAVE_FORCE_0);
	writel(DDR2_CMD_DELAY, CMD1_CTRL_SLAVE_DELAY_0);
	writel(DDR2_DLL_LOCK_DIFF, CMD1_DLL_LOCK_DIFF_0);
	writel(DDR2_INVERT_CLKOUT, CMD1_INVERT_CLKOUT_0);

	writel(DDR2_RATIO, CMD2_CTRL_SLAVE_RATIO_0);
	writel(DDR2_CMD_FORCE, CMD2_CTRL_SLAVE_FORCE_0);
	writel(DDR2_CMD_DELAY, CMD2_CTRL_SLAVE_DELAY_0);
	writel(DDR2_DLL_LOCK_DIFF, CMD2_DLL_LOCK_DIFF_0);
	writel(DDR2_INVERT_CLKOUT, CMD2_INVERT_CLKOUT_0);
}

static void config_emif_ddr2(void)
{
	u32 i;

	/*Program EMIF0 CFG Registers*/
	writel(DDR2_EMIF_READ_LATENCY, EMIF4_0_DDR_PHY_CTRL_1);
	writel(DDR2_EMIF_READ_LATENCY, EMIF4_0_DDR_PHY_CTRL_1_SHADOW);
	writel(DDR2_EMIF_READ_LATENCY, EMIF4_0_DDR_PHY_CTRL_2);
	writel(DDR2_EMIF_TIM1, EMIF4_0_SDRAM_TIM_1);
	writel(DDR2_EMIF_TIM1, EMIF4_0_SDRAM_TIM_1_SHADOW);
	writel(DDR2_EMIF_TIM2, EMIF4_0_SDRAM_TIM_2);
	writel(DDR2_EMIF_TIM2, EMIF4_0_SDRAM_TIM_2_SHADOW);
	writel(DDR2_EMIF_TIM3, EMIF4_0_SDRAM_TIM_3);
	writel(DDR2_EMIF_TIM3, EMIF4_0_SDRAM_TIM_3_SHADOW);

	writel(DDR2_EMIF_SDCFG, EMIF4_0_SDRAM_CONFIG);
	writel(DDR2_EMIF_SDCFG, EMIF4_0_SDRAM_CONFIG2);

	/* writel(DDR2_EMIF_SDMGT, EMIF0_0_SDRAM_MGMT_CTRL);
	writel(DDR2_EMIF_SDMGT, EMIF0_0_SDRAM_MGMT_CTRL_SHD); */
	writel(DDR2_EMIF_SDREF1, EMIF4_0_SDRAM_REF_CTRL);
	writel(DDR2_EMIF_SDREF1, EMIF4_0_SDRAM_REF_CTRL_SHADOW);

	for (i = 0; i < 5000; i++) {

	}

	/* writel(DDR2_EMIF_SDMGT, EMIF0_0_SDRAM_MGMT_CTRL);
	writel(DDR2_EMIF_SDMGT, EMIF0_0_SDRAM_MGMT_CTRL_SHD); */
	writel(DDR2_EMIF_SDREF2, EMIF4_0_SDRAM_REF_CTRL);
	writel(DDR2_EMIF_SDREF2, EMIF4_0_SDRAM_REF_CTRL_SHADOW);

	writel(DDR2_EMIF_SDCFG, EMIF4_0_SDRAM_CONFIG);
	writel(DDR2_EMIF_SDCFG, EMIF4_0_SDRAM_CONFIG2);
}

/*  void DDR2_EMIF_Config(void); */
static void config_am335x_ddr2(void)
{
	int data_macro_0 = 0;
	int data_macro_1 = 1;

	enable_ddr2_clocks();

	config_vtp();

	Cmd_Macro_Config_ddr2();

	Data_Macro_Config_ddr2(data_macro_0);
	Data_Macro_Config_ddr2(data_macro_1);

	writel(DDR2_PHY_RANK0_DELAY, DATA0_RANK0_DELAYS_0);
	writel(DDR2_PHY_RANK0_DELAY, DATA1_RANK0_DELAYS_0);

	writel(DDR2_IOCTRL_VALUE, DDR_CMD0_IOCTRL);
	writel(DDR2_IOCTRL_VALUE, DDR_CMD1_IOCTRL);
	writel(DDR2_IOCTRL_VALUE, DDR_CMD2_IOCTRL);
	writel(DDR2_IOCTRL_VALUE, DDR_DATA0_IOCTRL);
	writel(DDR2_IOCTRL_VALUE, DDR_DATA1_IOCTRL);

	writel(readl(DDR_IO_CTRL) & MDDR_SEL_DDR2, DDR_IO_CTRL);
	writel(readl(DDR_CKE_CTRL) | CKE_NORMAL_OP, DDR_CKE_CTRL);

	config_emif_ddr2();
}

static void init_timer(void)
{
	/* Reset the Timer */
	writel(0x2, (DM_TIMER2_BASE + TSICR_REG));

	/* Wait until the reset is done */
	while (readl(DM_TIMER2_BASE + TIOCP_CFG_REG) & 1);

	/* Start the Timer */
	writel(0x1, (DM_TIMER2_BASE + TCLR_REG));
}

static void rtc32k_enable(void)
{
	/* Unlock the rtc's registers */
	writel(0x83e70b13, (AM335X_RTC_BASE + RTC_KICK0_REG));
	writel(0x95a4f1e0, (AM335X_RTC_BASE + RTC_KICK1_REG));

	/* Enable the RTC 32K OSC */
	writel(0x48, (AM335X_RTC_BASE + RTC_OSC_REG));
}
#endif

/*
void print_i2c_BBB_id_gm(struct am335x_baseboard_id *header)
{
	printf("magic number (0x%x) in EEPROM\n",
					header->magic);
	printf("board name (%s) in EEPROM\n",
					header->name);
	printf("board version (%s) in EEPROM\n",
					header->version);
	printf("board serial (%s) in EEPROM\n",
					header->serial);
	printf("board config (%s) in EEPROM\n",
					header->config);
	printf("board mac_addr0 (%s) in EEPROM\n",
					header->mac_addr[0]);					
	printf("board mac_addr1 (%s) in EEPROM\n",
					header->mac_addr[1]);		
	printf("board mac_addr2 (%s) in EEPROM\n",
					header->mac_addr[2]);		
															
	int i;
	char *char_p = (char *)header;
	for(i=0;i<sizeof(struct am335x_baseboard_id);i++,char_p++)
	{
		printf("%02x ",*char_p);
	}
	printf("\nboard info print over.  ok.gaoming\n");
}
*/
void fork_baseboard_info(struct am335x_baseboard_id *header)
{
/*	header->magic = 0xee3355aa;
	header->name = "A335BNLT";
	header->version = "0A5C";
	header->serial = "3513BBBK2712";
	header->config = '0xff';
*/
	header->magic = 0xee3355aa;
	strcpy(header->name,"A335BNLT");
	strcpy(header->version,"0A5C");
	strcpy(header->serial,"3513BBBK2712");
	header->config[0]=0xff;
}

/*
 * Read header information from EEPROM into global structure.
 */
int read_eeprom(void)
{
        /* Check if baseboard eeprom is available */
	if (i2c_probe(I2C_BASE_BOARD_ADDR)) {
		printf("Could not probe the EEPROM; something fundamentally "
			"wrong on the I2C bus.\n");
		return 1;
	}

	/* read the eeprom using i2c */
	if (i2c_read(I2C_BASE_BOARD_ADDR, 0, 2, (uchar *)&header,
							sizeof(header))) {
		printf("Could not read the EEPROM; something fundamentally"
			" wrong on the I2C bus.\n");
		return 1;
	}
//	print_i2c_BBB_id_gm(&header);
	fork_baseboard_info(&header);
	printf("\nreplay board info !\n");
//	print_i2c_BBB_id_gm(&header);

	if (header.magic != 0xEE3355AA) {
		/* read the eeprom using i2c again, but use only a 1 byte address */
		if (i2c_read(I2C_BASE_BOARD_ADDR, 0, 1, (uchar *)&header,
								sizeof(header))) {
			printf("Could not read the EEPROM; something fundamentally"
				" wrong on the I2C bus.\n");
			return 1;
		}

		if (header.magic != 0xEE3355AA) {
			printf("Incorrect magic number in EEPROM\n");
			return 1;
		}
	}
	return 0;
}

#if defined(CONFIG_SPL_BUILD) && defined(CONFIG_SPL_BOARD_INIT)

/**
 * tps65217_reg_read() - Generic function that can read a TPS65217 register
 * @src_reg:          Source register address
 * @src_val:          Address of destination variable
 */

unsigned char tps65217_reg_read(uchar src_reg, uchar *src_val)
{
	if (i2c_read(TPS65217_CHIP_PM, src_reg, 1, src_val, 1))
		return 1;
	return 0;
}

/**
 *  tps65217_reg_write() - Generic function that can write a TPS65217 PMIC
 *                         register or bit field regardless of protection
 *                         level.
 *
 *  @prot_level:        Register password protection.
 *                      use PROT_LEVEL_NONE, PROT_LEVEL_1, or PROT_LEVEL_2
 *  @dest_reg:          Register address to write.
 *  @dest_val:          Value to write.
 *  @mask:              Bit mask (8 bits) to be applied.  Function will only
 *                      change bits that are set in the bit mask.
 *
 *  @return:            0 for success, 1 for failure.
 */
int tps65217_reg_write(uchar prot_level, uchar dest_reg,
        uchar dest_val, uchar mask)
{
        uchar read_val;
        uchar xor_reg;

        /* if we are affecting only a bit field, read dest_reg and apply the mask */
        if (mask != MASK_ALL_BITS) {
                if (i2c_read(TPS65217_CHIP_PM, dest_reg, 1, &read_val, 1))
                        return 1;
                read_val &= (~mask);
                read_val |= (dest_val & mask);
                dest_val = read_val;
        }

        if (prot_level > 0) {
                xor_reg = dest_reg ^ PASSWORD_UNLOCK;
                if (i2c_write(TPS65217_CHIP_PM, PASSWORD, 1, &xor_reg, 1))
                        return 1;
        }

        if (i2c_write(TPS65217_CHIP_PM, dest_reg, 1, &dest_val, 1))
                return 1;

        if (prot_level == PROT_LEVEL_2) {
                if (i2c_write(TPS65217_CHIP_PM, PASSWORD, 1, &xor_reg, 1))
                        return 1;

                if (i2c_write(TPS65217_CHIP_PM, dest_reg, 1, &dest_val, 1))
                        return 1;
        }

        return 0;
}

/**
 *  tps65217_voltage_update() - Controls output voltage setting for the DCDC1,
 *       DCDC2, or DCDC3 control registers in the PMIC.
 *
 *  @dc_cntrl_reg:      DCDC Control Register address.
 *                      Must be DEFDCDC1, DEFDCDC2, or DEFDCDC3.
 *  @volt_sel:          Register value to set.  See PMIC TRM for value set.
 *
 *  @return:            0 for success, 1 for failure.
 */
int tps65217_voltage_update(unsigned char dc_cntrl_reg, unsigned char volt_sel)
{
        if ((dc_cntrl_reg != DEFDCDC1) && (dc_cntrl_reg != DEFDCDC2)
                && (dc_cntrl_reg != DEFDCDC3))
                return 1;

        /* set voltage level */
        if (tps65217_reg_write(PROT_LEVEL_2, dc_cntrl_reg, volt_sel, MASK_ALL_BITS))
                return 1;

        /* set GO bit to initiate voltage transition */
        if (tps65217_reg_write(PROT_LEVEL_2, DEFSLEW, DCDC_GO, DCDC_GO))
                return 1;

        return 0;
}

/*
 * voltage switching for MPU frequency switching.
 * @module = mpu - 0, core - 1
 * @vddx_op_vol_sel = vdd voltage to set
 */

#define MPU	0
#define CORE	1

int voltage_update(unsigned int module, unsigned char vddx_op_vol_sel)
{
	uchar buf[4];
	unsigned int reg_offset;

	if(module == MPU)
		reg_offset = PMIC_VDD1_OP_REG;
	else
		reg_offset = PMIC_VDD2_OP_REG;

	/* Select VDDx OP   */
	if (i2c_read(PMIC_CTRL_I2C_ADDR, reg_offset, 1, buf, 1))
		return 1;

	buf[0] &= ~PMIC_OP_REG_CMD_MASK;

	if (i2c_write(PMIC_CTRL_I2C_ADDR, reg_offset, 1, buf, 1))
		return 1;

	/* Configure VDDx OP  Voltage */
	if (i2c_read(PMIC_CTRL_I2C_ADDR, reg_offset, 1, buf, 1))
		return 1;

	buf[0] &= ~PMIC_OP_REG_SEL_MASK;
	buf[0] |= vddx_op_vol_sel;

	if (i2c_write(PMIC_CTRL_I2C_ADDR, reg_offset, 1, buf, 1))
		return 1;

	if (i2c_read(PMIC_CTRL_I2C_ADDR, reg_offset, 1, buf, 1))
		return 1;

	if ((buf[0] & PMIC_OP_REG_SEL_MASK ) != vddx_op_vol_sel)
		return 1;

	return 0;
}

void spl_board_init(void)
{
	uchar pmic_status_reg;

	/* init board_id, configure muxes */
	board_init();

	if (!strncmp("A335BONE", header.name, 8)) {
		/* BeagleBone PMIC Code */
		if (i2c_probe(TPS65217_CHIP_PM))
			return;

		if (tps65217_reg_read(STATUS, &pmic_status_reg))
			return;

		/* Increase USB current limit to 1300mA */
		if (tps65217_reg_write(PROT_LEVEL_NONE, POWER_PATH,
				       USB_INPUT_CUR_LIMIT_1300MA,
				       USB_INPUT_CUR_LIMIT_MASK))
			printf("tps65217_reg_write failure\n");

		/* Only perform PMIC configurations if board rev > A1 */
		if (!strncmp(header.version, "00A1", 4))
			return;

		/* Set DCDC2 (MPU) voltage to 1.275V */
		if (tps65217_voltage_update(DEFDCDC2,
					     DCDC_VOLT_SEL_1275MV)) {
			printf("tps65217_voltage_update failure\n");
			return;
		}

		/* Set LDO3, LDO4 output voltage to 3.3V */
		if (tps65217_reg_write(PROT_LEVEL_2, DEFLS1,
				       LDO_VOLTAGE_OUT_3_3, LDO_MASK))
			printf("tps65217_reg_write failure\n");

		if (tps65217_reg_write(PROT_LEVEL_2, DEFLS2,
				       LDO_VOLTAGE_OUT_3_3, LDO_MASK))
			printf("tps65217_reg_write failure\n");

		if (!(pmic_status_reg & PWR_SRC_AC_BITMASK)) {
			printf("No AC power, disabling frequency switch\n");
			return;
		}

		/* Set MPU Frequency to 720MHz */
		mpu_pll_config(MPUPLL_M_720);

	} else if (!strncmp("A335BNLT", header.name, 8)) {
		/* BeagleBone PMIC Code */
		if (i2c_probe(TPS65217_CHIP_PM))
			return;

		if (tps65217_reg_read(STATUS, &pmic_status_reg))
			return;

		/* Set DCDC2 (MPU) voltage to 1.325V */
		if (tps65217_voltage_update(DEFDCDC2,
					     DCDC_VOLT_SEL_1325MV)) {
			printf("tps65217_voltage_update failure\n");
			return;
		}

		/* Set MPU Frequency to 1GHz */
		//mpu_pll_config(MPUPLL_M_1000);
		mpu_pll_config(MPUPLL_M_720);

	} else {
		uchar buf[4];
		/*
		 * EVM PMIC code.  All boards currently want an MPU voltage
		 * of 1.2625V and CORE voltage of 1.1375V to operate at
		 * 720MHz.
		 */
		if (i2c_probe(PMIC_CTRL_I2C_ADDR))
			return;

		/* VDD1/2 voltage selection register access by control i/f */
		if (i2c_read(PMIC_CTRL_I2C_ADDR, PMIC_DEVCTRL_REG, 1, buf, 1))
			return;

		buf[0] |= PMIC_DEVCTRL_REG_SR_CTL_I2C_SEL_CTL_I2C;

		if (i2c_write(PMIC_CTRL_I2C_ADDR, PMIC_DEVCTRL_REG, 1, buf, 1))
			return;

		if (!voltage_update(MPU, PMIC_OP_REG_SEL_1_2_6) &&
				!voltage_update(CORE, PMIC_OP_REG_SEL_1_1_3))
			/* Frequency switching for OPP 120 */
	 		mpu_pll_config(MPUPLL_M_720);
	}
}
#endif

#define GPIO_DDR_VTT_EN		7
#define GPIO_BOOT_MMC		72
#define GPIO_LED0		19
#define GPIO_LED1		20
/*
 * early system init of muxing and clocks.
 */
void s_init(void)
{
	/* Can be removed as A8 comes up with L2 enabled */
	l2_cache_enable();

	/* WDT1 is already running when the bootloader gets control
	 * Disable it to avoid "random" resets
	 */
	writel(0xAAAA, WDT_WSPR);
	while(readl(WDT_WWPS) != 0x0);
	writel(0x5555, WDT_WSPR);
	while(readl(WDT_WWPS) != 0x0);

#ifdef CONFIG_SPL_BUILD
	/* Setup the PLLs and the clocks for the peripherals */
	pll_init();

	/* UART softreset */
	u32 regVal;
	u32 uart_base = DEFAULT_UART_BASE;

	enable_uart0_pin_mux();
	enable_uart1_pin_mux();
	/* IA Motor Control Board has default console on UART3*/
	/* XXX: This is before we've probed / set board_id */
	if (board_id == IA_BOARD) {
		uart_base = UART3_BASE;
	}

	regVal = readl(uart_base + UART_SYSCFG_OFFSET);
	regVal |= UART_RESET;
	writel(regVal, (uart_base + UART_SYSCFG_OFFSET) );
	while ((readl(uart_base + UART_SYSSTS_OFFSET) &
			UART_CLK_RUNNING_MASK) != UART_CLK_RUNNING_MASK);

	/* Disable smart idle */
	regVal = readl((uart_base + UART_SYSCFG_OFFSET));
	regVal |= UART_SMART_IDLE_EN;
	writel(regVal, (uart_base + UART_SYSCFG_OFFSET));

	/* Initialize the Timer */
	init_timer();

	preloader_console_init();

        enable_i2c0_pin_mux();

        i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);

        if (read_eeprom()) {
                printf("read_eeprom() failure. continuing with ddr3\n");
        }

        u32 is_ddr3 = 1;
        if (!strncmp("A335BNLT", header.name, 8)) {
                is_ddr3 = 1;

                enable_gpio0_7_pin_mux();
                gpio_request(GPIO_DDR_VTT_EN, "ddr_vtt_en");
                gpio_direction_output(GPIO_DDR_VTT_EN, 1);
        }

        if(is_ddr3 == 1){
		/* Set DDR3 clock to 400MHz for BBBlack */
                ddr_pll_config(400);
                config_am335x_ddr3_bbb();
        } else {
                ddr_pll_config(266);
                config_am335x_ddr2();
        }
#endif
}

static void detect_daughter_board(void)
{
	/* Check if daughter board is conneted */
	if (i2c_probe(I2C_DAUGHTER_BOARD_ADDR)) {
		printf("No daughter card present\n");
		return;
	} else {
		printf("Found a daughter card connected\n");
		daughter_board_connected = 1;
	}
}

static unsigned char profile = PROFILE_0;
static void detect_daughter_board_profile(void)
{
	unsigned short val;

	if (i2c_probe(I2C_CPLD_ADDR))
		return;

	if (i2c_read(I2C_CPLD_ADDR, CFG_REG, 1, (unsigned char *)(&val), 2))
		return;

	profile = 1 << (val & 0x7);
}

/*
 * Basic board specific setup
 */
#ifndef CONFIG_SPL_BUILD
int board_evm_init(void)
{
	/* mach type passed to kernel */
	if (board_id == IA_BOARD)
		gd->bd->bi_arch_number = MACH_TYPE_TIAM335IAEVM;
	else
		gd->bd->bi_arch_number = MACH_TYPE_TIAM335EVM;

	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_DRAM_1 + 0x100;

	return 0;
}
#endif

#if 0
struct serial_device *default_serial_console(void)
{

	if (board_id != IA_BOARD) {
		return &eserial1_device;	/* UART0 */
	} else {
		return &eserial4_device;	/* UART3 */
	}
}
#endif

int board_init(void)
{
        /* Configure the i2c0 pin mux */
        enable_i2c0_pin_mux();

        i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);
#ifndef CONFIG_SPL_BUILD
	/* Enable LCD backlight */
	enable_gpio1_18_pin_mux();
	gpio_request(50, "");
	gpio_direction_output(50, 1);
	/* Display logo */
//	Lcd_Init();
#endif
//	enable_eth_pin_mux();

	board_id = GP_BOARD;
	profile = 1;	/* profile 0 is internally considered as 1 */
	daughter_board_connected = 1;
	configure_evm_pin_mux(board_id, header.version, profile, daughter_board_connected);

#ifndef CONFIG_SPL_BUILD
	board_evm_init();
#endif
	gpmc_init();

	return 0;
}

int misc_init_r(void)
{
#ifdef DEBUG
	unsigned int cntr;
	unsigned char *valPtr;

	debug("EVM Configuration - ");
	debug("\tBoard id %x, profile %x, db %d\n", board_id, profile,
						daughter_board_connected);
	debug("Base Board EEPROM Data\n");
	valPtr = (unsigned char *)&header;
	for(cntr = 0; cntr < sizeof(header); cntr++) {
		if(cntr % 16 == 0)
			debug("\n0x%02x :", cntr);
		debug(" 0x%02x", (unsigned int)valPtr[cntr]);
	}
	debug("\n\n");

	debug("Board identification from EEPROM contents:\n");
	debug("\tBoard name   : %.8s\n", header.name);
	debug("\tBoard version: %.4s\n", header.version);
	debug("\tBoard serial : %.12s\n", header.serial);
	debug("\tBoard config : %.6s\n\n", header.config);
#endif
	return 0;
}

#ifdef BOARD_LATE_INIT
int board_late_init(void)
{
	if (board_id == IA_BOARD) {
		/*
		* SPI bus number is switched to in case Industrial Automation
		* motor control EVM.
		*/
		setenv("spi_bus_no", "1");
		/* Change console to tty03 for IA Motor Control EVM */
		setenv("console", "ttyO3,115200n8");
	}

	if(boot_mmc)
	{
		set_default_env("## Reseting to default environment\n");
	}

	return 0;
}
#endif

#ifdef CONFIG_DRIVER_TI_CPSW
/* TODO : Check for the board specific PHY */
static void evm_phy_init(char *name, int addr)
{
	unsigned short val;
	unsigned int cntr = 0;
	unsigned short phyid1, phyid2;
	int bone_pre_a3 = 0;

	if (board_id == BONE_BOARD && (!strncmp(header.version, "00A1", 4) ||
		    !strncmp(header.version, "00A2", 4)))
		bone_pre_a3 = 1;

	/*
	 * This is done as a workaround to support TLK110 rev1.0 PHYs.
	 * We can only perform these reads on these PHYs (currently
	 * only found on the IA EVM).
	 */
	if ((miiphy_read(name, addr, MII_PHYSID1, &phyid1) != 0) ||
			(miiphy_read(name, addr, MII_PHYSID2, &phyid2) != 0))
		return;

	if ((phyid1 == TLK110_PHYIDR1) && (phyid2 == TLK110_PHYIDR2)) {
		miiphy_read(name, addr, TLK110_COARSEGAIN_REG, &val);
		val |= TLK110_COARSEGAIN_VAL;
		miiphy_write(name, addr, TLK110_COARSEGAIN_REG, val);

		miiphy_read(name, addr, TLK110_LPFHPF_REG, &val);
		val |= TLK110_LPFHPF_VAL;
		miiphy_write(name, addr, TLK110_LPFHPF_REG, val);

		miiphy_read(name, addr, TLK110_SPAREANALOG_REG, &val);
		val |= TLK110_SPAREANALOG_VAL;
		miiphy_write(name, addr, TLK110_SPAREANALOG_REG, val);

		miiphy_read(name, addr, TLK110_VRCR_REG, &val);
		val |= TLK110_VRCR_VAL;
		miiphy_write(name, addr, TLK110_VRCR_REG, val);

		miiphy_read(name, addr, TLK110_SETFFE_REG, &val);
		val |= TLK110_SETFFE_VAL;
		miiphy_write(name, addr, TLK110_SETFFE_REG, val);

		miiphy_read(name, addr, TLK110_FTSP_REG, &val);
		val |= TLK110_FTSP_VAL;
		miiphy_write(name, addr, TLK110_FTSP_REG, val);

		miiphy_read(name, addr, TLK110_ALFATPIDL_REG, &val);
		val |= TLK110_ALFATPIDL_VAL;
		miiphy_write(name, addr, TLK110_ALFATPIDL_REG, val);

		miiphy_read(name, addr, TLK110_PSCOEF21_REG, &val);
		val |= TLK110_PSCOEF21_VAL;
		miiphy_write(name, addr, TLK110_PSCOEF21_REG, val);

		miiphy_read(name, addr, TLK110_PSCOEF3_REG, &val);
		val |= TLK110_PSCOEF3_VAL;
		miiphy_write(name, addr, TLK110_PSCOEF3_REG, val);

		miiphy_read(name, addr, TLK110_ALFAFACTOR1_REG, &val);
		val |= TLK110_ALFAFACTOR1_VAL;
		miiphy_write(name, addr, TLK110_ALFAFACTOR1_REG, val);

		miiphy_read(name, addr, TLK110_ALFAFACTOR2_REG, &val);
		val |= TLK110_ALFAFACTOR2_VAL;
		miiphy_write(name, addr, TLK110_ALFAFACTOR2_REG, val);

		miiphy_read(name, addr, TLK110_CFGPS_REG, &val);
		val |= TLK110_CFGPS_VAL;
		miiphy_write(name, addr, TLK110_CFGPS_REG, val);

		miiphy_read(name, addr, TLK110_FTSPTXGAIN_REG, &val);
		val |= TLK110_FTSPTXGAIN_VAL;
		miiphy_write(name, addr, TLK110_FTSPTXGAIN_REG, val);

		miiphy_read(name, addr, TLK110_SWSCR3_REG, &val);
		val |= TLK110_SWSCR3_VAL;
		miiphy_write(name, addr, TLK110_SWSCR3_REG, val);

		miiphy_read(name, addr, TLK110_SCFALLBACK_REG, &val);
		val |= TLK110_SCFALLBACK_VAL;
		miiphy_write(name, addr, TLK110_SCFALLBACK_REG, val);

		miiphy_read(name, addr, TLK110_PHYRCR_REG, &val);
		val |= TLK110_PHYRCR_VAL;
		miiphy_write(name, addr, TLK110_PHYRCR_REG, val);
	}

	/* Enable Autonegotiation */
	if (miiphy_read(name, addr, MII_BMCR, &val) != 0) {
		printf("failed to read bmcr\n");
		return;
	}

	if (bone_pre_a3) {
		val &= ~(BMCR_FULLDPLX | BMCR_ANENABLE | BMCR_SPEED100);
		val |= BMCR_FULLDPLX;
	} else
		val |= BMCR_FULLDPLX | BMCR_ANENABLE | BMCR_SPEED100;

	if (miiphy_write(name, addr, MII_BMCR, val) != 0) {
		printf("failed to write bmcr\n");
		return;
	}
	miiphy_read(name, addr, MII_BMCR, &val);

	/*
	 * The 1.0 revisions of the GP board don't have functional
	 * gigabit ethernet so we need to disable advertising.
	 */
	if (board_id == GP_BOARD && !strncmp(header.version, "1.0", 3)) {
		miiphy_read(name, addr, MII_CTRL1000, &val);
		val &= ~PHY_1000BTCR_1000FD;
		val &= ~PHY_1000BTCR_1000HD;
		miiphy_write(name, addr, MII_CTRL1000, val);
		miiphy_read(name, addr, MII_CTRL1000, &val);
	}

	/* Setup general advertisement */
	if (miiphy_read(name, addr, MII_ADVERTISE, &val) != 0) {
		printf("failed to read anar\n");
		return;
	}

	if (bone_pre_a3)
		val |= (LPA_10HALF | LPA_10FULL);
	else
		val |= (LPA_10HALF | LPA_10FULL | LPA_100HALF | LPA_100FULL);

	if (miiphy_write(name, addr, MII_ADVERTISE, val) != 0) {
		printf("failed to write anar\n");
		return;
	}
	miiphy_read(name, addr, MII_ADVERTISE, &val);

	/* Restart auto negotiation*/
	miiphy_read(name, addr, MII_BMCR, &val);
	val |= BMCR_ANRESTART;
	miiphy_write(name, addr, MII_BMCR, val);

	/*check AutoNegotiate complete - it can take upto 3 secs*/
	do {
		udelay(40000);
		cntr++;
		if (!miiphy_read(name, addr, MII_BMSR, &val)) {
			if (val & BMSR_ANEGCOMPLETE)
				break;
		}
	} while (cntr < 250);

	if (cntr >= 250)
		printf("Auto Negotitation failed for port %d\n", addr);

	return;
}

static void cpsw_control(int enabled)
{
	/* nothing for now */
	/* TODO : VTP was here before */
	return;
}

static struct cpsw_slave_data cpsw_slaves[] = {
	{
		.slave_reg_ofs	= 0x208,
		.sliver_reg_ofs	= 0xd80,
		.phy_id		= 0,
	},
	{
		.slave_reg_ofs	= 0x308,
		.sliver_reg_ofs	= 0xdc0,
		.phy_id		= 1,
	},
};

static struct cpsw_platform_data cpsw_data = {
	.mdio_base		= AM335X_CPSW_MDIO_BASE,
	.cpsw_base		= AM335X_CPSW_BASE,
	.mdio_div		= 0xff,
	.channels		= 8,
	.cpdma_reg_ofs		= 0x800,
	.slaves			= 2,
	.slave_data		= cpsw_slaves,
	.ale_reg_ofs		= 0xd00,
	.ale_entries		= 1024,
	.host_port_reg_ofs	= 0x108,
	.hw_stats_reg_ofs	= 0x900,
	.mac_control		= (1 << 5) /* MIIEN */,
	.control		= cpsw_control,
	.phy_init		= evm_phy_init,
	.gigabit_en		= 1,
	.host_port_num		= 0,
	.version		= CPSW_CTRL_VERSION_2,
};
#endif

#if defined(CONFIG_MUSB_GADGET) && \
	(!defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_MUSB_GADGET_SUPPORT))
#ifdef CONFIG_MUSB_GADGET_PORT0
#define USB_CTRL_REG	USB_CTRL0
#define OTG_REGS_BASE	((void *)AM335X_USB0_OTG_BASE)
#elif defined(CONFIG_MUSB_GADGET_PORT1)
#define USB_CTRL_REG	USB_CTRL1
#define OTG_REGS_BASE	((void *)AM335X_USB1_OTG_BASE)
#else
#error "Please define CONFIG_MUSB_GADGET_PORT0 or CONFIG_MUSB_GADGET_PORT1"
#endif

/* USB 2.0 PHY Control */
#define CM_PHY_PWRDN			(1 << 0)
#define CM_PHY_OTG_PWRDN		(1 << 1)
#define OTGVDET_EN			(1 << 19)
#define OTGSESSENDEN			(1 << 20)

static void am33xx_usb_set_phy_power(u8 on)
{
	u32 usb_ctrl_reg;

	usb_ctrl_reg = readl(USB_CTRL_REG);
	if (on) {
		usb_ctrl_reg &= ~(CM_PHY_PWRDN | CM_PHY_OTG_PWRDN);
		usb_ctrl_reg |= (OTGVDET_EN | OTGSESSENDEN);
	} else {
		usb_ctrl_reg |= (CM_PHY_PWRDN | CM_PHY_OTG_PWRDN);
	}
	writel(usb_ctrl_reg, USB_CTRL_REG);
}

static struct musb_hdrc_config musb_config = {
	.multipoint     = 1,
	.dyn_fifo       = 1,
	.num_eps        = 16,
	.ram_bits       = 12,
};

struct omap_musb_board_data musb_board_data = {
	.set_phy_power = am33xx_usb_set_phy_power,
};

static struct musb_hdrc_platform_data musb_plat = {
	.mode           = MUSB_PERIPHERAL,
	.config         = &musb_config,
	.power          = 50,
	.platform_ops	= &musb_dsps_ops,
	.board_data	= &musb_board_data,
};
#endif

#if defined(CONFIG_DRIVER_TI_CPSW) || \
	(defined(CONFIG_USB_ETHER) && defined(CONFIG_MUSB_GADGET) && \
	(!defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_USB_ETH_SUPPORT)))
int board_eth_init(bd_t *bis)
{
	int rv, n = 0;
#ifdef CONFIG_DRIVER_TI_CPSW
	uint8_t mac_addr[6];
	uint32_t mac_hi, mac_lo;
	u_int32_t i;

	if (!eth_getenv_enetaddr("ethaddr", mac_addr)) {
		debug("<ethaddr> not set. Reading from E-fuse\n");
		/* try reading mac address from efuse */
		mac_lo = readl(MAC_ID0_LO);
		mac_hi = readl(MAC_ID0_HI);
		mac_addr[0] = mac_hi & 0xFF;
		mac_addr[1] = (mac_hi & 0xFF00) >> 8;
		mac_addr[2] = (mac_hi & 0xFF0000) >> 16;
		mac_addr[3] = (mac_hi & 0xFF000000) >> 24;
		mac_addr[4] = mac_lo & 0xFF;
		mac_addr[5] = (mac_lo & 0xFF00) >> 8;

		if (!is_valid_ether_addr(mac_addr)) {
			debug("Did not find a valid mac address in e-fuse. "
					"Trying the one present in EEPROM\n");

			for (i = 0; i < ETH_ALEN; i++)
				mac_addr[i] = header.mac_addr[0][i];
		}

		if (is_valid_ether_addr(mac_addr))
			eth_setenv_enetaddr("ethaddr", mac_addr);
		else {
			printf("Caution: Using hardcoded mac address. "
				"Set <ethaddr> variable to overcome this.\n");
		}
	}

	if (board_id == BONE_BOARD) {
		/* For beaglebone > Rev A2 , enable MII mode, for others enable RMII */
		if (!strncmp(header.version, "00A1", 4) ||
		    !strncmp(header.version, "00A2", 4))
			writel(RMII_MODE_ENABLE, MAC_MII_SEL);
		else
			writel(MII_MODE_ENABLE, MAC_MII_SEL);
		/* No gigabit */
		cpsw_data.gigabit_en = 0;
	} else if (board_id == IA_BOARD) {
		cpsw_slaves[0].phy_id = 30;
		cpsw_slaves[1].phy_id = 0;
		/* No gigabit */
		cpsw_data.gigabit_en = 0;
	} else {
		/* set mii mode to rgmii in in device configure register */
		writel(RGMII_MODE_ENABLE, MAC_MII_SEL);
	}

	/* GP EVM 1.0 (A, B) does not have functional gigabit */
	if (board_id == GP_BOARD && !strncmp(header.version, "1.0", 3))
		cpsw_data.gigabit_en = 0;

	rv = cpsw_register(&cpsw_data);
	if (rv < 0)
		printf("Error %d registering CPSW switch\n", rv);
	else
		n += rv;
#endif
#if defined(CONFIG_USB_ETHER) && \
	(!defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_USB_ETH_SUPPORT))
	rv = musb_register(&musb_plat, &musb_board_data, OTG_REGS_BASE);
	if (rv < 0) {
		printf("Error %d registering MUSB device\n", rv);
	} else {
		rv = usb_eth_initialize(bis);
		if (rv < 0)
			printf("Error %d registering USB_ETHER\n", rv);
		else
			n += rv;
	}
#endif
	return n;
}
#endif

#ifndef CONFIG_SPL_BUILD
#ifdef CONFIG_GENERIC_MMC
int board_mmc_init(bd_t *bis)
{
	omap_mmc_init(0);
//	omap_mmc_init(1);
	return 0;
}
#endif

#ifdef CONFIG_NAND_TI81XX
/******************************************************************************
 * Command to switch between NAND HW and SW ecc
 *****************************************************************************/
extern void ti81xx_nand_switch_ecc(nand_ecc_modes_t hardware, int32_t mode);
int do_switch_ecc(cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	int type = 0;
	if (argc < 2)
		goto usage;

	if (strncmp(argv[1], "hw", 2) == 0) {
		if (argc == 3)
			type = simple_strtoul(argv[2], NULL, 10);
		ti81xx_nand_switch_ecc(NAND_ECC_HW, type);
	}
	else if (strncmp(argv[1], "sw", 2) == 0)
		ti81xx_nand_switch_ecc(NAND_ECC_SOFT, 0);
	else
		goto usage;

	return 0;

usage:
	printf("Usage: nandecc %s\n", cmdtp->usage);
	return 1;
}

U_BOOT_CMD(
	nandecc, 3, 1,	do_switch_ecc,
	"Switch NAND ECC calculation algorithm b/w hardware and software",
	"[sw|hw <hw_type>] \n"
	"   [sw|hw]- Switch b/w hardware(hw) & software(sw) ecc algorithm\n"
	"   hw_type- 0 for Hamming code\n"
	"            1 for bch4\n"
	"            2 for bch8\n"
	"            3 for bch16\n"
);

#endif /* CONFIG_NAND_TI81XX */
#endif /* CONFIG_SPL_BUILD */
