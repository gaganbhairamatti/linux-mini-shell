# Mini Shell (msh) using C

## Overview

This project implements a minimalistic Linux shell (msh) using the C programming language. It replicates basic functionalities of a Unix shell, allowing users to execute commands, manage processes, handle signals, and perform job control.

The project focuses on system-level programming concepts such as process creation, signal handling, process synchronization, and command parsing.

---

## Objective

- Understand process creation using `fork()`
- Execute programs using `exec()` family
- Handle signals (`SIGINT`, `SIGTSTP`, `SIGCHLD`)
- Implement process synchronization using `wait()`
- Perform command parsing and execution
- Implement job control and piping

---

## Features

- Default shell prompt (`msh>`)
- Custom prompt using environment variable `PS1`
- Execute external commands
- Built-in commands:
  - `cd` – change directory
  - `pwd` – print working directory
  - `exit` – terminate shell
- Special variables:
  - `echo $?` – last command exit status
  - `echo $$` – shell PID
  - `echo $SHELL` – shell executable path
- Signal handling:
  - `Ctrl+C` (SIGINT)
  - `Ctrl+Z` (SIGTSTP)
- Background execution using `&`
- Job control:
  - `jobs` – list background processes
  - `fg` – bring process to foreground
  - `bg` – resume stopped process in background
- Pipe support using `|`

---

## System Architecture

### Command Execution Flow

User Input → Parse Command → Check Built-in → Fork Process → Execute (`exec`) → Wait / Background Execution

### Pipe Execution

Command1 → Pipe → Command2 → Pipe → Command3 → Output

---

## Project Structure

```
mini-shell
│
├── main.c              # Entry point of shell
├── scan_input.c        # Input reading and parsing
├── ext_cmd.c           # External command execution
├── commands.c          # Built-in commands
│
├── header.h            # Common declarations and definitions
│
└── README.md
```

---

## How It Works

### Command Execution

- Displays prompt (`msh>`)
- Reads user input
- Parses command and arguments
- Executes:
  - Built-in commands internally
  - External commands using `fork()` + `exec()`

### Background Execution

- Commands ending with `&` run in background
- Shell does not wait for completion
- Uses `SIGCHLD` to handle terminated processes

### Signal Handling

- `Ctrl+C`: Sends SIGINT to foreground process
- `Ctrl+Z`: Stops process and moves it to background

### Pipe Handling

- Supports multiple commands using `|`
- Dynamically creates pipes between processes

---

## Compilation

```bash
gcc main.c scan_input.c ext_cmd.c commands.c -o msh
```

---

## Usage

```bash
./msh
```

---

## Example Commands

```bash
ls
pwd
cd ..
sleep 10 &
jobs
fg
bg
ls | wc
ls -l | grep txt | wc -l
```

---

## Requirements

- GCC or any C compiler
- Linux / Unix environment (recommended)
- Command-line interface

---

## Notes

- No whitespace allowed in `PS1=NEW_PROMPT`
- Background processes are managed using signals
- Multiple background processes are supported
- Pipes are handled dynamically based on user input

---

## Future Improvements

- Command history support
- Auto-completion
- Advanced parsing (quotes, redirection)
- Better error handling

---

## Author

Developed by: Gagan Bhairamatti
