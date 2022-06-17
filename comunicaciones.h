#ifndef COMUNICACIONES_H_
#define COMUNICACIONES_H_


#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>
#include <commons/config.h>
#include <nivel-gui/nivel-gui.h>
#include <nivel-gui/tad_nivel.h>

//Las distintas comunicaciones
typedef enum {
	INICIAR_PATOTA, //Discordiador --> Mi-RAM
	DESPLAZAR_TRIPULANTE, //Discordiador --> Mi-RAM
	CAMBIAR_ESTADO_TRIPULANTE, //Discordiador --> Mi-RAM
	SOLICITAR_TAREA, //Discordiador --> Mi-RAM
	EXPULSAR_TRIPULANTE, //Discordiador --> Mi-RAM
	ACTIVAR_FSCK, //Discordiador --> i-Mongo
	OBTENER_BITACORA, //Discordiador --> i-Mongo Idpatota,Idtripulante.
	EMPEZAR_TAREA,	//tipoTarea tarea, char* Tarea ,int idTripulante, int idPatota, int recursosUtilizados
	FINALIZAR_TAREA,//tipoTarea tarea, char* Tarea ,int idTripulante, int idPatota, int recursosUtilizados
	DESPLAZAMIENTOIMONGO,
	ENVIAR_TAREA,
	PANICO, //Mi-RAM --> Discordiador
	VALIDAR_PATOTA, //Mi-RAM --> Discordiador
	ALERTA_SABOTAJE,//i-Mongo --> Discordiador
	EMPEZAR_SABOTAJE,
	FIN_SABOTAJE
} operacion;


typedef enum {
	NORMAL,
	GENERAR_OXIGENO, //Discordiador --> i-Mongo
	CONSUMIR_OXIGENO,
	GENERAR_COMIDA,
	CONSUMIR_COMIDA,
	GENERAR_BASURA,
	DESCARTAR_BASURA
}tipoTarea;


typedef struct
{
	int size;
	void* stream;
} t_buffer;

typedef struct
{
	operacion codigo_operacion;
	t_buffer* buffer;
} t_paquete;


void* serializar_paquete(t_paquete*, int);
void crear_buffer(t_paquete*);
t_paquete* crear_paquete(void);
void agregar_a_paquete(t_paquete*, void*, int);
void enviar_paquete(t_paquete*, int);
void eliminar_paquete(t_paquete*);

int recibir_operacion(int);
void* recibir_buffer(int*, int);

int crear_conexion(char*, char*);
void liberar_conexion(int);

int iniciar_servidor(char*, char*);
int esperar_cliente(int);


t_config* leer_config(char*);

void enviar_paquete2(t_paquete*, int32_t);
void* recibir_buffer2(int*, int);

#endif /* COMUNICACIONES_H_ */
