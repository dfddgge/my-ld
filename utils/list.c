//
// Created by 86193 on 2023/11/12.
//

#include <stddef.h>
#include "list.h"

void List_Initialize(List *list){
    list->data=list->next=NULL;
}
