# ServicioDeEnvioDeMensajes-SistemasDistribuidos

# Autoras
Ana Grima Vázquez de Prada y Alicia Mei García Morín


# Compilación y despliegue de la aplicación

## 1. Compilación

Para compilar la parte desarrollada en C, situarse en el directorio donde se encuentran los ficheros fuente y ejecutar:

make clean
make

El Makefile genera automáticamente los ficheros necesarios del servicio RPC a partir del fichero logger.x mediante rpcgen.

Después de compilar correctamente se generan los siguientes ejecutables:

server
logger_server

El ejecutable server corresponde al servidor principal de mensajería.
El ejecutable logger_server corresponde al servidor RPC encargado de registrar las operaciones realizadas por los usuarios.

------------------------------
## 2. Despliegue del servicio web

El servicio web debe ejecutarse en la misma máquina donde se ejecute cada cliente, ya que el cliente accede a él mediante localhost.

Para arrancar el servicio web, ejecutar en una terminal:

python3 web_service.py

El servicio web queda escuchando en:

http://localhost:5000

Este proceso se encarga de normalizar los mensajes antes de enviarlos, eliminando espacios en blanco repetidos.

------------------------------
## 3. Despliegue del servidor RPC

Antes de ejecutar el servidor RPC, debe estar activo el servicio rpcbind en la máquina donde se lance logger_server.

Para arrancar el servidor RPC, ejecutar en otra terminal:

./logger_server

Este proceso queda esperando peticiones RPC del servidor principal e imprime por pantalla las operaciones realizadas por los usuarios.

------------------------------
## 4. Despliegue del servidor principal

El servidor principal necesita conocer la dirección IP o el nombre de la máquina donde se ejecuta el servidor RPC. Para ello se utiliza la variable de entorno LOG_RPC_IP.

Si el servidor RPC se está ejecutando en la misma máquina que el servidor principal, ejecutar:

export LOG_RPC_IP=localhost

A continuación, arrancar el servidor principal indicando el puerto de escucha:

./server -p <PUERTO>

Por ejemplo, para ejecutar el servidor principal en el puerto 8888:

export LOG_RPC_IP=localhost
./server -p 8888

El servidor principal queda escuchando conexiones TCP de los clientes en el puerto indicado.

------------------------------
## 5. Despliegue de los clientes

Para ejecutar un cliente, debe indicarse la IP o nombre de la máquina donde se ejecuta el servidor principal y el puerto utilizado por dicho servidor.

La forma general de ejecución es:

python3 client.py -s <IP_SERVIDOR> -p <PUERTO>

Si el servidor principal se ejecuta en la misma máquina y escucha en el puerto 8888, ejecutar:

python3 client.py -s localhost -p 8888

Si el servidor principal se ejecuta en otra máquina o contenedor, sustituir localhost por la IP correspondiente:

python3 client.py -s <IP_SERVIDOR_PRINCIPAL> -p 8888

------------------------------
## 6. Orden recomendado de ejecución

Para desplegar correctamente toda la aplicación, se recomienda ejecutar los procesos en el siguiente orden:

1. En una terminal, arrancar el servicio web:

python3 web_service.py

2. En otra terminal, arrancar el servidor RPC:

./logger_server

3. En otra terminal, arrancar el servidor principal:

export LOG_RPC_IP=localhost
./server -p 8888

4. En una o varias terminales adicionales, arrancar los clientes:

python3 client.py -s localhost -p 8888

Si los clientes se ejecutan en máquinas distintas, en cada máquina cliente debe ejecutarse también el servicio web:

python3 web_service.py

Además, los clientes deben conectarse usando la IP real de la máquina donde se ejecuta el servidor principal.

------------------------------
## 7. Ejecución en varias máquinas

Si los procesos se despliegan en máquinas distintos:

- En la máquina del servicio RPC se ejecuta:

./logger_server

- En la máquina del servidor principal se define LOG_RPC_IP con la IP de la máquina donde está el servidor RPC:

export LOG_RPC_IP=<IP_SERVIDOR_RPC>
./server -p <PUERTO>

- En cada máquina cliente se ejecuta el servicio web:

python3 web_service.py

- En cada máquina cliente se ejecuta el cliente indicando la IP del servidor principal:

python3 client.py -s <IP_SERVIDOR_PRINCIPAL> -p <PUERTO>

Para que la transferencia de ficheros funcione correctamente, los clientes deben poder conectarse entre sí mediante las IP y puertos que devuelve la operación USERS.

------------------------------
## 8. Limpieza

Para eliminar los ejecutables, ficheros objeto y ficheros generados automáticamente por rpcgen, ejecutar:

make clean

------------------------------
## 9. Resumen

### Compilación

make clean
make

### Terminal 1: servicio web

python3 web_service.py

### Terminal 2: servidor RPC

./logger_server

### Terminal 3: servidor principal

export LOG_RPC_IP=localhost
./server -p 8888

### Terminal 4: cliente

python3 client.py -s localhost -p 8888

### Limpieza

make clean
