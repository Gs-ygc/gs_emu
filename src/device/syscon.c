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
#include <utils.h>
#include "difftest-def.h"

static uint32_t *syscon_base = NULL;

static void syscon_io_handler(uint32_t offset, int len, bool is_write) {
  assert(offset == 0 || offset == 4);
  if (is_write && offset == 0) {
    if(syscon_base[0]==0x7777)
        printf("reboot\n");
    else if (syscon_base[0]==0x5555)
        printf("shutdown\n");
  }
}


void init_syscon() {
  syscon_base = (uint32_t *)new_space(8);
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map ("syscon", CONFIG_syscon, syscon_base, 8, syscon_io_handler);
#else
  add_mmio_map("syscon", CONFIG_SYSCON_MMIO, syscon_base, 0x1000, syscon_io_handler);
#endif

}
