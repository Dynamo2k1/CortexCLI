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

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_WHITE   "\033[1;37m"
#define MAX_DIR_STACK 20
#define MAX_HISTORY 100
#define MAX_SESSION_MEMORY 5 
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

typedef struct {
    char *user_input;
    char *ai_response;
} SessionExchange;

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
void analyze_scan_results(const char *scan_output);
void research_vulnerability(const char *service_info);
void ctf_assistance(const char *challenge_info);
void pushd_builtin(char **args);
void popd_builtin(char **args);
void dirs_builtin(char **args);
void cd_minus(char **args);
char **splitstring(char *str, const char *delim);
void execute(char **argv);
void *_realloc(void *ptr, unsigned int old_size, unsigned int new_size);
void freearv(char **arv);
char *_getenv(const char *name);
list_path *add_node_end(list_path **head, char *str);
list_path *linkpath(char *path);
char *_which(char *filename, list_path *head);
void free_list(list_path *head);
void execute_single_command(char *cmd);
void execute_scan_command(char *scan_cmd);
void handle_ai_response(char *response);
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
void expand_variables(char **args);
extern char **environ;
extern History hist;
extern char *dir_stack[MAX_DIR_STACK];
extern int dir_stack_ptr;
extern SessionExchange session_memory[MAX_SESSION_MEMORY];
extern int session_memory_count;

#endif