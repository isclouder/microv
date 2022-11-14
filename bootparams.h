#ifndef MICROV_BOOTPARAM_H
#define MICROV_BOOTPARAM_H

#include <stdint.h>

#define E820_MAX_ENTRIES_ZEROPAGE 0x80
#define E820_RAM        1
#define E820_RESERVED   2
#define E820_ACPI       3
#define E820_NVS        4
#define E820_UNUSABLE   5

struct boot_e820_entry {
        uint64_t addr;  /* start of memory segment */
        uint64_t size;  /* size of memory segment */
        uint32_t type;  /* type of memory segment */
} __attribute__((packed));

struct setup_header {
	uint8_t		setup_sects;
	uint16_t	root_flags;
	uint32_t	syssize;
	uint16_t	ram_size;
	uint16_t	vid_mode;
	uint16_t	root_dev;
	uint16_t	boot_flag;
	uint16_t	jump;
	uint32_t	header;
	uint16_t	version;
	uint32_t	realmode_swtch;
	uint16_t	start_sys_seg;
	uint16_t	kernel_version;
	uint8_t		type_of_loader;
	uint8_t		loadflags;
	uint16_t	setup_move_size;
	uint32_t	code32_start;
	uint32_t	ramdisk_image;
	uint32_t	ramdisk_size;
	uint32_t	bootsect_kludge;
	uint16_t	heap_end_ptr;
	uint8_t		ext_loader_ver;
	uint8_t		ext_loader_type;
	uint32_t	cmd_line_ptr;
	uint32_t	initrd_addr_max;
	uint32_t	kernel_alignment;
	uint8_t		relocatable_kernel;
	uint8_t		min_alignment;
	uint16_t	xloadflags;
	uint32_t	cmdline_size;
	uint32_t	hardware_subarch;
	uint64_t	hardware_subarch_data;
	uint32_t	payload_offset;
	uint32_t	payload_length;
	uint64_t	setup_data;
	uint64_t	pref_address;
	uint32_t	init_size;
	uint32_t	handover_offset;
	uint32_t	kernel_info_offset;
} __attribute__((packed));;

struct boot_params {
	uint8_t  _pad_screen_info[0x40];			/* 0x000 */
	uint8_t  _pad_apm_bios_info[0x14];			/* 0x040 */
	uint8_t  _pad2[4];					/* 0x054 */
	uint64_t tboot_addr;					/* 0x058 */
	uint8_t  _pad_ist_info[0x10];				/* 0x060 */
	uint64_t acpi_rsdp_addr;				/* 0x070 */
	uint8_t  _pad3[8];					/* 0x078 */
	uint8_t  hd0_info[0x10];	/* obsolete! */		/* 0x080 */
	uint8_t  hd1_info[0x10];	/* obsolete! */		/* 0x090 */
	uint8_t  _pad_sys_desc_table[0x10]; /*obsolete!*/	/* 0x0a0 */
	uint8_t  _pad_olpc_ofw_header[0x10];			/* 0x0b0 */
	uint32_t ext_ramdisk_image;				/* 0x0c0 */
	uint32_t ext_ramdisk_size;				/* 0x0c4 */
	uint32_t ext_cmd_line_ptr;				/* 0x0c8 */
	uint8_t  _pad4[0x74];					/* 0x0cc */
	uint8_t  _pad_edid_info[0x80];				/* 0x140 */
	uint8_t  _pad_efi_info[0x20];				/* 0x1c0 */
	uint32_t alt_mem_k;					/* 0x1e0 */
	uint32_t scratch;		/* Scratch field! */	/* 0x1e4 */
	uint8_t  e820_entries;					/* 0x1e8 */
	uint8_t  eddbuf_entries;				/* 0x1e9 */
	uint8_t  edd_mbr_sig_buf_entries;			/* 0x1ea */
	uint8_t  kbd_status;					/* 0x1eb */
	uint8_t  secure_boot;					/* 0x1ec */
	uint8_t  _pad5[2];					/* 0x1ed */
	uint8_t  sentinel;					/* 0x1ef */
	uint8_t  _pad6;						/* 0x1f0 */
	struct   setup_header hdr;    /* setup header */	/* 0x1f1 */
	uint8_t  _pad7[0x290-0x1f1-sizeof(struct setup_header)];
	uint8_t  _pad_edd_mbr_sig_buffer[0x40];			/* 0x290 */
	struct   boot_e820_entry e820_table[E820_MAX_ENTRIES_ZEROPAGE]; /* 0x2d0 */
	uint8_t  _pad8[0x30];					/* 0xcd0 */
	uint8_t  _pad_edd_info[0x1ec];				/* 0xd00 */
	uint8_t  _pad9[276];					/* 0xeec */
} __attribute__((packed));;

void setup_cmdline();
void setup_boot_params(const char *vmlinux_path, const char *initrd_path);
void _test_boot_params();

#endif  /* MICROV_BOOTPARAM_H */
