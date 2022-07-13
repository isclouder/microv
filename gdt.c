#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "memory.h"
#include "gdt.h"

#define BOOT_GDT_MAX			4
#define GDT_ENTRY_KERNEL_CS		2
#define GDT_ENTRY_KERNEL_DS		3

/* 8 byte segment descriptor */
struct desc_struct {
	uint16_t	limit0;
	uint16_t	base0;
	uint16_t	base1: 8, type: 4, s: 1, dpl: 2, p: 1;
	uint16_t	limit1: 4, avl: 1, l: 1, d: 1, g: 1, base2: 8;
} __attribute__((packed));

#define GDT_ENTRY_INIT(flags, base, limit)			\
	{							\
		.limit0		= (uint16_t) (limit),		\
		.limit1		= ((limit) >> 16) & 0x0F,	\
		.base0		= (uint16_t) (base),		\
		.base1		= ((base) >> 16) & 0xFF,	\
		.base2		= ((base) >> 24) & 0xFF,	\
		.type		= (flags & 0x0f),		\
		.s		= (flags >> 4) & 0x01,		\
		.dpl		= (flags >> 5) & 0x03,		\
		.p		= (flags >> 7) & 0x01,		\
		.avl		= (flags >> 12) & 0x01,		\
		.l		= (flags >> 13) & 0x01,		\
		.d		= (flags >> 14) & 0x01,		\
		.g		= (flags >> 15) & 0x01,		\
	}

static struct desc_struct STARTUP_GDT[BOOT_GDT_MAX] = {
	[0]                   = GDT_ENTRY_INIT(0,      0, 0),
	[1]                   = GDT_ENTRY_INIT(0,      0, 0),
	[GDT_ENTRY_KERNEL_CS] = GDT_ENTRY_INIT(0xa09b, 0, 0xfffff),
	[GDT_ENTRY_KERNEL_DS] = GDT_ENTRY_INIT(0xc093, 0, 0xfffff),
};

struct kvm_segment entry_to_kvm_seg(uint64_t flags, uint64_t base, uint64_t limit)
{
    struct kvm_segment seg;

    seg.base = base;
    seg.limit = limit;
    seg.type = (flags & 0xf);
    seg.present = ((flags >> (15 - 8)) & 0x1);
    seg.dpl = ((flags >> (13 - 8)) & 0x3);
    seg.db = ((flags >> (22 - 8)) & 0x1);
    seg.s = ((flags >> (12 - 8)) & 0x1);
    seg.l = ((flags >> (21 - 8)) & 0x1);
    seg.g = ((flags >> (23 - 8)) & 0x1);
    seg.avl = ((flags >> (20 - 8)) & 0x1);

    return seg;
}

void setup_gdt()
{
    uint64_t boot_gdt_addr = BOOT_GDT_START;
    for(int i=0; i<BOOT_GDT_MAX; i++) {
        *(struct desc_struct *)get_userspace_addr(boot_gdt_addr+i*8) = STARTUP_GDT[i];
    }
}

struct kvm_segment get_code_kvm_seg()
{
    struct kvm_segment seg;
    seg = entry_to_kvm_seg(0xa09b, 0, 0xfffff);
    return seg;
}

struct kvm_segment get_data_kvm_seg()
{
    struct kvm_segment seg;
    seg = entry_to_kvm_seg(0xc093, 0, 0xfffff);
    return seg;
}

void setup_idt()
{
    *(uint64_t *)get_userspace_addr(BOOT_IDT_START) = 0;
}

uint16_t get_gdt_limit()
{
    return sizeof(STARTUP_GDT) - 1;
}

uint16_t get_idt_limit()
{
    return sizeof(uint64_t) - 1;
}

void _test_gdt()
{
/*
    uint64_t entry = \
            ((base & 0xff000000) << (56 - 24)) | ((base & 0x00ffffff) << 16) | \
           ((limit & 0x000f0000) << (48 - 16)) | (limit & 0x0000ffff)     | \
           ((flags & 0x0000f0ff) << 40);
*/
/*
    uint64_t base = (entry >> 16 & 0x00ffffff) | (entry >> (56 - 24) & 0xff000000);
    uint64_t limit = (entry >> (48 - 16) & 0x000f0000) | (entry & 0x0000ffff);
    uint64_t flags = (entry >> 40) & 0x0000f0ff;
*/
    fprintf(stderr, "gdt addr:%ld gdt size:%ld\n",BOOT_GDT_START, get_gdt_limit());
    fprintf(stderr, "idt addr:%ld idt size:%ld\n",BOOT_IDT_START, get_idt_limit());
}

