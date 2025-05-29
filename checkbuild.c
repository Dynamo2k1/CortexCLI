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
        {NULL, NULL}
    };

    for(int i = 0; builtins[i].name; i++) {
        if(strcmp(arv[0], builtins[i].name) == 0) {
            _puts(COLOR_BLUE);
            _puts("Executing built-in: ");
            _puts(arv[0]);
            _puts("\n");
            _puts(COLOR_RESET);
            return builtins[i].func;
        }
    }
    return NULL;
}