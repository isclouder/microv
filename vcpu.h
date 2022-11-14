#ifndef MICROV_VCPU_H
#define MICROV_VCPU_H

void setup_vcpu(int kvm_fd, int vcpu_fd, int vcpu_count, int vcpu_id);
void reset_vcpu(int kvm_fd, int vcpu_fd, int vcpu_count, int vcpu_id);

#endif /* MICROV_VCPU_H */
