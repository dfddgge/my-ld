//
// Created by 86193 on 2023/11/13.
//
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../common/GlobalDefs.h"
#include "LinkAndRelocate.h"
#include "../utils/WrappedList.h"
//add 11 segment,including NULL,.bss,.plt,.got,.dynsym,.dynamic,.shstrtab,.interp,.dynstr,.rela.dyn
#define EXTRA_NO_PROGRAM_BIT_SECTION_COUNT 10
#define BSS_SECTION_INDEX (secCount+1)
#define PLT_SECTION_INDEX (secCount+2)
#define GOT_SECTION_INDEX (secCount+3)
#define DYNSYM_SECTION_INDEX (secCount+4)
#define DYNAMIC_SECTION_INDEX (secCount+5)
#define SHSTRTAB_SECTION_INDEX (secCount+6)
#define INTERP_SECTION_INDEX (secCount+7)
#define DYNSTR_SECTION_INDEX (secCount+8)
#define RELA_DYN_SECTION_INDEX (secCount+9)
#define DYNAMIC_LENGTH (dynamicNum+19)
typedef struct _outputSectionUnit{
    uint64_t offset;
    char *addr;
    Elf64_Shdr *header;
    uint64_t size;
    uint64_t fileIndex;
}OutputSectionUnit;
typedef struct outputSectionList{
    uint64_t alignBit;
    uint64_t size;
    uint64_t offset;
    uint64_t flag;
    List *begin;
    List *end;
}OutputSectionList;
typedef struct _pltUnit{
    uint64_t offset;
    uint64_t index;
    uint64_t got_index;
}pltUnit;
//typedef struct _Elf64_Sym_Extended{
//    Elf64_Sym sym;
//    uint64_t offset;
//}Elf64_Sym_Extended;
const char MyPltData[]="\x68\0\0\0\0\0\0\0"/*pushq index*/
                      "\x68\0\0\0\0\0\0\0"/*push GOT[0]*/
                      "\xFF\x25\0\0\0\0"/*jmp GOT[3]*/
                      "\0" /*align to 8 bytes*/
                      ;
Set outputSections;
int *dynamics,dynamicNum=0;//make a dynamic head
Elf64_Phdr *programHeader;
Elf64_Shdr *sectionHeader;
char **sectionNames;
int pltIndex=0;
Set pltSet;
WrappedList pltAddrs;
//except for standard-defined sections, others flag can be merged
//finally set standard-flag

WrappedList outputRela;
Set DynSymSet;
WrappedList outputDynSym;
int DynSymCount=0;
int secCount=0;
uint64_t bss_size=0;
uint64_t bss_offset=0;
uint64_t outputFileOffset;
uint64_t bss_align=8;
uint64_t vMemAfterBSS;

//int RelocateLocal(FileSections *fileSections,int i);
uint64_t Align(uint64_t wait,uint64_t align);//align should be pow of 2.
int RelocateGlobal(FileSections *fileSections,int curFileIndex);
void MergeOffset(FileSections *fileSections);
void RelocateSym(FileSections *fileSections,const char *symName, Elf64_Sym *sym, const Elf64_Rela *rel, int curFileIndex, int symFileIndex,
                 OutputSectionUnit *targetSection, uint64_t targetOffset);
void SetProgramHeader(Elf64_Phdr *phdr, uint64_t p_align, uint64_t p_filesz, int p_flags, uint64_t p_memsz,
                      uint64_t p_offset, uint64_t p_paddr, uint64_t p_type, uint64_t p_vaddr);
void SetSectionHeader(Elf64_Shdr *shdr, uint64_t sh_addr,uint64_t sh_size, uint64_t sh_addralign, Elf64_Xword sh_entsize, int sh_name,
                      int sh_link, int sh_info, uint64_t sh_offset, Elf64_Word sh_type, uint64_t sh_flags);
void CreateDynamic();
int LinkAndRelocate(FileSections *fileSections, int inputfileNum){
    //todo: finally link the file.
    WrappedList_Initialize(&outputRela);
    WrappedList_Initialize(&outputDynSym);
    WrappedList_Initialize(&pltAddrs);
    Set_Initialize(&pltSet);
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
                case SHT_NOBITS:
                    bss_size+=curSection->sectionHeader.sh_size;
//                    if(bss_align<curSection->sectionHeader.sh_addralign)bss_align=curSection->sectionHeader.sh_addralign;
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
                    unit->header=&curSection->sectionHeader;
                    List *list=malloc(sizeof(List));
                    list->data=unit;
                    list->next=NULL;
                    if(!sectionListStore){
                        sectionListStore=malloc(sizeof(OutputSectionList));
                        sectionListStore->alignBit=curSection->sectionHeader.sh_addralign;
                        sectionListStore->size=curSection->sectionHeader.sh_size;
                        unit->offset=0;
                        curSection->outputOffset=0;
                        sectionListStore->begin=sectionListStore->end=list;
                        sectionListStore->flag=curSection->sectionHeader.sh_flags;
                        Set_Insert(&outputSections,sectionListStore,curSection->sectionName,sizeof(OutputSectionList));
                    }
                    else{
                        if(sectionListStore->alignBit<curSection->sectionHeader.sh_addralign){
                            sectionListStore->alignBit=curSection->sectionHeader.sh_addralign;
                        }
                        OutputSectionUnit *last=((OutputSectionUnit *)(sectionListStore->end->data));
                        if(curSection->sectionHeader.sh_addralign&&
                            ((last->offset+last->size) & (curSection->sectionHeader.sh_addralign-1)) ){//offset != 0
                            curSection->outputOffset=unit->offset=((last->offset+last->size)&~(curSection->sectionHeader.sh_addralign-1))+curSection->sectionHeader.sh_addralign;
                        }
                        else{
                            unit->offset=last->offset+last->size;
                        }
                        sectionListStore->flag|=curSection->sectionHeader.sh_flags;
                        sectionListStore->size=unit->offset+curSection->sectionHeader.sh_size;
                        sectionListStore->end->next=list;
                        sectionListStore->end=list;
                    }
            }
        }
//        ret|=RelocateLocal(fileSections,i);
    }
    MergeOffset(fileSections);
    for(int i=0;i<inputfileNum;++i){
        ret|=RelocateGlobal(fileSections,i);
    }
    outputFileOffset=Align(outputFileOffset,0x1000);
    vMemAfterBSS=Align(vMemAfterBSS,0x1000);
    SetProgramHeader(&programHeader[PLT_SECTION_INDEX],8,pltIndex*sizeof(MyPltData),PF_X|PF_R,pltIndex*sizeof(MyPltData),outputFileOffset,0,PT_LOAD,vMemAfterBSS);
    SetSectionHeader(&sectionHeader[PLT_SECTION_INDEX],vMemAfterBSS,pltIndex*sizeof(MyPltData),8,sizeof(MyPltData),PLT_SECTION_INDEX,0,0,outputFileOffset,SHT_PROGBITS,SHF_EXECINSTR|SHF_ALLOC);
    sectionNames[PLT_SECTION_INDEX]=".plt";
    sectionNames[GOT_SECTION_INDEX]=".got";
    outputFileOffset+=pltIndex*sizeof(MyPltData);
    vMemAfterBSS+=pltIndex*sizeof(MyPltData);
    outputFileOffset=Align(outputFileOffset,0x1000);
    vMemAfterBSS=Align(vMemAfterBSS,0x1000);
    SetProgramHeader(&programHeader[GOT_SECTION_INDEX],8,(pltIndex+3)*sizeof(void *),PF_R|PF_W|PF_X,(pltIndex+3)*sizeof(void *),
                     outputFileOffset,0,PT_LOAD,vMemAfterBSS);
    SetSectionHeader(&sectionHeader[GOT_SECTION_INDEX],vMemAfterBSS,(pltIndex+3)*sizeof(void *),8,sizeof(void *),GOT_SECTION_INDEX,
                     0,0,outputFileOffset,SHT_PROGBITS,SHF_EXECINSTR|SHF_WRITE|SHF_ALLOC);

    outputFileOffset=Align(outputFileOffset,0x1000);
    sectionNames[DYNSYM_SECTION_INDEX]=".dynsym";
    vMemAfterBSS=Align(vMemAfterBSS,0x1000);
    SetProgramHeader(&programHeader[DYNSYM_SECTION_INDEX],8,DynSymCount*sizeof(Elf64_Sym),PF_R,DynSymCount*sizeof(Elf64_Sym),outputFileOffset,0,PT_LOAD,vMemAfterBSS);
    SetSectionHeader(&sectionHeader[DYNSYM_SECTION_INDEX],vMemAfterBSS,DynSymCount*sizeof(Elf64_Sym),8,sizeof(Elf64_Sym),
                     DYNSYM_SECTION_INDEX,0,0,outputFileOffset,SHT_DYNSYM,SHF_ALLOC);

    outputFileOffset=Align(outputFileOffset,0x1000);
    vMemAfterBSS=Align(vMemAfterBSS,0x1000);
    sectionNames[DYNAMIC_SECTION_INDEX]=".dynamic";
    SetProgramHeader(&programHeader[DYNAMIC_SECTION_INDEX],8,DYNAMIC_LENGTH*sizeof(Elf64_Dyn),PF_R,DYNAMIC_LENGTH*sizeof(Elf64_Dyn),
                     outputFileOffset,0,PT_LOAD,vMemAfterBSS);
    SetSectionHeader(&sectionHeader[DYNAMIC_SECTION_INDEX],vMemAfterBSS,DYNAMIC_LENGTH*sizeof(Elf64_Dyn),8,sizeof(Elf64_Dyn),DYNAMIC_SECTION_INDEX,
                     0,0,outputFileOffset,SHT_DYNAMIC,SHF_ALLOC);
    sectionNames[SHSTRTAB_SECTION_INDEX]=".shstrtab";
    sectionNames[INTERP_SECTION_INDEX]=".interp";
    sectionNames[DYNSTR_SECTION_INDEX]=".dynstr";
    sectionNames[RELA_DYN_SECTION_INDEX]=".rela.dyn";
    outputFileOffset=Align(outputFileOffset,0x1000);
    vMemAfterBSS=Align(vMemAfterBSS,0x1000);
    uint64_t shstrSize=0;
    for(int i=0;i<secCount+EXTRA_NO_PROGRAM_BIT_SECTION_COUNT;++i){
        shstrSize+=strlen(sectionNames[i])+1;
    }
    SetProgramHeader(&programHeader[SHSTRTAB_SECTION_INDEX],8,shstrSize,PF_R,shstrSize,outputFileOffset,0,PT_LOAD,vMemAfterBSS);
    SetSectionHeader(&sectionHeader[SHSTRTAB_SECTION_INDEX],vMemAfterBSS,shstrSize,8,0,SHSTRTAB_SECTION_INDEX,0,0,outputFileOffset,SHT_STRTAB,SHF_ALLOC);
    //todo:make last 3 header
    //todo:write to file
//    CreateDynamic();
    return ret;
}
//void CreateDynamic(){
//    for
//}
//int findInHash(const char *hashAddr,const char *symAddr){
//
//}
int RelocateGlobal(FileSections *fileSections,int curFileIndex){
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
                OutputSectionList *outputSectionList=(OutputSectionList *)(((List *)Set_Find(&outputSections,targetSectionName))->data);
                List *sectionList=outputSectionList->begin;
                while(((OutputSectionUnit *)(sectionList->data))->fileIndex!=curFileIndex){
                    sectionList=sectionList->next;
                    if(!sectionList){
                        printf("Relocation Section %s has no matched target Section!\n",curSection->sectionName);
                        ret=1;
                        goto NextRelSec;
                    }
                }
                OutputSectionUnit *targetSection=(OutputSectionUnit *)(sectionList->data);
                uint64_t targetOffset=outputSectionList->offset;
                Elf64_Rela *relArray=(Elf64_Rela *)curSection->secAddr;
                for(int i=0;i<curSection->sectionHeader.sh_size/curSection->sectionHeader.sh_entsize;++i){
//                    const char *sym=fileSections[i].sections[fileSections->secSym].secAddr+ELF64_R_SYM(relArray[i].r_info);
                    uint32_t symIndex=ELF64_R_SYM(relArray[i].r_info);
                    Elf64_Sym *symArray=(Elf64_Sym *)curFile->sections[curFile->secSym].secAddr;
                    SectionDef *curFileSym=&curFile->sections[curFile->secSym];
                    const char *curFileSymStr=curFile->sections[curFile->symstr].secAddr;
                    const char *name=&curFileSymStr[symIndex];
                    for(int j=0;j<curFileSym->sectionHeader.sh_size/curFileSym->sectionHeader.sh_entsize;++j){
                        if(symArray[j].st_name==symIndex){
                            Elf64_Sym sym;
                            int symFileIndex;
                            if(symArray[j].st_shndx==STN_UNDEF){
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
                                    curFileIndex=symStore->fileIndex;
                                    List *list=malloc(sizeof(SymStore));
                                    list->data=malloc(sizeof(Elf64_Rela));
                                    WrappedList_insert_last(&outputRela,list);
                                    Elf64_Rela *relStruct=(Elf64_Rela *)list->data;
                                    *relStruct=relArray[j];
                                    relStruct->r_offset+=targetSection->offset;
                                }
                                else{//global definition
                                    sym=symStore->sym_info;
                                    symFileIndex=symStore->fileIndex;
                                }
                            }
                            else{//local definition
                                sym=symArray[j];
                                symFileIndex=curFileIndex;
                            }
                            RelocateSym(fileSections,name, &sym, &relArray[i], curFileIndex, symFileIndex, targetSection,
                                        targetOffset);
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
void RelocateSym(FileSections *fileSections,const char *symName, Elf64_Sym *sym, const Elf64_Rela *rel, int curFileIndex, int symFileIndex,
                 OutputSectionUnit *targetSection, uint64_t targetOffset){
    uint64_t value;
    switch(sym->st_shndx){
        case SHN_ABS:
            value=sym->st_value;
            break;
        case SHN_COMMON:
            sym->st_value=(bss_offset+=sym->st_value);
            sym->st_shndx=SHN_LORESERVE;
            break;
        case SHN_LORESERVE://in bss
            value=sym->st_value;
            break;
        default:;
            const char *name=fileSections[curFileIndex].sections[sym->st_shndx].sectionName;
            OutputSectionList *list=Set_Find(&outputSections,name);
            value=list->offset+fileSections[curFileIndex].sections[sym->st_shndx].outputOffset;
            break;
    }
    switch(ELF64_R_TYPE(rel->r_info)){
        case R_X86_64_PC32:;
            {
                int32_t pc=(int32_t)(targetSection->offset+targetOffset+rel->r_addend);
                *(int32_t *)(&targetSection->addr[targetOffset])=(int32_t)value-pc;
                break;
            }
        case R_X86_64_PLT32:{
            pltUnit *pltStore=Set_Find(&pltSet,symName);
            int32_t value;
            if(!pltStore){
                List *list;
                pltUnit *unit=malloc(sizeof(pltUnit));
                list->data=unit;
                unit->index=pltIndex++;unit->got_index=unit->index+3;
                unit->offset=pltIndex*sizeof(MyPltData);
                WrappedList_insert_last(&pltAddrs,list);
                Set_Insert(&pltSet,unit,symName,sizeof(pltUnit));
                value=unit->offset;
            }
            else{
                value=pltStore->offset;
            }
            int32_t pc=(int32_t)(targetSection->offset+targetOffset+rel->r_addend);
            *(int32_t *)(&targetSection->addr[targetOffset])=(int32_t)(value+vMemAfterBSS-pc);
            break;
        }
        default:
            printf("Rel type %lu is not support", ELF64_R_TYPE(rel->r_info));
    }
}
void MergeOffset(FileSections *fileSections){
    programHeader=malloc((secCount+EXTRA_NO_PROGRAM_BIT_SECTION_COUNT)*sizeof(Elf64_Phdr));
    sectionHeader=malloc((secCount+EXTRA_NO_PROGRAM_BIT_SECTION_COUNT)*sizeof(Elf64_Shdr));
    sectionNames=malloc((secCount+EXTRA_NO_PROGRAM_BIT_SECTION_COUNT)*sizeof(char *));
    sectionNames[0]="";
    memset(programHeader,0,sizeof(Elf64_Phdr));//Null Header
    memset(sectionHeader,0,sizeof(Elf64_Shdr));//Null Header
    int segmentIndex=1;
    outputFileOffset=sizeof(Elf64_Ehdr)+(secCount+6)*sizeof(Elf64_Phdr);
    for(int i=0;i<128;++i){
        struct InSetUnit *p=(struct InSetUnit *)outputSections.data[i].data->next;
        while(p){
            OutputSectionList *list=((OutputSectionList *)p->data);
            Elf64_Shdr *singleSecHeader=((OutputSectionUnit *)list->begin)->header;
            if(outputFileOffset&0xfff) outputFileOffset=(outputFileOffset&~(0xfff))+0x1000;
            SetProgramHeader(&programHeader[segmentIndex],list->alignBit,list->size,PF_R|((list->flag&SHF_WRITE)?PF_W:0)|((list->flag&SHF_EXECINSTR)?PF_X:0),
                             list->size,outputFileOffset,0x8004000+outputFileOffset,PT_LOAD,0x8004000+outputFileOffset);
            SetSectionHeader(&sectionHeader[segmentIndex], 0x8004000+outputFileOffset,list->size, list->alignBit,
                             singleSecHeader->sh_entsize, segmentIndex,
                             SHN_UNDEF, 0, outputFileOffset, singleSecHeader->sh_type, list->flag);
            sectionNames[segmentIndex]=p->str;
            ++segmentIndex;
            outputFileOffset+=list->size;
            p=(struct InSetUnit *)p->data->next;
        }
    }
    if(outputFileOffset&0xfff) outputFileOffset=(outputFileOffset&~0xfff)+0x1000;
    SetProgramHeader(&programHeader[BSS_SECTION_INDEX],bss_align,0,PF_R|PF_W,bss_size,outputFileOffset,0x8004000+outputFileOffset,PT_LOAD,0x8004000+outputFileOffset);
    sectionNames[secCount+1]=".bss";
    SetSectionHeader(&sectionHeader[BSS_SECTION_INDEX], outputFileOffset,bss_size, bss_align, 0, BSS_SECTION_INDEX,
                     0, 0, outputFileOffset,
                     SHT_NOBITS, SHF_WRITE|SHF_ALLOC);
    vMemAfterBSS=0x8004000+outputFileOffset+bss_size;
}
void SetSectionHeader(Elf64_Shdr *shdr, uint64_t sh_addr,uint64_t sh_size,uint64_t sh_addralign, Elf64_Xword sh_entsize, int sh_name,
                      int sh_link, int sh_info, uint64_t sh_offset, Elf64_Word sh_type, uint64_t sh_flags){
    shdr->sh_addr=sh_addr;
    shdr->sh_addralign=sh_addralign;
    shdr->sh_entsize=sh_entsize;
    shdr->sh_name=sh_name;
    shdr->sh_link=sh_link;
    shdr->sh_info=sh_info;
    shdr->sh_offset=sh_offset;
    shdr->sh_type=sh_type;
    shdr->sh_flags=sh_flags;
    shdr->sh_size=sh_size;
}
void SetProgramHeader(Elf64_Phdr *phdr, uint64_t p_align, uint64_t p_filesz, int p_flags, uint64_t p_memsz,
                      uint64_t p_offset, uint64_t p_paddr, uint64_t p_type, uint64_t p_vaddr){
    phdr->p_align=p_align;
    phdr->p_filesz=p_filesz;
    phdr->p_flags=p_flags;
    phdr->p_memsz=p_memsz;
    phdr->p_offset=p_offset;
    phdr->p_paddr=p_paddr;
    phdr->p_type=p_type;
    phdr->p_vaddr=p_vaddr;
}
uint64_t Align(uint64_t wait, uint64_t align){
    if(align==1||!align) return wait;
    if(wait&align-1){
        wait=(wait&~(align-1))+align;
    }
    return wait;
}
