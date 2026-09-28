#include "server_common.h"

/* Envía un mensaje desde el servidor al hilo de escucha de un cliente.
 * Usa el protocolo SEND MESSAGE o SEND MESSAGE ATTACH según corresponda.
 */
int deliver_message_to_endpoint(const char *ip, const char *port, const char *sender, unsigned int msg_id, const char *text,
                                int has_attach,
                                const char *filename) {
    int fd;
    char id_str[32];

    fd = connect_to_host(ip, port);
    if (fd == -1) {
        return -1;
    }

    snprintf(id_str, sizeof(id_str), "%u", msg_id);

    if (!has_attach) {
        if (send_cstring(fd, "SEND MESSAGE") != 0 ||
            send_cstring(fd, sender) != 0 ||
            send_cstring(fd, id_str) != 0 ||
            send_cstring(fd, text) != 0) {
            close(fd);
            return -1;
        }
    } else {
        if (send_cstring(fd, "SEND MESSAGE ATTACH") != 0 ||
            send_cstring(fd, sender) != 0 ||
            send_cstring(fd, id_str) != 0 ||
            send_cstring(fd, text) != 0 ||
            send_cstring(fd, filename) != 0) {
            close(fd);
            return -1;
        }
    }

    close(fd);
    return 0;
}

/* Notifica al remitente que uno de sus mensajes ha sido entregado.
 * Si el remitente ya no está conectado, la confirmación se descarta.
 */
void send_ack_to_sender_if_connected(const char *sender_name,
                                     unsigned int msg_id,
                                     int has_attach,
                                     const char *filename) {
    user_t *sender;
    char ip[MAX_IP];
    char port[MAX_PORT_STR];
    int fd;
    char id_str[32];

    pthread_mutex_lock(&g_users_mutex);

    sender = find_user_locked(sender_name);
    if (sender == NULL || sender->state != USER_CONNECTED) {
        pthread_mutex_unlock(&g_users_mutex);
        return;
    }

    strncpy(ip, sender->ip, sizeof(ip) - 1);
    ip[sizeof(ip) - 1] = '\0';

    strncpy(port, sender->port, sizeof(port) - 1);
    port[sizeof(port) - 1] = '\0';

    pthread_mutex_unlock(&g_users_mutex);

    fd = connect_to_host(ip, port);
    if (fd == -1) {
        return;
    }

    snprintf(id_str, sizeof(id_str), "%u", msg_id);

    if (!has_attach) {
        (void)send_cstring(fd, "SEND MESS ACK");
        (void)send_cstring(fd, id_str);
    } else {
        (void)send_cstring(fd, "SEND MESS ATTACH ACK");
        (void)send_cstring(fd, id_str);
        (void)send_cstring(fd, filename);
    }

    close(fd);
}

/* Entrega todos los mensajes pendientes de un usuario cuando se conecta.
 * Cada mensaje entregado correctamente se elimina de la cola de pendientes.
 */
void deliver_pending_messages_for_user(const char *username) {
    while (1) {
        user_t *user;
        pending_message_t *msg;
        char target_ip[MAX_IP];
        char target_port[MAX_PORT_STR];
        unsigned int msg_id;
        char sender[MAX_USERNAME];
        char text[MAX_MESSAGE_LEN];
        char filename[MAX_FILENAME_LEN];
        int has_attach;

        pthread_mutex_lock(&g_users_mutex);

        user = find_user_locked(username);
        if (user == NULL || user->state != USER_CONNECTED || user->pending_head == NULL) {
            pthread_mutex_unlock(&g_users_mutex);
            break;
        }

        msg = user->pending_head;

        strncpy(target_ip, user->ip, sizeof(target_ip) - 1);
        target_ip[sizeof(target_ip) - 1] = '\0';

        strncpy(target_port, user->port, sizeof(target_port) - 1);
        target_port[sizeof(target_port) - 1] = '\0';

        msg_id = msg->id;
        has_attach = msg->has_attach;

        strncpy(sender, msg->sender, sizeof(sender) - 1);
        sender[sizeof(sender) - 1] = '\0';

        strncpy(text, msg->text, sizeof(text) - 1);
        text[sizeof(text) - 1] = '\0';

        strncpy(filename, msg->filename, sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';

        pthread_mutex_unlock(&g_users_mutex);

        if (deliver_message_to_endpoint(target_ip, target_port, sender, msg_id,
                                        text, has_attach, filename) == 0) {
            pthread_mutex_lock(&g_users_mutex);

            user = find_user_locked(username);
            if (user != NULL) {
                (void)remove_pending_message_locked(user, msg_id, sender);
            }

            pthread_mutex_unlock(&g_users_mutex);

            printf("s> SEND MESSAGE %u FROM %s TO %s\n", msg_id, sender, username);
            fflush(stdout);

            send_ack_to_sender_if_connected(sender, msg_id, has_attach, filename);
        } else {
            pthread_mutex_lock(&g_users_mutex);

            user = find_user_locked(username);
            if (user != NULL) {
                user->state = USER_DISCONNECTED;
                user->ip[0] = '\0';
                user->port[0] = '\0';
            }

            pthread_mutex_unlock(&g_users_mutex);
            break;
        }
    }
}