#include "audit.h"
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

static int audit_enabled = 1;
static char audit_log_path[512] = {0};
static char session_id[64] = {0};

void audit_init(void) {
    /* Set default log path */
    char *home = getenv("HOME");
    if (home) {
        snprintf(audit_log_path, sizeof(audit_log_path), 
                 "%s/.cortexcli_audit.log", home);
    } else {
        strncpy(audit_log_path, "/tmp/cortexcli_audit.log", sizeof(audit_log_path) - 1);
    }
    
    /* Check environment for custom path */
    char *custom_path = getenv("CORTEX_AUDIT_LOG");
    if (custom_path) {
        strncpy(audit_log_path, custom_path, sizeof(audit_log_path) - 1);
    }
    
    /* Check if audit is disabled */
    char *disabled = getenv("CORTEX_AUDIT_DISABLED");
    if (disabled && strcmp(disabled, "1") == 0) {
        audit_enabled = 0;
    }
    
    /* Generate session ID */
    snprintf(session_id, sizeof(session_id), "%ld_%d", time(NULL), getpid());
}

void audit_cleanup(void) {
    /* Flush any pending writes */
}

const char *audit_get_type_name(AuditEntryType type) {
    switch (type) {
        case AUDIT_AI_QUERY: return "AI_QUERY";
        case AUDIT_AI_RESPONSE: return "AI_RESPONSE";
        case AUDIT_COMMAND_EXEC: return "COMMAND_EXEC";
        case AUDIT_COMMAND_BLOCKED: return "COMMAND_BLOCKED";
        case AUDIT_SAFETY_WARNING: return "SAFETY_WARNING";
        case AUDIT_USER_CONFIRM: return "USER_CONFIRM";
        case AUDIT_BACKEND_SWITCH: return "BACKEND_SWITCH";
        case AUDIT_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

static const char *get_current_user(void) {
    struct passwd *pw = getpwuid(getuid());
    if (pw) return pw->pw_name;
    return "unknown";
}

void audit_log(AuditEntryType type, const char *details) {
    audit_log_with_user(type, details, get_current_user());
}

void audit_log_with_user(AuditEntryType type, const char *details, const char *user) {
    if (!audit_enabled) return;
    
    FILE *fp = fopen(audit_log_path, "a");
    if (!fp) return;
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    /* Sanitize details for log - replace newlines with spaces */
    char *safe_details = strdup(details ? details : "");
    for (char *p = safe_details; *p; p++) {
        if (*p == '\n') *p = ' ';
    }
    
    fprintf(fp, "[%s] [%s] [%s] [%s] %s\n",
            timestamp,
            session_id,
            user,
            audit_get_type_name(type),
            safe_details);
    
    free(safe_details);
    fclose(fp);
}

void audit_show_recent(int count) {
    if (!audit_enabled) {
        _puts("Audit logging is disabled.\n");
        return;
    }
    
    FILE *fp = fopen(audit_log_path, "r");
    if (!fp) {
        _puts("No audit log found.\n");
        return;
    }
    
    /* Count total lines */
    int total_lines = 0;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), fp)) {
        total_lines++;
    }
    
    /* Reset and skip to last N lines */
    rewind(fp);
    int skip = total_lines - count;
    if (skip < 0) skip = 0;
    
    int current_line = 0;
    _puts("\n");
    _puts(COLOR_CYAN);
    _puts("Recent Audit Log Entries:\n");
    _puts(COLOR_RESET);
    _puts("─────────────────────────────────────────────────\n");
    
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (current_line >= skip) {
            /* Parse and colorize output */
            if (strstr(buffer, "AI_QUERY")) {
                _puts(COLOR_BLUE);
            } else if (strstr(buffer, "COMMAND_EXEC")) {
                _puts(COLOR_GREEN);
            } else if (strstr(buffer, "BLOCKED") || strstr(buffer, "ERROR")) {
                _puts(COLOR_RED);
            } else if (strstr(buffer, "WARNING")) {
                _puts(COLOR_YELLOW);
            }
            _puts(buffer);
            _puts(COLOR_RESET);
        }
        current_line++;
    }
    
    _puts("─────────────────────────────────────────────────\n");
    fclose(fp);
}

void audit_show_all(void) {
    if (!audit_enabled) {
        _puts("Audit logging is disabled.\n");
        return;
    }
    
    FILE *fp = fopen(audit_log_path, "r");
    if (!fp) {
        _puts("No audit log found.\n");
        return;
    }
    
    char buffer[4096];
    _puts("\n");
    _puts(COLOR_CYAN);
    _puts("Full Audit Log:\n");
    _puts(COLOR_RESET);
    _puts("─────────────────────────────────────────────────\n");
    
    while (fgets(buffer, sizeof(buffer), fp)) {
        _puts(buffer);
    }
    
    _puts("─────────────────────────────────────────────────\n");
    fclose(fp);
}

void audit_show_by_type(AuditEntryType type) {
    if (!audit_enabled) {
        _puts("Audit logging is disabled.\n");
        return;
    }
    
    FILE *fp = fopen(audit_log_path, "r");
    if (!fp) {
        _puts("No audit log found.\n");
        return;
    }
    
    const char *type_str = audit_get_type_name(type);
    char buffer[4096];
    int found = 0;
    
    _puts("\n");
    _puts(COLOR_CYAN);
    _puts("Audit Log Entries for type: ");
    _puts(type_str);
    _puts("\n");
    _puts(COLOR_RESET);
    _puts("─────────────────────────────────────────────────\n");
    
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (strstr(buffer, type_str)) {
            _puts(buffer);
            found = 1;
        }
    }
    
    if (!found) {
        _puts("No entries found for this type.\n");
    }
    
    _puts("─────────────────────────────────────────────────\n");
    fclose(fp);
}

void audit_set_enabled(int enabled) {
    audit_enabled = enabled;
}

int audit_is_enabled(void) {
    return audit_enabled;
}

void audit_set_log_path(const char *path) {
    if (path) {
        strncpy(audit_log_path, path, sizeof(audit_log_path) - 1);
    }
}

const char *audit_get_log_path(void) {
    return audit_log_path;
}

void audit_clear(void) {
    if (!audit_enabled) return;
    
    FILE *fp = fopen(audit_log_path, "w");
    if (fp) {
        fclose(fp);
        _puts("Audit log cleared.\n");
    }
}
