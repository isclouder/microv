#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/kvm.h>

#include "global.h"
#include "memory.h"
#include "string.h"

typedef struct KVMSlot
{
    uint64_t start_addr;
    uint64_t memory_size;
    void *ram;
    int slot;
    int flags;
} KVMSlot;

enum MemLayoutType
{
    MemBelow4g = 0,
    Mmio,
    IoApic,
    LocalApic,
    MemAbove4g,
    MemLayoutEnd
};

const uint64_t MemLayout[MemLayoutEnd][2] = {
    {0,                           0xC0000000    },  // MemBelow4g
    {0xF0100000,                  0x200         },  // Mmio
    {IO_APIC_DEFAULT_PHYS_BASE,   0x100000      },  // IoApic
    {APIC_DEFAULT_PHYS_BASE,      0x100000      },  // LocalApic
    {0x100000000,                 0x8000000000  }   // MemAbove4g
};

static uint64_t RamSize;
static struct kvm_userspace_memory_region MemMapper[2];

int init_memory_map(int vmfd, uint64_t ram_size)
{
    int ret;
    RamSize = ram_size;
    uint64_t rams[2][2] = {0};
    uint64_t gap_start = MemLayout[MemBelow4g][0] + MemLayout[MemBelow4g][1];

    rams[0][0] = 0;
    if(ram_size <= gap_start) {
        rams[0][1] = ram_size;
    }
    else {
        uint64_t gap_end = MemLayout[MemAbove4g][0];
        rams[0][1] = gap_start;
        rams[1][0] = gap_end;
        rams[1][1] = ram_size - gap_start;
    }

    for(int i=0;i<2;i++) {
        if(rams[i][1]<=0) continue;
        struct KVMSlot *slot = malloc(sizeof(struct KVMSlot));
        slot->memory_size = rams[i][1];
        slot->start_addr = rams[i][0];
        slot->slot = i;
        slot->flags = 0;
        slot->ram = mmap(NULL, slot->memory_size, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
                                  -1, 0);
        if ((void *)slot->ram == MAP_FAILED) {
            fprintf(stderr, "mmap vm ram failed\n");
            return -1;
        }

        MemMapper[i].flags = slot->flags;
        MemMapper[i].slot = slot->slot;
        MemMapper[i].guest_phys_addr =  slot->start_addr;
        MemMapper[i].memory_size = slot->memory_size;
        MemMapper[i].userspace_addr = (uint64_t)slot->ram;
        ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &(MemMapper[i]));
        if (ret < 0) {
            fprintf(stderr, "set user memory region failed\n");
            return -1;
        }
    }
}

uint64_t get_gap_start()
{
    return MemLayout[MemBelow4g][0] + MemLayout[MemBelow4g][1];
}

uint64_t get_gap_end()
{
    return MemLayout[MemAbove4g][0];
}

uint64_t get_ram_end()
{
    uint64_t gap_start = MemLayout[MemBelow4g][0] + MemLayout[MemBelow4g][1];

    if(RamSize <= MemLayout[MemBelow4g][1]) {
        return MemLayout[MemBelow4g][0] + RamSize;
    }
    else {
        return MemLayout[MemAbove4g][0] + (RamSize - MemLayout[MemBelow4g][1]);
    }
}

static int find_mapper_index(uint64_t guest_addr)
{
    for(int i=0;i<2;i++) {
        if(guest_addr >= MemMapper[i].guest_phys_addr
                && guest_addr < MemMapper[i].guest_phys_addr + MemMapper[i].memory_size) {
            return i;
        }
    }
    fprintf(stderr, "get memory region failed\n");
    return -1;
}

void write_userspace_memory(void *src, uint64_t guest_addr, uint64_t len)
{
    int mapper_index = find_mapper_index(guest_addr);
    uint64_t offset = guest_addr - MemMapper[mapper_index].guest_phys_addr;
    memcpy((void *)(MemMapper[mapper_index].userspace_addr + offset), src, len);
  
}

uint64_t get_userspace_addr(uint64_t guest_addr)
{
    int mapper_index = find_mapper_index(guest_addr);
    uint64_t offset = guest_addr - MemMapper[mapper_index].guest_phys_addr;
    return MemMapper[mapper_index].userspace_addr + offset;
}
