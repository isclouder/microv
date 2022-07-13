/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MEMORY_H
#define _MEMORY_H

#include <inttypes.h>

int init_memory_map(int vmfd, uint64_t ram_size);
uint64_t get_gap_start();
uint64_t get_gap_end();
uint64_t get_ram_end();
void write_userspace_memory(void *src, uint64_t guest_addr, uint64_t len);
uint64_t get_userspace_addr(uint64_t guest_addr);

#endif /* _MEMORY_H */
