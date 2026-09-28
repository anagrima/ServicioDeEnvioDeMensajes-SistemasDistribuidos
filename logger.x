struct log_request {
    string username<256>;
    string operation<64>;
    string filename<256>;
};

program LOGGER_PROG {
    version LOGGER_VERS {
        int LOG_OPERATION(log_request) = 1;
    } = 1;
} = 0x20498828;