#include "shell.h"

char *_getenv(const char *name) {
    size_t len = strlen(name);
    for(int i = 0; environ[i]; i++) {
        if(strncmp(name, environ[i], len) == 0 && environ[i][len] == '=')
            return &environ[i][len+1];
    }
    return NULL;
}

list_path *add_node_end(list_path **head, char *str) {
    list_path *new = malloc(sizeof(list_path));
    if(!new) return NULL;
    
    new->dir = strdup(str);
    new->p = NULL;
    
    if(!*head) {
        *head = new;
    } else {
        list_path *tmp = *head;
        while(tmp->p) tmp = tmp->p;
        tmp->p = new;
    }
    return *head;
}

list_path *linkpath(char *path) {
    list_path *head = NULL;
    char *copy = strdup(path);
    char *token = strtok(copy, ":");
    
    while(token) {
        add_node_end(&head, token);
        token = strtok(NULL, ":");
    }
    
    free(copy);
    return head;
}

char *_which(char *filename, list_path *head) {
    struct stat st;
    char *path;
    
    while(head) {
        path = malloc(strlen(head->dir) + strlen(filename) + 2);
        sprintf(path, "%s/%s", head->dir, filename);
        
        if(stat(path, &st) == 0) {
            return path;
        }
        
        free(path);
        head = head->p;
    }
    return NULL;
}

void free_list(list_path *head) {
    while(head) {
        list_path *next = head->p;
        free(head->dir);
        free(head);
        head = next;
    }
}