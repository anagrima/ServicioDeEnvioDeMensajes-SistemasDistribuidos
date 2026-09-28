#include <stdio.h>
#include <string.h>
#include "logger.h"

int *log_operation_1_svc(log_request *argp, struct svc_req *rqstp) {
    static int result;

    (void)rqstp;

    if (argp != NULL && argp->username != NULL && argp->operation != NULL) {
        printf("%s\n%s\n", argp->username, argp->operation);

        if (strcmp(argp->operation, "SENDATTACH") == 0 &&
            argp->filename != NULL &&
            argp->filename[0] != '\0') {
            printf("%s\n", argp->filename);
        }

        fflush(stdout);
    }

    result = 0;
    return &result;
}