//
// Created by 86193 on 2023/11/12.
//

#include <string.h>
#include <stdlib.h>
#include "Hash.h"

uint8_t HashString(const char *str){
    uint8_t res=0;
    while(*str){
        res^=*str;
    }
    return res;
}
void Set_Initialize(Set *set){
    memset(set,0,sizeof(Set));
}
bool Set_Insert(Set *set, void *data,const char *str,uint32_t size){
    uint8_t hash=HashString(str);
    struct InSetUnit *cur=&set->data[hash];
    struct InSetUnit *curFront=&set->data[hash];
    while((cur=(struct InSetUnit *)cur->data->next)){
        if(!strcmp(str,cur->str)) return false;
        curFront=cur;
//        cur=(struct InSetUnit *)cur->data->next;
    }
    cur=(struct InSetUnit *)(curFront->data->next=malloc(sizeof(struct InSetUnit)));
    cur->data=malloc(size);
    memcpy(curFront->data,data,size);
    cur->str=malloc(strlen(str)+1);
    strcpy(cur->str,str);
    return true;
}
void *Set_Find(Set *set,const char *str){
    uint8_t hash=HashString(str);
    struct InSetUnit *cur=&set->data[hash];
    while((cur=(struct InSetUnit *)cur->data->next)){
        if(!strcmp(str,cur->str)) return cur->data->data;
    }
    return NULL;
}
void Set_Replace(Set *set, void *data,const char *str,uint32_t size){
    uint8_t hash=HashString(str);
    struct InSetUnit *cur=&set->data[hash];
    while((cur=(struct InSetUnit *)cur->data->next)){
        if(!strcmp(str,cur->str)){
            free(cur->data);
            cur->data=malloc(size);
            memcpy(cur->data,data,size);
            return;
        }
    }
    Set_Insert(set,data,str,size);
}
void Set_Free(Set *set){
    for(int i=0;i<128;++i){
        struct InSetUnit *cur=(struct InSetUnit *)set->data[i].data->next;
        while(cur){
            struct InSetUnit *curNext=(struct InSetUnit *)cur->data->next;
            free(cur->str);
            free(cur->data->data);
            free(cur);
            cur=curNext;
        }
    }
}

void Set_Clear(Set *set){
    Set_Free(set);
    Set_Initialize(set);
}
uint32_t Elf_Hash(char *str){
    unsigned int hash=0;
    unsigned int x=0;
    while(*str)
    {
        hash=(hash<<4)+*str;
        if((x=hash & 0xf0000000)!=0)
        {
            hash^=(x>>24);
            hash&=~x;
        }
        str++;
    }
    return (hash & 0x7fffffff);
}



