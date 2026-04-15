/*
 * wish - Wisconsin Shell (simple)
 * Practica 2 - API de Procesos (UdeA)
 *
 * Requisitos clave (ver enunciado):
 * - Modos: interactivo (prompt "wish> ") y batch (sin prompt)
 * - built-ins: exit, cd, path (se aceptan alias chd/route por typos del enunciado)
 * - ejecución de externos con fork()+execv(), y wait()/waitpid()
 * - redirección: ">" redirige STDOUT y STDERR al mismo archivo (sobrescribe)
 * - comandos paralelos: separados por '&' (ejecutar todos y luego esperar)
 * - único mensaje de error: "An error has occurred\n" a stderr
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

static const char error_message[] = "An error has occurred\n";

/* -------------------- utilidades -------------------- */

static void print_error(void) {
    write(STDERR_FILENO, error_message, strlen(error_message));
}

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/* recorta espacios al inicio/fin (in-place), retorna puntero al inicio recortado */
static char* trim(char *s) {
    if (s == NULL) return NULL;
    while (*s && is_space(*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && is_space(*end)) {
        *end = '\0';
        end--;
    }
    return s;
}

/* tokeniza por espacios/tabs, estilo strsep. Retorna argc y llena argv (terminado en NULL). */
static int tokenize(char *line, char **argv, int max_args) {
    int argc = 0;
    char *p = line;
    while (p && *p) {
        char *tok = strsep(&p, " \t\r\n");
        if (!tok) break;
        if (tok[0] == '\0') continue;
        if (argc >= max_args - 1) {
            return -1;
        }
        argv[argc++] = tok;
    }
    argv[argc] = NULL;
    return argc;
}

/* divide la línea en comandos por '&'. Retorna cantidad y llena cmds[] con punteros dentro de line. */
static int split_parallel(char *line, char **cmds, int max_cmds) {
    int n = 0;
    char *p = line;
    while (p) {
        if (n >= max_cmds) return -1;
        char *seg = strsep(&p, "&");
        if (seg == NULL) break;
        seg = trim(seg);
        /* Si hay un segmento vacío entre && -> error (lo marcamos con string vacío) */
        cmds[n++] = seg;
    }
    return n;
}

/* parsea redirección: separa cmd_part y outfile.
 * - acepta a lo sumo un '>'
 * - a la derecha debe haber exactamente 1 token (el nombre del archivo)
 * Retorna 0 si OK, -1 si error.
 */
static int parse_redirection(char *cmd, char **cmd_part, char **out_file) {
    *out_file = NULL;
    *cmd_part = cmd;

    char *first = strchr(cmd, '>');
    if (!first) {
        return 0;
    }

    /* si hay más de un '>' -> error */
    char *second = strchr(first + 1, '>');
    if (second) return -1;

    *first = '\0';
    char *left = trim(cmd);
    char *right = trim(first + 1);

    if (left[0] == '\0') return -1;
    if (right[0] == '\0') return -1;

    /* right debe ser un solo token */
    char *tmp = strdup(right);
    if (!tmp) return -1;

    char *argv[4];
    int argc = tokenize(tmp, argv, 4);
    if (argc != 1) {
        free(tmp);
        return -1;
    }

    *cmd_part = left;
    /* copiamos el nombre del archivo a un buffer propio */
    *out_file = strdup(argv[0]);
    free(tmp);

    if (!(*out_file)) return -1;
    return 0;
}

/* -------------------- PATH del shell -------------------- */

typedef struct {
    char **dirs;
    int count;
} PathList;

static void path_init(PathList *pl) {
    pl->dirs = NULL;
    pl->count = 0;
}

static void path_free(PathList *pl) {
    if (!pl) return;
    for (int i = 0; i < pl->count; i++) free(pl->dirs[i]);
    free(pl->dirs);
    pl->dirs = NULL;
    pl->count = 0;
}

/* sobrescribe path con argv[1..argc-1]; si argc==1 -> path vacío */
static int path_set(PathList *pl, char **argv, int argc) {
    path_free(pl);
    if (argc <= 1) {
        /* path vacío */
        return 0;
    }
    pl->dirs = (char **)calloc((size_t)(argc - 1), sizeof(char *));
    if (!pl->dirs) return -1;
    pl->count = argc - 1;
    for (int i = 1; i < argc; i++) {
        pl->dirs[i - 1] = strdup(argv[i]);
        if (!pl->dirs[i - 1]) return -1;
    }
    return 0;
}

/* retorna ruta ejecutable encontrada o NULL */
static char* find_executable(PathList *pl, const char *cmd) {
    if (!cmd || cmd[0] == '\0') return NULL;

    /* Si el usuario pasó una ruta con '/', intentamos ejecutarla tal cual */
    if (strchr(cmd, '/')) {
        if (access(cmd, X_OK) == 0) return strdup(cmd);
        return NULL;
    }

    for (int i = 0; i < pl->count; i++) {
        const char *dir = pl->dirs[i];
        if (!dir || dir[0] == '\0') continue;

        size_t len = strlen(dir) + 1 + strlen(cmd) + 1;
        char *full = (char *)malloc(len);
        if (!full) return NULL;
        snprintf(full, len, "%s/%s", dir, cmd);

        if (access(full, X_OK) == 0) return full;
        free(full);
    }
    return NULL;
}

/* -------------------- ejecución -------------------- */

static int is_builtin(const char *cmd) {
    if (!cmd) return 0;
    return (strcmp(cmd, "exit") == 0 ||
            strcmp(cmd, "cd") == 0   ||
            strcmp(cmd, "path") == 0 ||
            strcmp(cmd, "chd") == 0  ||   /* alias */
            strcmp(cmd, "route") == 0);   /* alias */
}

static int run_builtin(char **argv, int argc, PathList *pl) {
    if (argc <= 0) return 0;

    /* exit */
    if (strcmp(argv[0], "exit") == 0) {
        if (argc != 1) return -1;
        exit(0);
    }

    /* cd (alias chd) */
    if (strcmp(argv[0], "cd") == 0 || strcmp(argv[0], "chd") == 0) {
        if (argc != 2) return -1;
        if (chdir(argv[1]) != 0) return -1;
        return 0;
    }

    /* path (alias route) */
    if (strcmp(argv[0], "path") == 0 || strcmp(argv[0], "route") == 0) {
        if (path_set(pl, argv, argc) != 0) return -1;
        return 0;
    }

    return -1;
}

static int redirect_output(const char *outfile) {
    int fd = open(outfile, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd < 0) return -1;

    if (dup2(fd, STDOUT_FILENO) < 0) { close(fd); return -1; }
    if (dup2(fd, STDERR_FILENO) < 0) { close(fd); return -1; }

    close(fd);
    return 0;
}

/* ejecuta una línea completa: soporta paralelismo (&) y redirección (>) */
static void execute_line(char *line, PathList *pl) {
    /* separar por '&' */
    char *cmds[128];
    int ncmd = split_parallel(line, cmds, 128);
    if (ncmd < 0) { print_error(); return; }

    /* pids de externos para esperar al final */
    pid_t pids[128];
    int npids = 0;

    for (int i = 0; i < ncmd; i++) {
        char *raw = cmds[i];
        if (!raw || raw[0] == '\0') { print_error(); continue; }

        char *outfile = NULL;
        char *cmd_part = NULL;
        if (parse_redirection(raw, &cmd_part, &outfile) != 0) {
            if (outfile) free(outfile);
            print_error();
            continue;
        }

        /* tokenizar comando */
        char *argv[128];
        int argc = tokenize(cmd_part, argv, 128);
        if (argc <= 0) { if (outfile) free(outfile); continue; }
        if (argc < 0) { if (outfile) free(outfile); print_error(); continue; }

        if (is_builtin(argv[0])) {
            /* Nota: no se evalúa redirección en built-ins */
            if (outfile) {
                free(outfile);
                print_error();
                continue;
            }
            if (run_builtin(argv, argc, pl) != 0) {
                print_error();
            }
            continue;
        }

        /* externo */
        if (pl->count == 0) {
            if (outfile) free(outfile);
            print_error();
            continue;
        }

        char *exec_path = find_executable(pl, argv[0]);
        if (!exec_path) {
            if (outfile) free(outfile);
            print_error();
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            free(exec_path);
            if (outfile) free(outfile);
            print_error();
            continue;
        }

        if (pid == 0) {
            /* hijo */
            if (outfile) {
                if (redirect_output(outfile) != 0) {
                    print_error();
                    _exit(1);
                }
            }
            execv(exec_path, argv);
            /* si execv retorna, hay error */
            print_error();
            _exit(1);
        }

        /* padre */
        if (npids < 128) pids[npids++] = pid;
        free(exec_path);
        if (outfile) free(outfile);
    }

    /* esperar por todos los externos lanzados en esta línea */
    for (int i = 0; i < npids; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

/* -------------------- main -------------------- */

int main(int argc, char *argv[]) {
    FILE *input = NULL;
    int interactive = 1;

    if (argc == 1) {
        input = stdin;
        interactive = 1;
    } else if (argc == 2) {
        interactive = 0;
        input = fopen(argv[1], "r");
        if (!input) {
            print_error();
            exit(1);
        }
    } else {
        print_error();
        exit(1);
    }

    PathList pl;
    path_init(&pl);
    /* path inicial: /bin */
    char *initv[] = { "path", "/bin", NULL };
    if (path_set(&pl, initv, 2) != 0) {
        print_error();
        exit(1);
    }

    char *line = NULL;
    size_t cap = 0;

    while (1) {
        if (interactive) {
            printf("wish> ");
            fflush(stdout);
        }

        ssize_t n = getline(&line, &cap, input);
        if (n == -1) {
            /* EOF => salir */
            exit(0);
        }

        /* quitar \n */
        if (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[n - 1] = '\0';
        }

        char *t = trim(line);
        if (t[0] == '\0') continue;

        execute_line(t, &pl);
    }

    /* no se alcanza */
    path_free(&pl);
    free(line);
    return 0;
}
