//
// Created by 86193 on 2023/11/12.
//

#include "GlobalDefs.h"
const char *currentFilename;
Elf64_Ehdr Glob_Head;
FileSections *curFileSections;
int curFileIndex;
char **inputfiles;
Set GlobalSymSet,DynSymSet;
int error=0;
//Set sectionSet;
//DynSection dynSection;
//SymSection symSection;
FileSections *curFileSections;
#include "stdio.h"
void Initialize(){
//    printf("%ld",c);
    Set_Initialize(&DynSymSet);
    Set_Initialize(&GlobalSymSet);
}
