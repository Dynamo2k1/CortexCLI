#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

#define GEMINI_URL "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent"
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
    // Create a cleaned version of the response (single line)
    char *cleaned_response = strdup(ai_response);
    for (char *p = cleaned_response; *p; p++) {
        if (*p == '\n') *p = '|';  // Replace newlines with pipes
    }
    
    // Store new exchange
    session_memory[session_memory_count % MAX_SESSION_MEMORY].user_input = strdup(user_input);
    session_memory[session_memory_count % MAX_SESSION_MEMORY].ai_response = cleaned_response;
    session_memory_count++;
}

/* Signal handler */
void sig_handler(int sig_num)
{
    if (sig_num == SIGINT)
    {
        _puts("\n\033[0;91m#DYNAMO$\033[0m ");
    }
}

/* CURL write callback */
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr)
        return 0;

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

/* Get AI command from Gemini */
char *get_ai_command(const char *input)
{
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk = {0};
    char *api_key = getenv("GEMINI_API_KEY");
    char *command = NULL;

    if (!api_key)
    {
        _puts("Error: GEMINI_API_KEY not set\n");
        return NULL;
    }

    // Build context-enhanced query
    char context_query[4096] = {0};
    strcpy(context_query, PROMPT_PREFIX);
    
    // Add session memory
    for (int i = 0; i < MAX_SESSION_MEMORY; i++) {
        int idx = (session_memory_count - i - 1) % MAX_SESSION_MEMORY;
        if (idx < 0) idx += MAX_SESSION_MEMORY;
        
        if (session_memory[idx].user_input && session_memory[idx].ai_response) {
            strcat(context_query, "User: ");
            strcat(context_query, session_memory[idx].user_input);
            strcat(context_query, "\nAI: ");
            strcat(context_query, session_memory[idx].ai_response);
            strcat(context_query, "\n");
        }
    }
    
    strcat(context_query, "Current query: ");
    strcat(context_query, input);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl)
    {
        char url[256];
        snprintf(url, sizeof(url), "%s?key=%s", GEMINI_URL, api_key);

        json_t *root = json_object();
        json_t *contents = json_array();
        json_t *content = json_object();
        json_t *parts = json_array();

        // Use context_query instead of input
        json_array_append_new(parts, json_pack("{s:s}", "text", context_query));
        json_object_set_new(content, "parts", parts);
        json_array_append_new(contents, content);
        json_object_set_new(root, "contents", contents);

        char *payload = json_dumps(root, JSON_COMPACT);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);

        res = curl_easy_perform(curl);

        if (res == CURLE_OK && chunk.memory)
        {
            json_error_t error;
            json_t *root = json_loads(chunk.memory, 0, &error);
            if (root)
            {
                json_t *candidates = json_object_get(root, "candidates");
                if (candidates && json_array_size(candidates) > 0)
                {
                    json_t *content = json_object_get(json_array_get(candidates, 0), "content");
                    json_t *parts = json_object_get(content, "parts");
                    if (json_array_size(parts) > 0)
                    {
                        json_t *text = json_object_get(json_array_get(parts, 0), "text");
                        command = strdup(json_string_value(text));
                    }
                }
                json_decref(root);
            }
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        free(payload);
        free(chunk.memory);
    }

    curl_global_cleanup();
    return command;
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
    // Security check
    if (strstr(cmd, "rm -rf") || strstr(cmd, "sudo") || strstr(cmd, "chmod 777")) {
        _puts(COLOR_RED);
        _puts("Security: Command blocked\n");
        _puts(COLOR_RESET);
        return;
    }
    
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
                
                // Check for version info
                if (strstr(line, "version")) {
                    char service[256];
                    char version[256];
                    // Extract service and version (simplified)
                    if (sscanf(line, "%*d/tcp open %255s %255s", service, version) == 2) {
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
    display_logo();
    // Initialize history
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

        // Add to both histories
        add_history(input);               // Readline history
        add_custom_history(&hist, input); // Custom history

        // Handle AI commands first
        if (input[0] == '\'' || strstr(input, "ai:") == input)
        {
            handle_ai_command(input);
            free(input);
            continue;
        }

        char **arv = splitstring(input, " \n");

        // Built-in commands
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

        // Handle pipes
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

        // Regular commands
        if (arv[0])
        {
            list_path *head = linkpath(_getenv("PATH"));
            char *full_path = _which(arv[0], head);

            if (full_path)
            {
                free(arv[0]);
                arv[0] = full_path;
                expand_tilde(arv);

                // Print command in green before execution
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
                // Print error in red
                _puts(COLOR_RED);
                _puts(arv[0]);
                _puts(": command not found\n");
                _puts(COLOR_RESET);
            }
            free_list(head);
        }

        freearv(arv);
        free(input);
    }
    free(hist.items);
    return 0;
}