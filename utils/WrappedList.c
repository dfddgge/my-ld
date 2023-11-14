//
// Created by 86193 on 2023/11/14.
//

#include <stddef.h>
#include "WrappedList.h"

void WrappedList_Initialize(WrappedList *list){
    list->head.next=NULL;
    list->end=&list->head;
}
void WrappedList_insert_last(WrappedList *list, List *node){
    list->end->next=node;
    list->end=node;
}
