#include "shell.h"
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h> 

int contains_pipes(const char *str) {
    return strchr(str, '|') != NULL;
}

char **splitstring(char *str, const char *delim) {
    char **array = NULL;
    int count = 0;
    char *copy = strdup(str);
    
    // Use proper delimiters and handle quotes
    char *token = strtok(copy, " \t\n");
    while(token) {
        // Handle redirect symbol splitting
        if(strchr(token, '>')) {
            char *ptr = token;
            while(*ptr) {
                if(*ptr == '>') {
                    // Split before '>'
                    *ptr = '\0';
                    if(ptr > token && *token != '\0') {
                        array = realloc(array, sizeof(char*) * (count + 1));
                        array[count++] = strdup(token);
                    }
                    // Add '>' itself
                    array = realloc(array, sizeof(char*) * (count + 1));
                    array[count++] = strdup(">");
                    token = ptr + 1;
                    ptr = token;
                } else {
                    ptr++;
                }
            }
            if(*token != '\0') {
                array = realloc(array, sizeof(char*) * (count + 1));
                array[count++] = strdup(token);
            }
        } else {
            // Normal token handling
            if(*token != '\0') {
                array = realloc(array, sizeof(char*) * (count + 1));
                array[count++] = strdup(token);
            }
        }
        token = strtok(NULL, " \t\n");
    }
    
    // Null-terminate array
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
    int prev_pipe_in = 0;
    int i = 0;

    while(commands[i+1]) {
        pipe(fds);
        
        pid_t pid = fork();
        if(pid == 0) {
            if(prev_pipe_in != 0) {
                dup2(prev_pipe_in, STDIN_FILENO);
                close(prev_pipe_in);
            }
            dup2(fds[1], STDOUT_FILENO);
            close(fds[0]);
            close(fds[1]);
            
            execvp(commands[i][0], commands[i]);
            perror(commands[i][0]);
            exit(EXIT_FAILURE);
        }
        else {
            wait(NULL);
            close(fds[1]);
            prev_pipe_in = fds[0];
        }
        i++;
    }

    // Last command
    pid_t pid = fork();
    if(pid == 0) {
        if(prev_pipe_in != 0) {
            dup2(prev_pipe_in, STDIN_FILENO);
            close(prev_pipe_in);
        }
        execvp(commands[i][0], commands[i]);
        perror(commands[i][0]);
        exit(EXIT_FAILURE);
    }
    else {
        close(prev_pipe_in);
        wait(NULL);
    }
}

char ***parse_pipeline(char *input) {
    int num_commands = 1;
    char **pipes = splitstring(input, "|");
    for(int i=0; pipes[i]; i++) num_commands++;
    
    char ***commands = malloc(sizeof(char**) * num_commands);
    for(int i=0; pipes[i]; i++) {
        commands[i] = splitstring(pipes[i], " \n\t");
    }
    commands[num_commands-1] = NULL;
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