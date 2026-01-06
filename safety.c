#include "safety.h"
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <readline/readline.h>

/* Buffer size constants */
#define MAX_CMD_LENGTH 2048
#define MAX_PATTERN_LENGTH 256

/* Blocked command patterns */
static const char *blocked_patterns[] = {
    "rm -rf /",
    "rm -rf /*",
    ":(){ :|:& };:",     /* Fork bomb */
    "> /dev/sda",
    "dd if=/dev/zero of=/dev/sd",
    "mkfs.",
    "chmod -R 777 /",
    "chmod 777 /",
    NULL
};

/* High risk patterns */
static const char *high_risk_patterns[] = {
    "rm -rf",
    "rm -r",
    "sudo rm",
    "sudo dd",
    "fdisk",
    "mkfs",
    "format",
    "> /dev/",
    "chmod 777",
    "chmod -R",
    "chown -R",
    "kill -9",
    "pkill",
    "shutdown",
    "reboot",
    "poweroff",
    "init 0",
    "init 6",
    NULL
};

/* Medium risk patterns */
static const char *medium_risk_patterns[] = {
    "sudo",
    "su -",
    "passwd",
    "adduser",
    "useradd",
    "deluser",
    "userdel",
    "chmod",
    "chown",
    "mount",
    "umount",
    "apt ",
    "apt-get",
    "yum ",
    "dnf ",
    "pip install",
    "npm install -g",
    "systemctl",
    "service",
    NULL
};

/* Low risk patterns (still worth noting) */
static const char *low_risk_patterns[] = {
    "git push",
    "git reset",
    "git checkout",
    "mv ",
    "cp -r",
    "tar ",
    "zip ",
    "unzip ",
    NULL
};

static int sandbox_mode = 0;
static RiskLevel confirmation_threshold = RISK_HIGH;

void safety_init(void) {
    char *sandbox = getenv("CORTEX_SANDBOX");
    if (sandbox && strcmp(sandbox, "1") == 0) {
        sandbox_mode = 1;
    }
    
    char *threshold = getenv("CORTEX_RISK_THRESHOLD");
    if (threshold) {
        if (strcasecmp(threshold, "low") == 0) confirmation_threshold = RISK_LOW;
        else if (strcasecmp(threshold, "medium") == 0) confirmation_threshold = RISK_MEDIUM;
        else if (strcasecmp(threshold, "high") == 0) confirmation_threshold = RISK_HIGH;
        else if (strcasecmp(threshold, "critical") == 0) confirmation_threshold = RISK_CRITICAL;
    }
}

void safety_cleanup(void) {
    /* Cleanup resources */
}

/* Check if pattern exists in command */
static int contains_pattern(const char *cmd, const char *pattern) {
    char lower_cmd[MAX_CMD_LENGTH];
    int len = strlen(cmd);
    if (len > MAX_CMD_LENGTH - 1) len = MAX_CMD_LENGTH - 1;
    
    for (int i = 0; i < len; i++) {
        lower_cmd[i] = tolower(cmd[i]);
    }
    lower_cmd[len] = '\0';
    
    char lower_pattern[MAX_PATTERN_LENGTH];
    len = strlen(pattern);
    if (len > MAX_PATTERN_LENGTH - 1) len = MAX_PATTERN_LENGTH - 1;
    
    for (int i = 0; i < len; i++) {
        lower_pattern[i] = tolower(pattern[i]);
    }
    lower_pattern[len] = '\0';
    
    return strstr(lower_cmd, lower_pattern) != NULL;
}

/* Add risky command to analysis */
static void add_risky_command(RiskAnalysis *analysis, const char *pattern) {
    analysis->risky_commands = realloc(analysis->risky_commands, 
                                       sizeof(char*) * (analysis->risky_count + 1));
    analysis->risky_commands[analysis->risky_count] = strdup(pattern);
    analysis->risky_count++;
}

RiskAnalysis *analyze_risk(const char *command) {
    RiskAnalysis *analysis = calloc(1, sizeof(RiskAnalysis));
    if (!analysis) return NULL;
    
    analysis->level = RISK_NONE;
    analysis->reason = NULL;
    analysis->risky_commands = NULL;
    analysis->risky_count = 0;
    analysis->requires_confirmation = 0;
    analysis->blocked = 0;
    analysis->suggestion = NULL;
    
    /* Check blocked patterns */
    for (int i = 0; blocked_patterns[i]; i++) {
        if (contains_pattern(command, blocked_patterns[i])) {
            analysis->level = RISK_CRITICAL;
            analysis->blocked = 1;
            analysis->reason = strdup("Command contains a blocked pattern that could cause severe system damage");
            add_risky_command(analysis, blocked_patterns[i]);
            return analysis;
        }
    }
    
    /* Check high risk patterns */
    for (int i = 0; high_risk_patterns[i]; i++) {
        if (contains_pattern(command, high_risk_patterns[i])) {
            if (analysis->level < RISK_HIGH) {
                analysis->level = RISK_HIGH;
                free(analysis->reason);
                analysis->reason = strdup("Command contains high-risk operations that could cause data loss");
            }
            add_risky_command(analysis, high_risk_patterns[i]);
        }
    }
    
    /* Check medium risk patterns */
    for (int i = 0; medium_risk_patterns[i]; i++) {
        if (contains_pattern(command, medium_risk_patterns[i])) {
            if (analysis->level < RISK_MEDIUM) {
                analysis->level = RISK_MEDIUM;
                free(analysis->reason);
                analysis->reason = strdup("Command involves system modifications");
            }
            add_risky_command(analysis, medium_risk_patterns[i]);
        }
    }
    
    /* Check low risk patterns */
    for (int i = 0; low_risk_patterns[i]; i++) {
        if (contains_pattern(command, low_risk_patterns[i])) {
            if (analysis->level < RISK_LOW) {
                analysis->level = RISK_LOW;
                free(analysis->reason);
                analysis->reason = strdup("Command may modify files or system state");
            }
            add_risky_command(analysis, low_risk_patterns[i]);
        }
    }
    
    /* Set confirmation requirement */
    if (analysis->level >= confirmation_threshold) {
        analysis->requires_confirmation = 1;
    }
    
    /* Generate suggestions for high-risk commands */
    if (analysis->level >= RISK_HIGH) {
        if (contains_pattern(command, "rm -rf")) {
            analysis->suggestion = strdup("Consider using 'rm -ri' for interactive mode or 'trash-put' for safer deletion");
        } else if (contains_pattern(command, "chmod 777")) {
            analysis->suggestion = strdup("Consider using more restrictive permissions like 'chmod 755' or 'chmod 644'");
        }
    }
    
    if (!analysis->reason) {
        analysis->reason = strdup("Command appears safe to execute");
    }
    
    return analysis;
}

void risk_analysis_free(RiskAnalysis *analysis) {
    if (analysis) {
        free(analysis->reason);
        free(analysis->suggestion);
        for (int i = 0; i < analysis->risky_count; i++) {
            free(analysis->risky_commands[i]);
        }
        free(analysis->risky_commands);
        free(analysis);
    }
}

int safety_confirm(const char *command, RiskAnalysis *analysis) {
    if (!analysis->requires_confirmation) return 1;
    
    _puts("\n");
    _puts(COLOR_YELLOW);
    _puts("⚠️  SAFETY WARNING\n");
    _puts(COLOR_RESET);
    
    _puts("Risk Level: ");
    switch (analysis->level) {
        case RISK_LOW:
            _puts(COLOR_GREEN);
            _puts("LOW");
            break;
        case RISK_MEDIUM:
            _puts(COLOR_YELLOW);
            _puts("MEDIUM");
            break;
        case RISK_HIGH:
            _puts(COLOR_RED);
            _puts("HIGH");
            break;
        case RISK_CRITICAL:
            _puts(COLOR_MAGENTA);
            _puts("CRITICAL");
            break;
        default:
            _puts("NONE");
    }
    _puts(COLOR_RESET);
    _puts("\n");
    
    _puts("Reason: ");
    _puts(analysis->reason);
    _puts("\n");
    
    _puts("Command: ");
    _puts(COLOR_CYAN);
    _puts(command);
    _puts(COLOR_RESET);
    _puts("\n");
    
    if (analysis->risky_count > 0) {
        _puts("Detected patterns: ");
        for (int i = 0; i < analysis->risky_count; i++) {
            _puts(COLOR_RED);
            _puts(analysis->risky_commands[i]);
            _puts(COLOR_RESET);
            if (i < analysis->risky_count - 1) _puts(", ");
        }
        _puts("\n");
    }
    
    if (analysis->suggestion) {
        _puts("Suggestion: ");
        _puts(COLOR_GREEN);
        _puts(analysis->suggestion);
        _puts(COLOR_RESET);
        _puts("\n");
    }
    
    _puts("\n");
    char *response = readline("Do you want to proceed? [y/N]: ");
    
    int proceed = 0;
    if (response && (response[0] == 'y' || response[0] == 'Y')) {
        proceed = 1;
    }
    free(response);
    
    return proceed;
}

int safety_should_block(const char *command) {
    for (int i = 0; blocked_patterns[i]; i++) {
        if (contains_pattern(command, blocked_patterns[i])) {
            return 1;
        }
    }
    return 0;
}

void safety_set_sandbox_mode(int enabled) {
    sandbox_mode = enabled;
}

int safety_get_sandbox_mode(void) {
    return sandbox_mode;
}

char *safety_preview_command(const char *command) {
    /* Generate a dry-run preview of what the command would do */
    size_t preview_len = strlen(command) + 256;
    char *preview = malloc(preview_len);
    
    if (!preview) return NULL;
    
    snprintf(preview, preview_len,
             "[SANDBOX PREVIEW]\n"
             "Command: %s\n"
             "This would be executed but sandbox mode is enabled.\n"
             "No actual changes will be made.\n",
             command);
    
    return preview;
}

const char *safety_get_level_name(RiskLevel level) {
    switch (level) {
        case RISK_NONE: return "None";
        case RISK_LOW: return "Low";
        case RISK_MEDIUM: return "Medium";
        case RISK_HIGH: return "High";
        case RISK_CRITICAL: return "Critical";
        default: return "Unknown";
    }
}

void safety_set_confirmation_required(RiskLevel min_level) {
    confirmation_threshold = min_level;
}

void safety_add_blocked_pattern(const char *pattern) {
    /* In a full implementation, this would add to a dynamic list */
    (void)pattern;
}

void safety_remove_blocked_pattern(const char *pattern) {
    /* In a full implementation, this would remove from a dynamic list */
    (void)pattern;
}
