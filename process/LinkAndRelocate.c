//
// Created by 86193 on 2023/11/13.
//
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
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
#define DYNAMIC_LENGTH (dynamicNum+16)
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
char *dynamicLinkerPath="/lib64/ld-linux-x86-64.so.2";
//typedef struct _Elf64_Sym_Extended{
//    Elf64_Sym sym;
//    uint64_t offset;
//}Elf64_Sym_Extended;
const char MyPltData[]="\x68\0\0\0\0"/*pushq index*/
                      "\xff\35\0\0\0\0"/*push GOT[0]*/
                      "\xFF\x25\0\0\0\0"/*jmp GOT[3]*/
                      "\0\0\0\0\0\0" /*align to 8 bytes*/
                      ;
Set outputSections;
int *dynamics,dynamicNum=0;//make a dynamic head
char **dynamicName;
Elf64_Phdr *programHeader;
Elf64_Shdr *sectionHeader;
char **sectionNames;
int pltIndex=0;
Set pltSet;
WrappedList pltAddrs;
uint64_t *strOffset;
//except for standard-defined sections, others flag can be merged
//finally set standard-flag

WrappedList outputRela;
WrappedList outputDynSym;
int DynSymCount=0;
uint64_t dynRelaCount=0;
int secCount=0;
uint64_t bss_size=0;
uint64_t bss_offset=0;
uint64_t outputFileOffset;
uint64_t bss_align=8;
uint64_t vMemAfterBSS;

uint64_t *outputSymNameOffset;
Set OutDynSymSet;
//int RelocateLocal(FileSections *fileSections,int i);
uint64_t Align(uint64_t wait,uint64_t align);//align should be pow of 2.
int RelocateGlobal(FileSections *fileSections,int curFileIndex);
void MergeOffset(FileSections *fileSections);
void RelocateSym(FileSections *fileSections, const char *symName, Elf64_Sym *sym, Elf64_Rela *rel, int curFileIndex, int symFileIndex,
                 OutputSectionUnit *targetSection, uint64_t targetOffset);
void SetProgramHeader(Elf64_Phdr *phdr, uint64_t p_align, uint64_t p_filesz, int p_flags, uint64_t p_memsz,
                      uint64_t p_offset, uint64_t p_paddr, uint64_t p_type, uint64_t p_vaddr);
void SetSectionHeader(Elf64_Shdr *shdr, uint64_t sh_addr,uint64_t sh_size, uint64_t sh_addralign, Elf64_Xword sh_entsize, int sh_name,
                      int sh_link, int sh_info, uint64_t sh_offset, Elf64_Word sh_type, uint64_t sh_flags);
void CreateDynamic();
void WriteDyn(char *sectionName, uint64_t d_tag, Elf64_Dyn *dyn, Elf64_Dyn *emptyDyn);
void WriteDynWithSize(const char *sectionName, uint64_t d_tag, uint64_t d_tag_sz, Elf64_Dyn *dyn, Elf64_Dyn *emptyDyn);
int LinkAndRelocate(FileSections *fileSections, int inputfileNum){
    //todo: finally link the file.
    WrappedList_Initialize(&outputRela);
    WrappedList_Initialize(&outputDynSym);
    WrappedList_Initialize(&pltAddrs);
    Set_Initialize(&pltSet);
    Set_Initialize(&OutDynSymSet);
    int ret=0;
    Set_Initialize(&outputSections);
    dynamics=malloc(inputfileNum*sizeof(int));
    for(int i=0;i<inputfileNum;++i){
//        int rel[fileSections[i].sectionSize],relNum=0;
        fileSections[i].rel=malloc(sizeof(int)*fileSections->sectionSize);
        fileSections[i].relNum=0;
        if(fileSections[i].isDynamicLib){
            dynamics[dynamicNum++]=i;
            for(int j=0;j<fileSections[i].sectionSize;++j){
                SectionDef *curSection=&fileSections[i].sections[j];
                switch(curSection->sectionHeader.sh_type){
                    case SHT_REL:
                    case SHT_RELA:
                        fileSections[i].rel[fileSections[i].relNum++]=j;
                    default:;
                }
            }
            continue;
        }
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
                    continue;
                case SHT_REL:
                case SHT_RELA:
                    fileSections[i].rel[fileSections[i].relNum++]=j;
                    continue;
                case SHT_DYNSYM:
                case SHT_DYNAMIC:
                case SHT_SYMTAB:
                case SHT_NULL:
                case SHT_STRTAB:
                    continue;
                default:;
                    {
                        OutputSectionList *sectionListStore=Set_Find(&outputSections, curSection->sectionName);
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
                            Set_Insert(&outputSections, sectionListStore, curSection->sectionName,
                                       sizeof(OutputSectionList));
                            ++secCount;
                        }
                        else{
                            if(sectionListStore->alignBit<curSection->sectionHeader.sh_addralign){
                                sectionListStore->alignBit=curSection->sectionHeader.sh_addralign;
                            }
                            OutputSectionUnit *last=((OutputSectionUnit *)(sectionListStore->end->data));
                            if(curSection->sectionHeader.sh_addralign&&
                               ((last->offset+last->size)&(curSection->sectionHeader.sh_addralign-1))){//offset != 0
                                curSection->outputOffset=unit->offset=
                                        ((last->offset+last->size)&~(curSection->sectionHeader.sh_addralign-1))+
                                        curSection->sectionHeader.sh_addralign;
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
        }
//        ret|=RelocateLocal(fileSections,i);
    }
    MergeOffset(fileSections);
    for(int i=0;i<inputfileNum;++i){
        ret|=RelocateGlobal(fileSections,i);
    }
    outputFileOffset=Align(outputFileOffset,0x1000);
    vMemAfterBSS=Align(vMemAfterBSS,0x1000);
    sectionNames[PLT_SECTION_INDEX]=".plt";
    strOffset[PLT_SECTION_INDEX]=strOffset[PLT_SECTION_INDEX-1]+strlen(sectionNames[PLT_SECTION_INDEX-1])+1;
    SetProgramHeader(&programHeader[PLT_SECTION_INDEX],8,pltIndex*sizeof(MyPltData),PF_X|PF_R,pltIndex*sizeof(MyPltData),outputFileOffset,0,PT_LOAD,vMemAfterBSS);
    SetSectionHeader(&sectionHeader[PLT_SECTION_INDEX],vMemAfterBSS,pltIndex*sizeof(MyPltData),8,sizeof(MyPltData),strOffset[PLT_SECTION_INDEX],0,0,outputFileOffset,SHT_PROGBITS,SHF_EXECINSTR|SHF_ALLOC);
    sectionNames[GOT_SECTION_INDEX]=".got";
    strOffset[GOT_SECTION_INDEX]=strOffset[GOT_SECTION_INDEX-1]+strlen(sectionNames[GOT_SECTION_INDEX-1])+1;
    outputFileOffset+=pltIndex*sizeof(MyPltData);
    vMemAfterBSS+=pltIndex*sizeof(MyPltData);
    outputFileOffset=Align(outputFileOffset,0x1000);
    vMemAfterBSS=Align(vMemAfterBSS,0x1000);
    SetProgramHeader(&programHeader[GOT_SECTION_INDEX],8,(pltIndex+3)*sizeof(void *),PF_R|PF_W|PF_X,(pltIndex+3)*sizeof(void *),
                     outputFileOffset,0,PT_LOAD,vMemAfterBSS);
    SetSectionHeader(&sectionHeader[GOT_SECTION_INDEX],vMemAfterBSS,(pltIndex+3)*sizeof(void *),8,sizeof(void *),strOffset[GOT_SECTION_INDEX],
                     0,0,outputFileOffset,SHT_PROGBITS,SHF_EXECINSTR|SHF_WRITE|SHF_ALLOC);
    outputFileOffset+=(pltIndex+3)*sizeof(void *);
    vMemAfterBSS+=(pltIndex+3)*sizeof(void *);
    outputFileOffset=Align(outputFileOffset,0x1000);
    sectionNames[DYNSYM_SECTION_INDEX]=".dynsym";
    strOffset[DYNSYM_SECTION_INDEX]=strOffset[DYNSYM_SECTION_INDEX-1]+strlen(sectionNames[DYNSYM_SECTION_INDEX-1])+1;
    vMemAfterBSS=Align(vMemAfterBSS,0x1000);
    SetProgramHeader(&programHeader[DYNSYM_SECTION_INDEX],8,(DynSymCount+1)*sizeof(Elf64_Sym),PF_R,(DynSymCount+1)*sizeof(Elf64_Sym),outputFileOffset,0,PT_LOAD,vMemAfterBSS);
    SetSectionHeader(&sectionHeader[DYNSYM_SECTION_INDEX],vMemAfterBSS,(DynSymCount+1)*sizeof(Elf64_Sym),8,sizeof(Elf64_Sym),
                     strOffset[DYNSYM_SECTION_INDEX],DYNSTR_SECTION_INDEX,1,outputFileOffset,SHT_DYNSYM,SHF_ALLOC);

    outputFileOffset+=(DynSymCount+1)*sizeof(Elf64_Sym);
    vMemAfterBSS+=(DynSymCount+1)*sizeof(Elf64_Sym);
    outputFileOffset=Align(outputFileOffset,0x1000);
    vMemAfterBSS=Align(vMemAfterBSS,0x1000);
    sectionNames[DYNAMIC_SECTION_INDEX]=".dynamic";
    strOffset[DYNAMIC_SECTION_INDEX]=strOffset[DYNAMIC_SECTION_INDEX-1]+strlen(sectionNames[DYNAMIC_SECTION_INDEX-1])+1;
    SetProgramHeader(&programHeader[DYNAMIC_SECTION_INDEX],8,DYNAMIC_LENGTH*sizeof(Elf64_Dyn),PF_R,DYNAMIC_LENGTH*sizeof(Elf64_Dyn),
                     outputFileOffset,0,PT_LOAD,vMemAfterBSS);
    SetSectionHeader(&sectionHeader[DYNAMIC_SECTION_INDEX],vMemAfterBSS,DYNAMIC_LENGTH*sizeof(Elf64_Dyn),8,sizeof(Elf64_Dyn),strOffset[DYNAMIC_SECTION_INDEX],
                     DYNSTR_SECTION_INDEX,0,outputFileOffset,SHT_DYNAMIC,SHF_ALLOC);
    outputFileOffset+=DYNAMIC_LENGTH*sizeof(Elf64_Dyn);
    vMemAfterBSS+=DYNAMIC_LENGTH*sizeof(Elf64_Dyn);
    sectionNames[SHSTRTAB_SECTION_INDEX]=".shstrtab";
    strOffset[SHSTRTAB_SECTION_INDEX]=strOffset[SHSTRTAB_SECTION_INDEX-1]+strlen(sectionNames[SHSTRTAB_SECTION_INDEX-1])+1;
    sectionNames[INTERP_SECTION_INDEX]=".interp";
    strOffset[INTERP_SECTION_INDEX]=strOffset[INTERP_SECTION_INDEX-1]+strlen(sectionNames[INTERP_SECTION_INDEX-1])+1;
    sectionNames[DYNSTR_SECTION_INDEX]=".dynstr";
    strOffset[DYNSTR_SECTION_INDEX]=strOffset[DYNSTR_SECTION_INDEX-1]+strlen(sectionNames[DYNSTR_SECTION_INDEX-1])+1;
    sectionNames[RELA_DYN_SECTION_INDEX]=".rela.dyn";
    strOffset[RELA_DYN_SECTION_INDEX]=strOffset[RELA_DYN_SECTION_INDEX-1]+strlen(sectionNames[RELA_DYN_SECTION_INDEX-1])+1;
    outputFileOffset=Align(outputFileOffset,0x1000);
    vMemAfterBSS=Align(vMemAfterBSS,0x1000);
    uint64_t shstrSize=0;
    for(int i=0;i<secCount+EXTRA_NO_PROGRAM_BIT_SECTION_COUNT;++i){
        shstrSize+=strlen(sectionNames[i])+1;
    }
    SetProgramHeader(&programHeader[SHSTRTAB_SECTION_INDEX],8,shstrSize,PF_R,shstrSize,outputFileOffset,0,PT_LOAD,vMemAfterBSS);
    SetSectionHeader(&sectionHeader[SHSTRTAB_SECTION_INDEX],vMemAfterBSS,shstrSize,8,0,strOffset[SHSTRTAB_SECTION_INDEX],0,0,outputFileOffset,SHT_STRTAB,SHF_ALLOC);

    outputFileOffset+=shstrSize;
    vMemAfterBSS+=shstrSize;
    //todo:write to file
    outputFileOffset=Align(outputFileOffset,0x1000);
    vMemAfterBSS=Align(vMemAfterBSS,0x1000);
    int pathLength=strlen(dynamicLinkerPath)+1;
    SetProgramHeader(&programHeader[INTERP_SECTION_INDEX],8,pathLength,PF_R,pathLength,outputFileOffset,0,PT_LOAD,vMemAfterBSS);
    SetSectionHeader(&sectionHeader[INTERP_SECTION_INDEX],vMemAfterBSS,pathLength,8,0,strOffset[INTERP_SECTION_INDEX],0,0,outputFileOffset,SHT_PROGBITS,SHF_ALLOC);
    outputFileOffset+=pathLength;
    vMemAfterBSS+=pathLength;
    outputFileOffset=Align(outputFileOffset,0x1000);
    vMemAfterBSS=Align(vMemAfterBSS,0x1000);
    uint64_t length=1;
    List *head=outputDynSym.head.next;
    outputSymNameOffset=malloc((DynSymCount+dynamicNum)*sizeof(uint64_t));
    outputSymNameOffset[0]=0;
    uint64_t lastLength=1;
    for(int i=0;i<DynSymCount;++i){
        outputSymNameOffset[i+1]=outputSymNameOffset[i]+lastLength;
        length+=(lastLength=strlen(((SymStore *)head->data)->sym)+1);
        head=head->next;
    }
    dynamicName=malloc(dynamicNum*sizeof(char *));
    for(int i=0;i<dynamicNum;++i){
        outputSymNameOffset[i+1+DynSymCount]=outputSymNameOffset[i+DynSymCount]+lastLength;
        dynamicName[i]=inputfiles[dynamics[i]];
        uint64_t secDyn=fileSections[dynamics[i]].secDyn;
        const SectionDef *dynamicSec=&fileSections[dynamics[i]].sections[secDyn];
        int64_t sonameIndex=-1;
        uint64_t offset;
        for(uint64_t i=0;i<dynamicSec->sectionHeader.sh_size/sizeof(Elf64_Dyn);++i){
            const Elf64_Dyn *curDyn=&((Elf64_Dyn *)dynamicSec->secAddr)[i];
            if(curDyn->d_tag==DT_SONAME){
                sonameIndex=curDyn->d_un.d_val;
            }
            else if(curDyn->d_tag==DT_STRTAB){
                offset=curDyn->d_un.d_ptr;
            }
        }
        if(sonameIndex==-1){
            dynamicName[i]=inputfiles[dynamics[i]];
        }
        else{
            FILE *file=fopen(inputfiles[dynamics[i]],"rb");
            if(!file){
                printf("Cannot open file %s!Error code:%d\n",inputfiles[dynamics[i]],errno);
                exit(-1);
            }
            fseek(file,offset+sonameIndex,SEEK_SET);
            {
                char *soname=malloc(1024);
                int sonameIndex=0;
                while(sonameIndex<1023&&(soname[sonameIndex++]=fgetc(file)));
                if(sonameIndex==1023){
                    printf("Too long soname to solve at file %s", inputfiles[dynamics[i]]);
                }
                else{
                    soname[sonameIndex]=0;
                    dynamicName[i]=soname;
                }
            }
            fclose(file);
        }
        length+=(lastLength=strlen(dynamicName[i])+1);
    }
    SetProgramHeader(&programHeader[DYNSTR_SECTION_INDEX],8,length,PF_R,length,outputFileOffset,0,PT_LOAD,vMemAfterBSS);
    SetSectionHeader(&sectionHeader[DYNSTR_SECTION_INDEX],vMemAfterBSS,length,8,0,strOffset[DYNSTR_SECTION_INDEX],0,0,outputFileOffset,SHT_STRTAB,SHF_ALLOC);
    outputFileOffset+=length;
    vMemAfterBSS+=length;
    outputFileOffset=Align(outputFileOffset,0x1000);
    vMemAfterBSS=Align(vMemAfterBSS,0x1000);
    SetProgramHeader(&programHeader[RELA_DYN_SECTION_INDEX],8,dynRelaCount*sizeof(Elf64_Rela),PF_R,dynRelaCount*sizeof(Elf64_Rela),outputFileOffset,0,PT_LOAD,vMemAfterBSS);
    SetSectionHeader(&sectionHeader[RELA_DYN_SECTION_INDEX],vMemAfterBSS,dynRelaCount*sizeof(Elf64_Rela),8,sizeof(Elf64_Rela),strOffset[RELA_DYN_SECTION_INDEX],DYNSYM_SECTION_INDEX,0,outputFileOffset,SHT_RELA,SHF_ALLOC);
    outputFileOffset+=dynRelaCount*sizeof(Elf64_Rela);
    vMemAfterBSS+=dynRelaCount*sizeof(Elf64_Rela);
    outputFileOffset=Align(outputFileOffset,0x1000);
    vMemAfterBSS=Align(vMemAfterBSS,0x1000);
    SymStore *store=GetSymStructFromSymSet("_start",0);

    if(!store){
        printf("Undefined entry point _start!\n");
        exit(-1);
    }
    const char *name=fileSections[store->fileIndex].sections[store->sym_info.st_shndx].sectionName;
    OutputSectionList *list=Set_Find(&outputSections,name);
    uint64_t value=list->offset+fileSections[store->fileIndex].sections[store->sym_info.st_shndx].outputOffset;
    elfHeader.e_entry=value+0x8004000;
    elfHeader.e_shnum=secCount+EXTRA_NO_PROGRAM_BIT_SECTION_COUNT;
    elfHeader.e_shoff=outputFileOffset;
    elfHeader.e_shstrndx=SHSTRTAB_SECTION_INDEX;
    elfHeader.e_phnum=elfHeader.e_shnum;

    outputFile=fopen(outputFileName,"wb");
    if(!outputFile){
        printf("cannot open output file %s!\n",outputFileName);
        return -1;
    }
    fwrite(&elfHeader,sizeof(Elf64_Ehdr),1,outputFile);
    for(int i=0;i<secCount+EXTRA_NO_PROGRAM_BIT_SECTION_COUNT;++i){
        fwrite(&programHeader[i],sizeof(Elf64_Phdr),1,outputFile);
    }
    uint64_t fileOffset=ftell(outputFile);
    uint64_t curOffset=programHeader[1].p_offset;
    for(int i=fileOffset;i<curOffset;++i){
        fputc(0,outputFile);
    }
    for(int i=1;i<secCount+1;++i){
        if(programHeader[i].p_offset>curOffset){
            for(uint64_t j=curOffset;j<programHeader[i].p_offset;++j) fputc(0,outputFile);
            curOffset=programHeader[i].p_offset;
        }
        OutputSectionList *outputSectionList=Set_Find(&outputSections,sectionNames[i]);
        List *innerList=outputSectionList->begin;
        OutputSectionUnit *outputSectionUnit=outputSectionList->begin->data;
        uint64_t inSectionOffset=0;
        while(innerList){
            outputSectionUnit=innerList->data;
            if(outputSectionUnit->offset>inSectionOffset){
                for(uint64_t j=inSectionOffset;j<outputSectionUnit->offset;++j) fputc(0,outputFile);
                inSectionOffset=outputSectionUnit->offset;
            }
            fwrite(outputSectionUnit->addr,outputSectionUnit->size,1,outputFile);
            inSectionOffset+=outputSectionUnit->size;
            innerList=innerList->next;
        }
        curOffset+=inSectionOffset;
    }
//    List *innerList=pltAddrs.head.next;
    if(curOffset<sectionHeader[PLT_SECTION_INDEX].sh_offset){
        for(uint64_t i=curOffset;i<sectionHeader[PLT_SECTION_INDEX].sh_offset;++i) fputc(0,outputFile);
    }
    curOffset=sectionHeader[PLT_SECTION_INDEX].sh_offset;
    for(uint64_t i=0;i<pltIndex;++i){
        char a[sizeof(MyPltData)];
        memcpy(a, MyPltData, sizeof(MyPltData));
        *(int32_t *)(&a[1])=sectionHeader[GOT_SECTION_INDEX].sh_offset+(3+i)*sizeof(void *);
        *(int32_t *)(&a[7])=sectionHeader[GOT_SECTION_INDEX].sh_offset+(3+i)*sizeof(void *)-(curOffset+7+sizeof(MyPltData)*i);
        fwrite(a,sizeof(MyPltData),1,outputFile);
    }
    curOffset+=pltIndex*sizeof(MyPltData);
    if(curOffset<sectionHeader[GOT_SECTION_INDEX].sh_offset){
        for(uint64_t i=curOffset;i<sectionHeader[GOT_SECTION_INDEX].sh_offset;++i) fputc(0,outputFile);
    }
    curOffset=sectionHeader[GOT_SECTION_INDEX].sh_offset;
    fileOffset=ftell(outputFile);
    char nullPtr[8]={0};
    fwrite(&sectionHeader[DYNAMIC_SECTION_INDEX].sh_addr,8,1,outputFile);
    fwrite(nullPtr,8,1,outputFile);
    fwrite(nullPtr,8,1,outputFile);

    for(uint64_t i=0;i<pltIndex;++i){
        uint64_t addr=sectionHeader[PLT_SECTION_INDEX].sh_addr+i*sizeof(MyPltData)+5;
        fwrite(&addr,sizeof(void *),1,outputFile);
    }
    curOffset+=sectionHeader[GOT_SECTION_INDEX].sh_size;
    if(curOffset<sectionHeader[DYNSYM_SECTION_INDEX].sh_offset){
        for(uint64_t i=curOffset;i<sectionHeader[DYNSYM_SECTION_INDEX].sh_offset;++i) fputc(0,outputFile);
    }
    curOffset=sectionHeader[DYNSYM_SECTION_INDEX].sh_offset;
    for(int i=0;i<sizeof(Elf64_Sym);++i){
        fputc(0,outputFile);
    }
    {
        List *list=outputDynSym.head.next;
        for(uint64_t i=0;i<DynSymCount;++i){
            SymStore *store=(SymStore *)list->data;
            Elf64_Sym sym=store->sym_info;
            sym.st_name=outputSymNameOffset[i];
            fwrite(&sym, sizeof(Elf64_Sym), 1, outputFile);
        }
    }
    curOffset+=sectionHeader[DYNSYM_SECTION_INDEX].sh_size;
    if(curOffset<sectionHeader[DYNAMIC_SECTION_INDEX].sh_offset){
        for(uint64_t i=curOffset;i<sectionHeader[DYNAMIC_SECTION_INDEX].sh_offset;++i) fputc(0,outputFile);
    }
    curOffset=sectionHeader[DYNAMIC_SECTION_INDEX].sh_offset;
    Elf64_Dyn dyn,emptyDyn;
    memset(&emptyDyn,0,sizeof(dyn));
    fwrite(&emptyDyn,sizeof(dyn),1,outputFile);
    for(uint64_t i=0;i<dynamicNum;++i){
        dyn.d_tag=DT_NEEDED;
        dyn.d_un.d_val=i+DynSymCount;
        fwrite(&dyn,sizeof(Elf64_Dyn),1,outputFile);
    }
    WriteDyn(".init", DT_INIT, &dyn, &emptyDyn);
    WriteDyn(".fini", DT_FINI, &dyn, &emptyDyn);
    WriteDynWithSize(".init_array", DT_INIT_ARRAY, DT_INIT_ARRAYSZ, &dyn, &emptyDyn);
    WriteDynWithSize(".fini_array", DT_FINI_ARRAY, DT_FINI_ARRAYSZ, &dyn, &emptyDyn);
    dyn.d_tag=DT_STRTAB;
    dyn.d_un.d_ptr=sectionHeader[DYNSTR_SECTION_INDEX].sh_offset;
    fwrite(&dyn,sizeof(Elf64_Dyn),1,outputFile);
    dyn.d_tag=DT_SYMTAB;
    dyn.d_un.d_ptr=sectionHeader[DYNSYM_SECTION_INDEX].sh_offset;
    fwrite(&dyn,sizeof(Elf64_Dyn),1,outputFile);
    dyn.d_tag=DT_STRSZ;
    dyn.d_un.d_val=sectionHeader[DYNSTR_SECTION_INDEX].sh_size;
    fwrite(&dyn,sizeof(Elf64_Dyn),1,outputFile);
    dyn.d_tag=DT_SYMENT;
    dyn.d_un.d_val=sizeof(Elf64_Sym);
    fwrite(&dyn,sizeof(Elf64_Dyn),1,outputFile);
    dyn.d_tag=DT_PLTGOT;
    dyn.d_un.d_ptr=sectionHeader[GOT_SECTION_INDEX].sh_offset;
    fwrite(&dyn,sizeof(Elf64_Dyn),1,outputFile);
    dyn.d_tag=DT_RELA;
    dyn.d_un.d_ptr=sectionHeader[RELA_DYN_SECTION_INDEX].sh_offset;
    fwrite(&dyn,sizeof(Elf64_Dyn),1,outputFile);
    dyn.d_tag=DT_RELASZ;
    dyn.d_un.d_val=sectionHeader[RELA_DYN_SECTION_INDEX].sh_size;
    fwrite(&dyn,sizeof(Elf64_Dyn),1,outputFile);
    dyn.d_tag=DT_RELAENT;
    dyn.d_un.d_val=sizeof(Elf64_Rela);
    fwrite(&dyn,sizeof(Elf64_Dyn),1,outputFile);
    dyn.d_tag=DT_FLAGS;
    dyn.d_un.d_val=DT_BIND_NOW;
    fwrite(&dyn,sizeof(Elf64_Dyn),1,outputFile);
    curOffset+=sectionHeader[DYNAMIC_SECTION_INDEX].sh_size;

    if(curOffset<sectionHeader[SHSTRTAB_SECTION_INDEX].sh_offset){
        for(uint64_t i=curOffset;i<sectionHeader[SHSTRTAB_SECTION_INDEX].sh_offset;++i) fputc(0,outputFile);
    }
    curOffset=sectionHeader[SHSTRTAB_SECTION_INDEX].sh_offset;
    for(int i=0;i<secCount+EXTRA_NO_PROGRAM_BIT_SECTION_COUNT;++i){
        fwrite(sectionNames[i],strlen(sectionNames[i]),1,outputFile);
        fputc(0,outputFile);
    }

    curOffset+=sectionHeader[SHSTRTAB_SECTION_INDEX].sh_size;
    fileOffset=ftell(outputFile);

    if(curOffset<sectionHeader[INTERP_SECTION_INDEX].sh_offset){
        for(uint64_t i=curOffset;i<sectionHeader[INTERP_SECTION_INDEX].sh_offset;++i) fputc(0,outputFile);
    }
    curOffset=sectionHeader[INTERP_SECTION_INDEX].sh_offset;
    fprintf(outputFile,"%s",dynamicLinkerPath);
    fputc(0,outputFile);

    curOffset+=sectionHeader[INTERP_SECTION_INDEX].sh_size;

    if(curOffset<sectionHeader[DYNSTR_SECTION_INDEX].sh_offset){
        for(uint64_t i=curOffset;i<sectionHeader[DYNSTR_SECTION_INDEX].sh_offset;++i) fputc(0,outputFile);
    }
    curOffset=sectionHeader[DYNSTR_SECTION_INDEX].sh_offset;
    head=outputDynSym.head.next;
    fputc(0,outputFile);
    for(int i=0;i<DynSymCount;++i){
        fprintf(outputFile,"%s",((SymStore *)head->data)->sym);
        fputc(0,outputFile);
//        length+=(lastLength=strlen()+1);
        head=head->next;
    }
    for(int i=0;i<dynamicNum;++i){
        fprintf(outputFile,"%s",dynamicName[i]);
        fputc(0,outputFile);
    }

    curOffset+=sectionHeader[DYNSTR_SECTION_INDEX].sh_size;
    if(curOffset<sectionHeader[RELA_DYN_SECTION_INDEX].sh_offset){
        for(uint64_t i=curOffset;i<sectionHeader[RELA_DYN_SECTION_INDEX].sh_offset;++i) fputc(0,outputFile);
    }
    curOffset=sectionHeader[RELA_DYN_SECTION_INDEX].sh_offset;
    head=outputRela.head.next;
    for(uint64_t i=0;i<dynRelaCount;++i){
        fwrite(head->data,sizeof(Elf64_Rela),1,outputFile);
        head=head->next;
    }

    curOffset+=sectionHeader[RELA_DYN_SECTION_INDEX].sh_size;
    fileOffset=ftell(outputFile);

    if(curOffset<elfHeader.e_shoff){
        for(uint64_t i=curOffset;i<elfHeader.e_shoff;++i) fputc(0,outputFile);
    }
    fwrite(sectionHeader,(secCount+EXTRA_NO_PROGRAM_BIT_SECTION_COUNT)*sizeof(Elf64_Shdr),1,outputFile);
    fclose(outputFile);
    return ret;
}
void WriteDynWithSize(const char *sectionName, uint64_t d_tag, uint64_t d_tag_sz, Elf64_Dyn *dyn, Elf64_Dyn *emptyDyn){
    OutputSectionList *initSection=Set_Find(&outputSections, sectionName);
    if(!initSection){
        fwrite(emptyDyn, sizeof(Elf64_Dyn), 1, outputFile);
        fwrite(emptyDyn, sizeof(Elf64_Dyn), 1, outputFile);
    }
    else{
        (*dyn).d_tag=d_tag;
        (*dyn).d_un.d_ptr=initSection->offset;
        fwrite(dyn, sizeof(Elf64_Dyn), 1, outputFile);
        dyn->d_tag=d_tag_sz;
        dyn->d_un.d_val=initSection->size;
        fwrite(dyn, sizeof(Elf64_Dyn), 1, outputFile);
    }

}
void WriteDyn(char *sectionName, uint64_t d_tag, Elf64_Dyn *dyn, Elf64_Dyn *emptyDyn){
    OutputSectionList *initSection=Set_Find(&outputSections, sectionName);
    if(!initSection){
        fwrite(emptyDyn, sizeof(Elf64_Dyn), 1, outputFile);
    }
    else{
        (*dyn).d_tag=d_tag;
        (*dyn).d_un.d_ptr=initSection->offset;
        fwrite(dyn, sizeof(Elf64_Dyn), 1, outputFile);
    }
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
        SectionDef *curSection=&curFile->sections[curFile->rel[i]];
        const static char *relName=".rela";
        if(!memcmp(relName,curSection->sectionName,4+(curSection->sectionHeader.sh_type==SHT_RELA))){
            const char *targetSectionName=curSection->sectionName+4+(curSection->sectionHeader.sh_type==SHT_RELA);
            if(curSection->sectionHeader.sh_type==SHT_RELA){
                uint64_t offset;
                OutputSectionList *outputSectionList=(((OutputSectionList *)Set_Find(&outputSections,targetSectionName)));
                List *sectionList=outputSectionList->begin;
                while(((OutputSectionUnit *)(sectionList->data))->fileIndex!=curFileIndex){
                    sectionList=sectionList->next;
                    if(!sectionList){
                        printf("Relocation Section %s in file %s has no matched target Section!\n",curSection->sectionName,inputfiles[curFileIndex]);
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
                    int j=symIndex;
                    Elf64_Sym sym;
                    const char *name=&curFileSymStr[symArray[j].st_name];
                    int symFileIndex;
                    Elf64_Rela *relStruct=&relArray[j];
                    if(symArray[j].st_shndx==STN_UNDEF){
                        SymStore *symStore=GetSymStructFromSymSet(name,0);
                        if(!symStore||symStore->sym_info.st_shndx==STN_UNDEF){//Dynamic linking
                            if(!(symStore=Set_Find(&OutDynSymSet,name))||symStore->sym_info.st_shndx==STN_UNDEF){
                                symStore=GetSymStructFromSymSet(name,1);//in dynamic lib
                                if(!symStore||(symStore->sym_info.st_shndx=SHN_UNDEF&&ELF64_ST_BIND(symStore->sym_info.st_info)!=STB_WEAK)){
                                    printf("Undefined reference of symbol %s\n",name);
                                    continue;
                                }
                                Set_Insert(&OutDynSymSet,symStore,name,sizeof(*symStore));
                                List *list=malloc(sizeof(SymStore));
                                SymStore *newSym=malloc(sizeof(SymStore));
                                list->data=newSym;
                                *newSym=*symStore;
                                WrappedList_insert_last(&outputDynSym,list);
                                newSym->outIndex=DynSymCount;
                                symStore->outIndex=DynSymCount;
                                ++DynSymCount;
                            }
//                            curFileIndex=symStore->fileIndex;
                            List *list=malloc(sizeof(SymStore));
                            list->data=malloc(sizeof(Elf64_Rela));
                            WrappedList_insert_last(&outputRela,list);
                            ++dynRelaCount;
                            Elf64_Rela *relStruct=(Elf64_Rela *)list->data;
                            *relStruct=relArray[j];
                            relStruct->r_offset+=targetSection->offset;
                            relStruct->r_info=(relStruct->r_info&0xffffffff)|((symStore?symStore->outIndex:0ll)<<32);
                            symFileIndex=symStore?symStore->fileIndex:-1;
//                            relStruct->r_info=ELF64_R_INFO(symStore->outIndex,R_X86_64_GLOB_DAT);
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
                    RelocateSym(fileSections,name, &sym, relStruct, curFileIndex, symFileIndex, targetSection,
                                targetOffset);
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
void RelocateSym(FileSections *fileSections, const char *symName, Elf64_Sym *sym, Elf64_Rela *rel, int curFileIndex, int symFileIndex,
                 OutputSectionUnit *targetSection, uint64_t targetOffset){
    uint64_t value;
    switch(sym->st_shndx){
        case SHN_UNDEF:
            value=0;
            break;
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
            const char *name=fileSections[symFileIndex].sections[sym->st_shndx].sectionName;
            OutputSectionList *list=Set_Find(&outputSections,name);
            value=list->offset+fileSections[symFileIndex].sections[sym->st_shndx].outputOffset;
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
            rel->r_info=(rel->r_info&0xffffffff00000000)|R_X86_64_GLOB_DAT;
//            ++dynRelaCount;
            break;
        }
        default:
            printf("Rel type %lu is not support\n", ELF64_R_TYPE(rel->r_info));
    }
}
void MergeOffset(FileSections *fileSections){
    programHeader=malloc((secCount+EXTRA_NO_PROGRAM_BIT_SECTION_COUNT)*sizeof(Elf64_Phdr));
    sectionHeader=malloc((secCount+EXTRA_NO_PROGRAM_BIT_SECTION_COUNT)*sizeof(Elf64_Shdr));
    sectionNames=malloc((secCount+EXTRA_NO_PROGRAM_BIT_SECTION_COUNT)*sizeof(char *));
    strOffset=malloc(sizeof(uint64_t)*(secCount+EXTRA_NO_PROGRAM_BIT_SECTION_COUNT));
    sectionNames[0]="";
    strOffset[0]=0;
    memset(programHeader,0,sizeof(Elf64_Phdr));//Null Header
    memset(sectionHeader,0,sizeof(Elf64_Shdr));//Null Header
    int segmentIndex=1;
    outputFileOffset=sizeof(Elf64_Ehdr)+(secCount+EXTRA_NO_PROGRAM_BIT_SECTION_COUNT)*sizeof(Elf64_Phdr);
//    int tmp=0;
    for(int i=0;i<128;++i){
        struct InSetUnit *p=(struct InSetUnit *)outputSections.data[i].data->next;
        while(p){
            OutputSectionList *list=((OutputSectionList *)p->data->data);
            Elf64_Shdr *singleSecHeader=((OutputSectionUnit *)list->begin->data)->header;
            if(outputFileOffset&0xfff) outputFileOffset=(outputFileOffset&~(0xfff))+0x1000;
            list->offset=outputFileOffset;
            strOffset[segmentIndex]=strOffset[segmentIndex-1]+strlen(sectionNames[segmentIndex-1])+1;
            SetProgramHeader(&programHeader[segmentIndex],list->alignBit,list->size,PF_R|((list->flag&SHF_WRITE)?PF_W:0)|((list->flag&SHF_EXECINSTR)?PF_X:0),
                             list->size,outputFileOffset,0x8004000+outputFileOffset,PT_LOAD,0x8004000+outputFileOffset);
            SetSectionHeader(&sectionHeader[segmentIndex], 0x8004000+outputFileOffset,list->size, list->alignBit,
                             singleSecHeader->sh_entsize, strOffset[segmentIndex],
                             SHN_UNDEF, 0, outputFileOffset, singleSecHeader->sh_type, list->flag);
            sectionNames[segmentIndex]=p->str;
            ++segmentIndex;
            outputFileOffset+=list->size;
            p=(struct InSetUnit *)p->data->next;
//            ++tmp;
        }
//        ++secCount;
    }
    if(outputFileOffset&0xfff) outputFileOffset=(outputFileOffset&~0xfff)+0x1000;
    SetProgramHeader(&programHeader[BSS_SECTION_INDEX],bss_align,0,PF_R|PF_W,bss_size,outputFileOffset,0x8004000+outputFileOffset,PT_LOAD,0x8004000+outputFileOffset);
    sectionNames[BSS_SECTION_INDEX]=".bss";
    strOffset[BSS_SECTION_INDEX]=strOffset[BSS_SECTION_INDEX-1]+strlen(sectionNames[BSS_SECTION_INDEX-1])+1;
    SetSectionHeader(&sectionHeader[BSS_SECTION_INDEX], outputFileOffset,bss_size, bss_align, 0, strOffset[BSS_SECTION_INDEX],
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
