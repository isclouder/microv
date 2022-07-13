#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "loader.h"
#include "global.h"
#include "memory.h"

static int file_len(FILE *fp)
{
    int num;
    fseek(fp,0,SEEK_END);
    num=ftell(fp);
    fseek(fp,0,SEEK_SET);
    return num;
}

static void setup_initrd_addr(uint64_t initrd_size)
{
  uint64_t initrd_addr_max = INITRD_ADDR_MAX;
  if(initrd_addr_max  > get_ram_end()) {
      initrd_addr_max = get_ram_end();
  }
  InitrdAddr = (initrd_addr_max - initrd_size) & ~(uint64_t)0xfff;
}

void load_initrd(const char *path)
{
    FILE *fp;
    int len;
    if((fp = fopen(path, "r")) == NULL)
    {
        printf("open file %s error.\n",path);
        exit(0);
    }
    InitrdSize = file_len(fp); 
    setup_initrd_addr(InitrdSize);
    fread((uint8_t *)get_userspace_addr(InitrdAddr), InitrdSize, 1, fp);
    fprintf(stderr, "load initrd at 0x%lx size: 0x%lx\n", InitrdAddr, InitrdSize);
    fclose(fp);
}

void load_kernel(const char *path)
{
    FILE *fp;
    int len;
    if((fp = fopen(path, "r")) == NULL)
    {
        printf("open file %s error.\n",path);
        exit(0);
    }
    len = file_len(fp); 
    fread((uint8_t *)get_userspace_addr(VMLINUX_STARTUP), len, 1, fp);
    fprintf(stderr, "load kernel at 0x%lx size: 0x%lx\n", VMLINUX_STARTUP, len);
    fclose(fp);
}

