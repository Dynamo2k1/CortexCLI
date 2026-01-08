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
        {"..", cd_dotdot},
        {"pushd", pushd_builtin},
        {"popd", popd_builtin},
        {"dirs", dirs_builtin},
        {"-", cd_minus},
        {"ai", ai_builtin},
        {"sandbox", sandbox_builtin},
        {"audit", audit_builtin},
        {NULL, NULL}
    };

    for(int i = 0; builtins[i].name; i++) {
        if(strcmp(arv[0], builtins[i].name) == 0) {
            return builtins[i].func;
        }
    }
    return NULL;
}