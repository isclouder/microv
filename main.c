#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/kvm.h>
#include <linux/errno.h>

#include "global.h"
#include "memory.h"
#include "mptable.h"
#include "bootparams.h"
#include "gdt.h"
#include "vcpu.h"
#include "serial.h"

#define KVM_API_VERSION 12
#define VCPU_ID 0
#define VCPU_COUNT 1

#define DPRINTF(fmt, ...) \
    do { fprintf(stderr, fmt, ## __VA_ARGS__); } while (0)

struct KVMState {
   int fd;
   int vmfd;
};

typedef struct VCPUState {
    int vcpu_fd;
    struct kvm_run *kvm_run;
} X86VCPUState;

struct KVMState *kvm_state;

const char VMLINUX_FILE[] = "/root/img/vmlinux.bin";
const char INITRD_FILE[] = "/root/img/initrd.img";

static void init_vcpu(struct VCPUState *vcpu)
{
    long mmap_size;

    vcpu->vcpu_fd = ioctl(kvm_state->vmfd, KVM_CREATE_VCPU, VCPU_ID);
    if (vcpu->vcpu_fd < 0) {
        fprintf(stderr, "kvm_create_vcpu failed\n");
    }
    mmap_size = ioctl(kvm_state->fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    if (mmap_size < 0) {
        fprintf(stderr, "KVM_GET_VCPU_MMAP_SIZE failed\n");
    }
    vcpu->kvm_run = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                        vcpu->vcpu_fd, 0);
    if (vcpu->kvm_run == MAP_FAILED) {
        fprintf(stderr, "mmap'ing vcpu state failed\n");
    }
}

static int destroy_vcpu(struct VCPUState *vcpu)
{
    int ret = 0;
    long mmap_size;

    mmap_size = ioctl(kvm_state->fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    if (mmap_size < 0) {
        fprintf(stderr, "KVM_GET_VCPU_MMAP_SIZE failed\n");
    }
    ret = munmap(vcpu->kvm_run, mmap_size);
    if (ret < 0) {
        fprintf(stderr, "munmap vcpu state failed\n");
    }
}

static int vcpu_exec(struct VCPUState *vcpu)
{
    struct kvm_run *run = vcpu->kvm_run;
    int ret, run_ret;
    do{
        //sleep(1);
        run_ret = ioctl(vcpu->vcpu_fd, KVM_RUN, 0);
        if (run_ret < 0) {
            fprintf(stderr, "error: kvm run failed %s\n",
                    strerror(-run_ret));
            ret = -1;
            break;
        }
        switch (run->exit_reason) {
        case KVM_EXIT_HLT:
	    DPRINTF("hlt\n");
            return 0;
        case KVM_EXIT_IO:
            if(MMIO_SERIAL_START<=run->io.port && \
               run->io.port<MMIO_SERIAL_START+MMIO_SERIAL_SIZE){
                handle_serial_io(run);
            }
            ret = 0;
            break;
        case KVM_EXIT_MMIO:
            DPRINTF("handle_mmio\n");
            ret = 0;
            break;
        case KVM_EXIT_IRQ_WINDOW_OPEN:
            DPRINTF("irq_window_open\n");
            ret = -1;
            break;
        case KVM_EXIT_SHUTDOWN:
            DPRINTF("shutdown\n");
            ret = -1;
            break;
        case KVM_EXIT_UNKNOWN:
            fprintf(stderr, "KVM: unknown exit, hardware reason  %" PRIx64 "\n",
                    (uint64_t)run->hw.hardware_exit_reason);
            ret = -1;
            break;
        case KVM_EXIT_INTERNAL_ERROR:
            DPRINTF("internal_error\n");
            break;
        case KVM_EXIT_SYSTEM_EVENT:
            DPRINTF("system_event\n");
            break;
        default:
            DPRINTF("kvm_arch_handle_exit:%d\n",run->exit_reason);
            break;
        }
    }while (ret == 0);
    return ret;
}

static void *vcpu_thread_fn(void *arg)
{
    struct VCPUState *cpu = arg;
    vcpu_exec(cpu);
    destroy_vcpu(cpu);
}

static void setup_pagetable() { 
    *(uint64_t *)get_userspace_addr(PML4_START) = PDPTE_START | 0x03;
    *(uint64_t *)get_userspace_addr(PDPTE_START) = PDE_START | 0x03;
    for (int i=0;i<512;i++) {
        *(uint64_t *)get_userspace_addr(PDE_START + i * 8) = (i << 21) + 0x83;
    }
}

static void init_linux_boot() { 
    setup_pagetable();
    setup_mptable(VCPU_COUNT);
    setup_cmdline();
    setup_boot_params(VMLINUX_FILE, INITRD_FILE);
    setup_gdt();
    setup_idt();
}

static void create_base_dev()
{
    int ret;
    ret = ioctl(kvm_state->vmfd, KVM_CREATE_IRQCHIP);
    if (ret < 0) {
        fprintf(stderr, "create irqchip failed\n");
    }
    ret = ioctl(kvm_state->vmfd, KVM_SET_TSS_ADDR, 0xfffbc000+0x1000);
    if (ret < 0) {
        fprintf(stderr, "set tss addr failed\n");
    }
    struct kvm_pit_config config = {
        .flags = KVM_PIT_SPEAKER_DUMMY,
    };
    if (ioctl(kvm_state->vmfd, KVM_CHECK_EXTENSION, KVM_CAP_PIT2)) {
        ret = ioctl(kvm_state->vmfd, KVM_CREATE_PIT2, &config);
    } else {
        ret = ioctl(kvm_state->vmfd, KVM_CREATE_PIT);
    }
    if (ret < 0) {
        fprintf(stderr, "create pit failed\n");
    }
}

int main(int argc, char **argv) {
    int ret;
    kvm_state = malloc(sizeof(struct KVMState));
    struct VCPUState *vcpu = malloc(sizeof(struct VCPUState));
    pthread_t vcpu_thread;

    //open kvm device
    kvm_state->fd = open("/dev/kvm", O_RDWR);
    if (kvm_state->fd < 0) {
        fprintf(stderr, "Could not access KVM kernel module\n");
        return -1;
    }

    //check api version
    if (ioctl(kvm_state->fd, KVM_GET_API_VERSION, 0) != KVM_API_VERSION) {
        fprintf(stderr, "kvm version not supported\n");
        return -1;
    }

    //create vm
    do {
        ret = ioctl(kvm_state->fd, KVM_CREATE_VM, 0);
    } while (ret == -EINTR);
    if (ret < 0) {
        fprintf(stderr, "ioctl(KVM_CREATE_VM) failed: %d %s\n", -ret,
                strerror(-ret));
        return -1;
    }
    kvm_state->vmfd = ret;

    //Init kvm_based vm devices
    create_base_dev();

    //init ram
    init_memory_map(kvm_state->vmfd, RAM_SIZE);

    //init vcpu
    init_vcpu(vcpu);

    //create serial dev
    create_serial_dev(kvm_state->vmfd);

    //run bios
    init_linux_boot();

    //vcpu run
    setup_vcpu(kvm_state->fd, vcpu->vcpu_fd, VCPU_COUNT, VCPU_ID);
    reset_vcpu(kvm_state->fd, vcpu->vcpu_fd, VCPU_COUNT, VCPU_ID);
    if (pthread_create(&(vcpu_thread), (const pthread_attr_t *)NULL,
                        vcpu_thread_fn, vcpu) != 0) {
        fprintf(stderr, "can not create kvm cpu thread");
        exit(1);
    }
    pthread_join(vcpu_thread, NULL);

    //exit
    close(vcpu->vcpu_fd);
    close(kvm_state->vmfd);
    close(kvm_state->fd);
    free(vcpu);
    free(kvm_state);
}

