#include "server_common.h"

/* Atiende la operación REGISTER.
 * Recibe el nombre de usuario, registra al usuario si no existe
 * y devuelve al cliente el código de resultado correspondiente.
 */
void handle_register_operation(int client_fd) {
    char username[MAX_USERNAME];
    int rc;
    uint8_t code;

    if (recv_cstring(client_fd, username, sizeof(username)) != 0) {
        code = 2;
        (void)send_byte_code(client_fd, code);
        return;
    }

    rpc_log_operation(username, "REGISTER");

    rc = register_user_internal(username);

    if (rc == 0) {
        code = 0;
        (void)send_byte_code(client_fd, code);
        printf("s> REGISTER %s OK\n", username);
    } else if (rc == 1) {
        code = 1;
        (void)send_byte_code(client_fd, code);
        printf("s> REGISTER %s FAIL\n", username);
    } else {
        code = 2;
        (void)send_byte_code(client_fd, code);
        printf("s> REGISTER %s FAIL\n", username);
    }

    fflush(stdout);
}

void handle_unregister_operation(int client_fd) {
    char username[MAX_USERNAME];
    int rc;
    uint8_t code;

    if (recv_cstring(client_fd, username, sizeof(username)) != 0) {
        code = 2;
        (void)send_byte_code(client_fd, code);
        return;
    }

    rpc_log_operation(username, "UNREGISTER");

    rc = unregister_user_internal(username);

    if (rc == 0) {
        code = 0;
        (void)send_byte_code(client_fd, code);
        printf("s> UNREGISTER %s OK\n", username);
    } else if (rc == 1) {
        code = 1;
        (void)send_byte_code(client_fd, code);
        printf("s> UNREGISTER %s FAIL\n", username);
    } else {
        code = 2;
        (void)send_byte_code(client_fd, code);
        printf("s> UNREGISTER %s FAIL\n", username);
    }

    fflush(stdout);
}

void handle_connect_operation(int client_fd, const struct sockaddr *client_sa) {
    char username[MAX_USERNAME];
    char port[MAX_PORT_STR];
    char ip[MAX_IP];
    user_t *user;
    int rc = 3;
    uint8_t code;

    if (recv_cstring(client_fd, username, sizeof(username)) != 0 ||
        recv_cstring(client_fd, port, sizeof(port)) != 0) {
        code = 3;
        (void)send_byte_code(client_fd, code);
        return;
    }

    rpc_log_operation(username, "CONNECT");

    get_ip_str(client_sa, ip, sizeof(ip));

    pthread_mutex_lock(&g_users_mutex);

    user = find_user_locked(username);
    if (user == NULL) {
        rc = 1;
    } else if (user->state == USER_CONNECTED) {
        rc = 2;
    } else {
        strncpy(user->ip, ip, sizeof(user->ip) - 1);
        user->ip[sizeof(user->ip) - 1] = '\0';

        strncpy(user->port, port, sizeof(user->port) - 1);
        user->port[sizeof(user->port) - 1] = '\0';

        user->state = USER_CONNECTED;
        rc = 0;
    }

    pthread_mutex_unlock(&g_users_mutex);

    code = (uint8_t)rc;
    (void)send_byte_code(client_fd, code);

    if (rc == 0) {
        printf("s> CONNECT %s OK\n", username);
        fflush(stdout);
        deliver_pending_messages_for_user(username);
    } else {
        printf("s> CONNECT %s FAIL\n", username);
        fflush(stdout);
    }
}

void handle_disconnect_operation(int client_fd, const struct sockaddr *client_sa) {
    char username[MAX_USERNAME];
    char caller_ip[MAX_IP];
    user_t *user;
    int rc = 3;
    uint8_t code;

    if (recv_cstring(client_fd, username, sizeof(username)) != 0) {
        code = 3;
        (void)send_byte_code(client_fd, code);
        return;
    }

    rpc_log_operation(username, "DISCONNECT");

    get_ip_str(client_sa, caller_ip, sizeof(caller_ip));

    pthread_mutex_lock(&g_users_mutex);

    user = find_user_locked(username);
    if (user == NULL) {
        rc = 1;
    } else if (user->state != USER_CONNECTED) {
        rc = 2;
    } else if (strcmp(user->ip, caller_ip) != 0) {
        rc = 3;
    } else {
        user->state = USER_DISCONNECTED;
        user->ip[0] = '\0';
        user->port[0] = '\0';
        rc = 0;
    }

    pthread_mutex_unlock(&g_users_mutex);

    code = (uint8_t)rc;
    (void)send_byte_code(client_fd, code);

    if (rc == 0) {
        printf("s> DISCONNECT %s OK\n", username);
    } else {
        printf("s> DISCONNECT %s FAIL\n", username);
    }

    fflush(stdout);
}

/* Atiende SEND y SENDATTACH.
 * Recibe remitente, destinatario, mensaje y opcionalmente fichero.
 * Asigna un identificador al mensaje, lo almacena como pendiente
 * y, si el destinatario está conectado, intenta entregarlo inmediatamente.
 */
void handle_send_common(int client_fd, int has_attach) {
    char sender_name[MAX_USERNAME];
    char receiver_name[MAX_USERNAME];
    char text[MAX_MESSAGE_LEN];
    char filename[MAX_FILENAME_LEN];
    user_t *sender;
    user_t *receiver;
    unsigned int msg_id = 0;
    int receiver_connected = 0;
    char receiver_ip[MAX_IP];
    char receiver_port[MAX_PORT_STR];
    uint8_t code;

    filename[0] = '\0';

    if (recv_cstring(client_fd, sender_name, sizeof(sender_name)) != 0 ||
        recv_cstring(client_fd, receiver_name, sizeof(receiver_name)) != 0 ||
        recv_cstring(client_fd, text, sizeof(text)) != 0) {
        code = 2;
        (void)send_byte_code(client_fd, code);
        return;
    }

    if (has_attach) {
        if (recv_cstring(client_fd, filename, sizeof(filename)) != 0) {
            code = 2;
            (void)send_byte_code(client_fd, code);
            return;
        }
    }

    if (has_attach) {
        rpc_log_operation_file(sender_name, "SENDATTACH", filename);
    } else {
        rpc_log_operation(sender_name, "SEND");
    }

    pthread_mutex_lock(&g_users_mutex);

    sender = find_user_locked(sender_name);
    receiver = find_user_locked(receiver_name);

    if (sender == NULL || receiver == NULL) {
        pthread_mutex_unlock(&g_users_mutex);
        code = 1;
        (void)send_byte_code(client_fd, code);
        printf("s> SEND MESSAGE FROM %s TO %s FAIL\n", sender_name, receiver_name);
        fflush(stdout);
        return;
    }

    if (sender->state != USER_CONNECTED) {
        pthread_mutex_unlock(&g_users_mutex);
        code = 2;
        (void)send_byte_code(client_fd, code);
        printf("s> SEND MESSAGE FROM %s TO %s FAIL\n", sender_name, receiver_name);
        fflush(stdout);
        return;
    }

    msg_id = next_message_id_for_sender(sender);

    if (add_pending_message_locked(receiver, sender_name, msg_id, text,
                               has_attach, filename) != 0) {
        pthread_mutex_unlock(&g_users_mutex);
        code = 2;
        (void)send_byte_code(client_fd, code);
        printf("s> SEND MESSAGE FROM %s TO %s FAIL\n", sender_name, receiver_name);
        fflush(stdout);
        return;
    }

    if (receiver->state == USER_CONNECTED) {
        receiver_connected = 1;

        strncpy(receiver_ip, receiver->ip, sizeof(receiver_ip) - 1);
        receiver_ip[sizeof(receiver_ip) - 1] = '\0';

        strncpy(receiver_port, receiver->port, sizeof(receiver_port) - 1);
        receiver_port[sizeof(receiver_port) - 1] = '\0';
    }

    pthread_mutex_unlock(&g_users_mutex);

    code = 0;
    if (send_byte_code(client_fd, code) != 0) {
        return;
    }

    {
        char id_str[32];
        snprintf(id_str, sizeof(id_str), "%u", msg_id);
        if (send_cstring(client_fd, id_str) != 0) {
            return;
        }
    }

    if (receiver_connected) {
        if (deliver_message_to_endpoint(receiver_ip, receiver_port, sender_name,
                                        msg_id, text, has_attach, filename) == 0) {
            pthread_mutex_lock(&g_users_mutex);

            receiver = find_user_locked(receiver_name);
            if (receiver != NULL) {
                (void)remove_pending_message_locked(receiver, msg_id, sender_name);
            }

            pthread_mutex_unlock(&g_users_mutex);

            printf("s> SEND MESSAGE %u FROM %s TO %s\n", msg_id, sender_name, receiver_name);
            fflush(stdout);

            send_ack_to_sender_if_connected(sender_name, msg_id, has_attach, filename);
        } else {
            pthread_mutex_lock(&g_users_mutex);

            receiver = find_user_locked(receiver_name);
            if (receiver != NULL) {
                receiver->state = USER_DISCONNECTED;
                receiver->ip[0] = '\0';
                receiver->port[0] = '\0';
            }

            pthread_mutex_unlock(&g_users_mutex);

            printf("s> MESSAGE %u FROM %s TO %s STORED\n", msg_id, sender_name, receiver_name);
            fflush(stdout);
        }
    } else {
        printf("s> MESSAGE %u FROM %s TO %s STORED\n", msg_id, sender_name, receiver_name);
        fflush(stdout);
    }
}

/* Atiende la operación USERS.
 * Comprueba que el solicitante existe y está conectado.
 * Devuelve el número de usuarios conectados y, para cada uno,
 * su nombre, IP y puerto en el formato requerido por la parte B.
 */
void handle_users_operation(int client_fd) {
    char requester[MAX_USERNAME];
    user_t *user;
    user_t *curr;
    int connected_count = 0;
    char **entries = NULL;
    int idx = 0;
    uint8_t code;
    char count_str[32];

    if (recv_cstring(client_fd, requester, sizeof(requester)) != 0) {
        code = 2;
        (void)send_byte_code(client_fd, code);
        printf("s> CONNECTEDUSERS FAIL\n");
        fflush(stdout);
        return;
    }

    rpc_log_operation(requester, "USERS");

    pthread_mutex_lock(&g_users_mutex);

    user = find_user_locked(requester);
    if (user == NULL) {
        pthread_mutex_unlock(&g_users_mutex);
        code = 2;
        (void)send_byte_code(client_fd, code);
        printf("s> CONNECTEDUSERS FAIL\n");
        fflush(stdout);
        return;
    }

    if (user->state != USER_CONNECTED) {
        pthread_mutex_unlock(&g_users_mutex);
        code = 1;
        (void)send_byte_code(client_fd, code);
        printf("s> CONNECTEDUSERS FAIL\n");
        fflush(stdout);
        return;
    }

    for (curr = g_users; curr != NULL; curr = curr->next) {
        if (curr->state == USER_CONNECTED) {
            connected_count++;
        }
    }

    if (connected_count > 0) {
        entries = (char **)malloc((size_t)connected_count * sizeof(char *));
        if (entries == NULL) {
            pthread_mutex_unlock(&g_users_mutex);
            code = 2;
            (void)send_byte_code(client_fd, code);
            printf("s> CONNECTEDUSERS FAIL\n");
            fflush(stdout);
            return;
        }

        for (curr = g_users; curr != NULL; curr = curr->next) {
            if (curr->state == USER_CONNECTED) {
                char line[MAX_USERNAME + MAX_IP + MAX_PORT_STR + 16];

                snprintf(line, sizeof(line), "%s :: %s :: %s", curr->username, curr->ip, curr->port);

                entries[idx] = (char *)malloc(strlen(line) + 1);
                if (entries[idx] == NULL) {
                    int j;
                    for (j = 0; j < idx; ++j) {
                        free(entries[j]);
                    }

                    free(entries);
                    pthread_mutex_unlock(&g_users_mutex);
                    code = 2;
                    (void)send_byte_code(client_fd, code);
                    printf("s> CONNECTEDUSERS FAIL\n");
                    fflush(stdout);
                    return;
                }

                strcpy(entries[idx], line);
                idx++;
            }
        }
    }

    pthread_mutex_unlock(&g_users_mutex);

    code = 0;
    if (send_byte_code(client_fd, code) != 0) {
        printf("s> CONNECTEDUSERS FAIL\n");
        fflush(stdout);
        goto cleanup;
    }

    snprintf(count_str, sizeof(count_str), "%d", connected_count);
    if (send_cstring(client_fd, count_str) != 0) {
        printf("s> CONNECTEDUSERS FAIL\n");
        fflush(stdout);
        goto cleanup;
    }

    for (idx = 0; idx < connected_count; ++idx) {
        if (send_cstring(client_fd, entries[idx]) != 0) {
            printf("s> CONNECTEDUSERS FAIL\n");
            fflush(stdout);
            goto cleanup;
        }
    }

    printf("s> CONNECTEDUSERS OK\n");
    fflush(stdout);

cleanup:
    if (entries != NULL) {
        int j;
        for (j = 0; j < connected_count; ++j) {
            free(entries[j]);
        }
        free(entries);
    }
}