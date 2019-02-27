#define PIPE_NAME "PIPE_NAME"
#define lenlinha 100
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#define BUF_MAX 1024
#define lenlinha 100
#define MAX 50


typedef struct{
    	int n_threads;
	char politica[MAX];
	int n_autorizados;
	char autorizados[MAX][MAX];


} pipestruct;


char linha[lenlinha];
int main()
{	

	
pipestruct * pipes;

pipes = (pipestruct*) malloc(sizeof(pipestruct));
	printf("criou pipe\n");
	
	// Opens the pipe for writing
	int fd;
	if ((fd = open(PIPE_NAME, O_WRONLY)) < 0) {
		perror("Cannot open pipe for writing: ");
		exit(0);
	}
	// Do some work
	printf("criou pipe\n");
	
	while (1) {
		printf("inserir comando\n");
		printf("<numero de threads> <politica> <numero de autorizados> <<nome fich1> <nome fichn...>>\n");
		fgets(linha, lenlinha-1, stdin);
		pipes->n_threads = atoi(linha);
		printf("threads %d\n",pipes->n_threads);
		
		fgets(linha, lenlinha-1, stdin);
		linha[strlen(linha)-1] = 0;
		strcpy(pipes->politica, linha);
		printf("%s\n",pipes->politica);
		fgets(linha, lenlinha-1, stdin);
		pipes->n_autorizados = atoi(linha);
		printf("autorizados %d\n",pipes->n_autorizados);
		int i;
	
		for(i=0;i<(pipes->n_autorizados);i++){
			fgets(linha, lenlinha-1, stdin);
			linha[strlen(linha)-1] = 0;
			printf("linha: %s\n", linha);
			strcpy(pipes->autorizados[i],linha);
			
		}
	
		write(fd, pipes, sizeof(pipestruct));
		printf("enviou!!\n");
	}
	return 0;
}
