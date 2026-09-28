CC = gcc
RPCGEN = rpcgen

CFLAGS = -Wall -Wextra -pedantic -std=c11 -pthread
CPPFLAGS = -I/usr/include/tirpc
RPCGEN_CFLAGS = -Wno-unused-variable -Wno-unused-parameter -Wno-cast-function-type
LDLIBS = -ltirpc

SERVER_OBJS = server.o net_utils.o user_store.o delivery.o operations.o rpc_logger.o logger_clnt.o logger_xdr.o
LOGGER_OBJS = logger_svc.o logger_xdr.o logger_service.o

all: server logger_server

logger.h logger_clnt.c logger_svc.c logger_xdr.c: logger.x
	$(RPCGEN) logger.x

server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) $(SERVER_OBJS) $(LDLIBS) -o server

logger_server: $(LOGGER_OBJS)
	$(CC) $(CFLAGS) $(LOGGER_OBJS) $(LDLIBS) -o logger_server

server.o: server.c server_common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c server.c

net_utils.o: net_utils.c server_common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c net_utils.c

user_store.o: user_store.c server_common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c user_store.c

delivery.o: delivery.c server_common.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c delivery.c

operations.o: operations.c server_common.h logger.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c operations.c

rpc_logger.o: rpc_logger.c server_common.h logger.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c rpc_logger.c

logger_service.o: logger_service.c logger.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c logger_service.c

logger_clnt.o: logger_clnt.c logger.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RPCGEN_CFLAGS) -c logger_clnt.c

logger_xdr.o: logger_xdr.c logger.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RPCGEN_CFLAGS) -c logger_xdr.c

logger_svc.o: logger_svc.c logger.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RPCGEN_CFLAGS) -c logger_svc.c

clean:
	rm -f *.o server logger_server logger_clnt.c logger_svc.c logger_xdr.c logger.h