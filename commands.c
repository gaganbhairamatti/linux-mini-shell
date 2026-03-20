#include "header.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

jobs_t *head = NULL;           //linked list

char *builtins[] = {"echo", "printf", "read", "cd", "pwd", "pushd", "popd", "dirs", "let", "eval",
                    "set", "unset", "export", "declare", "typeset", "readonly", "getopts", "source",
                    "exit", "exec", "shopt", "caller", "true", "type", "hash", "bind", "help","fg","bg","jobs", NULL};

char *get_command(char *input_string)
{
    static char cmd[50];
    int i=0;
    while(input_string[i] != ' ' && input_string[i] != '\0')
    {
        cmd[i] = input_string[i];
        i++;
    }
    cmd[i] = '\0';
    // printf("cmd = %s\n",cmd);
    return cmd;
}

int check_command_type(char *command, char external_commands[152][15])
{
    int i = 0;

    // Check builtins
    while (builtins[i] != NULL)
    {
        if (strcmp(command, builtins[i]) == 0)
            return BUILTIN;
        i++;
    }

    // Check external commands
    i = 0;
    while (i < 152 && external_commands[i][0] != '\0')  
    {
        if (strcmp(command, external_commands[i]) == 0)
        {
            return EXTERNAL;
        }
        i++;
    }

    return NO_COMMAND;
}
