#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/mman.h>

//#include "semlib.h"
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#define MAX 10

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <sys/stat.h>
#include <sys/time.h>


typedef struct{
    	int n_threads;
	char politica[MAX];
	int n_autorizados;
	char autorizados[MAX][MAX];


} pipestruct;

typedef struct {
	int horas;
	int minutos;
	int segundos;
	int milisegundos;
} tempohora;
tempohora * tempo;
typedef struct{
	int tipo;
	int socket;
	char request[MAX];
	tempohora tempo;
}request;

typedef struct{
    	int porto;
    	int n_threads;
	char politica[MAX];
	char autorizados[MAX][MAX];
	int nautorizados;


} confstruct;

confstruct * configuracao;
request * pedidos;
pipestruct * pipes;

void init();
