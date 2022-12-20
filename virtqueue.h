#ifndef MICROV_VIRTQUEUE_H
#define MICROV_VIRTQUEUE_H

#include <linux/virtio_ring.h>
#include <stdbool.h>
#include <stdint.h>

struct virtq;
typedef void (*virtio_output_fn)(struct virtq *);

struct virtq_info {
    uint16_t size;
    uint16_t msix_vector;
    uint16_t enable;
    uint16_t notify_off;
    uint64_t desc_addr;
    uint64_t driver_addr;
    uint64_t device_addr;
} __attribute__((packed));

struct virtq {
    struct vring_packed_desc *desc_ring;
    struct vring_packed_desc_event *device_event;
    struct vring_packed_desc_event *guest_event;
    struct virtq_info info;
    void *dev;
    uint16_t next_avail_idx;
    bool used_wrap_count;
    virtio_output_fn handle_output;
};

void virtq_notify(struct virtq *vq);
void virtq_init(struct virtq *vq, void *dev, uint16_t queue_size, virtio_output_fn handle_output);
void virtq_enable(struct virtq *vq);
bool virtq_check_next(struct vring_packed_desc *desc);
struct vring_packed_desc *virtq_get_avail(struct virtq *vq);
void virtq_handle_avail(struct virtq *vq);


#endif /* MICROV_VIRTQUEUE_H */
