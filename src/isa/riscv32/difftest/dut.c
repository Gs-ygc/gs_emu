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
#include <cpu/difftest.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../local-include/reg.h"
#include "common.h"
#include "memory/paddr.h"

bool isa_difftest_checkregs(CPU_state *ref_r, vaddr_t pc) {
  int len=ARRLEN(ref_r->gpr);
  int error=0;
  
  // uint32_t time=paddr_read((CONFIG_CLINT_MMIO+MTIMELO),4);
  // printf("0x%08x ",time);

  for (int i=0; i<len; i++) {
    int32_t dert=ref_r->gpr[i]-cpu.gpr[i];
    bool tm=abs(dert)>0x100?true:false;
    if(ref_r->gpr[i]!=cpu.gpr[i]&&tm ){
      printf("pc: "FMT_WORD"    gpr %-4s    "FMT_WORD"    "FMT_WORD"\n",pc, reg_name(i), cpu.gpr[i], ref_r->gpr[i]);
      // printf("skip?\n");
      // int tmp;
      // tmp=getchar();
      // if(tmp!=1){continue;};
      pc=cpu.pc;
      error += 1;
    }
  }
  #ifdef CONFIG_DIFFTEST_REF_SPIKE
  for (int i=0; i<NR_CSR-1; i++) {
    if(ref_r->csr[i]!=cpu.csr[i]){
      printf("pc: "FMT_WORD"    csr %-4s    "FMT_WORD"    "FMT_WORD"\n",pc, csr_reg_name(i), cpu.csr[i], ref_r->csr[i]);
      pc=cpu.pc;
      error +=1 ;
    }
  }
  #else
  int csr_error=0;
  #define check_csr_reg(i) do{if(((ref_r->csr[i])&8)!=((cpu.csr[i])&8)) {\
                                printf("pc: "FMT_WORD"    csr %-4s    "FMT_WORD"    "FMT_WORD"\n",pc, csr_reg_name(i), cpu.csr[i], ref_r->csr[i]);\
                                pc=cpu.pc;csr_error +=1;}\
                            }while(0)
                            
  // check_csr_reg((check_csr_idx(0x341)));
  // check_csr_reg((check_csr_idx(0x342)));
  // check_csr_reg((check_csr_idx(0x304)));
  // check_csr_reg((check_csr_idx(0x344)));
  // check_csr_reg((check_csr_idx(0x340)));
  // check_csr_reg((check_csr_idx(0x300)));
  // check_csr_reg((check_csr_idx(0x343)));
  // check_csr_reg((check_csr_idx(0x305)));
  // check_csr_reg((check_csr_idx(0x300)));
  #undef check_csr_reg
  if(csr_error) return false;
  #endif

  if(cpu.pc!=ref_r->pc){
    printf("gsemu pc ="FMT_PADDR"\nspike pc ="FMT_PADDR"!!\n",cpu.pc,ref_r->pc);
    error += 1;
    if(error&& (cpu.pc!=ref_r->pc)+4){
      int skip=0;
      printf("是否跳过这条异常\n");
      int ret=scanf("%d",&skip);
      if(ret==1){
        switch (skip){
        case 0:return true;
        case 1:difftest_skip_dut(1,0);break;
        case 2:difftest_skip_ref_one();break;
        default:return false;
        }
      }
    }
    // return !error;
  }
  // return !(error_csr_error);
  // return true;
  return !error;
}

void isa_difftest_attach() {
}
