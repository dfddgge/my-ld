//
// Created by 86193 on 2023/11/12.
//

#ifndef LD_GLOBALDEFS_H
#define LD_GLOBALDEFS_H

#include <elf.h>
#include <stdio.h>
#include "../utils/Hash.h"
#include "SectionDef.h"

extern const char *currentFilename;
extern Elf64_Ehdr Glob_Head;
extern FileSections *curFileSections;
extern int curFileIndex;
extern char **inputfiles;
extern int error;
typedef struct _SymStore{
    int fileIndex;
    char *sym;
    Elf64_Sym sym_info;
    uint64_t outIndex;
}SymStore;

#define GetSymStructFromSymSet(str,isDynamic) ((SymStore *)(Set_Find(GetCurSymSet(isDynamic),str)))
#define GetCurSymSet(isDynamic) ((isDynamic)?&DynSymSet:&GlobalSymSet)
extern Set GlobalSymSet,DynSymSet;
extern char *outputFileName;
extern FILE *outputFile;
extern Elf64_Ehdr elfHeader;
//extern Set sectionSet;
//extern DynSection dynSection;
//extern SymSection symSection;
void Initialize();
#endif //LD_GLOBALDEFS_H
