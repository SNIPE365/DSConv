#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#define MAX_VALUES 1000
#define MAX_NAME   64
#define MAX_TYPE   32

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
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

int main(int argc, char **argv) {
    FILE *f;
    char line[1024];
    char type[MAX_TYPE];
    char name[MAX_NAME];
    int size = 0;
    int values[MAX_VALUES];
    int value_count = 0;
    char *p;
    
    int silent = 0;
    char *filename = NULL;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: %s [-s] <file>\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 's') silent = 1;
        else filename = argv[i];
    }

    if (!filename) {
        fprintf(stderr, "error: no input file specified\n");
        return 1;
    }

    f = fopen(filename, "rb");
    if (!f) { perror("fopen"); return 1; }
    if (!fgets(line, sizeof(line), f)) {
        fprintf(stderr, "empty file\n");
        fclose(f);
        return 1;
    }
    fclose(f);

    p = skip_ws(line);

    /* parse type */
    {
        int i = 0;
        while (isalpha(*p)) {
            if (i < MAX_TYPE - 1) type[i++] = *p;
            p++;
        }
        type[i] = 0;
    }

    p = skip_ws(p);

    /* parse name */
    {
        int i = 0;
        while (isalnum(*p) || *p == '_') {
            if (i < MAX_NAME - 1) name[i++] = *p;
            p++;
        }
        name[i] = 0;
    }

    p = skip_ws(p);
    if (*p++ != '[') { fprintf(stderr, "expected '['\n"); return 1; }
    while (isdigit(*p)) { size = size * 10 + (*p - '0'); p++; }
    if (*p++ != ']') { fprintf(stderr, "expected ']'\n"); return 1; }

    p = skip_ws(p);
    if (*p++ != '=') { fprintf(stderr, "expected '='\n"); return 1; }
    p = skip_ws(p);
    if (*p++ != '{') { fprintf(stderr, "expected '{'\n"); return 1; }

    while (1) {
        p = skip_ws(p);
        if (*p == '}') break;
        if (value_count >= MAX_VALUES) { fprintf(stderr, "too many values\n"); return 1; }
        values[value_count++] = (int)strtol(p, &p, 10);
        p = skip_ws(p);
        if (*p == ',') p++;
        else if (*p == '}') break;
    }

    /* success block */
    if (!silent) {
        printf("type: %s\n", type);
        printf("name: %s\n", name);
        
        if (strcmp(type, "int") == 0) {
            printf("arch: %s\n", get_arch());
            printf("actual size: %zu bytes\n", sizeof(int));
            printf("min value: %d\n", INT_MIN);
            printf("max value: %d\n", INT_MAX);
            printf("data range: 4294967296\n");
            printf("theoretical size: 4 bytes\n");
        }

        printf("no. of elements %d\n", size);
        printf("values: ");

        int current_line_len = 8; // offset for "values: "
        const int MAX_WIDTH = 12;

        // Loop up to 'size' (the declared size), not just 'value_count'
        for (int i = 0; i < size; i++) {
            char buffer[32];
            if (i < value_count) {
                sprintf(buffer, "%d%s", values[i], (i < size - 1) ? "," : "");
            } else {
                sprintf(buffer, "{uninit}%s", (i < size - 1) ? "," : "");
            }

            int len = strlen(buffer);

            // If it's not the first item on a line and it exceeds width, wrap
            if (current_line_len + len > MAX_WIDTH && current_line_len > 0) {
                printf("\n");
                current_line_len = 0;
            }

            printf("%s", buffer);
            current_line_len += len;
        }
        printf("\n");
    }

    return 0;
}