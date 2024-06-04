#ifndef _RISCV_CSR_H_
#define _RISCV_CSR_H_
#include <generated/autoconf.h>
#include "cpu_bits.h"





#ifdef CONFIG_RVPrivMSU
#define CSR_IDX_MAP_OTHER   CSR_MEDELEG, CSR_MIDELEG, CSR_MCOUNTEREN, CSR_SSTATUS,\
                            CSR_SIE, CSR_STVEC, CSR_SCOUNTEREN, CSR_SENVCFG,CSR_SSCRATCH,CSR_SEPC,\
                            CSR_SCAUSE,CSR_STVAL,CSR_SIP,CSR_SATP,
#else
#define CSR_IDX_MAP_OTHER
#endif


#define CSR_IDX_MAP_DEFINE_LIST CSR_MVENDORID,CSR_MHARTID,CSR_MSTATUS,CSR_MIP,CSR_MIE,CSR_MCAUSE,\
                                CSR_MTVEC,CSR_MTVAL,CSR_MEPC,CSR_MSCRATCH,CSR_MISA,CSR_MARCHID,\
                                CSR_MIMPID,CSR_PMPADDR0,CSR_PMPADDR1,CSR_PMPCFG0,CSR_TSELECT,\
                                CSR_TDATA1,CSR_TDATA2,CSR_TDATA3,CSR_TINFO,CSR_DCSR,CSR_DPC,CSR_DSCRATCH0,\
                                CSR_IDX_MAP_OTHER
                                


#define ZERO_CSR 63


// #define NR_CSR (1+33)
// #define ERROR_CSR_IDX NR_CSR
#endif
