#ifndef DSCONV_H
#define DSCONV_H

#include <stdio.h>

typedef struct {
    char type[32];
    char name[64];
    int size;
    char values[1000][32];
    int value_count;
} DataStruct;

void convert_to_struct(FILE *dest, DataStruct ds, int struct_wrap, int use_ev, int use_iv);

extern int silent;
extern char struct_var_name[64];

#endif