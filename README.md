# CortexCLI - AI-Powered Linux Shell 🚀

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![GitHub Stars](https://img.shields.io/github/stars/dynamo2k1/CortexCLI?style=social)](https://github.com/dynamo2k1/CortexCLI)

A modern, production-ready Linux shell with integrated AI capabilities supporting **multiple AI backends** (OpenAI, Google Gemini, Anthropic Claude, DeepSeek, Ollama), **automatic natural language detection**, **multilingual input** (English, Urdu, Arabic, Hindi, and more), and **comprehensive safety features**.

![Demo GIF](https://media.giphy.com/media/v1.Y2lkPTc5MGI3NjExY3E4d3l1b2NscGx6eW1xN2w0dWxwN3JrbjV5a2l3d3R5Z3Z5cHd5ZyZlcD12MV9pbnRlcm5hbF9naWZfYnlfaWQmY3Q9Zw/26tn33aiTi1jkl6H6/giphy.gif)

## Features ✨

### Core Features
- **🤖 Multi-Backend AI Support** - OpenAI, Google Gemini, Anthropic Claude, DeepSeek, and local Ollama
- **🧠 Automatic Language Detection** - Routes natural language to AI, commands to shell
- **🌐 Multilingual Support** - Urdu, Arabic, Hindi, Spanish, French, Chinese, and more
- **🔗 Full Shell Features** - Pipes (`|`), redirection (`>`), command chaining (`;`)
- **📜 Command History** - `history`, `!!`, `!<num>`
- **🏠 Path Expansion** - Tilde (`~`) and variable expansion

### Safety & Security
- **🛡️ Risk Analysis** - Automatic risk evaluation before command execution
- **⚠️ Safety Confirmation** - User confirmation for risky commands
- **🚫 Blocked Patterns** - Prevents catastrophic commands (`rm -rf /`, fork bombs)
- **📦 Sandbox Mode** - Preview commands without execution
- **📝 Audit Logging** - Complete history of AI interactions

### Shell Integration
- **Zsh Plugin** - Native zsh integration with keybindings
- **Bash Plugin** - Bash integration with command_not_found handler
- **Fish Plugin** - Fish shell integration with completions
- **PowerShell Module** - Windows PowerShell support

## Installation ⚙️

### Dependencies
```bash
# Ubuntu/Debian
sudo apt install build-essential libcurl4-openssl-dev libjansson-dev libreadline-dev

# Fedora
sudo dnf install gcc libcurl-devel jansson-devel readline-devel

# macOS
brew install curl jansson readline
```

### Compilation
```bash
git clone https://github.com/Dynamo2k1/CortexCLI.git
cd CortexCLI
make
```

### Shell Integration (Optional)
```bash
# Zsh
echo 'source /path/to/CortexCLI/shell_integrations/cortexcli.zsh' >> ~/.zshrc

# Bash  
echo 'source /path/to/CortexCLI/shell_integrations/cortexcli.bash' >> ~/.bashrc

# Fish
echo 'source /path/to/CortexCLI/shell_integrations/cortexcli.fish' >> ~/.config/fish/config.fish

# PowerShell
Add-Content $PROFILE '. /path/to/CortexCLI/shell_integrations/cortexcli.ps1'
```

## Configuration 🔧

### AI Backend API Keys

Set one or more of the following environment variables:

```bash
# Google Gemini (Default)
export GEMINI_API_KEY="your-gemini-key"

# OpenAI
export OPENAI_API_KEY="your-openai-key"

# Anthropic Claude
export ANTHROPIC_API_KEY="your-anthropic-key"

# DeepSeek
export DEEPSEEK_API_KEY="your-deepseek-key"

# Ollama (Local LLMs)
export OLLAMA_HOST="http://localhost:11434"
```

### Optional Settings

```bash
# Enable sandbox mode by default
export CORTEX_SANDBOX=1

# Set preferred language
export CORTEX_LANG=urdu

# Custom audit log path
export CORTEX_AUDIT_LOG=/var/log/cortexcli.log

# Disable audit logging
export CORTEX_AUDIT_DISABLED=1

# Set risk threshold (low/medium/high/critical)
export CORTEX_RISK_THRESHOLD=high
```

## Usage 🖥️

### Starting CortexCLI
```bash
./dynamo
```

### Natural Language Queries
```bash
# Explicit AI prefix
➤ 'create a Python script to parse JSON files

# Alternative prefix
➤ ai:show files larger than 10MB

# Auto-detected natural language
➤ how do I find all log files?

# Multilingual (Urdu)
➤ اردو میں Python اسکرپٹ بناؤ جو CSV فائل ریڈ کرے

# Multilingual (Hindi)
➤ एक Python स्क्रिप्ट बनाओ जो JSON पढ़े
```

### Managing AI Backends
```bash
# List available backends
➤ ai backend

# Switch backend
➤ ai use openai
➤ ai use claude
➤ ai use ollama

# Change model
➤ ai model gpt-4
➤ ai model claude-3-sonnet-20240229
```

### Safety Features
```bash
# Enable sandbox mode (preview only)
➤ sandbox on

# Disable sandbox mode
➤ sandbox off

# View audit log
➤ audit

# Clear audit log
➤ audit clear
```

### Regular Shell Commands
```bash
# Standard commands work as expected
➤ ls -la | grep ".txt"
➤ cat file.txt > output.txt
➤ cd ~/Documents
```

## Interactive Examples 💡

### Project Scaffolding
```
➤ 'create a new React project with Tailwind CSS and TypeScript
🤖 Detected: English (confidence: 87%)
COMMAND: npx create-react-app my-app --template typescript
COMMAND: cd my-app && npm install -D tailwindcss postcss autoprefixer
COMMAND: npx tailwindcss init -p
EXPLAIN: Created React project with TypeScript template and installed Tailwind CSS...
```

### Multilingual Support
```
➤ اردو میں ایک Python اسکرپٹ بناؤ جو CSV فائل ریڈ کرے اور JSON میں تبدیل کرے
🤖 Detected: Urdu (confidence: 95%)
COMMAND: cat > csv_to_json.py << 'EOF'
import csv
import json
...
EOF
EXPLAIN: یہ اسکرپٹ CSV فائل کو JSON میں تبدیل کرتا ہے...
```

### Git Error Recovery
```
➤ 'fix the last git error and commit with a proper message
🤖 Processing natural language query...
COMMAND: git status
COMMAND: git add -A
COMMAND: git commit -m "fix: resolve merge conflicts and update dependencies"
EXPLAIN: Fixed the staging issue and created a descriptive commit message...
```

## Architecture 🏗️

```
┌─────────────────────────────────────────────────────────────┐
│                        CortexCLI                            │
├─────────────────────────────────────────────────────────────┤
│  Input Parser → Language Detector → AI Router → Executor    │
│       ↓              ↓                 ↓           ↓        │
│  [Classify]    [Detect Lang]    [Multi-Backend]  [Safe Exec]│
│       ↓              ↓                 ↓           ↓        │
│  Command/NL    EN/UR/AR/HI     Gemini/OpenAI   Risk Check   │
│                                Claude/DeepSeek  Confirm     │
│                                Ollama           Sandbox     │
├─────────────────────────────────────────────────────────────┤
│                      Audit Logger                           │
│              (All AI interactions logged)                   │
└─────────────────────────────────────────────────────────────┘
```

## Demo 🎥

https://github.com/user-attachments/assets/570492af-3abb-41d7-bcd3-e85caabe0f1d

## Contributing 🤝

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the Branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## License 📄
MIT License - See [LICENSE](LICENSE) for details

## Acknowledgments 🌟
- Google Gemini API
- OpenAI API
- Anthropic Claude API
- DeepSeek API
- Ollama Project
- Readline Library
- Jansson JSON Parser



## **Crafted with ❤️ by:**

* [Dynamo2k1](https://github.com/Dynamo2k1)
* [Prof.Paradox](https://github.com/ProfParadox)
* [hurrainjhl](https://github.com/hurrainjhl)
* [ZUNATIC](https://github.com/ZUNATIC)

*"Where Human Intuition Meets Machine Intelligence"*
