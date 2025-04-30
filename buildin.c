#include "shell.h"

void exitt(char **arv) {
    int status = 0;
    if(arv[1]) {
        status = _atoi(arv[1]);
        status = status < 0 ? 2 : status;
    }
    exit(status);
}

void cd_builtin(char **args) {
    char *dir = args[1];
    char expanded_dir[1024] = {0};

    if (!dir) {
        dir = getenv("HOME");
    } else {
        // Handle tilde expansion
        if(dir[0] == '~') {
            char *home = getenv("HOME");
            snprintf(expanded_dir, sizeof(expanded_dir), "%s%s", home, dir+1);
            dir = expanded_dir;
        }
    }
    
    if (chdir(dir) != 0) {
        perror("cd");
    } else {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            setenv("PWD", cwd, 1);
        } else {
            perror("getcwd");
        }
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