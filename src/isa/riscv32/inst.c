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

#include "common.h"
#include "difftest-def.h"
#include "isa-def.h"
#include "isa.h"
#include "local-include/cpu_bits.h"
#include "local-include/reg.h"
#include <cpu/cpu.h>
#include <cpu/ifetch.h>
#include <cpu/decode.h>
#include <unistd.h>
#include "utils.h"


extern privilege cur_pri;
extern privilege have_RISCVException;
extern privilege RISCVException_tval;
#define R(i) gpr(i)

#ifndef CONFIG_RVPrivM
#define isMonly 0
#else
#define isMonly 1
#endif

#define CSR_RW_CHECK(i) if(check_csr_idx(i)==ERROR_CSR_IDX) {trap_peek_exception(RISCV_EXCP_ILLEGAL_INST,s->isa.inst.val);}else
#define CSR_W_MSTATUS_CHECK(i) if(isMonly&&(i==CSR_MSTATUS)) {/*fprintf(stderr,"is Monly\n")*/;}else

#define trap_peek_exception(_x1,_x2) do{\
                                        have_RISCVException=_x1;RISCVException_tval=_x2;\
                                      }while(0)
#define recy_exception() trap_peek_exception(RISCV_EXCP_NONE,0)
#ifdef CONFIG_RVMemMisalignAccess
#define MEM_ALL_ACCESS 1
#else
#define MEM_ALL_ACCESS 0
#endif

#define Mr(addr,len)  ({                                \
                          word_t ret=0;                     \
                          if( MEM_ALL_ACCESS||(((addr)%len)==0) )                 \
                            ret=vaddr_read((addr),len);             \
                          else{ \
                            trap_peek_exception(RISCV_EXCP_LOAD_ADDR_MIS,(addr));\
                          }\
                          ret;                                  \
})

#define Mw(addr,len,data) do{                            \
                          if( MEM_ALL_ACCESS || (((addr)%len)==0) )                 \
                            vaddr_write((addr),(len),(data));             \
                          else{trap_peek_exception(RISCV_EXCP_STORE_AMO_ADDR_MIS,(addr));}\
}while(0)

#define dnpc_and_check(_x_target_addr) do{                                          \
                                        if(MUXDEF(CONFIG_RVC, 0, 1) && ((_x_target_addr)%4)!=0)                                    \
                                          trap_peek_exception(RISCV_EXCP_INST_ADDR_MIS,_x_target_addr);  \
                                        else                                                    \
                                          (s)->dnpc=_x_target_addr;                                \
                                    }while(0)

#define mstatus_mie csr(CSR_MSTATUS)&0x8
#define mstatus_mpie csr(CSR_MSTATUS)&0x80
#define mstatus_mpp BITS(csr(CSR_MSTATUS),12,11)
// 向量中断
#define have_Mvectored_interrupts csr(CSR_MTVEC) & 0x01
// 存在中断
#define have_IP ( ( csr( CSR_MIE) & ( csr(CSR_MIP) ) ) & 0x0aaa)
// 全局中断
#define enabled_Global_interrupts mstatus_mie

#define enable_machine_mode() csr(CSR_MSTATUS)=csr(CSR_MSTATUS)| MSTATUS_MPP

#define illegal_instruction(exception_causes,mtval_val)                                      \
  do {                                                             \
    csr(CSR_MSTATUS)=(csr(CSR_MSTATUS)&~MSTATUS_MPP)|(cur_pri<<11);                                         \
    csr(CSR_MCAUSE) = exception_causes;                                         \
    csr(CSR_MEPC) = s->pc;                                          \
    csr(CSR_MTVAL) = mtval_val;                              \
    csr(CSR_MSTATUS) = csr(CSR_MSTATUS) & ~0x80;                    \
    csr(CSR_MSTATUS) = (mstatus_mie)<<4 | csr(CSR_MSTATUS);         \
    csr(CSR_MSTATUS) = csr(CSR_MSTATUS) & ~0x8;                     \
    dnpc_and_check(csr(CSR_MTVEC));                                         \
    if( (exception_causes) < (RISCV_EXCP_U_ECALL) || (exception_causes) > (RISCV_EXCP_M_ECALL) )\
      fprintf(stderr,ANSI_FMT("Illegal instruction: case = %d ", ANSI_FG_RED) FMT_WORD " \n",exception_causes, s->isa.inst.val); \
    else {fprintf(stderr," cur_pri %d   ecall 0x%08x 0x%08x\n",cur_pri, R(17), exception_causes);\
      switch(R(17)){ \
        case 93:NEMUTRAP(s->pc, R(10));\
        case 0x53525354:NEMUTRAP(s->pc, R(10));\
      }\
    }\
    \
  } while (0)



word_t word_len = sizeof(word_t)*8;// TODO 全部替换成XLEN

enum {
  TYPE_I, TYPE_U, 
  TYPE_S, TYPE_J, 
  TYPE_B, TYPE_R,
  TYPE_CSR_II,TYPE_CSR_I,
  TYPE_N, // none
};

/* clang-format off */
#define src1R() do { *src1 = R(rs1); } while (0)
#define src2R() do { *src2 = R(rs2); } while (0)
#define immI() do { *imm = SEXT(BITS(i, 31, 20), 12); } while(0)
#define immU() do { *imm = SEXT(BITS(i, 31, 12), 20) << 12; } while(0)
#define immS() do { *imm = (SEXT(BITS(i, 31, 25), 7) << 5) | BITS(i, 11, 7); } while(0)
#define immB() do { *imm = SEXT( ((BITS(i,31,31) << 12 ) | (BITS(i,  7,  7) << 11) | (BITS(i, 30, 25) <<  5) | (BITS(i, 11,  8)<<1)), 13); } while(0)
/*BUG TODO FIX*/#define immJ() do { *imm = SEXT( ((BITS(i,31,31) << 20 )|(BITS(i, 19, 12) << 12)  | (BITS(i, 20, 20) << 11) | (BITS(i, 30, 25) << 5) | (BITS(i, 24, 21)<<1)), 21); } while(0)
/* clang-format on */ //0x081c 0x041c

static void decode_operand(Decode *s, int *rd, word_t *src1, word_t *src2, word_t *imm, int type) {
  uint32_t i = s->isa.inst.val;
  int rs1 = BITS(i, 19, 15);
  int rs2 = BITS(i, 24, 20);
  *rd     = BITS(i, 11, 7);
  switch (type) {
    case TYPE_I: src1R();          immI(); break;
    case TYPE_U:                   immU(); break;
    case TYPE_J:                   immJ(); break;
    case TYPE_S: src1R(); src2R(); immS(); break;
    case TYPE_B: src1R(); src2R(); immB(); break;
    case TYPE_R: src1R(); src2R();         break;
    case TYPE_CSR_I: src1R();          *imm = BITS(i, 31, 20); ; break;
    case TYPE_CSR_II: *src1 = rs1;     immI(); break;
  }
}

static inline word_t find_irq_type_val(word_t y) {
    word_t x = XLEN-12;
    word_t target=1<<(XLEN-1); // if is 32 , target = 0x8000 0000;
    y<<=x;
    while (y != 0) {
        if ( (y & target) == target) {
            if( ((31-x)%2) !=0 )return 31-x;
        }
        y <<= 1;
        x++;
    }
    return -1; 
}

static int irp_and(Decode *s) {
  // fprintf(stderr,"debug 0x%08x%08x 0x%08x%08x\n",(Mr(CONFIG_CLINT_MMIO+MTIMEHI,4)),(Mr(CONFIG_CLINT_MMIO+MTIMELO,4)),(Mr(CONFIG_CLINT_MMIO+MTIMECMPHI,4)),(Mr(CONFIG_CLINT_MMIO+MTIMECMPLO,4)));
  if( (Mr(CONFIG_CLINT_MMIO+MTIMEHI,4)) > (Mr(CONFIG_CLINT_MMIO+MTIMECMPHI,4)) || (Mr(CONFIG_CLINT_MMIO+MTIMELO,4)) > (Mr(CONFIG_CLINT_MMIO+MTIMECMPLO,4)) ) {
    // fprintf(stderr,"时钟中断");
    set_field(csr(CSR_MIP),MIP_MTIP,1);
  }else{
    set_field(csr(CSR_MIP),MIP_MTIP,0);
  }
  if( enabled_Global_interrupts ) {

    // fprintf(stderr,"debug info enabled_Global_interrupts = 1\n");

    word_t irq_type = find_irq_type_val(have_IP);

    if(csr(CSR_MIDELEG)!=0) {
      fprintf(stderr,"中断委托到s\n");
      if(cur_pri==PRV_S&&(csr(CSR_SSTATUS)&0x8)){
        fprintf(stderr,"S模式下S处理S中断\n");
        if((csr(CSR_SIE) & (csr(CSR_SIP))) & 0x0aaa){
          fprintf(stderr,"有sss中断\n");
          //TODO SSS
          return 1;
        }
        else {
          fprintf(stderr,"无sss中断\n");
        }
      }else if(csr(CSR_MIDELEG)==MIP_SSIP && irq_type==MIP_SSIP){
        fprintf(stderr,"成功委托SSIP\n");
      }else {
        fprintf(stderr,"无事发生\n");
      }
    }
    if( irq_type != -1){ 
      csr(CSR_MCAUSE)= irq_type | 0x1<<(XLEN-1); // set cause
      csr(CSR_MEPC)=s->pc;
      dnpc_and_check( csr(CSR_MTVEC) + (( 4 * irq_type-1)*(get_field(csr(CSR_MTVEC),0x01))) );
      set_field(csr(CSR_MSTATUS),MSTATUS_MPIE, get_field(csr(CSR_MSTATUS),MSTATUS_MIE));
      csr(CSR_MSTATUS)=csr(CSR_MSTATUS)&~MSTATUS_MIE;
      set_field(csr(CSR_MSTATUS),MSTATUS_MPP,cur_pri);
      // fprintf(stderr,"have a tarp %d  cur mpde %d"
      //               "   npc 0x%08x   status: 0x%08x\n"
      //                 ,irq_type,cur_pri,s->dnpc,csr(CSR_MSTATUS));
      cur_pri=PRV_M;
      return 1;
    }
  }
  return 0;
}

static int decode_exec(Decode *s) {
  int rd = 0;
  word_t src1 = 0, src2 = 0, imm = 0;
  dnpc_and_check(s->snpc);

#define INSTPAT_INST(s) ((s)->isa.inst.val)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ ) { \
  decode_operand(s, &rd, &src1, &src2, &imm, concat(TYPE_, type)); \
  __VA_ARGS__ ; \
}

  /* clang-format off */
  INSTPAT_START();
  // RV32I <><><><><>><><><>><><>><><><><><><><> 
  INSTPAT("??????? ????? ????? ??? ????? 01101 11", lui    , U, R(rd) = imm);
  INSTPAT("??????? ????? ????? ??? ????? 00101 11", auipc  , U, R(rd) = s->pc + imm;);
  INSTPAT("??????? ????? ????? ??? ????? 11011 11", jal    , J,   word_t targe_addr= s->pc + imm;
                                                                      if (MUXDEF(CONFIG_RVC, 0, 1) && targe_addr%4!=0) 
                                                                        {trap_peek_exception(RISCV_EXCP_INST_ADDR_MIS,targe_addr);}
                                                                      else
                                                                        { R(rd) = s->pc + 4;s->dnpc = s->pc + imm;});
  INSTPAT("??????? ????? ????? 000 ????? 11001 11", jalr   , I, word_t targe_addr= ((src1 + imm)&~1);
                                                                      if(MUXDEF(CONFIG_RVC, 0, 1) && targe_addr%4!=0)
                                                                        {trap_peek_exception(RISCV_EXCP_INST_ADDR_MIS,targe_addr);R(rd)=0;}
                                                                      else
                                                                        {s->dnpc=targe_addr;R(rd)=s->pc+4;});
  // TYPE_B 
  INSTPAT("??????? ????? ????? 000 ????? 11000 11", beq    , B,if (src1 == src2) dnpc_and_check(s->pc + SEXT(imm,13)));
  INSTPAT("??????? ????? ????? 001 ????? 11000 11", bne    , B, if ((sword_t)src1 != (sword_t)src2) dnpc_and_check(s->pc + SEXT(imm,13)));
  INSTPAT("??????? ????? ????? 100 ????? 11000 11", blt    , B, if ((sword_t)src1 < (sword_t)src2) dnpc_and_check(s->pc + SEXT(imm,13)));

  INSTPAT("??????? ????? ????? 101 ????? 11000 11", bge    , B, if ((sword_t)src1 >= (sword_t)src2) {dnpc_and_check(s->pc + SEXT(imm,13));});
  INSTPAT("??????? ????? ????? 110 ????? 11000 11", bltu   , B, if (src1 < src2) dnpc_and_check(s->pc + SEXT(imm,13)));
  INSTPAT("??????? ????? ????? 111 ????? 11000 11", bgeu   , B, if (src1 >= src2) dnpc_and_check(s->pc + SEXT(imm,13)));
  // TYPE_I
  INSTPAT("??????? ????? ????? 000 ????? 00000 11", lb     , I, R(rd) = SEXT(Mr((src1 + imm), 1),1*8)); 
  INSTPAT("??????? ????? ????? 001 ????? 00000 11", lh     , I, R(rd) = SEXT(Mr((src1 + imm), 2),2*8););
  INSTPAT("??????? ????? ????? 010 ????? 00000 11", lw     , I, R(rd) = SEXT(Mr((src1 + imm), 4),4*8));
  INSTPAT("??????? ????? ????? 100 ????? 00000 11", lbu    , I, R(rd) = Mr(src1 + imm, 1));
  INSTPAT("??????? ????? ????? 101 ????? 00000 11", lhu    , I, R(rd) = Mr(src1 + imm, 2););
  // TYPE_S
  INSTPAT("??????? ????? ????? 000 ????? 01000 11", sb     , S, Mw((src1 + imm), 1, src2));
  INSTPAT("??????? ????? ????? 001 ????? 01000 11", sh     , S, Mw((src1 + imm), 2, src2));
  INSTPAT("??????? ????? ????? 010 ????? 01000 11", sw     , S, Mw((src1 + imm), 4, src2));
  
  INSTPAT("??????? ????? ????? 000 ????? 00100 11", addi   , I, R(rd) = (sword_t)src1+(sword_t)imm);
  INSTPAT("??????? ????? ????? 010 ????? 00100 11", slti   , I, R(rd) = ((sword_t)src1< (sword_t)imm));
  INSTPAT("??????? ????? ????? 011 ????? 00100 11", sltiu  , I, R(rd) = ((word_t) src1< (word_t)imm));
  INSTPAT("??????? ????? ????? 100 ????? 00100 11", xori   , I, R(rd) = src1^imm);
  INSTPAT("??????? ????? ????? 110 ????? 00100 11", ori    , I, R(rd) = src1|imm);
  INSTPAT("??????? ????? ????? 111 ????? 00100 11", andi   , I, R(rd) = src1&imm);

  INSTPAT("000000? ????? ????? 001 ????? 00100 11", slli   , I, if((imm&0x20)&&XLEN==32) {trap_peek_exception(RISCV_EXCP_ILLEGAL_INST,s->isa.inst.val);}
                                                                     else R(rd) = src1<<BITS(imm,6,0));
  INSTPAT("0000000 ????? ????? 101 ????? 00100 11", srli   , I, R(rd) = (word_t)src1>>BITS(imm,5,0));
  INSTPAT("0100000 ????? ????? 101 ????? 00100 11", srai   , I, R(rd) = (sword_t)src1>>BITS(imm,5,0));

  // TYPE_R
  INSTPAT("0000000 ????? ????? 000 ????? 01100 11", add    , R, R(rd) = (sword_t)src1+(sword_t)src2);
  INSTPAT("0100000 ????? ????? 000 ????? 01100 11", sub    , R, R(rd) = (sword_t)src1-(sword_t)src2);
  INSTPAT("0000000 ????? ????? 001 ????? 01100 11", sll    , R, R(rd) = src1<<BITS(src2,4,0));
  INSTPAT("0000000 ????? ????? 010 ????? 01100 11", slt    , R, R(rd) = (sword_t)src1<(sword_t)src2);
  INSTPAT("0000000 ????? ????? 011 ????? 01100 11", sltu   , R, R(rd) = src1<src2);
  INSTPAT("0000000 ????? ????? 100 ????? 01100 11", xor    , R, R(rd) = src1^src2);
  INSTPAT("0000000 ????? ????? 101 ????? 01100 11", srl    , R, R(rd) = (word_t)src1>>BITS(src2,4,0));
  INSTPAT("0100000 ????? ????? 101 ????? 01100 11", sra    , R, R(rd) = (sword_t)src1>>BITS(src2,4,0));
  INSTPAT("0000000 ????? ????? 110 ????? 01100 11", or     , R, R(rd) = src1|src2);
  INSTPAT("0000000 ????? ????? 111 ????? 01100 11", and    , R, R(rd) = src1&src2);

  INSTPAT("0000000 00000 00000 000 00000 11100 11", ecall  , N, trap_peek_exception((RISCV_EXCP_U_ECALL+cur_pri),s->pc);switch (R(17)) {
                                                                        case 8:{
                                                                          fprintf(stderr,"POWER DOWN");
                                                                          NEMUTRAP(s->pc, R(10));
                                                                          break;
                                                                        }default:break;}
                                                                        );

  INSTPAT("0000000 00001 00000 000 00000 11100 11", ebreak , N, fprintf(stderr,"have ebreak call\n");csr(CSR_MEPC)=s->pc;NEMUTRAP(s->pc, R(10))); // R(10) is $a0
  // RV32I <><><><><>><><><>><><>><><><><><><><>
  // RV32M <><><><><>><><><>><><>><><><><><><><>
  INSTPAT("0000001 ????? ????? 000 ????? 01100 11", mul      , R, R(rd) = (sword_t)src1*(sword_t)src2);
  INSTPAT("0000001 ????? ????? 001 ????? 01100 11", mulh     , R, R(rd) = (sword_t) (( ((int64_t)((sword_t)src1)) * ((int64_t)((sword_t)src2))) >> word_len) );
  INSTPAT("0000001 ????? ????? 010 ????? 01100 11", mulhsu   , R, R(rd) = (sword_t) (( ((int64_t)((sword_t)src1)) * ((uint64_t)((word_t)src2))) >> word_len) );
  INSTPAT("0000001 ????? ????? 011 ????? 01100 11", mulhu    , R, R(rd) = (word_t)  (( ((uint64_t)((word_t)src1)) * ((uint64_t)((word_t)src2))) >> word_len) );
  INSTPAT("0000001 ????? ????? 100 ????? 01100 11", div      , R, if((src1==1<<(XLEN-1)) && (sword_t)src2==-1){R(rd)=src1;}
                                                                      else if(src2!=0){R(rd) = (sword_t)src1/(sword_t)src2;}
                                                                      else R(rd)=~0;);
  INSTPAT("0000001 ????? ????? 101 ????? 01100 11", divu     , R, if(src2!=0){R(rd) = src1/src2;}
                                                                      else R(rd)=~0;);
  INSTPAT("0000001 ????? ????? 110 ????? 01100 11", rem      , R, if((src1==1<<(XLEN-1)) && (sword_t)src2==-1){R(rd)=0;}
                                                                      else if(src2!=0){R(rd) = (sword_t)src1%(sword_t)src2;}
                                                                      else R(rd)=src1;);
  INSTPAT("0000001 ????? ????? 111 ????? 01100 11", remu     , R, if(src2!=0){R(rd) = src1%src2;}
                                                                      else R(rd)=src1;);

  #ifdef CONFIG_RVA
  // RV32A
  INSTPAT("00010 00 00000 ????? 010 ????? 01011 11", lr.w    , R, R(rd) = Mr(src1,4),insertNode(&cpu.amw, src1, true););
  INSTPAT("00011 ?? ????? ????? 010 ????? 01011 11", sc.w    , R, atomic_mem_word* node=searchNode(cpu.amw,src1);if (node != NULL&&node->reserved) {
                                                                                                                                    Mw(src1,4,src2);
                                                                                                                                    R(rd) = 0;
                                                                                                                                    deleteNode(&cpu.amw,src1);
                                                                                                                                  }else R(rd) = 1;);
  INSTPAT("00001 ?? ????? ????? 010 ????? 01011 11", amoswap.w    , R, word_t t=Mr(src1,4); Mw(src1,4,src2); R(rd) = t);
  INSTPAT("00000 ?? ????? ????? 010 ????? 01011 11", amoadd.w    , R, word_t t=Mr(src1,4); Mw(src1,4,t+src2); R(rd) = t);
  INSTPAT("00100 ?? ????? ????? 010 ????? 01011 11", amoxor.w    , R, word_t t=Mr(src1,4); Mw(src1,4,t^src2); R(rd) = t );
  INSTPAT("01100 ?? ????? ????? 010 ????? 01011 11", amoand.d    , R, word_t t=Mr(src1,4); Mw(src1,4,t&src2); R(rd) = t );
  INSTPAT("01000 ?? ????? ????? 010 ????? 01011 11", amoor.w    , R, word_t t=Mr(src1,4); Mw(src1,4,t|src2); R(rd) = t );
  INSTPAT("10000 ?? ????? ????? 010 ????? 01011 11", amomin.w    , R, word_t t=Mr(src1,4); Mw(src1,4,(sword_t)t<(sword_t)src2?t:src2); R(rd) = t );
  INSTPAT("10100 ?? ????? ????? 010 ????? 01011 11", amomax.w    , R, word_t t=Mr(src1,4); Mw(src1,4,(sword_t)t>(sword_t)src2?t:src2); R(rd) = t );
  INSTPAT("11000 ?? ????? ????? 010 ????? 01011 11", amominu.w    , R, word_t t=Mr(src1,4); Mw(src1,4,t<src2?t:src2); R(rd) = t );
  INSTPAT("11100 ?? ????? ????? 010 ????? 01011 11", amomaxu.w    , R, word_t t=Mr(src1,4); Mw(src1,4,t>src2?t:src2); R(rd) = t );
  #endif

  #ifdef CONFIG_RVZicsr
    //TYPE_I 
  INSTPAT("??????? ????? ????? 001 ????? 11100 11", csrrw   , CSR_I, CSR_RW_CHECK(imm){word_t t = csr(imm); CSR_W_MSTATUS_CHECK(imm) csr(imm) = src1; R(rd) = t;});
  INSTPAT("??????? ????? ????? 011 ????? 11100 11", csrrc   , CSR_I, CSR_RW_CHECK(imm){word_t t = csr(imm); CSR_W_MSTATUS_CHECK(imm) csr(imm) = t&~src1; R(rd) = t;});
  INSTPAT("??????? ????? ????? 010 ????? 11100 11", csrrs   , CSR_I, CSR_RW_CHECK(imm){word_t t = csr(imm); CSR_W_MSTATUS_CHECK(imm) csr(imm) = t | src1; R(rd) = t;});
  INSTPAT("??????? ????? ????? 101 ????? 11100 11", csrrwi  , CSR_II, CSR_RW_CHECK(imm){R(rd) = csr(imm);CSR_W_MSTATUS_CHECK(imm) csr(imm)=src1;});
  INSTPAT("??????? ????? ????? 111 ????? 11100 11", csrrci  , CSR_II, CSR_RW_CHECK(imm){word_t t = csr(imm);CSR_W_MSTATUS_CHECK(imm) csr(imm) =t&~src1;R(rd) = t;});
  INSTPAT("??????? ????? ????? 110 ????? 11100 11", csrrsi  , CSR_II, CSR_RW_CHECK(imm){word_t t = csr(imm);CSR_W_MSTATUS_CHECK(imm) csr(imm) =t|src1;R(rd) = t;});
  //TYPE 特权指令 
  INSTPAT("0001000 00010 00000 000 00000 11100 11", sret     , N, s->dnpc=csr(CSR_SEPC);
                                                                      csr(CSR_SSTATUS) |= (get_field(csr(CSR_SSTATUS),SSTATUS_SPIE)<<1);
                                                                      cur_pri=get_field(csr(CSR_SSTATUS),SSTATUS_SPP); 
                                                                      csr(CSR_SSTATUS)= (csr(CSR_SSTATUS) & (~ SSTATUS_SPP))|SSTATUS_SPIE;
                                                                      CSR_W_MSTATUS_CHECK(CSR_MSTATUS) csr(CSR_MSTATUS)= (csr(CSR_MSTATUS) & ~ MSTATUS_SPP)|SSTATUS_SPIE;);
  INSTPAT("0011000 00010 00000 000 00000 11100 11", mret     , R, s->dnpc=csr(CSR_MEPC); 
                                                                      set_field(csr(CSR_MSTATUS),MSTATUS_MIE,get_field(csr(CSR_MSTATUS),MSTATUS_MPIE));
                                                                      set_field(csr(CSR_MSTATUS),MSTATUS_MPIE,0x1);
                                                                      cur_pri=get_field(csr(CSR_MSTATUS),MSTATUS_MPP); 
                                                                      CSR_W_MSTATUS_CHECK(CSR_MSTATUS) set_field(csr(CSR_MSTATUS),MSTATUS_MPP,0x0));
  INSTPAT("0001000 00101 00000 000 00000 11100 11", wfi     , N, usleep(2000);;Mw(CONFIG_CLINT_MMIO+MTIMELO, 4, Mr(CONFIG_CLINT_MMIO+MTIMELO, 4)+2000););//RODO 由于时钟太慢，禁用wfi
  #endif

  #ifdef CONFIG_RVZifencei
  INSTPAT("0000??? ????? 00000 000 00000 00011 11", fence       , N, ;);//TODO fence
  INSTPAT("0000000 00000 00000 001 00000 00011 11", fence.i     , N, ;);
  INSTPAT("0001001 ????? ????? 000 00000 11100 11", sfence.vma  , N, ;);
  #endif

  //INV
  INSTPAT("0000000 00000 00000 000 00000 00000 00", inv     , N,    trap_peek_exception(RISCV_EXCP_ILLEGAL_INST,s->isa.inst.val););
  // INSTPAT("??????? ????? ????? ??? ????? ????? ??", inv    , N, INV(s->pc));
  INSTPAT("??????? ????? ????? ??? ????? ????? ??", inv    , N, trap_peek_exception(RISCV_EXCP_ILLEGAL_INST,s->isa.inst.val));
  INSTPAT_END();
  /* clang-format on */

  R(0) = 0; // reset $zero to 0
  csr(CSR_MISA)&=MUXDEF(CONFIG_RVC, 0x4, ~0x4);// 保持C拓展模式设置，避免异常；

  // csr(CSR_MCOUNTEREN)=0;//性能计数器
  // csr(CSR_SCOUNTEREN)=0;//性能计数器
  // 特权模式状态传播

  // 同步异常是由于特定的错误或非法操作在指令执行期间立即检测到的，所以指令执行时发生：
  #ifdef CONFIG_RVC
  if (have_RISCVException==RISCV_EXCP_LOAD_ADDR_MIS) {
      recy_exception();//C拓展支持不对齐访问
  #endif
  if (have_RISCVException!=RISCV_EXCP_NONE) {
    // fprintf(stderr,ANSI_FMT("tarp: ", ANSI_FG_RED)); 
    // fprintf(stderr,"cur mode is %d 0x%08x 0x%08x\n",cur_pri, have_RISCVException, RISCVException_tval);
    #ifdef CONFIG_RVPrivMSU
    //MSU模式支持异常委托到S
    if( cur_pri<=PRV_S && csr(CSR_MEDELEG)&(1<<have_RISCVException) ) { 
      csr(CSR_SCAUSE) = have_RISCVException; 
      csr(CSR_SEPC) = s->pc;           
      csr(CSR_STVAL) = RISCVException_tval;                              
      //保存SIE到SPIE
      set_field(csr(CSR_SSTATUS), SSTATUS_SPIE, get_field(csr(CSR_SSTATUS),SSTATUS_SIE));
      set_field(csr(CSR_MSTATUS), MSTATUS_SPIE, get_field(csr(CSR_MSTATUS),MSTATUS_SIE));  
      
      // 屏蔽中断 
      set_field(csr(CSR_SSTATUS), SSTATUS_SIE, 0);      
      dnpc_and_check(csr(CSR_STVEC));
      set_field(csr(CSR_SSTATUS), SSTATUS_SPP, cur_pri);
      cur_pri=PRV_S;
      set_field(csr(CSR_MSTATUS), MSTATUS_SPP, cur_pri);
      recy_exception();
      fprintf(stderr,ANSI_FMT("Triggers the software to handle a exception ", ANSI_FG_BLUE) " at pc = "FMT_PADDR" at next_pc = "FMT_PADDR"\n",s->pc,s->dnpc);
    }else {
      illegal_instruction(have_RISCVException,RISCVException_tval);
      recy_exception();
    }
    #else
    illegal_instruction(have_RISCVException,RISCVException_tval);
    recy_exception();
    #endif
  }
  return 0;
}

int isa_exec_once(Decode *s) {
  s->isa.inst.val = inst_fetch(&s->snpc, 4);
  if(irp_and(s)) return 0;
  return decode_exec(s);
}
