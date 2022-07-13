#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdint.h>

#define RAM_SIZE 			0x20000000

#define INITRD_ADDR_MAX        	 	0x37ffffff

#define APIC_DEFAULT_PHYS_BASE		0xfee00000
#define IO_APIC_DEFAULT_PHYS_BASE	0xfec00000

#define VMLINUX_STARTUP   		0x01000000
#define VMLINUX_RAM_START       	0x00100000
#define MB_BIOS_BEGIN           	0x000f0000
#define VGA_RAM_BEGIN           	0x000a0000
#define MPTABLE_START			0x0009fc00
#define CMDLINE_START           	0x00020000
#define PDE_START         		0x0000b000
#define PDPTE_START			0x0000a000
#define PML4_START        		0x00009000
#define BOOT_LOADER_SP			0x00008ff0
#define ZERO_PAGE_START         	0x00007000
#define BOOT_IDT_START                  0x00000520
#define BOOT_GDT_START                  0x00000500
#define REAL_MODE_IVT_BEGIN     	0x00000000

#define MMIO_SERIAL_ADDR		0x000003f8
#define MMIO_SERIAL_SIZE		0x00000008

uint64_t InitrdAddr;
uint64_t InitrdSize;

#endif /* GLOBAL_H */
