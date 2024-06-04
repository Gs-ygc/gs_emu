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

#include <device/map.h>
#include <stdint.h>
#include <utils.h>

#define MSIP       0x0000
#define MTIMECMPLO 0x4000
#define MTIMECMPHI 0x4004
#define MTIMELO    0xBFF8
#define MTIMEHI    0xBFFC


static uint32_t *clint_port_base = NULL;


static void clint_io_handler(uint32_t offset, int len, bool is_write) {
  // if (!is_write && (offset == MTIMELO|| offset == MTIMEHI )) {
  //   uint64_t us = get_time();
  //   *((uint64_t *)(&((uint8_t *)clint_port_base)[MTIMELO]))=us;
  //   // printf("%ld\n",us);
  //   *((uint64_t *)(&((uint8_t *)clint_port_base)[MTIMELO]))=us;
  // }
  // uint64_t us = get_time();
  // *((uint64_t *)(&((uint8_t *)clint_port_base)[MTIMELO])) =us/8;
}

void clint_update(){
  #ifdef CONFIG_DIFFTEST_REF_MINIRV32I
    uint64_t us = get_time();
    *((uint64_t *)(&((uint8_t *)clint_port_base)[MTIMELO])) =us;
  #else
    *((uint64_t *)(&((uint8_t *)clint_port_base)[MTIMELO])) +=1;
  #endif
    // printf("0x%16lx    0x%16lx     \n",*((uint64_t *)(&((uint8_t *)clint_port_base)[MTIMELO])),*((uint64_t *)(&((uint8_t *)clint_port_base)[MTIMECMPLO])));
}

void init_clint() {
  clint_port_base = (uint32_t *)new_space(8);
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map ("clint", CONFIG_clint_PORT, clint_port_base, 8, clint_io_handler);
#else
  add_mmio_map("clint", CONFIG_CLINT_MMIO, clint_port_base, 0x10000, clint_io_handler);
  *((uint64_t *)(&((uint8_t *)clint_port_base)[MTIMECMPLO]))=~0;
  *((uint64_t *)(&((uint8_t *)clint_port_base)[MTIMELO]))=0;
#endif
//   IFNDEF(CONFIG_TARGET_AM, add_alarm_handle(timer_intr));
}
