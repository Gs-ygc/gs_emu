// gs2ygc

#ifndef __ATOMIC_MEM_WORD_H__
#define __ATOMIC_MEM_WORD_H__

#include <stdlib.h>
#include <stdbool.h>
#include "common.h"


typedef struct Node {
    word_t mem_word;
    bool reserved;
    struct Node* next_node;
} atomic_mem_word;

// 创建新节点
atomic_mem_word* createNode(word_t data, bool reserved);

// 插入节点到链表尾部
void insertNode(atomic_mem_word** head, word_t data, bool reserved) ;

// 删除指定数值的节点
void deleteNode(atomic_mem_word** head, word_t data) ;
// 查找指定数值的节点
atomic_mem_word* searchNode(atomic_mem_word* head, word_t data);
// 修改节点的数值
void updateNode(atomic_mem_word* node, word_t new_data, bool new_reserved);

#endif
