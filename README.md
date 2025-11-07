# C Shell Implementation

A Unix shell written in C that implements command parsing, execution, piping, I/O redirection, job control, and background process management. This project demonstrates a deep understanding of Unix process management, file I/O, and system programming concepts.

## Features

- **Command Execution**: Run any system command (ls, grep, cat, etc.)
- **Built-in Commands**: Custom implementations of common shell utilities
- **Piping**: Connect commands with `|` to pass output between processes
- **I/O Redirection**: Support for `<`, `>`, and `>>` operators
- **Background Jobs**: Run commands in the background with `&`
- **Job Control**: Manage background jobs with `activities`, `fg`, and `bg` commands
- **Command History**: View and re-run previous commands
- **Signal Handling**: Proper handling of Ctrl-C, Ctrl-Z, and other signals

## Project Structure

```
shell/
├── src/                          # Source code directory
│   ├── main.c                   # Main shell loop and initialization
│   ├── parser.c                 # Command parsing and syntax validation
│   ├── exec.c                   # Command execution logic
│   ├── pipe.c                   # Pipeline implementation
│   ├── jobs.c                   # Background job management
│   ├── signals.c                # Signal handling
│   ├── prompt.c                 # Shell prompt display
│   ├── hop.c                    # Directory navigation (cd)
│   ├── reveal.c                 # Directory listing (ls)
│   ├── log.c                    # Command history
│   └── globals.c                # Global variables and state
├── include/                     # Header files
│   ├── config.h
│   ├── exec.h
│   ├── globals.h
│   ├── hop.h
│   ├── jobs.h
│   ├── log.h
│   ├── parser.h
│   ├── pipe.h
│   ├── prompt.h
│   ├── reveal.h
│   └── signals.h
├── Makefile                     # Build configuration
└── README.md                    # This file
```

## Building the Shell

### Prerequisites
- GCC compiler
- GNU Make
- POSIX-compliant operating system (Linux/macOS)

### Compilation

```bash
# Clone the repository (if not already done)
git clone <repository-url>
cd shell

# Clean any existing builds
make clean

# Build the shell
make all
```

This will create an executable named `shell.out` in the project root.

## Usage

### Starting the Shell

```bash
./shell.out
```

You'll see a prompt like:
```
<username@hostname:~/current/directory>
```

### Basic Commands

- Run any program:
  ```
  <user@host:~> echo "Hello, World!"
  ```

- Change directories:
  ```
  <user@host:~> hop /path/to/directory
  <user@host:/path/to/directory> hop ..
  <user@host:/path> hop ~
  ```

- List directory contents:
  ```
  <user@host:~> reveal
  <user@host:~> reveal -a      # Show hidden files
  <user@host:~> reveal -l      # Long format
  <user@host:~> reveal -la     # Combine options
  ```

### Advanced Features

- **I/O Redirection**:
  ```
  <user@host:~> echo "Hello" > output.txt
  <user@host:~> cat < input.txt
  <user@host:~> ls -la >> listing.txt
  ```

- **Piping**:
  ```
  <user@host:~> cat file.txt | grep "search" | wc -l
  ```

- **Background Jobs**:
  ```
  <user@host:~> sleep 100 &
  [1] 12345
  <user@host:~> activities
  [1] 12345 : sleep - Running
  <user@host:~> fg 1
  ```

- **Command History**:
  ```
  <user@host:~> log           # Show history
  <user@host:~> log execute 2 # Run command #2 from history
  <user@host:~> log purge    # Clear history
  ```

## Implementation Details

### Parser

The parser implements a recursive descent parser for the shell grammar. It validates command syntax and rejects invalid input before execution. The grammar supports:
- Simple commands: `command arg1 arg2`
- I/O redirection: `command < input > output`
- Piping: `command1 | command2 | command3`
- Background execution: `command &`
- Sequential execution: `command1 ; command2`

### Process Management

The shell uses the following system calls for process management:
- `fork()`: Create new processes
- `execvp()`: Execute commands
- `pipe()`: Create inter-process communication channels
- `dup2()`: Redirect standard I/O
- `wait()`/`waitpid()`: Wait for child processes
- `kill()`: Send signals to processes

### Signal Handling

The shell handles the following signals:
- `SIGINT` (Ctrl-C): Interrupts the foreground process
- `SIGTSTP` (Ctrl-Z): Stops the foreground process
- `SIGCHLD`: Notifies when a child process changes state
- `SIGQUIT`: Terminates the shell

## Error Handling

The shell provides meaningful error messages for common issues:
- Command not found
- File not found or permission denied
- Syntax errors in commands
- Invalid job specifications

## Testing

The shell has been tested with:
- Basic command execution
- Complex command pipelines
- File redirection
- Background job management
- Signal handling
- Edge cases and error conditions

## Acknowledgements

This project was developed as part of the **Operating Systems and Networks** course at the **International Institute of Information Technology, Hyderabad (IIIT-H)**. Special thanks to the course instructors and teaching assistants for their guidance and support throughout the development of this shell implementation.
