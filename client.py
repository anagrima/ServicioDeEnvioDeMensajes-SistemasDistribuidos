import requests
from enum import Enum
import argparse
import socket
import threading
import os
import sys


class client:

    class RC(Enum):
        OK = 0
        ERROR = 1
        USER_ERROR = 2

    _server = None
    _port = -1

    _connected_user = None
    _listen_socket = None
    _listen_port = None
    _listener_thread = None
    _stop_listener = False
    _print_lock = threading.Lock()

    # user -> (ip, port)
    _connected_users_info = {}

    @staticmethod
    def _send_cstring(sock, text):
        sock.sendall(text.encode() + b'\0')

    @staticmethod
    def _recv_exact(sock, n):
        data = b''
        while len(data) < n:
            chunk = sock.recv(n - len(data))
            if not chunk:
                raise ConnectionError("Socket closed")
            data += chunk
        return data

    @staticmethod
    def _recv_byte(sock):
        return client._recv_exact(sock, 1)[0]

    @staticmethod
    def _recv_cstring(sock):
        data = bytearray()
        while True:
            c = sock.recv(1)
            if not c:
                raise ConnectionError("Socket closed")
            if c == b'\0':
                break
            data.extend(c)
        return data.decode()

    @staticmethod
    def _create_server_connection():
        return socket.create_connection((client._server, client._port))

    @staticmethod
    def _normalize_message(message):
        try:
            response = requests.post("http://localhost:5000/normalize", json={"text": message}, timeout=2)
            if response.status_code == 200:
                return response.json().get("normalized_text", message)
        except Exception:
            # Si el servicio web falla, devolvemos el mensaje original para no bloquear el envío
            pass
        return message

    @staticmethod
    def _send_file(sock, file_name):
        try:
            size = os.path.getsize(file_name)
            client._send_cstring(sock, str(size))

            with open(file_name, "rb") as f:
                while True:
                    chunk = f.read(4096)
                    if not chunk:
                        break
                    sock.sendall(chunk)
        except Exception:
            client._send_cstring(sock, "-1")

    @staticmethod
    def _recv_file(sock, local_file_name):
        size_str = client._recv_cstring(sock)
        size = int(size_str)

        if size < 0:
            return False

        remaining = size
        with open(local_file_name, "wb") as f:
            while remaining > 0:
                chunk = sock.recv(min(4096, remaining))
                if not chunk:
                    raise ConnectionError("Socket closed during file transfer")
                f.write(chunk)
                remaining -= len(chunk)

        return True

    @staticmethod
    def _listener_loop():
        while not client._stop_listener:
            try:
                conn, _addr = client._listen_socket.accept()
            except OSError:
                break

            try:
                op = client._recv_cstring(conn)

                if op == "SEND MESSAGE":
                    sender = client._recv_cstring(conn)
                    msg_id = client._recv_cstring(conn)
                    message = client._recv_cstring(conn)

                    with client._print_lock:
                        print(f"s> MESSAGE {msg_id} FROM {sender}")
                        print(message)
                        print("END")

                elif op == "SEND MESS ACK":
                    msg_id = client._recv_cstring(conn)
                    with client._print_lock:
                        print(f"c> SEND MESSAGE {msg_id} OK")

                elif op == "SEND MESSAGE ATTACH":
                    sender = client._recv_cstring(conn)
                    msg_id = client._recv_cstring(conn)
                    message = client._recv_cstring(conn)
                    file_name = client._recv_cstring(conn)

                    with client._print_lock:
                        print(f"c> MESSAGE {msg_id} FROM {sender}")
                        print(message)
                        print("END")
                        print(f"FILE {file_name}")

                elif op == "SEND MESS ATTACH ACK":
                    msg_id = client._recv_cstring(conn)
                    file_name = client._recv_cstring(conn)

                    with client._print_lock:
                        print(f"c> SENDATTACH MESSAGE {msg_id} {file_name} OK")

                elif op == "GET FILE":
                    _requester = client._recv_cstring(conn)
                    file_name = client._recv_cstring(conn)
                    client._send_file(conn, file_name)

            except Exception:
                pass
            finally:
                try:
                    conn.close()
                except Exception:
                    pass

    @staticmethod
    def _start_listener():
        client._listen_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client._listen_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        client._listen_socket.bind(("", 0))
        client._listen_socket.listen(10)

        client._listen_port = client._listen_socket.getsockname()[1]
        client._stop_listener = False

        client._listener_thread = threading.Thread(target=client._listener_loop, daemon=True)
        client._listener_thread.start()

    @staticmethod
    def _stop_listener_thread():
        client._stop_listener = True

        if client._listen_socket is not None:
            try:
                client._listen_socket.close()
            except Exception:
                pass
            client._listen_socket = None

        if client._listener_thread is not None:
            try:
                client._listener_thread.join(timeout=1.0)
            except Exception:
                pass
            client._listener_thread = None

        client._listen_port = None

    @staticmethod
    def register(user):
        try:
            sock = client._create_server_connection()
            client._send_cstring(sock, "REGISTER")
            client._send_cstring(sock, user)

            code = client._recv_byte(sock)
            sock.close()

            if code == 0:
                print("c> REGISTER OK")
                return client.RC.OK
            elif code == 1:
                print("c> USERNAME IN USE")
                return client.RC.USER_ERROR
            else:
                print("c> REGISTER FAIL")
                return client.RC.ERROR

        except Exception:
            print("c> REGISTER FAIL")
            return client.RC.ERROR

    @staticmethod
    def unregister(user):
        try:
            sock = client._create_server_connection()
            client._send_cstring(sock, "UNREGISTER")
            client._send_cstring(sock, user)

            code = client._recv_byte(sock)
            sock.close()

            if code == 0:
                print("c> UNREGISTER OK")
                return client.RC.OK
            elif code == 1:
                print("c> USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            else:
                print("c> UNREGISTER FAIL")
                return client.RC.ERROR

        except Exception:
            print("c> UNREGISTER FAIL")
            return client.RC.ERROR

    @staticmethod
    def connect(user):
        try:
            if client._connected_user is not None:
                print("c> USER ALREADY CONNECTED")
                return client.RC.USER_ERROR

            client._start_listener()

            sock = client._create_server_connection()
            client._send_cstring(sock, "CONNECT")
            client._send_cstring(sock, user)
            client._send_cstring(sock, str(client._listen_port))

            code = client._recv_byte(sock)
            sock.close()

            if code == 0:
                client._connected_user = user
                print("c> CONNECT OK")
                return client.RC.OK
            elif code == 1:
                client._stop_listener_thread()
                print("c> CONNECT FAIL, USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            elif code == 2:
                client._stop_listener_thread()
                print("c> USER ALREADY CONNECTED")
                return client.RC.USER_ERROR
            else:
                client._stop_listener_thread()
                print("c> CONNECT FAIL")
                return client.RC.ERROR

        except Exception:
            client._stop_listener_thread()
            print("c> CONNECT FAIL")
            return client.RC.ERROR

    @staticmethod
    def _refresh_users_cache(print_output):
        try:
            if client._connected_user is None:
                if print_output:
                    print("c> CONNECTED USERS FAIL, USER IS NOT CONNECTED")
                return client.RC.USER_ERROR

            sock = client._create_server_connection()
            client._send_cstring(sock, "USERS")
            client._send_cstring(sock, client._connected_user)

            code = client._recv_byte(sock)

            if code == 0:
                n_str = client._recv_cstring(sock)
                n = int(n_str)
                users_list = []
                client._connected_users_info.clear()

                for _ in range(n):
                    entry = client._recv_cstring(sock)
                    users_list.append(entry)

                    parts = [p.strip() for p in entry.split("::")]
                    if len(parts) == 3:
                        user_name, ip, port = parts
                        client._connected_users_info[user_name] = (ip, port)

                sock.close()

                if print_output:
                    print(f"c> CONNECTED USERS ({n} users connected) OK")
                    for u in users_list:
                        print(u)

                return client.RC.OK

            elif code == 1:
                sock.close()
                if print_output:
                    print("c> CONNECTED USERS FAIL, USER IS NOT CONNECTED")
                return client.RC.USER_ERROR

            else:
                sock.close()
                if print_output:
                    print("c> CONNECTED USERS FAIL")
                return client.RC.ERROR

        except Exception:
            if print_output:
                print("c> CONNECTED USERS FAIL")
            return client.RC.ERROR

    @staticmethod
    def users():
        return client._refresh_users_cache(print_output=True)

    @staticmethod
    def disconnect(user):
        try:
            sock = client._create_server_connection()
            client._send_cstring(sock, "DISCONNECT")
            client._send_cstring(sock, user)

            code = client._recv_byte(sock)
            sock.close()

            client._stop_listener_thread()
            client._connected_user = None
            client._connected_users_info.clear()

            if code == 0:
                print("c> DISCONNECT OK")
                return client.RC.OK
            elif code == 1:
                print("c> DISCONNECT FAIL, USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            elif code == 2:
                print("c> DISCONNECT FAIL, USER NOT CONNECTED")
                return client.RC.USER_ERROR
            else:
                print("c> DISCONNECT FAIL")
                return client.RC.ERROR

        except Exception:
            client._stop_listener_thread()
            client._connected_user = None
            client._connected_users_info.clear()
            print("c> DISCONNECT FAIL")
            return client.RC.ERROR

    @staticmethod
    def send(user, message):
        try:
            if client._connected_user is None:
                print("c> SEND FAIL")
                return client.RC.ERROR

            message = client._normalize_message(message)

            if len(message.encode()) > 255:
                print("c> SEND FAIL")
                return client.RC.ERROR

            sock = client._create_server_connection()
            client._send_cstring(sock, "SEND")
            client._send_cstring(sock, client._connected_user)
            client._send_cstring(sock, user)
            client._send_cstring(sock, message)

            code = client._recv_byte(sock)

            if code == 0:
                msg_id = client._recv_cstring(sock)
                sock.close()
                print(f"c> SEND OK - MESSAGE {msg_id}")
                return client.RC.OK
            elif code == 1:
                sock.close()
                print("c> SEND FAIL, USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            else:
                sock.close()
                print("c> SEND FAIL")
                return client.RC.ERROR

        except Exception:
            print("c> SEND FAIL")
            return client.RC.ERROR

    @staticmethod
    def sendAttach(user, file, message):
        try:
            if client._connected_user is None:
                print("c> SENDATTACH FAIL")
                return client.RC.ERROR

            message = client._normalize_message(message)

            if len(message.encode()) > 255 or len(file.encode()) > 255:
                print("c> SENDATTACH FAIL")
                return client.RC.ERROR

            if not os.path.isabs(file):
                print("c> SENDATTACH FAIL")
                return client.RC.ERROR

            if not os.path.isfile(file):
                print("c> SENDATTACH FAIL")
                return client.RC.ERROR

            sock = client._create_server_connection()
            client._send_cstring(sock, "SENDATTACH")
            client._send_cstring(sock, client._connected_user)
            client._send_cstring(sock, user)
            client._send_cstring(sock, message)
            client._send_cstring(sock, file)

            code = client._recv_byte(sock)

            if code == 0:
                msg_id = client._recv_cstring(sock)
                sock.close()
                print(f"c> SENDATTACH OK - MESSAGE {msg_id}")
                return client.RC.OK
            elif code == 1:
                sock.close()
                print("c> SENDATTACH FAIL, USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            else:
                sock.close()
                print("c> SENDATTACH FAIL")
                return client.RC.ERROR

        except Exception:
            print("c> SENDATTACH FAIL")
            return client.RC.ERROR

    @staticmethod
    def getFile(user, remote_file_name, local_file_name):
        try:
            if client._connected_user is None:
                print("c> FILE TRANSFER FAILED, user not connected.")
                return client.RC.USER_ERROR

            if user not in client._connected_users_info:
                client._refresh_users_cache(print_output=False)

                if user not in client._connected_users_info:
                    print("c> FILE TRANSFER FAILED, user not connected.")
                    return client.RC.USER_ERROR

            ip, port = client._connected_users_info[user]

            sock = socket.create_connection((ip, int(port)))
            client._send_cstring(sock, "GET FILE")
            client._send_cstring(sock, client._connected_user if client._connected_user else "")
            client._send_cstring(sock, remote_file_name)

            ok = client._recv_file(sock, local_file_name)
            sock.close()

            if not ok:
                print("c> FILE TRANSFER FAILED")
                return client.RC.ERROR

            print("c> FILE TRANSFER OK")
            return client.RC.OK

        except Exception:
            print("c> FILE TRANSFER FAILED")
            return client.RC.ERROR

    @staticmethod
    def shell():
        while True:
            try:
                command = input("c> ")
                line = command.split(" ")

                if len(line) > 0:
                    line[0] = line[0].upper()

                    if line[0] == "REGISTER":
                        if len(line) == 2:
                            client.register(line[1])
                        else:
                            print("Syntax error. Usage: REGISTER <userName>")

                    elif line[0] == "UNREGISTER":
                        if len(line) == 2:
                            client.unregister(line[1])
                        else:
                            print("Syntax error. Usage: UNREGISTER <userName>")

                    elif line[0] == "CONNECT":
                        if len(line) == 2:
                            client.connect(line[1])
                        else:
                            print("Syntax error. Usage: CONNECT <userName>")

                    elif line[0] == "DISCONNECT":
                        if len(line) == 2:
                            client.disconnect(line[1])
                        else:
                            print("Syntax error. Usage: DISCONNECT <userName>")

                    elif line[0] == "USERS":
                        if len(line) == 1:
                            client.users()
                        else:
                            print("Syntax error. Usage: USERS")

                    elif line[0] == "SEND":
                        if len(line) >= 3:
                            message = ' '.join(line[2:])
                            client.send(line[1], message)
                        else:
                            print("Syntax error. Usage: SEND <userName> <message>")

                    elif line[0] == "SENDATTACH":
                        if len(line) >= 4:
                            user = line[1]
                            file_name = line[-1]
                            message = ' '.join(line[2:-1])
                            client.sendAttach(user, file_name, message)
                        else:
                            print("Syntax error. Usage: SENDATTACH <userName> <message> <fileName>")

                    elif line[0] == "GETFILE":
                        if len(line) == 4:
                            client.getFile(line[1], line[2], line[3])
                        else:
                            print("Syntax error. Usage: GETFILE <userName> <fileName> <localFileName>")

                    elif line[0] == "QUIT":
                        if len(line) == 1:
                            if client._connected_user is not None:
                                user_to_disconnect = client._connected_user
                                client.disconnect(user_to_disconnect)
                            break
                        else:
                            print("Syntax error. Use: QUIT")

                    else:
                        print("Error: command " + line[0] + " not valid.")

            except KeyboardInterrupt:
                if client._connected_user is not None:
                    user_to_disconnect = client._connected_user
                    client.disconnect(user_to_disconnect)
                print("\n[INFO] Cerrando cliente (Ctrl+C)...")
                break
            except Exception as e:
                print("Exception: " + str(e))

    @staticmethod
    def usage():
        print("Usage: python3 client.py -s <server> -p <port>")

    @staticmethod
    def parseArguments(argv):
        parser = argparse.ArgumentParser()
        parser.add_argument('-s', type=str, required=True, help='Server IP')
        parser.add_argument('-p', type=int, required=True, help='Server Port')
        args = parser.parse_args()

        if args.s is None:
            parser.error("Usage: python3 client.py -s <server> -p <port>")
            return False

        if args.p < 1024 or args.p > 65535:
            parser.error("Error: Port must be in the range 1024 <= port <= 65535")
            return False

        client._server = args.s
        client._port = args.p

        return True

    @staticmethod
    def main(argv):
        if not client.parseArguments(argv):
            client.usage()
            return

        client.shell()
        print("+++ FINISHED +++")


if __name__ == "__main__":
    client.main(sys.argv[1:])