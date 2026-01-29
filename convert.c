#include <stdio.h>
#include <string.h>
#include "dsconv.h"

void convert_to_struct(FILE *dest, DataStruct ds, int struct_wrap, int use_ev, int use_iv) {
    fprintf(dest, "/* Generated Structural Representation: %s */\n", ds.name);
    const char* s_tag = "s";
    const char* v_name = struct_var_name;

    if (struct_wrap) {
        fprintf(dest, "struct %s {\n", s_tag);
        if (use_iv) {
            fprintf(dest, "    %s %s[%d] = {", ds.type, ds.name, ds.size);
            for (int i = 0; i < ds.size; i++) {
                fprintf(dest, "%s%s", (i < ds.value_count) ? ds.values[i] : "0", (i < ds.size - 1) ? ", " : "");
            }
            fprintf(dest, "};\n} %s;\n", v_name);
        } else {
            fprintf(dest, "    %s %s[%d];\n} %s;\n", ds.type, ds.name, ds.size, v_name);
        }
        if (use_ev) {
            for (int i = 0; i < ds.size; i++) {
                fprintf(dest, "%s.%s[%d] = %s;\n", v_name, ds.name, i, (i < ds.value_count) ? ds.values[i] : "0");
            }
        }
    } else {
        fprintf(dest, "struct %s {\n", s_tag);
        for (int i = 0; i < ds.size; i++) {
            if (use_iv) fprintf(dest, "    %s %s_%d = %s;\n", ds.type, ds.name, i, (i < ds.value_count) ? ds.values[i] : "0");
            else fprintf(dest, "    %s %s_%d;\n", ds.type, ds.name, i);
        }
        fprintf(dest, "} %s;\n", v_name);
        if (use_ev) {
            for (int i = 0; i < ds.size; i++) {
                fprintf(dest, "%s.%s_%d = %s;\n", v_name, ds.name, i, (i < ds.value_count) ? ds.values[i] : "0");
            }
        }
    }
    fprintf(dest, "\n");
}