# Shell_cjh

Shell_cjh is a C++17 Linux shell project adapted for an operating systems course design. It demonstrates core shell mechanisms such as command parsing, process creation, foreground and background execution, I/O redirection, pipelines, history, aliases, completion, and signal handling.

The project is based on a modern Unix shell codebase and is intended to run on Ubuntu or another Linux distribution. For course presentation, focus on the basic shell workflow: read a command, parse it, dispatch builtins, or run external programs through Linux process APIs.

## Features

- Command interpretation and execution through `fork`, `execvp`, and `waitpid`
- Builtin commands such as `cd`, `pwd`, `exit`, `history`, `alias`, `unalias`, `type`, and `which`
- PATH command execution, including common tools such as `ls`, `cat`, `grep`, and `echo`
- Tab completion for builtins, PATH commands, variables, files, and directories
- Command history and history expansion
- Alias definition and expansion
- Background job management through builtins such as `bg`, `fg`, and `bglist`
- I/O redirection with `<`, `>`, `>>`, `2>`, and `2>&1`
- Pipeline execution with `|`
- Script mode through `tash <script-file>`

## Requirements

Recommended environment:

- Ubuntu 22.04 or later
- CMake 3.17 or later
- A C++17 compiler such as `g++`
- Make or Ninja
- libcurl development headers
- SQLite development headers, optional but recommended for enhanced history
- nlohmann-json development package, optional because CMake can fetch it when missing

Install common Ubuntu dependencies:

```bash
sudo apt update
sudo apt install -y cmake g++ make git \
  libcurl4-openssl-dev libsqlite3-dev nlohmann-json3-dev
```

## Build

Configure and build the project:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j "$(getconf _NPROCESSORS_ONLN)"
```

The shell executable is generated at:

```bash
./build/tash.out
```

## Run

Start the interactive shell:

```bash
./build/tash.out
```

Example commands:

```bash
pwd
ls
cd /tmp
echo hello > hello.txt
cat hello.txt
cat hello.txt | grep hello
alias ll='ls -l'
ll
history
bg sleep 5
bglist
type ls
exit
```

Run a script file:

```bash
./build/tash.out demo.tash
```

## Optional Installation

Install the built binary into a user-local directory:

```bash
mkdir -p "$HOME/.local/bin"
install -m 755 build/tash.out "$HOME/.local/bin/tash"
```

Make sure `~/.local/bin` is in your `PATH`:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

System-wide installation on Ubuntu:

```bash
sudo install -m 755 build/tash.out /usr/local/bin/tash
```

To register it as an available login shell:

```bash
command -v tash
echo /usr/local/bin/tash | sudo tee -a /etc/shells
chsh -s /usr/local/bin/tash
```

To switch back to Bash:

```bash
chsh -s /bin/bash
```

## Test

Build and run the test suite:

```bash
cmake -B build -DBUILD_TESTS=ON
cmake --build build -j "$(getconf _NPROCESSORS_ONLN)"
ctest --test-dir build --output-on-failure
```

## Directory Structure

```text
.
├── CMakeLists.txt          # CMake project entry
├── README.md               # Project guide
├── AGENTS.md               # Git/GitHub collaboration rules for this project
├── install.sh              # Upstream install helper
├── tash.1.in               # Man page template
├── cmake/                  # CMake feature, plugin, test, and sanitizer modules
├── data/                   # Runtime data such as themes and model metadata
├── docs/                   # Images and documentation assets
├── include/tash/           # Public project headers
├── src/                    # Shell implementation
│   ├── core/               # Parser, executor, process, signal, and session logic
│   ├── builtins/           # Builtin shell commands
│   ├── ui/                 # Prompt, completion, highlighting, and UI helpers
│   ├── history/            # History and frecency support
│   ├── plugins/            # Completion, history, prompt, and hook providers
│   ├── util/               # Shared utility code
│   └── ai/                 # Optional AI-related support from the upstream project
├── tests/                  # Unit and integration tests
├── fuzz/                   # Parser fuzzing corpus and harness
└── Formula/                # Homebrew formula from the upstream project
```

## Course Notes

For the operating systems course design, the most important files to read are:

- `src/repl.cpp`: interactive read-evaluate loop and history handling
- `src/core/parser.cpp`: command parsing, redirection parsing, and pipeline splitting
- `src/core/executor.cpp`: builtin dispatch and command execution flow
- `src/core/process.cpp`: `fork`, `execvp`, `waitpid`, `pipe`, and `dup2` usage
- `src/builtins/`: implementation of builtin commands
- `src/ui/completion.cpp`: command and file completion

## License

This project is derived from the MIT-licensed Tash shell project. Keep the original license and attribution when redistributing or submitting derived work.
