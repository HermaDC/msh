#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>

#include "built_ins.h"

#define BUFF_SIZE 1024
#define TOK_SIZE 64
#define TOK_SEPARETORS " \t\r\n\a"

#define RED_COLOR "\x1b[31m"
#define GREEN_COLOR "\x1b[32m"
#define YELLOW_COLOR "\x1b[33m"
#define PALE_YELLOW_COLOR "\x1b[93m"
#define BLUE_COLOR "\x1b[34m"
#define LIGHT_BLUE_COLOR "\x1b[94m"
#define RESET_COLOR "\x1b[0m"


//TODO historial con todos los args incluido redireccionamientos OK
//TODO args ML en expnad_alias
//TODO split_line_quotes comprobar realloc
//TODO read alias ver alias vacio
//TODO mensajes en el shell

const char *home_directory;

typedef int (*builtin_func)(char** args);

// Alias is declared in built_ins.h

int last_command_status = 0;

typedef struct Builtin{
    char *name;
    builtin_func func;
} Builtin;

Builtin builtins[] = {
    {"echo", builtin_echo},
    {"cd", builtin_cd},
    {"exit", builtin_exit},
    {"help", builtin_help},
    {"alias", builtin_show_alias},
    {NULL, NULL} // marca el final
};

static inline void print_prompt(int status_command) {
    char cwd[1024];
    int command_error = (status_command != 0);

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("\n%s\n ", cwd);
    }

    if (access(".git", F_OK) != -1) {
        printf(PALE_YELLOW_COLOR "[git] " RESET_COLOR);
    }

    if (command_error) {
        printf(RED_COLOR "> " RESET_COLOR);
    } else {
        printf(GREEN_COLOR "> " RESET_COLOR);
    }
}

//free after use
char *read_line(){
    char *buffer = malloc(BUFF_SIZE * sizeof(char));
    int position = 0;
    int actual_buff_size = BUFF_SIZE;
    int c;

    if (!buffer )
    {
        fprintf(stdout, "msh error: Alocation error\n");
        exit(EXIT_FAILURE);
    }
    
    while(1){
        c = getchar();
        if(c == EOF || c == '\n'){
            buffer[position] = '\0';
            return buffer;
        }
        else{   
            buffer[position] = c;
        }
        position++;
        if( position >= actual_buff_size){
            actual_buff_size +=BUFF_SIZE;
            buffer = realloc(buffer, actual_buff_size);
            if (!buffer)
            {
                fprintf(stderr, "msh error: failed to alocate memory");
                exit(EXIT_FAILURE);
            }
            
        }
    }
    
}

//free after use
char **split_line(char* line){
    int bufsize = TOK_SIZE;
    int position = 0;
    char **tokens = malloc(TOK_SIZE*sizeof(char*));
    char *token;

    if(!tokens){
        fprintf(stderr, "msh error. Failed allocation");
        exit(EXIT_FAILURE);
    }
    token = strtok(line, TOK_SEPARETORS);
    while(token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
        bufsize += TOK_SIZE;
        tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, TOK_SEPARETORS);
    }
    tokens[position] = NULL;
    return tokens;

}

//free args[i] after use
char **split_line_quotes(char *line) {
    int bufsize = TOK_SIZE;
    int position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    if (!tokens) {
        fprintf(stderr, "msh: allocation error\n");
        return NULL;
    }
    char token[1024];
    int tok_pos = 0;
    int in_quotes = 0;

    for (int i = 0; line[i] != '\0'; i++) {
        char c = line[i];

        if (c == '"') {
            in_quotes = !in_quotes;
        } else if ((c == ' ' || c == '\t') && !in_quotes) {
            if (tok_pos != 0) {
                token[tok_pos] = '\0';
                tokens[position++] = strdup(token);
                tok_pos = 0;
                if (position >= bufsize) {
                    bufsize += TOK_SIZE;
                    tokens = realloc(tokens, bufsize * sizeof(char*));
                    if (!tokens) {
                        fprintf(stderr, "msh: allocation error\n");
                        return NULL;
                    }
                }
            }
        } else {
            token[tok_pos++] = c;
        }
    }

    if (tok_pos != 0) {
        token[tok_pos] = '\0';
        tokens[position++] = strdup(token);
    }

    tokens[position] = NULL;
    return tokens;
}

//free name and command after use, returns NULL if error, empty alias if no file
//en el final hay un alias con name NULL para saber el final
Alias *read_alias(){
    struct passwd *pw = getpwuid(getuid());
    if (!pw) {
        fprintf(stderr, "No se pudo obtener el usuario\n");
        return NULL;
    }

    const char *home = pw->pw_dir;

    char path[1024];
    snprintf(path, sizeof(path), "%s/.aliasrc", home);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        Alias *empty = malloc(sizeof(Alias));
        if (empty) {
            empty[0].name = NULL;
            empty[0].command = NULL;
        }
        return empty;
    }
    //el alias es X=Y donde X es el nombre e Y el comando
    //X es la cadena entera de stdin
    //
    //leer el archivo
    //
    int buffer = 0;
    char line[1024];
    Alias *alias = NULL;
    while(fgets(line, sizeof(line), file)) {
        char *equals = strchr(line, '=');
        if(equals != NULL) {
            *equals = '\0';
            char *name = line;
            char *command = equals + 1;
            command[strcspn(command, "\n")] = 0; // eliminar salto de linea

            Alias *tmp = realloc(alias, sizeof(Alias) * (buffer + 1));
            if (!tmp) {
                fprintf(stderr, "msh error: failed to allocate memory for aliases\n");
                // free previously allocated entries
                for (int j = 0; j < buffer; j++) {
                    free(alias[j].name);
                    free(alias[j].command);
                }
                free(alias);
                fclose(file);
                return NULL;
            }
            alias = tmp;
            alias[buffer].name = strdup(name);
            alias[buffer].command = strdup(command);
            buffer++;
        }
    }
    Alias *tmp2 = realloc(alias, sizeof(Alias) * (buffer + 1));
    if (!tmp2) {
        for (int j = 0; j < buffer; j++) {
            free(alias[j].name);
            free(alias[j].command);
        }
        free(alias);
        fclose(file);
        return NULL;
    }
    alias = tmp2;
    // append a sentinel entry so callers can iterate until name == NULL
    alias[buffer].name = NULL;
    alias[buffer].command = NULL;
    fclose(file);
    return alias;
}

//Devuelve NULL si falla 
//free new[i] despues de usar
char **expand_alias(Alias *alias, char **args)
{
    char **alias_tokens = NULL;
    char **new_args = NULL;
    int alias_len = 0;
    int args_len = 0;
    int total = 0;
    int i = 0;

    /* Validaciones básicas */
    if (!alias || !alias->command || !args || !args[0])
        return NULL;

    /* Contar args */
    while (args[args_len] != NULL)
        args_len++;

    /* Tokenizar comando del alias */
    alias_tokens = split_line_quotes(alias->command);
    if (!alias_tokens)
        return NULL;

    while (alias_tokens[alias_len] != NULL)
        alias_len++;

    /* alias tokens + (args sin args[0]) + NULL */
    total = alias_len + (args_len - 1) + 1;

    new_args = malloc(total * sizeof(char *));
    if (!new_args)
        goto cleanup_alias;

    /* Copiar tokens del alias */
    for (i = 0; i < alias_len; i++) {
        new_args[i] = strdup(alias_tokens[i]);
        if (!new_args[i])
            goto cleanup_new_args;
    }

    /* Copiar args originales excepto args[0] */
    for (int j = 1; j < args_len; j++, i++) {
        new_args[i] = strdup(args[j]);
        if (!new_args[i])
            goto cleanup_new_args;
    }

    new_args[i] = NULL;

    /* Liberar alias_tokens */
    for (int k = 0; k < alias_len; k++)
        free(alias_tokens[k]);
    free(alias_tokens);

    return new_args;

cleanup_new_args:
    for (int k = 0; k < i; k++)
        free(new_args[k]);
    free(new_args);

cleanup_alias:
    for (int k = 0; k < alias_len; k++)
        free(alias_tokens[k]);
    free(alias_tokens);

    return NULL;
}

Alias* find_alias(Alias *alias_list, const char *name) {
    for (int i = 0; alias_list[i].name != NULL; i++) {
        if (strcmp(alias_list[i].name, name) == 0) {
            return &alias_list[i];
        }
    }
    return NULL;
}
// Modifica args para eliminar los tokens de redirección
// Devuelve el nombre del archivo de salida o NULL si no hay redirección
char *control_redirection_output(char** args, int* file_descriptor) {
    /* default: no redirection */
    *file_descriptor = 0;
    for (size_t i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            printf("Redirección de salida detectada\n");
            if (args[i + 1] == NULL) {
                fprintf(stderr, "msh: expected argument after '>'\n");
                *file_descriptor = -1; /* signal error: missing filename */
                return NULL;
            }
            *file_descriptor = 1;
            char *fname = args[i + 1];
            args[i] = NULL;
            return fname;
        } else if (strcmp(args[i], ">>") == 0) {
            printf("Redirección de salida detectada\n");
            if (args[i + 1] == NULL) {
                fprintf(stderr, "msh: expected argument after '>>'\n");
                *file_descriptor = -1;
                return NULL;
            }
            *file_descriptor = 2;
            char *fname = args[i + 1];
            args[i] = NULL;
            return fname;
        }
    }
    return NULL;
}

//returns status of command
int execute_command(char** args, int write_file, char* output_file){
    int status;
    int status_command;
    pid_t pid = fork();
        if (pid == 0) { //Child process
            signal(SIGINT, SIG_DFL);
            if (write_file ==1) {
                printf("Redirigiendo salida a: %s\n", output_file);
                int fd = open(output_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
                if (fd < 0) { perror("open"); exit(1); }

                // Redirige stdout al archivo
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            else if (write_file ==2) {
                printf("Redirigiendo salida a: %s\n", output_file);
                int fd = open(output_file, O_CREAT | O_WRONLY | O_APPEND, 0644);
                if (fd < 0) { perror("open"); exit(1); }

                // Redirige stdout al archivo
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            //printf("Ejecutando comando: %s\n", args[0]);
            
            execvp(args[0], args);
            status_command = 0;
            perror("msh");
            exit(EXIT_FAILURE);

        } else if (pid < 0) {
            perror("msh");
        } else {
            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) {
                status_command = WEXITSTATUS(status);
            } else {
                status_command = -1; // fallo
            }
        }
        return status_command;
}

//returns -1 if not builtin, else the status of the builtin
int check_builtins(char** args){
    for (int i = 0; builtins[i].name != NULL; i++) {
        if (strcmp(args[0], builtins[i].name) == 0) {
            return builtins[i].func(args);
        }
    }
    return -1; //no es builtin
}

void shell_loop() {
    signal(SIGINT, SIG_IGN);
    int shell_status = 1;
    int file_descriptor = 0;

    
    while(shell_status){
        print_prompt(last_command_status);
        char *line = read_line();
        char **args = split_line_quotes(line);

        if (args[0] == NULL) {
            free(line);
            for(int i = 0; args[i] != NULL; i++) {
                free(args[i]);
            }
            free(args);
            continue; // empty command
        }

        char *output_file = control_redirection_output(args, &file_descriptor);
        if (file_descriptor == -1) {
            free(line);
            free(args);
            continue;
        }

        Alias *alias = read_alias();
        if (alias == NULL) {
            free(line);
            for(int i = 0; args[i] != NULL; i++) {
                free(args[i]);
            }
            free(args); 
            continue;
        }
        Alias *found_alias = find_alias(alias, args[0]);
        if (found_alias == NULL) {
            for (size_t i = 0; alias[i].name != NULL; i++){
                free(alias[i].name);
                free(alias[i].command);
            }
            free(alias);
        }
        else {
            char **new_args = expand_alias(found_alias, args);
            if (new_args != NULL) {
                for(int i = 0; args[i] != NULL; i++) {
                    free(args[i]);
                }
                free(args);
                args = new_args;
            }
            for (size_t i = 0; alias[i].name != NULL; i++){
                free(alias[i].name);
                free(alias[i].command);
            }
            free(alias);
        }
        last_command_status = check_builtins(args);
        if(last_command_status != -1) {
            free(line);
            for(int i = 0; args[i] != NULL; i++) {
                free(args[i]);
            }
            free(args);
            continue;
        }

        last_command_status = execute_command(args, file_descriptor, output_file);

        free(line);
        for(int i = 0; args[i] != NULL; i++) {
            free(args[i]);
        }
        free(args);
    }

}

int main(int argc, char** argv){
    if (argc > 1) {
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
            printf("msh shell, version %s\n", VERSION);
            return EXIT_SUCCESS;
        } else {
            fprintf(stderr, "Usage: %s [--version|-v]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }
    shell_loop(); 
    return EXIT_SUCCESS;
}