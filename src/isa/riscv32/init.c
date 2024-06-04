/***************************************************************************************
 * Copyright (c) 2014-2022 Zihao Yu, Nanjing University
 *
 * NEMU is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 *PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 *KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 *NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * See the Mulan PSL v2 for more details.
 ***************************************************************************************/

#include <isa.h>
#include <memory/paddr.h>
#include "isa-def.h"
#include "local-include/cpu_bits.h"
#include "local-include/reg.h"

privilege cur_pri;
privilege have_RISCVException;
privilege RISCVException_tval;
// this is not consistent with uint8_t
// but it is ok since we do not access the array directly
static const uint32_t img[] = {
  0x0ce6d063,// 0x800ffffa

};

static inline void csr_init() {
  csr(CSR_MSTATUS) =0x00000000;//MSTATUS_MPP;// M-mode
  csr(CSR_MHARTID) =0x00000000;//CSR_MHARTID
  csr(CSR_MSTATUS) =0x00001800;
  #ifndef CONFIG_DIFFTEST_REF_MINIRV32I
  csr(CSR_MARCHID) =0x05;// CSR_MARCHID
  #else
  csr(CSR_MARCHID) =0x00;// CSR_MARCHID
  #endif
  
  csr(CSR_PMPADDR0)=0xffffffff;//配置内存权限
  csr(CSR_PMPCFG0) =0x0000001f;
  csr(ZERO_CSR)=0;
  // DEBUG CSR
  csr(CSR_TDATA1)  =0xf0000000;
  csr(CSR_TINFO)   =0x0000807c;
  csr(CSR_DCSR)   =0x40000000;

  #ifdef CONFIG_RVPrivM
  csr(CSR_MISA)    =0x40001101;
  #endif

  #ifdef CONFIG_RVPrivMU
  csr(CSR_MISA)    =0x40101101;
  csr(CSR_MEDELEG) =0x00000000;
  #endif

  #ifdef CONFIG_RVPrivMSU
  csr(CSR_MISA)    =0x40141101;
  csr(CSR_MEDELEG) =0x00000000;
  #endif

  cur_pri=PRV_M;
  
  have_RISCVException=RISCV_EXCP_NONE;

  RISCVException_tval=0;

  cpu.amw=NULL;
}

static void restart() {
  /* Set the initial program counter. */
  #ifndef CONFIG_DIFFTEST_REF_PATH
  cpu.pc = 0x80000000;
  #else
  // cpu.pc = PMEM_KERNEL;
  cpu.pc = RESET_VECTOR;
  #endif

  /* The zero register is always 0. */
  cpu.gpr[0] = 0;
  cpu.gpr[10] = 0;
  cpu.gpr[11] = PMEM_DTB;

  /* RV CSR init. */
  csr_init();
}

void init_isa() {
  /* Load built-in image. */
  memcpy(guest_to_host(RESET_VECTOR), img, sizeof(img));

  /* Initialize this virtual computer system. */
  restart();
}
