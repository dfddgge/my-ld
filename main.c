#include <stdio.h>
#include <elf.h>
#include <string.h>
#include <stdlib.h>
#include "utils/Hash.h"
#include "process/ProcessSingleFile.h"
#include "common/GlobalDefs.h"
#include "process/LinkAndRelocate.h"

//char __attribute__((weak)) MergeSymtab;
//int isEverythingInitialed;
int Endianness;


void DoOption(int argc,char **argv,int *optionIndex);
int ReadFile(char *filename);

int CheckHead(const Elf64_Ehdr *head);
int CheckPlatform(const Elf64_Ehdr *head);
enum Error{INVALID_ELF=1,FILE_IO_FAILED,INVALID_OPTION=-1};
bool forceOutput=1;
int main(int argc, char **argv){
//    printf("%p\n",&MergeSymtab);
    Initialize();
    inputfiles=malloc(argc*sizeof(char *));
    int inputFileNum=0;
    for(int i=1;i<argc;++i){
        if(argv[i][0]=='-') DoOption(argc,argv,&i);
        else inputfiles[inputFileNum++]=argv[i];
    }
    FileSections fileSections[inputFileNum];
    for(int i=0;i<inputFileNum;++i){
        currentFilename=inputfiles[i];
        curFileIndex=i;
        curFileSections=fileSections+curFileIndex;
        if((ReadFile(inputfiles[i]))) error=1;
    }
    if(!error||forceOutput){
        if(LinkAndRelocate(fileSections, inputFileNum)) return error;
    }
    return error;
}
int CheckPlatform(const Elf64_Ehdr *head){
    int ret=INVALID_ELF;
    if(head->e_type!=ET_REL){
        if(head->e_type==ET_DYN){
            curFileSections->isDynamicLib=true;
        }
        else{
            printf("%s is not a relocatable file!\n", currentFilename);
            goto ret;
        }
    }
    else curFileSections->isDynamicLib=false;
    if(head->e_machine!=EM_X86_64||head->e_flags||head->e_ehsize!=sizeof(Elf64_Ehdr)){ printf("%s is not a x86-64 file!\n",currentFilename);goto ret;}
    ret=0;
    ret:
    return ret;
}
int CheckHead(const Elf64_Ehdr *head){
//    if(isEverythingInitialed){
//        if(memcmp(head,&Glob_Head,20)&&(head->e_flags!=Glob_Head.e_flags)&&(head->e_ehsize!=Glob_Head.e_ehsize)) goto ret;
//        return 0;
//    }
    if(!memcmp(head->e_ident,"\x7f""ELF",4)){
        if(head->e_ident[EI_CLASS]==ELFCLASS64){
            if(head->e_ident[EI_DATA]&3 && head->e_ident[EI_DATA]!=3){
                Endianness=head->e_ident[EI_DATA]==ELFDATA2LSB;
                elfHeader.e_ident[EI_DATA]=Endianness;
                if(head->e_ident[EI_VERSION]==EV_CURRENT){
                    if(!*(int64_t *)(&head->e_ident[8])){
//                        isEverythingInitialed=1;
                        memcpy(&Glob_Head,head,sizeof(Glob_Head));
                        return CheckPlatform(head);
                    }
                }
            }
        }
    }
    ret:
    printf("%s is not a valid 64 bit elf file!\n",currentFilename);
    return INVALID_ELF;
}
int ReadFile(char *filename){
    Elf64_Ehdr head;
    FILE *f;
    int ret;
    if(!(f=fopen(filename,"rb"))){
        printf("Cannot open file %s!\n",filename);
        return FILE_IO_FAILED;
    }
    fread(&head,sizeof(head),1,f);
    if((ret=CheckHead(&head)))return ret;
    return ProcessFile(f,&head);
}
void DoOption(int argc,char **argv,int *optionIndex){
    if(!strcmp(argv[*optionIndex],"-h")){
        printf("This is a x86-64 elf linker!\n Usage:ld file1 [file2...][-o outputfile][-h]\n Attention, clib won't "
               "link by default\n");
        exit(0);
    }
    else if(!strcmp(argv[*optionIndex],"-o")){
        if(argc<=(++*optionIndex)){
            printf("option -o should followed by output filename!\n");
            exit(-1);
        }
        else{
            outputFileName=argv[*optionIndex];
        }
    }
    printf("%s is not a valid option\n",argv[*optionIndex]);
    exit(INVALID_OPTION);
}
