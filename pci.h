#ifndef MICROV_PCI_H
#define MICROV_PCI_H

#include <stdint.h>
#include <stdbool.h>
#include <linux/pci_regs.h>

#include "iobus.h"

union pci_config_address {
    struct {
        unsigned reg_offset : 2;
        unsigned reg_num : 6;
        unsigned func_num : 3;
        unsigned dev_num : 5;
        unsigned bus_num : 8;
        unsigned reserved : 7;
        unsigned enable_bit : 1;
    };
    uint32_t value;
};

#define PCI_HDR_READ(hdr, offset, width) (*((uint##width##_t *) (hdr + offset)))
#define PCI_HDR_WRITE(hdr, offset, value, width) \
    ((uint##width##_t *) (hdr + offset))[0] = value
#define PCI_BAR_OFFSET(bar) (PCI_BASE_ADDRESS_0 + ((bar) << 2))

struct pci_dev {
    uint8_t cfg_space[PCI_CFG_SPACE_SIZE];
    void *hdr;
    struct region config_region;
    struct region bar_region[PCI_STD_NUM_BARS];
    uint32_t bar_size[PCI_STD_NUM_BARS];
    bool bar_active[PCI_STD_NUM_BARS];
    bool bar_is_io_space[PCI_STD_NUM_BARS];
};

void pcibus_init();
void pci_init_bar(struct pci_dev *dev,
                 uint8_t bar,
                 uint32_t bar_size,
                 bool is_io_space,
                 region_io_fn do_io);

void pci_dev_init(struct pci_dev *dev);

#endif /* MICROV_PCI_H */
