#ifndef SERVER_COMMON_H
#define SERVER_COMMON_H

#define _POSIX_C_SOURCE 200112L

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BACKLOG 50
#define MAX_USERNAME 256
#define MAX_IP INET6_ADDRSTRLEN
#define MAX_PORT_STR 16
#define MAX_MESSAGE_LEN 256
#define MAX_FILENAME_LEN 256
#define MAX_OPERATION 64

typedef enum {
    USER_DISCONNECTED = 0,
    USER_CONNECTED = 1
} user_state_t;

typedef struct pending_message {
    unsigned int id;
    char sender[MAX_USERNAME];
    char text[MAX_MESSAGE_LEN];
    char filename[MAX_FILENAME_LEN];
    int has_attach;
    struct pending_message *next;
} pending_message_t;

typedef struct user {
    char username[MAX_USERNAME];
    user_state_t state;
    char ip[MAX_IP];
    char port[MAX_PORT_STR];
    unsigned int last_message_id;
    pending_message_t *pending_head;
    pending_message_t *pending_tail;
    struct user *next;
} user_t;

typedef struct {
    int client_fd;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len;
} client_thread_arg_t;

extern user_t *g_users;
extern pthread_mutex_t g_users_mutex;
extern volatile sig_atomic_t g_stop;
extern int g_listen_fd;

/* net_utils.c */
void get_ip_str(const struct sockaddr *sa, char *out, size_t out_size);
int get_port_num(const struct sockaddr *sa);
int get_local_ip_port(int sockfd, char *ip, size_t ip_size, int *port);
int send_all(int fd, const void *buf, size_t len);
int recv_all(int fd, void *buf, size_t len);
int send_byte_code(int fd, uint8_t code);
int send_cstring(int fd, const char *text);
int recv_cstring(int fd, char *buf, size_t buf_size);
int connect_to_host(const char *ip, const char *port);

/* user_store.c */
void free_pending_messages(pending_message_t *head);
user_t *find_user_locked(const char *username);
unsigned int next_message_id_for_sender(user_t *sender);
int add_pending_message_locked(user_t *receiver,
                               const char *sender_name,
                               unsigned int msg_id,
                               const char *text,
                               int has_attach,
                               const char *filename);
int remove_pending_message_locked(user_t *receiver,
                                  unsigned int msg_id,
                                  const char *sender_name);
int register_user_internal(const char *username);
int unregister_user_internal(const char *username);
void free_all_users(void);

/* delivery.c */
int deliver_message_to_endpoint(const char *ip,
                                const char *port,
                                const char *sender,
                                unsigned int msg_id,
                                const char *text,
                                int has_attach,
                                const char *filename);
void send_ack_to_sender_if_connected(const char *sender_name,
                                     unsigned int msg_id,
                                     int has_attach,
                                     const char *filename);
void deliver_pending_messages_for_user(const char *username);

/* operations.c */
void handle_register_operation(int client_fd);
void handle_unregister_operation(int client_fd);
void handle_connect_operation(int client_fd, const struct sockaddr *client_sa);
void handle_disconnect_operation(int client_fd, const struct sockaddr *client_sa);
void handle_send_common(int client_fd, int has_attach);
void handle_users_operation(int client_fd);

/* rpc_logger.c */
void rpc_logger_init(const char *ip);
void rpc_log_operation(const char *username, const char *operation);
void rpc_log_operation_file(const char *username, const char *operation, const char *filename);

#endif