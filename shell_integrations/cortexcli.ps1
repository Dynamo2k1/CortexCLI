# CortexCLI PowerShell Integration
# Source this file in your profile: . /path/to/cortexcli.ps1
#
# This enables natural language commands directly in PowerShell.
# Usage: Just type natural language with 'ai' or 'ask' prefix.

# Find CortexCLI executable
if (-not $env:CORTEXCLI_BIN) {
    $cortexPath = Get-Command dynamo -ErrorAction SilentlyContinue
    if ($cortexPath) {
        $env:CORTEXCLI_BIN = $cortexPath.Source
    }
}

# Check if input looks like natural language
function Test-CortexNaturalLanguage {
    param([string]$Input)
    
    # Check for explicit AI prefix
    if ($Input -match "^'") { return $true }
    if ($Input -match "^ai:") { return $true }
    
    # Check for non-ASCII characters (multilingual)
    if ($Input -match '[^\x00-\x7F]') { return $true }
    
    # Check for natural language keywords
    $keywords = @(
        'create', 'make', 'generate', 'show', 'list', 'find', 'help',
        'how', 'what', 'why', 'when', 'where', 'who', 'please', 'can you',
        'i want', 'i need', 'give me', 'tell me', 'explain', 'fix', 'solve',
        'write', 'build', 'delete', 'remove', 'add', 'update', 'change'
    )
    
    foreach ($keyword in $keywords) {
        if ($Input -match "^$keyword\b") { return $true }
    }
    
    # Check for question marks
    if ($Input -match '\?') { return $true }
    
    return $false
}

# AI command function
function Invoke-CortexAI {
    param(
        [Parameter(ValueFromRemainingArguments=$true)]
        [string[]]$Query
    )
    
    $input = $Query -join ' '
    
    if ($env:CORTEXCLI_BIN -and (Test-Path $env:CORTEXCLI_BIN)) {
        Write-Host "🤖 CortexCLI: Processing query..." -ForegroundColor Cyan
        $result = "'$input" | & $env:CORTEXCLI_BIN
        Write-Output $result
    } else {
        Write-Host "CortexCLI not found. Set CORTEXCLI_BIN environment variable or compile CortexCLI." -ForegroundColor Red
    }
}

# Aliases
Set-Alias -Name ai -Value Invoke-CortexAI
Set-Alias -Name ask -Value Invoke-CortexAI
Set-Alias -Name cortex -Value Invoke-CortexAI

# AI Backend management
function Set-CortexBackend {
    param(
        [Parameter(Mandatory=$true)]
        [ValidateSet('gemini', 'openai', 'claude', 'deepseek', 'ollama')]
        [string]$Backend
    )
    
    if ($env:CORTEXCLI_BIN -and (Test-Path $env:CORTEXCLI_BIN)) {
        "ai use $Backend" | & $env:CORTEXCLI_BIN
    }
}

function Get-CortexBackends {
    if ($env:CORTEXCLI_BIN -and (Test-Path $env:CORTEXCLI_BIN)) {
        "ai backend" | & $env:CORTEXCLI_BIN
    }
}

# Tab completion for ai command
Register-ArgumentCompleter -CommandName ai -ScriptBlock {
    param($commandName, $wordToComplete, $commandAst, $fakeBoundParameters)
    
    @('backend', 'use', 'model') | Where-Object { $_ -like "$wordToComplete*" } | ForEach-Object {
        [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_)
    }
}

# Key handler for Ctrl+G (requires PSReadLine)
if (Get-Module PSReadLine) {
    Set-PSReadLineKeyHandler -Chord 'Ctrl+g' -ScriptBlock {
        $line = $null
        $cursor = $null
        [Microsoft.PowerShell.PSConsoleReadLine]::GetBufferState([ref]$line, [ref]$cursor)
        
        if ($line) {
            [Microsoft.PowerShell.PSConsoleReadLine]::RevertLine()
            Write-Host ""
            Write-Host "🤖 Sending to CortexCLI: $line" -ForegroundColor Cyan
            Invoke-CortexAI $line
        }
    }
}

# Show status
if ($env:CORTEXCLI_BIN -and (Test-Path $env:CORTEXCLI_BIN)) {
    Write-Host "CortexCLI integration loaded." -ForegroundColor Green
    Write-Host "  - Use " -NoNewline
    Write-Host "'query'" -ForegroundColor Cyan -NoNewline
    Write-Host " or " -NoNewline
    Write-Host "ai query" -ForegroundColor Cyan -NoNewline
    Write-Host " for AI"
    Write-Host "  - Press " -NoNewline
    Write-Host "Ctrl+G" -ForegroundColor Yellow -NoNewline
    Write-Host " to send current line to AI"
    Write-Host "  - Type " -NoNewline
    Write-Host 'ask "your question"' -ForegroundColor Cyan -NoNewline
    Write-Host " for quick queries"
} else {
    Write-Host "CortexCLI binary not found. Set CORTEXCLI_BIN environment variable." -ForegroundColor Yellow
}
