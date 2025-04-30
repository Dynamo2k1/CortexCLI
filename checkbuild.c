#include "shell.h"

void(*checkbuild(char **arv))(char **) {
    mybuild builtins[] = {
        {"exit", exitt},
        {"env", env},
        {"setenv", _setenv},
        {"unsetenv", _unsetenv},
        {"history", show_history},
        {"help", show_help},
        {"cd", cd_builtin}, 
        {NULL, NULL}
    };

    for(int i = 0; builtins[i].name; i++) {
        if(strcmp(arv[0], builtins[i].name) == 0) {
            return builtins[i].func;
        }
    }
    return NULL;
}