TARGET = microv
OBJECT = main.o memory.o mptable.o bootparams.o gdt.o vcpu.o serial.o string.o
CC = gcc
CXXFLAG = -Wno-int-to-pointer-cast
LDFLAG = -lpthread

PWD=$(shell pwd)
CONFIG = ${PWD}/config
OUT = ${PWD}/out

LINUX_VER = 5.19.9
LINUX_SRC_URL = https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-${LINUX_VER}.tar.xz
LINUX_SRC = $(OUT)/linux-${LINUX_VER}

BUSYBOX_VER = 1.34.1
BUSYBOX_SRC_URL = https://busybox.net/downloads/busybox-${BUSYBOX_VER}.tar.bz2
BUSYBOX_SRC = $(OUT)/busybox-${BUSYBOX_VER}

$(TARGET):$(OBJECT)
	$(CC) $(CXXFLAG) main.c memory.c mptable.c bootparams.c gdt.c vcpu.c serial.c string.c -o $@ $(LDFLAG)

vmlinux.bin:
	mkdir -p ${OUT}
	wget -O ${OUT}/$(shell basename ${LINUX_SRC_URL}) --show-progress ${LINUX_SRC_URL}
	tar -xf ${OUT}/$(shell basename ${LINUX_SRC_URL}) -C ${OUT}
	cp -f ${CONFIG}/kernel.config ${LINUX_SRC}/.config
	cd ${LINUX_SRC} ; $(MAKE) ARCH=x86 bzImage
	objcopy -O binary ${LINUX_SRC}/vmlinux $(OUT)/$@

initrd.img:
	mkdir -p ${OUT}
	wget -O ${OUT}/$(shell basename ${BUSYBOX_SRC_URL}) --show-progress ${BUSYBOX_SRC_URL}
	tar -jxf ${OUT}/$(shell basename ${BUSYBOX_SRC_URL}) -C ${OUT}
	cp -f ${CONFIG}/busybox.config ${BUSYBOX_SRC}/.config
	cd ${BUSYBOX_SRC} ; $(MAKE) install
	cd ${BUSYBOX_SRC}/_install ; find . | cpio -o --format=newc > $(OUT)/$@

all:$(TARGET) vmlinux.bin initrd.img

clean:
	rm -rf *.o out microv

