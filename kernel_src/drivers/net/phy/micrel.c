/*
 * drivers/net/phy/micrel.c
 *
 * Driver for Micrel PHYs
 *
 * Author: David J. Choi
 *
 * Copyright (c) 2010-2013 Micrel, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Support : Micrel Phys:
 *		Giga phys: ksz9021, ksz9031
 *		100/10 Phys : ksz8001, ksz8721, ksz8737, ksz8041
 *			   ksz8021, ksz8031, ksz8051,
 *			   ksz8081, ksz8091,
 *			   ksz8061,
 *		Switch : ksz8873, ksz886x
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/micrel_phy.h>
#include <linux/of.h>

/* Operation Mode Strap Override */
#define MII_KSZPHY_OMSO				0x16
#define KSZPHY_OMSO_B_CAST_OFF			(1 << 9)
#define KSZPHY_OMSO_RMII_OVERRIDE		(1 << 1)
#define KSZPHY_OMSO_MII_OVERRIDE		(1 << 0)

/* general Interrupt control/status reg in vendor specific block. */
#define MII_KSZPHY_INTCS			0x1B
#define	KSZPHY_INTCS_JABBER			(1 << 15)
#define	KSZPHY_INTCS_RECEIVE_ERR		(1 << 14)
#define	KSZPHY_INTCS_PAGE_RECEIVE		(1 << 13)
#define	KSZPHY_INTCS_PARELLEL			(1 << 12)
#define	KSZPHY_INTCS_LINK_PARTNER_ACK		(1 << 11)
#define	KSZPHY_INTCS_LINK_DOWN			(1 << 10)
#define	KSZPHY_INTCS_REMOTE_FAULT		(1 << 9)
#define	KSZPHY_INTCS_LINK_UP			(1 << 8)
#define	KSZPHY_INTCS_ALL			(KSZPHY_INTCS_LINK_UP |\
			KSZPHY_INTCS_LINK_DOWN)

/* general PHY control reg in vendor specific block. */
#define	MII_KSZPHY_CTRL			0x1F
/* bitmap of PHY register to set interrupt mode */
#define KSZPHY_CTRL_INT_ACTIVE_HIGH		(1 << 9)
#define KSZ9021_CTRL_INT_ACTIVE_HIGH		(1 << 14)
#define KS8737_CTRL_INT_ACTIVE_HIGH		(1 << 14)
#define KSZ8051_RMII_50MHZ_CLK			(1 << 7)

/* Write/read to/from extended registers */
#define MII_KSZPHY_EXTREG                       0x0b
#define KSZPHY_EXTREG_WRITE                     0x8000

#define MII_KSZPHY_EXTREG_WRITE                 0x0c
#define MII_KSZPHY_EXTREG_READ                  0x0d

/* Extended registers */
#define MII_KSZPHY_CLK_CONTROL_PAD_SKEW         0x104
#define MII_KSZPHY_RX_DATA_PAD_SKEW             0x105
#define MII_KSZPHY_TX_DATA_PAD_SKEW             0x106

#define PS_TO_REG				200

static int ksz_config_flags(struct phy_device *phydev)
{
	int regval;

	if (phydev->dev_flags & MICREL_PHY_50MHZ_CLK) {
		regval = phy_read(phydev, MII_KSZPHY_CTRL);
		regval |= KSZ8051_RMII_50MHZ_CLK;
		return phy_write(phydev, MII_KSZPHY_CTRL, regval);
	}
	return 0;
}

static int kszphy_ack_interrupt(struct phy_device *phydev)
{
	/* bit[7..0] int status, which is a read and clear register. */
	int rc;

	rc = phy_read(phydev, MII_KSZPHY_INTCS);

	return (rc < 0) ? rc : 0;
}

static int kszphy_set_interrupt(struct phy_device *phydev)
{
	int temp;
	temp = (PHY_INTERRUPT_ENABLED == phydev->interrupts) ?
	KSZPHY_INTCS_ALL :
															0;
	return phy_write(phydev, MII_KSZPHY_INTCS, temp);
}

static int kszphy_config_intr(struct phy_device *phydev)
{
	int temp, rc;

	/* set the interrupt pin active low */
	temp = phy_read(phydev, MII_KSZPHY_CTRL);
	temp &= ~KSZPHY_CTRL_INT_ACTIVE_HIGH;
	phy_write(phydev, MII_KSZPHY_CTRL, temp);
	rc = kszphy_set_interrupt(phydev);
	return rc < 0 ? rc : 0;
}

static int kszphy_setup_led(struct phy_device *phydev,
		unsigned int reg, unsigned int shift)
{

	struct device *dev = &phydev->dev;
	struct device_node *of_node = dev->of_node;
	int rc, temp;
	u32 val;

	if (!of_node && dev->parent->of_node)
		of_node = dev->parent->of_node;

	if (of_property_read_u32(of_node, "micrel,led-mode", &val))
		return 0;

	temp = phy_read(phydev, reg);
	if (temp < 0)
		return temp;

	temp &= ~(3 << shift);
	temp |= val << shift;
	rc = phy_write(phydev, reg, temp);

	return rc < 0 ? rc : 0;
}

static int kszphy_config_init(struct phy_device *phydev)
{
	return 0;
}

static int kszphy_config_init_led8041(struct phy_device *phydev)
{

	struct device_node *of_node = phydev->dev.of_node;

	/* single led control, register 0x1e bits 15..14 */
	return kszphy_setup_led(phydev, 0x1e, 14);
}

static struct phy_driver ksz8041_driver = {
		.phy_id = PHY_ID_KSZ8041,
		.phy_id_mask = 0x00fffff0,
		.name = "Micrel KSZ8041",
		.features = (PHY_BASIC_FEATURES | SUPPORTED_Pause
				| SUPPORTED_Asym_Pause),
		.flags = PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
		.config_init = kszphy_config_init_led8041,
		.config_aneg = genphy_config_aneg,
		.read_status = genphy_read_status,
		.ack_interrupt = kszphy_ack_interrupt,
		.config_intr = kszphy_config_intr,
		.suspend = genphy_suspend,
		.resume = genphy_resume,
		.driver = { .owner = THIS_MODULE, },
};

static int __init ksphy_init(void)
{
	return phy_driver_register(&ksz8041_driver);
}

static void __exit ksphy_exit(void)
{
	phy_driver_unregister(&ksz8041_driver);
}

module_init(ksphy_init);
module_exit(ksphy_exit);

MODULE_DESCRIPTION("Micrel PHY driver");
MODULE_AUTHOR("David J. Choi");
MODULE_LICENSE("GPL");

static struct mdio_device_id __maybe_unused micrel_tbl[] = {
    { PHY_ID_KSZ8041, 0x00fffff0 },
    { }
};

MODULE_DEVICE_TABLE(mdio, micrel_tbl);
