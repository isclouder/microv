#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mptable.h"
#include "mpspec_def.h"
#include "global.h"
#include "memory.h"
#include "string.h"

#define APIC_VERSION     0x14
#define MPC_SPEC         0x4

#define MP_IRQDIR_DEFAULT 0
#define MP_IRQDIR_HIGH    1
#define MP_IRQDIR_LOW     3

static const char MPC_OEM[]        = "STRATSTR";
static const char MPC_PRODUCT_ID[] = "1.1.1.1.1.1.";
static const char BUS_TYPE_ISA[]   = "ISAISA";

static int mptable_checksum(char *buf, int size)
{
    int i;
    unsigned char sum = 0;

    for (i = 0; i < size; i++) {
        sum += buf[i];
    }

    return sum;
}

void setup_mptable(int num_cpus)
{
    struct mpf_intel *mpf;
    struct mpc_table *table;
    struct mpc_cpu *cpu;
    struct mpc_bus *bus;
    struct mpc_ioapic *ioapic;
    struct mpc_intsrc *intsrc;
    struct mpc_lintsrc *lintsrc;
    const char mpc_signature[] = MPC_SIGNATURE;
    const char smp_magic_ident[] = "_MP_";
    unsigned char checksum = 0;
    int offset = 0;
    int ssize;
    int i;

    //1.Intel MP Floating Pointer Structure
    ssize = sizeof(struct mpf_intel);

    mpf = (struct mpf_intel *)get_userspace_addr(MPTABLE_START);
    memset(mpf, 0, ssize);
    memcpy(mpf->signature, smp_magic_ident, sizeof(smp_magic_ident) - 1);
    mpf->length = 1;
    mpf->specification = 4;
    mpf->physptr = MPTABLE_START + ssize;
    mpf->checksum -= mptable_checksum((char *) mpf, ssize);

    //2.Intel MP Config Table Header Structure
    offset += ssize;
    ssize = sizeof(struct mpc_table);

    table = (struct mpc_table *)get_userspace_addr(MPTABLE_START + offset);
    memset(table, 0, ssize);
    memcpy(table->signature, mpc_signature, sizeof(mpc_signature) - 1);
    table->spec = MPC_SPEC;
    memcpy(table->oem, MPC_OEM, sizeof(MPC_OEM) - 1);
    memcpy(table->productid, MPC_PRODUCT_ID, sizeof(MPC_PRODUCT_ID) - 1);
    table->lapic = APIC_DEFAULT_PHYS_BASE;

    //3.Intel MP cpu
    offset += ssize;
    ssize = sizeof(struct mpc_cpu);

    for (i = 0; i < num_cpus; i++) {
        cpu = (struct mpc_cpu *)get_userspace_addr(MPTABLE_START + offset);
        memset(cpu, 0, ssize);
        cpu->type = MP_PROCESSOR;
        cpu->apicid = i;
        cpu->apicver = APIC_VERSION;
        cpu->cpuflag = CPU_ENABLED;
        if (i == 0) {
            cpu->cpuflag |= CPU_BOOTPROCESSOR;
        }
        cpu->cpufeature = 0x600; // Intel CPU Family Number: 0x6
        cpu->featureflag = 0x201; // APIC & FPU
        cpu->reserved1 = 0;
        cpu->reserved2 = 0;
        checksum += mptable_checksum((char *) cpu, ssize);
        offset += ssize;
    }

    //4.Intel MP bus
    ssize = sizeof(struct mpc_bus);

    bus = (struct mpc_bus *)get_userspace_addr(MPTABLE_START + offset);
    memset(bus, 0, ssize);
    bus->type = MP_BUS;
    bus->busid = 0;
    memcpy(bus->bustype, BUS_TYPE_ISA, sizeof(BUS_TYPE_ISA) - 1);
    checksum += mptable_checksum((char *) bus, ssize);

    //5.Intel MP ioapic
    offset += ssize;
    ssize = sizeof(struct mpc_ioapic);

    ioapic = (struct mpc_ioapic *)get_userspace_addr(MPTABLE_START + offset);
    memset(ioapic, 0, ssize);
    ioapic->type = MP_IOAPIC;
    ioapic->apicid = num_cpus + 1;
    ioapic->apicver = APIC_VERSION;
    ioapic->flags = MPC_APIC_USABLE;
    ioapic->apicaddr = IO_APIC_DEFAULT_PHYS_BASE;
    checksum += mptable_checksum((char *) ioapic, ssize);

    //6.Intel MP io interrupt
    offset += ssize;
    ssize = sizeof(struct mpc_intsrc);

    for (i = 0; i < 16; i++) {
        intsrc = (struct mpc_intsrc *)get_userspace_addr(MPTABLE_START + offset);
        memset(intsrc, 0, ssize);
        intsrc->type = MP_INTSRC;
        intsrc->irqtype = mp_INT;
        intsrc->irqflag = MP_IRQDIR_DEFAULT;
        intsrc->srcbus = 0;
        intsrc->srcbusirq = i;
        intsrc->dstapic = num_cpus + 1;
        intsrc->dstirq = i;
        checksum += mptable_checksum((char *) intsrc, ssize);
        offset += ssize;
    }

    //7.Intel MP local ext interrupt
    ssize = sizeof(struct mpc_lintsrc);

    lintsrc = (struct mpc_lintsrc *)get_userspace_addr(MPTABLE_START + offset);
    memset(lintsrc, 0, ssize);
    lintsrc->type = MP_LINTSRC;
    lintsrc->irqtype = mp_ExtINT;
    lintsrc->irqflag = MP_IRQDIR_DEFAULT;
    lintsrc->srcbusid = 0;
    lintsrc->srcbusirq = 0;
    lintsrc->destapic = num_cpus + 1;
    lintsrc->destapiclint = 0;
    checksum += mptable_checksum((char *) lintsrc, ssize);

    //8.Intel MP local nmi interrupt
    offset += ssize;

    lintsrc = (struct mpc_lintsrc *)get_userspace_addr(MPTABLE_START + offset);
    memset(lintsrc, 0, ssize);
    lintsrc->type = MP_LINTSRC;
    lintsrc->irqtype = mp_NMI;
    lintsrc->irqflag = MP_IRQDIR_DEFAULT;
    lintsrc->srcbusid = 0;
    lintsrc->srcbusirq = 0;
    lintsrc->destapic = 0xFF;
    lintsrc->destapiclint = 1;
    checksum += mptable_checksum((char *) lintsrc, ssize);

    offset += ssize;
    ssize = sizeof(struct mpc_table);

    table->length = offset - sizeof(struct mpf_intel);
    checksum += mptable_checksum((char *) table, ssize);
    table->checksum -= checksum;
}

void _test_mptable()
{
    struct mpf_intel *mpf;
    mpf = (struct mpf_intel *)get_userspace_addr(MPTABLE_START);

    fprintf(stderr, "mpf->signature:");
    for(int i=0;i<4;i++){
        fprintf(stderr, "%c",mpf->signature[i]);
    }
    fprintf(stderr, "\n");
}
