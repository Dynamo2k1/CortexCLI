#ifndef SAFETY_H
#define SAFETY_H

/* Risk levels */
typedef enum {
    RISK_NONE = 0,     /* Safe to execute */
    RISK_LOW,          /* Minor changes */
    RISK_MEDIUM,       /* System modifications */
    RISK_HIGH,         /* Potentially destructive */
    RISK_CRITICAL      /* Definitely destructive */
} RiskLevel;

/* Risk analysis result */
typedef struct {
    RiskLevel level;
    char *reason;              /* Why this risk level */
    char **risky_commands;     /* List of risky commands found */
    int risky_count;
    int requires_confirmation;
    int blocked;
    char *suggestion;          /* Safer alternative suggestion */
} RiskAnalysis;

/* Initialize safety module */
void safety_init(void);
void safety_cleanup(void);

/* Analyze command for risks */
RiskAnalysis *analyze_risk(const char *command);
void risk_analysis_free(RiskAnalysis *analysis);

/* Request user confirmation for risky commands */
int safety_confirm(const char *command, RiskAnalysis *analysis);

/* Check if command should be blocked */
int safety_should_block(const char *command);

/* Sandbox mode control */
void safety_set_sandbox_mode(int enabled);
int safety_get_sandbox_mode(void);

/* Preview command in sandbox (dry-run) */
char *safety_preview_command(const char *command);

/* Get risk level name */
const char *safety_get_level_name(RiskLevel level);

/* Configuration */
void safety_set_confirmation_required(RiskLevel min_level);
void safety_add_blocked_pattern(const char *pattern);
void safety_remove_blocked_pattern(const char *pattern);

#endif /* SAFETY_H */
