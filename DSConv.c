#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>

#define MAX_VALUES 1000
#define MAX_NAME   64
#define MAX_TYPE   32
#define MAX_VAL_STR 32
#define MAX_BUFFER 16384

FILE *log_file = NULL;
int silent = 0;

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
    printf("Usage: DSConv.exe [flags] [-p [file.txt]] target[:filter] [target[:filter] ...]\n\n");
    printf("TARGETS:\n");
    printf("  A target can be a filename or a raw C-style string.\n");
    printf("  Strings must be wrapped in double quotes (\" \").\n\n");
    printf("  file.c            Processes all structures found in the file.\n");
    printf("  \"int x[2]={0};\"   Processes the provided code string.\n\n");
    printf("FILTERS:\n");
    printf("  Filters are appended to targets to select specific data structures.\n\n");
    printf("  :index            Selects the Nth structure in that source (1-based).\n");
    printf("                    Example: data.c:2 (Gets the second struct in the file).\n");
    printf("  ::identifier      Selects a structure by its variable name.\n");
    printf("                    Example: \"int x[1]={1};\"::x\n\n");
    printf("FLAGS:\n");
    printf("  -p [file.txt]     Print-to-file. Logs console output to a file.\n");
    printf("                    If no filename is provided for a file target, it\n");
    printf("                    defaults to file.txt.\n");
    printf("                    Example: DSConv.exe main.c -p (Creates main.txt)\n\n");
    printf("  -s                Silent Mode. Suppresses all console output.\n");
    printf("                    Best used with -p to generate logs in the background.\n\n");
    printf("  -?                Show this help manual.\n\n");
    printf("COMBINATIONS & EXAMPLES:\n\n");
    printf("  1. Multiple Files (Whole):\n");
    printf("     DSConv.exe file1.c file2.c\n\n");
    printf("  2. Mixed File and String with Index:\n");
    printf("     DSConv.exe source.c:1 \"int x[3]={1,2,3};\":1\n\n");
    printf("  3. Identifier Selection with Logging:\n");
    printf("     DSConv.exe data.c::PlayerStats -p results.txt\n\n");
    printf("  4. Silent Auto-Logging:\n");
    printf("     DSConv.exe script.c -s -p\n");
    printf("     (Writes data from script.c to script.txt with no console output)\n\n");
    printf("  5. Complex Multi-Source:\n");
    printf("     DSConv.exe fileA.c:2 \"int x[2]={1,2}; int y[2]={3,4};\"::y fileB.c\n\n");
    printf("NOTES:\n");
    printf("  * Semicolons (;) are required at the end of every C structure.\n");
    printf("  * Logic detects strings vs files by looking for '{', '[', or ';'.\n");
    printf("  * All leading zeros in integers are preserved in output and logs.\n");
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
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

void process_source(char *raw_input, char *filter, int filter_type) {
    char source_content[MAX_BUFFER] = {0};
    
    // Detect if input is code (contains structural C chars) or a filename
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
        char *struct_start = p;
        char type[MAX_TYPE], name[MAX_NAME], values[MAX_VALUES][MAX_VAL_STR];
        int size = 0, value_count = 0;

        int ti = 0;
        while (isalpha(*p)) { if (ti < MAX_TYPE - 1) type[ti++] = *p; p++; }
        type[ti] = 0;
        p = skip_ws(p);

        int ni = 0;
        while (isalnum(*p) || *p == '_') { if (ni < MAX_NAME - 1) name[ni++] = *p; p++; }
        name[ni] = 0;
        p = skip_ws(p);

        if (*p++ != '[') { while(*p && *p != ';') p++; if(*p == ';') p++; continue; }
        while (isdigit(*p)) { size = size * 10 + (*p - '0'); p++; }
        if (*p++ != ']') { while(*p && *p != ';') p++; if(*p == ';') p++; continue; }

        p = skip_ws(p);
        if (*p++ != '=') { while(*p && *p != ';') p++; if(*p == ';') p++; continue; }
        p = skip_ws(p);
        if (*p++ != '{') { while(*p && *p != ';') p++; if(*p == ';') p++; continue; }

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
        char *struct_end = (*p == ';') ? p : p-1;
        if (*p == ';') p++;

        int match = 0;
        if (filter_type == 0) match = 1;
        else if (filter_type == 1 && current_index == target_index) match = 1;
        else if (filter_type == 2 && strcmp(name, filter) == 0) match = 1;

        if (match) {
            dual_print("%.*s\n", (int)(struct_end - struct_start + 1), struct_start);
            dual_print("type: %s\nname: %s\n", type, name);
            if (strcmp(type, "int") == 0) {
                dual_print("arch: %s\nactual size: %zu bytes\n", get_arch(), sizeof(int));
                dual_print("min value: %d\nmax value: %d\n", INT_MIN, INT_MAX);
                dual_print("theoretical size: 4 bytes\n");
            }
            dual_print("no. of elements %d\n", size);
            dual_print("no. of values %d\n", value_count);
            
            dual_print("values: ");
            for (int i = 0; i < size; i++) {
                if (i < value_count) dual_print("%s", values[i]);
                else dual_print("{uninit}");
                if (i < size - 1) dual_print(", ");
            }
            dual_print("\n");

            if (value_count > size) {
                dual_print("excess values: ");
                for (int i = size; i < value_count; i++) {
                    dual_print("%s", values[i]);
                    if (i < value_count - 1) dual_print(", ");
                }
                dual_print("\n");
            }
            dual_print("\n");
        }
        current_index++;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "error: no input specified\n");
        return 1;
    }

    char *log_filename = NULL;
    int log_flag_idx = -1;
    int targets_found = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-?") == 0 || strcmp(argv[i], "-?") == 0) {
            print_help();
            return 0;
        }
        if (strcmp(argv[i], "-s") == 0) silent = 1;
        else if (strcmp(argv[i], "-p") == 0) {
            log_flag_idx = i;
            if (i + 1 < argc && argv[i+1][0] != '-') log_filename = argv[++i];
        }
    }

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-p") == 0 && log_filename == (i+1 < argc ? argv[i+1] : NULL)) i++;
            continue;
        }
        targets_found++;

        char target[MAX_BUFFER];
        strncpy(target, argv[i], MAX_BUFFER - 1);
        
        char *filter = NULL;
        int filter_type = 0;

        char *sep = strstr(target, "::");
        if (sep) {
            filter_type = 2;
            *sep = '\0';
            filter = sep + 2;
        } else {
            sep = strchr(target, ':');
            if (sep) {
                filter_type = 1;
                *sep = '\0';
                filter = sep + 1;
            }
        }

        if (log_flag_idx != -1 && log_filename == NULL && !strchr(target, '{') && !strchr(target, '[')) {
            static char auto_log[256];
            strncpy(auto_log, target, 250);
            char *dot = strrchr(auto_log, '.');
            if (dot) *dot = '\0';
            strcat(auto_log, ".txt");
            log_file = fopen(auto_log, "w");
        } else if (log_filename && log_file == NULL) {
            log_file = fopen(log_filename, "w");
        }

        process_source(target, filter, filter_type);
        
        if (log_file && log_flag_idx != -1 && log_filename == NULL) {
             fclose(log_file); log_file = NULL;
        }
    }

    if (targets_found == 0) {
        fprintf(stderr, "error: no input specified\n");
        return 1;
    }

    if (log_file) fclose(log_file);
    return 0;
}