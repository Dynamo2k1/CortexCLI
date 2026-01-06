# CortexCLI Fish Shell Integration
# Source this file in your config.fish: source /path/to/cortexcli.fish
#
# This enables natural language commands directly in fish.
# Usage: Just type natural language and press Enter.
# The integration will auto-detect and route to CortexCLI.

# Find CortexCLI executable
set -q CORTEXCLI_BIN; or set -gx CORTEXCLI_BIN (which dynamo 2>/dev/null)

# Check if input looks like natural language
function _cortex_is_natural_language
    set -l input $argv[1]
    
    # Check for explicit AI prefix
    if string match -q "\'*" "$input"; or string match -q "ai:*" "$input"
        return 0
    end
    
    # Check for non-ASCII characters (multilingual)
    if string match -qr '[^\x00-\x7F]' "$input"
        return 0
    end
    
    # Check for natural language keywords
    set -l keywords "create|make|generate|show|list|find|help|how|what|why|when|where|who|please|can you|i want|i need|give me|tell me|explain|fix|solve|write|build|delete|remove|add|update|change"
    if string match -qri "^($keywords)" "$input"
        return 0
    end
    
    # Check for question marks
    if string match -q "*?*" "$input"
        return 0
    end
    
    return 1
end

# Handle command not found
function fish_command_not_found
    set -l input $argv
    
    if test -n "$CORTEXCLI_BIN"; and test -x "$CORTEXCLI_BIN"
        if _cortex_is_natural_language "$input"
            set_color cyan
            echo "🤖 CortexCLI: Processing natural language query..."
            set_color normal
            echo "$input" | $CORTEXCLI_BIN
            return $status
        end
    end
    
    # Default error message
    set_color red
    echo "fish: Unknown command: $argv[1]" >&2
    set_color normal
    return 127
end

# Alias for quick AI access
function ai
    _cortex_ai $argv
end

function ask
    _cortex_ai $argv
end

function cortex
    _cortex_ai $argv
end

function _cortex_ai
    if test -n "$CORTEXCLI_BIN"; and test -x "$CORTEXCLI_BIN"
        echo "'$argv" | $CORTEXCLI_BIN
    else
        set_color red
        echo "CortexCLI not found. Set CORTEXCLI_BIN environment variable or compile CortexCLI."
        set_color normal
        return 1
    end
end

# Keybinding for AI invocation (Ctrl+G)
function _cortex_ai_keybinding
    set -l input (commandline)
    if test -n "$input"
        commandline -r ""
        set_color cyan
        echo "🤖 Sending to CortexCLI: $input"
        set_color normal
        _cortex_ai $input
        commandline -f repaint
    end
end

bind \cg _cortex_ai_keybinding

# Completion for ai command
complete -c ai -f -n "__fish_is_first_token" -a "backend" -d "List available AI backends"
complete -c ai -f -n "__fish_is_first_token" -a "use" -d "Switch AI backend"
complete -c ai -f -n "__fish_is_first_token" -a "model" -d "Set model for current backend"
complete -c ai -f -n "__fish_seen_subcommand_from use" -a "gemini openai claude deepseek ollama"

# Show status
if test -n "$CORTEXCLI_BIN"; and test -x "$CORTEXCLI_BIN"
    set_color green
    echo "CortexCLI integration loaded."
    set_color normal
    echo "  - Use 'query' or ai:query for AI"
    echo "  - Press Ctrl+G to send current line to AI"
    echo "  - Type 'ask \"your question\"' for quick queries"
else
    set_color yellow
    echo "CortexCLI binary not found. Set CORTEXCLI_BIN environment variable."
    set_color normal
end
