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

#include <isa.h>
#include "local-include/reg.h"
#include "common.h"
#include "local-include/csr.h"

const char *regs[] = {
  "$0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
  "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
  "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
  "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

const word_t csr_idx_map[]={ CSR_IDX_MAP_DEFINE_LIST };

const char *csregs[] = {
  "mvendorid","mhartid","mstatus", "mip","mie","mcause","mtvec",
  "mtval","mepc","mscratch", "misa","marchid","mimpid","pmpaddr0","pmpaddr1",
  "pmpcfg0","tselect","tdata1","tdata2","tdata3","tinfo", "dcsr",
  "dpc","dscratch0",
  #ifdef CONFIG_RVPrivMSU
  "medeleg","mideleg","mcounteren","sstatus","sie","stvec","scounteren",\
  "senvcfg","sscratch","sepc",\
  "scause","stval","sip","satp",
  #endif
  "Nan",
};

void isa_reg_display() {
  int len=ARRLEN(regs);

  printf("%-4s    "FMT_WORD"    %010d\n", "pc", cpu.pc, cpu.pc);

  for (int i=0; i<len; i++) {
    printf("%-4s    "FMT_WORD"    %010d\n", reg_name(i), cpu.gpr[i], cpu.gpr[i]);
  }

  len = ARRLEN(csregs)-1;

  for (int i=0; i<len; i++) {
    printf("%-10s    "FMT_WORD"    %012d\n", csregs[i], cpu.csr[i], cpu.csr[i]);
  }
}

word_t isa_reg_str2val(const char *s, bool *success) {
  int len=ARRLEN(regs);
  unsigned int i=0;

  if (!strcmp("pc",s)) {
    *success=true;
    return cpu.pc;
  }
  for (; i<len && !!strcmp(reg_name(i++),s);) {;}
  if(i>=len){
    *success=false;
    return -1;
  }else{
    *success=true;
    return cpu.gpr[i-1];
  }
}