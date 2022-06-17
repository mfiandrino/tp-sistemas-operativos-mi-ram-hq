#include "funciones_mi-ram-hq.h"

//----------INICIALIZACIONES----------
void inicializarSemaforos()
{
	//Semaforos generales
	pthread_mutex_init(&sem_guardarPatota,0);
	pthread_mutex_init(&sem_ptrClock,0);
	pthread_mutex_init(&sem_imprimir,0);

	//Semaforo Memorias
	pthread_mutex_init(&sem_memoriaReal,0);
	pthread_mutex_init(&sem_swapFile,0);

	//Semaforos listas
	pthread_mutex_init(&sem_listaProcesos,0);
	pthread_mutex_init(&sem_listaHuecos,0);
	pthread_mutex_init(&sem_listaHuecosAux,0);

	//Semaforo compactar memoria
	sem_init(&sem_compactar,0,0);

	//Semaforo dump de memoria
	pthread_mutex_init(&sem_dump,0);

	//Semaforos mapa
	sem_init(&semIniciarPatotaMapa,0,0);
	sem_init(&semDesplazarTripMapa,0,0);
	sem_init(&semExpulsarTripMapa,0,0);
	sem_init(&semExpulsarTripMapa2,0,0);
	pthread_mutex_init(&sem_idsIniciar,0);
	pthread_mutex_init(&sem_idsDesplazar,0);
	pthread_mutex_init(&sem_idsExpulsar,0);

	//Semaforos bitarrays
	pthread_mutex_init(&sem_framesMP,0);
	pthread_mutex_init(&sem_paginasMV,0);
}

void inicializar(int *sk_server)
{
	log_info(loggerRam, "Iniciando mi-Ram");
	punteroClock = 0;


	t_config *config = leer_config(CONFIG_PATH);

	// Iniciar el servidor
	char *puerto = config_get_string_value(config, "PUERTO");
	char *ip = config_get_string_value(config, "IP");
	*sk_server = iniciar_servidor(ip, puerto);

	// Reservar el espacio de memoria indicado por el archivo de configuración.
	reservarMemoria(config);


	// Dibujar el mapa inicial vacío, dado de que al inicio no se encontrará ningún tripulante conectado.
	log_info(loggerRam, "mi-Ram inicializado correctamente");
	config_destroy(config);

	listaProcesos = list_create();
	return;
}

void reservarMemoria(t_config *config){
	log_info(loggerRam,"Comienzo a Reservar Memoria");

	int tamanio_memoria = config_get_int_value(config,"TAMANIO_MEMORIA");
	log_info(loggerRam,"Tamanio Memoria: %d",tamanio_memoria);

	memoriaReal = malloc(tamanio_memoria);

	cod_esquema = obtenerCodigoEsquema(config_get_string_value(config,"ESQUEMA_MEMORIA"));
	switch(cod_esquema){
		case PAGINACION:
			log_info(loggerRam,"Se utilizo Esquema de Paginación Simple con Memoria Virtual");
			cod_algoritmoReemplazo = obtenerAlgoritmoReemplazo(config_get_string_value(config,"ALGORITMO_REEMPLAZO"));
			tamanio_pagina = 64;
			tamanio_pagina = config_get_int_value(config,"TAMANIO_PAGINA");
			tamanio_swap = config_get_int_value(config,"TAMANIO_SWAP");
			pathSwapFile = string_duplicate(config_get_string_value(config,"PATH_SWAP"));
			log_info(loggerRam,"pathSwapFile: %s",pathSwapFile);
			log_info(loggerRam,"Tamanio Pagina: %d",tamanio_pagina);
			log_info(loggerRam,"Tamanio Swap: %d",tamanio_swap);

			crearSwapFile();

			cantFramesMP = tamanio_memoria / tamanio_pagina; // 2048/64 = 32
			log_info(loggerRam,"tamanio_memoria : %d",tamanio_memoria);
			log_info(loggerRam," :tamanio_pagina %d",tamanio_pagina);
			log_info(loggerRam,"cantPaginasMP : %d",cantFramesMP);
			char *bitarrayMP = malloc(ceil(cantFramesMP/8.0));
			framesMP = bitarray_create_with_mode(bitarrayMP, ceil(cantFramesMP/8.0), LSB_FIRST);

			cantFramesMV = tamanio_swap / tamanio_pagina; //4096/64 = 64
			char *bitarrayMV = malloc(ceil(cantFramesMV/8.0));
			paginasMV = bitarray_create_with_mode(bitarrayMV, ceil(cantFramesMV/8.0), LSB_FIRST);

			inicializarBitarray(framesMP,cantFramesMP);
			inicializarBitarray(paginasMV,cantFramesMV);

			break;


		case SEGMENTACION:
			log_info(loggerRam,"Se utilizo Esquema de Segmentacion Pura");
			cod_criterio = obtenerCriterio(config_get_string_value(config,"CRITERIO_SELECCION"));
			listaHuecos = list_create();
			switch(cod_criterio){
				case FIRST_FIT:
					log_info(loggerRam,"Criterio First Fit");
					break;
				case BEST_FIT:
					log_info(loggerRam,"Criterio Best Fit");
					break;
				default:
					log_info(loggerRam, "Error de criterio");
					break;
			}

			contHuecos = -1;
			agregarATabla(listaHuecos,contHuecos,0,tamanio_memoria);

			break;
		default:
			log_info(loggerRam,"Error de configuracion");
			break;
	}
}


esquema_memoria obtenerCodigoEsquema(char *esquema){
	if(string_equals_ignore_case(esquema,"PAGINACION"))
		return PAGINACION;
	else
		if(string_equals_ignore_case(esquema,"SEGMENTACION"))
			return SEGMENTACION;
		else
			return SIN_ESQUEMA;
}

criterio_segmentacion obtenerCriterio(char* criterio){
	if(string_equals_ignore_case(criterio,"FF"))
			return FIRST_FIT;
		else
			if(string_equals_ignore_case(criterio,"BF"))
				return BEST_FIT;
			else
				return SIN_CRITERIO;
}

algoritmoReemplazo obtenerAlgoritmoReemplazo(char* algoritmo){
	if(string_equals_ignore_case(algoritmo,"LRU"))
			return LRU;
		else
			if(string_equals_ignore_case(algoritmo,"CLOCK"))
				return CLOCK;
			else
				return SIN_ALGORITMO;
}

void inicializarBitarray(t_bitarray *bitarray, int32_t cantPaginas)
{
	for(int i=0;i<cantPaginas;i++){
		bitarray_clean_bit(bitarray,i);
	}
}

void crearSwapFile()
{
	int swapF = open(pathSwapFile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if(swapF != -1){
		ftruncate(swapF, tamanio_swap);
		fsync(swapF);
		close(swapF);
	}
	else
		log_info(loggerRam, "Error al abrir el SwapFile");

}
//---------------------------------------





//----------ATENDER AL DISCORDIADOR----------
void hiloAtenderTripulates(int *sk_server)
{
	int32_t contHilo = 0;
	int sk_client;

	conexionAtendida *hiloXConexion = malloc(sizeof(conexionAtendida));

	while(1)
	{
		log_info(loggerRam,"antes del sk_client");
		sk_client = esperar_cliente(*sk_server);

		hiloXConexion = realloc(hiloXConexion, sizeof(conexionAtendida) * (contHilo+1));
		hiloXConexion[contHilo].socket = sk_client;

		//log_info(loggerRam,"hiloXConexion[contHilo].socket: %d",hiloXConexion[contHilo].socket);
		//log_info(loggerRam,"hiloXConexion[contHilo].hilo: %d",hiloXConexion[contHilo].hilo);

		pthread_create(&hiloXConexion[contHilo].hilo,NULL,(void*) atenderConexion, &hiloXConexion[contHilo].socket);
		pthread_detach(hiloXConexion[contHilo].hilo); //PARA QUE SE LIBERE EL MISMO AL TERMINAR
		contHilo++;
	}
	return;
}

void atenderConexion(int *sk_client){
	int32_t evento = -1;
	log_info(loggerRam,"Un cliente se ha conectado");
	evento = recibir_operacion(*sk_client);
	log_info(loggerRam,"Evento: %d",evento);



	switch(evento){
		case INICIAR_PATOTA:
			iniciarPatotaRam(*sk_client);
			log_info(loggerRam, "termine iniciarPatota");
			imprimirMemoria();
			break;
		case DESPLAZAR_TRIPULANTE:
			desplazarTripulanteRam(*sk_client);
			log_info(loggerRam, "termine desplazarTripulante");
			break;
		case CAMBIAR_ESTADO_TRIPULANTE:
			cambiarEstadoTripulanteRam(*sk_client);
			log_info(loggerRam, "termine cambiarEstadoTripulante");
			break;
		case SOLICITAR_TAREA:
			solicitarTareaRam(*sk_client);
			log_info(loggerRam, "termine solicitarTarea");
			break;
		case EXPULSAR_TRIPULANTE:
			expulsarTripulanteRam(*sk_client);
			log_info(loggerRam, "termine expulsarTripulante");
			break;
		case -1:
			log_info(loggerRam, "Error, el evento es -1");
			break;
		default:
			log_info(loggerRam, "No es para miRam");
			break;
	}

	liberar_conexion(*sk_client);
}
//-----------------------------------------





//---------INICIAR PATOTA----------
void iniciarPatotaRam(int sk_client)
{
	log_info(loggerRam, "Iniciar Patota");
	t_list *listaPaquete = recibir_paquete_iniciarPatotaRam(sk_client);

	// GUARDO LA PATOTA
	pthread_mutex_lock(&sem_guardarPatota);
	log_info(loggerRam,"Antes de if guardarPatota");
	if(guardarPatota(listaPaquete)){
		t_paquete *paqueteValidarPatota = crearPaqueteValidarPatota(1);
		enviar_paquete2(paqueteValidarPatota, sk_client);
		eliminar_paquete(paqueteValidarPatota);
		log_info(loggerRam,"Hubo espacio suficiente en RAM");

		pthread_mutex_lock(&sem_idsIniciar);
		idPatotaAIniciar = *((int*)list_get(listaPaquete,2));
		sem_post(&semIniciarPatotaMapa); //Signal para que el mapa cargue los tripulantes
	}
	else{
		log_info(loggerRam,"Se mandara paqueteValidarPatota(0)");
		t_paquete *paqueteValidarPatota = crearPaqueteValidarPatota(0);
		enviar_paquete2(paqueteValidarPatota, sk_client);
		eliminar_paquete(paqueteValidarPatota);
		log_info(loggerRam,"No hay espacio en la RAM");
	}

	if(cod_esquema == SEGMENTACION)
		borrarListaHuecosAuxiliar();

	pthread_mutex_unlock(&sem_guardarPatota);

	log_info(loggerRam, "Fin Iniciar Patota");
	list_destroy_and_destroy_elements(listaPaquete,(void*)free);
}

t_list* recibir_paquete_iniciarPatotaRam(int socket_cliente)
{
	int size, tamanio, desplazamiento = 0;
	void * buffer;
	t_list* valores = list_create();
	char *archTareas;
	int *cantTrip=NULL, *idPatota=NULL, *idTripulante=NULL, *posX=NULL, *posY=NULL;

	buffer = recibir_buffer2(&size, socket_cliente);

	//lista: cantTrip, archTareas, IdPatota, IdTrip1, PosX1, PosY1, IdTrip2, PosX2, PosY2,....

	//Cantidad de tripulantes
	memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	cantTrip = malloc(tamanio);
	memcpy(cantTrip, buffer+desplazamiento, tamanio);
	desplazamiento+=tamanio;
	list_add(valores,cantTrip);
	log_info(loggerRam,"cantTrip: %d",*cantTrip);

	// cantTrip * sizeof(TCB)

	//Archivo de tareas
	memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	archTareas = malloc(tamanio);
	memcpy(archTareas, buffer+desplazamiento, tamanio);
	desplazamiento+=tamanio;
	list_add(valores, archTareas);
	log_info(loggerRam,"archTareas: %s",archTareas);

	log_info(loggerRam,"tamanio archTareas: %d",strlen(archTareas));


	//Id de Patota
	memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	idPatota = malloc(tamanio);
	memcpy(idPatota, buffer+desplazamiento, tamanio);
	desplazamiento+=tamanio;
	list_add(valores, idPatota);
	log_info(loggerRam,"idPatota: %d",*idPatota);

	for(int i=0; i<*cantTrip ; i++)
	{
		memcpy(&tamanio, buffer+desplazamiento, sizeof(int));
		desplazamiento += sizeof(int);
		idTripulante = malloc(tamanio);
		memcpy(idTripulante, buffer+desplazamiento, tamanio);
		desplazamiento += tamanio;
		list_add(valores, idTripulante);
		log_info(loggerRam,"idTrip: %d",*idTripulante);

		memcpy(&tamanio, buffer+desplazamiento, sizeof(int));
		desplazamiento += sizeof(int);
		posX = malloc(tamanio);
		memcpy(posX, buffer+desplazamiento, tamanio);
		desplazamiento += tamanio;
		list_add(valores, posX);
		log_info(loggerRam,"posX: %d",*posX);

		memcpy(&tamanio, buffer+desplazamiento, sizeof(int));
		desplazamiento += sizeof(int);
		posY = malloc(tamanio);
		memcpy(posY, buffer+desplazamiento, tamanio);
		desplazamiento += tamanio;
		list_add(valores, posY);
		log_info(loggerRam,"posY: %d",*posY);
	}

	free(buffer);
	return valores;
}

t_paquete *crearPaqueteValidarPatota(int32_t respuesta)
{
	t_paquete* paquete = crear_paquete();
	paquete->codigo_operacion=VALIDAR_PATOTA;
	agregar_a_paquete(paquete, &respuesta, sizeof(int32_t)); // Agrego si hay lugar en ram

	return paquete;
}

int32_t guardarPatota(t_list *listaPaquete){

	int32_t cantTrip = *((int*)list_get(listaPaquete,0));
	int32_t largoTareas = 1+strlen((char*)list_get(listaPaquete,1));
	int32_t pid = *((int*)list_get(listaPaquete,2));

	switch(cod_esquema){
		case SEGMENTACION:
			crearListaHuecosAuxiliar();
			if(validarPatotaConSegmentacion(cantTrip,largoTareas))
			{
				proceso *proceso1 = malloc(sizeof(proceso));
				proceso1->id = pid;
				proceso1->segmentos = guardarPatotaConSegmentacion(listaPaquete);
				proceso1->largoTareas = largoTareas;
				proceso1->cantTrip = cantTrip;
				proceso1->sizePatota = sizeof(PCB) + largoTareas + cantTrip * sizeof(TCB);
				proceso1->idsTripExpulsados = list_create();
				pthread_mutex_init(&(proceso1->sem_segmentos),0);
				pthread_mutex_init(&(proceso1->sem_idsTripExpulsados),0);
				pthread_mutex_lock(&sem_listaProcesos);
				list_add(listaProcesos,proceso1);
				pthread_mutex_unlock(&sem_listaProcesos);
				return 1;
			}
			else
			{
				log_info(loggerRam,"No entra en memoria fragmentada.");
				compactarMemoria();
				borrarListaHuecosAuxiliar();
				crearListaHuecosAuxiliar();
				if(validarPatotaConSegmentacion(cantTrip,largoTareas))
				{
					proceso *proceso1 = malloc(sizeof(proceso));
					proceso1->id = pid;
					proceso1->segmentos = guardarPatotaConSegmentacion(listaPaquete);
					proceso1->largoTareas = largoTareas;
					proceso1->cantTrip = cantTrip;
					proceso1->sizePatota = sizeof(PCB) + largoTareas + cantTrip * sizeof(TCB);
					proceso1->idsTripExpulsados = list_create();
					pthread_mutex_init(&(proceso1->sem_segmentos),0);
					pthread_mutex_init(&(proceso1->sem_idsTripExpulsados),0);
					pthread_mutex_lock(&sem_listaProcesos);
					list_add(listaProcesos,proceso1);
					pthread_mutex_unlock(&sem_listaProcesos);
					return 1;
				}
				else{
					log_info(loggerRam,"No entra en memoria compactada");
					return 0;
				}
			}

			break;
		case PAGINACION:
			if(validarPatotaConPaginacion(cantTrip,largoTareas))
			{
				log_info(loggerRam,"Entro a guardar patota con paginacion");
				proceso *proceso1 = malloc(sizeof(proceso));
				proceso1->id = pid;
				proceso1->largoTareas = largoTareas;
				proceso1->cantTrip = cantTrip;
				proceso1->paginas= guardarPatotaConPaginacion(listaPaquete);
				proceso1->espaciosLibres = list_create();
				proceso1->sizePatota = sizeof(PCB) + largoTareas + cantTrip * sizeof(TCB);
				proceso1->idsTripExpulsados = list_create();
				pthread_mutex_init(&(proceso1->sem_paginas),0);
				pthread_mutex_init(&(proceso1->sem_espaciosLibres),0);
				pthread_mutex_init(&(proceso1->sem_idsTripExpulsados),0);

				tabla_direcciones *fragmentacionInterna = malloc(sizeof(tabla_direcciones));
				fragmentacionInterna->inicio = proceso1->sizePatota;
				fragmentacionInterna->size = list_size(proceso1->paginas) * tamanio_pagina - proceso1->sizePatota;
				list_add(proceso1->espaciosLibres,fragmentacionInterna);

				pthread_mutex_lock(&sem_listaProcesos);
				list_add(listaProcesos,proceso1);
				pthread_mutex_unlock(&sem_listaProcesos);

				return 1;
			}
			else
				return 0;
			break;
		default:
			return 0;
			break;
	}

}

int32_t validarPatotaConPaginacion(int32_t cantTrip, int32_t largoTareas){
	log_info(loggerRam,"Entro a validarPatotaConPag");
	int32_t sizePatota = cantTrip*sizeof(TCB) + largoTareas + sizeof(PCB);

	pthread_mutex_lock(&sem_framesMP);
	pthread_mutex_lock(&sem_paginasMV);
	int32_t sizeLibre = tamanio_pagina * (espacioLibreBitarray(framesMP,cantFramesMP)+espacioLibreBitarray(paginasMV,cantFramesMV));
	pthread_mutex_unlock(&sem_paginasMV);
	pthread_mutex_unlock(&sem_framesMP);

	if(sizePatota <= sizeLibre)
		return 1;
	return 0;
}

t_list *guardarPatotaConPaginacion(t_list *listaPaquete){

	char *archTareas = (char*)list_get(listaPaquete,1);
	PCB *pcbPatota = malloc(sizeof(PCB));
	int32_t cantTrip = *((int*)list_get(listaPaquete,0));
	int32_t largoTareas = strlen(archTareas) + 1;

	int32_t desplazamiento = 0;

	int32_t sizePatota = cantTrip*sizeof(TCB) + largoTareas + sizeof(PCB);
	void *datosPatota = malloc(sizePatota);

	log_info(loggerRam,"Archivo: %s",archTareas);
	log_info(loggerRam,"LargoArchivo: %d",largoTareas);

	log_info(loggerRam,"Guardado de Tareas");
	memcpy(datosPatota,archTareas,largoTareas);
	desplazamiento += largoTareas;

	log_info(loggerRam,"Guardado de PCB");
	pcbPatota->pid = *((int*)list_get(listaPaquete,2));
	pcbPatota->tareas = generarDireccionLogica(pcbPatota->pid,S_TAREAS,DESP_TAREAS,largoTareas);
	memcpy(datosPatota+desplazamiento,pcbPatota,sizeof(PCB));
	desplazamiento += sizeof(PCB);


	TCB *tcbTripulante = malloc(sizeof(TCB));
	for(int i=0; i<cantTrip; i++)
	{
		log_info(loggerRam,"Guardado de TCB %d",i);
		tcbTripulante->tid = *(int*)list_get(listaPaquete,3 + i*3);
		tcbTripulante->posX = *((int*)list_get(listaPaquete,4 + i*3));
		tcbTripulante->posY = *((int*)list_get(listaPaquete,5 + i*3));
		tcbTripulante->estado = 'N';
		tcbTripulante->puntero_PCB = generarDireccionLogica(pcbPatota->pid,S_PCB,DESP_PCB_PID,largoTareas);
		tcbTripulante->proximaInstruccion = 0;
		log_info(loggerRam,"antes del memcpy");
		memcpy(datosPatota+desplazamiento,tcbTripulante,sizeof(TCB));
		desplazamiento+=sizeof(TCB);
	}

	log_info(loggerRam,"COMIENZO A GUARDAR PAGINAS");

	int32_t contadorPaginas = 0;
	t_list *paginas = list_create();

	pthread_mutex_lock(&sem_memoriaReal);
	pthread_mutex_lock(&sem_framesMP);
	while(espacioLibreBitarray(framesMP,cantFramesMP) && sizePatota>0)
	{
		int32_t frameLibre = primerFrameLibre(framesMP,cantFramesMP);
		if(frameLibre == -1)
		{
			log_info(loggerRam,"No hay ningun frame libre en la memoria real"); //Nunca deberia llegar por la condicion del while
			break;
		}

		int32_t direccionMP = (frameLibre * tamanio_pagina);

		if(sizePatota >= tamanio_pagina)
		{
			memcpy(memoriaReal + direccionMP,datosPatota,tamanio_pagina);
			sizePatota -= tamanio_pagina;
		}
		else
		{
			//int32_t fragmentacionInterna = tamanio_pagina - sizePatota;
			memcpy(memoriaReal + direccionMP , datosPatota , sizePatota);
			sizePatota -= sizePatota;
		}

		bitarray_set_bit(framesMP,frameLibre);
		pthread_mutex_unlock(&sem_framesMP);
		pthread_mutex_unlock(&sem_memoriaReal);

		tabla_paginas *tablaPaginas = malloc(sizeof(tabla_paginas));
		tablaPaginas->nroPagina = contadorPaginas;
		tablaPaginas->nroFrame = frameLibre;
		tablaPaginas->bitPresencia = 1;
		tablaPaginas->ultimaReferencia = temporal_get_string_time("%y%m%d%H%M%S%MS");
		tablaPaginas->bitUso = 1;
		log_info(loggerRam,"GUARDE UNA PAGINA EN MP. contPag: %d",contadorPaginas);
		contadorPaginas++;
		list_add(paginas,tablaPaginas);
		datosPatota += tamanio_pagina;
		pthread_mutex_lock(&sem_memoriaReal);
		pthread_mutex_lock(&sem_framesMP);

	}
	pthread_mutex_unlock(&sem_framesMP);
	pthread_mutex_unlock(&sem_memoriaReal);



	if(sizePatota > 0) //SI NOS QUEDAMOS SIN PAGINAS LIBRES EN MP
	{
		log_info(loggerRam,"SI NO HAY MAS EN MP");
		pthread_mutex_lock(&sem_swapFile);
		pthread_mutex_lock(&sem_paginasMV);
		while(espacioLibreBitarray(paginasMV,cantFramesMV) && (sizePatota>0))
		{
			int32_t frameLibre = primerFrameLibre(paginasMV,cantFramesMV);

			if(frameLibre == -1)
			{
				log_info(loggerRam,"No hay ningun frame libre en la memoria virtual"); //Nunca deberia llegar por la condicion del while
				break;
			}

			int32_t direccionMV = (frameLibre * tamanio_pagina);

			if(sizePatota >= tamanio_pagina)
			{

				int swapF = open(pathSwapFile, O_RDWR , S_IRUSR | S_IWUSR);
				if(swapF != -1){
					void *swapFile = mmap(NULL, tamanio_swap, PROT_WRITE | PROT_READ, MAP_SHARED, swapF, 0);
					memcpy(swapFile + direccionMV,datosPatota,tamanio_pagina);
					msync(swapFile,tamanio_swap,MS_SYNC);
					munmap(swapFile, tamanio_swap);
					close(swapF);
				}
					else
						log_info(loggerRam, "Error al abrir el SwapFile");

				sizePatota -= tamanio_pagina;
			}
			else
			{
				//int32_t fragmentacionInterna = tamanio_pagina - sizePatota;

				int swapF = open(pathSwapFile, O_RDWR , S_IRUSR | S_IWUSR);
				if(swapF != -1){
					void *swapFile = mmap(NULL, tamanio_swap, PROT_WRITE | PROT_READ, MAP_SHARED, swapF, 0);
					memcpy(swapFile + direccionMV , datosPatota , sizePatota);
					msync(swapFile,tamanio_swap,MS_SYNC);
					munmap(swapFile, tamanio_swap);
					close(swapF);
				}
					else
						log_info(loggerRam, "Error al abrir el SwapFile");

				sizePatota -= sizePatota;
			}

			bitarray_set_bit(paginasMV,frameLibre);
			pthread_mutex_unlock(&sem_paginasMV);
			pthread_mutex_unlock(&sem_swapFile);
			tabla_paginas *tablaPaginas = malloc(sizeof(tabla_paginas));
			tablaPaginas->nroPagina = contadorPaginas;
			tablaPaginas->nroFrame = frameLibre;
			tablaPaginas->bitPresencia = 0;
			tablaPaginas->bitUso = 1;
			tablaPaginas->ultimaReferencia = temporal_get_string_time("%y%m%d%H%M%S%MS");
			contadorPaginas++;
			list_add(paginas,tablaPaginas);
			datosPatota += tamanio_pagina;
			pthread_mutex_lock(&sem_swapFile);
			pthread_mutex_lock(&sem_paginasMV);
		}
		pthread_mutex_unlock(&sem_paginasMV);
		pthread_mutex_unlock(&sem_swapFile);
	}


	log_info(loggerRam,"La cantidad de paginas del proceso %d es: %d",pcbPatota->pid,contadorPaginas);
	/*int32_t cantPaginasNecesarias = ((cantTrip*sizeof(TCB) + largoTareas + sizeof(PCB))/tamanio_pagina);
	log_info(loggerRam,"La cantidad de paginas necesarias: %d",cantPaginasNecesarias);*/
	free(tcbTripulante);
	free(pcbPatota);
	free(datosPatota - contadorPaginas*tamanio_pagina);

	return paginas;
}


int32_t validarPatotaConSegmentacion(int32_t cantTrip, int32_t largoTareas){
	log_info(loggerRam,"Validacion de Tareas:");
	if(validarSegmentacion(largoTareas)){
		log_info(loggerRam,"Validacion de PCB:");
		if(validarSegmentacion(sizeof(PCB))){
			for(int i=0; i<cantTrip ; i++){
				log_info(loggerRam,"Validacion de TCB:");
				if(!validarSegmentacion(sizeof(TCB)))
					return 0; //VALIDO MAL algun TCB
			}
			return 1; //VALIDO TOD0 BIEN
		}
	}
	return 0; //VALIDO MAL largoTareas o PCB
}

int32_t validarSegmentacion(int32_t size)
{
	bool hayEspacio(tabla_direcciones *tab){
		return tab->size >= size;
	}

	pthread_mutex_lock(&sem_listaHuecosAux);
	switch(cod_criterio)
	{
		case FIRST_FIT:
			list_sort(listaHuecosAux, (void*)compararInicio);
			break;
		case BEST_FIT:
			list_sort(listaHuecosAux, (void*)compararSize);
			break;
		default:
			log_info(loggerRam, "Error de criterio");
			break;
	}


	tabla_direcciones *hueco = (tabla_direcciones*)list_find(listaHuecosAux,(void*)hayEspacio);
	if(hueco != NULL)
	{
		log_info(loggerRam,"Hueco Seleccionado Para Validar:");
		imprimirTabla(hueco);

		hueco->inicio = hueco->inicio + size;
		hueco->size = hueco->size - size;
		pthread_mutex_unlock(&sem_listaHuecosAux);

	}
	else
	{
		log_info(loggerRam,"No hay espacio para guardar la patota, compacta amigo");
		pthread_mutex_unlock(&sem_listaHuecosAux);
		return 0;
	}
	return 1;
}

void crearListaHuecosAuxiliar(){

	void duplicarElemento(tabla_direcciones *hueco){
		int32_t id = hueco->id;
		int32_t inicio = hueco->inicio;
		int32_t size = hueco->size;
		agregarATabla(listaHuecosAux, id, inicio, size);
	}

	listaHuecosAux = list_create();

	pthread_mutex_lock(&sem_listaHuecos);
	pthread_mutex_lock(&sem_listaHuecosAux);
	list_iterate(listaHuecos, (void*) duplicarElemento);
	pthread_mutex_unlock(&sem_listaHuecosAux);
	pthread_mutex_unlock(&sem_listaHuecos);
}

void borrarListaHuecosAuxiliar(){
	pthread_mutex_lock(&sem_listaHuecosAux);
	list_destroy_and_destroy_elements(listaHuecosAux,(void*)free);
	pthread_mutex_unlock(&sem_listaHuecosAux);
}

t_list *guardarPatotaConSegmentacion(t_list* lista)
{
	char *archTareas = (char*)list_get(lista,1);
	PCB *pcbPatota = malloc(sizeof(PCB));
	int32_t cantTrip = *((int*)list_get(lista,0));
	t_list *segmentos = list_create();

	pthread_mutex_lock(&sem_memoriaReal);
	int32_t direccionFisicaTareas = guardarSegmento(archTareas, strlen(archTareas)+1);
	int32_t largoTareas = strlen(archTareas)+1;
	agregarATabla(segmentos,S_TAREAS,direccionFisicaTareas,largoTareas);
	pthread_mutex_unlock(&sem_memoriaReal);


	log_info(loggerRam,"Guardado de PCB:");
	pcbPatota->pid = *((int*)list_get(lista,2));
	pcbPatota->tareas = generarDireccionLogica(pcbPatota->pid,S_TAREAS,DESP_TAREAS,largoTareas);

	pthread_mutex_lock(&sem_memoriaReal);
	int32_t direccionFisicaPCB = guardarSegmento(pcbPatota, sizeof(PCB));
	agregarATabla(segmentos,S_PCB,direccionFisicaPCB,sizeof(PCB));
	pthread_mutex_unlock(&sem_memoriaReal);


	TCB *tcbTripulante = malloc(sizeof(TCB));
	int32_t direccionTCB;
	for(int i=0; i<cantTrip; i++)
	{
		log_info(loggerRam,"Guardado de TCB:");
		tcbTripulante->tid = *(int*)list_get(lista,3 + i*3);
		tcbTripulante->posX = *((int*)list_get(lista,4 + i*3));
		tcbTripulante->posY = *((int*)list_get(lista,5 + i*3));
		tcbTripulante->estado = 'N';
		tcbTripulante->puntero_PCB = generarDireccionLogica(pcbPatota->pid,S_PCB,DESP_PCB_PID,largoTareas);
		tcbTripulante->proximaInstruccion = 0;

		pthread_mutex_lock(&sem_memoriaReal);
		direccionTCB = guardarSegmento(tcbTripulante, sizeof(TCB));
		agregarATabla(segmentos,S_TCB+i,direccionTCB,sizeof(TCB));
		pthread_mutex_unlock(&sem_memoriaReal);
	}

	free(pcbPatota);
	free(tcbTripulante);
	return segmentos;
}


int32_t guardarSegmento(void* segmento, int32_t size)
{

	bool hayEspacio(tabla_direcciones *tab){
		return tab->size >= size;
	}

	bool estaVacio(tabla_direcciones *hueco){
		return (hueco->size==0);
	}

	pthread_mutex_lock(&sem_listaHuecos);
	switch(cod_criterio)
	{
		case FIRST_FIT:
			list_sort(listaHuecos, (void*)compararInicio);
			break;
		case BEST_FIT:
			list_sort(listaHuecos, (void*)compararSize);
			break;
		default:
			log_info(loggerRam, "Error de criterio");
			break;
	}

	tabla_direcciones *hueco = (tabla_direcciones*)list_find(listaHuecos,(void*)hayEspacio);
	log_info(loggerRam,"Hueco Seleccionado Para Guardar:");
	imprimirTabla(hueco);

	memcpy(memoriaReal+(hueco->inicio), segmento, size);


	int32_t direccionMemoria = hueco->inicio;

	hueco->inicio = hueco->inicio + size;
	hueco->size = hueco->size - size;
	pthread_mutex_unlock(&sem_listaHuecos);

	log_info(loggerRam,"Hueco Modificado Dsp Guardar:");
	imprimirTabla(hueco);

	if(hueco->size == 0)
	{
		log_info(loggerRam,"Hueco de size 0 sera eliminado");
		list_remove_and_destroy_by_condition(listaHuecos,(void*)estaVacio,(void*)free);
	}

	return direccionMemoria;
}
//----------------------------------------

//----------DESPLAZAR TRIPULANTE----------
void desplazarTripulanteRam(int sk_client)
{
	log_info(loggerRam, "Desplazar Tripulante:");

	t_list *listaPaquete = recibir_paquete_desplazarTripulante(sk_client);

	int32_t idPatota = *(int32_t*)list_get(listaPaquete,0);
	int32_t idTripulante = *(int32_t*)list_get(listaPaquete,1);
	int32_t posX = *(int32_t*)list_get(listaPaquete,2);
	int32_t posY = *(int32_t*)list_get(listaPaquete,3);

	proceso *proceso1 = buscarProceso(idPatota);

	if(proceso1 == NULL)
	{
		log_info(loggerRam,"La patota del tripulante a desplazar no existe");
		list_destroy_and_destroy_elements(listaPaquete,(void*)free);
		return;
	}

	int32_t cantTripTotales = (proceso1->sizePatota - proceso1->largoTareas - sizeof(PCB)) / sizeof(TCB);
	if(buscarTripExpulsado(idTripulante,proceso1) != NULL || idTripulante > cantTripTotales || idTripulante <= 0)
	{
		log_info(loggerRam,"Tripulante invalido a desplazar");
		list_destroy_and_destroy_elements(listaPaquete,(void*)free);
		return;
	}

	modificarDatoTCB(idPatota,idTripulante,DESP_TCB_POSX,&posX,sizeof(int32_t));
	modificarDatoTCB(idPatota,idTripulante,DESP_TCB_POSY,&posY,sizeof(int32_t));
	pthread_mutex_lock(&sem_idsDesplazar);
	idPatotaADesplazar = idPatota;
	idTripulanteADesplazar = idTripulante;
	sem_post(&semDesplazarTripMapa); //Signal para que el mapa actualice la posicion

	list_destroy_and_destroy_elements(listaPaquete,(void*)free);
}

t_list *recibir_paquete_desplazarTripulante(int socket_cliente)
{
	int size;
	int desplazamiento = 0;
	void * buffer;
	int tamanio;
	int32_t *idPatota;
	int32_t *idTrip;
	int32_t *posX;
	int32_t *posY;
	t_list *valores = list_create();

	buffer = recibir_buffer2(&size, socket_cliente);

	//ID de la patota
	memcpy(&tamanio, buffer + desplazamiento, sizeof(int32_t));
	desplazamiento+=sizeof(int32_t);
	idPatota = malloc(tamanio);
	memcpy(idPatota, buffer+desplazamiento, tamanio);
	desplazamiento+=tamanio;
	list_add(valores, idPatota);

	//ID de tripulante
	memcpy(&tamanio, buffer + desplazamiento, sizeof(int32_t));
	desplazamiento+=sizeof(int32_t);
	idTrip = malloc(tamanio);
	memcpy(idTrip, buffer+desplazamiento, tamanio);
	desplazamiento+=tamanio;
	list_add(valores, idTrip);

	//Posicion X
	memcpy(&tamanio, buffer + desplazamiento, sizeof(int32_t));
	desplazamiento+=sizeof(int32_t);
	posX = malloc(tamanio);
	memcpy(posX , buffer+desplazamiento, tamanio);
	desplazamiento+=tamanio;
	list_add(valores, posX);

	//Posicion Y
	memcpy(&tamanio, buffer + desplazamiento, sizeof(int32_t));
	desplazamiento+=sizeof(int32_t);
	posY = malloc(tamanio);
	memcpy(posY, buffer+desplazamiento, tamanio);
	desplazamiento+=tamanio;
	list_add(valores, posY);

	log_info(loggerRam, "Paquete de desplazar tripulante recibido");
	free(buffer);
	return valores;
}
//----------------------------------------





//----------CAMBIAR ESTADO----------
void cambiarEstadoTripulanteRam(int sk_client)
{
	log_info(loggerRam, "Cambiar estado de tripulante:");

	t_list *listaPaquete = recibir_paquete_cambiarEstadoTripulante(sk_client);

	int32_t *idPatota = (int32_t*)list_get(listaPaquete,0);
	int32_t *idTripulante = (int32_t*)list_get(listaPaquete,1);
	StatusRam *nuevoEstado= (StatusRam*)list_get(listaPaquete,2);

	proceso *proceso1 = buscarProceso(*idPatota);

	if(proceso1 == NULL)
	{
		log_info(loggerRam,"La patota del tripulante a cambiarEstado no existe");
		list_destroy_and_destroy_elements(listaPaquete,(void*)free);
		return;
	}

	int32_t cantTripTotales = (proceso1->sizePatota - proceso1->largoTareas - sizeof(PCB)) / sizeof(TCB);
	if(buscarTripExpulsado(*idTripulante,proceso1) != NULL || *idTripulante > cantTripTotales || *idTripulante <= 0)
	{
		log_info(loggerRam,"Tripulante invalido a cambiarEstado");
		list_destroy_and_destroy_elements(listaPaquete,(void*)free);
		return;
	}

	char elNuevoEstado = convertirEstado(*nuevoEstado);
	log_info(loggerRam,"ESTADO: pid %d, tid %d, Nuevo Estado %c",*idPatota, *idTripulante, elNuevoEstado);

	modificarDatoTCB(*idPatota,*idTripulante,DESP_TCB_ESTADO,&elNuevoEstado,sizeof(char));
	list_destroy_and_destroy_elements(listaPaquete,(void*)free);
}

char convertirEstado(StatusRam estado){
	switch(estado){
		case NEW_RAM:
			return 'N';
			break;
		case READY_RAM:
			return 'R';
			break;
		case BLOCKED_I_O_RAM:
		case BLOCKED_SABOTAGE_RAM:
			return 'B';
			break;
		case EXEC_RAM:
			return 'E';
			break;
		default: //exit y sin estado no nos importa (nunca deberia pasar)
			return '0';
			break;
	}
}

t_list* recibir_paquete_cambiarEstadoTripulante(int socket_cliente)
{
	int size;
		int desplazamiento = 0;
		void *buffer;
		int tamanio;
		int32_t *idPatota;
		int32_t *idTrip;
		StatusRam *nuevoEstado;
		t_list *valores = list_create();

		buffer = recibir_buffer2(&size, socket_cliente);

		//ID de la patota
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int32_t));
		desplazamiento+=sizeof(int32_t);
		idPatota = malloc(tamanio);
		memcpy(idPatota, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, idPatota);

		//ID de tripulante
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int32_t));
		desplazamiento+=sizeof(int32_t);
		idTrip = malloc(tamanio);
		memcpy(idTrip, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, idTrip);

		//Nuevo Estado
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int32_t));
		desplazamiento+=sizeof(int32_t);
		nuevoEstado = malloc(tamanio);
		memcpy(nuevoEstado , buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, nuevoEstado);

		log_info(loggerRam, "Paquete de cambiar estado del tripulante recibido");
		free(buffer);
		return valores;
}
//----------------------------------------





//----------SOLICITAR TAREA----------
void solicitarTareaRam(int sk_client)
{
	t_list *datos = recibir_paquete_solicitarTareaRam(sk_client);
	int32_t *idPatota = (int32_t*)list_get(datos,0);
	int32_t *idTripulante = (int32_t*)list_get(datos,1);

	proceso *proceso1 = buscarProceso(*idPatota);
	if(proceso1 == NULL)
	{
		log_info(loggerRam,"La patota del tripulante a solicitar tarea no existe");
		list_destroy_and_destroy_elements(datos,(void*)free);
		t_paquete *paqueteEnviarTarea = crearPaqueteEnviarTarea("Patota invalida para solicitar Tarea");
		enviar_paquete2(paqueteEnviarTarea, sk_client);
		eliminar_paquete(paqueteEnviarTarea);
		return;
	}

	int32_t cantTripTotales = (proceso1->sizePatota - proceso1->largoTareas - sizeof(PCB)) / sizeof(TCB);
	if(buscarTripExpulsado(*idTripulante,proceso1) != NULL || *idTripulante > cantTripTotales || *idTripulante <= 0)
	{
		log_info(loggerRam,"Tripulante invalido para solicitar tarea");
		list_destroy_and_destroy_elements(datos,(void*)free);
		t_paquete *paqueteEnviarTarea = crearPaqueteEnviarTarea("Tripulante invalido para solicitar Tarea");
		enviar_paquete2(paqueteEnviarTarea, sk_client);
		eliminar_paquete(paqueteEnviarTarea);
		return;
	}

	switch(cod_esquema)
	{
	case SEGMENTACION:
		solicitarTareaSegmentacion(idPatota,idTripulante,sk_client);
		break;

	case PAGINACION:
		solicitarTareaPaginacion(idPatota,idTripulante,sk_client);
		break;

	default:
		log_info(loggerRam,"Error inesperado en solicitarTareaRam");
	}

	list_destroy_and_destroy_elements(datos,(void*)free);
}

t_list *recibir_paquete_solicitarTareaRam(int socket_cliente)
{
	int size;
	int desplazamiento = 0;
	void *buffer;
	int tamanio;
	t_list *valores = list_create();
	int32_t *idTrip;
	int32_t *idPatota;

	buffer = recibir_buffer2(&size, socket_cliente);
	//ID de Patota
	memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	idPatota = malloc(tamanio);
	memcpy(idPatota, buffer+desplazamiento, tamanio);
	desplazamiento+=tamanio;
	list_add(valores,idPatota);

	//ID de tripulante
	memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	idTrip = malloc(tamanio);
	memcpy(idTrip, buffer+desplazamiento, tamanio);
	desplazamiento+=tamanio;
	list_add(valores,idTrip);
	free(buffer);

	return valores;
}

t_paquete *crearPaqueteEnviarTarea(char *tarea)
{
	t_paquete* paquete = crear_paquete();
	paquete->codigo_operacion=ENVIAR_TAREA;
	agregar_a_paquete(paquete, tarea, strlen(tarea)+1); // Agrego el id del Tripulante

	return paquete;
}

void solicitarTareaPaginacion(int32_t *idPatota , int32_t *idTripulante , int sk_client)
{
	log_info(loggerRam,"Entro a solicitar tarea con paginacion");
	proceso *proceso1 = buscarProceso(*idPatota);
	log_info(loggerRam,"Despues de buscar Proceso");
	int32_t largoArchivo = proceso1->largoTareas;
	int32_t direccionLogicaProximaInstruccion = generarDireccionLogica(*idPatota,S_TCB+*idTripulante-1,DESP_TCB_PROX_INSTR,largoArchivo);
	log_info(loggerRam,"Antes de buscar Dato");
	int32_t *numeroDeTareaParaTripulante = (int32_t*)buscarDato(direccionLogicaProximaInstruccion,sizeof(int32_t));
	log_info(loggerRam,"Despues de buscar Dato");
	int32_t direccionLogicaArchivoTareas = generarDireccionLogica(*idPatota,S_TAREAS,DESP_TAREAS,largoArchivo);
	char* archivoTareas = (char*)buscarDato(direccionLogicaArchivoTareas,largoArchivo);
	char **tareasSeparadas = string_split(archivoTareas,"\n");

	t_paquete *paqueteEnviarTarea;
	if(*numeroDeTareaParaTripulante < longitudArray(tareasSeparadas))
	{
		log_info(loggerRam,"SOLICITAR TAREA: %s",tareasSeparadas[*numeroDeTareaParaTripulante]);
		paqueteEnviarTarea = crearPaqueteEnviarTarea(tareasSeparadas[*numeroDeTareaParaTripulante]);
		enviar_paquete2(paqueteEnviarTarea, sk_client);
	}

	else
	{
		paqueteEnviarTarea = crearPaqueteEnviarTarea("No hay mas tareas");
		enviar_paquete2(paqueteEnviarTarea, sk_client);
	}
	eliminar_paquete(paqueteEnviarTarea);

	(*numeroDeTareaParaTripulante)++;

	modificarDatoTCB(*idPatota,*idTripulante,DESP_TCB_PROX_INSTR,(int32_t*)(numeroDeTareaParaTripulante),sizeof(int32_t));

	liberar_string_split(tareasSeparadas);
	free(numeroDeTareaParaTripulante);
	free(archivoTareas);
}

void solicitarTareaSegmentacion(int32_t *idPatota, int32_t *idTripulante, int sk_client)
{
	int32_t largoArchivo = buscarSegmentoDeUnProceso(*idPatota,S_TAREAS)->size;
	int32_t *numeroDeTareaParaTripulante = (int32_t*)buscarDato(generarDireccionLogica(*idPatota,S_TCB+(*idTripulante)-1,DESP_TCB_PROX_INSTR,largoArchivo),sizeof(int32_t));
	char* archivoTareas = (char*)buscarDato(generarDireccionLogica(*idPatota,S_TAREAS,DESP_TAREAS,largoArchivo),largoArchivo);
	char **tareasSeparadas = string_split(archivoTareas,"\n");

	t_paquete *paqueteEnviarTarea;
	if(*numeroDeTareaParaTripulante < longitudArray(tareasSeparadas))
	{
		paqueteEnviarTarea = crearPaqueteEnviarTarea(tareasSeparadas[*numeroDeTareaParaTripulante]);
		enviar_paquete2(paqueteEnviarTarea, sk_client);
	}
	else
	{
		paqueteEnviarTarea = crearPaqueteEnviarTarea("No hay mas tareas");
		enviar_paquete2(paqueteEnviarTarea, sk_client);
	}
	eliminar_paquete(paqueteEnviarTarea);
	liberar_string_split(tareasSeparadas);

	(*numeroDeTareaParaTripulante)++;
	modificarDatoTCB(*idPatota,*idTripulante,DESP_TCB_PROX_INSTR,(int32_t*)(numeroDeTareaParaTripulante),sizeof(int32_t));
	free(numeroDeTareaParaTripulante);
	free(archivoTareas);
}

int longitudArray (char** consolaSeparado)
{
	int i = 0;
	while(consolaSeparado[i] != NULL)
	{
		i++;
	}
	return i;
}

void liberar_string_split(char** lista)
{
	for(int i=0; lista[i] != NULL; i++)
	{
		free(lista[i]);
	}
	free(lista);
}
//-----------------------------------------




//----------EXPULSAR TRIPULANTE----------
void expulsarTripulanteRam(int sk_client){

	t_list *datos = recibir_paquete_expulsarTripulante(sk_client);

	int32_t *idPatota = list_get(datos,0);
	int32_t *idTripulante = list_get(datos,1);
	proceso *proceso1 = buscarProceso(*idPatota);

	if(proceso1 == NULL)
	{
		log_info(loggerRam,"La patota del tripulante a expulsar no existe");
		list_destroy_and_destroy_elements(datos,(void*)free);
		return;
	}

	int32_t cantTripTotales = (proceso1->sizePatota - proceso1->largoTareas - sizeof(PCB)) / sizeof(TCB);
	if(buscarTripExpulsado(*idTripulante,proceso1) != NULL || *idTripulante > cantTripTotales || *idTripulante <= 0)
	{
		log_info(loggerRam,"Tripulante invalido a expulsar");
		list_destroy_and_destroy_elements(datos,(void*)free);
		return;
	}

	log_info(loggerRam,"El idTripulante a expulsar es: %d de la Patota: %d",*idTripulante, *idPatota);

	pthread_mutex_lock(&sem_idsExpulsar);
	idPatotaAExpulsar = *idPatota;
	idTripulanteAExpulsar = *idTripulante;
	sem_post(&semExpulsarTripMapa);

	sem_wait(&semExpulsarTripMapa2);
	switch(cod_esquema){
	case SEGMENTACION:
		expulsarTripulanteConSegmentacion(*idPatota, *idTripulante);
		break;
	case PAGINACION:
		expulsarTripulanteConPaginacion(*idPatota, *idTripulante);
		break;
	default:
		break;
	}

	log_info(loggerRam,"El tripulante %d de la patota %d fue expulsado",*idTripulante, *idPatota);
	if(buscarProceso(*idPatota)!= NULL){
		int32_t *idTripulanteAux = malloc(sizeof(int32_t));
		*idTripulanteAux = *idTripulante;
		list_add(proceso1->idsTripExpulsados,idTripulanteAux);
	}
	else
		log_info(loggerRam,"La patota %d fue totalmente expulsada", *idPatota);


	list_destroy_and_destroy_elements(datos,(void*)free);
}

t_list *recibir_paquete_expulsarTripulante(int socket_cliente)
{
	int size;
	int desplazamiento = 0;
	void *buffer;
	int tamanio;
	t_list *valores = list_create();
	int32_t *idTrip;
	int32_t *idPatota;

	buffer = recibir_buffer2(&size, socket_cliente);
	//ID de Patota
	memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	idPatota = malloc(tamanio);
	memcpy(idPatota, buffer+desplazamiento, tamanio);
	desplazamiento+=tamanio;
	list_add(valores,idPatota);

	//ID de tripulante
	memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	idTrip = malloc(tamanio);
	memcpy(idTrip, buffer+desplazamiento, tamanio);
	desplazamiento+=tamanio;
	list_add(valores,idTrip);
	free(buffer);

	return valores;
}

void expulsarTripulanteConPaginacion(int32_t idPatota, int32_t idTripulante){

	proceso *proceso1 = buscarProceso(idPatota);


	int32_t direccionLogicaComienzoTCB = generarDireccionLogica(idPatota,S_TCB+idTripulante-1,DESP_TCB_TID,proceso1->largoTareas);
	int32_t nroPagina = (direccionLogicaComienzoTCB / 100) / tamanio_pagina;

	t_list *listaDeEspaciosLibres = proceso1->espaciosLibres; //ya protegido en revisarFrame todo

	bool buscarPorInicio(tabla_direcciones *espacio){
		return (espacio->inicio == direccionLogicaComienzoTCB/100) && (espacio->size == sizeof(TCB));
	}

	tabla_direcciones *espacioLibre = malloc(sizeof(tabla_direcciones));
	espacioLibre->id = 0;
	espacioLibre->inicio = direccionLogicaComienzoTCB/100;
	espacioLibre->size = sizeof(TCB);

	list_add(listaDeEspaciosLibres,espacioLibre);
	int32_t direccionLogicaDePagina = nroPagina * tamanio_pagina;

	log_info(loggerRam,"SE REVISA PAGINA %d", nroPagina);
	//Si el frame queda libre pone el 0 en el bitarray, sino lo ignora
	revisarFrameLibre(direccionLogicaDePagina, proceso1); //Revisa si queda libre la pagina del comienzo del TCB

	bool esNroPagina(tabla_paginas* pagina)
	{
		return pagina->nroPagina == nroPagina+1;
	}

	t_list *paginasProceso = proceso1->paginas;
	if(buscarPagina(paginasProceso,nroPagina+1)!=NULL){
		log_info(loggerRam,"SE REVISA PAGINA %d", nroPagina+1);
		revisarFrameLibre(direccionLogicaDePagina + tamanio_pagina, proceso1); //Revisa si queda libre la pagina siguiente en caso que el TCB ocupe otra pagina
	}
	proceso1->cantTrip--;
	if(!proceso1->cantTrip){
		expulsarPatotaConPaginacion(proceso1);
	}
}

void expulsarPatotaConPaginacion(proceso *proceso1){

	log_info(loggerRam,"Se Expulsara toda la patota");

	void liberarFrame(tabla_paginas *pagina){
		if(pagina->bitPresencia)
		{
			pthread_mutex_lock(&sem_framesMP);
			bitarray_clean_bit(framesMP,pagina->nroFrame);
			pthread_mutex_unlock(&sem_framesMP);
		}
		else
		{
			pthread_mutex_lock(&sem_paginasMV);
			bitarray_clean_bit(paginasMV,pagina->nroFrame);
			pthread_mutex_unlock(&sem_paginasMV);
		}
		free(pagina->ultimaReferencia);
	}

	log_info(loggerRam,"Antes de paginasProceso");
	t_list *paginasProceso = proceso1->paginas;
	log_info(loggerRam,"Antes de iterate");
	list_iterate(paginasProceso,(void*)liberarFrame);
	log_info(loggerRam,"Antes de destroy1");
	list_destroy_and_destroy_elements(proceso1->paginas,(void*)free);
	log_info(loggerRam,"Antes de destroy2");
	list_destroy_and_destroy_elements(proceso1->espaciosLibres,(void*)free);
	list_destroy_and_destroy_elements(proceso1->idsTripExpulsados,(void*)free);

	bool esIDPatota(proceso *proc)
	{
		return proc->id == proceso1->id;
	}
	pthread_mutex_lock(&sem_listaProcesos);
	list_remove_and_destroy_by_condition(listaProcesos,(void*)esIDPatota,(void*)free);
	pthread_mutex_unlock(&sem_listaProcesos);
}

void expulsarPatotaConSegmentacion(proceso *proceso1){
	log_info(loggerRam,"Se Expulsara toda la patota");
	tabla_direcciones *segmentoPCB = buscarSegmentoDeUnProceso(proceso1->id, S_PCB);

	pthread_mutex_lock(&sem_listaHuecos);
	agregarATabla(listaHuecos, contHuecos, segmentoPCB->inicio, segmentoPCB->size);
	pthread_mutex_unlock(&sem_listaHuecos);

	tabla_direcciones *segmentoTareas = buscarSegmentoDeUnProceso(proceso1->id, S_TAREAS);

	pthread_mutex_lock(&sem_listaHuecos);
	agregarATabla(listaHuecos, contHuecos, segmentoTareas->inicio, segmentoTareas->size);
	pthread_mutex_unlock(&sem_listaHuecos);

	list_destroy_and_destroy_elements(proceso1->segmentos,(void*)free);
	list_destroy_and_destroy_elements(proceso1->idsTripExpulsados,(void*)free);

	bool esIDPatota(proceso *proc)
	{
		return proc->id == proceso1->id;
	}
	pthread_mutex_lock(&sem_listaProcesos);
	list_remove_and_destroy_by_condition(listaProcesos,(void*)esIDPatota,(void*)free);
	pthread_mutex_unlock(&sem_listaProcesos);
}

void expulsarTripulanteConSegmentacion(int32_t idPatota, int32_t idTripulante)
{
	tabla_direcciones *segmentoTCB = buscarSegmentoDeUnProceso(idPatota,S_TCB+(idTripulante)-1);
	tabla_direcciones *hueco = malloc(sizeof(tabla_direcciones));
	hueco->inicio = segmentoTCB->inicio;
	hueco->size = segmentoTCB->size;
	hueco->id = contHuecos;
	pthread_mutex_lock(&sem_listaHuecos);
	list_add(listaHuecos,hueco);
	pthread_mutex_unlock(&sem_listaHuecos);



	bool esIDTripulante(tabla_direcciones *segmentoTCB){
		return segmentoTCB->id == S_TCB+(idTripulante)-1;
	}
	proceso *proceso1 = buscarProceso(idPatota);
	t_list *listaDeSegmentosDelProceso = proceso1->segmentos;
	list_remove_and_destroy_by_condition(listaDeSegmentosDelProceso,(void*)esIDTripulante,(void*)free);

	proceso1->cantTrip--;
	if(!proceso1->cantTrip){
		expulsarPatotaConSegmentacion(proceso1);
	}
}

void revisarFrameLibre(int32_t direccionLogicaDePagina, proceso *proceso1){
	int32_t direccionLogicaFinalDePagina = direccionLogicaDePagina + tamanio_pagina - 1;
	int32_t bytesLibre = 0;

	void sumarEspacioLibreDeUnaPagina(tabla_direcciones *espacioLibre2){
		int32_t inicioEspacio = espacioLibre2->inicio;
		int32_t finalEspacio = espacioLibre2->inicio + espacioLibre2->size - 1;

		switch(tipoDeEspacioIncluidoEnPagina(espacioLibre2, direccionLogicaDePagina)){
			case 1://empieza antes del inicio de pagina y termina dsp del inicio de pagina
				bytesLibre += (finalEspacio - direccionLogicaDePagina) + 1;
				break;
			case 2: //empieza dsp del inicio de pagina y termina antes del final de pagina
				bytesLibre += (espacioLibre2->size);
				break;
			case 3: //empieza antes del final de pagina y termina dsp del final de pagina
				bytesLibre += (direccionLogicaFinalDePagina - inicioEspacio) + 1;
				break;
			case 4: //empieza antes del inicio de pagina y termina dsp del final de pagina: sería un caso donde el espacio incluye toda la página y aún más (no deberia pasar nunca)
				bytesLibre += tamanio_pagina;
				break;
			default: //espacio no incluye a la pagina => no suma bytes
				break;
		}
		log_info(loggerRam,"bytes a liberar1: %d", bytesLibre);
	}

	int32_t nroPagina = direccionLogicaDePagina / tamanio_pagina;
	log_info(loggerRam,"REVISARFRAME(huecos) Antes de lock"); //todo check 2 revisarFrame al mismo tiempo?
	pthread_mutex_lock(&(proceso1->sem_espaciosLibres));
	log_info(loggerRam,"REVISARFRAME(huecos) Dsp de lock");
	t_list *listaDeEspaciosLibresDelProceso = proceso1->espaciosLibres;
	list_iterate(listaDeEspaciosLibresDelProceso,(void*)sumarEspacioLibreDeUnaPagina);
	log_info(loggerRam,"bytes totales a liberar de pag %d: %d",nroPagina, bytesLibre);
	log_info(loggerRam,"REVISARFRAME(huecos) Antes de unlock");
	pthread_mutex_unlock(&(proceso1->sem_espaciosLibres));
	log_info(loggerRam,"REVISARFRAME(huecos) Dsp de unlock");


	bool esNroPagina(tabla_paginas* pagina)
	{
		return pagina->nroPagina == nroPagina;
	}

	if(bytesLibre == tamanio_pagina){ //Queda libre todo el frame
		log_info(loggerRam,"REVISARFRAME Antes de lock");
		pthread_mutex_lock(&(proceso1->sem_paginas));
		log_info(loggerRam,"REVISARFRAME Dsp de lock");
		tabla_paginas *pagina = buscarPagina(proceso1->paginas,nroPagina);

		if(pagina->bitPresencia)
		{
			pthread_mutex_lock(&sem_framesMP);
			bitarray_clean_bit(framesMP,pagina->nroFrame);
			pthread_mutex_unlock(&sem_framesMP);
		}
		else
		{
			pthread_mutex_lock(&sem_paginasMV);
			bitarray_clean_bit(paginasMV,pagina->nroFrame);
			pthread_mutex_unlock(&sem_paginasMV);
		}

		free(pagina->ultimaReferencia);
		list_remove_and_destroy_by_condition(proceso1->paginas,(void*)esNroPagina,(void*)free);
		log_info(loggerRam,"REVISARFRAME Antes de unlock");
		pthread_mutex_unlock(&(proceso1->sem_paginas));
		log_info(loggerRam,"REVISARFRAME Dsp de unlock");
	}

}

int32_t tipoDeEspacioIncluidoEnPagina(tabla_direcciones *espacioLibre1, int32_t direccionLogicaDePagina){
	int32_t inicioEspacio = espacioLibre1->inicio;
	int32_t finalEspacio = espacioLibre1->inicio + espacioLibre1->size - 1;
	int32_t direccionLogicaFinalDePagina = direccionLogicaDePagina + tamanio_pagina - 1;

	if(inicioEspacio < direccionLogicaDePagina && (finalEspacio > direccionLogicaDePagina && finalEspacio <= direccionLogicaFinalDePagina))
		return 1; //empieza antes del inicio de pagina y termina dsp del inicio de pagina
	else if(inicioEspacio >= direccionLogicaDePagina && finalEspacio <= direccionLogicaFinalDePagina)
		return 2; //empieza dsp del inicio de pagina y termina antes del final de pagina
	else if((inicioEspacio >= direccionLogicaDePagina && inicioEspacio <= direccionLogicaFinalDePagina) && finalEspacio > direccionLogicaFinalDePagina)
		return 3; //empieza antes del final de pagina y termina dsp del final de pagina
	else if(inicioEspacio < direccionLogicaDePagina && finalEspacio > direccionLogicaFinalDePagina)
		return 4; //empieza antes del inicio de pagina y termina dsp del final de pagina: sería un caso donde el espacio incluye toda la página y aún más (no deberia pasar nunca)
	else
		return 5; //el espacio no ocupa nada de la página
}
//----------------------------------------





//----------FUNCIONES GENERALES----------
tabla_direcciones *buscarSegmentoDeUnProceso (int32_t idProceso, int32_t idSegmento)
{
	bool esIDTripulante(tabla_direcciones *segmentoTCB){
		return segmentoTCB->id == idSegmento;
	}

	proceso *proceso1 = (proceso*)buscarProceso(idProceso);
	if(proceso1 == NULL)
		return NULL;

	pthread_mutex_lock(&(proceso1->sem_segmentos));
	t_list *listaDeSegmentosDelProceso = proceso1->segmentos;
	tabla_direcciones *segmento = (tabla_direcciones *)list_find(listaDeSegmentosDelProceso,(void*)esIDTripulante);
	pthread_mutex_unlock(&(proceso1->sem_segmentos));

	log_info(loggerRam,"Segmento del proceso encontrado:");
	if(segmento!=NULL)
		imprimirTabla(segmento);
	return segmento;
}

t_list *obtenerListaDeTodosLosSegmentos()
{
	t_list *listaSegmentosTotales = list_create();
	void obtenerTodosLosSegmentos (proceso *proceso1)
	{
		void agregarSegmento(tabla_direcciones *segmento)
		{
			list_add(listaSegmentosTotales,segmento);
		}

		list_iterate(proceso1->segmentos, (void*)agregarSegmento);
	}
	pthread_mutex_lock(&sem_listaProcesos);
	list_iterate(listaProcesos,(void*)obtenerTodosLosSegmentos);
	pthread_mutex_unlock(&sem_listaProcesos);

	list_sort(listaSegmentosTotales,(void*)compararInicio);
	return listaSegmentosTotales;
}

proceso *buscarProcesoEnFrame(int32_t frame, int32_t *nroPagina, int32_t tipoDeMemoria){
	bool estaEnFrame(proceso *proceso1){
		int32_t nroPag = 0;
		t_list *paginasProceso = proceso1->paginas;
		while(nroPag < list_size(paginasProceso)){
			tabla_paginas *pagina = list_get(paginasProceso,nroPag);
			if((pagina->nroFrame == frame)&&(pagina->bitPresencia==tipoDeMemoria)){
				*nroPagina = pagina->nroPagina;
				return 1;
			}
			nroPag++;
		}
		return 0;
	}
	pthread_mutex_lock(&sem_listaProcesos);
	proceso *proceso1 = list_find(listaProcesos,(void*)estaEnFrame);
	pthread_mutex_unlock(&sem_listaProcesos);
	return proceso1;
}

int32_t primerFrameLibre(t_bitarray* bitarray, int32_t cantPaginas)
{
	int32_t cont=0;
	while(bitarray_test_bit(bitarray,cont) && cont < cantPaginas)
	{
		cont++;
	}
	if(cont == cantPaginas)
		return -1;
	else
		return cont;
}

proceso *buscarProceso(int32_t idPatota)
{
	bool esIDPatota(proceso *proceso1)
	{
		return proceso1->id == idPatota;
	}

	pthread_mutex_lock(&sem_listaProcesos);
	proceso *proceso2 = (proceso*)list_find(listaProcesos,(void*)esIDPatota);
	pthread_mutex_unlock(&sem_listaProcesos);

	return proceso2;
}


int32_t *buscarTripExpulsado(int32_t idTripulante, proceso *proceso1)
{
	bool buscarIDTripulante (int32_t *idTrip)
	{
		if(idTripulante == *idTrip)
			return true;
		return false;
	}
	pthread_mutex_lock(&(proceso1->sem_idsTripExpulsados));
	int32_t *idExpulsado = list_find(proceso1->idsTripExpulsados,(void*)buscarIDTripulante);
	pthread_mutex_unlock(&(proceso1->sem_idsTripExpulsados));

	return idExpulsado;
}

tabla_paginas *buscarPagina(t_list *listaDePaginas, int32_t nroPagina)
{
	bool esNroPagina(tabla_paginas* pagina)
	{
		return pagina->nroPagina == nroPagina;
	}

	return (tabla_paginas*)list_find(listaDePaginas,(void*)esNroPagina);
}

int32_t traducirLogicaAFisica (int32_t direccionLogica){
	switch(cod_esquema){
		case SEGMENTACION:
			return traducirLogicaAFisicaConSegmentacion(direccionLogica);
			break;
		case PAGINACION:
			return traducirLogicaAFisicaConPaginacion(direccionLogica);
			break;
		default:
			return 0;
			break;
	}
}

int32_t traducirLogicaAFisicaConSegmentacion (int32_t direccionLogica)
{
	int32_t idProceso = direccionLogica/10000;
	int32_t idSegmento = direccionLogica/100 - idProceso*100;
	int32_t desplazamiento = direccionLogica % 100;

	tabla_direcciones *segmento = buscarSegmentoDeUnProceso(idProceso,idSegmento);
	log_info(loggerRam,"Segmento del proceso al traducir la dirLogica:");
	imprimirTabla(segmento);

	int32_t inicioDelSegmento = segmento->inicio;
	int32_t direccionDelDato = inicioDelSegmento + desplazamiento;
	log_info(loggerRam,"DirFisica obtenida tras traduccion: %d",direccionDelDato);

	return direccionDelDato;
}

int32_t generarDireccionLogica(int32_t idProceso, int32_t seccion, int32_t desplazamiento, int32_t largoTareas)
{
	switch(cod_esquema)
	{
	case SEGMENTACION:
		return generarDireccionLogicaConSegmentacion(idProceso, seccion, desplazamiento);
		break;

	case PAGINACION:
		return generarDireccionLogicaConPaginacion(idProceso,seccion,desplazamiento,largoTareas);
		break;

	default:
		log_info(loggerRam,"No debiste haber llegado aqui(generarDireccionLogica)");
	}
	return -1;
}

int32_t generarDireccionLogicaConSegmentacion(int32_t idProceso, int32_t idSegmento, int32_t desplazamiento)
{
	return idProceso * 10000 + idSegmento * 100 + desplazamiento;
}

void *buscarDatoSegmentacion(int32_t direccionLogica,int32_t size)
{
	pthread_mutex_lock(&sem_memoriaReal);
	int32_t direccionDelDato = traducirLogicaAFisica(direccionLogica);
	void *dato = malloc(size);
	memcpy(dato,memoriaReal+direccionDelDato,size);
	pthread_mutex_unlock(&sem_memoriaReal);
	return dato;
}

void *buscarDato(int32_t direccionLogica, int32_t size)
{
	switch(cod_esquema)
	{
	case SEGMENTACION:
		return buscarDatoSegmentacion(direccionLogica,size);
		break;

	case PAGINACION:
		return buscarDatoPaginacion(direccionLogica,size);
		break;

	default:
		return NULL;
	}

}

void *buscarDatoPaginacion(int32_t direccionLogica, int32_t size)
{
	log_info(loggerRam,"Antes de buscarDatoPaginacion");
	void* dato = malloc(size);
	int32_t idPatota = direccionLogica % 100;
	int32_t nroPagina = (direccionLogica / 100) / tamanio_pagina;
	proceso *proceso1 = buscarProceso(idPatota);
	int32_t offset = (direccionLogica / 100) % tamanio_pagina;
	int32_t sizeRestante = size - (tamanio_pagina - offset);
	sizeRestante = (int32_t)fmax(0,sizeRestante);
	int32_t cantPaginas = ceil(((float)sizeRestante / (float)tamanio_pagina)) + 1;
	void *datoAuxiliar = malloc(cantPaginas*tamanio_pagina);
	int32_t desplazamiento = 0;

	for(int32_t cont=0;cont < cantPaginas; cont++){
		log_info(loggerRam,"BUSCARDATOPAG Antes de lock");
		pthread_mutex_lock(&(proceso1->sem_paginas));//todo si
		log_info(loggerRam,"BUSCARDATOPAG Dsp de lock");
		tabla_paginas *pagina = buscarPagina(proceso1->paginas,nroPagina);

		pthread_mutex_lock(&sem_memoriaReal);
		log_info(loggerRam,"Depsues de semMemoRealBuscarDatoPAginacion");
		int32_t frameSeleccionado = pagina->nroFrame;

		if(!pagina->bitPresencia) // Si la pagina no está en MP, primero la trae de la MV (Page Fault)
		{
			log_info(loggerRam, "PAGE FAULT");

			pthread_mutex_lock(&sem_framesMP);
			if((frameSeleccionado = primerFrameLibre(framesMP,cantFramesMP)) == -1){ //Si no hay frame libre devuelve -1 y se obtiene el frame con el algoritmo
				log_info(loggerRam, "ALGORITMOS DE REEMPLAZO");
				frameSeleccionado = obtenerFrameAReemplazar();
				log_info(loggerRam,"Frame Seleccionado tras algoritmo: %d",frameSeleccionado);
			}
			pthread_mutex_unlock(&sem_framesMP);

			direccionLogica = (nroPagina*tamanio_pagina)*100 + idPatota;

			traerAMemoria(frameSeleccionado,direccionLogica);
		}
		else
			log_info(loggerRam, "SIN PAGE FAULT");
		nroPagina++;

		memcpy(datoAuxiliar+desplazamiento,memoriaReal+frameSeleccionado*tamanio_pagina,tamanio_pagina);

		pthread_mutex_unlock(&sem_memoriaReal);


		desplazamiento+=tamanio_pagina;

		free(pagina->ultimaReferencia);
		pagina->ultimaReferencia = temporal_get_string_time("%y%m%d%H%M%S%MS");
		pagina->bitUso = 1;
		log_info(loggerRam,"BUSCARDATOPAG Antes de unlock");
		pthread_mutex_unlock(&(proceso1->sem_paginas)); //todo si
		log_info(loggerRam,"BUSCARDATOPAG Dsp de unlock");
	}
	memcpy(dato,datoAuxiliar+offset,size);
	free(datoAuxiliar);
	log_info(loggerRam,"Despues de buscarDatoPaginacion");
	return dato;
}

int32_t traducirLogicaAFisicaConPaginacion(int32_t direccionLogica)
{
	int32_t idPatota = direccionLogica % 100;
	int32_t nroPagina = (direccionLogica / 100) / tamanio_pagina;
	int32_t offset = (direccionLogica / 100) % tamanio_pagina;

	proceso *proceso1 = buscarProceso(idPatota);

	t_list* tablaPaginas = proceso1->paginas;
	tabla_paginas *pagina = buscarPagina(tablaPaginas,nroPagina);

	int32_t nroFrame = pagina->nroFrame;

	int32_t direccionFisica = nroFrame * tamanio_pagina + offset;

	log_info(loggerRam,"Fin de traducir");
	return direccionFisica;
}

int32_t generarDireccionLogicaConPaginacion(int32_t pid, int32_t tipoDato, int32_t offset,int32_t largoTareas)
{
	//log_info(loggerRam,"Generando Direccion Logica");
	int32_t desplazamiento;

	switch(tipoDato)
	{
		case S_TAREAS:
			desplazamiento = 0;
			break;
		case S_PCB:
			desplazamiento = largoTareas;
			break;
		default:
			desplazamiento = largoTareas + sizeof(PCB) + (tipoDato-2)*sizeof(TCB);
			break;
	}
	desplazamiento += offset;
	int32_t nroPagina = desplazamiento / tamanio_pagina;
	int32_t offsetDePagina = desplazamiento % tamanio_pagina;

	int32_t direccionLogica = (nroPagina * tamanio_pagina + offsetDePagina) * 100 + pid;

	return direccionLogica;
}

int32_t espacioLibreBitarray(t_bitarray *bitarray, int32_t size){
	int32_t cont=0;
	for(int i=0; i<size; i++){
		if(!bitarray_test_bit(bitarray,i))
			cont++;
	}
	return cont;
}

bool compararInicio(tabla_direcciones *tab1, tabla_direcciones *tab2){
	if(tab1->inicio < tab2->inicio)
		return true;
	else if(tab1->inicio == tab2->inicio)
		log_info(loggerRam, "Error superInesperado");
	return false;
}



bool compararSize(tabla_direcciones *tab1, tabla_direcciones *tab2){
	if(tab1->size < tab2->size)
		return true;
	return false;
}

void agregarATabla(t_list *lista, int32_t id, int32_t inicio, int32_t size){

	tabla_direcciones *elemento = malloc(sizeof(tabla_direcciones));

	elemento->id = id;
	elemento->inicio = inicio;
	elemento->size = size;

	list_add(lista, elemento);
	//free(elemento); liberar dsp
}

void modificarDatoTCB(int32_t idPatota, int32_t idTripulante, int32_t desplazamiento, void* nuevoDato, int32_t size)
{
	int32_t direccionLogicaDelDato,direccionFisicaDelDato; //TODO SI NO ENCUENTRA PROCESO ROMPE
	proceso *proceso1 = buscarProceso(idPatota);
	switch(cod_esquema)
	{
		case SEGMENTACION:
			direccionLogicaDelDato = generarDireccionLogica(idPatota,S_TCB+idTripulante-1,desplazamiento,proceso1->largoTareas);

			pthread_mutex_lock(&sem_memoriaReal);
			direccionFisicaDelDato = traducirLogicaAFisica(direccionLogicaDelDato);
			memcpy(memoriaReal+direccionFisicaDelDato,nuevoDato,size);
			pthread_mutex_unlock(&sem_memoriaReal);

			break;




		case PAGINACION:
			direccionLogicaDelDato = generarDireccionLogica(idPatota,S_TCB+idTripulante-1,desplazamiento,proceso1->largoTareas);
			log_info(loggerRam,"El dato es: Patota %d Tripulante %d",idPatota,idTripulante);
			void* dato = buscarDato(direccionLogicaDelDato,size); //Descartamos el dato pero al buscarlo lo trae a MP
			free(dato);

			log_info(loggerRam,"MODIFICARDATOTCB Antes de lock");
			pthread_mutex_lock(&(proceso1->sem_paginas)); //todo si
			log_info(loggerRam,"MODIFICARDATOTCB Dsp de lock");

			int32_t paginaComienzoDelDato = (direccionLogicaDelDato / 100) / tamanio_pagina;
			log_info(loggerRam,"paginaComienzoDelDato: %d",paginaComienzoDelDato);
			int32_t paginaFinDelDato = ((direccionLogicaDelDato/100) + size -1 ) / tamanio_pagina;
			//Le resto 1 porque si el dato entra justo, la direccion queda en la pagina siguiente
			log_info(loggerRam,"paginaFinDelDato: %d",paginaFinDelDato);
			int32_t cantPaginasQueOcupa = paginaFinDelDato - paginaComienzoDelDato + 1;

			pthread_mutex_lock(&sem_memoriaReal);
			direccionFisicaDelDato = traducirLogicaAFisica(direccionLogicaDelDato);

			if(cantPaginasQueOcupa == 1)
			{
				memcpy(memoriaReal+direccionFisicaDelDato,nuevoDato,size);
			}
			else
			{
				int32_t direccionSiguientePagina = (paginaComienzoDelDato+1) * tamanio_pagina;
				int32_t bytesAGuardar = direccionSiguientePagina - (direccionLogicaDelDato / 100);
				log_info(loggerRam,"direccionSiguientePagina: %d",direccionSiguientePagina);
				log_info(loggerRam,"direccionLogicaDelDato / 100: %d",direccionLogicaDelDato / 100);
				log_info(loggerRam,"bytesAGuardar: %d",bytesAGuardar);
				memcpy(memoriaReal+direccionFisicaDelDato,nuevoDato,bytesAGuardar);

				int32_t frameDeLaSiguientePagina = buscarPagina(proceso1->paginas,paginaComienzoDelDato+1)->nroFrame;
				int32_t direccionFisicaSiguienteFrame = frameDeLaSiguientePagina * tamanio_pagina;
				memcpy(memoriaReal+direccionFisicaSiguienteFrame,nuevoDato+bytesAGuardar,size- bytesAGuardar);
			}
			log_info(loggerRam,"MODIFICARDATOTCB Antes de unlock");
			pthread_mutex_unlock(&(proceso1->sem_paginas)); //todo si
			log_info(loggerRam,"MODIFICARDATOTCB Dsp de unlock");

			pthread_mutex_unlock(&sem_memoriaReal);
			break;

		default:
			log_info(loggerRam,"Nunca debiste llegar aqui (modificarDatoTCB)");
			break;
	}
}
//----------------------------------------





//----------ALGORITMOS DE REEMPLAZO----------
int32_t algoritmoLRU(){
	uintmax_t menorReferencia, referenciaNueva;
	int32_t frameSeleccionado;
	int32_t nroPagina;
	proceso *proceso1;
	tabla_paginas *pagina1;

	for(int i=0; i<cantFramesMP; i++){
		proceso1 = buscarProcesoEnFrame(i,&nroPagina,1);
		if(proceso1!=NULL){ //Nunca debería haber frame libre si llegó a esta funcion
			pagina1 = buscarPagina(proceso1->paginas,nroPagina);
			if(!i){
				menorReferencia = strtoumax(pagina1->ultimaReferencia,NULL,10);
				frameSeleccionado = i;
			}
			else{
				referenciaNueva = strtoumax(pagina1->ultimaReferencia,NULL,10);
				if(menorReferencia > referenciaNueva){
					frameSeleccionado = i;
					menorReferencia = referenciaNueva;
				}
			}
		}
	}
	log_info(loggerRam,"Funcion algoritmoLRU:");
	log_info(loggerRam,"menorReferencia: %lld",menorReferencia);
	log_info(loggerRam,"frame seleccionado: %d",frameSeleccionado);
	return frameSeleccionado;
}

int32_t algoritmoClock()
{
	log_info(loggerRam,"Entro a algoritmo Clock");
	int32_t frameSeleccionado;
	proceso *proceso1;
	tabla_paginas *pagina1;
	int32_t nroPagina;
	pthread_mutex_lock(&sem_ptrClock);
	if(punteroClock == cantFramesMP) //Reseteo el ptr xq la última vez quedó apuntando al último frame
		punteroClock=0;

	for(; punteroClock< cantFramesMP; punteroClock++)
	{
		proceso1 = buscarProcesoEnFrame(punteroClock,&nroPagina,1);
		if(proceso1!=NULL)//Nunca debería haber frame libre si llegó a esta funcion
		{
			pagina1 = buscarPagina(proceso1->paginas,nroPagina);
			if(pagina1->bitUso)
			{
				pagina1->bitUso = 0;
			}
			else
			{
				frameSeleccionado = punteroClock;
				punteroClock++;
				pthread_mutex_unlock(&sem_ptrClock);
				return frameSeleccionado;
			}

			//log_info(loggerRam,"Imprimo Memoria durante algoritmo reemplazo:");
			//imprimirMemoria();
		}

		if(punteroClock == cantFramesMP-1){ //Reseteo el ptr xq la última vez quedó apuntando al último frame
			punteroClock=-1;
			log_info(loggerRam,"LLEGUE AL FINAL DE LA MEMORIA (Alg Clk)");
		}
	}
	pthread_mutex_unlock(&sem_ptrClock);
	return -1;
}

int32_t obtenerFrameAReemplazar(){
	switch(cod_algoritmoReemplazo){
		case LRU:
			log_info(loggerRam, "Algoritmo LRU");
			return algoritmoLRU();
			break;
		case CLOCK:
			log_info(loggerRam, "Algoritmo CLOCK");
			return algoritmoClock();
			break;
		default:
			break;
	}
	return 0;
}

void traerAMemoria(int32_t frameSeleccionado, int32_t direccionLogica){

	int32_t idPatota = direccionLogica % 100;
	int32_t nroPagina = (direccionLogica / 100) / tamanio_pagina;
	proceso *proceso1 = buscarProceso(idPatota);

	int32_t offset = (direccionLogica / 100) % tamanio_pagina;

	//Obtenemos los lugares de la MP y MV para los reemplazos
	int32_t dirLogicaPagina = direccionLogica - offset * 100;
	pthread_mutex_lock(&sem_swapFile);

	int32_t dirFisicaMV = traducirLogicaAFisica(dirLogicaPagina);
	int32_t dirFisicaMP = frameSeleccionado * tamanio_pagina;



	void *paginaAux=malloc(tamanio_pagina);

	int swapF = open(pathSwapFile, O_RDWR , S_IRUSR | S_IWUSR);
	if(swapF != -1){
		void *swapFile = mmap(NULL, tamanio_swap, PROT_WRITE | PROT_READ, MAP_SHARED, swapF, 0);
		memcpy(paginaAux,memoriaReal+dirFisicaMP,tamanio_pagina);
		memcpy(memoriaReal+dirFisicaMP,swapFile+dirFisicaMV,tamanio_pagina);
		memcpy(swapFile+dirFisicaMV,paginaAux,tamanio_pagina);
		msync(swapFile,tamanio_swap,MS_SYNC);
		munmap(swapFile, tamanio_swap);
		close(swapF);
	}
	else
		log_info(loggerRam, "Error al abrir el SwapFile");

	pthread_mutex_unlock(&sem_swapFile);
	free(paginaAux);


	int32_t nroPaginaAReemplazar;
	proceso *procesoDeLaMP = buscarProcesoEnFrame(frameSeleccionado,&nroPaginaAReemplazar,1);

	tabla_paginas *paginaAReemplazar = buscarPagina(procesoDeLaMP->paginas,nroPaginaAReemplazar);

	//Actualizacion de las Tablas

	tabla_paginas *pagina = buscarPagina(proceso1->paginas,nroPagina);
	log_info(loggerRam,"Frame MP a reemplazar: %d",frameSeleccionado);
	log_info(loggerRam,"Frame MV a reemplazar: %d",pagina->nroFrame);
	paginaAReemplazar->nroFrame = pagina->nroFrame;
	paginaAReemplazar->bitPresencia = 0;
	pagina->nroFrame = frameSeleccionado;
	pagina->bitPresencia=1;

	log_info(loggerRam,"Imprimo Memoria dsp de traer a MP:");
	imprimirMemoria();
}
//----------------------------------------





//----------MAPA----------
int32_t hiloActualizarMapa()
{
	itemsID = dictionary_create();
	idMapa = malloc(sizeof(char));
	*idMapa = 65;

	//get_term_size();
	//set_term_size(miRows,miCols,miXPxl,miYPxl);

	nivel_gui_inicializar();

	nivel = nivel_crear("Mapa de la nave");

	nivel_gui_dibujar(nivel);

	pthread_t tid[3];
	pthread_create(&tid[0],NULL,(void*) iniciarPatotaMapa, NULL);
	pthread_create(&tid[1],NULL,(void*) moverTripMapa,NULL);
	pthread_create(&tid[2],NULL,(void*) expulsarTripMapa,NULL);

	pthread_join(tid[0],NULL);
	pthread_join(tid[1],NULL);
	pthread_join(tid[2],NULL);
	return EXIT_SUCCESS;
}

int iniciarPatotaMapa()
{
	int err;
	char *idLogico;
	int32_t *posX;
	int32_t *posY;
	proceso *proceso1;
	int32_t idProceso;
	int32_t largoTareas;
	int32_t cantTrip;
	int32_t direccionLogicaPosX;
	int32_t direccionLogicaPosY;


	while(1)
	{
		//SEMAFORO AL INICIAR PATOTA
		sem_wait(&semIniciarPatotaMapa);
		log_info(loggerRam,"Entro a iniciarPatotaMapa");

		proceso1 = buscarProceso(idPatotaAIniciar);
		//proceso1 = (proceso*)list_get(listaProcesos,list_size(listaProcesos)-1);
		pthread_mutex_unlock(&sem_idsIniciar);
		idProceso = proceso1->id;
		largoTareas = proceso1->largoTareas;
		cantTrip = proceso1->cantTrip;

		for(int i=0; i<cantTrip ; i++)
		{
			direccionLogicaPosX = generarDireccionLogica(idProceso,S_TCB+i,DESP_TCB_POSX,largoTareas);
			direccionLogicaPosY = generarDireccionLogica(idProceso,S_TCB+i,DESP_TCB_POSY,largoTareas);

			posX = (int32_t*)buscarDato(direccionLogicaPosX,sizeof(int32_t));
			posY = (int32_t*)buscarDato(direccionLogicaPosY,sizeof(int32_t));

			err = personaje_crear(nivel, *idMapa, *posX, *posY);
			ASSERT_CREATE(nivel, *idMapa, err);

			idLogico = string_itoa(idProceso*1000 + i+1);
			dictionary_put(itemsID, idLogico, idMapa);

			idMapa = malloc(sizeof(char));
			*idMapa= *(char*)dictionary_get(itemsID,idLogico) + 1;

			free(posX);
			free(posY);
			free(idLogico); //Deberia hacerlo al eliminar el elemento del diccionario
		}

		nivel_gui_dibujar(nivel);
	}
}

void moverTripMapa()
{
	int32_t *posX;
	int32_t *posY;
	int32_t direccionLogicaPosX;
	int32_t direccionLogicaPosY;
	proceso *proceso2;
	int32_t largoTareas;
	char idADesplazar;
	int32_t idPatota, idTripulante;

	while(1)
	{
		//SEMAFORO DE DESPLAZAR TRIPULANTE
		sem_wait(&semDesplazarTripMapa);
		idPatota = idPatotaADesplazar;
		idTripulante = idTripulanteADesplazar;
		pthread_mutex_unlock(&sem_idsDesplazar);

		proceso2 = buscarProceso(idPatota);
		largoTareas = proceso2->largoTareas;

		//MUEVO LA POSICION DEL TRIPULANTE Y DIBUJO NUEVAMENTE EL MAPA
		direccionLogicaPosX = generarDireccionLogica(idPatota,S_TCB+idTripulante-1,DESP_TCB_POSX,largoTareas);
		direccionLogicaPosY = generarDireccionLogica(idPatota,S_TCB+idTripulante-1,DESP_TCB_POSY,largoTareas);

		posX = (int32_t*)buscarDato(direccionLogicaPosX,sizeof(int32_t));
		posY = (int32_t*)buscarDato(direccionLogicaPosY,sizeof(int32_t));

		char* idLogico2 = string_itoa(idPatota*1000+idTripulante);

		log_info(loggerRam,"El idLogico2 es: %s",idLogico2);
		idADesplazar = *(char*)dictionary_get(itemsID,idLogico2);

		log_info(loggerRam,"El id a desplazar es: %c",idADesplazar);
		item_mover(nivel,idADesplazar,*posX,*posY);

		nivel_gui_dibujar(nivel);

		free(idLogico2);
		free(posX);
		free(posY);
	}
}

void expulsarTripMapa()
{
	char idAExpulsar;

	while(1)
	{
		//SEMAFORO DE DESPLAZAR TRIPULANTE
		sem_wait(&semExpulsarTripMapa);

		char* idLogico2 = string_itoa(idPatotaAExpulsar*1000+idTripulanteAExpulsar);
		pthread_mutex_unlock(&sem_idsExpulsar);
		sem_post(&semExpulsarTripMapa2);

		log_info(loggerRam,"El idLogico2 es: %s",idLogico2);
		idAExpulsar = *(char*)dictionary_get(itemsID,idLogico2);

		log_info(loggerRam,"El id a expulsar es: %c",idAExpulsar);
		item_borrar(nivel,idAExpulsar);

		nivel_gui_dibujar(nivel);
		log_info(loggerRam,"Tripulante %d(%c) eliminado del mapa.",atoi(idLogico2)%1000,idAExpulsar);
		free(idLogico2);
	}
}
//----------------------------------------





//----------IMPRIMIR MEMORIA----------
void imprimirMemoria(){

	pthread_mutex_lock(&sem_imprimir);

	switch(cod_esquema){
		case SEGMENTACION:
			imprimirMemoriaConSegmentacion();
			break;
		case PAGINACION:
			imprimirMemoriaConPaginacion();
			break;
		default:
			break;
	}

	pthread_mutex_unlock(&sem_imprimir);
}

void imprimirMemoriaConPaginacion(){
	log_info(loggerRam,F_NEGRITA "\t-- MEMORIA REAL --");

	for(int32_t frame=0; frame<cantFramesMP; frame++){
		if(bitarray_test_bit(framesMP,frame)){
			int32_t nroPagina;
			proceso *proceso1 = buscarProcesoEnFrame(frame,&nroPagina,1); //Siempre deberia devolver un proceso xq el bitarray en 1
			tabla_paginas *pagina1 = buscarPagina(proceso1->paginas,nroPagina);
			log_info(loggerRam,F_NEGRITA F_ROJO "--------------------------" F_RESET);
			log_info(loggerRam,F_NEGRITA F_ROJO "| PROCESO %2d - PAGINA %2d |" F_RESET,proceso1->id, nroPagina);
			log_info(loggerRam,F_NEGRITA F_ROJO "| ultRef: %lld|" F_RESET, strtoumax(pagina1->ultimaReferencia,NULL,10));
			log_info(loggerRam,F_NEGRITA F_ROJO "|      Bit de uso: %d     |" F_RESET,pagina1->bitUso);
			log_info(loggerRam,F_NEGRITA F_ROJO "--------------------------" F_RESET);
		}
		else{
			log_info(loggerRam,F_NEGRITA F_VERDE "--------------------------" F_RESET);
			log_info(loggerRam,F_NEGRITA F_VERDE "|     FRAME %3d LIBRE    |" F_RESET,frame);
			log_info(loggerRam,F_NEGRITA F_VERDE "--------------------------" F_RESET);
		}
	}
	log_info(loggerRam,F_NEGRITA "\t-- MEMORIA VIRTUAL --");

	for(int32_t frame=0; frame<cantFramesMV; frame++){
		if(bitarray_test_bit(paginasMV,frame)){
			int32_t nroPagina;
			proceso *proceso1 = buscarProcesoEnFrame(frame,&nroPagina,0); //Siempre deberia devolver un proceso xq el bitarray en 1
			tabla_paginas *pagina1 = buscarPagina(proceso1->paginas,nroPagina);
			log_info(loggerRam,F_NEGRITA F_ROJO "--------------------------" F_RESET);
			log_info(loggerRam,F_NEGRITA F_ROJO "| PROCESO %2d - PAGINA %2d |" F_RESET,proceso1->id, nroPagina);
			log_info(loggerRam,F_NEGRITA F_ROJO "| ultRef: %lld|" F_RESET, strtoumax(pagina1->ultimaReferencia,NULL,10));
			log_info(loggerRam,F_NEGRITA F_ROJO "|      Bit de uso: %d     |" F_RESET,pagina1->bitUso);
			log_info(loggerRam,F_NEGRITA F_ROJO "--------------------------" F_RESET);
		}
		else{
			log_info(loggerRam,F_NEGRITA F_AMARILLO "--------------------------" F_RESET);
			log_info(loggerRam,F_NEGRITA F_AMARILLO "|     FRAME %3d LIBRE    |" F_RESET,frame);
			log_info(loggerRam,F_NEGRITA F_AMARILLO "--------------------------" F_RESET);
		}
	}

}

void imprimirMemoriaConSegmentacion()
{
	pthread_mutex_lock(&sem_listaHuecos);
	t_list *listaSegmentosTotales = obtenerListaDeTodosLosSegmentos();
	list_sort(listaHuecos,(void*)compararInicio);

	int32_t contImprimirHuecos = 0;
	int32_t contImprimirSegmentos = 0;

	while(contImprimirHuecos < list_size(listaHuecos) && contImprimirSegmentos < list_size(listaSegmentosTotales)){
		tabla_direcciones *hueco = (tabla_direcciones*)list_get(listaHuecos,contImprimirHuecos);
		tabla_direcciones *segmento = (tabla_direcciones*)list_get(listaSegmentosTotales,contImprimirSegmentos);

		if(hueco->inicio < segmento->inicio){
			imprimirTabla(hueco);
			contImprimirHuecos++;
		}
		else{
			imprimirTabla(segmento);
			contImprimirSegmentos++;
		}
	}

	if(contImprimirHuecos < list_size(listaHuecos)){
		while(contImprimirHuecos < list_size(listaHuecos)){
			tabla_direcciones *hueco = (tabla_direcciones*)list_get(listaHuecos,contImprimirHuecos);
			imprimirTabla(hueco);
			contImprimirHuecos++;
		}
	}
	else{
		while(contImprimirSegmentos < list_size(listaSegmentosTotales)){
			tabla_direcciones *segmento = (tabla_direcciones*)list_get(listaSegmentosTotales,contImprimirSegmentos);
			imprimirTabla(segmento);
			contImprimirSegmentos++;
		}
	}
	list_destroy(listaSegmentosTotales);
	pthread_mutex_unlock(&sem_listaHuecos);
}

void imprimirTabla(tabla_direcciones *tabla)
{
	char segmento[3];

	switch(tabla->id){
		case -1: // O REALIZAR FUNCION SEPARADA PARA HUECOS
			strcpy(segmento,"HUE");
			break;
		case S_PCB:
			strcpy(segmento,"PCB");
			break;
		case S_TAREAS:
			strcpy(segmento,"TAR");
			break;
		default:
			strcpy(segmento,"TCB");
			break;
	}

	log_info(loggerRam,  "%5d------------",tabla->inicio);
	log_info(loggerRam, "     |          |");
	log_info(loggerRam, "     |    %s   |",segmento);
	log_info(loggerRam, "     |          |");
	log_info(loggerRam,  "%5d------------",tabla->size);
}

void imprimirListaTabla(t_list *lista){
	log_info(loggerRam,"Tablas de la lista:\n");
	list_sort(lista,(void*)compararInicio);
	list_iterate(lista,(void*)imprimirTabla);
	//printf("\n");
}

void imprimirprocesox(proceso* proceso1)
{
	log_info(loggerRam,"IMPRESION DE UN PROCESO");
	log_info(loggerRam,"proceso1->id: %d",proceso1->id);
	log_info(loggerRam,"proceso1->cantTrip: %d",proceso1->cantTrip);
	log_info(loggerRam,"proceso1->largoTareas: %d",proceso1->largoTareas);
}
//----------------------------------------





//----------COMPACTAR LA MEMORIA----------
void hiloCompactar(){
	while(1){
		sem_wait(&sem_compactar);
		compactarMemoria();
	}
}

void signalCompactar(){
	log_info(loggerRam,"SIGNAL COMPACTAR");
	sem_post(&sem_compactar);
}

void compactarMemoria(){
	pthread_mutex_lock(&sem_memoriaReal);
	log_info(loggerRam,"Se Compactara la memoria");
	t_list *listaSegmentosTotales = obtenerListaDeTodosLosSegmentos();

	//Realocamos los segmentos
	int32_t inicio = 0;
	int32_t contImprimirSegmentos = 0;
	tabla_direcciones *segmento1;
	while(contImprimirSegmentos < list_size(listaSegmentosTotales))
	{
		segmento1 = ((tabla_direcciones*)list_get(listaSegmentosTotales,contImprimirSegmentos));
		if(inicio != segmento1->inicio)
		{
			void *segmento = malloc(segmento1->size);
			memcpy(segmento,memoriaReal+segmento1->inicio,segmento1->size);
			segmento1->inicio = inicio;
			memcpy(memoriaReal+inicio,segmento,segmento1->size);
			free(segmento);
		}
		inicio += segmento1->size;
		contImprimirSegmentos++;
	}
	list_destroy(listaSegmentosTotales);

	//Creamos el nuevo unico hueco libre
	int32_t memoriaLibre = 0;
	void agregarAMemoriaLibre(tabla_direcciones *hueco)
	{
		memoriaLibre += hueco->size;
	}

	pthread_mutex_lock(&sem_listaHuecos);
	list_sort(listaHuecos,(void*)compararInicio);
	list_iterate(listaHuecos,(void*)agregarAMemoriaLibre);
	list_clean_and_destroy_elements(listaHuecos,(void*)free);
	if(memoriaLibre > 0)
		agregarATabla(listaHuecos,contHuecos,inicio,memoriaLibre);
	pthread_mutex_unlock(&sem_listaHuecos);

	pthread_mutex_unlock(&sem_memoriaReal);
	imprimirMemoria();
}
//----------------------------------------





//----------DUMP DE LA MEMORIA----------
void hiloDumpMemoria()
{
	log_info(loggerRam,"Ingresaste al hilo dumpMemoria");
	pthread_mutex_lock(&sem_dump); //El Mutex empieza en 1
	while(1){
		pthread_mutex_lock(&sem_dump);
		realizarDumpMemoria();
	}
}

void signalDump(){
	log_info(loggerRam,"SIGNAL DUMP");
	pthread_mutex_unlock(&sem_dump);
}

void realizarDumpMemoria(){
	log_info(loggerRam, "Realizo Dump de Memoria");
	char* nombreArch = temporal_get_string_time("Dump_%y%m%d_%H%M%S");
	FILE *archDump = fopen(nombreArch,"a");
	char *fecha = temporal_get_string_time("FECHA: %d/%m/%y  -- --  HORA: %H:%M:%S:%MS\n\n");
	fprintf(archDump,"%s",fecha);

	void dumpSegmentacion(proceso *unProceso){
		int32_t pid = unProceso->id;

		void archivarSegmentos(tabla_direcciones *segmento){
			fprintf(archDump,"Proceso: %2d  |  Segmento: %2d  |  Inicio: %4d  |  Final: %4d  |  Tam: %4d\n",
					pid,
					segmento->id,
					segmento->inicio,
					segmento->inicio + segmento->size - 1,
					segmento->size);
		}

		pthread_mutex_lock(&(unProceso->sem_segmentos));
		t_list *segmentos = unProceso->segmentos;
		list_iterate(segmentos,(void*)archivarSegmentos);
		pthread_mutex_unlock(&(unProceso->sem_segmentos));
	}

	void dumpPaginacion(){
		for(int32_t frame=0; frame<cantFramesMP; frame++){
			fprintf(archDump,"Frame: %2d  |  ",frame);
			if(bitarray_test_bit(framesMP,frame)){
				int32_t nroPagina;
				proceso *proceso1 = buscarProcesoEnFrame(frame,&nroPagina,1); //Siempre deberia devolver un proceso xq el bitarray en 1
				fprintf(archDump,"Estado: Ocupado  |  Proceso: %d  |  Pagina: %d\n",proceso1->id,nroPagina);
			}
			else
				fprintf(archDump,"Estado: Libre\n");
		}
	}

	switch(cod_esquema){
		case PAGINACION:
			dumpPaginacion();
			break;
		case SEGMENTACION:
			pthread_mutex_lock(&sem_listaProcesos);
			list_iterate(listaProcesos,(void*)dumpSegmentacion);
			pthread_mutex_unlock(&sem_listaProcesos);
			break;
		default:
			log_info(loggerRam,"Error de Configuracion");
			break;
	}

	fclose(archDump);
	free(fecha);
	free(nombreArch);
	log_info(loggerRam, "Fin del Dump de Memoria");
}
//----------------------------------------






//TODO
void desmembrarDireccionLogicaConPaginacion(int32_t dirLogica, int32_t *pid, int32_t *nroPag, int32_t *offset){
	*pid = dirLogica % 100;
	*nroPag = (dirLogica / 100) / tamanio_pagina;
	*offset = (dirLogica /100) % tamanio_pagina;
}

