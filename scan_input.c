#include "header.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

char external_commands[152][15];                                 // global array
int pid;
char *prompt1;
extern jobs_t *head;           //linked list
int last_status = 0;
char last_input[1024];

void scan_input(char *prompt, char *input_string)
{
    prompt1 = prompt;
    extract_external_commands(external_commands);
    
    //register the signals
    signal(SIGINT,signal_handler);
    signal(SIGTSTP,signal_handler);
    signal(SIGCHLD, signal_handler);

    while (1)
    {
         printf("%s ", prompt);
        // printf("\033[0;31m%s \033[0m", prompt);

        if (fgets(input_string, 1024, stdin) == NULL)
        {
            // input_string[0] = '\0';
            continue;
        }

        input_string[strcspn(input_string, "\n")] = '\0';
        trim_string(input_string);  // Trim user input

        strcpy(last_input, input_string);   // for jobs fg bg 

        if (input_string[0] == '\0')
        continue;
          
        if (strncmp(input_string, "PS1=", 4) == 0)
        {
            int valid = 1;

            for (int i = 4; input_string[i] != '\0'; i++)
            {
                if (input_string[i] == ' ')
                {
                    valid = 0;
                    break;
                }
            }

            
            if (valid && input_string[4] != '\0')
            {
                strcpy(prompt, input_string + 4);
            }
            else
            {
                printf("ERROR: Space not allowed in PS1\n");
            }
        }
        else
        {
            char *command = get_command(input_string);
            trim_string(command);  
            int type = check_command_type(command, external_commands);

            if (type == BUILTIN)
            {
                // printf("command '%s' is a BUILTIN command\n", command);
                execute_internal_commands(input_string);
             
            }
            else if (type == EXTERNAL)
            {
                // printf("command '%s' is an EXTERNAL command\n", command);
                  pid = fork();
                 int status;
                if (pid > 0)
                {
                    // wait(NULL);
                    waitpid(pid,&status,WUNTRACED);
                     if (WIFEXITED(status))
                        last_status = WEXITSTATUS(status);
                    pid=0;
                } 
                else if(pid==0)
                {
                    signal(SIGINT,SIG_DFL);
                    signal(SIGTSTP,SIG_DFL);

                    execute_external_commands(input_string);
                      exit(1);
                }
                
            }
            else
            {
                 printf("command '%s' not found\n", command);
            }
        }
    }
}

void trim_string(char *str)
{
   
    int start = 0;
    while(str[start] == ' ' || str[start] == '\t')
    {
        start++;
    }
    if(start > 0)
    {
        memmove(str, str + start, strlen(str + start) + 1);
    }

    int end = strlen(str) - 1;
    while(end >= 0 && (str[end] == ' ' || str[end] == '\r' || str[end] == '\n'))
    {
        str[end--] = '\0';
    }
}

void extract_external_commands(char external_commands[152][15])
{
    int fd = open("ext_cmd.txt", O_RDONLY);
    if (fd < 0)
    {
        perror("open");
        return;
    }

    int i = 0, j = 0;
    char ch;

    while (read(fd, &ch, 1) == 1)
    {
        if (ch == '\n')
        {
            external_commands[i][j] = '\0';
            trim_string(external_commands[i]);  // Trim the line
            i++;
            j = 0;
            if (i >= 152)
                break;
        }
        else if (j < 14)
        {
            external_commands[i][j++] = ch;
        }
    }

    if (j > 0 && i < 152)
    {
        external_commands[i][j] = '\0';
        trim_string(external_commands[i]);  // Trim last line
        i++;
    }

    close(fd);

    
}

void execute_internal_commands(char *input_string)
{
    char buff[50];
    int status;
    if (strcmp(input_string, "exit") == 0)            //command
    {
        exit(0);
    }

    if (strcmp(input_string, "pwd") == 0)              //command pwd
    {
        getcwd(buff, 50);
        printf("%s\n", buff);
    }

    if (strncmp(input_string, "cd", 2) == 0)            //command cd 
    {
        chdir(input_string + 3);
        getcwd(buff, 50);
        printf("%s\n", buff);
    }

    if (strcmp(input_string, "echo $$") == 0)          // command echo $$
    {
        printf("%d \n",getpid());
    }
    
    if(strcmp(input_string,"echo $?")==0)                            //performing command echo $?
    {
        // if(WIFEXITED(status))
        // {
        //     printf("%d \n",WEXITSTATUS(status));
        // }
        // else
        // {
        //     //      printf("%d \n",WEXITSTATUS(status));            //status is not changing ask sir this,old status is printing
        //     printf("1\n");
        // }
        if (strcmp(input_string, "echo $?") == 0)
        {
            printf("%d\n", last_status);
        }

    }

    
    if(strcmp(input_string,"echo $SHELL")==0)                      //performing echo $SHELL
    {
       char *ptr = getenv("SHELL");
       printf("%s\n",ptr);
    }

     else if (strcmp("jobs", input_string) == 0)
    {
        jobs_t *temp = head;
        int job_id = 1;

        while (temp)
        {
            // Print actual command stored in struct
            printf("[%d] Stopped %d %s\n",
                   job_id++, temp->pid, temp->command);
            temp = temp->link;
        }

    }

    else if (strcmp("fg", input_string) == 0)
{
    if (head == NULL)
    {
        printf("fg: no jobs\n");
        return ;
    }

    // Remove job from the list
    jobs_t *job = head;
    head = head->link;

    pid = job->pid;               // set as foreground pid
    printf("%s\n", job->command); // show which job is resumed

    // Continue the stopped process
    kill(pid, SIGCONT);

    int status;
    waitpid(pid, &status, WUNTRACED); // wait for stop or exit
    delete_first(&head);

    
    free(job);

    // Print prompt again

    fflush(stdout);


}

    // 10. bg
    else if (strcmp("bg", input_string) == 0)
{
    if (head == NULL)
    {
        printf("bg: no jobs\n");
           return ;
    }

    jobs_t *job = head;   // next stopped job (older after fg)
    head = head->link;

    // kill(-job->pid, SIGCONT);
    // delete_first(&head);

    printf("[%d] %s &\n", job->pid, job->command);

    free(job);
   
}

}

void execute_external_commands(char *input_string)
{
    char *argv[20];
    int i = 0, j = 0;
    int flag = 0;   // 0 = space, 1 = word

    while (input_string[i] != '\0')
    {
       
        if (input_string[i] != ' ' && flag == 0)
        {
            argv[j++] = &input_string[i];   // word start
            flag = 1;
        }

        if (input_string[i] == ' ' && flag == 1)
        {
            input_string[i] = '\0';         // end of word
            flag = 0;
        }

        i++;
    }
    argv[j] = NULL;
   
    int pipe_count=0;
    i=0;
    while (argv[i] != NULL)
    {
       
     
        if(strcmp(argv[i],"|")==0)
        {
            pipe_count++;
        }
        i++;
     }
   
     if(pipe_count==0)
     {
        execvp(argv[0],argv);
     }

     else 
     {

      //  printf("pipe count %d \n",pipe_count);
        
        int cmd[20];                            //create array
        int cmd_count=0;
        cmd[cmd_count++] = 0;   

        for(int i=0;argv[i]!=NULL;i++)
        {
            if(strcmp(argv[i],"|")==0)                      //checking pipe is present or not
            {
                argv[i] = NULL;                               //'|' pipe is present make it NULl
                cmd[cmd_count++] = i+1;                                // next command is  wc
                                         
            }  
        }
        int fd_in = 0;
        for(int i=0;i<=pipe_count;i++)              
        {
            int pipe_fd[2];

            if(i!=pipe_count)
            {
                pipe(pipe_fd);
            }

            int pid = fork();

            if(pid>0)                                               //parent processs
            {
                wait(NULL);

                if(i!=pipe_count)
                {
                    // dup2(pipe_fd[0],0);
                    close(pipe_fd[1]);
                    if(fd_in!=0)
                    {
                        close(fd_in);
                    }
                    fd_in = pipe_fd[0];
                }  
            }

            else if(pid==0)                                     //child process
            {
                dup2(fd_in, 0); 
                if(i!=pipe_count)
                {
                    dup2(pipe_fd[1],1);
                     close(pipe_fd[0]);
                }
                execvp(argv[cmd[i]],argv+cmd[i]);
                exit(1);
            }
        }
    }   
}


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
    }

    if (signum == SIGTSTP)
    {
        if (pid == 0)
        {
            printf("\n%s ", prompt1);
            fflush(stdout);
        }
         else
        {
            // Stop the foreground process
            kill(pid, SIGSTOP);

            // Save stopped process in job list
            insert_at_first(&head, pid, last_input);

            printf("\n");
            //pid = 0;  // reset foreground pid
        }
    }
    if(signum==SIGCHLD)
    {
        while (waitpid(-1, &status, WNOHANG) > 0);
    }
}


int insert_at_first(jobs_t **head, pid_t pid, char *cmd)
{
    jobs_t *new;
    new = malloc(sizeof(jobs_t));
    if (new == NULL)
    {
        return -1; // malloc failed
    }

    new->pid = pid;

    // Copy the command name into the struct
    if (cmd != NULL)
        strncpy(new->command, cmd, sizeof(new->command) - 1);

    new->command[sizeof(new->command) - 1] = '\0'; // ensure null termination

    new->link = *head;
    *head = new;

    return 0; // success
}

int delete_first(jobs_t **head)
{
    if (*head == NULL)
    {
        return -1;
    }
    else
    {
        jobs_t *temp = *head;
        *head = temp->link;
        free(temp);
        return 0;
    }
}
