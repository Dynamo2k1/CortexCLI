# CortexCLI - AI-Powered Linux Shell 🚀

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![GitHub Stars](https://img.shields.io/github/stars/dynamo2k1/CortexCLI?style=social)](https://github.com/dynamo2k1/CortexCLI)

A modern Linux shell with integrated AI capabilities powered by Google Gemini, supporting multilingual interactions and advanced command execution.

![Demo GIF](https://media.giphy.com/media/v1.Y2lkPTc5MGI3NjExY3E4d3l1b2NscGx6eW1xN2w0dWxwN3JrbjV5a2l3d3R5Z3Z5cHd5ZyZlcD12MV9pbnRlcm5hbF9naWZfYnlfaWQmY3Q9Zw/26tn33aiTi1jkl6H6/giphy.gif)

## Features ✨

- **🤖 AI Command Generation** (English/Urdu/Any Language)
- **🔗 Pipe Support** (`|`, `>`, `;`)
- **🏠 Tilde Expansion** (`~/Documents`)
- **📜 Command History** with `history` and `!!`
- **🌐 Multilingual Support** (Urdu, Arabic, etc.)
- **🛡️ Security** (Blocks `sudo`/`rm -rf`)
- **💻 Custom Color Prompt** with User/Host info

## Installation ⚙️

### Dependencies
```bash
# Ubuntu/Debian
sudo apt install build-essential libcurl4-openssl-dev libjansson-dev libreadline-dev

# Fedora
sudo dnf install gcc libcurl-devel jansson-devel readline-devel
```

### Compilation
```bash
git clone https://github.com/yourusername/CortexCLI.git
cd CortexCLI
make  # Or manually compile with:
# gcc -o dynamo *.c -lcurl -ljansson -lreadline
```

## Usage 🖥️

```bash
# Start shell
./dynamo

# Regular commands
dynamo@Host:~ ➤ ls -l | wc -l

# AI interaction
dynamo@Host:~ ➤ 'show files in home directory
```

## Configuration 🔧

1. **Get Gemini API Key** from [Google AI Studio](https://aistudio.google.com/)
2. **Add to Environment**:
```bash
export GEMINI_API_KEY="your-api-key-here"
```

## Demo 🎥

https://github.com/user-attachments/assets/570492af-3abb-41d7-bcd3-e85caabe0f1d

## Contributing 🤝

1. Fork the Project
2. Create your Feature Branch
3. Commit your Changes
4. Push to the Branch
5. Open a Pull Request

## License 📄
MIT License - See [LICENSE](LICENSE) for details

## Acknowledgments 🌟
- Google Gemini API
- Readline Library
- Jansson JSON Parser

---

**Crafted with ❤️ by Dynamo2k1, Prof.Paradox and hurrainjhl**  
*"Where Human Intuition Meets Machine Intelligence"*
