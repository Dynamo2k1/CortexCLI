#!/usr/bin/env zsh
# CortexCLI Zsh Integration
# Source this file in your .zshrc: source /path/to/cortexcli.zsh
#
# This enables natural language commands directly in zsh.
# Usage: Just type natural language and press Enter.
# The integration will auto-detect and route to CortexCLI.

# Find CortexCLI executable
CORTEXCLI_BIN="${CORTEXCLI_BIN:-$(which dynamo 2>/dev/null)}"

# Colors
autoload -U colors && colors

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
command_not_found_handler() {
    local input="$*"
    
    if [[ -n "$CORTEXCLI_BIN" ]] && [[ -x "$CORTEXCLI_BIN" ]]; then
        if _cortex_is_natural_language "$input"; then
            echo "$fg_bold[cyan]🤖 CortexCLI: Processing natural language query...$reset_color"
            echo "$input" | "$CORTEXCLI_BIN"
            return $?
        fi
    fi
    
    # Default error message
    echo "$fg_bold[red]zsh: command not found: $1$reset_color" >&2
    return 127
}

# ZLE widget for AI invocation
_cortex_zle_ai() {
    local input="$BUFFER"
    if [[ -n "$input" ]]; then
        BUFFER=""
        zle reset-prompt
        print -P "$fg_bold[cyan]🤖 Sending to CortexCLI: $input$reset_color"
        _cortex_ai "$input"
    fi
}
zle -N _cortex_zle_ai

# Alias for quick AI access
alias ai='_cortex_ai'
alias ask='_cortex_ai'
alias cortex='_cortex_ai'

_cortex_ai() {
    if [[ -n "$CORTEXCLI_BIN" ]] && [[ -x "$CORTEXCLI_BIN" ]]; then
        echo "'$*" | "$CORTEXCLI_BIN"
    else
        print -P "$fg_bold[red]CortexCLI not found. Set CORTEXCLI_BIN or install dynamo.$reset_color"
        return 1
    fi
}

# Bind Ctrl+G to AI invocation
bindkey '^G' _cortex_zle_ai

# Completion for ai command
_cortex_complete() {
    local -a subcommands
    subcommands=(
        'backend:List available AI backends'
        'use:Switch AI backend (gemini/openai/claude/deepseek/ollama)'
        'model:Set model for current backend'
    )
    _describe 'command' subcommands
}
compdef _cortex_complete ai

# Show status
if [[ -n "$CORTEXCLI_BIN" ]] && [[ -x "$CORTEXCLI_BIN" ]]; then
    print -P "$fg_bold[green]CortexCLI integration loaded.$reset_color"
    print -P "  - Use $fg[cyan]'query'$reset_color or $fg[cyan]ai:query$reset_color for AI"
    print -P "  - Press $fg[yellow]Ctrl+G$reset_color to send current line to AI"
    print -P "  - Type $fg[cyan]ask \"your question\"$reset_color for quick queries"
else
    print -P "$fg_bold[yellow]CortexCLI binary not found. Set CORTEXCLI_BIN environment variable.$reset_color"
fi
