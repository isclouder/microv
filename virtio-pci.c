#include <fcntl.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <linux/virtio_config.h>
#include <sys/eventfd.h>

#include "pci.h"
#include "virtio-pci.h"
#include "ioeventfd.h"

#define VIRTIO_PCI_VENDOR_ID 0x1AF4
#define VIRTIO_PCI_CAP_NUM 5

#define container_of(ptr, type, member)               \
    ({                                                \
        void *__mptr = (void *) (ptr);                \
        ((type *) (__mptr - offsetof(type, member))); \
    })

static void virtio_pci_ioevent_callback(void *virtq)
{
    struct virtq *vq = virtq;
    virtq_notify(vq);
}

static void virtio_pci_init_ioeventfd(struct virtio_pci_dev *dev, uint16_t vqn)
{
    uint64_t base = PCI_HDR_READ(dev->pci_dev.hdr,
                                 PCI_BAR_OFFSET(dev->notify_cap->cap.bar), 32);
    uint64_t offset =
        dev->notify_cap->cap.offset +
        dev->notify_cap->notify_off_multiplier * dev->vq[vqn].info.notify_off;

    int flags = KVM_IOEVENTFD_FLAG_DATAMATCH;
    struct ioevent ioevent = (struct ioevent) {
        .kvm_ioeventfd.datamatch = vqn,
        .kvm_ioeventfd.addr      = base + offset,
        .kvm_ioeventfd.len       = 2,
        .kvm_ioeventfd.fd        = eventfd(0, 0),
        .kvm_ioeventfd.flags     = flags,
        .fn                      = virtio_pci_ioevent_callback,
        .fn_ptr                  = &(dev->vq[vqn]),
    };
    ioeventfd_add_event(dev->vmfd, &ioevent);
}

static void virtio_pci_cmd_select_device_feature(struct virtio_pci_dev *dev)
{
    uint32_t select = dev->config.common_cfg.device_feature_select;
    uint64_t feature = dev->device_feature;

    switch (select) {
    case 0:
        dev->config.common_cfg.device_feature = feature;
        break;
    case 1:
        dev->config.common_cfg.device_feature = feature >> 32;
        break;
    default:
        dev->config.common_cfg.device_feature = 0;
        break;
    }
}

static void virtio_pci_cmd_write_guest_feature(struct virtio_pci_dev *dev)
{
    uint32_t select = dev->config.common_cfg.guest_feature_select;
    uint32_t feature = dev->config.common_cfg.guest_feature;

    switch (select) {
    case 0:
        dev->guest_feature |= feature;
        break;
    case 1:
        dev->guest_feature |= (uint64_t) feature << 32;
        break;
    default:
        break;
    }
}

static void virtio_pci_cmd_select_virtq(struct virtio_pci_dev *dev)
{
    uint16_t select = dev->config.common_cfg.queue_select;
    struct virtio_pci_common_cfg *config = &dev->config.common_cfg;

    if (select < config->num_queues) {
        uint64_t offset = offsetof(struct virtio_pci_common_cfg, queue_size);
        memcpy((void *) config + offset, &dev->vq[select].info,
               sizeof(struct virtq_info));
    } else {
        config->queue_size = 0;
    }
}

static void virtio_pci_cmd_enable_virtq(struct virtio_pci_dev *dev)
{
    uint16_t select = dev->config.common_cfg.queue_select;
    virtq_enable(&dev->vq[select]);
    virtio_pci_init_ioeventfd(dev, select);
}

static void virtio_pci_iospace_write(struct virtio_pci_dev *dev,
                                   void *data,
                                   uint64_t offset,
                                   uint8_t size)
{
    //pci cfg
    if (offset < offsetof(struct virtio_pci_config, dev_cfg)) {
        memcpy((void *) &dev->config + offset, data, size);
        switch (offset) {
        case VIRTIO_PCI_COMMON_DFSELECT:
            virtio_pci_cmd_select_device_feature(dev);
            break;
        case VIRTIO_PCI_COMMON_GFSELECT:
            virtio_pci_cmd_write_guest_feature(dev);
            break;
        case VIRTIO_PCI_COMMON_Q_SELECT:
            virtio_pci_cmd_select_virtq(dev);
            break;
        case VIRTIO_PCI_COMMON_Q_ENABLE:
            if (dev->config.common_cfg.queue_enable)
                virtio_pci_cmd_enable_virtq(dev);
            else
                fprintf(stderr, "guest disable virtq\n");
            break;
        default:
            if (offset >= VIRTIO_PCI_COMMON_Q_SIZE &&
                offset <= VIRTIO_PCI_COMMON_Q_USEDHI) {
                uint16_t select = dev->config.common_cfg.queue_select;
                uint64_t info_offset = offset - VIRTIO_PCI_COMMON_Q_SIZE;
                if (select < dev->config.common_cfg.num_queues) {
                    memcpy((void *) &dev->vq[select].info + info_offset, data, size);
                }
            }
            else if (offset == offsetof(struct virtio_pci_config, notify_cfg)) {
                virtq_notify(&dev->vq[dev->config.notify_cfg.vqn]);
            }
            break;
        }
        return;
    }

    //dev cfg
    uint64_t dev_offset = offset - offsetof(struct virtio_pci_config, dev_cfg);
    memcpy((void *) dev->config.dev_cfg + dev_offset, data, size);
}

static void virtio_pci_iospace_read(struct virtio_pci_dev *dev,
                                  void *data,
                                  uint64_t offset,
                                  uint8_t size)
{
    if (offset < offsetof(struct virtio_pci_config, dev_cfg)) {
        memcpy(data, (void *) &dev->config + offset, size);
        if (offset == offsetof(struct virtio_pci_config, isr_cfg)) {
            dev->config.isr_cfg.isr_status = 0;
        }
    } else {
        /* dev config read */
        uint64_t dev_offset =
            offset - offsetof(struct virtio_pci_config, dev_cfg);
        memcpy(data, (void *) dev->config.dev_cfg + dev_offset, size);
    }
}

static void virtio_pci_iospace_handle_io(uint64_t offset,
                                         uint8_t size,
                                         void *data,
                                         uint8_t is_write,
                                         void *owner)
{
    struct virtio_pci_dev *virtio_pci_dev =
        container_of(owner, struct virtio_pci_dev, pci_dev);
    if (is_write) {
        virtio_pci_iospace_write(virtio_pci_dev, data, offset, size);
    }
    else {
        virtio_pci_iospace_read(virtio_pci_dev, data, offset, size);
    }
}

static void virtio_pci_set_cap(struct virtio_pci_dev *dev, uint8_t next)
{
    struct virtio_pci_cap *caps[VIRTIO_PCI_CAP_NUM + 1];

    for (int i = 1; i < VIRTIO_PCI_CAP_NUM + 1; i++) {
        caps[i] = dev->pci_dev.hdr + next;
        *caps[i] = (struct virtio_pci_cap){
            .cap_vndr = PCI_CAP_ID_VNDR,
            .cfg_type = i,
            .cap_len = sizeof(struct virtio_pci_cap),
            .bar = 0,
        };
        if (i == VIRTIO_PCI_CAP_NOTIFY_CFG || i == VIRTIO_PCI_CAP_PCI_CFG)
            caps[i]->cap_len += sizeof(uint32_t);
        next += caps[i]->cap_len;
        caps[i]->cap_next = next;
    }

    caps[VIRTIO_PCI_CAP_COMMON_CFG]->offset =
        offsetof(struct virtio_pci_config, common_cfg);
    caps[VIRTIO_PCI_CAP_COMMON_CFG]->length =
        sizeof(struct virtio_pci_common_cfg);

    caps[VIRTIO_PCI_CAP_NOTIFY_CFG]->offset =
        offsetof(struct virtio_pci_config, notify_cfg);
    caps[VIRTIO_PCI_CAP_NOTIFY_CFG]->length =
        sizeof(struct virtio_pci_notify_cfg);

    caps[VIRTIO_PCI_CAP_ISR_CFG]->offset =
        offsetof(struct virtio_pci_config, isr_cfg);
    caps[VIRTIO_PCI_CAP_ISR_CFG]->length = sizeof(struct virtio_pci_isr_cfg);

    caps[VIRTIO_PCI_CAP_DEVICE_CFG]->offset =
        offsetof(struct virtio_pci_config, dev_cfg);
    caps[VIRTIO_PCI_CAP_DEVICE_CFG]->length = 0;

    dev->notify_cap =
        (struct virtio_pci_notify_cap *) caps[VIRTIO_PCI_CAP_NOTIFY_CFG];
    dev->dev_cfg_cap = caps[VIRTIO_PCI_CAP_DEVICE_CFG];
}

void virtio_pci_set_dev_cfg(struct virtio_pci_dev *dev,
                            void *dev_cfg,
                            uint8_t len)
{
    dev->config.dev_cfg = dev_cfg;
    dev->dev_cfg_cap->length = len;
}

void virtio_pci_set_virtq_cfg(struct virtio_pci_dev *dev,
                          struct virtq *vq,
                          uint16_t num_queues)
{
    dev->config.common_cfg.num_queues = num_queues;
    dev->vq = vq;
}

void virtio_pci_init(int vmfd,
                     struct virtio_pci_dev *dev,
                     uint16_t device_id,
                     uint32_t class,
                     uint8_t irq_line)
{
    uint8_t cap_list = 0x40;

    memset(dev, 0x00, sizeof(struct virtio_pci_dev));
    dev->vmfd = vmfd;
    pci_dev_init(&dev->pci_dev);
    PCI_HDR_WRITE(dev->pci_dev.hdr, PCI_VENDOR_ID, VIRTIO_PCI_VENDOR_ID, 16);
    PCI_HDR_WRITE(dev->pci_dev.hdr, PCI_CAPABILITY_LIST, cap_list, 8);
    PCI_HDR_WRITE(dev->pci_dev.hdr, PCI_HEADER_TYPE, PCI_HEADER_TYPE_NORMAL, 8);
    PCI_HDR_WRITE(dev->pci_dev.hdr, PCI_INTERRUPT_PIN, 1, 8);
    PCI_HDR_WRITE(dev->pci_dev.hdr, PCI_STATUS, PCI_STATUS_CAP_LIST|PCI_STATUS_INTERRUPT, 16);
    PCI_HDR_WRITE(dev->pci_dev.hdr, PCI_DEVICE_ID, device_id, 16);
    PCI_HDR_WRITE(dev->pci_dev.hdr, PCI_CLASS_REVISION, class << 8, 32);
    PCI_HDR_WRITE(dev->pci_dev.hdr, PCI_INTERRUPT_LINE, irq_line, 8);
    pci_init_bar(&dev->pci_dev, 0, 0x100, PCI_BASE_ADDRESS_SPACE_MEMORY,
                virtio_pci_iospace_handle_io);
    virtio_pci_set_cap(dev, cap_list);
    dev->device_feature |=
        (1ULL << VIRTIO_F_RING_PACKED) | (1ULL << VIRTIO_F_VERSION_1);
}

