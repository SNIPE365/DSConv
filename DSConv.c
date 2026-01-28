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
    int is_file = 1;

    // Detect if input is a string of code or a filename
    if (strchr(raw_input, '{') || strchr(raw_input, '[') || strchr(raw_input, ';')) {
        is_file = 0;
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

        // Parsing
        int ti = 0;
        while (isalpha(*p)) { if (ti < MAX_TYPE - 1) type[ti++] = *p; p++; }
        type[ti] = 0;
        p = skip_ws(p);

        int ni = 0;
        while (isalnum(*p) || *p == '_') { if (ni < MAX_NAME - 1) name[ni++] = *p; p++; }
        name[ni] = 0;
        p = skip_ws(p);

        if (*p++ != '[') { while(*p && *p != ';') p++; p++; continue; }
        while (isdigit(*p)) { size = size * 10 + (*p - '0'); p++; }
        if (*p++ != ']') { while(*p && *p != ';') p++; p++; continue; }

        p = skip_ws(p);
        if (*p++ != '=') { while(*p && *p != ';') p++; p++; continue; }
        p = skip_ws(p);
        if (*p++ != '{') { while(*p && *p != ';') p++; p++; continue; }

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

        // Filter Check
        int match = 0;
        if (filter_type == 0) match = 1; // No filter, match all
        else if (filter_type == 1 && current_index == target_index) match = 1; // Index match
        else if (filter_type == 2 && strcmp(name, filter) == 0) match = 1; // Name match

        if (match) {
            dual_print("%.*s\n", (int)(struct_end - struct_start + 1), struct_start);
            dual_print("type: %s\nname: %s\n", type, name);
            if (strcmp(type, "int") == 0) {
                dual_print("arch: %s\nactual size: %zu bytes\n", get_arch(), sizeof(int));
                dual_print("min value: %d\nmax value: %d\n", INT_MIN, INT_MAX);
                dual_print("theoretical size: 4 bytes\n");
            }
            dual_print("no. of elements %d\nvalues: ", size);
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
        fprintf(stderr, "Usage: %s [flags] source[:index|::name] ...\n", argv[0]);
        return 1;
    }

    char *log_filename = NULL;
    int log_flag_idx = -1;

    // First pass: find global flags
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) silent = 1;
        else if (strcmp(argv[i], "-p") == 0) {
            log_flag_idx = i;
            if (i + 1 < argc && argv[i+1][0] != '-') log_filename = argv[++i];
        }
    }

    // Second pass: process targets
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-p") == 0 && log_filename == argv[i+1]) i++; // Skip the filename arg
            continue;
        }

        char target[MAX_BUFFER];
        strncpy(target, argv[i], MAX_BUFFER - 1);
        
        char *filter = NULL;
        int filter_type = 0; // 0=none, 1=index (:), 2=name (::)

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

        // Auto-log name if -p was used without filename and this is a file
        if (log_flag_idx != -1 && log_filename == NULL && !strchr(target, '{')) {
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
    }

    if (log_file) fclose(log_file);
    return 0;
}