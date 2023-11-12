//
// Created by 86193 on 2023/11/12.
//

#ifndef LD_LIST_H
#define LD_LIST_H

typedef struct _List{
    void *data;
    struct _List *next;
} List;
void List_Initialize(List *list);
#endif //LD_LIST_H
