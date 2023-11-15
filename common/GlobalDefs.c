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
FILE *outputFile;
char *outputFileName="a.out";
Elf64_Ehdr elfHeader={{0x7f,0x45,0x4c,0x46,ELFCLASS64,ELFDATANONE,EV_CURRENT},
                      ET_EXEC,EM_X86_64,EV_CURRENT,0,sizeof(Elf64_Ehdr),
                      0,0,sizeof(Elf64_Ehdr),sizeof(Elf64_Phdr),0,sizeof(Elf64_Shdr),0};
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
