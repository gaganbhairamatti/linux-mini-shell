#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdio_ext.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
 

#define BUILTIN		1
#define EXTERNAL	2
#define NO_COMMAND  3

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

extern char *builtins[];


typedef struct jobs
{
    pid_t pid;
    char command[100];
    struct jobs *link;
} jobs_t;

// Function declarations (must match exactly your definitions)

void extract_external_commands(char external_commands[152][15]);
int check_command_type(char *command, char external_commands[152][15]);
void scan_input(char *prompt, char *input_string);
char *get_command(char *input_string);
void copy_change(char *prompt, char *input_string);
void echo(char *input_string, int status);
void execute_internal_commands(char *input_string);
void signal_handler(int sig_num);

// // own defined functions
void scan_input(char *prompt,char *input_string);
void execute_external_commands(char *input_string);
void trim_string(char *str); 
void signal_handler(int signum);

int insert_at_first(jobs_t **head, pid_t pid, char *cmd);
int delete_first(jobs_t **head);

#endif
