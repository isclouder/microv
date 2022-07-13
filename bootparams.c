#include <malloc.h>
#include <string.h>
#include "global.h"
#include "bootparams.h"
#include "memory.h"
#include "string.h"

#define BOOT_FLAG	0xAA55
#define HDRS		0x53726448
#define UNDEFINED_ID	0xFF

static const char CMDLINE[] = "console=ttyS0 panic=1 reboot=k root=/dev/ram rdinit=/bin/sh";

static void setup_e820(struct boot_params *boot_params)
{
    boot_params->e820_entries = 0;

    boot_params->e820_table[0] = (struct boot_e820_entry)
                { .addr = REAL_MODE_IVT_BEGIN,
                  .size = MPTABLE_START - REAL_MODE_IVT_BEGIN,
                  .type = E820_RAM
                };
    boot_params->e820_table[1] = (struct boot_e820_entry)
                { .addr = MPTABLE_START,
                  .size = VGA_RAM_BEGIN - MPTABLE_START,
                  .type = E820_RESERVED
                };
    boot_params->e820_table[2] = (struct boot_e820_entry)
                { .addr = MB_BIOS_BEGIN,
                  .size = 0,
                  .type = E820_RESERVED
                };

    if(get_ram_end() <= get_gap_end()) {
        boot_params->e820_table[3] = (struct boot_e820_entry)
                { .addr = VMLINUX_RAM_START,
                  .size = get_ram_end() - VMLINUX_RAM_START,
                  .type = E820_RAM
                };
        boot_params->e820_entries = 4;
    } else {
        boot_params->e820_table[3] = (struct boot_e820_entry) 
                { .addr = VMLINUX_RAM_START,
                  .size = get_gap_start() - VMLINUX_RAM_START,
                  .type = E820_RAM
                };
        boot_params->e820_table[4] = (struct boot_e820_entry) 
                { .addr = get_gap_end(),
                  .size = get_ram_end() - get_gap_end(),
                  .type = E820_RAM
                };
        boot_params->e820_entries = 5;

    }
}

static void setup_header_ramdisk(struct boot_params *boot_params)
{
    //8 bytes aligned ramdisk_image addr
    boot_params->hdr.ramdisk_image = InitrdAddr;
    boot_params->hdr.ramdisk_size = InitrdSize;
    boot_params->hdr.boot_flag = BOOT_FLAG;
    boot_params->hdr.header = HDRS,
    boot_params->hdr.type_of_loader = UNDEFINED_ID;
    boot_params->hdr.cmd_line_ptr = CMDLINE_START;
    boot_params->hdr.cmdline_size = sizeof(CMDLINE) - 1;
}

void setup_cmdline()
{
    write_userspace_memory((void *)CMDLINE, CMDLINE_START, sizeof(CMDLINE) - 1);
}

void setup_boot_params(void)
{
    struct boot_params *boot_params = (struct boot_params *)get_userspace_addr(ZERO_PAGE_START);
    memset(boot_params, 0, sizeof(struct boot_params));
    setup_e820(boot_params);
    setup_header_ramdisk(boot_params);
}

void _test_boot_params()
{
    struct boot_params *boot_params = (struct boot_params *)get_userspace_addr(ZERO_PAGE_START);

    fprintf(stderr, "boot_params->hdr.ramdisk_image:0x%lx\n", boot_params->hdr.ramdisk_image);
    fprintf(stderr, "boot_params->hdr.cmdline_size:0x%lx\n", boot_params->hdr.cmdline_size);
    fprintf(stderr, "boot_params->e820_entries:0x%ld\n", boot_params->e820_entries);

    for(int i=0;i<boot_params->e820_entries;i++){
        fprintf(stderr, "e820:0x%lx,0x%lx,0x%lx\n",
                        boot_params->e820_table[i].addr,
                        boot_params->e820_table[i].size,
                        boot_params->e820_table[i].type);
    }

    char cmdline[1024] = {0};
    memcpy(cmdline,(void *)get_userspace_addr(CMDLINE_START), sizeof(CMDLINE) - 1);
    fprintf(stderr, "cmdline:%s\n", cmdline);
}
