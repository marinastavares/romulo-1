#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>

//Variáveis Globais
#define NSEC_PER_SEC    (1000000000)
#define FALHA 1
#define	TAM_MEU_BUFFER 10
#define NUMERO_CICLOS 10000
#define S 4184
#define P 1000
#define B 4
#define R 0.001
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

// Variáveis para o cálculo do sistema
float No, Nt, Ni, Nf, Na;
float Q, Qu, Qt;
float Tref;
float Href;
float Kp, Ki, Kni;
int esfriando, explosao;

//Variáveis referentes aos sockets
int socket_local;
struct sockaddr_in endereco_destino;
double Tref;
double Href;
unsigned long int tempo[NUMERO_CICLOS];
char comando[20];
int portaDestino;
int socketLocal;
struct sockaddr_in enderecoDestino;

//Variáveis para o buffer
float buffer1[TAM_MEU_BUFFER];
float buffer2[TAM_MEU_BUFFER];
int buffer_cheio;
int buffer_descarregado;
long int leitura = 0;

//Inicializando as threads
pthread_t temperatura, nivel, tela, buffer, dados_arquivo, alarme;

// Mutex para ser possível realizar a leitura das sockets
pthread_mutex_t mutex_socket = PTHREAD_MUTEX_INITIALIZER;
// Mutex sobre alarmes
pthread_mutex_t mutex_alarmes = PTHREAD_MUTEX_INITIALIZER;
// Mutex`s sobre as referencias
pthread_mutex_t mutex_nivel = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_temperatura = PTHREAD_MUTEX_INITIALIZER;
//Mutex e cond do buffer
pthread_mutex_t mutex_buffer = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t buff = PTHREAD_COND_INITIALIZER;


//funcoes


int cria_socket_local(void)
{
	int socket_local;		/* Socket usado na comunicacão */

	socket_local = socket( PF_INET, SOCK_DGRAM, 0);
	if (socket_local < 0) {
		perror("socket");
		//return;
	}
	return socket_local;
}

struct sockaddr_in cria_endereco_destino(char *destino, int porta_destino)
{
	struct sockaddr_in servidor; 	/* Endereço do servidor incluindo ip e porta */
	struct hostent *dest_internet;	/* Endereço destino em formato próprio */
	struct in_addr dest_ip;		/* Endereço destino em formato ip numérico */

	if (inet_aton ( destino, &dest_ip ))
		dest_internet = gethostbyaddr((char *)&dest_ip, sizeof(dest_ip), AF_INET);
	else
		dest_internet = gethostbyname(destino);

	if (dest_internet == NULL) {
		fprintf(stderr,"Endereço de rede inválido\n");
		exit(FALHA);
	}

	memset((char *) &servidor, 0, sizeof(servidor));
	memcpy(&servidor.sin_addr, dest_internet->h_addr_list[0], sizeof(servidor.sin_addr));
	servidor.sin_family = AF_INET;
	servidor.sin_port = htons(porta_destino);

	return servidor;
}


void envia_mensagem(int socket_local, struct sockaddr_in endereco_destino, char *mensagem)
{
	/* Envia msg ao servidor */

	if (sendto(socket_local, mensagem, strlen(mensagem)+1, 0, (struct sockaddr *) &endereco_destino, sizeof(endereco_destino)) < 0 )
	{
		perror("sendto");
		return;
	}
}


int recebe_mensagem(int socket_local, char *buffer, int TAM_BUFFER)
{
	int bytes_recebidos;		/* Número de bytes recebidos */

	/* Espera pela msg de resposta do servidor */
	bytes_recebidos = recvfrom(socket_local, buffer, TAM_BUFFER, 0, NULL, 0);
	if (bytes_recebidos < 0)
	{
		perror("recvfrom");
	}

	return bytes_recebidos;
}

void setaValores(char *string, float valor){
	int nrec;
	char msg_aux[1000];
	char mensagem_atuador[1000];
	pthread_mutex_lock(&mutex_socket);
	sprintf(mensagem_atuador,"%s%f",string,valor);
	envia_mensagem(socket_local, endereco_destino, mensagem_atuador);
	nrec = recebe_mensagem(socket_local, msg_aux, 1000);
	msg_aux[nrec]='\0';
	pthread_mutex_unlock(&mutex_socket);
}

float leValores(char *string){
	int nrec;
	char leitura_sensor[1000];
	float valorsensor;

	pthread_mutex_lock(&mutex_socket);
	envia_mensagem(socket_local,endereco_destino,string);
	nrec = recebe_mensagem(socket_local,leitura_sensor,1000);
	leitura_sensor[nrec] = '\0';
	pthread_mutex_unlock(&mutex_socket);

	valorsensor = atof(strncpy(leitura_sensor,leitura_sensor+3,sizeof(leitura_sensor)));
	return valorsensor;
}

void Inicializa()
{
	setaValores("anf",0.0); // Atuando em Nf (nivel)
	setaValores("ani",0.0); // Atuando em Ni (nivel)
	setaValores("ana",0.0); // Atuando em Na (nivel)
	printf("Controle de Temperatura e Nivel em um tanque\n\n");
	printf("Defina o valor de temperatura desejado: \t\t\t\t");
	scanf("%lf",&Tref);
	printf("Defina o valor de nivel desejado:l\t\t\t\t");
	scanf("%lf",&Href);
	while (Tref < 0 || Tref > 50 || Href<0.1 ||Href>3)
	{
		system("clear");
		printf("ERROR 404 valor invalido. \n A temperatura deve ser entre 0 e 50. O nivel deve ser entre 0.1 e 3.\n");
		Inicializa();
	}
	system("clear");
}

void mostraTela(void)
{
	float T,Ta,Ti,No,H;

	while(1){
	pthread_mutex_lock(&mutex_escrita);
	T = leValores("st-0");
	Ta = leValores("sta0");
	Ti = leValores("sti0");
	No = leValores("sno0");
	H = leValores("sh-0");
	system("clear");
	printf("*-*-*-*-  Controle de Temperatura e nivel em um tanque    *-*-*-*-\n\n");

	pthread_mutex_lock(&mutex_temperatura);
	printf("Valor desejado de temperatura\t\t\t\t%.2f\n",Tref);
	pthread_mutex_unlock(&mutex_temperatura);
	pthread_mutex_lock(&mutex_nivel);
	printf("Valor desejado de nivel\t\t\t\t%.2f\n",Href);
	pthread_mutex_unlock(&mutex_nivel);
	printf("Temperatura da agua no tanque (T)\t\t%.2f\n",T);
	printf("Temperatura externa (Ta)\t\t\t%.2f\n",Ta);
	printf("Temperatura da agua entrando (Ti)\t\t%.2f\n", Ti);
	printf("Fluxo de saida de agua (No)\t\t\t%.2f\n",No);
	printf("Nivel no tanque (H)\t\t\t\t%.2f\n",H);
	if (explosao == 1){
		printf(" CUIDADO O sistema passou de 30 graus)\t\t\t\\n");
	}
	printf("\n*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-\n");

	pthread_mutex_unlock(&mutex_escrita);
	sleep(1);
	}
}

void tocaAlarme(){
	float T;
	struct timespec t;
	long int interval = 10000000; /* 10ms*/

	 clock_gettime(CLOCK_MONOTONIC ,&t);
   	 t.tv_sec++;

	while(1){
		pthread_mutex_lock(&mutex_alarmes);
		T = leValores("st-0");
		if(T > 30) {
			explosao=1;
		} else {
			explosao=0;
		}
		pthread_mutex_unlock(&mutex_alarmes);

		        t.tv_nsec += interval;

        while (t.tv_nsec >= NSEC_PER_SEC)
        {
           t.tv_nsec -= NSEC_PER_SEC;
           t.tv_sec++;
        }

	}
}


void controleTemperatura(){
	struct timespec t;
	double Kp_temp, Terro, controle_temp;
	float T, Ta, Ti, H, erroT, C;
	float Qc;
	int t0;
	Kp_temp = 500000;
	float T;
	long int interval = 50000000; /* 50ms*/
	/* start after one second */
    	clock_gettime(CLOCK_MONOTONIC ,&t);
    	t.tv_sec++;
    	while(1)
        {
        /* wait until next shot */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

        /* do the stuff */

		T = leValores("st-0");
		H = leValores( "sh-0");
		Ti = leValores("sti0");
		Ta = leValores("sta0");
		pthread_mutex_lock(&mutex_temperatura);
		Terro = Tref - T;
		pthread_mutex_unlock(&mutex_temperatura);

		C = S*P*B*H;
		t0 = 10;
		Kp = C/t0 + 40;
		Qc = Kp*Terro;
		Qt = Q + Ni*S*(Ti-T) + Na*S*(80-T) - (T-Ta)/R;

		if(Terro > 0) {
			Q = MIN(1000000,Qc - (Qt - Q));
			Qu = Q;
			esfriando = 0;
		}
		else {
			Q = 0;
			esfriando = 1;
		}

		setaValores("aq-",Q);
                /* calculate next shot */
        t.tv_nsec += interval;

        while (t.tv_nsec >= NSEC_PER_SEC)
        {
           t.tv_nsec -= NSEC_PER_SEC;
           t.tv_sec++;
        }
	}

}

void controleNivel(){
	struct timespec t;
	double Kp_niv, Herro, C_nivel;
	Kp_niv = 1000;
	float H;
	long int interval = 70000000; /* 70ms*/
	/* start after one second */
   	 clock_gettime(CLOCK_MONOTONIC ,&t);
   	 t.tv_sec++;
   	 while(1)
        {
        /* wait until next shot */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

        /* do the stuff */


		H = leValores("sh-0");
		pthread_mutex_lock(&mutex_nivel);
		Herro = Href - H;
		pthread_mutex_unlock(&mutex_nivel);
		C_nivel = Kp_niv*Herro;
		if(fabs(C_nivel)>100)
		{
			C_nivel = 100*C_nivel/fabs(C_nivel);
		}
		if(C_nivel>0)
		{
			setaValores("ani",C_nivel);
		}else{
			setaValores("anf",fabs(C_nivel));
		}

                /* calculate next shot */
        t.tv_nsec += interval;

        while (t.tv_nsec >= NSEC_PER_SEC)
        {
           t.tv_nsec -= NSEC_PER_SEC;
           t.tv_sec++;
        }
	}

}

void bufferDuplo(){
	int buffer_selecionado = 0; // 0 buffer1 e 1 buffer2
	int posicao = 0;
	float Temp;
	struct timespec t;
	long int interval = 500000000; /* 500ms*/
	/* start after one second */
   	clock_gettime(CLOCK_MONOTONIC ,&t);
   	t.tv_sec++;
   	while(1){
		clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&t,NULL);
		pthread_mutex_lock(&mutex_buffer);
		Temp = leValores("st-0");

		if (buffer_selecionado == 0){
			buffer1[posicao] = Temp;
			posicao++;
		}
		else {
			buffer2[posicao] = Temp;
			posicao++;
		}

		if (posicao == TAM_MEU_BUFFER){
			buffer_descarregado = buffer_selecionado;
			buffer_selecionado = !buffer_selecionado;
			posicao = 0;
			buffer_cheio = 1;
			pthread_cond_signal(&buff);
		}

		pthread_mutex_unlock(&mutex_buffer);
		t.tv_nsec += interval;

		while (t.tv_nsec >= NSEC_PER_SEC) {
	  		t.tv_nsec -= NSEC_PER_SEC;
	  		t.tv_sec++;
		}
   	}
}


void escreveArquivo(){
	while(1){
		pthread_mutex_lock(&mutex_buffer);
		while(buffer_cheio == 0){
			pthread_cond_wait(&buff,&mutex_buffer);
		}
		FILE* arquivo = fopen("Trab2.txt","a+");

		if (buffer_descarregado == 0){
			int i;
			for (i =0 ; i < TAM_MEU_BUFFER ; i++){
				char conteudo[20]="";
				sprintf(conteudo,"Valor %ld: %f\n", ++leitura, buffer1[i]);
				fputs(conteudo,arquivo);
			}
		}else{
			int i;
			for (i =0 ; i < TAM_MEU_BUFFER ; i++){
				char conteudo[20]="";
				sprintf(conteudo,"Valor %ld: %f\n", ++leitura, buffer2[i]);
				fputs(conteudo,arquivo);
			}
		}
		fclose(arquivo);
		buffer_cheio = 0;
		pthread_mutex_unlock(&mutex_buffer);
	}
}


int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		fprintf(stderr,"ERROR Faltam parametros\n");
		fprintf(stderr,"Uso: endereço porta\n");
		fprintf(stderr,"Ex.: localhost 4545\n");
		exit(FALHA);
	}
	socket_local = cria_socket_local();
	endereco_destino = cria_endereco_destino(argv[1], atoi(argv[2]));
	system("clear");
	Inicializa();

	pthread_create(&temperatura, NULL, (void *) controleTemperatura, NULL);
	pthread_create(&nivel, NULL, (void *) controleNivel, NULL);
	pthread_create(&tela, NULL, (void *) mostraTela, NULL);
	pthread_create(&buffer,NULL,(void *) bufferDuplo,NULL);
	pthread_create(&dados_arquivo, NULL, (void *) escreveArquivo, NULL);
	pthread_create(&alarme, NULL, (void *)tocaAlarme, NULL);

	pthread_join(temperatura, NULL);
	pthread_join(nivel, NULL);
	pthread_join(tela, NULL);
	pthread_join(buffer,NULL);
	pthread_join(dados_arquivo,NULL);
	pthread_join(alarme,NULL);

}



