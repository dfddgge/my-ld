//
// Created by 86193 on 2023/11/13.
//
#include <stdlib.h>
#include "../common/GlobalDefs.h"
#include "LinkAndRelocate.h"
#include "../utils/Hash.h"

typedef struct _outputSectionUnit{
    uint64_t offset;
    char *addr;
    uint64_t size;
    uint64_t fileIndex;
}OutputSectionUnit;
typedef struct outputSectionList{
    uint64_t alignBit;
    List *begin;
    List *end;
}OutputSectionList;

Set outputSections;
int *dynamics,dynamicNum=0;//make a dynamic head
//except for standard-defined sections, others flag can be merged
//finally set standard-flag

int RelocateLocal(const FileSections *curFile);
int RelocateGlobal();
int LinkAndRelocate(FileSections *fileSections, int inputfileNum){
    //todo: finally link the file.
    int ret=0;
    Set_Initialize(&outputSections);
    dynamics=malloc(inputfileNum*sizeof(int));
    for(int i=0;i<inputfileNum;++i){
//        int rel[fileSections[i].sectionSize],relNum=0;
        if(fileSections->isDynamicLib){
            dynamics[dynamicNum++]=i;
            for(int j=0;j<fileSections[i].sectionSize;++j){
                SectionDef *curSection=&fileSections[i].sections[j];
                switch(curSection->sectionHeader.sh_type){
                    case SHT_REL:
                    case SHT_RELA:
                        fileSections[i].rel[fileSections[i].relNum++]=j;
                }
            }
        }
        fileSections[i].rel=malloc(sizeof(int)*fileSections->sectionSize);
        fileSections[i].relNum=0;
        for(int j=0;j<fileSections[i].sectionSize;++j){
            SectionDef *curSection=&fileSections[i].sections[j];
            switch(curSection->sectionHeader.sh_type){
                case SHT_GNU_verdef:
                case SHT_GNU_ATTRIBUTES:
                case SHT_GNU_LIBLIST:
                case SHT_GNU_HASH:
                case SHT_GNU_verneed:
                case SHT_GNU_versym:
                case SHT_NOTE:
                    break;
                case SHT_REL:
                case SHT_RELA:
                    fileSections[i].rel[fileSections[i].relNum++]=j;
                    break;
                default:;
                    OutputSectionList *sectionListStore=Set_Find(&outputSections,curSection->sectionName);
                    OutputSectionUnit *unit=malloc(sizeof(OutputSectionUnit));
                    unit->addr=curSection->secAddr;
                    unit->fileIndex=i;
                    unit->size=curSection->sectionHeader.sh_size;
                    List *list=malloc(sizeof(List));
                    list->data=unit;
                    list->next=NULL;
                    if(!sectionListStore){
                        sectionListStore=malloc(sizeof(OutputSectionList));
                        sectionListStore->alignBit=curSection->sectionHeader.sh_addralign;
                        unit->offset=0;
                        curSection->outputOffset=0;
                        sectionListStore->begin=sectionListStore->end=list;
                        Set_Insert(&outputSections,sectionListStore,curSection->sectionName,sizeof(OutputSectionList));
                    }
                    else{
                        if(sectionListStore->alignBit<curSection->sectionHeader.sh_addralign){
                            sectionListStore->alignBit=curSection->sectionHeader.sh_addralign;
                        }
                        OutputSectionUnit *last=((OutputSectionUnit *)(sectionListStore->end->data));
                        if(curSection->sectionHeader.sh_addralign&&
                            ((last->offset+last->size) & (curSection->sectionHeader.sh_addralign-1)) ){//offset != 0
                            curSection->outputOffset=unit->offset=((last->offset+last->size)|~(curSection->sectionHeader.sh_addralign-1))+curSection->sectionHeader.sh_addralign;
                        }
                        else{
                            unit->offset=last->offset+last->size;
                        }
                        sectionListStore->end->next=list;
                        sectionListStore->end=list;
                    }
            }
        }
        ret|=RelocateLocal(fileSections+i);
    }
    ret|=RelocateGlobal();
    return ret;
}
int RelocateGlobal(){
    //todo: link global symbol
}
int RelocateLocal(const FileSections *curFile){
    //todo: link symbol in local file
}
