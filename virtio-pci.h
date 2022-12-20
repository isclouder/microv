#ifndef MICROV_VIRTIO_PCI_H
#define MICROV_VIRTIO_PCI_H

#include <linux/virtio_pci.h>

#include "pci.h"
#include "virtqueue.h"

struct virtio_pci_isr_cfg {
    uint32_t isr_status;
};

struct virtio_pci_notify_cfg {
    uint16_t vqn;
    uint16_t next;
} __attribute__((packed));

struct virtio_pci_config {
    struct virtio_pci_common_cfg common_cfg;
    struct virtio_pci_isr_cfg isr_cfg;
    struct virtio_pci_notify_cfg notify_cfg;
    void *dev_cfg;
};

struct virtio_pci_dev {
    int vmfd;
    struct pci_dev pci_dev;
    struct virtio_pci_config config;
    uint64_t device_feature;
    uint64_t guest_feature;
    struct virtio_pci_notify_cap *notify_cap;
    struct virtio_pci_cap *dev_cfg_cap;
    struct virtq *vq;
};

void virtio_pci_set_dev_cfg(struct virtio_pci_dev *virtio_pci_dev,
                            void *dev_cfg,
                            uint8_t len);
void virtio_pci_set_virtq_cfg(struct virtio_pci_dev *dev,
                          struct virtq *vq,
                          uint16_t num_queues);
void virtio_pci_enable(struct virtio_pci_dev *dev);
void virtio_pci_init(int vmfd,
                     struct virtio_pci_dev *dev,
                     uint16_t device_id,
                     uint32_t class, 
                     uint8_t irq_line);

#endif /* MICROV_VIRTIO_PCI_H */
