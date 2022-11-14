TARGET = microv
OBJECT = main.o memory.o mptable.o bootparams.o gdt.o vcpu.o serial.o string.o
CC = gcc
CXXFLAG = -Wno-int-to-pointer-cast
LDFLAG = -lpthread

$(TARGET):$(OBJECT)
	$(CC) $(CXXFLAG) $(LDFLAG) main.c memory.c mptable.c bootparams.c gdt.c vcpu.c serial.c string.c -o $@

clean:
	rm *.o microv
