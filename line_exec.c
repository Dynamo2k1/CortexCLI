#include "shell.h"
#include <sys/wait.h>
#include <unistd.h>

int contains_pipes(const char *str) {
    return strchr(str, '|') != NULL;
}

char **splitstring(char *str, const char *delim) {
    char **array = NULL;
    char *token;
    int count = 0;
    char *copy = strdup(str);

    token = strtok(copy, delim);
    while(token) {
        // Trim whitespace
        while(*token == ' ' || *token == '\t') token++;
        int len = strlen(token);
        while(len > 0 && (token[len-1] == ' ' || token[len-1] == '\t')) token[--len] = '\0';
        
        if(*token) {  // Skip empty tokens
            array = realloc(array, sizeof(char*) * (count + 1));
            array[count++] = strdup(token);
        }
        token = strtok(NULL, delim);
    }
    
    if(array) {
        array = realloc(array, sizeof(char*) * (count + 1));
        array[count] = NULL;
    }
    
    free(copy);
    return array;
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

    pid_t pid = fork();
    if(pid == -1) {
        perror("fork");
        return;
    }
    
    if(pid == 0) {
        execvp(argv[0], argv);
        perror(argv[0]);
        exit(EXIT_FAILURE);
    } else {
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