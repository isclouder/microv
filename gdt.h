#ifndef MICROV_GDT_H
#define MICROV_GDT_H
#include <linux/kvm.h>

void setup_gdt();
void setup_idt();
struct kvm_segment get_code_kvm_seg();
struct kvm_segment get_data_kvm_seg();
uint16_t get_gdt_limit();
uint16_t get_idt_limit();
void _test_gdt();

#endif /* MICROV_GDT_H */
