/*
 *  PlayStation 2 system setup functions.
 *
 *  Copyright (C) 2010-2013 Juergen Urban
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/console.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/param.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/platform_device.h>

#include <asm/bootinfo.h>

#include <asm/mach-ps2/ps2.h>
#include <asm/mach-ps2/dma.h>
#include <asm/mach-ps2/sifdefs.h>
#include <asm/mach-ps2/sbios.h>

#include "loadfile.h"
#include "reset.h"

void (*__wbflush)(void);

EXPORT_SYMBOL(__wbflush);

static struct resource ps2_usb_ohci_resources[] = {
	[0] = {
		.start	= 0xbf801600,
		.end	= 0xbf801700,
		/* OHCI needs IO memory. */
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_SBUS_USB,
		.end	= IRQ_SBUS_USB,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device ps2_usb_ohci_device = {
	.name		= "ps2_ohci",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(ps2_usb_ohci_resources),
	.resource	= ps2_usb_ohci_resources,
};

static struct platform_device ps2_smap_device = {
	.name           = "ps2smap",
	.id		= -1,
};

static struct platform_device ps2_smaprpc_device = {
	.name           = "ps2smaprpc",
	.id		= -1,
};

static struct platform_device ps2_gs_device = {
	.name           = "ps2fb",
	.id		= -1,
};

static struct platform_device ps2_sd_device = {
	.name           = "ps2sd",
	.id		= -1,
};

static struct platform_device ps2_audio_device = {
	.name           = "ps2audio",
	.id		= -1,
};

static struct platform_device ps2_sbios_device = {
	.name		= "ps2sbios-uart",
	.id		= 0,
};

static struct platform_device ps2_uart_device = {
	.name		= "ps2-uart",
	.id		= -1,
};

static unsigned int ps2_blink_frequency = 500;
module_param_named(panicblink, ps2_blink_frequency, uint, 0600);
MODULE_PARM_DESC(panicblink, "Frequency with which the PS2 HDD should blink when kernel panics");

static void ps2_wbflush(void)
{
	__asm__ __volatile__("sync.l":::"memory");

	/* flush write buffer to bus */
	inl(ps2sif_bustophys(0));
}

static long ps2_panic_blink(long count)
{
	static long last_blink;
	static int powerbtn_enabled = 0;

	if (!powerbtn_enabled) {
		powerbtn_enabled = 1;

		/* Enable power button, because init will not handle any signals. */
		ps2_powerbutton_enable_auto_shutoff(-1);
	}

	/*
	 * We expect frequency to be about 1/2s. KDB uses about 1s.
	 * Make sure they are different.
	 */
	if (!ps2_blink_frequency)
		return 0;
	if (count - last_blink < ps2_blink_frequency)
		return 0;

	/* TBD: Blink HDD led of fat PS2. */
	return 0;
}

void __init plat_mem_setup(void)
{
	ps2_reset_init();
	panic_blink = ps2_panic_blink;

	__wbflush = ps2_wbflush;

	/* IO port (out and in functions). */
	ioport_resource.start = 0x10000000;
	ioport_resource.end   = 0x1FFFFFFF;

	/* IO memory. */
	iomem_resource.start = 0x00000000;
	iomem_resource.end   = KSEG2 - 1;

	/* Memory for exception vectors. */
	add_memory_region(0x00000000, 0x00001000, BOOT_MEM_RAM);
	/* Reserved for SBIOS. */
	add_memory_region(0x00001000, 0x0000F000, BOOT_MEM_RESERVED);
	/* Free memory. */
	add_memory_region(0x00010000, 0x01ff0000, BOOT_MEM_RAM);

	/* Set base address of outb()/outw()/outl()/outq()/
	 * inb()/inw()/inl()/inq().
	 * This memory region is uncached.
	 */
	set_io_port_base(CKSEG1);

#ifdef CONFIG_SERIAL_PS2_SBIOS_DEFAULT
	/* Use PS2 SBIOS as default console. */
	add_preferred_console(PS2_SBIOS_SERIAL_DEVICE_NAME, 0, NULL);
#endif
}

static struct platform_device *ps2_platform_devices[] __initdata = {
	&ps2_gs_device,
	&ps2_usb_ohci_device,
	&ps2_sd_device,
	&ps2_audio_device,
	&ps2_sbios_device,
	&ps2_uart_device,
};

static int __init ps2_board_setup(void)
{
	ps2dma_init();
	ps2sif_init();
	ps2rtc_init();
	ps2_powerbutton_init();
#ifdef CONFIG_SYSFS_IOP_MODULES
	ps2_loadfile_init();
#endif

	switch (ps2_pccard_present) {
	case 0x0100:
		platform_device_register(&ps2_smap_device);
		break;
	case 0x0200:
		platform_device_register(&ps2_smaprpc_device);
		break;
	default:
		printk("No SMAP network device found.");
		break;
	}

	platform_add_devices(ps2_platform_devices, ARRAY_SIZE(ps2_platform_devices));

	return 0;
}
arch_initcall(ps2_board_setup);
