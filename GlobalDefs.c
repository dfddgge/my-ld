//
// Created by 86193 on 2023/11/12.
//

#include "GlobalDefs.h"
const char *currentFilename;
Elf64_Ehdr Glob_Head;
FileSections *curFileSections;
int curFileIndex;
char **inputfiles;
Set GlobalSymSet;
//Set sectionSet;
//DynSection dynSection;
//SymSection symSection;
FileSections *curFileSections;
void __attribute__((weak)) c=1;
#include "stdio.h"
void Initialize(){
//    printf("%ld",c);
//    Set_Initialize(&symSection.syms);
}
