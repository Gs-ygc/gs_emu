#include "include/atomic_mem_word.h"

// 创建新节点
atomic_mem_word* createNode(word_t data, bool reserved) {
    atomic_mem_word* newNode = (atomic_mem_word*)malloc(sizeof(atomic_mem_word));
    newNode->mem_word = data;
    newNode->reserved = reserved;
    newNode->next_node = NULL;
    return newNode;
}

// 插入节点到链表尾部
void insertNode(atomic_mem_word** head, word_t data, bool reserved) {
    atomic_mem_word* newNode = createNode(data, reserved);
    if (*head == NULL) {
        *head = newNode;
    } else {
        atomic_mem_word* temp = *head;
        while (temp->next_node != NULL) {
            temp = temp->next_node;
        }
        temp->next_node = newNode;
    }
}

// 删除指定数值的节点
void deleteNode(atomic_mem_word** head, word_t data) {
    atomic_mem_word* temp = *head;
    atomic_mem_word* prev = NULL;

    while (temp != NULL && temp->mem_word != data) {
        prev = temp;
        temp = temp->next_node;
    }

    if (temp == NULL) {
        printf("Node with value %d not found\n", data);
        return;
    }

    if (prev == NULL) {
        *head = temp->next_node;
    } else {
        prev->next_node = temp->next_node;
    }

    free(temp);
}

// 查找指定数值的节点
atomic_mem_word* searchNode(atomic_mem_word* head, word_t data) {
    atomic_mem_word* temp = head;

    while (temp != NULL) {
        if (temp->mem_word == data) {
            return temp;
        }
        temp = temp->next_node;
    }

    return NULL;
}

// 修改节点的数值
void updateNode(atomic_mem_word* node, word_t new_data, bool new_reserved) {
    if (node != NULL) {
        node->mem_word = new_data;
        node->reserved = new_reserved;
    }
}