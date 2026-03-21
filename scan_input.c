#include "header.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

char external_commands[152][15];
int pid = 0;
char *prompt1;
extern jobs_t *head;

int last_status = 0;
char last_input[1024];

/* ---------------- TRIM ---------------- */
void trim_string(char *str)
{
    int start = 0;
    while (str[start] == ' ' || str[start] == '\t')
        start++;

    if (start > 0)
        memmove(str, str + start, strlen(str + start) + 1);

    int end = strlen(str) - 1;
    while (end >= 0 && (str[end] == ' ' || str[end] == '\n'))
        str[end--] = '\0';
}

/* ---------------- SIGNAL HANDLER ---------------- */
void signal_handler(int signum)
{
    int status;

    if (signum == SIGINT)
    {
        if (pid == 0)
        {
            printf("\n%s ", prompt1);
            fflush(stdout);
        }
        else
        {
            kill(pid, SIGINT);
        }
    }
    else if (signum == SIGTSTP)
    {
        if (pid == 0)
        {
            printf("\n%s ", prompt1);
            fflush(stdout);
        }
        else
        {
            kill(pid, SIGSTOP);
            insert_at_first(&head, pid, last_input);
            printf("\n[%d]+ Stopped %s\n", pid, last_input);
            pid = 0;
        }
    }
    else if (signum == SIGCHLD)
    {
        pid_t child;
        // Reaping children silently to avoid non-async-safe printf calls in handler
        while ((child = waitpid(-1, &status, WNOHANG)) > 0)
        {
            // Child reaped
        }
    }
}

/* ---------------- BUILTINS ---------------- */
void execute_internal_commands(char *input)
{
    char cwd[256];

    if (strcmp(input, "exit") == 0)
    {
        exit(0);
    }
    else if (strcmp(input, "pwd") == 0)
    {
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            printf("%s\n", cwd);
    }
    else if (strncmp(input, "cd", 2) == 0)
    {
        char *saveptr;
        strtok_r(input, " \t", &saveptr); // skip "cd"
        char *path = strtok_r(NULL, " \t", &saveptr);

        if (path == NULL)
            chdir(getenv("HOME"));
        else if (chdir(path) != 0)
            perror("cd");
    }
    else if (strcmp(input, "echo $$") == 0)
    {
        printf("%d\n", getpid());
    }
    else if (strcmp(input, "echo $?") == 0)
    {
        printf("%d\n", last_status);
    }
    else if (strcmp(input, "echo $SHELL") == 0)
    {
        printf("./msh\n");
    }
    else if (strcmp(input, "jobs") == 0)
    {
        jobs_t *temp = head;
        int id = 1;
        while (temp)
        {
            printf("[%d] %d %s\n", id++, temp->pid, temp->command);
            temp = temp->link;
        }
    }
    else if (strcmp(input, "fg") == 0)
    {
        if (!head)
        {
            printf("fg: no jobs\n");
            return;
        }
        jobs_t *job = head;
        head = head->link;
        pid = job->pid;
        kill(pid, SIGCONT);
        int status;
        waitpid(pid, &status, WUNTRACED);
        if (WIFEXITED(status))
            last_status = WEXITSTATUS(status);
        pid = 0;
        free(job);
    }
    else if (strcmp(input, "bg") == 0)
    {
        if (!head)
        {
            printf("bg: no jobs\n");
            return;
        }
        jobs_t *job = head;
        head = head->link;
        kill(job->pid, SIGCONT);
        printf("[BG] %d resumed\n", job->pid);
        free(job);
    }
}

/* ---------------- PIPE (FIXED RE-ENTRANCY & FD LEAKS) ---------------- */
void execute_pipeline(char *input)
{
    char *cmds[20];
    int n = 0;
    char *saveptr1, *saveptr2;

    // Use strtok_r to avoid corrupting internal pointers
    cmds[n++] = strtok_r(input, "|", &saveptr1);
    while (n < 20 && (cmds[n] = strtok_r(NULL, "|", &saveptr1)) != NULL) {
        n++;
    }

    int pipe_fd[2];
    int prev_fd = 0;
    pid_t pids[20];

    for (int i = 0; i < n; i++)
    {
        if (i < n - 1) {
            if (pipe(pipe_fd) == -1) {
                perror("pipe");
                return;
            }
        }

        if ((pids[i] = fork()) == 0)
        {
            if (prev_fd != 0) {
                dup2(prev_fd, 0);
                close(prev_fd);
            }

            if (i < n - 1) {
                dup2(pipe_fd[1], 1);
                close(pipe_fd[0]);
                close(pipe_fd[1]);
            }

            char *argv[20];
            int j = 0;
            char *token = strtok_r(cmds[i], " \t", &saveptr2);
            while (token && j < 19)
            {
                argv[j++] = token;
                token = strtok_r(NULL, " \t", &saveptr2);
            }
            argv[j] = NULL;

            execvp(argv[0], argv);
            perror("execvp");
            exit(1);
        }

        // Parent: Close FDs that are no longer needed
        if (prev_fd != 0)
            close(prev_fd);
        
        if (i < n - 1) {
            close(pipe_fd[1]);
            prev_fd = pipe_fd[0]; // Save read end for next command
        }
    }

    for (int i = 0; i < n; i++)
        waitpid(pids[i], NULL, 0);
}

/* ---------------- EXTERNAL ---------------- */
void execute_external_commands(char *input)
{
    if (strchr(input, '|'))
    {
        execute_pipeline(input);
        exit(0); // Exit child after pipeline finishes
    }

    char *argv[20];
    int i = 0;
    char *saveptr;
    char *token = strtok_r(input, " \t", &saveptr);
    while (token && i < 19)
    {
        argv[i++] = token;
        token = strtok_r(NULL, " \t", &saveptr);
    }
    argv[i] = NULL;

    execvp(argv[0], argv);
    perror("execvp");
    exit(1);
}

/* ---------------- MAIN LOOP ---------------- */
void scan_input(char *prompt, char *input)
{
    prompt1 = prompt;

    signal(SIGINT, signal_handler);
    signal(SIGTSTP, signal_handler);
    signal(SIGCHLD, signal_handler);

    while (1)
    {
        printf("%s ", prompt);
        fflush(stdout);

        if (!fgets(input, 1024, stdin))
            break;

        input[strcspn(input, "\n")] = '\0';
        trim_string(input);

        if (input[0] == '\0')
            continue;

        strcpy(last_input, input);

        if (strncmp(input, "PS1=", 4) == 0 && strchr(input, ' ') == NULL)
        {
            strcpy(prompt, input + 4);
            continue;
        }

        char temp[1024];
        strncpy(temp, input, sizeof(temp)-1);
        temp[sizeof(temp)-1] = '\0';

        char *saveptr;
        char *cmd = strtok_r(temp, " ", &saveptr);
        int type = check_command_type(cmd, external_commands);

        if (type == BUILTIN)
        {
            execute_internal_commands(input);
        }
        else if (type == EXTERNAL)
        {
            int background = 0;
            int len = strlen(input);
            if (len > 0 && input[len - 1] == '&')
            {
                background = 1;
                input[len - 1] = '\0';
                trim_string(input);
            }

            int status;
            pid = fork();

            if (pid > 0)
            {
                if (!background)
                {
                    waitpid(pid, &status, WUNTRACED);
                    if (WIFEXITED(status))
                        last_status = WEXITSTATUS(status);
                    pid = 0;
                }
                else
                {
                    printf("[BG] %d started\n", pid);
                }
            }
            else if (pid == 0)
            {
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                execute_external_commands(input);
            }
            else {
                perror("fork");
            }
        }
        else
        {
            printf("%s: command not found\n", cmd);
        }
    }
}
