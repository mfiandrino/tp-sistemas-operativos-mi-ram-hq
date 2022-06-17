
#ifndef FUNCIONES_MI_RAM_HQ_H
#define FUNCIONES_MI_RAM_HQ_H

#include <stdio.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/bitarray.h>
#include <commons/temporal.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include "../comunicaciones.h"
#include <math.h>
#include <semaphore.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <nivel-gui/nivel-gui.h>
#include <nivel-gui/tad_nivel.h>

#define CONFIG_PATH "/home/utnso/TP/tp-2021-1c-Los-futuros-Ingenieros/Mi-RAM-HQ/mi-ram-hq.config"
#define LOG_PATH "ram.log"

// DESPLAZAMIENTOS DEL STRUCT TCB
#define DESP_TCB_TID 0
#define DESP_TCB_ESTADO DESP_TCB_TID + sizeof(int32_t)
#define DESP_TCB_POSX DESP_TCB_ESTADO + sizeof(char)
#define DESP_TCB_POSY DESP_TCB_POSX + sizeof(int32_t)
#define DESP_TCB_PROX_INSTR DESP_TCB_POSY + sizeof(int32_t)
#define DESP_TCB_PUNTEROPCB DESP_TCB_PROX_INSTR + sizeof(int32_t)

// DESPLAZAMIENTOS DEL STRUCT PCB
#define DESP_PCB_PID 0
#define DESP_PCB_TAREAS DESP_PCB_PID + sizeof(int32_t)

// DESPLAZAMIENTOS DEL "STRUCT" TAREAS
#define DESP_TAREAS 0

#define F_NEGRO 	"\e[30m"
#define F_ROJO 		"\e[31m"
#define F_VERDE 	"\e[32m"
#define F_AMARILLO 	"\e[33m"
#define F_AZUL 		"\e[34m"
#define F_MAGENTA 	"\e[35m"
#define F_CYAN 		"\e[36m"
#define F_BLANCO 	"\e[37m"
#define F_GRIS		"\e[90m"

#define F_FONDO_NEGRO 		"\e[40m"
#define F_FONDO_ROJO 		"\e[41m"
#define F_FONDO_VERDE 		"\e[42m"
#define F_FONDO_AMARILLO	"\e[43m"
#define F_FONDO_AZUL 		"\e[44m"
#define F_FONDO_MAGENTA 	"\e[45m"
#define F_FONDO_CYAN 		"\e[46m"
#define F_FONDO_BLANCO 		"\e[47m"

#define F_FONDO_MAS_NEGRO 		"\e[100m"
#define F_FONDO_MAS_ROJO 		"\e[101m"
#define F_FONDO_MAS_VERDE 		"\e[102m"
#define F_FONDO_MAS_AMARILLO	"\e[103m"
#define F_FONDO_MAS_AZUL 		"\e[104m"
#define F_FONDO_MAS_MAGENTA 	"\e[105m"
#define F_FONDO_MAS_CYAN 		"\e[106m"
#define F_FONDO_MAS_BLANCO 		"\e[107m"

#define F_SUAVE 	"\e[2m"
#define F_SUBRAYADO "\e[4m"
#define F_TACHADO 	"\e[9m"
#define F_NEGRITA 	"\e[1m"
#define F_RESET 	"\e[0m"

//SEMAFOROS
sem_t semIniciarPatotaMapa;
sem_t semDesplazarTripMapa;
sem_t semExpulsarTripMapa;
sem_t semExpulsarTripMapa2;
pthread_mutex_t sem_dump;
pthread_mutex_t sem_memoriaReal;
pthread_mutex_t sem_swapFile;
pthread_mutex_t sem_listaProcesos;
pthread_mutex_t sem_listaHuecos;
pthread_mutex_t sem_listaHuecosAux;
pthread_mutex_t sem_framesMP;
pthread_mutex_t sem_paginasMV;
pthread_mutex_t sem_guardarPatota;
//pthread_mutex_t sem_compactar;
pthread_mutex_t sem_imprimir;
//pthread_mutex_t sem_dibujar;
pthread_mutex_t sem_sincroImprimir;
sem_t sem_compactar;
pthread_mutex_t sem_idsDesplazar;
pthread_mutex_t sem_idsExpulsar;
pthread_mutex_t sem_idsIniciar;
pthread_mutex_t sem_ptrClock;

typedef struct {
	int32_t pid;
	int32_t tareas; // DIRECCION LOGICA DEL INICIO DE LAS TAREAS (int32_t)
}PCB;

typedef struct __attribute__((__packed__)){
	int32_t tid;
	char estado;
	int32_t posX;
	int32_t posY;
	int32_t proximaInstruccion; //IDENTIFICADOR DE PROXIMA TAREA !!
	int32_t puntero_PCB; //DIRECCION LOGICA DE MEMORIA int32_t !!
}TCB;

typedef enum {
	PAGINACION,
	SEGMENTACION,
	SIN_ESQUEMA
} esquema_memoria;

typedef enum {
	FIRST_FIT,
	BEST_FIT,
	SIN_CRITERIO
} criterio_segmentacion;

typedef enum {
	LRU,
	CLOCK,
	SIN_ALGORITMO
} algoritmoReemplazo;


typedef enum {
	S_TAREAS,
	S_PCB,
	S_TCB
} seccionDePatota;


typedef struct{
	pthread_t hilo;
	int socket;
} conexionAtendida;

typedef struct{
	int32_t id;
	int32_t inicio;
	int32_t size;
} tabla_direcciones;

typedef struct{
	int32_t nroPagina;
	int32_t nroFrame;
	int32_t bitPresencia;
	int32_t bitUso;
	char* ultimaReferencia;
} tabla_paginas;

typedef struct{
	int32_t id;
	int32_t largoTareas;
	int32_t cantTrip;
	t_list *paginas;
	pthread_mutex_t sem_paginas;
	t_list *segmentos;
	pthread_mutex_t sem_segmentos;
	t_list *espaciosLibres;
	pthread_mutex_t sem_espaciosLibres;
	int32_t sizePatota;
	t_list *idsTripExpulsados;
	pthread_mutex_t sem_idsTripExpulsados;
} proceso;

typedef enum{
	SIN_ESTADO_RAM,
    NEW_RAM,                    //Recién creado
    READY_RAM,                  //En la cola ready
    BLOCKED_I_O_RAM,            //Bloqueado por Entrada/Salida
    BLOCKED_SABOTAGE_RAM,       //Bloquedo por sabotaje
    EXEC_RAM,                   //Ejecutando
    EXIT_RAM                    //Objetivos personales cumplidos
} StatusRam;

//VAN CON SEMAFOROS-------------------------------------
//1- Listas globales
//2- Variables globales

t_list *listaProcesos, *listaHuecos, *listaHuecosAux;
void *memoriaReal;
t_bitarray *framesMP;
t_bitarray *paginasMV;
int32_t punteroClock;
//-------------------------------------------------------

//MAPA
t_dictionary *itemsID;
NIVEL* nivel;
char *idMapa;
int32_t idPatotaADesplazar;
int32_t idTripulanteADesplazar;
int32_t idPatotaAExpulsar;
int32_t idTripulanteAExpulsar;
int32_t idPatotaAIniciar;


//--------------------


t_log* loggerRam;
int32_t contHuecos;
int32_t tamanio_pagina;
algoritmoReemplazo cod_algoritmoReemplazo;
esquema_memoria cod_esquema;
criterio_segmentacion cod_criterio;
int32_t tamanio_swap;
int32_t cantFramesMP;
int32_t cantFramesMV;
char* pathSwapFile;


void liberar_string_split(char**);
int longitudArray (char**);

//Inicializar
void inicializar(int*);


//Atender Tripulantes
void hiloAtenderTripulates(int*);
void atenderConexion(int*);

void iniciarPatotaRam(int);
t_list* recibir_paquete_iniciarPatotaRam(int);
t_paquete *crearPaqueteValidarPatota(int32_t);



void cambiarEstadoTripulanteRam(int);
t_list* recibir_paquete_cambiarEstadoTripulante(int);

void expulsarTripulanteRam(int);
t_list *recibir_paquete_expulsarTripulante(int);

void solicitarTareaRam(int);
t_list *recibir_paquete_solicitarTareaRam(int);
t_paquete *crearPaqueteEnviarTarea(char*);


void desplazarTripulanteRam(int);
t_list *recibir_paquete_desplazarTripulante(int);



//Hilo actualizarMapa
int32_t hiloActualizarMapa(void);

int iniciarPatotaMapa(void);
void moverTripMapa(void);
void expulsarTripMapa(void);




//Dump de Memoria
void hiloDumpMemoria(void);
void realizarDumpMemoria(void);
void signalDump(void);

//Funciones Memoria
void reservarMemoria(t_config*);
esquema_memoria obtenerCodigoEsquema(char*);
criterio_segmentacion obtenerCriterio(char*);
algoritmoReemplazo obtenerAlgoritmoReemplazo(char*);
void imprimirMemoria(void);



int32_t guardarPatota(t_list*);
int32_t traducirLogicaAFisica(int32_t);
void *buscarDato(int32_t, int32_t);
int32_t generarDireccionLogica(int32_t, int32_t, int32_t, int32_t);
proceso *buscarProceso(int32_t);
int32_t *buscarTripExpulsado(int32_t, proceso*);
char convertirEstado(StatusRam);


//SEGMENTACION
void imprimirMemoriaConSegmentacion(void);
int32_t generarDireccionLogicaConSegmentacion(int32_t, int32_t, int32_t);
int32_t validarPatotaConSegmentacion(int32_t, int32_t);
int32_t validarSegmentacion(int32_t size);
int32_t guardarSegmento(void*, int32_t);
t_list *guardarPatotaConSegmentacion(t_list*);
void *buscarDatoSegmentacion(int32_t,int32_t);
tabla_direcciones *buscarSegmentoDeUnProceso (int32_t, int32_t);
int32_t traducirLogicaAFisicaConSegmentacion(int32_t);
void modificarDatoTCB(int32_t, int32_t, int32_t, void*, int32_t);
void agregarATabla(t_list*, int32_t, int32_t, int32_t);
bool compararInicio(tabla_direcciones*, tabla_direcciones*);
bool compararSize(tabla_direcciones*, tabla_direcciones*);
bool hayEspacio(tabla_direcciones*);
void compactarMemoria();
void agregarHueco(t_list*, int32_t, int32_t, int32_t);
void imprimirTabla(tabla_direcciones*);
void imprimirListaTabla(t_list*);
void crearListaHuecosAuxiliar(void);
void borrarListaHuecosAuxiliar(void);
void expulsarTripulanteConSegmentacion(int32_t,int32_t);
void signalCompactar(void);
void hiloCompactar(void);
void solicitarTareaSegmentacion(int32_t *, int32_t *, int);



//PAGINACION
void imprimirMemoriaConPaginacion(void);
int32_t generarDireccionLogicaConPaginacion(int32_t, int32_t, int32_t, int32_t);
int32_t validarPatotaConPaginacion(int32_t, int32_t);
t_list *guardarPatotaConPaginacion(t_list*);
int32_t espacioLibreBitarray(t_bitarray*,int32_t);
int32_t primerFrameLibre(t_bitarray*,int32_t);
void inicializarBitarray(t_bitarray*,int32_t);
int32_t traducirLogicaAFisicaConPaginacion(int32_t);
tabla_paginas *buscarPagina(t_list *, int32_t);
proceso *buscarProcesoPaginado(int32_t);
void *buscarDatoPaginacion(int32_t, int32_t);
proceso *buscarProcesoEnFrame(int32_t, int32_t*,int32_t);
void traerAMemoria(int32_t, int32_t);
int32_t obtenerFrameAReemplazar(void);
int32_t algoritmoLRU(void);
void expulsarTripulanteConPaginacion(int32_t,int32_t);
int32_t tipoDeEspacioIncluidoEnPagina(tabla_direcciones*, int32_t);
void revisarFrameLibre(int32_t, proceso*);
void desmembrarDireccionLogicaConPaginacion(int32_t, int32_t*, int32_t*, int32_t*);
void expulsarPatotaConPaginacion(proceso*);
void solicitarTareaPaginacion(int32_t *, int32_t *, int);
void crearSwapFile(void);


#define ASSERT_CREATE(nivel, id, err)                                                   \
if(err) {                                                                           \
	nivel_destruir(nivel);                                                          \
	nivel_gui_terminar();                                                           \
	fprintf(stderr, "Error al crear '%c': %s\n", id, nivel_gui_string_error(err));  \
	return EXIT_FAILURE;                                                            \
}



#endif //FUNCIONES_MI_RAM_HQ_H
