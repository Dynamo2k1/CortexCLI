#ifndef AI_BACKEND_H
#define AI_BACKEND_H

#include <stddef.h>

/* AI Backend Types */
typedef enum {
    AI_BACKEND_GEMINI = 0,
    AI_BACKEND_OPENAI,
    AI_BACKEND_CLAUDE,
    AI_BACKEND_DEEPSEEK,
    AI_BACKEND_OLLAMA,
    AI_BACKEND_COUNT
} AIBackendType;

/* Task Types for model selection */
typedef enum {
    TASK_GENERAL = 0,        /* General reasoning/chat */
    TASK_CODE_GENERATION,    /* Code generation */
    TASK_SHELL_COMMAND,      /* Shell command generation */
    TASK_AUTOMATION,         /* Scripts/automation */
    TASK_EXPLANATION         /* Explanations requested */
} TaskType;

/* Model capability flags */
typedef enum {
    MODEL_CAP_GENERAL = 1,
    MODEL_CAP_CODE = 2,
    MODEL_CAP_SHELL = 4,
    MODEL_CAP_REASONING = 8,
    MODEL_CAP_FAST = 16
} ModelCapability;

/* Backend Configuration */
typedef struct {
    AIBackendType type;
    const char *name;
    const char *env_key;
    const char *default_model;
    const char *api_url;
    int enabled;
} AIBackendConfig;

/* Ollama Model Info */
typedef struct {
    char *name;
    char *modified_at;
    long size;
    int capabilities;   /* Bitmask of ModelCapability */
} OllamaModel;

/* Ollama Model List */
typedef struct {
    OllamaModel *models;
    int count;
} OllamaModelList;

/* AI Response */
typedef struct {
    char *content;
    int success;
    char *error_message;
} AIResponse;

/* Backend Functions */
void ai_backend_init(void);
void ai_backend_cleanup(void);

AIBackendType ai_get_active_backend(void);
int ai_set_backend(AIBackendType type);
int ai_set_backend_by_name(const char *name);
const char *ai_get_backend_name(AIBackendType type);

/* Check if backend is available (API key set) */
int ai_backend_available(AIBackendType type);
AIBackendType ai_get_fallback_backend(void);

/* Main AI query function */
AIResponse *ai_query(const char *prompt, const char *context);
void ai_response_free(AIResponse *response);

/* List available backends */
void ai_list_backends(void);

/* Get/Set model for current backend */
const char *ai_get_model(void);
void ai_set_model(const char *model);

/* Task type detection */
TaskType ai_detect_task_type(const char *input);
const char *ai_get_task_type_name(TaskType type);

/* Ollama model detection */
OllamaModelList *ai_ollama_list_models(void);
void ai_ollama_model_list_free(OllamaModelList *list);
const char *ai_ollama_select_best_model(TaskType task);
int ai_ollama_check_available(void);
void ai_list_ollama_models(void);

/* Intelligent model selection */
void ai_auto_select_model(TaskType task);
const char *ai_get_recommended_model(AIBackendType backend, TaskType task);

/* Get model-specific system prompt */
const char *ai_get_optimized_prompt(TaskType task);

#endif /* AI_BACKEND_H */
