//
// Created by 86193 on 2023/11/12.
//

#include <malloc.h>
#include <string.h>
#include "../common/GlobalDefs.h"
#include "ProcessSingleFile.h"
#include "../utils/Hash.h"
#include "../common/SectionDef.h"
#include "LinkAndRelocate.h"

//static Set SectionSetSingle;
//uint64_t *StrTable;
int MergeSymtab(SectionDef *src,uint16_t symstr,bool isDynamic);
//
//void MergeDyntab(DynSection *glob, SectionDef *src);
int ProcessFile(FILE *file, Elf64_Ehdr *head){
    int ret=0;
    SectionDef *sections=curFileSections->sections=malloc(head->e_shnum*sizeof(SectionDef));
    curFileSections->sectionSize=head->e_shnum;
    curFileSections->secSym=-1;
    fseek(file,(long)head->e_shoff,SEEK_SET);
    for(int i=0;i<head->e_shnum;++i){
        Elf64_Shdr secHeader;
        fread(&secHeader, sizeof(secHeader),1,file);
        sections[i].isNotProcessSec=secHeader.sh_type!=SHT_NULL;
        sections[i].sectionName=(char *)(((1ll<<63)|secHeader.sh_name));
        sections[i].sectionHeader=secHeader;
        char *data=NULL;
        if(secHeader.sh_type!=SHT_NOBITS){
            uint64_t cur=ftell(file);
            fseek(file,(long)secHeader.sh_offset,SEEK_SET);
            data=malloc(secHeader.sh_size);
            fread(data,secHeader.sh_size,1,file);
            fseek(file,cur,SEEK_SET);
        }
        sections[i].secAddr=data;
//        SectionDef_Insert(&sections[i],secHeader.sh_addralign,data,secHeader.sh_size);
        if(secHeader.sh_type==SHT_STRTAB&&!memcmp(data+secHeader.sh_name,".shstrtab",sizeof(".shstrtab"))){sections[i].isNotProcessSec=1, curFileSections->secShStr=i; }
//        else if(secHeader.sh_type==SHT_HASH) curFileSections->hash=i;
    }
    for(int i=0;i<head->e_shnum;++i){
        sections[i].sectionName=&sections[curFileSections->secShStr].secAddr[((uint64_t)sections[i].sectionName)&0xffffffff];
        unsigned long len=strlen(sections[i].sectionName);
        if(len==7&&((!curFileSections->isDynamicLib&&!strcmp(sections[i].sectionName,".symtab"))||(curFileSections->isDynamicLib&&!strcmp(sections[i].sectionName,".dynsym")))){sections[i].isNotProcessSec=1;curFileSections->secSym=i; }
        else if(len==8&&!strcmp(sections[i].sectionName,".dynamic")){sections[i].isNotProcessSec=1; curFileSections->secDyn=i; }
        else if(len==7&&((!curFileSections->isDynamicLib&&!strcmp(sections[i].sectionName,".strtab"))||(curFileSections->isDynamicLib&&!strcmp(sections[i].sectionName,".dynstr")))){sections[i].isNotProcessSec=1; curFileSections->symstr=i; }
    }
    if(curFileSections->secSym!=-1&&MergeSymtab(&sections[curFileSections->secSym],curFileSections->symstr,curFileSections->isDynamicLib)) ret=-1;
    fclose(file);
//    not_used_todo:process .dynamic and .dynsym etc on dynamic lib
//   reason:process in link and relocate
    return ret;
}
//void MergeDyntab(DynSection *glob, SectionDef *src){
//}

//two global->multiple def
//global & weak->global
//weak & weak-> use the defined, if more than two defined, use the first one, maybe a randomly chosen, UB?
//gnu ld does not care about the type of symbol!

int MergeSymtab(SectionDef *src,uint16_t symstr,bool isDynamic){
    int ret=0;
    Elf64_Sym *info=(Elf64_Sym *)(src->secAddr);
    if(!src->sectionHeader.sh_size) return 0;
    unsigned long num=src->sectionHeader.sh_size/src->sectionHeader.sh_entsize;
    for(int i=1;i<num;++i){
        char *name=&((char *)curFileSections->sections[symstr].secAddr)[info[i].st_name];
        SymStore *symStore=GetSymStructFromSymSet(name,isDynamic);
        SymStore newStore={curFileIndex,name,info[i]};
        if(!symStore) Set_Insert(GetCurSymSet(isDynamic),&newStore,name,sizeof(SymStore));
        else{
            uint8_t curBind=ELF64_ST_BIND(info[i].st_info), storeBind=ELF64_ST_BIND(symStore->sym_info.st_info);
            uint8_t curType=ELF64_ST_TYPE(info[i].st_info), storeType=ELF64_ST_TYPE(symStore->sym_info.st_info);
            const static char *st_type_str[]={"No_type", "Object", "Function", "Section", "File"};
//            if(curType&&storeType&&curType!=storeType)
//                printf("Global symbol %s mismatched, in %s has type %s, but in %s has type %s\n ",
//                       name, inputfiles[symStore->fileIndex], st_type_str[ELF64_ST_TYPE(info[i].st_info)],
//                       currentFilename, st_type_str[ELF64_ST_TYPE(symStore->sym_info.st_info)]);
//            else{
                if(curBind==STB_GLOBAL&&storeBind==STB_GLOBAL){
                    if(info[i].st_shndx!=STN_UNDEF&&symStore->sym_info.st_shndx!=STN_UNDEF){
                        if(!curFileSections->isDynamicLib){
                            printf("Multiple definition of symbol %s in file %s and %s", name, currentFilename,
                                   inputfiles[symStore->fileIndex]);
                            ret=-1;
                        }
                    }
                    else if(symStore->sym_info.st_shndx==STN_UNDEF&&info[i].st_shndx!=STN_UNDEF)
                        Set_Replace(GetCurSymSet(isDynamic), &newStore, name, sizeof(newStore));
                }
                else if(curBind==STB_GLOBAL){
                    Set_Replace(GetCurSymSet(isDynamic), &newStore, name, sizeof(newStore));
                }
//            }
        }
    }
    return ret;
}
