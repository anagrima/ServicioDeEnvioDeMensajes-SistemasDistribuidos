#include "server_common.h"

void get_ip_str(const struct sockaddr *sa, char *out, size_t out_size) {
    if (sa->sa_family == AF_INET) {
        const struct sockaddr_in *addr4 = (const struct sockaddr_in *)sa;
        inet_ntop(AF_INET, &(addr4->sin_addr), out, (socklen_t)out_size);
    } else if (sa->sa_family == AF_INET6) {
        const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6 *)sa;
        inet_ntop(AF_INET6, &(addr6->sin6_addr), out, (socklen_t)out_size);
    } else {
        snprintf(out, out_size, "unknown");
    }
}

int get_port_num(const struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        const struct sockaddr_in *addr4 = (const struct sockaddr_in *)sa;
        return ntohs(addr4->sin_port);
    }

    if (sa->sa_family == AF_INET6) {
        const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6 *)sa;
        return ntohs(addr6->sin6_port);
    }

    return -1;
}

int get_local_ip_port(int sockfd, char *ip, size_t ip_size, int *port) {
    struct sockaddr_storage local_addr;
    socklen_t len = sizeof(local_addr);

    if (getsockname(sockfd, (struct sockaddr *)&local_addr, &len) == -1) {
        return -1;
    }

    get_ip_str((struct sockaddr *)&local_addr, ip, ip_size);
    *port = get_port_num((struct sockaddr *)&local_addr);
    return 0;
}

int send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t total = 0;

    while (total < len) {
        ssize_t n = send(fd, p + total, len - total, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (n == 0) {
            return -1;
        }

        total += (size_t)n;
    }

    return 0;
}

int recv_all(int fd, void *buf, size_t len) {
    char *p = (char *)buf;
    size_t total = 0;

    while (total < len) {
        ssize_t n = recv(fd, p + total, len - total, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (n == 0) {
            return -1;
        }

        total += (size_t)n;
    }

    return 0;
}

int send_byte_code(int fd, uint8_t code) {
    return send_all(fd, &code, sizeof(code));
}

int send_cstring(int fd, const char *text) {
    return send_all(fd, text, strlen(text) + 1);
}

int recv_cstring(int fd, char *buf, size_t buf_size) {
    size_t pos = 0;

    if (buf == NULL || buf_size == 0) {
        return -1;
    }

    while (1) {
        char c;

        if (recv_all(fd, &c, 1) != 0) {
            return -1;
        }

        if (pos >= buf_size) {
            return -1;
        }

        buf[pos++] = c;

        if (c == '\0') {
            break;
        }
    }

    return 0;
}

int connect_to_host(const char *ip, const char *port) {
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *rp;
    int fd = -1;
    int rc;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rc = getaddrinfo(ip, port, &hints, &result);
    if (rc != 0) {
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1) {
            continue;
        }

        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }

        close(fd);
        fd = -1;
    }

    freeaddrinfo(result);
    return fd;
}