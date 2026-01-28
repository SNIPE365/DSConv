#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h> // Added this to fix the compiler errors

#define MAX_VALUES 1000
#define MAX_NAME   64
#define MAX_TYPE   32
#define MAX_VAL_STR 32

FILE *out_file = NULL;

/* Helper to print to both console and file */
void dual_print(const char *format, ...) {
    va_list args;
    
    // Print to console
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    // Print to file if -o was used
    if (out_file) {
        va_start(args, format);
        vfprintf(out_file, format, args);
        va_end(args);
    }
}

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

static char *skip_ws(char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    return p;
}

int main(int argc, char **argv) {
    char line[4096] = {0}; 
    char type[MAX_TYPE];
    char name[MAX_NAME];
    char values[MAX_VALUES][MAX_VAL_STR]; 
    char *p;
    
    int silent = 0;
    int inline_mode = 0;
    char *input_arg = NULL;

    if (argc < 2) {
        fprintf(stderr, "usage: %s [-s] [-i \"str\"] [-o out.txt] <file>\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            silent = 1;
        } else if (strcmp(argv[i], "-i") == 0) {
            inline_mode = 1;
            if (i + 1 < argc) input_arg = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                out_file = fopen(argv[++i], "a"); 
            }
        } else {
            input_arg = argv[i];
        }
    }

    if (!input_arg) {
        fprintf(stderr, "error: no input specified\n");
        return 1;
    }

    if (inline_mode) {
        strncpy(line, input_arg, sizeof(line) - 1);
    } else {
        FILE *f = fopen(input_arg, "rb");
        if (!f) { perror("fopen"); return 1; }
        fread(line, 1, sizeof(line) - 1, f);
        fclose(f);
    }

    p = line;

    while (*(p = skip_ws(p)) != '\0') {
        char *struct_start = p;
        int size = 0, value_count = 0;

        /* parse type */
        int ti = 0;
        while (isalpha(*p)) { if (ti < MAX_TYPE - 1) type[ti++] = *p; p++; }
        type[ti] = 0;

        p = skip_ws(p);

        /* parse name */
        int ni = 0;
        while (isalnum(*p) || *p == '_') { if (ni < MAX_NAME - 1) name[ni++] = *p; p++; }
        name[ni] = 0;

        p = skip_ws(p);
        if (*p++ != '[') { fprintf(stderr, "error: expected '['\n"); return 1; }
        while (isdigit(*p)) { size = size * 10 + (*p - '0'); p++; }
        if (*p++ != ']') { fprintf(stderr, "error: expected ']'\n"); return 1; }

        p = skip_ws(p);
        if (*p++ != '=') { fprintf(stderr, "error: expected '='\n"); return 1; }
        p = skip_ws(p);
        if (*p++ != '{') { fprintf(stderr, "error: expected '{'\n"); return 1; }

        while (1) {
            p = skip_ws(p);
            if (*p == '}') break;
            int vi = 0;
            if (*p == '-') values[value_count][vi++] = *p++;
            while (isdigit(*p)) { if (vi < MAX_VAL_STR - 1) values[value_count][vi++] = *p; p++; }
            values[value_count][vi] = '\0';
            value_count++;
            p = skip_ws(p);
            if (*p == ',') p++;
            else if (*p == '}') break;
        }
        if (*p == '}') p++;
        p = skip_ws(p);
        if (*p != ';') { fprintf(stderr, "error: expected ';' after %s\n", name); return 1; }
        char *struct_end = p;
        p++;

        /* success block */
        if (!silent) {
            dual_print("%.*s\n", (int)(struct_end - struct_start + 1), struct_start);
            dual_print("type: %s\nname: %s\n", type, name);
            if (strcmp(type, "int") == 0) {
                dual_print("arch: %s\nactual size: %zu bytes\n", get_arch(), sizeof(int));
                dual_print("min value: %d\nmax value: %d\n", INT_MIN, INT_MAX);
                dual_print("data range: 4294967296\ntheoretical size: 4 bytes\n");
            }
            dual_print("no. of elements %d\nvalues: ", size);
            for (int i = 0; i < size; i++) {
                if (i < value_count) dual_print("%s", values[i]);
                else dual_print("{uninit}");
                if (i < size - 1) dual_print(", ");
            }
            dual_print("\n\n"); 
        }
    }

    if (out_file) fclose(out_file);
    return 0;
}