#include "server_common.h"

user_t *g_users = NULL;
pthread_mutex_t g_users_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t g_stop = 0;
int g_listen_fd = -1;

/* Manejador de SIGINT: permite cerrar el servidor de forma controlada
 * cuando el usuario pulsa Ctrl+C. Cierra el socket de escucha para
 * desbloquear accept() y terminar el bucle principal.
 */
static void handle_sigint(int sig) {
    (void)sig;
    g_stop = 1;

    if (g_listen_fd != -1) {
        close(g_listen_fd);
        g_listen_fd = -1;
    }
}

/* Función ejecutada por cada hilo de atención a cliente.
 * Lee la operación recibida por el socket y delega el tratamiento
 * en la función correspondiente.
 */
static void *client_thread_main(void *arg) {
    client_thread_arg_t *thread_arg = (client_thread_arg_t *)arg;
    int client_fd;
    char operation[MAX_OPERATION];

    if (thread_arg == NULL) {
        return NULL;
    }

    client_fd = thread_arg->client_fd;

    if (recv_cstring(client_fd, operation, sizeof(operation)) != 0) {
        close(client_fd);
        free(thread_arg);
        return NULL;
    }

    if (strcmp(operation, "REGISTER") == 0) {
        handle_register_operation(client_fd);
    } else if (strcmp(operation, "UNREGISTER") == 0) {
        handle_unregister_operation(client_fd);
    } else if (strcmp(operation, "CONNECT") == 0) {
        handle_connect_operation(client_fd, (struct sockaddr *)&thread_arg->client_addr);
    } else if (strcmp(operation, "DISCONNECT") == 0) {
        handle_disconnect_operation(client_fd, (struct sockaddr *)&thread_arg->client_addr);
    } else if (strcmp(operation, "SEND") == 0) {
        handle_send_common(client_fd, 0);
    } else if (strcmp(operation, "SENDATTACH") == 0) {
        handle_send_common(client_fd, 1);
    } else if (strcmp(operation, "USERS") == 0) {
        handle_users_operation(client_fd);
    } else {
        uint8_t code = 2;
        (void)send_byte_code(client_fd, code);
    }

    close(client_fd);
    free(thread_arg);
    return NULL;
}

/* Crea el socket TCP del servidor, lo asocia al puerto indicado
 * y lo deja escuchando conexiones entrantes.
 */
static int create_server_socket(const char *port_str) {
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *rp;
    int listen_fd = -1;
    int yes = 1;
    int rc;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    rc = getaddrinfo(NULL, port_str, &hints, &result);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listen_fd == -1) {
            continue;
        }

        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            close(listen_fd);
            listen_fd = -1;
            continue;
        }

        if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }

        close(listen_fd);
        listen_fd = -1;
    }

    freeaddrinfo(result);

    if (listen_fd == -1) {
        fprintf(stderr, "Error: no se pudo hacer bind en el puerto %s\n", port_str);
        return -1;
    }

    if (listen(listen_fd, BACKLOG) == -1) {
        perror("listen");
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}

static int parse_port_from_argv(int argc, char *argv[], char *out_port, size_t out_size) {
    int i;

    if (out_port == NULL || out_size == 0) {
        return -1;
    }

    for (i = 1; i < argc - 1; ++i) {
        if (strcmp(argv[i], "-p") == 0) {
            long port;
            char *endptr = NULL;

            port = strtol(argv[i + 1], &endptr, 10);
            if (*argv[i + 1] == '\0' || endptr == NULL || *endptr != '\0') {
                return -1;
            }

            if (port < 1024 || port > 65535) {
                return -1;
            }

            snprintf(out_port, out_size, "%ld", port);
            return 0;
        }
    }

    return -1;
}

int main(int argc, char *argv[]) {
    char port_str[MAX_PORT_STR];
    char local_ip[MAX_IP];
    int local_port;
    struct sigaction sa;

    if (parse_port_from_argv(argc, argv, port_str, sizeof(port_str)) != 0) {
        fprintf(stderr, "Usage: %s -p <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    rpc_logger_init(getenv("LOG_RPC_IP"));

    g_listen_fd = create_server_socket(port_str);
    if (g_listen_fd == -1) {
        return EXIT_FAILURE;
    }

    if (get_local_ip_port(g_listen_fd, local_ip, sizeof(local_ip), &local_port) != 0) {
        strncpy(local_ip, "0.0.0.0", sizeof(local_ip) - 1);
        local_ip[sizeof(local_ip) - 1] = '\0';
        local_port = atoi(port_str);
    }

    printf("s> init server %s:%d\n", local_ip, local_port);
    printf("s>\n");
    fflush(stdout);

    while (!g_stop) {
        struct sockaddr_storage client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd;
        pthread_t tid;
        client_thread_arg_t *thread_arg;

        client_fd = accept(g_listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            if (errno == EINTR && g_stop) {
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            perror("accept");
            continue;
        }

        thread_arg = (client_thread_arg_t *)malloc(sizeof(client_thread_arg_t));
        if (thread_arg == NULL) {
            fprintf(stderr, "Error: malloc thread_arg\n");
            close(client_fd);
            continue;
        }

        thread_arg->client_fd = client_fd;
        thread_arg->client_addr = client_addr;
        thread_arg->client_addr_len = client_len;

        if (pthread_create(&tid, NULL, client_thread_main, thread_arg) != 0) {
            fprintf(stderr, "Error: pthread_create\n");
            close(client_fd);
            free(thread_arg);
            continue;
        }

        if (pthread_detach(tid) != 0) {
            fprintf(stderr, "Error: pthread_detach\n");
        }
    }

    if (g_listen_fd != -1) {
        close(g_listen_fd);
        g_listen_fd = -1;
    }

    free_all_users();
    pthread_mutex_destroy(&g_users_mutex);

    return EXIT_SUCCESS;
}