#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <linux/kvm.h>

#include "ioeventfd.h"

#define IOEVENTFD_MAX_EVENTS 32

static int init_ioeventfd = 0;
static int epoll_fd, epoll_stop_fd;
static struct epoll_event event_sets[IOEVENTFD_MAX_EVENTS];
static LIST_HEAD(used_ioevents);

static void *ioeventfd_thread(void *param)
{
    int i, nfds;
    uint64_t tmp = 1;
    
    for (;;) {
        nfds = epoll_wait(epoll_fd, event_sets, IOEVENTFD_MAX_EVENTS, -1);
    	for (i = 0; i < nfds; i++) {
    	    struct ioevent *ioevent;
    
    	    if (event_sets[i].data.fd == epoll_stop_fd) {
                write(epoll_stop_fd, &tmp, sizeof(tmp));
                return NULL;
    	    }

    	    ioevent = event_sets[i].data.ptr;
    	    if (read(ioevent->kvm_ioeventfd.fd, &tmp, sizeof(tmp)) < 0) {
    	        fprintf(stderr, "failed reading event.\n");
    	    }

    	    ioevent->fn(ioevent->fn_ptr);
    	}
    }

    return NULL;
}

int ioeventfd_add_event(int vmfd, struct ioevent *ioevent)
{
    struct kvm_ioeventfd kvm_ioevent;
    struct epoll_event epoll_event;
    struct ioevent *new_ioevent;
    int event, ret;
    if(!init_ioeventfd) {
    	fprintf(stderr, "ioevent has no inited.\n");
        return -1;
    }

    //kvm ioeventfd
    ret = ioctl(vmfd, KVM_IOEVENTFD, &(ioevent->kvm_ioeventfd));
    if (ret) {
    	fprintf(stderr, "ioctl kvm ioeventfd failed.\n");
        goto cleanup;
    }

    //add event to epoll
    new_ioevent = malloc(sizeof(*new_ioevent));
    if (!new_ioevent) {
    	fprintf(stderr, "malloc ioevent failed.\n");
        return -1;
    }
    *new_ioevent = *ioevent;

    epoll_event = (struct epoll_event) {
        .events		= EPOLLIN,
        .data.ptr	= new_ioevent,
    };
    
    ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_ioevent->kvm_ioeventfd.fd, &epoll_event);
    if (ret) {
    	fprintf(stderr, "ctl epoll add failed.\n");
        goto cleanup;
    }
    
    list_add_tail(&new_ioevent->list, &used_ioevents);
    
    return 0;

cleanup:
    free(new_ioevent);
    return ret;
}

int ioeventfd_init(int vmfd)
{
    int ret;
    pthread_t thread;
    struct epoll_event epoll_event = { .events = EPOLLIN };

    if(ioctl(vmfd, KVM_CHECK_EXTENSION, KVM_CAP_IOEVENTFD) <= 0) {
        fprintf(stderr, "kvm not supportl ioevent fd\n");
        return -1;
    }
    
    epoll_fd = epoll_create(IOEVENTFD_MAX_EVENTS);
    if (epoll_fd < 0) {
        fprintf(stderr, "create epoll failed\n");
        return -1;
    }
    
    epoll_stop_fd = eventfd(0, 0);
    epoll_event.data.fd   = epoll_stop_fd;
    
    ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, epoll_stop_fd, &epoll_event);
    if (ret < 0)
    	goto epoll_err;
    
    ret = pthread_create(&thread, NULL, ioeventfd_thread, NULL);
    if (ret < 0)
        goto epoll_err;
    
    init_ioeventfd = 1;
    fprintf(stderr, "ioevent init done\n");
    return 0;

epoll_err:
    fprintf(stderr, "ioevent init failed\n");
    close(epoll_stop_fd);
    close(epoll_fd);

    return ret;
}

int ioeventfd_exit()
{
    int ret;
    uint64_t tmp = 1;

    if (!init_ioeventfd)
        return 0;

    ret = write(epoll_stop_fd, &tmp, sizeof(tmp));
    if (ret < 0)
        return ret;

    ret = read(epoll_stop_fd, &tmp, sizeof(tmp));
    if (ret < 0)
        return ret;

    close(epoll_fd);
    close(epoll_stop_fd);

    return 0;
}
