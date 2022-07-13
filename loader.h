/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LOADER_H
#define _LOADER_H

#include <stdint.h>

void load_kernel(const char *path);
void load_initrd(const char *path);

#endif /* _LOADER_H */
