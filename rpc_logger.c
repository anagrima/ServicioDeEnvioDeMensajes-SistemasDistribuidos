#include "server_common.h"
#include "logger.h"

static char g_rpc_ip[256];

void rpc_logger_init(const char *ip) {
    if (ip == NULL || ip[0] == '\0') {
        g_rpc_ip[0] = '\0';
        return;
    }

    strncpy(g_rpc_ip, ip, sizeof(g_rpc_ip) - 1);
    g_rpc_ip[sizeof(g_rpc_ip) - 1] = '\0';
}

void rpc_log_operation_file(const char *username,
                            const char *operation,
                            const char *filename) {
    CLIENT *clnt;
    log_request req;
    int *result;

    if (g_rpc_ip[0] == '\0') {
        return;
    }

    if (username == NULL || operation == NULL) {
        return;
    }

    clnt = clnt_create(g_rpc_ip, LOGGER_PROG, LOGGER_VERS, "tcp");
    if (clnt == NULL) {
        return;
    }

    req.username = (char *)username;
    req.operation = (char *)operation;
    req.filename = (char *)((filename != NULL) ? filename : "");

    result = log_operation_1(&req, clnt);
    (void)result;

    clnt_destroy(clnt);
}

void rpc_log_operation(const char *username, const char *operation) {
    rpc_log_operation_file(username, operation, "");
}