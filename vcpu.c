#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/kvm.h>
#include <sys/ioctl.h>

#include "global.h"
#include "memory.h"
#include "gdt.h"
#define KVM_MAX_CPUID_ENTRIES 80

#define X86_FEATURE_HYPERVISOR		31
#define X86_FEATURE_TSC_DEADLINE_TIMER	24

#define ECX_EPB_SHIFT 3

//see kernel arch/x86/include/asm/apicdef.h
#define	APIC_LVT0		0x350
#define	APIC_LVT1		0x360
#define	APIC_MODE_NMI		0x4
#define	APIC_MODE_EXTINT	0x7

//see kernel arch/x86/include/uapi/asm/processor-flags.h
#define X86_CR0_PE	0x1
#define X86_CR0_PG	0x80000000

#define MSR_EFER_LME   (1 << 8)
#define MSR_EFER_LMA   (1 << 10)

#define X86_CR4_PAE	0x20
#define MXCSR_DEFAULT	0x1f80

//see kernel arch/x86/include/asm/msr-index.h
#define MSR_IA32_TSC		0x0010
#define MSR_IA32_SYSENTER_CS	0x0174
#define MSR_IA32_SYSENTER_ESP	0x0175
#define MSR_IA32_SYSENTER_EIP	0x0176
#define MSR_IA32_MISC_ENABLE	0x01a0
#define MSR_STAR		0xc0000081
#define MSR_LSTAR		0xc0000082
#define MSR_CSTAR		0xc0000083
#define MSR_SYSCALL_MASK	0xc0000084
#define MSR_KERNELGSBASE	0xc0000102

#define SET_APIC_DELIVERY_MODE(x, y)	(((x) & ~0x700) | ((y) << 8))

struct kvm_lapic_state kapic;
struct kvm_mp_state mp_state;
struct kvm_regs regs;
struct kvm_sregs sregs;
struct kvm_fpu fpu;
struct {
    struct kvm_msrs info;
    struct kvm_msr_entry entries[100];
} msr_data;

struct kvm_msr_entry *msrs = msr_data.entries;

static void host_cpuid(uint32_t function, uint32_t count,
                       uint32_t *eax, uint32_t *ebx,
                       uint32_t *ecx, uint32_t *edx)
{
    uint32_t vec[4];

    asm volatile("cpuid"
                 : "=a"(vec[0]), "=b"(vec[1]),
                   "=c"(vec[2]), "=d"(vec[3])
                 : "0"(function), "c"(count) : "cc");

    if (eax)
        *eax = vec[0];
    if (ebx)
        *ebx = vec[1];
    if (ecx)
        *ecx = vec[2];
    if (edx)
        *edx = vec[3];
}

static void setup_cpuid(int kvm_fd, int vcpu_fd, int vcpu_count, int vcpu_id)
{
    int ret, size;
    struct kvm_cpuid2 *cpuid;
    uint32_t max = KVM_MAX_CPUID_ENTRIES;
    uint32_t apic_id = vcpu_id;

    size = sizeof(*cpuid) + max * sizeof(*cpuid->entries);
    cpuid = malloc(size);
    memset(cpuid, 0, size);
    cpuid->nent = max;
    ret = ioctl(kvm_fd, KVM_GET_SUPPORTED_CPUID, cpuid);
    if (ret < 0) {
        fprintf(stderr, "get kvm cpuid2 failed!\n");
    }
    if (ret == 0 && cpuid->nent >= max) {
        fprintf(stderr, "get kvm cpuid2 failed!\n");
    }
    
    for (int i = 0; i < cpuid->nent; i++) {
        struct kvm_cpuid_entry2 *entry = &(cpuid->entries[i]);
        switch (entry->function) {
        case 1:
            if(entry->index == 0) {
                        entry->ecx |= 1 << X86_FEATURE_HYPERVISOR;
                        entry->ecx |= 1 << X86_FEATURE_TSC_DEADLINE_TIMER;
            } 
            break;
        case 2:
            host_cpuid(
                        2,
                        0,
                        &entry->eax,
                        &entry->ebx,
                        &entry->ecx,
                        &entry->edx
                    );
            break;
        case 4:
            host_cpuid(
                        4,
                        entry->index,
                        &entry->eax,
                        &entry->ebx,
                        &entry->ecx,
                        &entry->edx
                    );
                    entry->eax &= ~0xfc000000;
                    if((entry->eax & 0x0001ffff) != 0 && vcpu_count > 1) {
                        entry->eax |= (vcpu_count - 1) << 26;
                    }
            break;
        case 6:
            entry->ecx &= ~(1 << ECX_EPB_SHIFT);
            break;
        case 10:
            if(entry->eax != 0) {
                uint32_t version_id = entry->eax & 0xff;
                uint32_t num_counters = entry->eax & 0xff00;
                if(version_id != 2 || num_counters == 0) {
                    entry->eax = 0;
                }
            }
            break;
        case 0xb:
            entry->edx = apic_id;
            entry->ecx = entry->index & 0xff;
            switch(entry->index) {
            case 0:
                entry->eax = 0;
                entry->ebx = 1;
                entry->ecx |= 1 << 8;
                break;
            case 1:
                entry->eax = 32 - vcpu_count;
                entry->ebx = vcpu_count;
                entry->ecx |= 2 << 8;
                break;
            default:
                entry->ebx = 0xff;
            }
            entry->ebx &= 0xffff;
            break;
        case 0x80000002:
        case 0x80000003:
        case 0x80000004:
            host_cpuid(
                        entry->function,
                        entry->index,
                        &entry->eax,
                        &entry->ebx,
                        &entry->ecx,
                        &entry->edx
                    );
            break;
        }
    }

    ret = ioctl(vcpu_fd, KVM_SET_CPUID2, cpuid);
    if (ret < 0) {
        fprintf(stderr, "set kvm cpuid2 failed\n");
    }
}

//see https://wiki.osdev.org/APIC
static void setup_lapic(int vcpu_fd)
{
    int ret = 0;
    char r;

    ret = ioctl(vcpu_fd, KVM_GET_LAPIC, &kapic);
    if (ret < 0) {
        fprintf(stderr, "get lapic failed\n");
    }

    r = SET_APIC_DELIVERY_MODE(kapic.regs[APIC_LVT0], APIC_MODE_EXTINT);
    kapic.regs[APIC_LVT0] = r;
    r = SET_APIC_DELIVERY_MODE(kapic.regs[APIC_LVT1], APIC_MODE_NMI);
    kapic.regs[APIC_LVT1] = r;
}

static void setup_mpstate(int vcpu_fd, int vcpu_id)
{
    if(vcpu_id == 0) {
        mp_state.mp_state = KVM_MP_STATE_RUNNABLE;
    } else {
        mp_state.mp_state = KVM_MP_STATE_UNINITIALIZED;
    }
}

static void setup_regs(int vcpu_fd)
{
    memset(&regs, 0, sizeof regs);
    regs.rflags = 0x0002;
    regs.rip = VMLINUX_START;
    regs.rsp = BOOT_LOADER_SP;
    regs.rbp = BOOT_LOADER_SP;
    regs.rsi = ZERO_PAGE_START;
}

//see kernel arch/x86/include/uapi/asm/processor-flags.h
static void setup_sregs(int vcpu_fd)
{
    int ret = 0;
    struct kvm_segment code_segment;
    struct kvm_segment data_segment;

    ret = ioctl(vcpu_fd, KVM_GET_SREGS, &sregs);
    if (ret < 0) {
        fprintf(stderr, "get sregs failed\n");
    }

    code_segment = get_code_kvm_seg();
    data_segment = get_data_kvm_seg();
    sregs.cs = code_segment;
    sregs.ds = data_segment;
    sregs.es = data_segment;
    sregs.fs = data_segment;
    sregs.gs = data_segment;
    sregs.ss = data_segment;

    // Init gdt table, gdt table has loaded to Guest Memory Space
    sregs.gdt.base = BOOT_GDT_START;
    sregs.gdt.limit = get_gdt_limit();

    // Init idt table, idt table has loaded to Guest Memory Space
    sregs.idt.base = BOOT_IDT_START;
    sregs.idt.limit = get_idt_limit();

    // Open 64-bit protected mode, include
    // Protection enable, Long mode enable, Long mode active
    sregs.cr0 |= X86_CR0_PE;
    sregs.efer |= (MSR_EFER_LME | MSR_EFER_LMA);

    // Setup page table
    sregs.cr3 = PML4_START;
    sregs.cr4 |= X86_CR4_PAE;
    sregs.cr0 |= X86_CR0_PG;
}

static void setup_fpu(int vcpu_fd)
{
    memset(&fpu, 0, sizeof fpu);
    fpu.fcw = 0x37f;
    fpu.mxcsr = MXCSR_DEFAULT;
}

static void setup_msr_entry(struct kvm_msr_entry *entry,
                              uint32_t index, uint64_t value)
{
    entry->index = index;
    entry->data = value;
}

static void setup_msr(int vcpu_fd)
{
    int n = 0;

    setup_msr_entry(&msrs[n++], MSR_IA32_SYSENTER_CS, 0);
    setup_msr_entry(&msrs[n++], MSR_IA32_SYSENTER_ESP, 0);
    setup_msr_entry(&msrs[n++], MSR_IA32_SYSENTER_EIP, 0);

    setup_msr_entry(&msrs[n++], MSR_STAR, 0);
    setup_msr_entry(&msrs[n++], MSR_LSTAR, 0);
    setup_msr_entry(&msrs[n++], MSR_CSTAR, 0);

    setup_msr_entry(&msrs[n++], MSR_SYSCALL_MASK, 0);
    setup_msr_entry(&msrs[n++], MSR_KERNELGSBASE, 0);
    setup_msr_entry(&msrs[n++], MSR_IA32_TSC, 0);
    setup_msr_entry(&msrs[n++], MSR_IA32_MISC_ENABLE, 1);

    msr_data.info.nmsrs = n;
}

void setup_vcpu(int kvm_fd, int vcpu_fd, int vcpu_count, int vcpu_id)
{
    setup_lapic(vcpu_fd);
    setup_mpstate(vcpu_fd, vcpu_id);
    setup_sregs(vcpu_fd);
    setup_regs(vcpu_fd);
    setup_fpu(vcpu_fd);
    setup_msr(vcpu_fd);
}

void reset_vcpu(int kvm_fd, int vcpu_fd, int vcpu_count, int vcpu_id)
{
    int ret = 0;

    setup_cpuid(kvm_fd, vcpu_fd, vcpu_count, vcpu_id);

    ret = ioctl(vcpu_fd, KVM_SET_LAPIC, &kapic);
    if (ret < 0) {
        fprintf(stderr, "set lapic failed\n");
    }

    ret = ioctl(vcpu_fd, KVM_SET_MP_STATE, &mp_state);
    if (ret < 0) {
        fprintf(stderr, "set mp state failed\n");
    }

    ret = ioctl(vcpu_fd, KVM_SET_REGS, &regs);
    if (ret < 0) {
        fprintf(stderr, "set regs failed\n");
    }

    ret = ioctl(vcpu_fd, KVM_SET_SREGS, &sregs);
    if (ret < 0) {
        fprintf(stderr, "set sregs failed\n");
    }

    ret = ioctl(vcpu_fd, KVM_SET_FPU, &fpu);
    if (ret < 0) {
        fprintf(stderr, "set fpu failed\n");
    }

    ret = ioctl(vcpu_fd, KVM_SET_MSRS, &msr_data);
    if (ret < 0) {
        fprintf(stderr, "set msrs failed\n");
    }
}
