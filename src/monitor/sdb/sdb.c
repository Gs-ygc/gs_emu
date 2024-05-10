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
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <memory/vaddr.h>
#include "sdb.h"

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_help(char *args);
static int cmd_si(char *args);
static int cmd_clear(char *args);
static int cmd_info(char *args);
static int cmd_x(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  {"si", "Single step. When N is not specified, the default is 1.", cmd_si },
  {"clear", "Clear all display on the current terminal screen.", cmd_clear },
  {"info", "Generic command for showing things about the program being debugged", cmd_info },
  {"x","Examine memory: x/FMT ADDRESS.ADDRESS is an expression for the memory address to examine.",cmd_x},
    /* TODO: Add more commands */
};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

static int cmd_si(char *args){
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  word_t N=1;
  if (arg != NULL) {
    N=atol(arg);
    if ((int64_t)N<=0||N>=0x7fffffffffffffff) {
      printf("Argument %s is not valid: Maybe it's  <= 0, too big, or a character...\n", arg);
      return 1;
    }
  }
  cpu_exec(N);
  return 0;
}

static int cmd_clear(char *args){
  printf("\033[H\033[2J");
  return 0;
}

static int cmd_info(char *args){
  char *arg = strtok(NULL, " ");
  bool success=false;

  if (arg!=NULL) {
    if ( !strcmp(arg,"r") || !strcmp(arg,"registers")) {
      arg = strtok(NULL, " ");
      if (arg!=NULL ) {
        word_t val=isa_reg_str2val(arg,&success);
        if(success) printf("%-4s    0x%08lx    %010ld\n", arg, val, val);
        else        printf("Invalid register `%s'\n",arg);
        return 0;
      }
    }else if (strcmp(arg,"all-registers")) {
      printf("Unknown Argument:%s \n",arg);
      return 1;
    }else if (strcmp(arg,"w")) {
      printf("print watch point\n");
      return 0;
    }
  }
  printf("print all reg info\n");
  isa_reg_display();
  return 0;
}

static int cmd_x(char *args){
  word_t N=0;
  word_t EXPR=0;
  char *arg = strtok(NULL, " ");
  bool SUCCESS=false;

  if (arg != NULL) {
    N=atol(arg);
    if ((int64_t)N<=0||N>=0x7fffffffffffffff) {
      printf("Argument %s is not valid: Maybe it's  <= 0, too big, or a character...\n", arg);
      return 1;
    }else if( (arg = strtok(NULL, " ") ) != NULL ){
      EXPR=expr(arg,&SUCCESS);
      if (!SUCCESS) {
        printf("EXPR is ERROR\n");
        return 1;
      }
    }else {
      EXPR=0;
      printf("EXPR is empty\n");
      return 1;
    }
  }
  for (int i=0; i<N; i++) {
    // vaddr_t tmp=vaddr_read(EXPR+4*i,4);
    printf("0x%08lx :   0x%02lx    0x%02lx    0x%02lx    0x%02lx\n",EXPR+4*i,
                  vaddr_read(EXPR+4*i+3,1),vaddr_read(EXPR+4*i+2,1),
                  vaddr_read(EXPR+4*i+1,1),vaddr_read(EXPR+4*i+0,1)
                  );
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
