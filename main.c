/*Name : Gagan Bhairamatti
Date : 03-02-2026
Description : A Mini shell is a simple command-line interpreter that reads user commands,
              creates child processes using fork(), executes programs with exec() system calls, 
              and waits for completion using wait(). It demonstrates core Linux internals concepts
               like process creation,system calls, and basic command parsing.*/

#include "header.h"

int main()
{
    system("clear");
    char prompt[25]="minishell$:";
    char input_string[50];
    scan_input(prompt,input_string);
}