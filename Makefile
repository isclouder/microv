TARGET = microv
OBJECT = main.o memory.o loader.o mptable.o bootparams.o gdt.o vcpu.o dev_serial.o string.o
CC = gcc
CXXFLAG = -Wno-int-to-pointer-cast
LDFLAG = -lpthread

$(TARGET):$(OBJECT)
	$(CC) $(CXXFLAG) $(LDFLAG) main.c memory.c loader.c mptable.c bootparams.c gdt.c vcpu.c dev_serial.c string.c -o $@

clean:
	rm *.o microv
