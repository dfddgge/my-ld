//
// Created by 86193 on 2023/11/12.
//

#ifndef LD_HASH_H
#define LD_HASH_H
#include <stdint.h>
#include <stdbool.h>
#include "list.h"

uint8_t HashString(char *str);
struct InSetUnit{
    List *data;
    char *str;
};
struct _Set{
    struct InSetUnit data[128];
};
typedef struct _Set Set;
void Set_Initialize(Set *set);
bool Set_Insert(Set *set,void *data,char *str,uint32_t size);
void *Set_Find(Set *set,char *str);
void Set_Free(Set *set);
void Set_Clear(Set *set);
void Set_Replace(Set *set,void *data,char *str,uint32_t size);
//void Set_Merge(Set *dst,Set *)
#endif //LD_HASH_H
