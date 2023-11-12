//
// Created by 86193 on 2023/11/12.
//

#include <malloc.h>
#include <string.h>
#include "GlobalDefs.h"
#include "ProcessSingleFile.h"
#include "Hash.h"
#include "SectionDef.h"

//static Set SectionSetSingle;
//uint64_t *StrTable;
int MergeSymtab(SectionDef *src);
//
//void MergeDyntab(DynSection *glob, SectionDef *src);
int ProcessFile(FILE *file, Elf64_Ehdr *head){
    SectionDef *sections=curFileSections->sections=malloc(head->e_shnum*sizeof(SectionDef));
    uint32_t symTab,dynSec;
    fseek(file,(long)head->e_shoff,SEEK_SET);
    for(int i=0;i<head->e_shnum;++i){
        Elf64_Shdr secHeader;
        fread(&secHeader, sizeof(secHeader),1,file);
        sections[i].isNotProcessSec=secHeader.sh_type!=SHT_NULL;
        sections[i].sectionName=(char *)(((1ll<<63)|secHeader.sh_name));
        sections[i].sectionHeader=secHeader;
        char *data=NULL;
        if(secHeader.sh_type!=SHT_NOBITS){
            fseek(file,(long)secHeader.sh_offset,SEEK_SET);
            data=malloc(secHeader.sh_size);
        }
//        SectionDef_Insert(&sections[i],secHeader.sh_addralign,data,secHeader.sh_size);
        if(secHeader.sh_type==SHT_STRTAB&&!memcmp(data,".shstrtab",sizeof(".shstrtab"))){sections[i].isNotProcessSec=1, curFileSections->secShStr=i; }
    }
    for(int i=0;i<head->e_shnum;++i){
        sections[i].sectionName=&sections[curFileSections->secShStr].secAddr[((uint64_t)sections[i].sectionName)&0xffffffff];
        unsigned long len=strlen(sections[i].sectionName);
        if(len==7&&!strcmp(sections[i].sectionName,".symtab")){sections[i].isNotProcessSec=1; symTab=i; }
        else if(len==8&&!strcmp(sections[i].sectionName,".dynamic")){sections[i].isNotProcessSec=1; dynSec=i; }
        else if(len==8&&!strcmp(sections[i].sectionName,".strtab")){sections[i].isNotProcessSec=1; dynSec=i; }
    }
    if(MergeSymtab(&sections[symTab])) return -1;
//    MergeDyntab(&dynSection,&sections[dynSec]);

}
//void MergeDyntab(DynSection *glob, SectionDef *src){
//}

//two global->multiple def
//global & weak->global
//weak & weak-> use the defined, if more than two defined, use the first one, maybe a randomly chosen, UB?


int MergeSymtab(SectionDef *src){
    Elf64_Sym *info=(Elf64_Sym *)(src->secAddr);
    unsigned long num=src->sectionHeader.sh_size/src->sectionHeader.sh_entsize;
    for(int i=1;i<num;++i){
        if(ELF64_ST_BIND(info[i].st_info)==STB_GLOBAL){
            char *name=&((char *)curFileSections->sections[curFileSections->secShStr].secAddr)[info[i].st_name];
            SymStore *symStore=GetSymStructFromSymSet(name);
            SymStore newStore={curFileIndex,name,info[i]};
//            bool isUnknownSizeCur=info[i].st_shndx==STN_UNDEF&&!info[i].st_size,isUnknownSizeOld=!symStore||symStore->sym_info.st_shndx==STN_UNDEF?!symStore->sym_info.st_size:0;
            if(!symStore) Set_Insert(&GlobalSymSet,&newStore,name,sizeof(SymStore));
//            if(ELF64_ST_BIND(symStore->sym_info.st_info)==STB_GLOBAL||ELF64_ST_BIND(symStore->sym_info.st_info)==STB_WEAK)
            else{
//                if(!isUnknownSizeOld&&!isUnknownSizeCur&&info[i].st_size!=symStore->sym_info.st_size)
//                    printf("Global symbol %s mismatched, in %s has size %lu, but in %s has size %lu\n ",
//                           name,inputfiles[symStore->fileIndex],symStore->sym_info.st_size,currentFilename,info[i].st_size);
//                else{
                    if(ELF64_ST_TYPE(info[i].st_info)!=ELF64_ST_TYPE(info[i].st_info))
                        printf("Global symbol %s mismatched, in %s has size %lu, but in %s has size %lu\n ",
                               name,inputfiles[symStore->fileIndex],symStore->sym_info.st_size,currentFilename,info[i].st_size);
                }
//            }
        }
        else if(ELF64_ST_BIND(info[i].st_info)==STB_WEAK){
            char *name=&((char *)curFileSections->sections[curFileSections->secShStr].secAddr)[info[i].st_name];
            SymStore *symStore=GetSymStructFromSymSet(name);
            if(!symStore){
                SymStore newStore={curFileIndex,name,info[i]};
                Set_Insert(&GlobalSymSet,&newStore,name,sizeof(SymStore));
            }
        }
    }
//    if(!Set_Free(&glob->syms,src))
}
