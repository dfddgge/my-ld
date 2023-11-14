//
// Created by 86193 on 2023/11/14.
//

#ifndef LD_WRAPPEDLIST_H
#define LD_WRAPPEDLIST_H

#include "list.h"

typedef struct _WrappedList{
    List head;
    List *end;
}WrappedList;
void WrappedList_Initialize(WrappedList *list);
void WrappedList_insert_last(WrappedList *list,List *node);
#endif //LD_WRAPPEDLIST_H
