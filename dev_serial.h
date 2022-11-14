#ifndef _DEV_SERIAL_H
#define _DEV_SERIAL_H
#include <linux/kvm.h>

void create_serial_dev(int vmfd);
void handle_serial_io(struct kvm_run *kvm_run);

#endif
