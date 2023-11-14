//
// Created by 86193 on 2023/11/12.
//

#ifndef LD_SECTIONDEF_H
#define LD_SECTIONDEF_H
#include <elf.h>
#include <stdbool.h>
#include "../utils/list.h"
#include "../utils/Hash.h"

#define Get_InnerSection_From_List(list) ((InnerSection *)(list->data))

enum Section_Error{SECTION_ERROR_FLAG=1,SECTION_ERROR_TYPE};
typedef struct _InnerSection{
    uint8_t addrAlign;
    char *secAddr;
    uint64_t size;
}InnerSection;
typedef struct _SectionDef{
    bool isNotProcessSec;
//    uint32_t sectionType;
//    uint32_t sectionFlag;
    char *sectionName;
    Elf64_Shdr sectionHeader;
    char *secAddr;
    uint64_t outputOffset;
//    List *sections;
}SectionDef;
typedef struct _MemoryRange{
    char *secAddr;
    uint64_t size;
} MemoryRange;
typedef struct _DynSection{
    List *sec;
}DynSection;
typedef struct _SymSection{
    Set syms;
    uint64_t index;
}SymSection;
typedef struct _FileSections{
    bool isDynamicLib;
    uint16_t sectionSize;
    SectionDef *sections;
    uint64_t secShStr;
    int secDyn;
    int *rel;
    int relNum;
//    int hash;
    int secSym;
    int symstr;
}FileSections;
//void SectionDef_Initialize(SectionDef *sectionDef);
//int SectionDef_Merge(SectionDef *dst,SectionDef *src);//Merge Don't need to Free
//int SectionDef_Insert(SectionDef *dst,uint8_t addrAlign,char *secAddr,uint64_t size);
//int SectionDef_Free(SectionDef *sec);//Don't use after Merge
#endif //LD_SECTIONDEF_H
