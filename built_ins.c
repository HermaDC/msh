#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "built_ins.h"



char* expose_HOME(){
    return getenv("HOME");
}

int builtin_cd(char** args){
    if(args[1] == NULL){
        fprintf(stderr, "msh: expected argument to \"cd\"\n");
    } else {
        if (strcmp(args[1],"~") == 0){
            char *home = expose_HOME();
            if (home == NULL) {
                fprintf(stderr, "msh: HOME environment variable not set\n");
                return 1;
            }
            if(chdir(home) != 0){
                perror("msh");
            }
            return 0;
        }
        if(chdir(args[1]) != 0){
            perror("msh");
        }
    }
    return 0;
}

int builtin_echo(char** args){
    int i = 1;
    while (args[i] != NULL) {
        if (strcmp(args[i], "$?") == 0) {
            printf("%d", last_command_status);
        } else {
            printf("%s", args[i]);
        }
        if (args[i + 1] != NULL)
            printf(" ");
        i++;
    }
    printf("\n");
    return 0;
}
int builtin_help(char** args){
    printf("msh: probably worst shell ever, version %s\n", VERSION);
    printf("Shell built-in commands:\n");
    printf("  cd [dir]      Change the current directory to 'dir'\n");
    printf("  echo [args]   Print 'args' to the standard output\n");
    printf("  help          Display this help message\n");
    printf("  exit [args]   Exit the shell with exit status 'args'\n");
    return 0;
}
int builtin_exit(char** args){
    printf("Exiting shell...\n");
    if (args[1] == NULL) {
        exit(0);
    }
    else {
        int exit_status = atoi(args[1]);
        exit(exit_status);
    }
    return 0;
}

int builtin_show_alias(char **args){
    Alias *alias = read_alias();
    if (alias == NULL) {
        fprintf(stderr, "msh: failed to read aliases\n");
        return 1;
    }
    for (size_t i = 0; alias[i].name != NULL; i++) {
        printf("%s='%s'\n", alias[i].name, alias[i].command);
        free(alias[i].name);
        free(alias[i].command);
    }
    free(alias);
    return 0;
}