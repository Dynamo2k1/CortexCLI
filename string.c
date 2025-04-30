#include "shell.h"

char *_strdup(char *str) {
    if(!str) return NULL;
    char *new = malloc(strlen(str) + 1);
    return new ? strcpy(new, str) : NULL;
}

char *concat_all(char *name, char *sep, char *value) {
    char *result = malloc(strlen(name) + strlen(sep) + strlen(value) + 1);
    if(!result) return NULL;
    sprintf(result, "%s%s%s", name, sep, value);
    return result;
}

int _strlen(char *s) {
    int len = 0;
    while(s && s[len]) len++;
    return len;
}

int _putchar(char c) {
    return write(1, &c, 1);
}

void _puts(const char *str) {
    while(str && *str) _putchar(*str++);
}