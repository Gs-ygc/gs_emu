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

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <memory/vaddr.h>
#include <regex.h>

#include "common.h"

enum {
  TK_NOTYPE = 256,
  TK_EQ,
  TK_DECIMAL,
  TK_HEX,
  TK_REG,
  TK_WORD

};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
    {" +", TK_NOTYPE},  // spaces
    {"\\+", '+'},       // plus
    {"\\-", '-'},
    {"\\*", '*'},
    {"/", '/'},
    {"\\(", '('},
    {"\\)", ')'},
    {"==", TK_EQ},  // equal
    // TODO 增加为运算与关系运算
    {"0x[0-9A-Fa-f]+", TK_HEX},
    {"\\b[0-9]+\\b", TK_DECIMAL},
    {"\\$[0-9a-zA-Z]+", TK_REG},
    {"\\b[a-zA-Z_][a-zA-Z0-9_]*\\b", TK_WORD},
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used)) = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;
  char bracket_stack[32] = {0};
  int stack_top = 0;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 &&
          pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        // Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
        //     i, rules[i].regex, position, substr_len, substr_len,
        //     substr_start);

        position += substr_len;

        switch (rules[i].token_type) {
          case ')': {
            if (stack_top == 0 || bracket_stack[stack_top - 1] != '(') {
              printf("Invalid expression at position %d\n%s\n%*.s^\n", position,
                     e, position - 1, "");
              return false;
            } else {
              bracket_stack[stack_top--] = 0;
              if (nr_token >= 32) {
                printf(
                    "The number of tokens is greater than 32 %d\n%s\n%*.s^\n",
                    position, e, position, "");
                return false;
              }
              strncpy(tokens[nr_token].str, substr_start, substr_len);
              tokens[nr_token].type = rules[i].token_type;
              nr_token++;
              break;
            }
          }
          case '(': {
            bracket_stack[stack_top++] = '(';
          }
          case TK_EQ:
          case '+':
          case '-':
          case '*':
          case '/':
          case TK_WORD:
          case TK_REG:
          case TK_DECIMAL:
          case TK_HEX: {
            if (nr_token >= 32) {
              printf("The number of tokens is greater than 32 %d\n%s\n%*.s^\n",
                     position, e, position, "");
              return false;
            }
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].type = rules[i].token_type;
            nr_token++;
            break;
          }
          default:
            break;
        }
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }
  if (stack_top != 0) {
    printf("Invalid expression at position %d\n%s\n%*.s^\n", position, e,
           position - 1, "");
    return false;
  }
  return true;
}

void clear_tokens() {
  for (int i = 0; i < nr_token; i++) {
    memset(tokens[i].str, 0, 32);
    tokens[i].type = 0;
  }
  nr_token = 0;
}

word_t calculate(char c, word_t a, word_t b) {
  word_t tmp = 0;
  switch (c) {
    case '+':
      tmp = a + b;
      break;
    case '-':
      tmp = a - b;
      break;
    case '*':
      tmp = a * b;
      break;
    case '/':
      Assert(b!=0, "Arithmetic Exception! \"%d / %d\" ",a,b);
      tmp = a / b;
      break;
  }
  return tmp;
}

bool is_not_num(int type) {
  switch (type) {
    case '+':
    case '-':
    case '*':
    case '/':
    case TK_NOTYPE:
      return true;
  }
  return false;
}

word_t tokens_calculation(bool *success) {
  // "0x80100000+   (($a0 +5)*4 - *(  $t1 + 8)) + number"
  word_t num_stack[32];
  int num_stack_top = 0;
  char op_stack[32];
  int op_stack_top = 0;
  word_t result = -1;

  for (int i = 0; i < nr_token; i++) {
    switch (tokens[i].type) {
      case TK_DECIMAL: {
        num_stack[num_stack_top++] = atol(tokens[i].str);
        break;
      }
      case TK_HEX: {
        num_stack[num_stack_top++] = strtol(tokens[i].str, NULL, 0);
        break;
      }
      case TK_REG: {
        num_stack[num_stack_top++] =
            isa_reg_str2val(tokens[i].str + 1, success);
        if (*success == false) {
          printf("Reg %s is not find\n", tokens[i].str);
          return -1;
        }
        break;
      }
      case TK_WORD: {
        // TODO add WORD
        num_stack[num_stack_top++] = 0;
        break;
      }
      case '(': {
        op_stack[op_stack_top++] = '(';
        break;
      }
      case ')': {
        while (op_stack_top > 0 && op_stack[--op_stack_top] != '(') {
          if (op_stack[op_stack_top - 1] == '#') {
            result = vaddr_read(num_stack[num_stack_top - 1], 4);
          } else {
            word_t a = num_stack[--num_stack_top];
            word_t b = num_stack[--num_stack_top];
            result = calculate(op_stack[op_stack_top], b, a);
          }
          num_stack[num_stack_top++] = result;
        }
        break;
      }
      case '*': {
        if (i == 0 || is_not_num(tokens[i - 1].type)) {
          op_stack[op_stack_top++] = '#';
          break;
        }
      }
      case '/': {
        if (op_stack_top>0) {
          while (op_stack[op_stack_top - 1] == '*' ||
                op_stack[op_stack_top - 1] == '/' ||
                op_stack[op_stack_top - 1] == '#') {
            if (op_stack[op_stack_top - 1] == '#') {
              result = vaddr_read(num_stack[num_stack_top - 1], 4);
            } else {
              word_t a = num_stack[--num_stack_top];
              word_t b = num_stack[--num_stack_top];
              result = calculate(op_stack[--op_stack_top], b, a);
            }
            num_stack[num_stack_top++] = result;
          }
        }
        op_stack[op_stack_top++] = tokens[i].type;
        break;
      }
      case '-':
      case '+': {
        if (op_stack_top == 0 || op_stack[op_stack_top - 1] == '(' ||
            op_stack[op_stack_top - 1] == '+' ||
            op_stack[op_stack_top - 1] == '-') {
          op_stack[op_stack_top++] = tokens[i].type;
          break;
        }
        while (op_stack[op_stack_top - 1] == '*' ||
               op_stack[op_stack_top - 1] == '/') {
          if (op_stack[op_stack_top - 1] == '#') {
            Assert(num_stack[num_stack_top-1]!=0, "Attempted to dereference a null pointer.");
            result = vaddr_read(num_stack[--num_stack_top], 4);
            op_stack_top--;
          } else {
            word_t a = num_stack[--num_stack_top];
            word_t b = num_stack[--num_stack_top];
            result = calculate(op_stack[--op_stack_top], b, a);
          }
          num_stack[num_stack_top++] = result;
        }
        break;
      }
    }
  }

  while (op_stack_top > 0&&num_stack_top>0) {
    if (op_stack_top>0&&op_stack[op_stack_top - 1] == '#') {
      Assert(num_stack[num_stack_top-1]!=0, "Attempted to dereference a null pointer.");
      result = vaddr_read(num_stack[--num_stack_top], 4);
      op_stack_top--;
    } else if(op_stack_top>0&&num_stack_top>1){
      word_t a = num_stack[--num_stack_top];
      word_t b = num_stack[--num_stack_top];
      result = calculate(op_stack[--op_stack_top], b, a);
    }
    num_stack[num_stack_top++] = result;
  }

  *success = true;
  return num_stack[0];
}

word_t expr(char *e, bool *success) {
  clear_tokens();

  if (!make_token(e)) {
    *success = false;
    clear_tokens();
    return 0;
  }

  return tokens_calculation(success);
}
