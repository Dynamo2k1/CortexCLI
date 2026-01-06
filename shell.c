#include "shell.h"
#include "ai_backend.h"
#include "lang_detect.h"
#include "safety.h"
#include "audit.h"
#include <readline/readline.h>
#include <readline/history.h>
#include <ctype.h>

#define PROMPT_PREFIX "You are CortexCLI, a security-focused Linux assistant. Follow these rules:\n" \
"1. Respond with MULTIPLE lines if needed, each starting with one of: COMMAND:, EXPLAIN:, SCAN:, VULN:, or CTF:\n" \
"2. For complex responses:\n" \
"   - Use COMMAND: for executable Linux commands\n" \
"   - Use EXPLAIN: for explanations and analysis\n" \
"   - Use SCAN: for security scan commands\n" \
"   - Use VULN: for vulnerability research\n" \
"   - Use CTF: for CTF challenges\n" \
"3. ALWAYS prefix each logical unit with the appropriate prefix\n" \
"4. Provide both commands and explanations when appropriate\n" \
"5. Never include text before prefixes\n" \
"6. Support multilingual input (Urdu, Arabic, Hindi, etc.) - detect language and respond appropriately\n" \
"Current session context:\n"

History hist = {0};
SessionExchange session_memory[MAX_SESSION_MEMORY] = {0};
int session_memory_count = 0;

struct MemoryStruct
{
    char *memory;
    size_t size;
};

void add_to_session_memory(const char *user_input, const char *ai_response) {
    /* Create a cleaned version of the response (single line) */
    char *cleaned_response = strdup(ai_response);
    for (char *p = cleaned_response; *p; p++) {
        if (*p == '\n') *p = '|';  /* Replace newlines with pipes */
    }
    
    /* Store new exchange */
    session_memory[session_memory_count % MAX_SESSION_MEMORY].user_input = strdup(user_input);
    session_memory[session_memory_count % MAX_SESSION_MEMORY].ai_response = cleaned_response;
    session_memory_count++;
}

/* Signal handler */
void sig_handler(int sig_num)
{
    if (sig_num == SIGINT)
    {
        _puts("\n\033[0;91m#CortexCLI$\033[0m ");
    }
}

/* Get AI command using the multi-backend system */
char *get_ai_command(const char *input)
{
    /* Build context-enhanced query */
    char context_query[8192] = {0};
    strncpy(context_query, PROMPT_PREFIX, sizeof(context_query) - 1);
    
    /* Add session memory */
    for (int i = 0; i < MAX_SESSION_MEMORY; i++) {
        int idx = (session_memory_count - i - 1) % MAX_SESSION_MEMORY;
        if (idx < 0) idx += MAX_SESSION_MEMORY;
        
        if (session_memory[idx].user_input && session_memory[idx].ai_response) {
            size_t remaining = sizeof(context_query) - strlen(context_query) - 1;
            if (remaining > 100) {
                strncat(context_query, "User: ", remaining);
                strncat(context_query, session_memory[idx].user_input, remaining - 50);
                strncat(context_query, "\nAI: ", remaining - 50);
                strncat(context_query, session_memory[idx].ai_response, remaining - 100);
                strncat(context_query, "\n", remaining - 100);
            }
        }
    }
    
    /* Log the AI query */
    audit_log(AUDIT_AI_QUERY, input);
    
    /* Query AI with the new backend system */
    AIResponse *response = ai_query(input, context_query);
    
    if (response && response->success && response->content) {
        audit_log(AUDIT_AI_RESPONSE, response->content);
        char *result = strdup(response->content);
        ai_response_free(response);
        return result;
    }
    
    if (response && response->error_message) {
        audit_log(AUDIT_ERROR, response->error_message);
        _puts(COLOR_RED);
        _puts("AI Error: ");
        _puts(response->error_message);
        _puts("\n");
        _puts(COLOR_RESET);
    }
    
    ai_response_free(response);
    return NULL;
}   

void expand_variables(char **args)
{
    if (!args)
        return;
    for (int i = 0; args[i]; i++)
    {
        if (args[i][0] == '$')
        {
            char *var_name = args[i] + 1;
            char *value = getenv(var_name);
            if (value)
            {
                free(args[i]);
                args[i] = strdup(value);
            }
        }
    }
}

void expand_tilde(char **args)
{
    if (!args)
        return;
    for (int i = 0; args[i]; i++)
    {
        if (args[i][0] == '~')
        {
            char *home = getenv("HOME");
            if (!home)
                home = getenv("PWD");
            char *new = malloc(strlen(home) + strlen(args[i]));
            strcpy(new, home);
            strcat(new, args[i] + 1);
            free(args[i]);
            args[i] = new;
        }
    }
}

void execute_single_command(char *cmd) {
    /* Use the safety module for risk analysis */
    RiskAnalysis *analysis = analyze_risk(cmd);
    
    if (analysis->blocked) {
        _puts(COLOR_RED);
        _puts("⛔ Security: Command BLOCKED\n");
        _puts("Reason: ");
        _puts(analysis->reason);
        _puts("\n");
        _puts(COLOR_RESET);
        audit_log(AUDIT_COMMAND_BLOCKED, cmd);
        risk_analysis_free(analysis);
        return;
    }
    
    /* Check if in sandbox mode */
    if (safety_get_sandbox_mode()) {
        char *preview = safety_preview_command(cmd);
        _puts(COLOR_YELLOW);
        _puts(preview);
        _puts(COLOR_RESET);
        free(preview);
        risk_analysis_free(analysis);
        return;
    }
    
    /* Request confirmation for risky commands */
    if (analysis->requires_confirmation) {
        if (!safety_confirm(cmd, analysis)) {
            _puts(COLOR_YELLOW);
            _puts("Command cancelled by user.\n");
            _puts(COLOR_RESET);
            audit_log(AUDIT_USER_CONFIRM, "denied");
            risk_analysis_free(analysis);
            return;
        }
        audit_log(AUDIT_USER_CONFIRM, "approved");
    }
    
    risk_analysis_free(analysis);
    
    /* Log command execution */
    audit_log(AUDIT_COMMAND_EXEC, cmd);
    
    char **args = splitstring(cmd, " \n");
    expand_tilde(args);
    expand_variables(args);

    void (*builtin_func)(char **) = checkbuild(args);
    if (builtin_func) {
        builtin_func(args);
    } else {
        if (contains_pipes(cmd)) {
            char ***pipeline = parse_pipeline(cmd);
            execute_pipeline(pipeline);
            for (int i = 0; pipeline[i]; i++)
                freearv(pipeline[i]);
            free(pipeline);
        } else {
            execute(args);
        }
    }
    freearv(args);
}

void execute_scan_command(char *scan_cmd) {
    // Check if nmap is installed
    if (strstr(scan_cmd, "nmap") && system("which nmap > /dev/null 2>&1") != 0) {
        _puts(COLOR_RED);
        _puts("Error: nmap is not installed. Please install it with:\n");
        _puts("  sudo apt install nmap   # Debian/Ubuntu\n");
        _puts("  sudo yum install nmap   # RedHat/CentOS\n");
        _puts(COLOR_RESET);
        return;
    }
    
    // Execute scan and capture output
    FILE *fp = popen(scan_cmd, "r");
    if (fp) {
        char buffer[4096];
        size_t bytes_read = fread(buffer, 1, sizeof(buffer)-1, fp);
        buffer[bytes_read] = '\0';
        pclose(fp);
        
        _puts("Scan results:\n");
        _puts(buffer);
        analyze_scan_results(buffer);
    } else {
        perror("popen");
    }
}

void handle_ai_response(char *response) {
    char *saveptr;
    char *line = strtok_r(response, "\n", &saveptr);
    
    while (line) {
        // Trim leading whitespace
        while (isspace(*line)) line++;
        
        if (strstr(line, "COMMAND:") == line) {
            char *cmd = line + 8;
            while (isspace(*cmd)) cmd++;
            
            _puts(COLOR_GREEN);
            _puts("Executing: ");
            _puts(cmd);
            _puts("\n");
            _puts(COLOR_RESET);
            
            execute_single_command(cmd);
        }
        else if (strstr(line, "EXPLAIN:") == line) {
            char *explanation = line + 8;
            while (isspace(*explanation)) explanation++;
            
            handle_explanation(explanation);
        }
        else if (strstr(line, "SCAN:") == line) {
            char *scan_cmd = line + 5;
            while (isspace(*scan_cmd)) scan_cmd++;
            
            _puts(COLOR_CYAN);
            _puts("Executing scan: ");
            _puts(scan_cmd);
            _puts("\n");
            _puts(COLOR_RESET);
            
            execute_scan_command(scan_cmd);
        }
        else if (strstr(line, "VULN:") == line) {
            char *vuln_info = line + 5;
            while (isspace(*vuln_info)) vuln_info++;
            
            research_vulnerability(vuln_info);
        }
        else if (strstr(line, "CTF:") == line) {
            char *ctf_info = line + 4;
            while (isspace(*ctf_info)) ctf_info++;
            
            ctf_assistance(ctf_info);
        }
        else if (strlen(line) > 0) {
            // Default to explanation for non-empty lines without prefix
            handle_explanation(line);
        }
        
        line = strtok_r(NULL, "\n", &saveptr);
    }
}

void handle_ai_command(char *input) {
    char *clean_input = input + (input[0] == '\'' ? 1 : (strstr(input, "ai:") == input ? 3 : 0));
    clean_input[strcspn(clean_input, "\n")] = 0;
    add_custom_history(&hist, clean_input);
    
    char *response = get_ai_command(clean_input);
    if (response) {
        add_to_session_memory(clean_input, response);
        handle_ai_response(response);  // Process the AI response
        free(response);
    } else {
        _puts("AI request failed\n");
    }
}



void handle_explanation(const char *text)
{
    _puts("\nExplanation:\n");
    char *copy = strdup(text);
    char *line = strtok(copy, "\n");
    while (line)
    {
        _puts("  ");
        _puts(line);
        _puts("\n");
        line = strtok(NULL, "\n");
    }
    free(copy);
}

char *get_hostname()
{
    static char hostname[256];
    if (gethostname(hostname, sizeof(hostname)))
        return NULL;
    // Trim domain if present
    char *dot = strchr(hostname, '.');
    if (dot)
        *dot = '\0';
    return hostname;
}

// Display logo
void display_logo() {
    system("printf \"\\033[1;36m\" && figlet -d ~/.figlet/fonts -f 3d \"CortexCLI\"");
    system("printf \"\\033[0;35m\" && figlet -f small \"by Dynamo2k1\"");
    printf("\033[0m\n");
    printf("     Security-focused Command Line Interface\n\n");
}

char* generate_prompt() {
    char *user = getenv("USER");
    char *host = get_hostname();
    char *cwd = getenv("PWD");
    
    if (!user) user = "unknown";
    if (!host) host = "localhost";
    if (!cwd) cwd = "~";

    // Create prompt string with colors
    char *prompt = malloc(512);
    snprintf(prompt, 512, 
        "\n\033[1;31m%s\033[0m@\033[1;36m%s\033[0m:\033[1;33m%s\033[0m\n➤ ",
        user, host, cwd);

    return prompt;
}

void analyze_scan_results(const char *scan_output) {
    // This would parse nmap/scan output and identify interesting findings
    _puts(COLOR_CYAN);
    _puts("\n[+] Analyzing scan results...\n");
    
    // Example parsing logic (you'd expand this)
    if (strstr(scan_output, "open")) {
        _puts("Found open ports:\n");
        // Extract port numbers and services
        // This is simplified - you'd want proper parsing
        char *copy = strdup(scan_output);
        char *line = strtok(copy, "\n");
        
        while (line) {
            if (strstr(line, "open")) {
                _puts("  ");
                _puts(line);
                _puts("\n");
                
                /* Check for version info */
                if (strstr(line, "version")) {
                    char service[128];  /* %127s + null terminator */
                    char version[128];  /* %127s + null terminator */
                    /* Extract service and version (simplified) */
                    if (sscanf(line, "%*d/tcp open %127s %127s", service, version) == 2) {
                        char query[512];
                        snprintf(query, sizeof(query), "VULN: %s %s", service, version);
                        handle_ai_command(query);
                    }
                }
            }
            line = strtok(NULL, "\n");
        }
        free(copy);
    } else {
        _puts("No open ports found\n");
    }
    _puts(COLOR_RESET);
}

void research_vulnerability(const char *service_info) {
    _puts(COLOR_MAGENTA);
    _puts("\n[+] Researching vulnerabilities for: ");
    _puts(service_info);
    _puts("\n");
    
    // Prepare query for AI
    char query[512];
    snprintf(query, sizeof(query), 
        "Find known vulnerabilities for %s. Include CVE references and exploit links if available.", 
        service_info);
    
    char *response = get_ai_command(query);
    if (response) {
        if (strstr(response, "EXPLAIN:")) {
            handle_explanation(strstr(response, "EXPLAIN:") + 8);
        } else {
            _puts(response);
            _puts("\n");
        }
        free(response);
    } else {
        _puts("Failed to research vulnerabilities\n");
    }
    _puts(COLOR_RESET);
}

void ctf_assistance(const char *challenge_info) {
    _puts(COLOR_YELLOW);
    _puts("\n[+] CTF Challenge Assistance for: ");
    _puts(challenge_info);
    _puts("\n");
    
    char *response = get_ai_command(challenge_info);
    if (response) {
        if (strstr(response, "EXPLAIN:")) {
            handle_explanation(strstr(response, "EXPLAIN:") + 8);
        } else if (strstr(response, "COMMAND:")) {
            _puts("Suggested solution:\n");
            handle_explanation(strstr(response, "COMMAND:") + 8);
        } else {
            _puts(response);
            _puts("\n");
        }
        free(response);
    } else {
        _puts("Failed to get CTF assistance\n");
    }
    _puts(COLOR_RESET);
}

int main(void)
{
    /* Initialize all modules */
    ai_backend_init();
    lang_detect_init();
    safety_init();
    audit_init();
    
    display_logo();
    
    /* Show current backend */
    _puts(COLOR_CYAN);
    _puts("AI Backend: ");
    _puts(ai_get_backend_name(ai_get_active_backend()));
    _puts(" (");
    _puts(ai_get_model());
    _puts(")\n");
    _puts(COLOR_RESET);
    
    /* Initialize history */
    hist.items = malloc(sizeof(char *) * MAX_HISTORY);
    hist.size = MAX_HISTORY;
    hist.count = 0;

    signal(SIGINT, sig_handler);
    rl_bind_key('\t', rl_complete);

    while (1)
    {

        char *prompt = generate_prompt();
        char *input = readline(prompt);
        free(prompt);
        if (!input)
            break;
        
        /* Skip empty input */
        if (strlen(input) == 0) {
            free(input);
            continue;
        }

        /* Add to both histories */
        add_history(input);               /* Readline history */
        add_custom_history(&hist, input); /* Custom history */

        /* Handle explicit AI commands first */
        if (input[0] == '\'' || strncmp(input, "ai:", 3) == 0)
        {
            handle_ai_command(input);
            free(input);
            continue;
        }

        char **arv = splitstring(input, " \n");

        /* Built-in commands */
        if (arv[0])
        {
            void (*builtin_func)(char **) = checkbuild(arv);
            if (builtin_func)
            {
                builtin_func(arv);
                freearv(arv);
                free(input);
                continue;
            }
        }

        /* Use automatic language detection */
        InputClassification *classification = classify_input(input);
        
        if (classification->is_natural_language) {
            /* Detected as natural language - route to AI */
            _puts(COLOR_CYAN);
            _puts("🤖 Detected: ");
            _puts(lang_get_name(classification->language));
            _puts(" (confidence: ");
            char conf_str[16];
            snprintf(conf_str, sizeof(conf_str), "%.0f%%", 
                     classification->confidence * 100);
            _puts(conf_str);
            _puts(")\n");
            _puts(COLOR_RESET);
            
            handle_ai_command(input);
            classification_free(classification);
            freearv(arv);
            free(input);
            continue;
        }
        
        classification_free(classification);

        /* Handle pipes */
        if (contains_pipes(input))
        {
            char ***pipeline = parse_pipeline(input);
            execute_pipeline(pipeline);
            for (int i = 0; pipeline[i]; i++)
                freearv(pipeline[i]);
            free(pipeline);
            freearv(arv);
            free(input);
            continue;
        }

        /* Regular commands */
        if (arv[0])
        {
            list_path *head = linkpath(_getenv("PATH"));
            char *full_path = _which(arv[0], head);

            if (full_path)
            {
                free(arv[0]);
                arv[0] = full_path;
                expand_tilde(arv);

                /* Print command in green before execution */
                _puts(COLOR_GREEN);
                _puts("Executing: ");
                for (int i = 0; arv[i]; i++)
                {
                    _puts(arv[i]);
                    _puts(" ");
                }
                _puts("\n");
                _puts(COLOR_RESET);

                execute(arv);
            }
            else
            {
                /* Command not found - try AI */
                _puts(COLOR_YELLOW);
                _puts("Command '");
                _puts(arv[0]);
                _puts("' not found. Trying AI...\n");
                _puts(COLOR_RESET);
                
                handle_ai_command(input);
            }
            free_list(head);
        }

        freearv(arv);
        free(input);
    }
    
    /* Cleanup */
    free(hist.items);
    ai_backend_cleanup();
    lang_detect_cleanup();
    safety_cleanup();
    audit_cleanup();
    
    return 0;
}

/* AI builtin command - manage AI backends */
void ai_builtin(char **args) {
    if (!args[1]) {
        /* Show current backend info */
        _puts("\nCurrent AI Backend: ");
        _puts(COLOR_GREEN);
        _puts(ai_get_backend_name(ai_get_active_backend()));
        _puts(COLOR_RESET);
        _puts("\nModel: ");
        _puts(ai_get_model());
        _puts("\n\nType 'ai backend' to list all backends\n");
        _puts("Type 'ai use <name>' to switch backend\n");
        _puts("Type 'ai model <name>' to change model\n");
        return;
    }
    
    if (strcmp(args[1], "backend") == 0 || strcmp(args[1], "backends") == 0) {
        ai_list_backends();
        return;
    }
    
    if (strcmp(args[1], "use") == 0) {
        if (!args[2]) {
            _puts("Usage: ai use <backend_name>\n");
            _puts("Available: gemini, openai, claude, deepseek, ollama\n");
            return;
        }
        if (ai_set_backend_by_name(args[2]) == 0) {
            _puts(COLOR_GREEN);
            _puts("Switched to backend: ");
            _puts(args[2]);
            _puts("\n");
            _puts(COLOR_RESET);
            audit_log(AUDIT_BACKEND_SWITCH, args[2]);
        } else {
            _puts(COLOR_RED);
            _puts("Failed to switch backend. Is ");
            _puts(args[2]);
            _puts(" configured?\n");
            _puts(COLOR_RESET);
        }
        return;
    }
    
    if (strcmp(args[1], "model") == 0) {
        if (!args[2]) {
            _puts("Current model: ");
            _puts(ai_get_model());
            _puts("\n");
            return;
        }
        ai_set_model(args[2]);
        _puts(COLOR_GREEN);
        _puts("Model set to: ");
        _puts(args[2]);
        _puts("\n");
        _puts(COLOR_RESET);
        return;
    }
    
    _puts("Unknown ai subcommand. Try: backend, use, model\n");
}

/* Sandbox builtin command */
void sandbox_builtin(char **args) {
    if (!args[1]) {
        _puts("Sandbox mode: ");
        if (safety_get_sandbox_mode()) {
            _puts(COLOR_GREEN);
            _puts("ON");
        } else {
            _puts(COLOR_YELLOW);
            _puts("OFF");
        }
        _puts(COLOR_RESET);
        _puts("\nUsage: sandbox on|off\n");
        return;
    }
    
    if (strcmp(args[1], "on") == 0) {
        safety_set_sandbox_mode(1);
        _puts(COLOR_GREEN);
        _puts("Sandbox mode enabled. Commands will be previewed but not executed.\n");
        _puts(COLOR_RESET);
        return;
    }
    
    if (strcmp(args[1], "off") == 0) {
        safety_set_sandbox_mode(0);
        _puts(COLOR_YELLOW);
        _puts("Sandbox mode disabled. Commands will be executed normally.\n");
        _puts(COLOR_RESET);
        return;
    }
    
    _puts("Usage: sandbox on|off\n");
}

/* Audit builtin command */
void audit_builtin(char **args) {
    if (!args[1]) {
        audit_show_recent(20);
        return;
    }
    
    if (strcmp(args[1], "clear") == 0) {
        audit_clear();
        return;
    }
    
    if (strcmp(args[1], "all") == 0) {
        audit_show_all();
        return;
    }
    
    if (strcmp(args[1], "on") == 0) {
        audit_set_enabled(1);
        _puts(COLOR_GREEN);
        _puts("Audit logging enabled.\n");
        _puts(COLOR_RESET);
        return;
    }
    
    if (strcmp(args[1], "off") == 0) {
        audit_set_enabled(0);
        _puts(COLOR_YELLOW);
        _puts("Audit logging disabled.\n");
        _puts(COLOR_RESET);
        return;
    }
    
    if (strcmp(args[1], "path") == 0) {
        _puts("Audit log path: ");
        _puts(audit_get_log_path());
        _puts("\n");
        return;
    }
    
    _puts("Usage: audit [clear|all|on|off|path]\n");
}