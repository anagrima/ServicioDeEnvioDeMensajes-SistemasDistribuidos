#include "server_common.h"

void free_pending_messages(pending_message_t *head) {
    pending_message_t *curr = head;

    while (curr != NULL) {
        pending_message_t *next = curr->next;
        free(curr);
        curr = next;
    }
}

user_t *find_user_locked(const char *username) {
    user_t *curr = g_users;

    while (curr != NULL) {
        if (strcmp(curr->username, username) == 0) {
            return curr;
        }
        curr = curr->next;
    }

    return NULL;
}

unsigned int next_message_id_for_sender(user_t *sender) {
    sender->last_message_id++;

    if (sender->last_message_id == 0) {
        sender->last_message_id = 1;
    }

    return sender->last_message_id;
}

int add_pending_message_locked(user_t *receiver,
                               const char *sender_name,
                               unsigned int msg_id,
                               const char *text,
                               int has_attach,
                               const char *filename) {
    pending_message_t *msg = (pending_message_t *)malloc(sizeof(pending_message_t));
    if (msg == NULL) {
        return -1;
    }

    memset(msg, 0, sizeof(pending_message_t));

    msg->id = msg_id;
    msg->has_attach = has_attach;

    strncpy(msg->sender, sender_name, sizeof(msg->sender) - 1);
    msg->sender[sizeof(msg->sender) - 1] = '\0';

    strncpy(msg->text, text, sizeof(msg->text) - 1);
    msg->text[sizeof(msg->text) - 1] = '\0';

    if (filename != NULL) {
        strncpy(msg->filename, filename, sizeof(msg->filename) - 1);
        msg->filename[sizeof(msg->filename) - 1] = '\0';
    }

    msg->next = NULL;

    if (receiver->pending_tail == NULL) {
        receiver->pending_head = msg;
        receiver->pending_tail = msg;
    } else {
        receiver->pending_tail->next = msg;
        receiver->pending_tail = msg;
    }

    return 0;
}

int remove_pending_message_locked(user_t *receiver,
                                  unsigned int msg_id,
                                  const char *sender_name) {
    pending_message_t *curr;
    pending_message_t *prev = NULL;

    if (receiver == NULL) {
        return -1;
    }

    curr = receiver->pending_head;

    while (curr != NULL) {
        if (curr->id == msg_id && strcmp(curr->sender, sender_name) == 0) {
            break;
        }

        prev = curr;
        curr = curr->next;
    }

    if (curr == NULL) {
        return -1;
    }

    if (prev == NULL) {
        receiver->pending_head = curr->next;
    } else {
        prev->next = curr->next;
    }

    if (receiver->pending_tail == curr) {
        receiver->pending_tail = prev;
    }

    free(curr);
    return 0;
}

int register_user_internal(const char *username) {
    user_t *existing;
    user_t *new_user;

    if (username == NULL || username[0] == '\0') {
        return -1;
    }

    pthread_mutex_lock(&g_users_mutex);

    existing = find_user_locked(username);
    if (existing != NULL) {
        pthread_mutex_unlock(&g_users_mutex);
        return 1;
    }

    new_user = (user_t *)malloc(sizeof(user_t));
    if (new_user == NULL) {
        pthread_mutex_unlock(&g_users_mutex);
        return -1;
    }

    memset(new_user, 0, sizeof(user_t));

    strncpy(new_user->username, username, sizeof(new_user->username) - 1);
    new_user->username[sizeof(new_user->username) - 1] = '\0';

    new_user->state = USER_DISCONNECTED;
    new_user->last_message_id = 0;

    new_user->next = g_users;
    g_users = new_user;

    pthread_mutex_unlock(&g_users_mutex);
    return 0;
}

int unregister_user_internal(const char *username) {
    user_t *curr;
    user_t *prev = NULL;

    if (username == NULL || username[0] == '\0') {
        return -1;
    }

    pthread_mutex_lock(&g_users_mutex);

    curr = g_users;

    while (curr != NULL) {
        if (strcmp(curr->username, username) == 0) {
            break;
        }

        prev = curr;
        curr = curr->next;
    }

    if (curr == NULL) {
        pthread_mutex_unlock(&g_users_mutex);
        return 1;
    }

    if (prev == NULL) {
        g_users = curr->next;
    } else {
        prev->next = curr->next;
    }

    free_pending_messages(curr->pending_head);
    free(curr);

    pthread_mutex_unlock(&g_users_mutex);
    return 0;
}

void free_all_users(void) {
    user_t *curr;
    user_t *next;

    pthread_mutex_lock(&g_users_mutex);

    curr = g_users;
    while (curr != NULL) {
        next = curr->next;
        free_pending_messages(curr->pending_head);
        free(curr);
        curr = next;
    }

    g_users = NULL;

    pthread_mutex_unlock(&g_users_mutex);
}