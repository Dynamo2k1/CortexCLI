#include "shell.h"
#include <ctype.h>  // Added for isdigit()

void add_custom_history(History *hist, const char *cmd) {
    if(hist->count >= hist->size) {
        hist->size *= 2;
        hist->items = realloc(hist->items, hist->size * sizeof(char*));
    }
    hist->items[hist->count++] = strdup(cmd);
}

void show_history(char **arv __attribute__ ((unused))) {
    _puts("\nCommand History:\n");
    for(int i=0; i<hist.count; i++) {
        char buf[32];
        snprintf(buf, 32, "%4d  ", i+1);
        _puts(buf);
        _puts(hist.items[i]);
        _puts("\n");
    }
}

void handle_history_replay(History *hist, const char *cmd) {
    if(strcmp(cmd, "!!") == 0) {
        if(hist->count == 0) {
            _puts("No history available\n");
            return;
        }
        char **args = splitstring(hist->items[hist->count-1], " \n");
        execute(args);
        freearv(args);
    }
    else if(isdigit(cmd[1])) {
        int num = atoi(cmd+1);
        if(num < 1 || num > hist->count) {
            _puts("Invalid history number\n");
            return;
        }
        char **args = splitstring(hist->items[num-1], " \n");
        execute(args);
        freearv(args);
    }
    else {
        _puts("Unknown history syntax\n");
    }
}

void show_help(char **arv __attribute__ ((unused))) {
    _puts(HELP_TEXT);
}