#include "lang_detect.h"
#include "shell.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Common shell commands for detection */
static const char *shell_commands[] = {
    "ls", "cd", "pwd", "mkdir", "rm", "cp", "mv", "cat", "echo",
    "grep", "find", "chmod", "chown", "ps", "kill", "top", "man",
    "git", "docker", "npm", "pip", "python", "node", "make", "gcc",
    "vim", "nano", "less", "more", "head", "tail", "sed", "awk",
    "curl", "wget", "ssh", "scp", "tar", "zip", "unzip", "df", "du",
    "mount", "umount", "apt", "yum", "dnf", "brew", "systemctl",
    "journalctl", "service", "export", "source", "alias", "which",
    "whoami", "hostname", "ifconfig", "ip", "ping", "netstat", "ss",
    NULL
};

/* Natural language indicators (common words/phrases) */
static const char *nl_indicators[] = {
    /* English */
    "create", "make", "generate", "show", "list", "find", "help",
    "how", "what", "why", "when", "where", "who", "please", "can you",
    "i want", "i need", "give me", "tell me", "explain", "fix", "solve",
    "write", "build", "delete", "remove", "add", "update", "change",
    /* Question words */
    "could", "would", "should", "will", "does", "is", "are", "was",
    NULL
};

/* Unicode ranges for script detection */
#define IS_ARABIC(c) ((c) >= 0x0600 && (c) <= 0x06FF)
#define IS_DEVANAGARI(c) ((c) >= 0x0900 && (c) <= 0x097F)
#define IS_CJK(c) ((c) >= 0x4E00 && (c) <= 0x9FFF)

static LanguageType preferred_language = LANG_ENGLISH;

void lang_detect_init(void) {
    /* Initialize language detection resources */
    char *lang = getenv("CORTEX_LANG");
    if (lang) {
        if (strcasecmp(lang, "urdu") == 0) preferred_language = LANG_URDU;
        else if (strcasecmp(lang, "arabic") == 0) preferred_language = LANG_ARABIC;
        else if (strcasecmp(lang, "hindi") == 0) preferred_language = LANG_HINDI;
        else if (strcasecmp(lang, "spanish") == 0) preferred_language = LANG_SPANISH;
        else if (strcasecmp(lang, "french") == 0) preferred_language = LANG_FRENCH;
        else if (strcasecmp(lang, "chinese") == 0) preferred_language = LANG_CHINESE;
        else if (strcasecmp(lang, "german") == 0) preferred_language = LANG_GERMAN;
    }
}

void lang_detect_cleanup(void) {
    /* Cleanup resources */
}

/* Get UTF-8 codepoint from string */
static unsigned int get_utf8_codepoint(const char *str, int *bytes_read) {
    unsigned char c = (unsigned char)str[0];
    unsigned int cp = 0;
    
    if ((c & 0x80) == 0) {
        *bytes_read = 1;
        return c;
    } else if ((c & 0xE0) == 0xC0) {
        *bytes_read = 2;
        cp = (c & 0x1F) << 6;
        cp |= ((unsigned char)str[1] & 0x3F);
    } else if ((c & 0xF0) == 0xE0) {
        *bytes_read = 3;
        cp = (c & 0x0F) << 12;
        cp |= ((unsigned char)str[1] & 0x3F) << 6;
        cp |= ((unsigned char)str[2] & 0x3F);
    } else if ((c & 0xF8) == 0xF0) {
        *bytes_read = 4;
        cp = (c & 0x07) << 18;
        cp |= ((unsigned char)str[1] & 0x3F) << 12;
        cp |= ((unsigned char)str[2] & 0x3F) << 6;
        cp |= ((unsigned char)str[3] & 0x3F);
    } else {
        *bytes_read = 1;
        return 0;
    }
    
    return cp;
}

/* Detect script type from text */
const char *lang_detect_script(const char *text) {
    int arabic_count = 0, devanagari_count = 0, cjk_count = 0, latin_count = 0;
    int total = 0;
    
    const char *p = text;
    while (*p) {
        int bytes;
        unsigned int cp = get_utf8_codepoint(p, &bytes);
        
        if (IS_ARABIC(cp)) arabic_count++;
        else if (IS_DEVANAGARI(cp)) devanagari_count++;
        else if (IS_CJK(cp)) cjk_count++;
        else if ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z')) latin_count++;
        
        total++;
        p += bytes;
    }
    
    if (arabic_count > latin_count && arabic_count > devanagari_count) return "Arabic";
    if (devanagari_count > latin_count && devanagari_count > arabic_count) return "Devanagari";
    if (cjk_count > latin_count) return "CJK";
    return "Latin";
}

/* Check if text contains non-ASCII characters */
int lang_is_multilingual(const char *text) {
    while (*text) {
        if ((unsigned char)*text > 127) return 1;
        text++;
    }
    return 0;
}

/* Check if input looks like a shell command */
static int looks_like_command(const char *input) {
    /* Skip leading whitespace */
    while (*input && isspace(*input)) input++;
    
    /* Empty input */
    if (!*input) return 0;
    
    /* Check for path-like input */
    if (input[0] == '/' || input[0] == '.') return 1;
    
    /* Check for common command patterns */
    if (strchr(input, '/') && !strchr(input, ' ')) return 1;
    
    /* Extract first word */
    char first_word[64] = {0};
    int i = 0;
    while (input[i] && !isspace(input[i]) && i < 63) {
        first_word[i] = tolower(input[i]);
        i++;
    }
    first_word[i] = '\0';
    
    /* Check against known commands */
    for (int j = 0; shell_commands[j]; j++) {
        if (strcmp(first_word, shell_commands[j]) == 0) {
            return 1;
        }
    }
    
    /* Check for command-like patterns */
    /* Starts with sudo, ./, -, or contains flags like -l, --help */
    if (strncmp(input, "sudo ", 5) == 0) return 1;
    if (strncmp(input, "./", 2) == 0) return 1;
    if (input[0] == '-') return 1;
    if (strstr(input, " -") != NULL) return 1;  /* Has flags */
    if (strstr(input, " | ") != NULL) return 1;  /* Pipe */
    if (strstr(input, " > ") != NULL) return 1;  /* Redirect */
    if (strstr(input, " >> ") != NULL) return 1; /* Append */
    if (strchr(input, '=') != NULL && !strchr(input, ' ')) return 1; /* var=value */
    
    return 0;
}

/* Check if input looks like natural language */
static int looks_like_natural_language(const char *input) {
    char lower[512];
    int len = strlen(input);
    if (len > 511) len = 511;
    
    for (int i = 0; i < len; i++) {
        lower[i] = tolower(input[i]);
    }
    lower[len] = '\0';
    
    /* Check for natural language indicators */
    for (int i = 0; nl_indicators[i]; i++) {
        if (strstr(lower, nl_indicators[i]) != NULL) {
            return 1;
        }
    }
    
    /* Check for question mark */
    if (strchr(input, '?') != NULL) return 1;
    
    /* Check if contains multiple words with spaces (likely sentence) */
    int spaces = 0;
    int words = 1;
    for (int i = 0; input[i]; i++) {
        if (input[i] == ' ') {
            spaces++;
            if (i > 0 && input[i-1] != ' ') words++;
        }
    }
    
    /* Sentences typically have 3+ words */
    if (words >= 4) return 1;
    
    /* Contains multilingual characters - likely natural language */
    if (lang_is_multilingual(input)) return 1;
    
    return 0;
}

/* Detect language from text */
static LanguageType detect_language(const char *text) {
    const char *script = lang_detect_script(text);
    
    if (strcmp(script, "Arabic") == 0) {
        /* Could be Arabic or Urdu - check for Urdu-specific patterns */
        /* Urdu uses more Persian/Arabic script combinations */
        return LANG_URDU;  /* Default to Urdu for Arabic script in this context */
    }
    
    if (strcmp(script, "Devanagari") == 0) {
        return LANG_HINDI;
    }
    
    if (strcmp(script, "CJK") == 0) {
        return LANG_CHINESE;
    }
    
    /* For Latin script, default to English */
    return LANG_ENGLISH;
}

/* Main classification function */
InputClassification *classify_input(const char *input) {
    InputClassification *result = calloc(1, sizeof(InputClassification));
    if (!result) return NULL;
    
    /* Clean and normalize input */
    result->normalized_text = strdup(input);
    
    /* Check for explicit AI prefix */
    if (input[0] == '\'' || strncmp(input, "ai:", 3) == 0) {
        result->is_natural_language = 1;
        result->confidence = 1.0f;
        /* Skip the prefix in normalized text */
        if (input[0] == '\'') {
            free(result->normalized_text);
            result->normalized_text = strdup(input + 1);
        } else if (strncmp(input, "ai:", 3) == 0) {
            free(result->normalized_text);
            result->normalized_text = strdup(input + 3);
        }
        result->language = detect_language(result->normalized_text);
        return result;
    }
    
    /* Score command likelihood vs natural language */
    int cmd_score = looks_like_command(input) ? 1 : 0;
    int nl_score = looks_like_natural_language(input) ? 1 : 0;
    
    /* If contains non-ASCII, strongly favor natural language */
    if (lang_is_multilingual(input)) {
        nl_score += 2;
    }
    
    if (cmd_score > nl_score) {
        result->is_natural_language = 0;
        result->language = LANG_COMMAND;
        result->confidence = (float)(cmd_score) / (float)(cmd_score + nl_score + 1);
    } else if (nl_score > cmd_score) {
        result->is_natural_language = 1;
        result->language = detect_language(input);
        result->confidence = (float)(nl_score) / (float)(cmd_score + nl_score + 1);
    } else {
        /* Ambiguous - default to command */
        result->is_natural_language = 0;
        result->language = LANG_COMMAND;
        result->confidence = 0.5f;
    }
    
    return result;
}

void classification_free(InputClassification *result) {
    if (result) {
        free(result->normalized_text);
        free(result);
    }
}

const char *lang_get_name(LanguageType lang) {
    switch (lang) {
        case LANG_ENGLISH: return "English";
        case LANG_URDU: return "Urdu";
        case LANG_ARABIC: return "Arabic";
        case LANG_HINDI: return "Hindi";
        case LANG_SPANISH: return "Spanish";
        case LANG_FRENCH: return "French";
        case LANG_CHINESE: return "Chinese";
        case LANG_GERMAN: return "German";
        case LANG_PORTUGUESE: return "Portuguese";
        case LANG_COMMAND: return "Shell Command";
        default: return "Unknown";
    }
}

void lang_set_preference(LanguageType lang) {
    preferred_language = lang;
}

LanguageType lang_get_preference(void) {
    return preferred_language;
}
