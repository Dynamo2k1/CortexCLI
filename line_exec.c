#include "shell.h"
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h> 
#include <string.h>

int contains_pipes(const char *str) {
    return strchr(str, '|') != NULL;
}

static char **add_token(char **array, int *count, char *token) {
    array = realloc(array, sizeof(char*) * (*count + 2));
    array[*count] = strdup(token);
    (*count)++;
    return array;
}

char **splitstring(char *str, const char *delim) {
    char **array = NULL;
    int count = 0;
    char *copy = strdup(str);
    char *token = strtok(copy, delim);

    while(token) {
        // Handle quoted arguments
        if(token[0] == '"' || token[0] == '\'') {
            char quote = token[0];
            char *end = strchr(token+1, quote);
            if(end) {
                *end = '\0';
                array = add_token(array, &count, token+1);
                token = end + 1;
                continue;
            }
        }

        // Handle special characters
        if(strchr(token, '>') || strchr(token, '|')) {
            char *ptr = token;
            while(*ptr) {
                if(*ptr == '>' || *ptr == '|') {
                    char sep[2] = {*ptr, '\0'};
                    *ptr = '\0';
                    if(ptr > token && *token) {
                        array = add_token(array, &count, token);
                    }
                    array = add_token(array, &count, sep);
                    token = ptr + 1;
                    ptr = token;
                } else {
                    ptr++;
                }
            }
        }
        
        if(token && *token) {
            array = add_token(array, &count, token);
        }
        token = strtok(NULL, delim);
    }

    array = realloc(array, sizeof(char*) * (count + 1));
    array[count] = NULL;
    free(copy);
    return array;
}

int contains_semicolons(const char *str) {
    return strchr(str, ';') != NULL;
}

void execute_sequence(char *input) {
    char **commands = splitstring(input, ";");
    for(int i=0; commands[i]; i++) {
        char **args = splitstring(commands[i], " \t\n");
        execute(args);
        freearv(args);
    }
    freearv(commands);
}

void execute(char **argv) {
    if(!argv || !argv[0]) return;
    
	if(contains_pipes(argv[0])) {
        char ***pipeline = parse_pipeline(argv[0]);
        execute_pipeline(pipeline);
        for(int i=0; pipeline[i]; i++) freearv(pipeline[i]);
        free(pipeline);
        return;
    }

	int out_fd = -1;
    for(int i=0; argv[i]; i++) {
        if(strcmp(argv[i], ">") == 0 && argv[i+1]) {
            out_fd = open(argv[i+1], O_WRONLY|O_CREAT|O_TRUNC, 0644);
            argv[i] = NULL;
            break;
        }
    }

    pid_t pid = fork();
    if(pid == -1) {
        perror("fork");
        return;
    }
    
    if(pid == 0) {
        if(out_fd != -1) {
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }
        execvp(argv[0], argv);
        perror(argv[0]);
        exit(EXIT_FAILURE);
    }else {
        wait(NULL);
    }
}

void execute_pipeline(char ***commands) {
    int fds[2];
    int prev_pipe = 0;
    pid_t pid;
    int i;

    for(i = 0; commands[i+1] != NULL; i++) {
        if(pipe(fds) == -1) {
            perror("pipe");
            return;
        }

        pid = fork();
        if(pid == -1) {
            perror("fork");
            return;
        }

        if(pid == 0) { // Child process
            if(prev_pipe != 0) {
                dup2(prev_pipe, STDIN_FILENO);
                close(prev_pipe);
            }
            dup2(fds[1], STDOUT_FILENO);
            close(fds[0]);
            close(fds[1]);

            execvp(commands[i][0], commands[i]);
            perror(commands[i][0]);
            exit(EXIT_FAILURE);
        } else { // Parent process
            close(fds[1]);
            if(prev_pipe != 0) close(prev_pipe);
            prev_pipe = fds[0];
        }
    }

    // Last command
    pid = fork();
    if(pid == 0) {
        dup2(prev_pipe, STDIN_FILENO);
        close(prev_pipe);
        execvp(commands[i][0], commands[i]);
        perror(commands[i][0]);
        exit(EXIT_FAILURE);
    } else {
        close(prev_pipe);
        while(wait(NULL) > 0);
    }
}

char ***parse_pipeline(char *input) {
    char **pipes = splitstring(input, "|");
    int num_commands = 0;
    while(pipes[num_commands]) num_commands++;

    char ***commands = malloc(sizeof(char**) * (num_commands + 1));
    for(int i=0; i<num_commands; i++) {
        commands[i] = splitstring(pipes[i], " \t\n");
    }
    commands[num_commands] = NULL;
    freearv(pipes);
    return commands;
}



void *_realloc(void *ptr, unsigned int old_size, unsigned int new_size) {
    if(new_size == 0) {
        free(ptr);
        return NULL;
    }
    
    void *new_ptr = malloc(new_size);
    if(!new_ptr) return NULL;
    
    if(ptr) {
        memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
        free(ptr);
    }
    
    return new_ptr;
}

void freearv(char **arv) {
    if(!arv) return;
    for(int i = 0; arv[i]; i++) free(arv[i]);
    free(arv);
}