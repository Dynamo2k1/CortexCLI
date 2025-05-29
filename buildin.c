#include "shell.h"

// Directory stack
char *dir_stack[MAX_DIR_STACK];
int dir_stack_ptr = -1;

// Pushd builtin
void pushd_builtin(char **args) {
    if (!args[1]) {
        _puts("pushd: missing argument\n");
        return;
    }

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        if (dir_stack_ptr < MAX_DIR_STACK - 1) {
            dir_stack[++dir_stack_ptr] = strdup(cwd);
        } else {
            _puts("Directory stack full\n");
            return;
        }
    } else {
        perror("getcwd");
        return;
    }

    cd_builtin(args);
}

// Popd builtin
void popd_builtin(char **args __attribute__ ((unused))) {
    if (dir_stack_ptr < 0) {
        _puts("popd: directory stack empty\n");
        return;
    }

    char *dir = dir_stack[dir_stack_ptr--];
    char *cd_args[] = {"cd", dir, NULL};
    cd_builtin(cd_args);
    free(dir);
}

// Dirs builtin
void dirs_builtin(char **args __attribute__ ((unused))) {
    for (int i = dir_stack_ptr; i >= 0; i--) {
        _puts(dir_stack[i]);
        _puts("\n");
    }
    
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        _puts(cwd);
        _puts("\n");
    }
}

// Cd minus
void cd_minus(char **args __attribute__ ((unused))) {
    if (dir_stack_ptr < 0) {
        _puts("cd-: no previous directory\n");
        return;
    }

    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) {
        perror("getcwd");
        return;
    }

    char *dir = dir_stack[dir_stack_ptr];
    dir_stack[dir_stack_ptr] = strdup(cwd);
    
    char *cd_args[] = {"cd", dir, NULL};
    cd_builtin(cd_args);
    free(dir);
}

// Exit the shell
void exitt(char **arv) {
    int status = 0;
    if(arv[1]) {
        status = _atoi(arv[1]);
        status = status < 0 ? 2 : status;
    }
    exit(status);
}
void cd_dotdot(char **arv __attribute__ ((unused))) {
    char *args[] = {"cd", "..", NULL};
    cd_builtin(args);
}

void cd_builtin(char **args) {
    char *dir = args[1];
    char expanded_dir[PATH_MAX] = {0};

    if (!dir || strcmp(dir, "-") == 0) {
        if (dir_stack_ptr >= 0) {
            dir = dir_stack[dir_stack_ptr];
        } else if (!dir) {
            dir = getenv("HOME");
            if (!dir) {
                _puts("cd: No home directory found\n");
                return;
            }
        } else {
            _puts("cd-: no previous directory\n");
            return;
        }
    } else {
        // Handle tilde expansion
        if(dir[0] == '~') {
            char *home = getenv("HOME");
            snprintf(expanded_dir, sizeof(expanded_dir), "%s%s", home, dir+1);
            dir = expanded_dir;
        }
    }
    
    char old_cwd[PATH_MAX];
    if (getcwd(old_cwd, sizeof(old_cwd))) {
        if (chdir(dir) != 0) {
            perror("cd");
        } else {
            char new_cwd[PATH_MAX];
            if (getcwd(new_cwd, sizeof(new_cwd))) {
                setenv("PWD", new_cwd, 1);
                // Push to stack if this wasn't a cd - command
                if (args[1] && strcmp(args[1], "-") != 0) {
                    if (dir_stack_ptr < MAX_DIR_STACK - 1) {
                        dir_stack[++dir_stack_ptr] = strdup(old_cwd);
                    }
                }
            } else {
                perror("getcwd");
            }
        }
    } else {
        perror("getcwd");
    }
}

int _atoi(char *s) {
    int res = 0, sign = 1, i = 0;
    while(s[i] && !(s[i] >= '0' && s[i] <= '9')) {
        if(s[i] == '-') sign *= -1;
        i++;
    }
    while(s[i] >= '0' && s[i] <= '9') {
        res = res * 10 + (s[i++] - '0') * sign;
    }
    return res;
}

void env(char **arv __attribute__ ((unused))) {
    for(int i = 0; environ[i]; i++) {
        _puts(environ[i]);
        _puts("\n");
    }
}

void _setenv(char **arv) {
    if(!arv[1] || !arv[2]) {
        _puts("setenv: missing arguments\n");
        return;
    }
    setenv(arv[1], arv[2], 1);
}

void _unsetenv(char **arv) {
    if(!arv[1]) {
        _puts("unsetenv: missing argument\n");
        return;
    }
    unsetenv(arv[1]);
}