/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _VCPU_H
#define _VCPU_H

void setup_vcpu(int kvm_fd, int vcpu_fd, int vcpu_count, int vcpu_id);
void reset_vcpu(int kvm_fd, int vcpu_fd, int vcpu_count, int vcpu_id);

#endif /* _VCPU_H */
