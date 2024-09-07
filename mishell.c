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

typedef struct {
    char command[MAX_INPUT_SIZE];
    int id;
} Favorite;

Favorite favorites[MAX_FAVS];
int fav_count = 0;
char favs_file[MAX_INPUT_SIZE] = "";

void parse_input(char *input, char **args);
void execute_command(char **args);
void execute_pipe(char **args1, char **args2);
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
        // Mostrar el prompt
        printf("mishell:$ ");
        fflush(stdout);

        // Leer la entrada
        if (!fgets(input, MAX_INPUT_SIZE, stdin)) {
            perror("Error leyendo la entrada");
            exit(EXIT_FAILURE);
        }

        // Si el usuario solo presiona "Enter"
        if (strcmp(input, "\n") == 0) {
            continue;
        }

        // Verificar si es el comando "exit"
        if (strncmp(input, "exit", 4) == 0) {
            save_favorites();
            break;
        }

        // Parsear la entrada
        has_pipe = 0;
        char *pipe_pos = strchr(input, '|');
        if (pipe_pos != NULL) {
            *pipe_pos = '\0';
            parse_input(input, args);
            parse_input(pipe_pos + 1, args_pipe);
            has_pipe = 1;
        } else {
            parse_input(input, args);
        }

        // Verificar si es el comando "favs" o "set recordatorio"
        if (strcmp(args[0], "favs") == 0) {
            handle_favs(args);
        } else if (strcmp(args[0], "set") == 0 && strcmp(args[1], "recordatorio") == 0) {
            int seconds = atoi(args[2]);
            set_reminder(seconds, args[3]);
        } else {
            // Ejecutar el comando
            if (has_pipe) {
                execute_pipe(args, args_pipe);
            } else {
                execute_command(args);
                add_favorite(input);  // Agregar a favoritos automáticamente
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
        if (execvp(args[0], args) < 0) {
            perror("Comando no encontrado");
            exit(EXIT_FAILURE);
        }
    } else {
        waitpid(pid, &status, 0);
    }
}

void execute_pipe(char **args1, char **args2) {
    int pipefd[2];
    pid_t pid1, pid2;
    int status;

    if (pipe(pipefd) == -1) {
        perror("Error en pipe");
        exit(EXIT_FAILURE);
    }

    pid1 = fork();
    if (pid1 < 0) {
        perror("Error en fork");
        exit(EXIT_FAILURE);
    }

    if (pid1 == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        if (execvp(args1[0], args1) < 0) {
            perror("Comando no encontrado");
            exit(EXIT_FAILURE);
        }
    } else {
        pid2 = fork();
        if (pid2 < 0) {
            perror("Error en fork");
            exit(EXIT_FAILURE);
        }

        if (pid2 == 0) {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            if (execvp(args2[0], args2) < 0) {
                perror("Comando no encontrado");
                exit(EXIT_FAILURE);
            }
        } else {
            close(pipefd[0]);
            close(pipefd[1]);
            waitpid(pid1, &status, 0);
            waitpid(pid2, &status, 0);
        }
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
    FILE *file = fopen(favs_file, "w");  // Abrir el archivo en modo de escritura ("w")
    if (file == NULL) {
        perror("Error abriendo el archivo de favoritos");
        return;
    }
    for (int i = 0; i < fav_count; i++) {
        fprintf(file, "%s \n", favorites[i].command);  // Asegurarse de que cada comando esté en una nueva línea
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
    while (fgets(favorites[fav_count].command, MAX_INPUT_SIZE, file) != NULL) {
        favorites[fav_count].id = fav_count + 1;
        fav_count++;
    }
    fclose(file);
}

void set_reminder(int seconds, const char *message) {
    alarm(seconds);
    printf("Recordatorio programado en %d segundos: %s\n", seconds, message);
}

void reminder_handler(int sig) {
    printf("¡Recordatorio!\n");
    fflush(stdout);
}
