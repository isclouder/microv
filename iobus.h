#ifndef MICROV_IOBUS_H
#define MICROV_IOBUS_H

#include <stdint.h>
#include <stdbool.h>
#include <linux/kvm.h>

typedef void (*region_io_fn)(uint64_t offset,
                          uint8_t size,
                          void *data,
                          uint8_t is_write,
                          void *owner);

struct region {
    struct bus *bus;
    uint64_t base;
    uint64_t len;
    void *owner;
    region_io_fn handle_io;
    struct region *next;
};

struct bus {
    uint64_t region_count;
    struct region *head;
};

struct region *iobus_find_region(struct bus *bus, uint64_t addr);
void iobus_register_region(struct bus *bus, struct region *region);
void iobus_deregister_region(struct region *region);
void iobus_init();
void region_init(struct region *region, uint64_t base,
                 uint64_t len, void *owner, region_io_fn do_io);
void iobus_handle_pio(struct kvm_run *run);
void iobus_handle_mmio(struct kvm_run *run);

#endif /* MICROV_IOBUS_H */
