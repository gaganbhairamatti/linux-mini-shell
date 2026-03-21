#include "header.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_ARGS 50
#define MAX_CMDS 20

char external_commands[152][15];
int fg_pid = 0;
char *prompt1;

extern jobs_t *head;

int last_status = 0;
char last_input[1024];

/* -------------------- TRIM -------------------- */
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

/* -------------------- SIGNAL HANDLER -------------------- */
void signal_handler(int signum)
{
    int status;

    if (signum == SIGINT)
    {
        if (fg_pid == 0)
        {
            printf("\n%s ", prompt1);
            fflush(stdout);
        }
    }

    else if (signum == SIGTSTP)
    {
        if (fg_pid != 0)
        {
            kill(fg_pid, SIGSTOP);
            insert_at_first(&head, fg_pid, last_input);
            printf("\n[Stopped] %d\n", fg_pid);
            fg_pid = 0;
        }
        else
        {
            printf("\n%s ", prompt1);
        }
    }

    else if (signum == SIGCHLD)
    {
        pid_t pid;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
        {
            if (WIFEXITED(status))
            {
                printf("\n[BG] Process %d exited (%d)\n", pid, WEXITSTATUS(status));
                printf("%s ", prompt1);
                fflush(stdout);
            }
        }
    }
}

/* -------------------- BUILT-IN COMMANDS -------------------- */
void execute_internal_commands(char *input)
{
    char cwd[256];

    if (strcmp(input, "exit") == 0)
    {
        exit(0);
    }

    else if (strcmp(input, "pwd") == 0)
    {
        getcwd(cwd, sizeof(cwd));
        printf("%s\n", cwd);
    }

    else if (strncmp(input, "cd", 2) == 0)
    {
        char *path = strtok(input + 2, " \t");

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

        fg_pid = job->pid;
        kill(fg_pid, SIGCONT);

        waitpid(fg_pid, NULL, WUNTRACED);
        fg_pid = 0;

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

/* -------------------- PIPE EXECUTION -------------------- */
void execute_pipeline(char *input)
{
    char *commands[MAX_CMDS];
    int cmd_count = 0;

    commands[cmd_count++] = strtok(input, "|");
    while ((commands[cmd_count++] = strtok(NULL, "|")) != NULL);
    cmd_count--;

    int prev_fd = 0;

    for (int i = 0; i < cmd_count; i++)
    {
        int pipe_fd[2];

        if (i < cmd_count - 1)
            pipe(pipe_fd);

        pid_t pid = fork();

        if (pid == 0)
        {
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            dup2(prev_fd, 0);
            if (i < cmd_count - 1)
                dup2(pipe_fd[1], 1);

            if (i < cmd_count - 1)
                close(pipe_fd[0]);

            char *argv[MAX_ARGS];
            int j = 0;

            char *token = strtok(commands[i], " ");
            while (token)
            {
                argv[j++] = token;
                token = strtok(NULL, " ");
            }
            argv[j] = NULL;

            execvp(argv[0], argv);
            perror("execvp");
            exit(1);
        }

        else
        {
            wait(NULL);

            if (i < cmd_count - 1)
            {
                close(pipe_fd[1]);
                prev_fd = pipe_fd[0];
            }
        }
    }
}

/* -------------------- EXTERNAL COMMAND -------------------- */
void execute_external_commands(char *input)
{
    if (strchr(input, '|'))
    {
        execute_pipeline(input);
        return;
    }

    char *argv[MAX_ARGS];
    int i = 0;

    char *token = strtok(input, " ");
    while (token)
    {
        argv[i++] = token;
        token = strtok(NULL, " ");
    }
    argv[i] = NULL;

    execvp(argv[0], argv);
    perror("execvp");
    exit(1);
}

/* -------------------- MAIN LOOP -------------------- */
void scan_input(char *prompt, char *input)
{
    prompt1 = prompt;

    signal(SIGINT, signal_handler);
    signal(SIGTSTP, signal_handler);
    signal(SIGCHLD, signal_handler);

    while (1)
    {
        printf("%s ", prompt);

        if (!fgets(input, 1024, stdin))
            continue;

        input[strcspn(input, "\n")] = '\0';
        trim_string(input);

        if (input[0] == '\0')
            continue;

        strcpy(last_input, input);

        if (strncmp(input, "PS1=", 4) == 0)
        {
            strcpy(prompt, input + 4);
            continue;
        }

        char temp[1024];
        strcpy(temp, input);

        char *cmd = strtok(temp, " ");
        int type = check_command_type(cmd, external_commands);

        if (type == BUILTIN)
        {
            execute_internal_commands(input);
        }

        else if (type == EXTERNAL)
        {
            int background = 0;

            if (input[strlen(input) - 1] == '&')
            {
                background = 1;
                input[strlen(input) - 1] = '\0';
                trim_string(input);
            }

            pid_t pid = fork();

            if (pid > 0)
            {
                if (!background)
                {
                    fg_pid = pid;
                    int status;
                    waitpid(pid, &status, WUNTRACED);

                    if (WIFEXITED(status))
                        last_status = WEXITSTATUS(status);

                    fg_pid = 0;
                }
                else
                {
                    printf("[BG] Started PID %d\n", pid);
                }
            }

            else
            {
                execute_external_commands(input);
            }
        }

        else
        {
            printf("Command not found\n");
        }
    }
}
