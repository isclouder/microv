#include <unistd.h>
#include <stdio.h>
#include <linux/kvm.h>
#include <sys/ioctl.h>

#include "memory.h"
#include "virtqueue.h"

void virtq_notify(struct virtq *vq)
{
    if (!vq->info.enable)
        return;
    vq->handle_output(vq);
}

void virtq_init(struct virtq *vq, void *dev,
                uint16_t queue_size, virtio_output_fn handle_output)
{
    vq->info.size = queue_size;
    vq->info.notify_off = 0;
    vq->info.enable = 0;
    vq->next_avail_idx = 0;
    vq->used_wrap_count = 1;
    vq->dev = dev;
    vq->handle_output = handle_output;
}

void virtq_enable(struct virtq *vq)
{
    if (vq->info.enable)
        return;
    vq->info.enable = true;

    vq->desc_ring = (struct vring_packed_desc *) get_userspace_addr(
        (uint64_t) vq->info.desc_addr);
    vq->guest_event = (struct vring_packed_desc_event *) get_userspace_addr(
        (uint64_t) vq->info.driver_addr); 
    vq->device_event = (struct vring_packed_desc_event *) get_userspace_addr(
        (uint64_t) vq->info.device_addr);
}

bool virtq_check_next(struct vring_packed_desc *desc)
{
    return desc->flags & VRING_DESC_F_NEXT;
}

struct vring_packed_desc *virtq_get_avail(struct virtq *vq)
{
    struct vring_packed_desc *desc = &vq->desc_ring[vq->next_avail_idx];
    uint16_t flags = desc->flags;
    bool avail = flags & (1ULL << VRING_PACKED_DESC_F_AVAIL);
    bool used = flags & (1ULL << VRING_PACKED_DESC_F_USED);

    if (avail != vq->used_wrap_count || used == vq->used_wrap_count) {
        return NULL;
    }
    vq->next_avail_idx++;
    if (vq->next_avail_idx >= vq->info.size) {
        vq->next_avail_idx -= vq->info.size;
        vq->used_wrap_count ^= 1;
    }
    return desc;
}

