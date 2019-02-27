#include "config.h"
//#include <semaphore.h>
#define lenlinha 20
#define BUF_MAX 1024
#define PIPE_NAME "PIPE_NAME"
#define FILE_NAME "server.log"
pid_t forksid[3];

confstruct *configuracao;
//request * pedidos;

int shmid,shmid2,shmid3;

char linha[lenlinha];
int n_threads;
int porto;
request * buffer;
pthread_t * threads;
//sem_t * mutex;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t * empty;
sem_t * full;
int* ids;

// Produce debug information
#define DEBUG	  	1

// Header of HTTP reply to client
#define	SERVER_STRING 	"Server: simpleserver/0.1.0\r\n"
#define HEADER_1	"HTTP/1.0 200 OK\r\n"
#define HEADER_2	"Content-Type: text/html\r\n\r\n"

#define GET_EXPR	"GET /"
#define CGI_EXPR	"cgi-bin/"
#define SIZE_BUF	1024


int  fireup(int port);
void identify(int socket);
void get_request(int socket);
int  read_line(int socket, int n);
void send_header(int socket);
void send_page(int socket);
void execute_script(int socket);
void not_found(int socket);
void catch_ctrlc(int);
void cannot_execute(int socket);
void processos();
void init();
void configuracoes();
void* gestaothreads(void *id);
void principal();
void estatisticas();
void insere_buffer();
int scheduler();
void create_pool_threads(int n_threads);
void terminate();
int verifica();
void* verifica_pipe(void *id);
void estatisticas();
void escreve_estatistica();
void imprime_estatistica();
void reset_estatistica();
char buf[SIZE_BUF];
char req_buf[SIZE_BUF];
char req_buf_aux[SIZE_BUF];
char buf_tmp[SIZE_BUF];
int port,socket_conn,new_conn;
int shmid4;
int buffer_id=0;
int nautorizados=0;
int offset = 0;
int PageSize = 0;
char *str;
//variaveis de estatisticas
int count_comprimido = 0;
int count_comp_hora = 0;
int count_comp_min = 0;
int count_comp_seg = 0;
int count_comp_mseg = 0;
int count_estatico = 0;
int count_est_hora = 0;
int count_est_min = 0;
int count_est_seg = 0;
int count_est_mseg = 0;

int main(){

	//funcao init
	init();
	//criar processos
	processos();

	//printf("principal\n");
	//principal();
	return 0;
}

void processos(){
	if(fork()==0){
		forksid[0] = getpid();
		printf("pid estatisticas %d\n",forksid[0] );
		printf("estatisticas\n");
		estatisticas();
		exit(0);
	}
	if(fork()==0){
		forksid[1] = getpid();
		printf("pid principal %d\n",forksid[1] );
		printf("principal\n");
		principal();
		exit(0);
	}
	if(fork()==0){
		forksid[2] = getpid();
		printf("pid configuraçoes %d\n",forksid[2] );
		printf("configuracoes\n");
		configuracoes();
		exit(0);
	}


}

void configuracoes(){

	FILE* file;


	if((file = fopen("configfile.txt", "r"))==0){
		perror("error finding file");
		return;
	}

	fgets(linha, lenlinha-1, file);
	configuracao->n_threads = atoi(linha);
	printf("%d\n",configuracao->n_threads);
	fgets(linha, lenlinha-1, file);
	configuracao->porto = atoi(linha);
	printf("%d\n",configuracao->porto);
	fgets(linha, lenlinha-1, file);
	linha[strlen(linha)-1] = 0;
	strcpy(configuracao->politica, linha);
	printf("%s\n",configuracao->politica);

	while(fgets(linha, lenlinha-1, file)!=NULL){
		linha[strlen(linha)-1] = 0;
		printf("linha: %s\n", linha);


		strcpy(configuracao->autorizados[nautorizados],linha);
		nautorizados++;

	}
	configuracao->nautorizados = nautorizados-1;
	printf("nautirizados na estrutura %d\n", configuracao->nautorizados);


	fclose(file);

}

void init(){



	//mmpaf
	int fd;
	if ((fd = open(FILE_NAME,O_RDWR | O_CREAT,0600)) < 0){
		perror("Error opening file for writing");
		exit(EXIT_FAILURE);
	}
	PageSize = (int)sysconf(_SC_PAGESIZE);
	lseek( fd, PageSize-1, SEEK_SET);

	// put something there
	write(fd, "" , 1);
	printf("vai mapear mmap\n");
	if ((str = mmap(0,PageSize,PROT_READ | PROT_WRITE,MAP_SHARED,fd,0)) == MAP_FAILED){
		perror("Error in mmap");
		exit(EXIT_FAILURE);
	}
	close(fd);

	//criar zona de memória partilhada
	shmid = shmget(IPC_PRIVATE, sizeof(request), IPC_CREAT|0700);
	shmid2 = shmget(IPC_PRIVATE, sizeof(confstruct), IPC_CREAT|0700);
    	shmid3 = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT|0700);

    	if(shmid < 0)
    	{
		perror("Error");
		exit(1);
    	}
	if(shmid2 < 0)
    	{
		perror("Error");
		exit(1);
    	}
	if(shmid3 < 0)
    	{
		perror("Error");
		exit(1);
    	}
    	buffer = (request*)shmat(shmid, NULL, 0);
	configuracao = (confstruct*)shmat(shmid2, NULL, 0);
	buffer_id =  (int)shmat(shmid3, NULL, 0);
	buffer_id = 0;


	//criaçao do named_pipe

	if ((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST)) {
		perror("Cannot create pipe for writing: ");
		exit(0);
	}
	printf("pipe criado com sucesso\n");

}

void principal(){


	int whil=0;
	n_threads = configuracao->n_threads;

	struct sockaddr_in client_name;
	socklen_t client_name_len = sizeof(client_name);
	int port = configuracao->porto;
	int socket_conn;
	signal(SIGINT,catch_ctrlc);
	//criar semáforos
    	sem_unlink("EMPTY");
        empty = sem_open("EMPTY", O_CREAT|O_EXCL, 0700, MAX);
        sem_unlink("FULL");
        full = sem_open("FULL", O_CREAT|O_EXCL, 0700, 0);

	int id;
	pthread_t thread_pipe;
	pthread_create(&thread_pipe, NULL, verifica_pipe , &id);

	create_pool_threads(n_threads);
	// Configure listening port
	if ((socket_conn=fireup(port))==-1)
		exit(1);

        // Accept connection on socket



	while(1){
		printf("while %d\n", whil);

		if ( (new_conn = accept(socket_conn,(struct sockaddr *)&client_name,&client_name_len)) == -1 ) {
			printf("Error accepting connection\n");
			exit(1);
		}

		insere_buffer(new_conn);
		whil++;

	}

}

void* verifica_pipe(void *id){

	int fd;
	if ((fd = open(PIPE_NAME, O_RDONLY)) < 0) {
	perror("Cannot open pipe for reading: ");
	exit(0);
	}
	pipestruct * pipes;
	pipes = (pipestruct*) malloc(sizeof(pipestruct));

	while(1){
		n_threads = configuracao->n_threads;
		read(fd, pipes,sizeof(*pipes));
		printf("depois de ler\n");
		int i;
		///matar threads
		for(i = 0; i < n_threads; i++){
			pthread_cancel(threads[i]);
		}
		printf("thread conf:%d\n",configuracao->n_threads);
		printf("lido: %d\n", pipes->n_threads);
		configuracao->n_threads = pipes->n_threads;
		printf("N_threads pipe: %d\n", configuracao->n_threads);
		strcpy(configuracao->politica,pipes->politica);
		printf("politica pipe: %s\n", pipes->politica);
		configuracao->nautorizados=pipes->n_autorizados;
		printf("n autorizados depois do pipe: %d\n", configuracao->nautorizados);
		for(i=0;i<configuracao->nautorizados;i++){
                strcpy(configuracao->autorizados[i], pipes->autorizados[i]);
			printf("autorizado: %s\n!!!!", configuracao->autorizados[i]);
		}

		///criar nova pool

		create_pool_threads(configuracao->n_threads);
	}
}

void insere_buffer(int new_conn){


		printf("entrou no insere\n");
		sem_wait(empty);
		printf("n fez id\n");
		// Identify new client
		identify(new_conn);
		printf("fez id\n");
		// Process request
		get_request(new_conn);
		//contar segundos
		struct timeval tp;
		gettimeofday(&tp, 0);
		time_t curtime = tp.tv_sec;
		struct tm *t = localtime(&curtime);
		buffer[buffer_id].tempo.horas = t->tm_hour;
		buffer[buffer_id].tempo.minutos = t->tm_min;
		buffer[buffer_id].tempo.segundos = t->tm_sec;
		buffer[buffer_id].tempo.milisegundos = tp.tv_usec/1000;
		printf("fez req\n");

		int i;
		int encontrou = 0;

		for(i=0;i<configuracao->nautorizados;i++){
			printf("esta autorizado %s\n", configuracao->autorizados[i]);
		}

		printf("nautorizados: %d\n",configuracao->nautorizados);
		for(i=0; i<(configuracao->nautorizados);i++){
			if(strstr(req_buf,configuracao->autorizados[i])!=NULL){
				printf("este era o igual %s\n", configuracao->autorizados[i]);
				printf("autorizou!\n");
				encontrou =1;
			}
		}
		printf("\t####\tencontrou ficou a: %d\n", encontrou);


		buffer[buffer_id].tipo = verifica();
		int var = 0;
		//descompressao
		if(buffer[buffer_id].tipo==0){
			if(encontrou == 1){
				var = 1;
				system("gzip -d htdocs/index.gz");
				int cnt=0;
				while(req_buf[cnt]!='.'){
					req_buf_aux[cnt] = req_buf[cnt];
					cnt++;
				}
				printf("pagina request: %s\n", req_buf_aux);
				strcpy(req_buf, req_buf_aux);


			}
			else

				printf("Nao encontrou pagina\n");

		}
		else {
			var =1;

		}/*
		if(buffer[buffer_id].tipo==1){
			char * resp = "[ -f ";
			char * resp2 = "' ] && echo 'yes' || echo 'no'";
			char *resp3 = "'htdocs/";
			char *result = malloc(strlen(resp)+strlen(req_buf)+strlen(resp2)+1);
			strcpy(result, resp);
			strcat(result, resp3);
    			strcat(result, req_buf);
			strcat(result, resp2);
			//char * nova = strcat(resp,req_buf);
			//printf("novo comando: %s\n", nova);
			char resposta[10];
			//printf("systemn: %s", system(result));
			//strcpy(system(result), resposta);


			//fgets(resposta, 10, stdin);

			//printf("linha de resposta: %s\n", resposta);
			//printf("comando system: %s\n", result);

			//if(system(result)){
				var = 1;
			//}
			printf("chegou aqui\n");



		}*/
		printf("var: %d\n",var);
		if(var==1){
			buffer[buffer_id].socket = new_conn;
			printf("Escreve no buffer o request: %s\n",buffer[buffer_id].request);
			strcpy(buffer[buffer_id].request, req_buf);
			printf("buffer antes de incrementar %d\n", buffer_id);
			buffer_id = ((buffer_id + 1 ) % MAX);
			printf("buffer id: %d\n", buffer_id);


			for(i=0;i<10;i++){
				printf("request:%s\n",buffer[i].request);
			}
			sem_post(full);
		}



	//encontrou

}


void* gestaothreads(void *id){
	int hora,min,seg,mseg;
   	while(1){
		sem_wait(full);
		int socket = scheduler();
		printf("vai fazer send page");
		send_page(socket);
		//diferenca de tempo de envio e receção
		struct timeval tp;
		gettimeofday(&tp, 0);
		time_t curtime = tp.tv_sec;
		struct tm *t = localtime(&curtime);
		hora = t->tm_hour;
		min = t->tm_min;
		seg = t->tm_sec;
		mseg = tp.tv_usec/1000;
		close(socket);
		escreve_estatistica(hora,min,seg,mseg);
		//imprime_estatistica();
		//raise(SIGUSR1);

		sem_post(empty);
	}
	return 0;
}

int scheduler(){
    char politica[MAX];
    int cnt=0;
    int ultimo = buffer_id-1;
    strcpy(politica, configuracao->politica);
    printf("politica:%s\n", politica);
    if(buffer[MAX-1].request==NULL){
	cnt = 0;
    }

    else{
	printf("DEU A VOLTA ENTROU AQUI VAI DAR RIP");
	cnt = buffer_id;
	ultimo = buffer_id-1;

    }
    printf("buffer id no scheduler: %d\n", buffer_id);
    printf("cnt antes do while %d\n", cnt);
    while(buffer[cnt-1].request!= NULL){

        printf("politica: %s\n", politica);
	printf("request no send page %s\n", buffer[cnt-1].request);
	if(strstr(politica,"normal") != NULL){
	    printf("entrou no scheduler normal\n");
	    return buffer[cnt-1].socket;
	}
	else if(strstr(politica,"estatico")!=NULL){
		if(buffer[cnt-1].tipo == 1){
		 	return buffer[cnt-1].socket;
		}
		if((cnt-1)==ultimo){
			printf("não há nada estatico vai voltar ao inicio");
			if(buffer[MAX-1].request==NULL){
				cnt = 0;
			 }
		    	else{

				cnt = buffer_id;
				ultimo = buffer_id-1;
		   	 }
			 while(buffer[cnt-1].request!= NULL){
				printf("esta a percorrer à procura de gz");
				if(buffer[cnt-1].tipo == 0){
					printf("encontrou o gz");
		 			return buffer[cnt-0].socket;
				}
				cnt++;
			}
		}
		printf("scheduler entrou no estatico\n");
	    }



	else if(strstr(politica,"comprimido")!=NULL){
	    	printf("scheduler entrou no comprimidp\n");
		printf("politica: %s\n dfgyuio", politica);
		printf("request no send page %s\n", buffer[cnt-1].request);

	    if(buffer[cnt-1].tipo == 0){
			printf("entrou vai enviar BÇDRTFYGUH");
			return buffer[cnt-1].socket;
		}

	    if((cnt-1)==ultimo){
			printf("não há nada gz vai voltar ao inicio");
			if(buffer[MAX-1].request==NULL){
				cnt = 0;
			 }
		    	else{

				cnt = buffer_id;
				ultimo = buffer_id-1;
		   	 }
			 while(buffer[cnt-1].request!= NULL){
				printf("esta a percorrer à procura de gz");
				if(buffer[cnt-1].tipo == 1){
					printf("encontrou o estatico");
		 			return buffer[cnt-0].socket;
				}
				cnt++;
			}
		}

	    }


	/*
	if((cnt-1)==ultimo){
		printf("politica: %s\n", politica);
		printf("request no send page %s\n", buffer[cnt-1].request);
		if(strstr(politica,"normal") != NULL){
			printf("vai enviar2");
		    return buffer[cnt-1].socket;}

		else if(strstr(politica,"estatico")!=NULL){
			printf("vai enviar34");
		    if(buffer[cnt-1].tipo == 1)

			 return buffer[cnt-1].socket;
		}
		else if(strstr(politica,"comprimido")!=NULL){
			printf("vai enviar");
		    if(buffer[cnt-1].tipo == 0)
			printf("entrou vai enviar");
			 return buffer[cnt-1].socket;
		}
		break;

	}*/
	cnt++;

    }
    return 0;
}


int verifica(){
	printf("verifica\n");
	int i;
	int cnt = 0;
	char extensao[30];
	printf("%s\n",req_buf);
	while(req_buf[cnt]!='.'){


		cnt++;
	}
	i=cnt;
	//printf("cnt:%d\n",cnt);
	while(i<(strlen(req_buf))){
		extensao[i-cnt] = req_buf[i];
		i++;
	}
	printf("extensão:%s\n",extensao);

	if(strstr(extensao, ".gz")!=NULL){
		printf("encontrou gz\n");
		return 0;

	}
	if(strstr(extensao, ".html")!=NULL){
		printf("encontrou html\n");
		return 1;

	}
	return 1;
}


int fireup(int port)
{
	int new_sock;
	struct sockaddr_in name;
	// Creates socket
	if ((new_sock = socket(PF_INET, SOCK_STREAM, 0))==-1) {
		printf("Error creating socket\n");
		return -1;
	}

	// Binds new socket to listening port
 	name.sin_family = AF_INET;
 	name.sin_port = htons(port);
 	name.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(new_sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
		printf("Error binding to socket\n");
		return -1;
	}

	// Starts listening on socket
 	if (listen(new_sock, 5) < 0) {
		printf("Error listening to socket\n");
		return -1;
	}

	return(new_sock);
}



void create_pool_threads(int n_threads){
	int i;
	threads = malloc(sizeof(n_threads*(sizeof(pthread_t))));
	ids = malloc(sizeof(n_threads*(sizeof(int))));
	for(i=0;i<n_threads;i++){
		printf("criou threads\n");
		ids[i] = i;
		pthread_create(&threads[i], NULL, gestaothreads ,&ids[i]);
	}
}



void estatisticas(){
	//criar ficheiro em memoria
	signal(SIGUSR1,imprime_estatistica);
	signal(SIGUSR2,reset_estatistica);
}

void escreve_estatistica(int fim_hora,int fim_min,int fim_seg,int fim_mseg){
	int fd;
	int id_tipo;
	char request_aux[MAX];
	int ini_hora,ini_min,ini_seg,ini_mseg,var_hora,var_min,var_seg,var_mseg;
	int bytes=0;
	char a1[MAX];
	char a2[MAX];

	printf("offset:%d\n",offset);

	if ((fd = open(FILE_NAME,O_RDWR | O_CREAT,0600)) < 0){
		perror("Error opening file for writing");
		exit(EXIT_FAILURE);
	}
	printf("entour estaaaa\n");
	id_tipo=buffer[buffer_id-1].tipo;
	strcpy(request_aux,buffer[buffer_id-1].request);
	ini_hora=buffer[buffer_id-1].tempo.horas;
	ini_min=buffer[buffer_id-1].tempo.minutos;
	ini_seg=buffer[buffer_id-1].tempo.segundos;
	ini_mseg=buffer[buffer_id-1].tempo.milisegundos;
	strcpy(a1,"\n");
	strcpy(a2,":");
	printf("guardou vaçpres resst\n");
	bytes+= sizeof(id_tipo)+sizeof(request_aux)+sizeof(ini_hora)+sizeof(ini_min)+sizeof(ini_seg)+sizeof(ini_mseg)+4*sizeof(a1)+6*sizeof(a2)+sizeof(fim_hora)+sizeof(fim_min)+sizeof(fim_seg)+sizeof(fim_mseg);
	printf("bytes a escrever: %d\n",bytes);

	printf("BYTES: %d E PAGESIZE: %d\n", bytes+offset,PageSize );

	if(bytes+offset>PageSize){
		PageSize = PageSize*2;
		lseek( fd, PageSize-1, SEEK_SET);

		// put something there
		write(fd, "" , 1);
		printf("Nao ha espaço no ficheiro, munmap e mmap novamente\n");
		if( munmap(str,PageSize) == -1){
			perror("Error in munmap");}
		printf("pagesize: %d", PageSize);

		if ((str = mmap(0,PageSize,PROT_READ | PROT_WRITE,MAP_SHARED,fd,0)) == MAP_FAILED)	{
			perror("Error in mmap");
			exit(EXIT_FAILURE);
		}

	}

	printf("escreve no fich de escrita\n");
	offset += sprintf( str + offset, "%d\n", buffer[buffer_id-1].tipo);

	offset += sprintf(str + offset, "%s\n", buffer[buffer_id-1].request);

	offset += sprintf(str + offset, "%d:%d:%d:%d\n", buffer[buffer_id-1].tempo.horas,buffer[buffer_id-1].tempo.minutos,buffer[buffer_id-1].tempo.segundos,buffer[buffer_id-1].tempo.milisegundos);

	offset += sprintf(str + offset, "%d:%d:%d:%d\n", fim_hora,fim_min,fim_seg,fim_mseg);


	//ve diferença de tempos
	if (ini_mseg > fim_mseg){
		var_mseg = 60 - ini_mseg + fim_mseg;
	}
	else{
		var_mseg = fim_mseg - ini_mseg;
	}
	if (ini_seg > fim_seg){
		var_seg = 60 - ini_seg + fim_seg;
	}
	else{
		var_seg = fim_seg - ini_seg;
	}
	if (ini_min > fim_min){
		var_min = 60 - ini_min + fim_min;
	}
	else{
		var_min = fim_min - ini_min;
	}
	var_hora = fim_hora - ini_hora;
	printf("Diferenca de tempo: %d:%d:%d:%d\n",var_hora,var_min,var_seg,var_mseg);
	if(id_tipo==0){
		printf("vai incrementar comprimido\n");
		count_comprimido++;
		count_comp_hora += var_hora;
		count_comp_min += var_min;
		count_comp_seg += var_seg;
		count_comp_mseg += var_mseg;
	}
	else if(id_tipo==1){
		printf("vai incrementar estatico\n");
		count_estatico++;
		count_est_hora += var_hora;
		count_est_min += var_min;
		count_est_seg += var_seg;
		count_est_mseg += var_mseg;
	}

	printf("count estatico: %d\n", count_estatico);
	printf("count comprimido: %d\n", count_comprimido);


	close(fd);
}


void imprime_estatistica(){
	printf("A imprimir estatisticas\n");
	count_comp_hora = count_comp_hora/count_comprimido;
	count_comp_min = count_comp_min/count_comprimido;
	count_comp_seg = count_comp_seg/count_comprimido;
	count_comp_mseg = count_comp_mseg/count_comprimido;
	count_est_hora = count_est_hora/count_estatico;
	count_est_min = count_est_min/count_estatico;
	count_est_seg = count_est_seg/count_estatico;
	count_est_mseg = count_est_mseg/count_estatico;

	printf("Numero total de pedidos servidos (paginas estaticas): %d\n",count_estatico);
	printf("Numero total de pedidos servidos (ficheiros comprimidos): %d\n",count_comprimido);
	printf("Tempo medio para servir um pedido a conteudo estatico nao comprimido: %d:%d:%d:%d\n",count_comp_hora,count_comp_min,count_comp_seg,count_comp_mseg);
	printf("Tempo medio para servir um pedido a conteudo estatico comprimido: %d:%d:%d:%d\n",count_est_hora,count_est_min,count_est_seg,count_est_mseg);
}

void reset_estatistica(int sig){
	printf("Fazer reset\n");
	count_comp_hora = 0;
	count_comp_min = 0;
	count_comp_seg = 0;
	count_comp_mseg = 0;
	count_est_hora = 0;
	count_est_min = 0;
	count_est_seg = 0;
	count_est_mseg = 0;
	count_estatico=0;
	count_comprimido=0;
}


void get_request(int socket)
{
	int i,j;
	int found_get;

	found_get=0;
	while ( read_line(socket,SIZE_BUF) > 0 ) {
		if(!strncmp(buf,GET_EXPR,strlen(GET_EXPR))) {
			// GET received, extract the requested page/script
			found_get=1;
			i=strlen(GET_EXPR);
			j=0;
			while( (buf[i]!=' ') && (buf[i]!='\0') )
				req_buf[j++]=buf[i++];
			req_buf[j]='\0';
		}
	}


	// Currently only supports GET
	if(!found_get) {
		printf("Request from client without a GET\n");
		exit(1);
	}
	// If no particular page is requested then we consider htdocs/index.html
	if(!strlen(req_buf))
		sprintf(req_buf,"dijkstra.gz");

	#if DEBUG
	printf("get_request: client requested the following page: %s\n",req_buf);
	#endif

	return;
}


// Send message header (before html page) to client
void send_header(int socket)
{
	#if DEBUG
	printf("send_header: sending HTTP header to client\n");
	#endif
	sprintf(buf,HEADER_1);
	send(socket,buf,strlen(HEADER_1),0);
	sprintf(buf,SERVER_STRING);
	send(socket,buf,strlen(SERVER_STRING),0);
	sprintf(buf,HEADER_2);
	send(socket,buf,strlen(HEADER_2),0);

	return;
}


// Execute script in /cgi-bin
void execute_script(int socket)
{
	// Currently unsupported, return error code to client
	cannot_execute(socket);

	return;
}


// Send html page to client
void send_page(int socket){
	FILE * fp;

	// Searchs for page in directory htdocs
	sprintf(buf_tmp,"htdocs/%s",req_buf);

	#if DEBUG
	printf("send_page: searching for %s\n",buf_tmp);
	#endif

	// Verifies if file exists
	if((fp=fopen(buf_tmp,"rt"))==NULL) {
		// Page not found, send error to client
		printf("send_page: page %s not found, alerting client\n",buf_tmp);
		not_found(socket);
	}
	else {
		// Page found, send to client

		// First send HTTP header back to client
		send_header(socket);

		printf("send_page: sending page %s to client\n",buf_tmp);
		while(fgets(buf_tmp,SIZE_BUF,fp))
			send(socket,buf_tmp,strlen(buf_tmp),0);

		// Close file
		fclose(fp);
	}

	return;

}


// Identifies client (address and port) from socket
void identify(int socket)
{
	char ipstr[INET6_ADDRSTRLEN];
	socklen_t len;
	struct sockaddr_in *s;
	int port;
	struct sockaddr_storage addr;

	len = sizeof addr;
	getpeername(socket, (struct sockaddr*)&addr, &len);

	// Assuming only IPv4
	s = (struct sockaddr_in *)&addr;
	port = ntohs(s->sin_port);
	inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

	printf("identify: received new request from %s port %d\n",ipstr,port);

	return;
}


// Reads a line (of at most 'n' bytes) from socket
int read_line(int socket,int n)
{
	int n_read;
	int not_eol;
	int ret;
	char new_char;

	n_read=0;
	not_eol=1;

	while (n_read<n && not_eol) {
		ret = read(socket,&new_char,sizeof(char));
		if (ret == -1) {
			printf("Error reading from socket (read_line)");
			return -1;
		}
		else if (ret == 0) {
			return 0;
		}
		else if (new_char=='\r') {
			not_eol = 0;
			// consumes next byte on buffer (LF)
			read(socket,&new_char,sizeof(char));
			continue;
		}
		else {
			buf[n_read]=new_char;
			n_read++;
		}
	}

	buf[n_read]='\0';
	#if DEBUG
	printf("read_line: new line read from client socket: %s\n",buf);
	#endif

	return n_read;
}


// Creates, prepares and returns new socket


// Sends a 404 not found status message to client (page not found)
void not_found(int socket)
{
 	sprintf(buf,"HTTP/1.0 404 NOT FOUND\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,SERVER_STRING);
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"Content-Type: text/html\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"<HTML><TITLE>Not Found</TITLE>\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"<BODY><P>Resource unavailable or nonexistent.\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"</BODY></HTML>\r\n");
	send(socket,buf, strlen(buf), 0);

	return;
}


// Send a 5000 internal server error (script not configured for execution)
void cannot_execute(int socket)
{
	sprintf(buf,"HTTP/1.0 500 Internal Server Error\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"Content-type: text/html\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"<P>Error prohibited CGI execution.\r\n");
	send(socket,buf, strlen(buf), 0);

	return;
}


// Closes socket before closing
void catch_ctrlc(int sig)
{
        int i;
	printf("Server terminating\n");

	//wait
	for(i=0;i<3;i++){
		wait(NULL);

	}
	if( munmap(str,PageSize) == -1){
		perror("Error in munmap");
	}
	shmctl(shmid, IPC_RMID,NULL);
	shmctl(shmid2, IPC_RMID,NULL);
	shmctl(shmid3, IPC_RMID,NULL);
	printf("oi\n");
	n_threads = configuracao->n_threads;
	for(i = 0; i < n_threads; i++){
		pthread_cancel(threads[i]);
	}
	for(i=0;i<n_threads;i++){
		pthread_join(threads[i],NULL);
	}
	for(i = 0; i < 3; i++){
		kill(forksid[i], SIGKILL);
	}
	printf("fechou\n");
	close(socket_conn);

	exit(0);
}


