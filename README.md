microv介绍:  

一个简易的KVM虚拟化hypervisor(目前支持X86)。  
实现了一个能够引导linux操作系统的linux boot规范，或者说实现了无bios引导linux系统。  
实现了KVM虚拟化的最基础CPU、内存、IO陷入陷出处理。  
实现了一个串口设备，用以和操作系统交互。  

作用:

理解bios如何引导linux系统；理解X86的实模式和保护模式；理解KVM虚拟化的基本原理。  

参考:

firecracker-microvm stratovirt

使用:

1.准备内核和initrd：  
vmlinux.bin,可以从下面下载：  
aws:https://s3.amazonaws.com/spec.ccfc.min/img/quickstart_guide/x86_64/kernels/vmlinux.bin  
euler:https://repo.openeuler.org/openEuler-21.03/stratovirt_img/x86_64/vmlinux.bin  

上面aws下载的vmlinux.bin是ELF格式的，需要objcopy -O binary vmlinux vmlinux.bin来转化成裸格式  

initrd的制作可以参考:  
https://gitee.com/openeuler/stratovirt/blob/master/docs/mk_initrd.md

最后vmlinux.bin和initrd.img放到/root/img/

2.编译：  
    cd microv; make

3.运行  
    ./microv
