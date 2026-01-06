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

/* Backend Configuration */
typedef struct {
    AIBackendType type;
    const char *name;
    const char *env_key;
    const char *default_model;
    const char *api_url;
    int enabled;
} AIBackendConfig;

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

#endif /* AI_BACKEND_H */
