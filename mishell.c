#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARGS 64
#define SEPARATORS " \t\n"
#define MAX_FAVS 100
#define MAX_CMDS 10

typedef struct {
    char command[MAX_INPUT_SIZE];
    int id;
} Favorite;

Favorite favorites[MAX_FAVS];
int fav_count = 0;
char favs_file[MAX_INPUT_SIZE] = "";

void parse_input(char *input, char **args);
void execute_command(char **args);
void execute_pipe(char *input);
void handle_favs(char **args);
void add_favorite(const char *command);
void save_favorites();
void load_favorites();
void set_reminder(int seconds, const char *message);
void reminder_handler(int sig);

int main() {
    char input[MAX_INPUT_SIZE];
    char *args[MAX_ARGS];
    char *args_pipe[MAX_ARGS];
    int has_pipe;

    signal(SIGALRM, reminder_handler);

    while (1) {
        
        printf("mishell:$ ");
        fflush(stdout);

        
        if (!fgets(input, MAX_INPUT_SIZE, stdin)) {
            perror("Error leyendo la entrada");
            exit(EXIT_FAILURE);
        }

       
        if (strcmp(input, "\n") == 0) {
            continue;
        }

      
        if (strncmp(input, "exit", 4) == 0) {
            
            if (favs_file == NULL) {
                save_favorites();
            }
            
            break;
        }

        // Parsear la entrada
        has_pipe = 0;
        char *pipe_pos = strchr(input, '|');
        if (pipe_pos != NULL) {
            has_pipe = 1;
            } 
        else {
            parse_input(input, args);
        }

        
        if (strcmp(args[0], "favs") == 0) {
            handle_favs(args);
        } else if (strcmp(args[0], "set") == 0 && strcmp(args[1], "recordatorio") == 0) {
            int seconds = atoi(args[2]);
            set_reminder(seconds, args[3]);
        } else {
            // Ejecutar el comando
            if (has_pipe) {
                execute_pipe(input);
            } else {
                execute_command(args);
                add_favorite(input); 
            }
        }
    }

    return 0;
}

void parse_input(char *input, char **args) {
    int i = 0;
    args[i] = strtok(input, SEPARATORS);
    while (args[i] != NULL) {
        i++;
        args[i] = strtok(NULL, SEPARATORS);
    }
}

void execute_command(char **args) {
    pid_t pid = fork();
    int status;

    if (pid < 0) {
        perror("Error en fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Proceso hijo
        if (execvp(args[0], args) < 0) {
            perror("Comando no encontrado");
            exit(EXIT_FAILURE);
        }
    } else {
       
        wait(&status);  
    }
}


void execute_pipe(char *input) {
    char *commands[MAX_CMDS];
    int num_cmds = 0;
    char *token = strtok(input, "|");

    
    while (token != NULL && num_cmds < MAX_CMDS) {
        commands[num_cmds++] = token;
        token = strtok(NULL, "|");
    }
    commands[num_cmds] = NULL;

    int pipefds[2 * (num_cmds - 1)];
    
    
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipefds + 2 * i) == -1) {
            perror("Error en pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < num_cmds; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Error en fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) { 
            
            if (i > 0) {
                if (dup2(pipefds[2 * (i - 1)], STDIN_FILENO) == -1) {
                    perror("Error en dup2 (input)");
                    exit(EXIT_FAILURE);
                }
            }
            
            if (i < num_cmds - 1) {
                if (dup2(pipefds[2 * i + 1], STDOUT_FILENO) == -1) {
                    perror("Error en dup2 (output)");
                    exit(EXIT_FAILURE);
                }
            }

            
            for (int j = 0; j < 2 * (num_cmds - 1); j++) {
                close(pipefds[j]);
            }

            
            char *args[MAX_ARGS];
            parse_input(commands[i], args);

            // Execute the command
            execvp(args[0], args);
            perror("Error en execvp");
            exit(EXIT_FAILURE);
        }
    }

    
    for (int i = 0; i < 2 * (num_cmds - 1); i++) {
        close(pipefds[i]);
    }

    
    for (int i = 0; i < num_cmds; i++) {
        wait(NULL);
    }
}
void handle_favs(char **args) {
    if (strcmp(args[1], "crear") == 0) {
        strcpy(favs_file, args[2]);
        FILE *file = fopen(favs_file, "a");
        if (file == NULL) {
            perror("Error creando el archivo de favoritos");
            return;
        }
        fclose(file);
    } else if (strcmp(args[1], "mostrar") == 0) {
        for (int i = 0; i < fav_count; i++) {
            printf("%d: %s \n", favorites[i].id, favorites[i].command);  
        }
    } else if (strcmp(args[1], "eliminar") == 0) {
        char *token = strtok(args[2], ",");
        while (token != NULL) {
            int id = atoi(token);
            for (int i = 0; i < fav_count; i++) {
                if (favorites[i].id == id) {
                    for (int j = i; j < fav_count - 1; j++) {
                        favorites[j] = favorites[j + 1];
                    }
                    fav_count--;
                    break;
                }
            }
            token = strtok(NULL, ",");
        }
    } else if (strcmp(args[1], "buscar") == 0) {
        for (int i = 0; i < fav_count; i++) {
            if (strstr(favorites[i].command, args[2]) != NULL) {
                printf("%d: %s ", favorites[i].id, favorites[i].command);
            }
        }
        printf(" \n");
    } else if (strcmp(args[1], "borrar") == 0) {
        fav_count = 0;
        FILE *file = fopen(favs_file, "w");
        if (file == NULL) {
            perror("Error borrando el archivo de favoritos");
            return;
        }
        fclose(file);
    } else if (strcmp(args[1], "num") == 0 && strcmp(args[2], "ejecutar") == 0) {
        int id = atoi(args[3]);
        for (int i = 0; i < fav_count; i++) {
            if (favorites[i].id == id) {
                char *fav_args[MAX_ARGS];
                parse_input(favorites[i].command, fav_args);
                execute_command(fav_args);
                break;
            }
        }
    } else if (strcmp(args[1], "cargar") == 0) {
        strcpy(favs_file, args[2]);
        load_favorites();
    } else if (strcmp(args[1], "guardar") == 0) {
        
        save_favorites();
    }
}

void add_favorite(const char *command) {
    for (int i = 0; i < fav_count; i++) {
        if (strcmp(favorites[i].command, command) == 0) {
            return;  // El comando ya está en favoritos
        }
    }
    if (fav_count < MAX_FAVS) {
        favorites[fav_count].id = fav_count + 1;
        strcpy(favorites[fav_count].command, command);
        fav_count++;
    } else {
        printf("Error: Lista de favoritos llena\n");
    }
}

void save_favorites() {
    if (strlen(favs_file) == 0) {
        printf("Error: No se ha especificado un archivo para guardar los favoritos\n");
        return;
    }
    FILE *file = fopen(favs_file, "w");  
    if (file == NULL) {
        perror("Error abriendo el archivo de favoritos");
        return;
    }
    for (int i = 0; i < fav_count; i++) {
        fprintf(file, "%s \n", favorites[i].command);  
    }
    fclose(file);
}

void load_favorites() {
    if (strlen(favs_file) == 0) {
        printf("Error: No se ha especificado un archivo para cargar los favoritos\n");
        return;
    }
    FILE *file = fopen(favs_file, "r");
    if (file == NULL) {
        perror("Error abriendo el archivo de favoritos");
        return;
    }
    
    fav_count = 0;
    char line[MAX_INPUT_SIZE];

    while (fgets(line, sizeof(line), file) != NULL) {
        
        line[strcspn(line, "\n")] = '\0';

        
        if (strlen(line) > 0) {
            strcpy(favorites[fav_count].command, line);
            favorites[fav_count].id = fav_count +1;
            fav_count++;
        }
    }
        
    fclose(file);
    for (int i = 0; i < fav_count; i++) {
            printf("%d: %s \n", favorites[i].id, favorites[i].command);  
        }
}

void set_reminder(int seconds, const char *message) {
    alarm(seconds);
    printf("Recordatorio programado en %d segundos: %s\n", seconds, message);
}

void reminder_handler(int sig) {
    printf("¡Recordatorio!\n");
    fflush(stdout);
}
