#ifndef MICROV_VIRTIO_BLK_H
#define MICROV_VIRTIO_BLK_H

#include <linux/virtio_blk.h>
#include "virtio-pci.h"

#define VIRTIO_BLK_VIRTQUEUE_NUM 1

struct diskimg {
    int fd;
    size_t size;
};

struct virtio_blk_dev {
    struct virtio_pci_dev virtio_pci_dev;
    struct virtio_blk_config config;
    struct virtq vq[VIRTIO_BLK_VIRTQUEUE_NUM];
    int irqfd;
    int irq_num;
    int ioevent_fd;
    pthread_t io_thread;
    struct diskimg *diskimg;
};

struct virtio_blk_req {
    struct virtio_blk_outhdr hdr;
    uint8_t *data;
};

int diskimg_init(struct diskimg *diskimg, const char *file_path);
void diskimg_exit(struct diskimg *diskimg);

void virtio_blk_exit(struct virtio_blk_dev *dev);
void virtio_blk_init_pci(int vmfd,
                         struct virtio_blk_dev *dev,
                         struct diskimg *diskimg);

#endif /* MICROV_VIRTIO_BLK_H */
