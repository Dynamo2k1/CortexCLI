#include "ai_backend.h"
#include "shell.h"
#include <curl/curl.h>
#include <jansson.h>
#include <string.h>
#include <stdlib.h>

/* Forward declarations for functions used in init */
static int ollama_check_available_internal(void);
static OllamaModelList *ollama_list_models_internal(void);
static void ollama_model_list_free_internal(OllamaModelList *list);

/* Backend configurations */
static AIBackendConfig backends[AI_BACKEND_COUNT] = {
    {AI_BACKEND_GEMINI, "gemini", "GEMINI_API_KEY", "gemini-2.0-flash",
     "https://generativelanguage.googleapis.com/v1beta/models/", 0},
    {AI_BACKEND_OPENAI, "openai", "OPENAI_API_KEY", "gpt-4o-mini",
     "https://api.openai.com/v1/chat/completions", 0},
    {AI_BACKEND_CLAUDE, "claude", "ANTHROPIC_API_KEY", "claude-3-haiku-20240307",
     "https://api.anthropic.com/v1/messages", 0},
    {AI_BACKEND_DEEPSEEK, "deepseek", "DEEPSEEK_API_KEY", "deepseek-chat",
     "https://api.deepseek.com/v1/chat/completions", 0},
    {AI_BACKEND_OLLAMA, "ollama", "OLLAMA_HOST", "llama3.2",
     "http://localhost:11434/api/generate", 0}
};

static AIBackendType active_backend = AI_BACKEND_GEMINI;
static char current_model[256] = {0};

/* CURL memory struct for response */
struct MemoryChunk {
    char *memory;
    size_t size;
};

/* CURL write callback */
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryChunk *mem = (struct MemoryChunk *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

/* Internal helper: Check if Ollama is available */
static int ollama_check_available_internal(void) {
    CURL *curl;
    CURLcode res;
    struct MemoryChunk chunk = {0};
    int available = 0;
    
    char *host = getenv("OLLAMA_HOST");
    if (!host) host = "http://localhost:11434";
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (curl) {
        char url[256];
        snprintf(url, sizeof(url), "%s/api/tags", host);
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK && chunk.memory) {
            available = 1;
        }
        
        curl_easy_cleanup(curl);
        free(chunk.memory);
    }
    
    curl_global_cleanup();
    return available;
}

/* Internal helper: Determine model capabilities from name */
static int get_model_capabilities_internal(const char *model_name) {
    int caps = MODEL_CAP_GENERAL;
    
    if (!model_name) return caps;
    
    /* Code-specialized models */
    if (strstr(model_name, "coder") || strstr(model_name, "code") ||
        strstr(model_name, "deepseek-coder") || strstr(model_name, "codellama") ||
        strstr(model_name, "starcoder") || strstr(model_name, "wizardcoder")) {
        caps |= MODEL_CAP_CODE | MODEL_CAP_SHELL;
    }
    
    /* Reasoning models */
    if (strstr(model_name, "llama") || strstr(model_name, "mistral") ||
        strstr(model_name, "gemma") || strstr(model_name, "phi")) {
        caps |= MODEL_CAP_REASONING;
    }
    
    /* Fast/small models */
    if (strstr(model_name, "tiny") || strstr(model_name, "small") ||
        strstr(model_name, "mini") || strstr(model_name, "1b") ||
        strstr(model_name, "3b")) {
        caps |= MODEL_CAP_FAST;
    }
    
    return caps;
}

/* Internal helper: List Ollama models */
static OllamaModelList *ollama_list_models_internal(void) {
    OllamaModelList *list = calloc(1, sizeof(OllamaModelList));
    if (!list) return NULL;
    
    CURL *curl;
    CURLcode res;
    struct MemoryChunk chunk = {0};
    
    char *host = getenv("OLLAMA_HOST");
    if (!host) host = "http://localhost:11434";
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (curl) {
        char url[256];
        snprintf(url, sizeof(url), "%s/api/tags", host);
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK && chunk.memory) {
            json_error_t error;
            json_t *root = json_loads(chunk.memory, 0, &error);
            
            if (root) {
                json_t *models = json_object_get(root, "models");
                if (models && json_is_array(models)) {
                    size_t model_count = json_array_size(models);
                    list->models = calloc(model_count, sizeof(OllamaModel));
                    list->count = (int)model_count;
                    
                    for (size_t i = 0; i < model_count; i++) {
                        json_t *model = json_array_get(models, i);
                        json_t *name = json_object_get(model, "name");
                        json_t *modified = json_object_get(model, "modified_at");
                        json_t *size = json_object_get(model, "size");
                        
                        if (name && json_is_string(name)) {
                            list->models[i].name = strdup(json_string_value(name));
                            list->models[i].capabilities = get_model_capabilities_internal(
                                list->models[i].name);
                        }
                        if (modified && json_is_string(modified)) {
                            list->models[i].modified_at = strdup(json_string_value(modified));
                        }
                        if (size && json_is_integer(size)) {
                            list->models[i].size = (long)json_integer_value(size);
                        }
                    }
                }
                json_decref(root);
            }
        }
        
        curl_easy_cleanup(curl);
        free(chunk.memory);
    }
    
    curl_global_cleanup();
    return list;
}

/* Internal helper: Free model list */
static void ollama_model_list_free_internal(OllamaModelList *list) {
    if (!list) return;
    
    for (int i = 0; i < list->count; i++) {
        free(list->models[i].name);
        free(list->models[i].modified_at);
    }
    free(list->models);
    free(list);
}

/* Initialize backends */
void ai_backend_init(void) {
    for (int i = 0; i < AI_BACKEND_COUNT; i++) {
        char *key = getenv(backends[i].env_key);
        backends[i].enabled = (key != NULL && strlen(key) > 0);
    }
    
    /* Special check for Ollama - it's enabled if the server is running */
    if (!backends[AI_BACKEND_OLLAMA].enabled) {
        /* Check if Ollama is available even without OLLAMA_HOST set */
        if (ollama_check_available_internal()) {
            backends[AI_BACKEND_OLLAMA].enabled = 1;
        }
    }
    
    /* Set active backend to first available */
    for (int i = 0; i < AI_BACKEND_COUNT; i++) {
        if (backends[i].enabled) {
            active_backend = (AIBackendType)i;
            break;
        }
    }
    
    /* Set default model */
    strncpy(current_model, backends[active_backend].default_model, 
            sizeof(current_model) - 1);
    current_model[sizeof(current_model) - 1] = '\0';
}

void ai_backend_cleanup(void) {
    /* Cleanup resources if needed */
}

AIBackendType ai_get_active_backend(void) {
    return active_backend;
}

int ai_set_backend(AIBackendType type) {
    if (type < 0 || type >= AI_BACKEND_COUNT) return -1;
    if (!backends[type].enabled) return -1;
    
    active_backend = type;
    strncpy(current_model, backends[type].default_model, sizeof(current_model) - 1);
    return 0;
}

int ai_set_backend_by_name(const char *name) {
    for (int i = 0; i < AI_BACKEND_COUNT; i++) {
        if (strcasecmp(backends[i].name, name) == 0) {
            return ai_set_backend((AIBackendType)i);
        }
    }
    return -1;
}

const char *ai_get_backend_name(AIBackendType type) {
    if (type < 0 || type >= AI_BACKEND_COUNT) return "unknown";
    return backends[type].name;
}

int ai_backend_available(AIBackendType type) {
    if (type < 0 || type >= AI_BACKEND_COUNT) return 0;
    return backends[type].enabled;
}

AIBackendType ai_get_fallback_backend(void) {
    for (int i = 0; i < AI_BACKEND_COUNT; i++) {
        if (backends[i].enabled && i != (int)active_backend) {
            return (AIBackendType)i;
        }
    }
    return active_backend;
}

const char *ai_get_model(void) {
    return current_model;
}

void ai_set_model(const char *model) {
    if (model) {
        strncpy(current_model, model, sizeof(current_model) - 1);
    }
}

void ai_list_backends(void) {
    _puts("\nAvailable AI Backends:\n");
    for (int i = 0; i < AI_BACKEND_COUNT; i++) {
        _puts("  ");
        if (i == (int)active_backend) {
            _puts(COLOR_GREEN);
            _puts("* ");
        } else {
            _puts("  ");
        }
        _puts(backends[i].name);
        _puts(": ");
        if (backends[i].enabled) {
            _puts(COLOR_GREEN);
            _puts("available");
        } else {
            _puts(COLOR_RED);
            _puts("not configured (set ");
            _puts(backends[i].env_key);
            _puts(")");
        }
        _puts(COLOR_RESET);
        _puts("\n");
    }
}

/* Task type detection based on input */
TaskType ai_detect_task_type(const char *input) {
    if (!input) return TASK_GENERAL;
    
    /* Check for explicit prefixes */
    if (strncasecmp(input, "explain:", 8) == 0 || 
        strstr(input, "explain ") || strstr(input, "what is") ||
        strstr(input, "how does") || strstr(input, "why ")) {
        return TASK_EXPLANATION;
    }
    
    /* Code generation indicators */
    if (strstr(input, "write code") || strstr(input, "generate code") ||
        strstr(input, "create a function") || strstr(input, "implement") ||
        strstr(input, "python script") || strstr(input, "javascript") ||
        strstr(input, "program") || strstr(input, "algorithm") ||
        strstr(input, ".py") || strstr(input, ".js") || strstr(input, ".c") ||
        strstr(input, "class ") || strstr(input, "function ") ||
        strstr(input, "def ") || strstr(input, "const ")) {
        return TASK_CODE_GENERATION;
    }
    
    /* Shell command indicators */
    if (strstr(input, "run ") || strstr(input, "execute") ||
        strstr(input, "command to") || strstr(input, "terminal") ||
        strstr(input, "shell") || strstr(input, "bash") ||
        strstr(input, "list files") || strstr(input, "find files") ||
        strstr(input, "delete") || strstr(input, "move") ||
        strstr(input, "copy") || strstr(input, "permissions")) {
        return TASK_SHELL_COMMAND;
    }
    
    /* Automation indicators */
    if (strstr(input, "automate") || strstr(input, "schedule") ||
        strstr(input, "cron") || strstr(input, "batch") ||
        strstr(input, "workflow") || strstr(input, "pipeline") ||
        strstr(input, "parallel") || strstr(input, "distributed")) {
        return TASK_AUTOMATION;
    }
    
    return TASK_GENERAL;
}

const char *ai_get_task_type_name(TaskType type) {
    switch (type) {
        case TASK_CODE_GENERATION: return "Code Generation";
        case TASK_SHELL_COMMAND: return "Shell Command";
        case TASK_AUTOMATION: return "Automation";
        case TASK_EXPLANATION: return "Explanation";
        default: return "General";
    }
}

/* Check if Ollama is available - public API */
int ai_ollama_check_available(void) {
    return ollama_check_available_internal();
}

/* List Ollama models - public API */
OllamaModelList *ai_ollama_list_models(void) {
    return ollama_list_models_internal();
}

void ai_ollama_model_list_free(OllamaModelList *list) {
    ollama_model_list_free_internal(list);
}

/* Select best Ollama model for task */
const char *ai_ollama_select_best_model(TaskType task) {
    OllamaModelList *list = ai_ollama_list_models();
    if (!list || list->count == 0) {
        ai_ollama_model_list_free(list);
        return "llama3.2";  /* Default fallback */
    }
    
    const char *best = NULL;
    int best_score = -1;
    
    for (int i = 0; i < list->count; i++) {
        int score = 0;
        int caps = list->models[i].capabilities;
        
        switch (task) {
            case TASK_CODE_GENERATION:
            case TASK_SHELL_COMMAND:
            case TASK_AUTOMATION:
                /* Prefer code-specialized models */
                if (caps & MODEL_CAP_CODE) score += 10;
                if (caps & MODEL_CAP_SHELL) score += 5;
                break;
            case TASK_EXPLANATION:
            case TASK_GENERAL:
            default:
                /* Prefer reasoning models */
                if (caps & MODEL_CAP_REASONING) score += 10;
                if (caps & MODEL_CAP_GENERAL) score += 5;
                break;
        }
        
        if (score > best_score) {
            best_score = score;
            best = list->models[i].name;
        }
    }
    
    /* Copy best model name before freeing list */
    static char selected_model[256];
    if (best) {
        strncpy(selected_model, best, sizeof(selected_model) - 1);
        selected_model[sizeof(selected_model) - 1] = '\0';
    } else if (list->count > 0) {
        strncpy(selected_model, list->models[0].name, sizeof(selected_model) - 1);
        selected_model[sizeof(selected_model) - 1] = '\0';
    } else {
        strncpy(selected_model, "llama3.2", sizeof(selected_model) - 1);
    }
    
    ai_ollama_model_list_free(list);
    return selected_model;
}

/* List Ollama models to user */
void ai_list_ollama_models(void) {
    _puts("\n");
    _puts(COLOR_CYAN);
    _puts("Ollama Models:\n");
    _puts(COLOR_RESET);
    
    if (!ai_ollama_check_available()) {
        _puts(COLOR_RED);
        _puts("  Ollama is not running or not available.\n");
        _puts("  Start Ollama with: ollama serve\n");
        _puts(COLOR_RESET);
        return;
    }
    
    OllamaModelList *list = ai_ollama_list_models();
    if (!list || list->count == 0) {
        _puts(COLOR_YELLOW);
        _puts("  No models installed. Install with: ollama pull <model>\n");
        _puts(COLOR_RESET);
        ai_ollama_model_list_free(list);
        return;
    }
    
    for (int i = 0; i < list->count; i++) {
        _puts("  ");
        
        /* Check if this is the current model */
        if (active_backend == AI_BACKEND_OLLAMA && 
            strcmp(current_model, list->models[i].name) == 0) {
            _puts(COLOR_GREEN);
            _puts("* ");
        } else {
            _puts("  ");
        }
        
        _puts(list->models[i].name);
        
        /* Show capabilities */
        int caps = list->models[i].capabilities;
        if (caps & MODEL_CAP_CODE) {
            _puts(COLOR_BLUE);
            _puts(" [code]");
        }
        if (caps & MODEL_CAP_REASONING) {
            _puts(COLOR_MAGENTA);
            _puts(" [reasoning]");
        }
        if (caps & MODEL_CAP_FAST) {
            _puts(COLOR_YELLOW);
            _puts(" [fast]");
        }
        
        /* Show size */
        if (list->models[i].size > 0) {
            char size_str[32];
            double size_gb = (double)list->models[i].size / (1024.0 * 1024.0 * 1024.0);
            snprintf(size_str, sizeof(size_str), " (%.1fGB)", size_gb);
            _puts(COLOR_WHITE);
            _puts(size_str);
        }
        
        _puts(COLOR_RESET);
        _puts("\n");
    }
    
    ai_ollama_model_list_free(list);
}

/* Get recommended model for backend and task */
const char *ai_get_recommended_model(AIBackendType backend, TaskType task) {
    switch (backend) {
        case AI_BACKEND_GEMINI:
            switch (task) {
                case TASK_CODE_GENERATION:
                case TASK_SHELL_COMMAND:
                case TASK_AUTOMATION:
                    return "gemini-2.0-flash";
                default:
                    return "gemini-2.0-flash";
            }
        case AI_BACKEND_OPENAI:
            switch (task) {
                case TASK_CODE_GENERATION:
                case TASK_SHELL_COMMAND:
                case TASK_AUTOMATION:
                    return "gpt-4o";
                case TASK_EXPLANATION:
                    return "gpt-4o";
                default:
                    return "gpt-4o-mini";
            }
        case AI_BACKEND_CLAUDE:
            switch (task) {
                case TASK_CODE_GENERATION:
                case TASK_AUTOMATION:
                    return "claude-3-5-sonnet-20241022";
                default:
                    return "claude-3-haiku-20240307";
            }
        case AI_BACKEND_DEEPSEEK:
            switch (task) {
                case TASK_CODE_GENERATION:
                case TASK_SHELL_COMMAND:
                case TASK_AUTOMATION:
                    return "deepseek-coder";
                default:
                    return "deepseek-chat";
            }
        case AI_BACKEND_OLLAMA:
            return ai_ollama_select_best_model(task);
        default:
            return "default";
    }
}

/* Auto-select the best model for the task */
void ai_auto_select_model(TaskType task) {
    const char *recommended = ai_get_recommended_model(active_backend, task);
    if (recommended) {
        strncpy(current_model, recommended, sizeof(current_model) - 1);
        current_model[sizeof(current_model) - 1] = '\0';
    }
}

/* Get optimized system prompt for task type */
const char *ai_get_optimized_prompt(TaskType task) {
    switch (task) {
        case TASK_CODE_GENERATION:
            return "You are an expert code generator. Respond ONLY with code. "
                   "Use COMMAND: prefix for shell commands, or output pure code blocks. "
                   "No explanations unless prefixed with EXPLAIN:. "
                   "Generate complete, production-ready code on the first try. "
                   "Include minimal comments only when necessary for clarity.";
        case TASK_SHELL_COMMAND:
            return "You are a Linux shell expert. Respond with COMMAND: prefix for "
                   "each shell command. Output exact commands ready to execute. "
                   "No explanations unless explicitly requested with EXPLAIN: prefix. "
                   "Prefer standard tools (grep, find, sed, awk). "
                   "Handle errors gracefully. Use pipes and redirects efficiently.";
        case TASK_AUTOMATION:
            return "You are an automation expert. Generate scripts prefixed with COMMAND: "
                   "for executable commands. Create robust shell scripts, cron jobs, "
                   "or workflow automation. Include error handling. "
                   "Use EXPLAIN: prefix only when explanation is requested.";
        case TASK_EXPLANATION:
            return "You are a helpful assistant. Use EXPLAIN: prefix for all explanations. "
                   "Be concise but thorough. Include examples when helpful. "
                   "If commands are relevant, include them with COMMAND: prefix.";
        default:
            return "You are CortexCLI, an AI shell assistant. "
                   "Use COMMAND: for shell commands, EXPLAIN: for explanations. "
                   "Be concise. Generate correct syntax on the first try.";
    }
}

/* Query Gemini */
static AIResponse *query_gemini(const char *prompt, const char *context) {
    AIResponse *response = calloc(1, sizeof(AIResponse));
    CURL *curl;
    CURLcode res;
    struct MemoryChunk chunk = {0};
    char *api_key = getenv("GEMINI_API_KEY");
    
    if (!api_key) {
        response->success = 0;
        response->error_message = strdup("GEMINI_API_KEY not set");
        return response;
    }
    
    /* Build full prompt with context */
    size_t full_len = strlen(prompt) + (context ? strlen(context) : 0) + 100;
    char *full_prompt = malloc(full_len);
    if (context && strlen(context) > 0) {
        snprintf(full_prompt, full_len, "%s\n\nUser Query: %s", context, prompt);
    } else {
        strncpy(full_prompt, prompt, full_len - 1);
    }
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (curl) {
        char url[512];
        snprintf(url, sizeof(url), "%s%s:generateContent?key=%s",
                 backends[AI_BACKEND_GEMINI].api_url, current_model, api_key);
        
        json_t *root = json_object();
        json_t *contents = json_array();
        json_t *content = json_object();
        json_t *parts = json_array();
        
        json_array_append_new(parts, json_pack("{s:s}", "text", full_prompt));
        json_object_set_new(content, "parts", parts);
        json_array_append_new(contents, content);
        json_object_set_new(root, "contents", contents);
        
        char *payload = json_dumps(root, JSON_COMPACT);
        
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK && chunk.memory) {
            json_error_t error;
            json_t *resp_root = json_loads(chunk.memory, 0, &error);
            if (resp_root) {
                /* Check for API error response first */
                json_t *api_error = json_object_get(resp_root, "error");
                if (api_error) {
                    json_t *err_msg = json_object_get(api_error, "message");
                    if (err_msg && json_is_string(err_msg)) {
                        response->success = 0;
                        response->error_message = strdup(json_string_value(err_msg));
                    } else {
                        response->success = 0;
                        response->error_message = strdup("Unknown API error");
                    }
                } else {
                    json_t *candidates = json_object_get(resp_root, "candidates");
                    if (candidates && json_array_size(candidates) > 0) {
                        json_t *first = json_array_get(candidates, 0);
                        json_t *content = first ? json_object_get(first, "content") : NULL;
                        json_t *parts = content ? json_object_get(content, "parts") : NULL;
                        if (parts && json_array_size(parts) > 0) {
                            json_t *first_part = json_array_get(parts, 0);
                            json_t *text = first_part ? json_object_get(first_part, "text") : NULL;
                            if (text && json_is_string(text)) {
                                response->content = strdup(json_string_value(text));
                                response->success = 1;
                            } else {
                                response->success = 0;
                                response->error_message = strdup("Invalid response format from Gemini");
                            }
                        } else {
                            response->success = 0;
                            response->error_message = strdup("Empty response from Gemini");
                        }
                    } else {
                        response->success = 0;
                        response->error_message = strdup("No candidates in Gemini response");
                    }
                }
                json_decref(resp_root);
            } else {
                response->success = 0;
                response->error_message = strdup("Failed to parse Gemini response");
            }
        } else {
            response->success = 0;
            response->error_message = strdup(curl_easy_strerror(res));
        }
        
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        free(payload);
        free(chunk.memory);
        json_decref(root);
    }
    
    curl_global_cleanup();
    free(full_prompt);
    return response;
}

/* Query OpenAI */
static AIResponse *query_openai(const char *prompt, const char *context) {
    AIResponse *response = calloc(1, sizeof(AIResponse));
    CURL *curl;
    CURLcode res;
    struct MemoryChunk chunk = {0};
    char *api_key = getenv("OPENAI_API_KEY");
    
    if (!api_key) {
        response->success = 0;
        response->error_message = strdup("OPENAI_API_KEY not set");
        return response;
    }
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (curl) {
        json_t *root = json_object();
        json_t *messages = json_array();
        
        /* System message with context */
        if (context && strlen(context) > 0) {
            json_array_append_new(messages, 
                json_pack("{s:s, s:s}", "role", "system", "content", context));
        }
        
        /* User message */
        json_array_append_new(messages,
            json_pack("{s:s, s:s}", "role", "user", "content", prompt));
        
        json_object_set_new(root, "model", json_string(current_model));
        json_object_set_new(root, "messages", messages);
        json_object_set_new(root, "max_tokens", json_integer(2048));
        
        char *payload = json_dumps(root, JSON_COMPACT);
        
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        char auth_header[256];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);
        headers = curl_slist_append(headers, auth_header);
        
        curl_easy_setopt(curl, CURLOPT_URL, backends[AI_BACKEND_OPENAI].api_url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK && chunk.memory) {
            json_error_t error;
            json_t *resp_root = json_loads(chunk.memory, 0, &error);
            if (resp_root) {
                /* Check for API error response first */
                json_t *api_error = json_object_get(resp_root, "error");
                if (api_error) {
                    json_t *err_msg = json_object_get(api_error, "message");
                    if (err_msg && json_is_string(err_msg)) {
                        response->success = 0;
                        response->error_message = strdup(json_string_value(err_msg));
                    } else {
                        response->success = 0;
                        response->error_message = strdup("Unknown OpenAI API error");
                    }
                } else {
                    json_t *choices = json_object_get(resp_root, "choices");
                    if (choices && json_array_size(choices) > 0) {
                        json_t *first = json_array_get(choices, 0);
                        json_t *message = first ? json_object_get(first, "message") : NULL;
                        json_t *content = message ? json_object_get(message, "content") : NULL;
                        if (content && json_is_string(content)) {
                            response->content = strdup(json_string_value(content));
                            response->success = 1;
                        } else {
                            response->success = 0;
                            response->error_message = strdup("Invalid response format from OpenAI");
                        }
                    } else {
                        response->success = 0;
                        response->error_message = strdup("No choices in OpenAI response");
                    }
                }
                json_decref(resp_root);
            } else {
                response->success = 0;
                response->error_message = strdup("Failed to parse OpenAI response");
            }
        } else {
            response->success = 0;
            response->error_message = strdup(curl_easy_strerror(res));
        }
        
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        free(payload);
        free(chunk.memory);
        json_decref(root);
    }
    
    curl_global_cleanup();
    return response;
}

/* Query Anthropic Claude */
static AIResponse *query_claude(const char *prompt, const char *context) {
    AIResponse *response = calloc(1, sizeof(AIResponse));
    CURL *curl;
    CURLcode res;
    struct MemoryChunk chunk = {0};
    char *api_key = getenv("ANTHROPIC_API_KEY");
    
    if (!api_key) {
        response->success = 0;
        response->error_message = strdup("ANTHROPIC_API_KEY not set");
        return response;
    }
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (curl) {
        json_t *root = json_object();
        json_t *messages = json_array();
        
        /* User message */
        json_array_append_new(messages,
            json_pack("{s:s, s:s}", "role", "user", "content", prompt));
        
        json_object_set_new(root, "model", json_string(current_model));
        json_object_set_new(root, "messages", messages);
        json_object_set_new(root, "max_tokens", json_integer(2048));
        
        if (context && strlen(context) > 0) {
            json_object_set_new(root, "system", json_string(context));
        }
        
        char *payload = json_dumps(root, JSON_COMPACT);
        
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        char auth_header[256];
        snprintf(auth_header, sizeof(auth_header), "x-api-key: %s", api_key);
        headers = curl_slist_append(headers, auth_header);
        headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
        
        curl_easy_setopt(curl, CURLOPT_URL, backends[AI_BACKEND_CLAUDE].api_url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK && chunk.memory) {
            json_error_t error;
            json_t *resp_root = json_loads(chunk.memory, 0, &error);
            if (resp_root) {
                /* Check for API error response first */
                json_t *api_error = json_object_get(resp_root, "error");
                if (api_error) {
                    json_t *err_msg = json_object_get(api_error, "message");
                    if (err_msg && json_is_string(err_msg)) {
                        response->success = 0;
                        response->error_message = strdup(json_string_value(err_msg));
                    } else {
                        response->success = 0;
                        response->error_message = strdup("Unknown Claude API error");
                    }
                } else {
                    json_t *content_arr = json_object_get(resp_root, "content");
                    if (content_arr && json_array_size(content_arr) > 0) {
                        json_t *first = json_array_get(content_arr, 0);
                        json_t *text = first ? json_object_get(first, "text") : NULL;
                        if (text && json_is_string(text)) {
                            response->content = strdup(json_string_value(text));
                            response->success = 1;
                        } else {
                            response->success = 0;
                            response->error_message = strdup("Invalid response format from Claude");
                        }
                    } else {
                        response->success = 0;
                        response->error_message = strdup("No content in Claude response");
                    }
                }
                json_decref(resp_root);
            } else {
                response->success = 0;
                response->error_message = strdup("Failed to parse Claude response");
            }
        } else {
            response->success = 0;
            response->error_message = strdup(curl_easy_strerror(res));
        }
        
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        free(payload);
        free(chunk.memory);
        json_decref(root);
    }
    
    curl_global_cleanup();
    return response;
}

/* Query DeepSeek (same API format as OpenAI) */
static AIResponse *query_deepseek(const char *prompt, const char *context) {
    AIResponse *response = calloc(1, sizeof(AIResponse));
    CURL *curl;
    CURLcode res;
    struct MemoryChunk chunk = {0};
    char *api_key = getenv("DEEPSEEK_API_KEY");
    
    if (!api_key) {
        response->success = 0;
        response->error_message = strdup("DEEPSEEK_API_KEY not set");
        return response;
    }
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (curl) {
        json_t *root = json_object();
        json_t *messages = json_array();
        
        if (context && strlen(context) > 0) {
            json_array_append_new(messages,
                json_pack("{s:s, s:s}", "role", "system", "content", context));
        }
        
        json_array_append_new(messages,
            json_pack("{s:s, s:s}", "role", "user", "content", prompt));
        
        json_object_set_new(root, "model", json_string(current_model));
        json_object_set_new(root, "messages", messages);
        json_object_set_new(root, "max_tokens", json_integer(2048));
        
        char *payload = json_dumps(root, JSON_COMPACT);
        
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        char auth_header[256];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);
        headers = curl_slist_append(headers, auth_header);
        
        curl_easy_setopt(curl, CURLOPT_URL, backends[AI_BACKEND_DEEPSEEK].api_url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK && chunk.memory) {
            json_error_t error;
            json_t *resp_root = json_loads(chunk.memory, 0, &error);
            if (resp_root) {
                /* Check for API error response first */
                json_t *api_error = json_object_get(resp_root, "error");
                if (api_error) {
                    json_t *err_msg = json_object_get(api_error, "message");
                    if (err_msg && json_is_string(err_msg)) {
                        response->success = 0;
                        response->error_message = strdup(json_string_value(err_msg));
                    } else {
                        response->success = 0;
                        response->error_message = strdup("Unknown DeepSeek API error");
                    }
                } else {
                    json_t *choices = json_object_get(resp_root, "choices");
                    if (choices && json_array_size(choices) > 0) {
                        json_t *first = json_array_get(choices, 0);
                        json_t *message = first ? json_object_get(first, "message") : NULL;
                        json_t *content = message ? json_object_get(message, "content") : NULL;
                        if (content && json_is_string(content)) {
                            response->content = strdup(json_string_value(content));
                            response->success = 1;
                        } else {
                            response->success = 0;
                            response->error_message = strdup("Invalid response format from DeepSeek");
                        }
                    } else {
                        response->success = 0;
                        response->error_message = strdup("No choices in DeepSeek response");
                    }
                }
                json_decref(resp_root);
            } else {
                response->success = 0;
                response->error_message = strdup("Failed to parse DeepSeek response");
            }
        } else {
            response->success = 0;
            response->error_message = strdup(curl_easy_strerror(res));
        }
        
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        free(payload);
        free(chunk.memory);
        json_decref(root);
    }
    
    curl_global_cleanup();
    return response;
}

/* Query Ollama (local) */
static AIResponse *query_ollama(const char *prompt, const char *context) {
    AIResponse *response = calloc(1, sizeof(AIResponse));
    CURL *curl;
    CURLcode res;
    struct MemoryChunk chunk = {0};
    
    char *host = getenv("OLLAMA_HOST");
    if (!host) host = "http://localhost:11434";
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (curl) {
        json_t *root = json_object();
        
        /* Build prompt with context */
        size_t full_len = strlen(prompt) + (context ? strlen(context) : 0) + 100;
        char *full_prompt = malloc(full_len);
        if (context && strlen(context) > 0) {
            snprintf(full_prompt, full_len, "%s\n\nUser: %s", context, prompt);
        } else {
            strncpy(full_prompt, prompt, full_len - 1);
        }
        
        json_object_set_new(root, "model", json_string(current_model));
        json_object_set_new(root, "prompt", json_string(full_prompt));
        json_object_set_new(root, "stream", json_false());
        
        char *payload = json_dumps(root, JSON_COMPACT);
        
        char url[256];
        snprintf(url, sizeof(url), "%s/api/generate", host);
        
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);  /* Longer timeout for local */
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK && chunk.memory) {
            json_error_t error;
            json_t *resp_root = json_loads(chunk.memory, 0, &error);
            if (resp_root) {
                /* Check for API error response first */
                json_t *api_error = json_object_get(resp_root, "error");
                if (api_error && json_is_string(api_error)) {
                    response->success = 0;
                    response->error_message = strdup(json_string_value(api_error));
                } else {
                    json_t *resp_text = json_object_get(resp_root, "response");
                    if (resp_text && json_is_string(resp_text)) {
                        response->content = strdup(json_string_value(resp_text));
                        response->success = 1;
                    } else {
                        response->success = 0;
                        response->error_message = strdup("No response from Ollama");
                    }
                }
                json_decref(resp_root);
            } else {
                response->success = 0;
                response->error_message = strdup("Failed to parse Ollama response");
            }
        } else {
            response->success = 0;
            response->error_message = strdup(curl_easy_strerror(res));
        }
        
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        free(payload);
        free(chunk.memory);
        json_decref(root);
        free(full_prompt);
    }
    
    curl_global_cleanup();
    return response;
}

/* Internal query function with fallback tracking */
static AIResponse *ai_query_internal(const char *prompt, const char *context, 
                                      int tried_backends[], int *retry_count) {
    AIResponse *response = NULL;
    
    /* Mark current backend as tried */
    tried_backends[active_backend] = 1;
    
    switch (active_backend) {
        case AI_BACKEND_GEMINI:
            response = query_gemini(prompt, context);
            break;
        case AI_BACKEND_OPENAI:
            response = query_openai(prompt, context);
            break;
        case AI_BACKEND_CLAUDE:
            response = query_claude(prompt, context);
            break;
        case AI_BACKEND_DEEPSEEK:
            response = query_deepseek(prompt, context);
            break;
        case AI_BACKEND_OLLAMA:
            response = query_ollama(prompt, context);
            break;
        default:
            response = calloc(1, sizeof(AIResponse));
            response->success = 0;
            response->error_message = strdup("Unknown backend");
    }
    
    /* Try fallback if failed - but only once per backend */
    if (!response->success && *retry_count < AI_BACKEND_COUNT) {
        /* Find next untried backend */
        AIBackendType fallback = AI_BACKEND_COUNT;
        for (int i = 0; i < AI_BACKEND_COUNT; i++) {
            if (backends[i].enabled && !tried_backends[i]) {
                fallback = (AIBackendType)i;
                break;
            }
        }
        
        if (fallback < AI_BACKEND_COUNT) {
            _puts(COLOR_YELLOW);
            _puts("Primary backend failed, trying fallback: ");
            _puts(ai_get_backend_name(fallback));
            _puts("\n");
            _puts(COLOR_RESET);
            
            ai_response_free(response);
            AIBackendType old_backend = active_backend;
            char old_model[256];
            strncpy(old_model, current_model, sizeof(old_model) - 1);
            old_model[sizeof(old_model) - 1] = '\0';
            
            active_backend = fallback;
            strncpy(current_model, backends[fallback].default_model, sizeof(current_model) - 1);
            current_model[sizeof(current_model) - 1] = '\0';
            (*retry_count)++;
            response = ai_query_internal(prompt, context, tried_backends, retry_count);
            
            active_backend = old_backend;
            strncpy(current_model, old_model, sizeof(current_model) - 1);
            current_model[sizeof(current_model) - 1] = '\0';
        }
    }
    
    return response;
}

/* Main query function */
AIResponse *ai_query(const char *prompt, const char *context) {
    int tried_backends[AI_BACKEND_COUNT] = {0};
    int retry_count = 0;
    return ai_query_internal(prompt, context, tried_backends, &retry_count);
}

void ai_response_free(AIResponse *response) {
    if (response) {
        free(response->content);
        free(response->error_message);
        free(response);
    }
}
