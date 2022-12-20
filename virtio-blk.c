#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/eventfd.h>
#include <sys/stat.h>

#include "memory.h"
#include "virtio-blk.h"

#define VIRTIO_PCI_DEVICE_ID_BLK 0x1042
#define VIRTIO_BLK_PCI_CLASS 0x018000
#define VIRTIO_BLK_DEVICE_IRQ 15
#define VIRTQUEUE_SIZE 128

ssize_t diskimg_read(struct diskimg *diskimg,
                     void *data,
                     off_t offset,
                     size_t size)
{
    if(offset < diskimg->size) {
        lseek(diskimg->fd, offset, SEEK_SET);
        return read(diskimg->fd, data, size);
    }
    return -1;
}

ssize_t diskimg_write(struct diskimg *diskimg,
                      void *data,
                      off_t offset,
                      size_t size)
{
    if(offset < diskimg->size) {
        lseek(diskimg->fd, offset, SEEK_SET);
        return write(diskimg->fd, data, size);
    }
    return -1;
}

int diskimg_init(struct diskimg *diskimg, const char *file_path)
{
    diskimg->fd = open(file_path, O_RDWR);
    if (diskimg->fd < 0)
        return -1;
    struct stat st;
    fstat(diskimg->fd, &st);
    diskimg->size = st.st_size;
    return 0;
}

void diskimg_exit(struct diskimg *diskimg)
{
    close(diskimg->fd);
}


static ssize_t virtio_blk_write(struct virtio_blk_dev *dev,
                                void *data,
                                uint64_t sector,
                                size_t size)
{
    off_t offset = sector * 512;
    return diskimg_write(dev->diskimg, data, offset, size);
}

static ssize_t virtio_blk_read(struct virtio_blk_dev *dev,
                               void *data,
                               uint64_t sector,
                               size_t size)
{
    off_t offset = sector * 512;
    return diskimg_read(dev->diskimg, data, offset, size);
}

static void virtio_blk_handle_output(struct virtq *vq)
{
    struct virtio_blk_dev *dev = (struct virtio_blk_dev *) vq->dev;
    uint8_t status;
    struct vring_packed_desc *desc;
    struct virtio_blk_req req;

    while ((desc = virtq_get_avail(vq))) {
        struct vring_packed_desc *used_desc = desc;
        ssize_t r = 0;

        memcpy(&req.hdr, (void *)get_userspace_addr((uint64_t) desc->addr), desc->len);
        if (req.hdr.type == VIRTIO_BLK_T_IN || req.hdr.type == VIRTIO_BLK_T_OUT) {
            if (!virtq_check_next(desc))
                return;
            desc = virtq_get_avail(vq);
            req.data = (uint8_t *)get_userspace_addr((uint64_t) desc->addr);

            if (req.hdr.type == VIRTIO_BLK_T_IN)
                r = virtio_blk_read(dev, req.data, req.hdr.sector, desc->len);
            else
                r = virtio_blk_write(dev, req.data, req.hdr.sector, desc->len);

            status = r < 0 ? VIRTIO_BLK_S_IOERR : VIRTIO_BLK_S_OK;
        } else {
            status = VIRTIO_BLK_S_UNSUPP;
        }
        if (!virtq_check_next(desc))
            return;
        desc = virtq_get_avail(vq);
        *(uint8_t *)get_userspace_addr((uint64_t) desc->addr) = status;

        used_desc->flags ^= (1ULL << VRING_PACKED_DESC_F_USED);
        used_desc->len = r;
    }

    if (vq->guest_event->flags == VRING_PACKED_EVENT_FLAG_ENABLE) {
        dev->virtio_pci_dev.config.isr_cfg.isr_status |= 1;
        uint64_t n = 1;
        if (write(dev->irqfd, &n, sizeof(n)) < 0)
            fprintf(stderr, "write irqfd failed\n");
    }
}

static void virtio_blk_setup(struct virtio_blk_dev *dev,
                             struct diskimg *diskimg)
{
    dev->diskimg = diskimg;
    dev->config.capacity = diskimg->size/512;
    dev->irq_num = VIRTIO_BLK_DEVICE_IRQ;
    dev->irqfd = eventfd(0, EFD_CLOEXEC);
    dev->ioevent_fd = eventfd(0, EFD_CLOEXEC);

    for (int i = 0; i < VIRTIO_BLK_VIRTQUEUE_NUM; i++) {
        virtq_init(&dev->vq[i], dev, VIRTQUEUE_SIZE, virtio_blk_handle_output);
    }
}

void virtio_blk_init_pci(int vmfd, struct virtio_blk_dev *virtio_blk_dev,
                         struct diskimg *diskimg)
{
    memset(virtio_blk_dev, 0x00, sizeof(struct virtio_blk_dev));
    virtio_blk_setup(virtio_blk_dev, diskimg);

    struct virtio_pci_dev *dev = &virtio_blk_dev->virtio_pci_dev;
    virtio_pci_init(vmfd, dev,
                    VIRTIO_PCI_DEVICE_ID_BLK,
                    VIRTIO_BLK_PCI_CLASS,
                    virtio_blk_dev->irq_num);
    virtio_pci_set_dev_cfg(dev, &virtio_blk_dev->config, sizeof(virtio_blk_dev->config));
    virtio_pci_set_virtq_cfg(dev, virtio_blk_dev->vq, VIRTIO_BLK_VIRTQUEUE_NUM);

    struct kvm_irqfd irqfd = {
        .fd = virtio_blk_dev->irqfd,
        .gsi = virtio_blk_dev->irq_num,
        .flags = 0,
    };
    if (ioctl(dev->vmfd, KVM_IRQFD, &irqfd) < 0) {
        fprintf(stderr, "ioctl kvm irqfd failed\n");
    }
}

void virtio_blk_exit(struct virtio_blk_dev *dev)
{
    diskimg_exit(dev->diskimg);
    close(dev->irqfd);
}
