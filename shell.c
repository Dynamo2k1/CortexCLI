#include "shell.h"

#define GEMINI_URL "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent"
#define PROMPT_PREFIX "Respond with either:\nCOMMAND: <linux command> - For executable commands\nEXPLAIN: <text> - For explanations\nQuery: "

History hist = {0}; // Global history instance

struct MemoryStruct {
    char *memory;
    size_t size;
};

/* Signal handler */
void sig_handler(int sig_num) {
    if (sig_num == SIGINT) {
        _puts("\n\033[0;91m#DYNAMO$\033[0m ");
    }
}

/* CURL write callback */
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) return 0;

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

/* Get AI command from Gemini */
char *get_ai_command(const char *input) {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk = {0};
    char *api_key = getenv("GEMINI_API_KEY");
    char *command = NULL;

    if(!api_key) {
        _puts("Error: GEMINI_API_KEY not set\n");
        return NULL;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if(curl) {
        char url[256];
        snprintf(url, sizeof(url), "%s?key=%s", GEMINI_URL, api_key);

        json_t *root = json_object();
        json_t *contents = json_array();
        json_t *content = json_object();
        json_t *parts = json_array();
        
        json_array_append_new(parts, json_pack("{s:s}", "text", PROMPT_PREFIX));
        json_array_append_new(parts, json_pack("{s:s}", "text", input));
        json_object_set_new(content, "parts", parts);
        json_array_append_new(contents, content);
        json_object_set_new(root, "contents", contents);

        char *payload = json_dumps(root, JSON_COMPACT);
        
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);

        res = curl_easy_perform(curl);
        
        if(res == CURLE_OK && chunk.memory) {
            json_error_t error;
            json_t *root = json_loads(chunk.memory, 0, &error);
            if(root) {
                json_t *candidates = json_object_get(root, "candidates");
                if(candidates && json_array_size(candidates) > 0) {
                    json_t *content = json_object_get(json_array_get(candidates, 0), "content");
                    json_t *parts = json_object_get(content, "parts");
                    if(json_array_size(parts) > 0) {
                        json_t *text = json_object_get(json_array_get(parts, 0), "text");
                        command = strdup(json_string_value(text));
                    }
                }
                json_decref(root);
            }
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        free(payload);
        free(chunk.memory);
    }
    
    curl_global_cleanup();
    return command;
}

void handle_ai_command(char *input) {
    char *clean_input = input + (input[0] == '\'' ? 1 : (strstr(input, "ai:") == input ? 3 : 0));
    clean_input[strcspn(clean_input, "\n")] = 0;
    add_history(&hist, clean_input);
    
    char *response = get_ai_command(clean_input);
    if(response) {
        if(strstr(response, "COMMAND:")) {
            char *cmd = strstr(response, "COMMAND:") + 8;
            cmd[strcspn(cmd, "\n")] = 0;
            
            if(strstr(cmd, "rm -rf") || strstr(cmd, "sudo")) {
                _puts("Security: Command blocked\n");
            } else {
                _puts("Executing: ");
                _puts(cmd);
                _puts("\n");
                char **args = splitstring(cmd, " \n");
                expand_tilde(args);
                if(contains_pipes(cmd)) {
                    char ***pipeline = parse_pipeline(cmd);
                    execute_pipeline(pipeline);
                    for(int i=0; pipeline[i]; i++) freearv(pipeline[i]);
                    free(pipeline);
                } else {
                    execute(args);
                }
                freearv(args);
            }
        }
        else if(strstr(response, "EXPLAIN:")) {
            handle_explanation(response + 8);
        }
        free(response);
    } else {
        _puts("AI request failed\n");
    }
}

void expand_tilde(char **args) {
    if(!args) return;
    for(int i=0; args[i]; i++) {
        if(args[i][0] == '~') {
            char *home = getenv("HOME");
            char *new = malloc(strlen(home) + strlen(args[i]));
            strcpy(new, home);
            strcat(new, args[i]+1);
            free(args[i]);
            args[i] = new;
        }
    }
}

void handle_explanation(const char *text) {
    _puts("\nExplanation:\n");
    char *copy = strdup(text);
    char *line = strtok(copy, "\n");
    while(line) {
        _puts("  ");
        _puts(line);
        _puts("\n");
        line = strtok(NULL, "\n");
    }
    free(copy);
}

int main(void) {
    // Initialize history
    hist.items = malloc(sizeof(char*) * MAX_HISTORY);
    hist.size = MAX_HISTORY;
    hist.count = 0;
    
    signal(SIGINT, sig_handler);
    char *buff = NULL;
    size_t size = 0;
    ssize_t len;

    while(1) {
        if(isatty(STDIN_FILENO))
            _puts("\033[0;91m#DYNAMO$\033[0m ");
        
        len = getline(&buff, &size, stdin);
        if(len == -1) {
            free(buff);
            exit(0);
        }
        buff[strcspn(buff, "\n")] = 0;

        // Handle AI commands
        if(buff[0] == '\'' || strstr(buff, "ai:") == buff) {
            handle_ai_command(buff);
            continue;
        }

        char **arv = splitstring(buff, " \n");
        if(!arv || !arv[0]) {
            freearv(arv);
            continue;
        }

        // Built-in commands
        void (*builtin_func)(char **) = checkbuild(arv);
        if(builtin_func) {
            builtin_func(arv);
            freearv(arv);
            continue;
        }

        // Handle pipes
        if(contains_pipes(buff)) {
            char ***pipeline = parse_pipeline(buff);
            execute_pipeline(pipeline);
            for(int i=0; pipeline[i]; i++) freearv(pipeline[i]);
            free(pipeline);
            freearv(arv);
            continue;
        }

        // Regular commands
        list_path *head = linkpath(_getenv("PATH"));
        char *full_path = _which(arv[0], head);
        if(full_path) {
            free(arv[0]);
            arv[0] = full_path;
            expand_tilde(arv);
            execute(arv);
        } else {
            _puts(arv[0]);
            _puts(": command not found\n");
        }
        free_list(head);
        freearv(arv);
    }
    free(buff);
    return 0;
}