#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "pci.h"

/***********************************************************************
pci bus
************************************************************************/

extern struct bus pio_bus;
extern struct bus mmio_bus;
struct bus pci_bus;

struct region pci_addr_region;
struct region pci_data_region;
union pci_config_address pci_addr;

static void pcibus_addr_io(uint64_t offset, uint8_t size, void *data, uint8_t is_write, void *owner)
{
    void *p = (void *)&pci_addr + offset; 
    if (is_write)
        memcpy(p, data, size);
    else    
        memcpy(data, p, size);
    pci_addr.reg_offset = 0;
}

static void pcibus_data_io(uint64_t offset, uint8_t size, void *data, uint8_t is_write, void *owner)
{
    uint64_t addr = pci_addr.value | offset; 
    struct region *region = iobus_find_region(&pci_bus, addr);

    if (region && addr + size - 1 <= region->base + region->len - 1) {
        //io pci config space
        region->handle_io(addr - region->base, size, data, is_write, region->owner);
    }
}

void pcibus_init()
{
    region_init(&pci_addr_region, IO_PCI_CONFIG_ADDR_START, IO_PCI_CONFIG_ADDR_SIZE, NULL, pcibus_addr_io);
    iobus_register_region(&pio_bus, &pci_addr_region);
    region_init(&pci_data_region, IO_PCI_CONFIG_DATA_START, IO_PCI_CONFIG_DATA_SIZE, NULL, pcibus_data_io);
    iobus_register_region(&pio_bus, &pci_data_region);
    pci_bus.region_count = 0;
    pci_bus.head = NULL; 
}

static void pcibus_register_dev(struct pci_dev *dev, region_io_fn handle_io)
{
    unsigned num = pci_bus.region_count;
    union pci_config_address addr = {.enable_bit = 1,
                                     .dev_num = num}; 
    //register pci config space
    region_init(&dev->config_region, addr.value, PCI_CFG_SPACE_SIZE, dev,
                handle_io);
    iobus_register_region(&pci_bus, &dev->config_region);
}

/***********************************************************************
pci deb
************************************************************************/

static void pci_bar_command(struct pci_dev *dev)
{
    bool enable_io = PCI_HDR_READ(dev->hdr, PCI_COMMAND, 16) & PCI_COMMAND_IO;
    bool enable_mem =
        PCI_HDR_READ(dev->hdr, PCI_COMMAND, 16) & PCI_COMMAND_MEMORY;
    for (int i = 0; i < PCI_STD_NUM_BARS; i++) {
        struct bus *bus = dev->bar_is_io_space[i] ? &pio_bus : &mmio_bus;
        bool enable = dev->bar_is_io_space[i] ? enable_io : enable_mem;

        if (enable) {
            uint32_t mask = ~(dev->bar_size[i] - 1);
            if (!dev->bar_active[i] && dev->bar_region[i].base & mask)
                iobus_register_region(bus, &dev->bar_region[i]);
            dev->bar_active[i] = true;
        }
        else {
            uint32_t mask = ~(dev->bar_size[i] - 1);
            if (dev->bar_active[i] && dev->bar_region[i].base & mask)
                iobus_deregister_region(&dev->bar_region[i]);
            dev->bar_active[i] = false;
        }
    }
}

static void pci_bar_config(struct pci_dev *dev, uint8_t bar)
{
    uint32_t mask = ~(dev->bar_size[bar] - 1);
    uint32_t old_bar = PCI_HDR_READ(dev->hdr, PCI_BAR_OFFSET(bar), 32);
    uint32_t new_bar = (old_bar & mask) | dev->bar_is_io_space[bar];
    PCI_HDR_WRITE(dev->hdr, PCI_BAR_OFFSET(bar), new_bar, 32);
    dev->bar_region[bar].base = new_bar;
}

static void pci_config_write(struct pci_dev *dev,
                             void *data,
                             uint64_t offset,
                             uint8_t size)
{
    void *p = dev->hdr + offset;

    memcpy(p, data, size);
    if (offset == PCI_COMMAND) {
        pci_bar_command(dev);
    } else if (offset >= PCI_BASE_ADDRESS_0 && offset <= PCI_BASE_ADDRESS_5) {
        uint8_t bar = (offset - PCI_BASE_ADDRESS_0) >> 2;
        pci_bar_config(dev, bar);
    }
}

static void pci_config_read(struct pci_dev *dev,
                            void *data,
                            uint64_t offset,
                            uint8_t size)
{
    void *p = dev->hdr + offset;
    memcpy(data, p, size);
}

static void pci_config_handle_io(uint64_t offset,
                                 uint8_t size,
                                 void *data,
                                 uint8_t is_write,
                                 void *owner)
{
    struct pci_dev *dev = (struct pci_dev *) owner;
    if (is_write)
        pci_config_write(dev, data, offset, size);
    else
        pci_config_read(dev, data, offset, size);
}

void pci_init_bar(struct pci_dev *dev,
                 uint8_t bar,
                 uint32_t bar_size,
                 bool is_io_space,
                 region_io_fn handle_io)
{
    PCI_HDR_WRITE(dev->hdr, PCI_BAR_OFFSET(bar), is_io_space, 32);
    dev->bar_size[bar] = bar_size;
    dev->bar_is_io_space[bar] = is_io_space;
    region_init(&dev->bar_region[bar], 0, bar_size, dev, handle_io);
}

void pci_dev_init(struct pci_dev *dev)
{
    memset(dev, 0x00, sizeof(struct pci_dev));
    dev->hdr = dev->cfg_space;
    pcibus_register_dev(dev, pci_config_handle_io);
}

