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
        while ((child = waitpid(-1, &status, WNOHANG)) > 0)
        {
            if (WIFEXITED(status))
            {
                printf("\n[BG] %d exited (%d)\n", child, WEXITSTATUS(status));
                printf("%s ", prompt1);
                fflush(stdout);
            }
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

/* ---------------- PIPE (FIXED PARALLEL) ---------------- */
void execute_pipeline(char *input)
{
    char *cmds[20];
    int n = 0;

    cmds[n++] = strtok(input, "|");
    while ((cmds[n++] = strtok(NULL, "|")) != NULL);
    n--;

    int pipe_fd[2];
    int prev_fd = 0;

    pid_t pids[20];

    for (int i = 0; i < n; i++)
    {
        pipe(pipe_fd);

        if ((pids[i] = fork()) == 0)
        {
            dup2(prev_fd, 0);

            if (i != n - 1)
                dup2(pipe_fd[1], 1);

            close(pipe_fd[0]);

            char *argv[20];
            int j = 0;

            char *token = strtok(cmds[i], " ");
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

        close(pipe_fd[1]);
        prev_fd = pipe_fd[0];
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
        return;
    }

    char *argv[20];
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

        if (!fgets(input, 1024, stdin))
            continue;

        input[strcspn(input, "\n")] = '\0';
        trim_string(input);

        if (input[0] == '\0')
            continue;

        strcpy(last_input, input);

        /* -------- PS1 STRICT -------- */
        if (strncmp(input, "PS1=", 4) == 0 && strchr(input, ' ') == NULL)
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

            int len = strlen(input);
            if (input[len - 1] == '&')
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
            else
            {
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                execute_external_commands(input);
            }
        }
        else
        {
            printf("command not found\n");
        }
    }
}
