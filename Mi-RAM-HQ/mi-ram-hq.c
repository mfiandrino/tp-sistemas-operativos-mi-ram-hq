#include "funciones_mi-ram-hq.h"

int main()
{
	remove(LOG_PATH);
	loggerRam = log_create(LOG_PATH, "mi-ram-hq", 0, LOG_LEVEL_INFO);

	int sk_server;
	pthread_t tid[4];

	inicializar(&sk_server);

	imprimirMemoria();

	pthread_create(&tid[0],NULL,(void*) hiloAtenderTripulates, &sk_server);
	pthread_create(&tid[1],NULL,(void*) hiloActualizarMapa,NULL);
	pthread_create(&tid[2],NULL,(void*) hiloDumpMemoria,NULL);
	signal(SIGUSR2,(void*)signalDump);

	if(cod_esquema == SEGMENTACION)
	{
		pthread_create(&tid[3],NULL,(void*) hiloCompactar,NULL);
		signal(SIGUSR1, (void*)signalCompactar);
		pthread_join(tid[3],NULL);
	}

	pthread_join(tid[0],NULL);
	pthread_join(tid[1],NULL);
	pthread_join(tid[2],NULL);

	bitarray_destroy(framesMP);
	bitarray_destroy(paginasMV);
	free(memoriaReal);
}
