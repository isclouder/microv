#ifndef MICROV_IOEVENTFD_H
#define MICROV_IOEVENTFD_H

#include <linux/kvm.h>
#include "list.h"

struct ioevent {
	struct kvm_ioeventfd kvm_ioeventfd;
	void		*fn_ptr;
	struct list_head list;
	void(*fn)(void *ptr);
};

int ioeventfd_add_event(int vmfd, struct ioevent *ioevent);
int ioeventfd_init();
int ioeventfd_exit();

#endif /* MICROV_IOEVENTFD_H */
