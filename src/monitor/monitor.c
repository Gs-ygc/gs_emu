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
#include <memory/paddr.h>
#include <stdio.h>
#include "common.h"
#include "isa-def.h"

void init_rand();
void init_log(const char *log_file);
void init_mem();
void init_difftest(char *ref_so_file, long img_size, int port);
void init_device();
void init_sdb();
void init_disasm(const char *triple);


#ifndef CONFIG_TARGET_AM
#include <getopt.h>

void sdb_set_batch_mode();

static char *log_file = NULL;
static char *diff_so_file = NULL;
static char *img_file = NULL;
static char *bios_file = NULL;
static char *kernel_file = NULL;
static char *dtb_file = NULL;

static int difftest_port = 1234;
extern privilege cur_pri;

static long input_file_load(char * file_name, paddr_t paddr){
  FILE *fp = NULL;
  unsigned int size=0;

  if (file_name!=NULL) {
    fp = fopen(file_name, "rb");
    Assert(fp, "Can not open '%s'", file_name);
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    Log("The image is %s, size = %d mmap  at ["FMT_PADDR", "FMT_PADDR"]", file_name, size,paddr,paddr+size);
    fseek(fp, 0, SEEK_SET);
    int ret = fread(guest_to_host(paddr), size, 1, fp);
    assert(ret == 1);
    fclose(fp);
  }
  return size;
}
static long load_img() {
  long total_size=0;
  if(img_file == NULL && bios_file ==NULL && dtb_file==NULL && kernel_file == NULL){
    Log("no image no bios no kernel from built in image boot");
    return 4096;
  }
  if(img_file != NULL)
    total_size += input_file_load(img_file,RESET_VECTOR);
  if(bios_file != NULL)
    total_size += input_file_load(bios_file,RESET_VECTOR+total_size);

  if (kernel_file!=NULL) 
    total_size += input_file_load(kernel_file,PMEM_KERNEL);

  if(total_size==0) {Log("no image no bios no kernel from built in image boot");return 4096;}

  if (dtb_file!=NULL) 
    total_size += input_file_load(dtb_file,PMEM_DTB);

  return total_size;
}

static int parse_args(int argc, char *argv[]) {
  const struct option table[] = {
    {"batch"    , no_argument      , NULL, 'b'},
    {"log"      , required_argument, NULL, 'l'},
    {"diff"     , required_argument, NULL, 'd'},
    {"port"     , required_argument, NULL, 'p'},
    {"mode"     , required_argument, NULL, 'm'},
    {"bios"     , required_argument, NULL, 'i'},  // 新增的--bios选项
    {"kernel"   , required_argument, NULL, 'k'},  // 新增的--kernel选项
    {"dtb"      , required_argument, NULL, 't'},  // 新增的--dtb选项
    {"help"     , no_argument      , NULL, 'h'},
    {0          , 0                , NULL,  0 },
  };
  int o;
  while ( (o = getopt_long(argc, argv, "-bhl:d:p:", table, NULL)) != -1) {
    switch (o) {
      case 'b': sdb_set_batch_mode(); break;
      case 'p': sscanf(optarg, "%d", &difftest_port); break;
      case 'm': sscanf(optarg, "%d", &cur_pri); break;
      case 'l': log_file = optarg; break;
      case 'd': diff_so_file = optarg; break;
      case 'i': bios_file = optarg; break;  // 处理--bios选项
      case 'k': kernel_file = optarg; break;  // 处理--kernel选项
      case 't': dtb_file = optarg; break;  // 处理--kernel选项
      case 1: img_file = optarg; return 0;
      default:
        printf("Usage: %s [OPTION...] IMAGE [args]\n\n", argv[0]);
        printf("\t-b,--batch              run with batch mode\n");
        printf("\t-l,--log=FILE           output log to FILE\n");
        printf("\t-d,--diff=REF_SO        run DiffTest with reference REF_SO\n");
        printf("\t-p,--port=PORT          run DiffTest with port PORT\n");
        printf("\t-m,--mode=mode          run mode MSU: 0/1/3\n");
        printf("\n");
        exit(0);
    }
  }
  return 0;
}

void init_monitor(int argc, char *argv[]) {
  /* Perform some global initialization. */

  /* Parse arguments. */
  if( freopen("/dev/pts/3", "w", stderr)==NULL ){assert(-1);};
  parse_args(argc, argv);

  /* Set random seed. */
  init_rand();

  /* Open the log file. */
  init_log(log_file);

  /* Initialize memory. */
  init_mem();

  /* Initialize devices. */
  IFDEF(CONFIG_DEVICE, init_device());

  /* Perform ISA dependent initialization. */
  init_isa();

  /* Load the image to memory. This will overwrite the built-in image. */
  long img_size = load_img();

  /* Initialize differential testing. */
  init_difftest(diff_so_file, img_size, difftest_port);

  /* Initialize the simple debugger. */
  init_sdb();

#ifndef CONFIG_ISA_loongarch32r
  IFDEF(CONFIG_ITRACE, init_disasm(
    MUXDEF(CONFIG_ISA_x86,     "i686",
    MUXDEF(CONFIG_ISA_mips32,  "mipsel",
    MUXDEF(CONFIG_ISA_riscv,
      MUXDEF(CONFIG_RV64,      "riscv64",
                               "riscv32"),
                               "bad"))) "-pc-linux-gnu"
  ));
#endif

  /* Display welcome message. */
}
#else // CONFIG_TARGET_AM
static long load_img() {
  extern char bin_start, bin_end;
  size_t size = &bin_end - &bin_start;
  Log("img size = %ld", size);
  memcpy(guest_to_host(RESET_VECTOR), &bin_start, size);
  return size;
}

void am_init_monitor() {
  init_rand();
  init_mem();
  init_isa();
  load_img();
  IFDEF(CONFIG_DEVICE, init_device());
  welcome();
}
#endif
