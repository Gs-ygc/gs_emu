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

#ifndef __RISCV_REG_H__
#define __RISCV_REG_H__

#include <common.h>
#include <stdio.h>
#include "macro.h"



static inline int check_reg_idx(int idx) {
  IFDEF(CONFIG_RT_CHECK, assert(idx >= 0 && idx < MUXDEF(CONFIG_RVE, 16, 32)));
  return idx;
}

// CSR REG
#define NR_CSR 1+22

#ifdef CONFIG_RVPrivMSU
#undef NR_CSR
#define NR_CSR 1+37
#endif

#define ZERO_CSR 63

#define ERROR_CSR_IDX NR_CSR
static inline int check_csr_idx(int idx) {
  if(idx==ZERO_CSR) return ZERO_CSR;

  extern const word_t csr_idx_map[];

  for (int i=0; i<NR_CSR; i++) {
    if(idx==csr_idx_map[i]) {return i;}
  }
  fprintf(stderr,"NRCSR ERROR : 0x%03x\n",idx);
    return NR_CSR;
}

#define gpr(idx) (cpu.gpr[check_reg_idx(idx)])
#define csr(idx) (cpu.csr[check_csr_idx(idx)])

static inline const char* reg_name(int idx) {
  extern const char* regs[];
  return regs[check_reg_idx(idx)];
}
static inline const char* csr_reg_name(int idx) {
  extern const char* csregs[];
  return csregs[idx];
}
#endif
