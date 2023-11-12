//
// Created by 86193 on 2023/11/12.
//

#ifndef LD_PROCESSSINGLEFILE_H
#define LD_PROCESSSINGLEFILE_H

#include <stdio.h>
#include <elf.h>

int ProcessFile(FILE *file, Elf64_Ehdr *head);

#endif //LD_PROCESSSINGLEFILE_H
