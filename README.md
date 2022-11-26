# microv介绍:  

一个简易的KVM虚拟化hypervisor(目前支持X86)。  
实现了一个能够引导linux操作系统的linux boot规范，或者说实现了无bios引导linux系统。  
实现了KVM虚拟化的最基础CPU、内存、IO陷入陷出处理。  
实现了一个串口设备，用以和操作系统交互。  

# 作用:

用以KVM虚拟化和计算机体系结构的学习例子;  
理解bios如何引导linux系统；理解X86的实模式和保护模式；理解KVM虚拟化的基本原理。  

# 参考:

firecracker-microvm stratovirt

# 编译：  

```shell
    编译代码、内核、initrd：
    cd microv; make all  
    只编译代码：
    cd microv; make
```

# 运行:  

```shell
    ./microv -k ./out/vmlinux.bin -i ./out/initrd.img
```

## END.如有交流请联系作者

email:isclouder@163.com  
微信:kvmvirt  
