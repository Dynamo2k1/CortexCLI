#!/bin/bash
# CortexCLI Bash Integration
# Source this file in your .bashrc: source /path/to/cortexcli.bash
#
# This enables natural language commands directly in bash.
# Usage: Just type natural language and press Enter.
# The integration will auto-detect and route to CortexCLI.

# Find CortexCLI executable
CORTEXCLI_BIN="${CORTEXCLI_BIN:-$(which dynamo 2>/dev/null)}"

# Colors
RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
BLUE='\033[1;34m'
CYAN='\033[1;36m'
RESET='\033[0m'

# Check if input looks like natural language
_cortex_is_natural_language() {
    local input="$1"
    
    # Check for explicit AI prefix
    if [[ "$input" == \'* ]] || [[ "$input" == ai:* ]]; then
        return 0
    fi
    
    # Check for non-ASCII characters (multilingual)
    if [[ "$input" =~ [^[:ascii:]] ]]; then
        return 0
    fi
    
    # Check for natural language keywords
    local keywords="create|make|generate|show|list|find|help|how|what|why|when|where|who|please|can you|i want|i need|give me|tell me|explain|fix|solve|write|build|delete|remove|add|update|change"
    if echo "$input" | grep -iE "^($keywords)" > /dev/null 2>&1; then
        return 0
    fi
    
    # Check for question marks
    if [[ "$input" == *\?* ]]; then
        return 0
    fi
    
    return 1
}

# Handle command not found
command_not_found_handle() {
    local input="$*"
    
    if [[ -n "$CORTEXCLI_BIN" ]] && [[ -x "$CORTEXCLI_BIN" ]]; then
        echo -e "${CYAN}🤖 CortexCLI: Analyzing input...${RESET}"
        
        if _cortex_is_natural_language "$input"; then
            echo -e "${GREEN}Detected as natural language query${RESET}"
            echo "$input" | "$CORTEXCLI_BIN"
            return $?
        fi
    fi
    
    # Default error message
    echo -e "${RED}bash: $1: command not found${RESET}" >&2
    return 127
}

# Alias for quick AI access
alias ai='_cortex_ai'
alias ask='_cortex_ai'

_cortex_ai() {
    if [[ -n "$CORTEXCLI_BIN" ]] && [[ -x "$CORTEXCLI_BIN" ]]; then
        echo "'$*" | "$CORTEXCLI_BIN"
    else
        echo -e "${RED}CortexCLI not found. Set CORTEXCLI_BIN environment variable or compile CortexCLI.${RESET}"
        return 1
    fi
}

# Ctrl+G shortcut to invoke AI on current line
_cortex_readline_ai() {
    local input="$READLINE_LINE"
    if [[ -n "$input" ]]; then
        READLINE_LINE=""
        READLINE_POINT=0
        _cortex_ai "$input"
    fi
}

# Bind Ctrl+G to AI invocation
bind -x '"\C-g": _cortex_readline_ai'

# Show status
if [[ -n "$CORTEXCLI_BIN" ]] && [[ -x "$CORTEXCLI_BIN" ]]; then
    echo -e "${GREEN}CortexCLI integration loaded.${RESET}"
    echo -e "  - Use ${CYAN}'query'${RESET} or ${CYAN}ai:query${RESET} for AI"
    echo -e "  - Press ${YELLOW}Ctrl+G${RESET} to send current line to AI"
    echo -e "  - Type ${CYAN}ask \"your question\"${RESET} for quick queries"
else
    echo -e "${YELLOW}CortexCLI binary not found. Set CORTEXCLI_BIN environment variable.${RESET}"
fi
