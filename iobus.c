#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "iobus.h"

struct bus pio_bus;
struct bus mmio_bus;

struct region *iobus_find_region(struct bus *bus, uint64_t addr)
{
    struct region **p;
    p = &(bus->head);

    for (; *p; p = &(*p)->next) {
        uint64_t start = (*p)->base;
        uint64_t end = start + (*p)->len - 1;
        if (addr >= start && addr <= end)
            return *p;
    }
    return NULL;
}

void iobus_register_region(struct bus *bus, struct region *region)
{
    struct bus *p = bus;

    region->next = p->head;
    p->head = region;
    p->region_count++;

    region->bus = p;
}

void iobus_deregister_region(struct region *region)
{
    if (region->bus == NULL) {
        return;
    }

    struct region **p = &(region->bus->head);

    while (*p != region && *p) {
        p = &(*p)->next;
    }

    if (*p)
        *p = (*p)->next;
}

void iobus_init()
{
    pio_bus.region_count = 0;
    pio_bus.head = NULL;
    mmio_bus.region_count = 0;
    mmio_bus.head = NULL;
}

void region_init(struct region *region,
                 uint64_t base,
                 uint64_t len,
                 void *owner,
                 region_io_fn handle_io)
{
    region->bus = NULL;
    region->base = base;
    region->len = len;
    region->owner = owner;
    region->handle_io = handle_io;
    region->next = NULL;
}

static void bus_handle_io(struct bus *bus,
                          uint64_t addr,
                          uint8_t size,
                          void *data,
                          uint8_t is_write)
{
    struct region *region = iobus_find_region(bus, addr);

    if (region && addr + size - 1 <= region->base + region->len - 1) {
        region->handle_io(addr - region->base, size, data, is_write, region->owner);
    }
}

void iobus_handle_pio(struct kvm_run *run)
{
    void *data = (void *) run + run->io.data_offset;
    bool is_write = run->io.direction == KVM_EXIT_IO_OUT;

    for (int i = 0; i < run->io.count; i++) {
        bus_handle_io(&pio_bus,
                      run->io.port,
                      run->io.size,
                      data,
                      is_write);
        data += run->io.size;
    }
}

void iobus_handle_mmio(struct kvm_run *run)
{
    bus_handle_io(&mmio_bus,
                  run->mmio.phys_addr,
                  run->mmio.len,
                  run->mmio.data,
                  run->mmio.is_write);
}

