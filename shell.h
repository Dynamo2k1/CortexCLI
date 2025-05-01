#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>
#include <signal.h>
#include <curl/curl.h>
#include <jansson.h>
#include <fcntl.h>

#define MAX_HISTORY 100
#define HELP_TEXT \
"DYNAMO SHELL HELP:\n"\
"  'command'      - Execute AI command\n"\
"  ai:command    - Execute AI command\n"\
"  history       - Show command history\n"\
"  help          - Show this help\n"\
"  !<num>        - Repeat command from history\n"\
"  !!            - Repeat last command\n"\

typedef struct list_path {
    char *dir;
    struct list_path *p;
} list_path;

typedef struct mybuild {
    char *name;
    void (*func)(char **);
} mybuild;

typedef struct {
    char **items;
    int count;
    int size;
} History;

/* Function prototypes */
int contains_pipes(const char *str);
char ***parse_pipeline(char *input);
void execute_pipeline(char ***commands);
int _putchar(char c);
void _puts(const char *str);
int _strlen(char *s);
char *_strdup(char *str);
char *concat_all(char *name, char *sep, char *value);
void expand_tilde(char **args);
void handle_explanation(const char *text);

char **splitstring(char *str, const char *delim);
void execute(char **argv);
void *_realloc(void *ptr, unsigned int old_size, unsigned int new_size);
void freearv(char **arv);

char *_getenv(const char *name);
list_path *add_node_end(list_path **head, char *str);
list_path *linkpath(char *path);
char *_which(char *filename, list_path *head);
void free_list(list_path *head);

void(*checkbuild(char **arv))(char **arv);
int _atoi(char *s);
void exitt(char **arv);
void env(char **arv);
void _setenv(char **arv);
void _unsetenv(char **arv);

char *get_ai_command(const char *input);
void handle_ai_command(char *input);
void add_custom_history(History *hist, const char *cmd);
void show_history(char **arv);
void handle_history_replay(History *hist, const char *cmd);
void show_help(char **arv);
void cd_builtin(char **args);
void cd_dotdot(char **args);
//void cd_dotdot(char **arv __attribute__ ((unused)))
extern char **environ;
extern History hist;

#endif