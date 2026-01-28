#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define MAX_VALUES 1000
#define MAX_NAME   64
#define MAX_TYPE   32

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
    int size;
    int values[MAX_VALUES];
    int value_count = 0;
    char *p;
    
    int silent = 0;
    char *filename = NULL;

    /* Argument parsing for -s flag */
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: %s [-s] <file>\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 's') {
            silent = 1;
        } else {
            filename = argv[i];
        }
    }

    if (!filename) {
        fprintf(stderr, "error: no input file specified\n");
        return 1;
    }

    f = fopen(filename, "rb");
    if (!f) {
        perror("fopen"); // Prints to stderr
        return 1;
    }

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
            if (i < MAX_TYPE - 1)
                type[i++] = *p;
            p++;
        }
        type[i] = 0;
    }

    p = skip_ws(p);

    /* parse identifier */
    {
        int i = 0;
        if (!isalpha(*p) && *p != '_') {
            fprintf(stderr, "invalid identifier\n");
            return 1;
        }
        while (isalnum(*p) || *p == '_') {
            if (i < MAX_NAME - 1)
                name[i++] = *p;
            p++;
        }
        name[i] = 0;
    }

    p = skip_ws(p);

    /* parse [size] */
    if (*p++ != '[') {
        fprintf(stderr, "expected '['\n");
        return 1;
    }

    size = 0;
    while (isdigit(*p)) {
        size = size * 10 + (*p - '0');
        p++;
    }

    if (*p++ != ']') {
        fprintf(stderr, "expected ']'\n");
        return 1;
    }

    p = skip_ws(p);

    /* parse = */
    if (*p++ != '=') {
        fprintf(stderr, "expected '='\n");
        return 1;
    }

    p = skip_ws(p);

    /* parse { values } */
    if (*p++ != '{') {
        fprintf(stderr, "expected '{'\n");
        return 1;
    }

    while (1) {
        p = skip_ws(p);
        if (*p == '}') break;

        if (value_count >= MAX_VALUES) {
            fprintf(stderr, "too many values\n");
            return 1;
        }

        values[value_count++] = (int)strtol(p, &p, 10);
        p = skip_ws(p);

        if (*p == ',') {
            p++;
            continue;
        } else if (*p == '}') {
            break;
        } else {
            fprintf(stderr, "expected ',' or '}'\n");
            return 1;
        }
    }

    p++; /* skip '}' */
    p = skip_ws(p);

    if (*p != ';') {
        fprintf(stderr, "expected ';'\n");
        return 1;
    }

    /* success block */
    if (!silent) {
        printf("type: %s\n", type);
        printf("name: %s\n", name);
        printf("size: %d\n", size);
        printf("init: %d\n", value_count);
        printf("values:\n");

        int current_line_len = 0;
        const int MAX_WIDTH = 12;

        for (int i = 0; i < value_count; i++) {
            char buffer[32];
            // Format number + comma space if not last
            int len = sprintf(buffer, "%d%s", values[i], (i < value_count - 1) ? ", " : "");

            // Wrap if we exceed 12 chars
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