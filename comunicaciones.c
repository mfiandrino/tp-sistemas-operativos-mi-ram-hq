#include "comunicaciones.h"

//Funciones de emisor
void* serializar_paquete(t_paquete* paquete, int bytes)
{
	void * magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

void crear_buffer(t_paquete* paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}


t_paquete* crear_paquete(void)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = -1;
	crear_buffer(paquete);
	return paquete;
}

t_paquete* crear_paquete_operacion(operacion OP)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = OP;
	crear_buffer(paquete);
	return paquete;
}


void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio)
{
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int)); //Tamaño anterior + Tamaño a agregar + 4B para indicar el tamaño

	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

	paquete->buffer->size += tamanio + sizeof(int);
}


void enviar_paquete(t_paquete *paquete, int32_t socket_cliente){
	if(socket_cliente != -1){
		int32_t bytes = paquete->buffer->size + 2*sizeof(int32_t); //cod_operacion + size_paquete + paquete
		void *buffer = serializar_paquete(paquete, bytes);
		send(socket_cliente, buffer, bytes, 0);
		free(buffer);
		int32_t ack;
		recv(socket_cliente,&ack,sizeof(int32_t),0);
	}
	else
		printf("No se pudo enviar el paquete (Socket Invalido)");
}

void* recibir_buffer(int* size, int socket_cliente)
{
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	int32_t ack = 1;

	send(socket_cliente,&ack,sizeof(int32_t),0);

	return buffer;
}

void eliminar_paquete(t_paquete* paquete)
{
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

//Funciones de receptor
int recibir_operacion(int socket_cliente)
{
	int cod_op;
	//printf("RECIBIR OP");
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) != 0){
		//printf("RECIBIR OP if");
		return cod_op;
	}
	else
	{
		//printf("RECIBIR OP else");
		close(socket_cliente);
		return -1;
	}
}


int crear_conexion(char *ip, char* puerto)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &server_info);

	int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
	//printf("socket_cliente: %d\n",socket_cliente);

	if(connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1)
	{
		printf("\nError al realizar conexion\n");
		socket_cliente = -1;
	}


	freeaddrinfo(server_info);

	return socket_cliente;
}

void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}


t_config* leer_config(char *path)
{
	t_config *config = config_create(path);
	if(config)
		return config;
	else
		printf("\nError al abrir el archivo de configuracion\n");
	exit(1);
}


int esperar_cliente(int sk_servidor)
{
	struct sockaddr_in dir_cliente;
	int32_t tam_direccion = sizeof(struct sockaddr_in);

	int sk_cliente = accept(sk_servidor, (void*) &dir_cliente, &tam_direccion);

	//printf("Se conecto un cliente!");

	return sk_cliente;
}




int iniciar_servidor(char *ip, char* puerto)
{
	int aux = 1;
	int sk_servidor;

    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(ip, puerto, &hints, &servinfo);

    for (p=servinfo; p != NULL; p = p->ai_next)
    {
        if ((sk_servidor = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;

        //setsockopt(sk_servidor,SOL_SOCKET,SO_REUSEADDR,&aux,sizeof(aux));

        if (bind(sk_servidor, p->ai_addr, p->ai_addrlen) == -1) {
            close(sk_servidor);
            continue;
        }
        break;
    }

	listen(sk_servidor, SOMAXCONN);

    freeaddrinfo(servinfo);

    return sk_servidor;
}
