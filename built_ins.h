
#include <stddef.h>

#ifdef VERSION
#else
#define VERSION "unknown"
#endif

// Alias type shared with main
typedef struct Alias {
	char *name;
	char *command;
} Alias;

// Expose read_alias so built-ins can list aliases
Alias *read_alias();

// last command status (set in main)
extern int last_command_status;

//Changes the actual path of the shell
//Returns 0 if success, 1 if error
int builtin_cd(char** args);


int builtin_exit(char** args);

//Prints all the builins
int builtin_help(char** args);

//Prints args to the stdout
int builtin_echo(char** args);

int builtin_show_alias(char** args);
