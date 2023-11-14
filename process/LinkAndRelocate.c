//
// Created by 86193 on 2023/11/13.
//
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../common/GlobalDefs.h"
#include "LinkAndRelocate.h"
#include "../utils/WrappedList.h"

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

WrappedList outputRela;
Set DynSymSet;
WrappedList outputDynSym;
int DynSymCount=0;

//int RelocateLocal(FileSections *fileSections,int i);
int RelocateGlobal(FileSections *fileSections,int i);
void MergeOffset();
int LinkAndRelocate(FileSections *fileSections, int inputfileNum){
    //todo: finally link the file.
    WrappedList_Initialize(&outputRela);
    WrappedList_Initialize(&outputDynSym);
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
//                case SHT_GNU_verdef:
//                case SHT_GNU_ATTRIBUTES:
//                case SHT_GNU_LIBLIST:
//                case SHT_GNU_HASH:
//                case SHT_GNU_verneed:
//                case SHT_GNU_versym:
//                case SHT_NOTE:
//                    break;
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
//        ret|=RelocateLocal(fileSections,i);
    }
    for(int i=0;i<inputfileNum;++i){
        ret|=RelocateGlobal(fileSections,i);
    }
    return ret;
}
//int findInHash(const char *hashAddr,const char *symAddr){
//
//}
int RelocateGlobal(FileSections *fileSections,int curFileIndex){
    //todo: link symbol from non-Dynamic lib file
    const FileSections *curFile=&fileSections[curFileIndex];
    if(fileSections[curFileIndex].isDynamicLib) return 0;
    int ret=0;
    for(int i=0;i<curFile->relNum;++i){
        SectionDef *curSection=&curFile->sections[i];
        const static char *relName=".rela";
        if(!memcmp(relName,curSection->sectionName,4+(curSection->sectionHeader.sh_type==SHT_RELA))){
            const char *targetSectionName=curSection->sectionName+4+(curSection->sectionHeader.sh_type==SHT_RELA);
            if(curSection->sectionHeader.sh_type==SHT_RELA){
                uint64_t offset;
                List *SectionList=((OutputSectionList *)(((List *)Set_Find(&outputSections,targetSectionName))->data))->begin;
                while(((OutputSectionUnit *)(SectionList->data))->fileIndex!=curFileIndex){
                    SectionList=SectionList->next;
                    if(!SectionList){
                        printf("Relocation Section %s has no matched target Section!\n",curSection->sectionName);
                        ret=1;
                        goto NextRelSec;
                    }
                }
                OutputSectionUnit *targetSection=(OutputSectionUnit *)(SectionList->data);
                Elf64_Rela *relArray=(Elf64_Rela *)curSection->secAddr;
                for(int i=0;i<curSection->sectionHeader.sh_size/curSection->sectionHeader.sh_entsize;++i){
//                    const char *sym=fileSections[i].sections[fileSections->secSym].secAddr+ELF64_R_SYM(relArray[i].r_info);
                    uint32_t symIndex=ELF64_R_SYM(relArray[i].r_info);
                    Elf64_Sym *symArray=(Elf64_Sym *)curFile->sections[curFile->secSym].secAddr;
                    SectionDef *curFileSym=&curFile->sections[curFile->secSym];
                    const char *curFileSymStr=curFile->sections[curFile->symstr].secAddr;
                    for(int j=0;j<curFileSym->sectionHeader.sh_size/curFileSym->sectionHeader.sh_entsize;++j){
                        if(symArray[j].st_name==symIndex){
                            int s;
                            if(symArray[j].st_shndx==STN_UNDEF){
                                const char *name=&curFileSymStr[symIndex];
                                SymStore *symStore=GetSymStructFromSymSet(name,0);
                                if(!symStore){//Dynamic linking
                                    if(!(symStore=Set_Find(&DynSymSet,name))){
                                        symStore=GetSymStructFromSymSet(name,1);//in dynamic lib
                                        Set_Insert(&DynSymSet,symStore,name,sizeof(*symStore));
                                        List *list=malloc(sizeof(SymStore));
                                        SymStore *newSym=malloc(sizeof(SymStore));
                                        list->data=newSym;
                                        *newSym=*symStore;
                                        WrappedList_insert_last(&outputDynSym,list);
                                        ++DynSymCount;
                                    }
                                    List *list=malloc(sizeof(SymStore));
                                    list->data=malloc(sizeof(Elf64_Rela));
                                    WrappedList_insert_last(&outputRela,list);
                                    Elf64_Rela *relStruct=(Elf64_Rela *)list->data;
                                    *relStruct=relArray[j];
                                    relStruct->r_offset+=targetSection->offset;
                                }
                                else{//global definition

                                }
                            }
                            else{//local definition

                            }
                        }
                    }
                }
            }
        }
        else{
            printf("Section %s in file %s has a relocation section but not match syntax\n",curSection->sectionName,inputfiles[i]);
            ret=1;
        }
        NextRelSec:;
    }
    return ret;
}
