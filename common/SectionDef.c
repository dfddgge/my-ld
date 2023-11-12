//
// Created by 86193 on 2023/11/12.
//

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "SectionDef.h"

//void SectionDef_Initialize(SectionDef *sectionDef){
//    sectionDef->sectionsAddr.data=sectionDef->sectionsAddr.next=NULL;
////    sectionDef->SectionName=
//}
//int SectionDef_Merge(SectionDef *dst, SectionDef *src){
////    if(dst->sectionFlag!=src->sectionFlag) return SECTION_ERROR_FLAG;
////    if(dst->sectionType!=src->sectionType) return SECTION_ERROR_TYPE;
//#ifdef DEBUG
//    if(strcmp(dst->sectionName,src->sectionName)) return 3;
//#endif
//    if(!dst->sections) dst->sections=src->sections;
//    else{
//        List *nextSection=dst->sections;
//        while(nextSection->next){
//            nextSection=nextSection->next;
//        }
//        nextSection->next=src->sections;
//    }
//    free(src);
//    return 0;
//}
//int SectionDef_Insert(SectionDef *dst, uint8_t addrAlign, char *secAddr,uint64_t size){
//    List *nextSection=dst->sections;
//    while(nextSection->next){
//        nextSection=nextSection->next;
//    }
//    InnerSection *inner=malloc(sizeof(InnerSection));
//    inner->addrAlign=addrAlign;
//    inner->secAddr=secAddr;
//    inner->size=size;
//    nextSection=nextSection->next=malloc(sizeof(List));
//    nextSection->data=inner;
//    nextSection->next=NULL;
//    return 0;
//}
//int SectionDef_Free(SectionDef *sec){
//    List *cur=sec->sections;
//    while(cur){
//        List *next=cur->next;
//        free(Get_InnerSection_From_List(cur)->secAddr);
//        free(Get_InnerSection_From_List(cur));
//        free(cur);
//        cur=next;
//    }
//    return 0;
//}
