#ifndef AUDIT_H
#define AUDIT_H

#include <time.h>

/* Audit entry types */
typedef enum {
    AUDIT_AI_QUERY,          /* AI query sent */
    AUDIT_AI_RESPONSE,       /* AI response received */
    AUDIT_COMMAND_EXEC,      /* Command executed */
    AUDIT_COMMAND_BLOCKED,   /* Command blocked by safety */
    AUDIT_SAFETY_WARNING,    /* Safety warning shown */
    AUDIT_USER_CONFIRM,      /* User confirmation response */
    AUDIT_BACKEND_SWITCH,    /* AI backend switched */
    AUDIT_ERROR              /* Error occurred */
} AuditEntryType;

/* Audit entry */
typedef struct {
    time_t timestamp;
    AuditEntryType type;
    char *details;
    char *user;
    char *session_id;
} AuditEntry;

/* Initialize audit system */
void audit_init(void);
void audit_cleanup(void);

/* Log audit entry */
void audit_log(AuditEntryType type, const char *details);
void audit_log_with_user(AuditEntryType type, const char *details, const char *user);

/* View audit log */
void audit_show_recent(int count);
void audit_show_all(void);
void audit_show_by_type(AuditEntryType type);

/* Configure audit */
void audit_set_enabled(int enabled);
int audit_is_enabled(void);
void audit_set_log_path(const char *path);
const char *audit_get_log_path(void);

/* Clear audit log */
void audit_clear(void);

/* Get audit entry type name */
const char *audit_get_type_name(AuditEntryType type);

#endif /* AUDIT_H */
