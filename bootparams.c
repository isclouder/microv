#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include "global.h"
#include "bootparams.h"
#include "memory.h"
#include "string.h"

#define BOOT_FLAG	0xAA55
#define HDRS		0x53726448
#define UNDEFINED_ID	0xFF

uint64_t InitrdSize;
uint64_t InitrdAddr;
static const char CMDLINE[] = "console=ttyS0 pci=conf1 panic=1 reboot=k root=/dev/ram rdinit=/bin/sh";

static int file_len(FILE *fp)
{
    int num;
    fseek(fp,0,SEEK_END);
    num=ftell(fp);
    fseek(fp,0,SEEK_SET);
    return num;
}

static void load_initrd(const char *initrd_path)
{
    FILE *fp;
    int len;
    if((fp = fopen(initrd_path, "r")) == NULL)
    {
        printf("open file %s error.\n",initrd_path);
        exit(0);
    }
    
    InitrdSize = file_len(fp); 
    uint64_t initrd_addr_max = INITRD_ADDR_MAX;
    if(initrd_addr_max  > get_ram_end()) {
        initrd_addr_max = get_ram_end();
    }
    InitrdAddr = (initrd_addr_max - InitrdSize) & ~(uint64_t)0xfff;
    
    fread((uint8_t *)get_userspace_addr(InitrdAddr), InitrdSize, 1, fp);
    fprintf(stderr, "load initrd at 0x%lx size: 0x%lx\n", InitrdAddr, InitrdSize);
    fclose(fp);
}

static void load_kernel(const char *vmlinux_path)
{
    FILE *fp;
    int len;
    if((fp = fopen(vmlinux_path, "r")) == NULL)
    {
        printf("open file %s error.\n",vmlinux_path);
        exit(0);
    }
    len = file_len(fp);
    fread((uint8_t *)get_userspace_addr(VMLINUX_START), len, 1, fp);
    fprintf(stderr, "load kernel at 0x%lx size: 0x%lx\n", VMLINUX_START, len);
    fclose(fp);
}

static void setup_e820(struct boot_params *boot_params)
{
    boot_params->e820_entries = 0;

    boot_params->e820_table[0] = (struct boot_e820_entry)
                { .addr = REAL_MODE_IVT_START,
                  .size = MPTABLE_START - REAL_MODE_IVT_START,
                  .type = E820_RAM
                };
    boot_params->e820_table[1] = (struct boot_e820_entry)
                { .addr = MPTABLE_START,
                  .size = VGA_RAM_START - MPTABLE_START,
                  .type = E820_RESERVED
                };
    boot_params->e820_table[2] = (struct boot_e820_entry)
                { .addr = MB_BIOS_START,
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

void setup_boot_params(const char *vmlinux_path, const char *initrd_path)
{
    struct boot_params *boot_params = (struct boot_params *)get_userspace_addr(ZERO_PAGE_START);
    memset(boot_params, 0, sizeof(struct boot_params));
    load_kernel(vmlinux_path);
    load_initrd(initrd_path);
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
