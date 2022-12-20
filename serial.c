/*********************************************************************************
rs232 Table of Registers
----------------------------------------------------------------------------------
Base Address	DLAB	Read/Write	Abr.	Register Name
----------------------------------------------------------------------------------
+ 0	        =0	Write	        -	Transmitter Holding Buffer
                =0	Read	        -	Receiver Buffer
                =1	Read/Write	-	Divisor Latch Low Byte
----------------------------------------------------------------------------------
+ 1	        =0	Read/Write	IER	Interrupt Enable Register
                =1	Read/Write	-	Divisor Latch High Byte
----------------------------------------------------------------------------------
+ 2	        -	Read	        IIR	Interrupt Identification Register
                -	Write	        FCR	FIFO Control Register
----------------------------------------------------------------------------------
+ 3	        -	Read/Write	LCR	Line Control Register
----------------------------------------------------------------------------------
+ 4	        -	Read/Write	MCR	Modem Control Register
----------------------------------------------------------------------------------
+ 5	        -	Read	        LSR	Line Status Register
----------------------------------------------------------------------------------
+ 6	        -	Read	        MSR	Modem Status Register
----------------------------------------------------------------------------------
+ 7	        -	Read/Write	-	Scratch Register
----------------------------------------------------------------------------------
*********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/kvm.h>
#include <sys/ioctl.h>
#include <sys/eventfd.h>

#include <unistd.h>
#include <termios.h>
#include <sys/epoll.h>
#include <pthread.h>

#include "global.h"
#include "iobus.h"
#include "serial.h"

#define MMIO_SERIAL_IRQ		4
#define RECEIVER_BUFF_SIZE	1024

#define UART_IER_RDI		0x01
#define UART_IER_THRI		0x02
#define UART_IIR_NO_INT		0x01
#define UART_IIR_THRI		0x02
#define UART_IIR_RDI		0x04
#define UART_IIR_ID		0x06

#define UART_LCR_DLAB		0x80
#define UART_LSR_DR		0x01
#define UART_LSR_OE		0x02
#define UART_LSR_BI		0x10
#define UART_LSR_THRE		0x20
#define UART_LSR_TEMT		0x40

#define UART_MCR_OUT2		0x08
#define UART_MCR_LOOP		0x10
#define UART_MSR_CTS		0x10
#define UART_MSR_DSR		0x20
#define UART_MSR_DCD		0x80

#define UART_FIFO_LENGTH    16      /* 16550A Fifo Length */

struct SerialFIFO {
    uint8_t data[UART_FIFO_LENGTH];
    uint8_t count;
    uint8_t itl;                        /* Interrupt Trigger Level */
    uint8_t tail;
    uint8_t head;
} typedef SerialFIFO;

struct Serial {
    uint8_t rbr;
    uint8_t thr;

    uint8_t ier;

    uint8_t iir;
    uint8_t fcr;

    uint8_t lcr;
    uint8_t mcr;
    uint8_t lsr;
    uint8_t msr;
    uint8_t scr;

    uint16_t div;
    uint32_t thr_pending;
    uint32_t interrupt_evt;
    SerialFIFO recv_fifo;
} Serial = {
    .ier = 0,
    .iir = UART_IIR_NO_INT,
    .lcr = 0x03,
    .mcr = UART_MCR_OUT2,
    .lsr = UART_LSR_TEMT | UART_LSR_THRE,
    .msr = UART_MSR_DCD | UART_MSR_DSR | UART_MSR_CTS,
    .scr = 0,
    .div = 0x0c,
    .thr_pending = 0,
};

extern struct bus pio_bus;
struct region io_region;

static void fifo_clear()
{
    SerialFIFO *f = &(Serial.recv_fifo);
    memset(f->data, 0, UART_FIFO_LENGTH);
    f->count = 0;
    f->head = 0;
    f->tail = 0;
}

static int fifo_put(uint8_t chr)
{
    SerialFIFO *f = &(Serial.recv_fifo);
    f->data[f->head++] = chr;
    if (f->head == UART_FIFO_LENGTH)
        f->head = 0;
    f->count++;

    return 1;
}

static uint8_t fifo_get()
{
    SerialFIFO *f = &(Serial.recv_fifo);
    uint8_t c;

    if(f->count == 0)
        return 0;

    c = f->data[f->tail++];
    if (f->tail == UART_FIFO_LENGTH)
        f->tail = 0;
    f->count--;

    return c;
}

static void update_serial_iir()
{
    uint8_t iir = UART_IIR_NO_INT;

    if((Serial.ier & UART_IER_RDI) != 0 && (Serial.lsr & UART_LSR_DR) != 0) {
        iir &= ~UART_IIR_NO_INT;
        iir |= UART_IIR_RDI;
    } else if((Serial.ier & UART_IER_THRI) != 0 && (Serial.thr_pending > 0)) {
        iir &= ~UART_IIR_NO_INT;
        iir |= UART_IIR_THRI;
    }

    Serial.iir = iir;

    if(iir != UART_IIR_NO_INT) {
        uint64_t u = 1;
        write(Serial.interrupt_evt, &u, sizeof(uint64_t));
    }
}

static void receive_serial_input(uint8_t data)
{
    if((Serial.mcr & UART_MCR_LOOP) == 0) {
        if(Serial.recv_fifo.count >= UART_FIFO_LENGTH) {
            fprintf(stderr, "Overflow UART_FIFO_LENGTH\n");
        }

        fifo_put(data);
        Serial.lsr |= UART_LSR_DR;

        update_serial_iir();
    }
}

static void *serial_thread_fn(void *arg)
{
    //set stdin raw mode
    int fd = STDIN_FILENO;
    struct termios t;
    if (tcgetattr(fd, &t) < 0){
        fprintf(stderr, "stdin tcgetattr failed\n");
    }
    t.c_lflag &= ~ICANON;
    t.c_lflag &= ~ECHO;
    if (tcsetattr(fd, TCSANOW, &t) < 0){
        fprintf(stderr, "stdin tcsetattr failed\n");
    }

    //epoll stdin
    int epoll_fd = epoll_create(1024);
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = fd;
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&event);
    struct epoll_event event_buf[16];
    uint8_t read_buf[1];
    while (1){
        int cnt = epoll_wait(epoll_fd,event_buf,16,-1);
        if (cnt<0){
            fprintf(stderr, "epoll failed\n");
        }else{
            for(int i=0;i<cnt;++i){
                int n = read(event_buf[i].data.fd,read_buf,1);
                read_buf[n]=0;
                receive_serial_input(read_buf[0]);
            }
        }
    }
}

static uint8_t read_serial_reg(uint64_t port)
{
    uint8_t ret;
    uint64_t reg = port;

    switch (reg){
    case 0:
        if((Serial.lcr & UART_LCR_DLAB) != 0) {
            ret = Serial.div;
        } else {
            if (Serial.recv_fifo.count != 0){
                ret = fifo_get();
            }
            if (Serial.recv_fifo.count == 0){
                Serial.lsr &= ~UART_LSR_DR;
            }
            update_serial_iir();
        }
        break;
    case 1:
        if((Serial.lcr & UART_LCR_DLAB) != 0) {
            ret = (Serial.div >> 8);
        } else {
            ret = Serial.ier;
        }
        break;
    case 2:
        ret = Serial.iir | 0xc0;
        Serial.thr_pending = 0;
        Serial.iir = UART_IIR_NO_INT;
        break;
    case 3:
        ret = Serial.lcr;
        break;
    case 4:
        ret = Serial.mcr;
        break;
    case 5:
        ret = Serial.lsr;
        break;
    case 6:
        if((Serial.mcr & UART_MCR_LOOP) != 0) {
            ret = (Serial.mcr & 0x0c) << 4;
            ret |= (Serial.mcr & 0x02) << 3;
            ret |= (Serial.mcr & 0x01) << 5;
        } else {
            ret = Serial.msr;
        }
        break;
    case 7:
        ret = Serial.scr;
        break;
    }
    return ret;
}

static void write_serial_reg(uint64_t port, uint8_t data)
{
    uint64_t reg = port;

    switch (reg){
    case 0:
        if((Serial.lcr & UART_LCR_DLAB) != 0) {
            Serial.div = (Serial.div & 0xff00) | (uint16_t)data;
        } else {
            Serial.thr_pending = 1;

            if((Serial.mcr & UART_MCR_LOOP) != 0){
                if(Serial.recv_fifo.count >= UART_FIFO_LENGTH) {
                    fprintf(stderr, "Overflow UART_FIFO_LENGTH\n");
                }

                fifo_put(data);
                Serial.lsr |= UART_LSR_DR;
            } else {
                fprintf(stderr, "%c",data);//output
            }

            update_serial_iir();
        }
        break;
    case 1:
        if((Serial.lcr & UART_LCR_DLAB) != 0) {
            Serial.div = (Serial.div & 0x00ff) | ((uint16_t)(data) << 8);
        } else {
            int changed = (Serial.ier ^ data) & 0x0f;
            Serial.ier = data & 0x0f;

            if(changed != 0) {
                update_serial_iir();
            }
        }
        break;
    case 3:
        Serial.lcr = data;
        break;
    case 4:
        Serial.mcr = data;
        break;
    case 7:
        Serial.scr = data;
        break;
    }

}

static void serial_handle_io(uint64_t port, uint8_t size, void *data, uint8_t is_write, void *owner)
{
    if(is_write) {
        write_serial_reg(port, *(uint8_t *)(data));
    } else {
        *(uint8_t *)(data) = read_serial_reg(port);
    }
}

void create_serial_dev(int vmfd)
{
    int ret;
    Serial.interrupt_evt = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    struct kvm_irqfd irqfd = {
        .fd = Serial.interrupt_evt,
        .gsi = MMIO_SERIAL_IRQ,
    };

    ret = ioctl(vmfd, KVM_IRQFD, &irqfd);
    if(ret<0){
        fprintf(stderr, "register serial irq fd failed\n");
    }

    region_init(&io_region, IO_SERIAL_START, IO_SERIAL_SIZE, NULL, serial_handle_io);
    iobus_register_region(&pio_bus, &io_region);

    pthread_t serial_thread;

    if (pthread_create(&(serial_thread), (const pthread_attr_t *)NULL,
                        serial_thread_fn, NULL) != 0) {
        fprintf(stderr, "can not create serial thread");
    }
}
