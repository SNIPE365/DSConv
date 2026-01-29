#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include "dsconv.h"

#define MAX_BUFFER 16384

FILE *log_file = NULL;
FILE *out_file = NULL; 
int silent = 0;
int st_flag = 0;   
int stw_flag = 0;  
int ev_flag = 0; 
int iv_flag = 0;
char struct_var_name[64] = "s";

const char* get_arch() {
    #if defined(_M_X64) || defined(__x86_64__)
        return "x86_64";
    #elif defined(_M_IX86) || defined(__i386__)
        return "x86";
    #elif defined(_M_ARM64) || defined(__aarch64__)
        return "ARM64";
    #else
        return "Unknown";
    #endif
}

void dual_print(const char *format, ...) {
    va_list args;
    if (!silent) {
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
    if (log_file) {
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
    }
}

void print_help() {
    printf("DSConv - C Data Structure Converter\n");
    printf("USAGE 1 (Metadata):  DSConv.exe [flags] [-p file.txt] target[:filter] ...\n");
    printf("USAGE 2 (Conversion): DSConv.exe [-st|-stw] [-ev] [-iv] [-sn name] [-o out.c] target ...\n");
    printf("USAGE 3 (Batch):      DSConv.exe -s -p log.txt file1.c file2.c ...\n\n");
    printf("TARGETS:\n");
    printf("  file.c            Processes all structures found in the file.\n");
    printf("  \"int x[2]={0};\"   Processes the provided code string.\n\n");
    printf("CONVERSION FLAGS:\n");
    printf("  -st               Struct Conversion (Array -> individual members).\n");
    printf("  -stw              Struct Wrap (Array -> member inside struct).\n");
    printf("  -iv               Internal Values (C++ style initialization inside struct).\n");
    printf("  -ev               External Values (C style assignment outside struct).\n");
    printf("  -sn [name]        Set the struct instance variable name (default: 's').\n\n");
    printf("GENERAL FLAGS:\n");
    printf("  -o [file.c]       Output file for generated structural code.\n");
    printf("  -p [file.txt]     Log file for metadata output.\n");
    printf("  -s                Silent mode (suppresses console output).\n");
    printf("  -h / -?           Show this help manual.\n");
}

static char *skip_ws(char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

void process_source(char *raw_input, char *filter, int filter_type) {
    char source_content[MAX_BUFFER] = {0};
    if (strchr(raw_input, '{') || strchr(raw_input, '[') || strchr(raw_input, ';')) {
        strncpy(source_content, raw_input, MAX_BUFFER - 1);
    } else {
        FILE *f = fopen(raw_input, "rb");
        if (!f) { fprintf(stderr, "error: could not open %s\n", raw_input); return; }
        fread(source_content, 1, MAX_BUFFER - 1, f);
        fclose(f);
    }

    char *p = source_content;
    int current_index = 1;
    int target_index = (filter_type == 1) ? atoi(filter) : -1;

    while (*(p = skip_ws(p)) != '\0') {
        DataStruct ds = {0};
        char *struct_start = p;

        int ti = 0; while (isalpha(*p)) { if (ti < 31) ds.type[ti++] = *p; p++; }
        ds.type[ti] = 0; p = skip_ws(p);
        int ni = 0; while (isalnum(*p) || *p == '_') { if (ni < 63) ds.name[ni++] = *p; p++; }
        ds.name[ni] = 0; p = skip_ws(p);

        if (*p++ != '[') { while(*p && *p != ';') p++; if(*p == ';') p++; continue; }
        while (isdigit(*p)) { ds.size = ds.size * 10 + (*p - '0'); p++; }
        if (*p++ != ']') { while(*p && *p != ';') p++; if(*p == ';') p++; continue; }

        p = skip_ws(p); if (*p++ != '=') { while(*p && *p != ';') p++; if(*p == ';') p++; continue; }
        p = skip_ws(p); if (*p++ != '{') { while(*p && *p != ';') p++; if(*p == ';') p++; continue; }

        while (1) {
            p = skip_ws(p); if (*p == '}') break;
            int vi = 0; if (*p == '-') ds.values[ds.value_count][vi++] = *p++;
            while (isdigit(*p)) { if (vi < 31) ds.values[ds.value_count][vi++] = *p; p++; }
            ds.values[ds.value_count][vi] = '\0'; ds.value_count++;
            p = skip_ws(p); if (*p == ',') p++; else if (*p == '}') break;
        }
        if (*p == '}') p++; p = skip_ws(p);
        char *struct_end = (*p == ';') ? p : p-1; if (*p == ';') p++;

        int match = (filter_type == 0) || (filter_type == 1 && current_index == target_index) || (filter_type == 2 && strcmp(ds.name, filter) == 0);

        if (match) {
            dual_print("%.*s\n", (int)(struct_end - struct_start + 1), struct_start);
            dual_print("type: %s\nname: %s\n", ds.type, ds.name);
            if (strcmp(ds.type, "int") == 0) {
                dual_print("arch: %s\nactual size: %zu bytes\n", get_arch(), sizeof(int));
                dual_print("min value: %d\nmax value: %d\n", INT_MIN, INT_MAX);
                dual_print("theoretical size: 4 bytes\n");
            }
            dual_print("no. of elements %d\nno. of values %d\nvalues: ", ds.size, ds.value_count);
            for (int i = 0; i < ds.size; i++) {
                dual_print("%s%s", (i < ds.value_count) ? ds.values[i] : "{uninit}", (i < ds.size - 1) ? ", " : "");
            }
            dual_print("\n\n");

            if (st_flag || stw_flag) {
                convert_to_struct(out_file ? out_file : stdout, ds, stw_flag, ev_flag, iv_flag);
            }
        }
        current_index++;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "error: no input specified\n"); return 1; }

    char *log_filename = NULL;
    char *out_filename = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-?") == 0) { print_help(); return 0; }
        if (strcmp(argv[i], "-s") == 0) silent = 1;
        else if (strcmp(argv[i], "-st") == 0) st_flag = 1;
        else if (strcmp(argv[i], "-stw") == 0) stw_flag = 1;
        else if (strcmp(argv[i], "-ev") == 0) ev_flag = 1;
        else if (strcmp(argv[i], "-iv") == 0) iv_flag = 1;
        else if (strncmp(argv[i], "-sn=", 4) == 0) strncpy(struct_var_name, argv[i] + 4, 63);
        else if (strcmp(argv[i], "-sn") == 0 && i + 1 < argc) strncpy(struct_var_name, argv[++i], 63);
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) log_filename = argv[++i];
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out_filename = argv[++i];
    }

    if (out_filename) out_file = fopen(out_filename, "w");

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-sn") == 0 || strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "-o") == 0) i++;
            continue;
        }
        if (log_filename) log_file = fopen(log_filename, "a");
        
        char target[256]; strncpy(target, argv[i], 255);
        char *filter = NULL; int filter_type = 0;
        char *sep = strstr(target, "::");
        if (sep) { filter_type = 2; *sep = '\0'; filter = sep + 2; }
        else { sep = strchr(target, ':'); if (sep) { filter_type = 1; *sep = '\0'; filter = sep + 1; } }

        process_source(target, filter, filter_type);
        if (log_file) { fclose(log_file); log_file = NULL; }
    }

    if (out_file) fclose(out_file);
    return 0;
}