/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <stdio.h>
#include <utils.h>
#include <device/map.h>
#include <fcntl.h>
#include <unistd.h>
#include "debug.h"
/* http://en.wikibooks.org/wiki/Serial_Programming/8250_UART_Programming */
// NOTE: this is compatible to 16550
#include <linux/serial_reg.h>


#define CH_OFFSET 0

static uint8_t *serial_base = NULL;
// static char* pts_fd = "/dev/pts/3";
// static bool rdi = 0;
#include <termios.h>
#include <sys/ioctl.h>

static int is_eofd;

static int ReadKBByte() {
  if (is_eofd) return 0xffffffff;
  char rxchar = 0;
  int rread = read(fileno(stdin), (char *)&rxchar, 1);

  if (rread > 0){  // Tricky: getchar can't be used with arrow keys.
    serial_base[UART_RX]=rxchar;
    return rxchar;
    }
  else
    return -1;
}

static int IsKBHit() {
  if (is_eofd) return -1;
  int byteswaiting;
  ioctl(0, FIONREAD, &byteswaiting);
  if (!byteswaiting && write(fileno(stdin), 0, 0) != 0) {
    is_eofd = 1;
    return -1;
  }  // Is end-of-file for
  return !!byteswaiting;
}

static void serial_putc(char ch) {
  printf("%c", ch);
  fflush(stdout);
}


static void serial_getc(char ch) {
    if( IsKBHit() ) ReadKBByte();
    // if(ch!='\n' && ch!=0 ){
    //   serial_base[UART_LSR]= UART_LSR_TEMT|UART_LSR_THRE |UART_LSR_DR;
    // }
}

static void ResetKeyboardInput() {
  // Re-enable echo, etc. on keyboard.
  struct termios term;
  tcgetattr(0, &term);
  term.c_lflag |= ICANON | ECHO;
  tcsetattr(0, TCSANOW, &term);
}

static void CaptureKeyboardInput() {
  // Hook exit, because we want to re-enable keyboard.
  atexit(ResetKeyboardInput);

  struct termios term;
  tcgetattr(0, &term);
  term.c_lflag &=  ~(ICANON | ECHO);  // Disable echo as well
  tcsetattr(0, TCSANOW, &term);
}



static void serial_io_handler(uint32_t offset, int len, bool is_write) {
  int target_val = serial_base[offset];
  // 重定向标准输出到文件output.txt
  // if( freopen(pts_fd, "w", stdout)==NULL ){assert(-1);};
  // // 重定向标准输入到文件input.txt
  // if( freopen(pts_fd, "r", stdin)==NULL ) {assert(-1);};

  switch (offset) {
    case CH_OFFSET:{
      if (is_write) { serial_putc(serial_base[0]);}
      else {          serial_getc(serial_base[0]);}
      break;
    }
    case UART_FCR: {
      if (is_write && (target_val &(UART_FCR_ENABLE_FIFO|UART_FCR_CLEAR_RCVR|UART_FCR_CLEAR_XMIT|UART_FCR_TRIGGER_4))!=0){
        fprintf(stderr,"FIFO init\n");
      }else if (is_write && (target_val &UART_FCR_ENABLE_FIFO)!=0) {
        fprintf(stderr,"Enable FIFO's\n");
      }else if(is_write && (target_val &UART_FCR_CLEAR_RCVR)!=0) {
        fprintf(stderr,"Clear receiver FIFO\n");
      }else if(is_write && (target_val &UART_FCR_CLEAR_XMIT)!=0) {
        fprintf(stderr,"Clear transmitter FIFO\n");
      }else if(is_write && (target_val &UART_FCR_TRIGGER_4)!=0) {
        fprintf(stderr,"Set FIFO trigger at 4-bytes\n");
      }
      else {
        // serial_base[offset]=0;
      }
      serial_base[offset]=0;//ban fc
      break;
    }
    case UART_LCR:{
      if(is_write) {
        fprintf(stderr,"write uart lcr val is %02x\n",serial_base[offset]);
        if(serial_base[offset]==UART_LCR_WLEN8) {
          fprintf(stderr,"uart set 8n1 mode\n");
          serial_base[offset]=0;
        }
      }
      else{
        fprintf(stderr,"read uart lcr val is %02x\n",serial_base[offset]);
      }
      break;
    }
    case UART_LSR:
    if (is_write) {
      fprintf(stderr,"write uart lsr val is %02x\n",serial_base[offset]);
    }else{
      fprintf(stderr,"read uart lsr val is %02x\n",serial_base[offset]);
      serial_base[offset]=(UART_LSR_TEMT | UART_LSR_THRE) | IsKBHit() ;// Line Status Register 
    }
    break;
    case UART_IER:{
      if (is_write && (target_val & UART_IER_RDI)!=0){
        fprintf(stderr,"Enable receiver data interrupt\n");
        // rdi=true;
      }else if (is_write && (target_val & UART_IER_THRI)!=0) {
        fprintf(stderr,"Enable Transmitter holding register int\n");
      }else if (is_write && (target_val & UART_IER_RLSI)!=0) {
        fprintf(stderr,"Enable receiver line status interrupt\n");
      }else if (is_write && (target_val & UART_IER_MSI)!=0) {
        fprintf(stderr,"Enable Modem status interrupt\n");
      }
      serial_base[UART_IER]=0;
      fprintf(stderr,"UART IER NULL\n");
      break;}
    case UART_MCR:{
      if(is_write ){
        fprintf(stderr,"uart MCR val is %02x !! null \n",target_val);
      }
      serial_base[UART_MCR]=0;
      break;
    }
    case UART_MSR:{
      serial_base[UART_MSR]=0;//UART_MSR_DCTS|UART_MSR_DSR|UART_MSR_DDSR|UART_MSR_DDCD;
      break;
    }
    // case UART_IIR:
    default:fprintf(stderr,"other offset %02x write is %d val is %d %c\n",offset,is_write,
                        serial_base[offset],serial_base[offset]!=12 ? serial_base[offset]:12);
                        serial_base[offset]=0;
                        break;
  }

}

void init_serial() {
  serial_base = new_space(8);
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map ("serial", CONFIG_SERIAL_PORT, serial_base, 8, serial_io_handler);
#else
  add_mmio_map("serial", CONFIG_SERIAL_MMIO, serial_base, 0x1000, serial_io_handler);
#endif
  serial_base[UART_LSR] = UART_LSR_TEMT|UART_LSR_THRE;
  CaptureKeyboardInput();

}
