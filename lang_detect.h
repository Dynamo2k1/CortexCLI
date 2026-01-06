#ifndef LANG_DETECT_H
#define LANG_DETECT_H

/* Language codes */
typedef enum {
    LANG_UNKNOWN = 0,
    LANG_ENGLISH,
    LANG_URDU,
    LANG_ARABIC,
    LANG_HINDI,
    LANG_SPANISH,
    LANG_FRENCH,
    LANG_CHINESE,
    LANG_GERMAN,
    LANG_PORTUGUESE,
    LANG_COMMAND  /* Not a natural language - a shell command */
} LanguageType;

/* Input classification result */
typedef struct {
    int is_natural_language;     /* 1 if natural language, 0 if shell command */
    LanguageType language;        /* Detected language */
    float confidence;             /* Detection confidence (0.0 - 1.0) */
    char *normalized_text;        /* Normalized/cleaned input */
} InputClassification;

/* Initialize language detection */
void lang_detect_init(void);
void lang_detect_cleanup(void);

/* Classify input */
InputClassification *classify_input(const char *input);
void classification_free(InputClassification *result);

/* Get language name */
const char *lang_get_name(LanguageType lang);

/* Set preferred language for responses */
void lang_set_preference(LanguageType lang);
LanguageType lang_get_preference(void);

/* Check if text contains non-ASCII (potential multilingual) */
int lang_is_multilingual(const char *text);

/* Get script type (Latin, Arabic, Devanagari, etc.) */
const char *lang_detect_script(const char *text);

#endif /* LANG_DETECT_H */
