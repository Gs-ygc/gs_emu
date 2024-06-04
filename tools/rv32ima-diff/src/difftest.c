#include <common.h>
#include <macro.h>
#include <difftest-def.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/prctl.h>

#include "mini-rv32ima.h"

#include "../../../src/isa/riscv64/local-include/csr.h"
#define NR_CSR 1+22

#ifdef CONFIG_RVPrivMSU
#undef NR_CSR
#define NR_CSR 1+37
#endif

#define ZERO_CSR 63
#define ERROR_CSR_IDX NR_CSR

static const word_t csr_idx_map[]={ CSR_IDX_MAP_DEFINE_LIST };

static inline int check_csr_idx(int idx) {
  if(idx==ZERO_CSR) return ZERO_CSR;

  for (int i=0; i<NR_CSR; i++) {
    if(idx==csr_idx_map[i]) {return i;}
  }
  printf("NRCSR ERROR : 0x%03x\n",idx);
    return NR_CSR;
}

typedef struct Node {
    word_t mem_word;
    bool reserved;
    struct Node* next_node;
} atomic_mem_word;

struct minirv32_regs{
  uint32_t gpr[32];
  #ifdef CONFIG_RVZicsr
  uint32_t csr[64];
  #endif
  #ifdef CONFIG_RVA
  atomic_mem_word *amw;
  #endif
  uint32_t pc;
};

static struct MiniRV32IMAState *state=NULL;
static uint8_t *ram_base=NULL;
static int difftest_memcpy_call_count=0;

void init_run_config(int argc, char **argv);
void init_vm();
void si(int n);

void t_get_regs(void *st){
    struct minirv32_regs * tmp=(struct minirv32_regs *) st;
    memcpy(tmp->gpr,state->regs,DIFFTEST_REG_SIZE-1);
    tmp->pc=state->pc;
    // printf("0x%04x\n",csr_idx_map[idx]);
    tmp->csr[check_csr_idx(CSR_MEPC)]=state->mepc;
    // tmp->csr[check_csr_idx(CSR_MCYCLEH)]=state->cycleh;
    // tmp->csr[check_csr_idx(CSR_MCYCLE)]=state->cyclel;
    tmp->csr[check_csr_idx(CSR_MCAUSE)]=state->mcause;
    tmp->csr[check_csr_idx(CSR_MIE)]=state->mie;
    tmp->csr[check_csr_idx(CSR_MIP)]=state->mip;
    tmp->csr[check_csr_idx(CSR_MSCRATCH)]=state->mscratch;
    tmp->csr[check_csr_idx(CSR_MSTATUS)]=state->mstatus;
    tmp->csr[check_csr_idx(CSR_MTVAL)]=state->mtval;
    tmp->csr[check_csr_idx(CSR_MTVEC)]=state->mtvec;
    tmp->csr[check_csr_idx(CSR_MVENDORID)]=0xff0ff0ff;
    tmp->csr[check_csr_idx(CSR_MISA)]=0x40401101;
    // printf("cyle:0x%08x%08x    timmer :0x%08x%08x    timercmp :0x%08x%08x\n",state->cycleh,state->cyclel,state->timerh,state->timerl,state->timermatchh,state->timermatchl);
};
void set_regs(void *st);

// mem/ram op
void * mini_get_ram_addr();
size_t mini_get_ram_size();

__EXPORT void difftest_memcpy(paddr_t addr, void *buf, size_t n, bool direction) {

  if (direction == DIFFTEST_TO_REF) {
    // bool ok = 0;//gdb_memcpy_to_qemu(addr, buf, n);
    // assert(ok == 1);
    // diff stack 16kb
    // uint32_t sp_offset = state->regs[2]-MINIRV32_RAM_IMAGE_OFFSET;
    // if(ram_base!=NULL){
    //   printf("%d",*(uint32_t *)(ram_base+sp_offset));
    // }
  } // TODO 
  else{
    if(difftest_memcpy_call_count==0){
      void *img=mini_get_ram_addr();
      size_t ram_size=mini_get_ram_size();
      memcpy((uint8_t*)buf, (uint8_t*)img, ram_size);
      ++difftest_memcpy_call_count;
    }else{
      memcpy((uint8_t*)buf, (uint8_t*)(ram_base+(addr-MINIRV32_RAM_IMAGE_OFFSET)), n);
      ++difftest_memcpy_call_count;
    }
  }
}

__EXPORT void difftest_regcpy(void *dut, bool direction) {
  struct minirv32_regs * tmp=(struct minirv32_regs *) dut;
  // state=(struct MiniRV32IMAState *)get_core();
  if (direction == DIFFTEST_TO_REF) {
    // set_regs(dut);//仅仅只是做时钟校准
    printf("cyle:0x%08x%08x    timmer :0x%08x%08x    timercmp :0x%08x%08x\n",
    state->cycleh,state->cyclel,state->timerh,state->timerl,state->timermatchh,state->timermatchl);
    // *((uint64_t *)(&((uint8_t *)dut)[0])) =
    // printf("timmer :0x%016lx ",state->timerl+((uint64_t)(state->timerh)<<32));
    *((uint64_t *)(dut))=state->timerl+((uint64_t)(state->timerh)<<32);
    // *(uint64_t *)dut=state->timerl+((uint64_t)(state->timerh)<<32);

  } else {
    memcpy(tmp->gpr,state->regs,DIFFTEST_REG_SIZE-1);
    t_get_regs(dut);
  }

}

__EXPORT void difftest_exec(uint64_t n) {
  while (n --) si(1);
}

__EXPORT void difftest_init(int port) {
  char *image_name=getenv("NEMU_RUN_IMAGE_NAME");
  printf("%s\n",image_name);
  char *config[]={"difftest_init","-f",image_name,"-p"};
  init_run_config(ARRLEN(config),config);
  init_vm();
  state=get_core();
  ram_base=mini_get_ram_addr();
}

__EXPORT void difftest_raise_intr(uint64_t NO) {
  printf("raise_intr is not supported\n");
  assert(0);
}
