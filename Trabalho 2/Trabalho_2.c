// INCLUDES
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

//DEFINES
#define NSEC_PER_SEC    (1000000000) /* The number of nsecs per sec. */
#define FALHA 1
#define	TAM_MEU_BUFFER 10
#define NCICLOS 10000
#define S 4184
#define P 1000
#define B 4
#define R 0.001
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int portaDestino;
int socketLocal, socketLocal2;
struct sockaddr_in enderecoDestino;
float No, Nt, Ni, Nf, Na;
float Q, Qu, Qt;
float Tref;
float Href;
float Kp, Ki, Kni;
int esfriando, explosao;

//Variaveis
int socket_local;
struct sockaddr_in endereco_destino;
double setpoint_temp;
double setpoint_niv;
unsigned long int tempo[NCICLOS];
char comando[20];

//buffer
float buffer1[TAM_MEU_BUFFER];
float buffer2[TAM_MEU_BUFFER];
int buffer_cheio;
int descarrega_buffer;
long int leitura = 0;


// Mutex da exclusao mutua na utilizacao do socket
pthread_mutex_t em_socket = PTHREAD_MUTEX_INITIALIZER;
// Mutex sobre escrita
pthread_mutex_t em_escrita = PTHREAD_MUTEX_INITIALIZER;
// Mutex sobre alarmes
pthread_mutex_t em_alarmes = PTHREAD_MUTEX_INITIALIZER;
// Mutex`s sobre as referencias
pthread_mutex_t em_href = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t em_tref = PTHREAD_MUTEX_INITIALIZER;
//Mutex e cond do buffer
pthread_mutex_t em_buff = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t buff = PTHREAD_COND_INITIALIZER;

//threads
pthread_t temperatura, nivel, tela, buffer, teclado, escrevearquivo, alarme;

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

void atuar(char *string, float valor){
	int nrec;
	char msg_aux[1000];
	char mensagem_atuador[1000];
	pthread_mutex_lock(&em_socket);
	sprintf(mensagem_atuador,"%s%f",string,valor);
	envia_mensagem(socket_local, endereco_destino, mensagem_atuador);
	nrec = recebe_mensagem(socket_local, msg_aux, 1000);
	msg_aux[nrec]='\0';
	pthread_mutex_unlock(&em_socket);
}

float lesensor(char *string){
	int nrec;
	char leitura_sensor[1000];
	float valorsensor;

	pthread_mutex_lock(&em_socket);
	envia_mensagem(socket_local,endereco_destino,string);
	nrec = recebe_mensagem(socket_local,leitura_sensor,1000);
	leitura_sensor[nrec] = '\0';
	pthread_mutex_unlock(&em_socket);

	valorsensor = atof(strncpy(leitura_sensor,leitura_sensor+3,sizeof(leitura_sensor)));
	return valorsensor;
}

void inicializacao()
{
	atuar("anf",0.0); // Atuando em Nf (nivel)
	atuar("ani",0.0); // Atuando em Ni (nivel)
	atuar("ana",0.0); // Atuando em Na (nivel)
	printf("-.-.-.-.- Controle de Temperatura e Nivel em um tanque -.-.-.-.-\n\n");
	printf("\n");
	printf("\n");
	printf("\n");
	printf("\n");
	printf("\n");
	printf("\n");
	printf("\n-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-\n");	
	system("echo \"\033[2;1H\"");
	printf("Setpoint de temperatura\t\t\t\t");
	scanf("%lf",&setpoint_temp);
	printf("Setpoint de nivel\t\t\t\t");
	scanf("%lf",&setpoint_niv);
	while (setpoint_temp < 0 || setpoint_temp > 50 || setpoint_niv<0.1 ||setpoint_niv>3)
	{
		system("clear");
		printf("ERRO: O valor que voce inseriu nao e valido. A temperatura deve ser entre 0 e 50. O nivel deve ser entre 0.1 e 3.\n");
		inicializacao();
	}
}

void update_tela(void)
{	
	float st,sta,sti,sno,sho;
	
	while(1){
	pthread_mutex_lock(&em_escrita);
	st = lesensor("st-0");
	sta = lesensor("sta0");
	sti = lesensor("sti0");
	sno = lesensor("sno0");
	sho = lesensor("sh-0");
	system("clear");
	printf("-.-.-.-.- Controle de Temperatura e nivel em um tanque -.-.-.-.-\n\n");
	
	pthread_mutex_lock(&em_tref);
	printf("Setpoint de temperatura\t\t\t\t%.2f\n",setpoint_temp);
	pthread_mutex_unlock(&em_tref);
	pthread_mutex_lock(&em_href);
	printf("Setpoint de nivel\t\t\t\t%.2f\n",setpoint_niv);
	pthread_mutex_unlock(&em_href);
	printf("Temperatura da agua no tanque (T)\t\t%.2f\n",st);
	printf("Temperatura externa (Ta)\t\t\t%.2f\n",sta);
	printf("Temperatura da agua entrando (Ti)\t\t%.2f\n", sti);
	printf("Fluxo de saida de agua (No)\t\t\t%.2f\n",sno);
	printf("Nivel no tanque (H)\t\t\t\t%.2f\n",sho);
	if (explosao == 1){
		printf("O sistema passou de 30 graus)\t\t\t\\n");
	}
	printf("\n-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-\n");
	
	pthread_mutex_unlock(&em_escrita);
	sleep(1);
	}
}

void toca_alarme(){
	float T;
	struct timespec t;
	long int interval = 10000000; /* 70ms*/

	 clock_gettime(CLOCK_MONOTONIC ,&t);
   	 t.tv_sec++;

	while(1){
		pthread_mutex_lock(&em_alarmes);
		T = lesensor("st-0");
		if(T > 30) {
			explosao=1;
		} else {
			explosao=0;
		}
		pthread_mutex_unlock(&em_alarmes);

		        t.tv_nsec += interval;

        while (t.tv_nsec >= NSEC_PER_SEC) 
        {
           t.tv_nsec -= NSEC_PER_SEC;
           t.tv_sec++;
        }

	}
}

void escreve_teclado(){
	while(1){
		char letra;
		letra = getc(stdin);
		__fpurge(stdin);
		if(letra == 32){
			pthread_mutex_lock(&em_escrita);
			char escolha;
			float novovalor;
			printf("Voce deseja atualizar a referencia de nivel(h) ou de temperatura(t)? Para atualizar os dois use k! Para sair digite exit(e)");
			while(1){
				scanf("%c",&escolha);
				__fpurge(stdin);
				if (escolha == 'h' || escolha =='t' || escolha == 'e' || escolha == 'k'){
					break;
				}else{
					printf("h, t, k ou e por favor!\n");
				}
			}
			
			if (escolha == 'h'){			 
				printf("Qual o novo valor?");
				while(1){
					scanf("%f",&novovalor);
					__fpurge(stdin);
					if(novovalor < 0.1 || novovalor > 3){
						printf("Valor deve estar em 0.1 e 3! Novo valor:");
					}else{
						pthread_mutex_lock(&em_href);
						setpoint_niv = novovalor;
						pthread_mutex_unlock(&em_href);
						break;
					}
				}						
			}else if (escolha == 't'){
				printf("Qual o novo valor?");
				while(1){
					scanf("%f",&novovalor);
					__fpurge(stdin);
					if(novovalor < 0 || novovalor > 50){
						printf("Valor deve estar em 0 e 50! Novo valor:");
					}else{
						pthread_mutex_lock(&em_tref);
						setpoint_temp = novovalor;
						pthread_mutex_unlock(&em_tref);
						break;
					}
				}
			}else if (escolha == 'e'){
				system("clear");
				exit(0);
				break;
			}else if (escolha == 'k'){
				printf("Qual o novo valor de nivel?");
				while(1){
					scanf("%f",&novovalor);
					__fpurge(stdin);
					if(novovalor < 0.1 || novovalor > 3){
						printf("Valor deve estar em 0.1 e 3! Novo valor:");
					}else{
						pthread_mutex_lock(&em_href);
						setpoint_niv = novovalor;
						pthread_mutex_unlock(&em_href);
						break;
					}
				}
				printf("Qual o novo valor de temperatura?");
				while(1){
					scanf("%f",&novovalor);
					__fpurge(stdin);
					if(novovalor < 0 || novovalor > 50){
						printf("Valor deve estar em 0 e 50! Novo valor:");
					}else{
						pthread_mutex_lock(&em_tref);
						setpoint_temp = novovalor;
						pthread_mutex_unlock(&em_tref);
						break;
					}
				}			
			}
		pthread_mutex_unlock(&em_escrita);
		}
		else{
		
		}
	}
	pthread_mutex_unlock(&em_escrita);
}


void controle_temperatura(){
	struct timespec t;
	double Kp_temp, erro_temp, controle_temp;
	float T, Ta, Ti, H, erroT, C;
	float Qc;
	int t0;
	Kp_temp = 500000;
	float st;
	long int interval = 50000000; /* 50ms*/
	/* start after one second */	
    	clock_gettime(CLOCK_MONOTONIC ,&t);
    	t.tv_sec++;
    	while(1) 
        {
        /* wait until next shot */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

        /* do the stuff */       
		
		T = lesensor("st-0");
		H = lesensor( "sh-0");
		Ti = lesensor("sti0"); 
		Ta = lesensor("sta0");
		pthread_mutex_lock(&em_tref);
		erro_temp = setpoint_temp - T;
		pthread_mutex_unlock(&em_tref);
		
		C = S*P*B*H;
		t0 = 10;
		Kp = C/t0 + 40;
		Qc = Kp*erro_temp;
		Qt = Q + Ni*S*(Ti-T) + Na*S*(80-T) - (T-Ta)/R;
	
		if(erro_temp > 0) {
			Q = MIN(1000000,Qc - (Qt - Q));
			Qu = Q;
			esfriando = 0;
		}
		else {
			Q = 0;
			esfriando = 1;
		}

		atuar("aq-",Q);	
                /* calculate next shot */
        t.tv_nsec += interval;

        while (t.tv_nsec >= NSEC_PER_SEC) 
        {
           t.tv_nsec -= NSEC_PER_SEC;
           t.tv_sec++;
        }
	}

}

void controle_nivel(){
	struct timespec t;
	double Kp_niv, erro_niv, controle_niv;
	Kp_niv = 1000;
	float sh;
	long int interval = 70000000; /* 70ms*/
	/* start after one second */	
   	 clock_gettime(CLOCK_MONOTONIC ,&t);
   	 t.tv_sec++;
   	 while(1) 
        {
        /* wait until next shot */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

        /* do the stuff */

		
		sh = lesensor("sh-0");
		pthread_mutex_lock(&em_href);
		erro_niv = setpoint_niv - sh;
		pthread_mutex_unlock(&em_href);
		controle_niv = Kp_niv*erro_niv;
		if(fabs(controle_niv)>100)
		{
			controle_niv = 100*controle_niv/fabs(controle_niv);
		}
		if(controle_niv>0)
		{
			atuar("ani",controle_niv);	
		}else{
			atuar("anf",fabs(controle_niv));
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

void bufferduplo(){
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
		pthread_mutex_lock(&em_buff);
		Temp = lesensor("st-0");
	
		if (buffer_selecionado == 0){
			buffer1[posicao] = Temp;
			posicao++;
		}
		else{
			buffer2[posicao] = Temp;
			posicao++;
		}
	
		if (posicao == TAM_MEU_BUFFER){
			descarrega_buffer = buffer_selecionado;
			buffer_selecionado = !buffer_selecionado;
			posicao = 0;
			buffer_cheio = 1;
			pthread_cond_signal(&buff);
		}
	
		pthread_mutex_unlock(&em_buff);
		t.tv_nsec += interval;

		while (t.tv_nsec >= NSEC_PER_SEC) {
	  		t.tv_nsec -= NSEC_PER_SEC;
	  		t.tv_sec++;
		}
   	}
}


void escreve_arquivo(){
	//static *FILE arquivo = fopen("","")
	while(1){
		pthread_mutex_lock(&em_buff);
		while(buffer_cheio == 0){
			pthread_cond_wait(&buff,&em_buff);
		}
		//pthread_mutex_lock(&em_buff);
		FILE* arquivo = fopen("Trab2.txt","a+");
	
		if (descarrega_buffer == 0){
			int i;
			for (i =0 ; i < TAM_MEU_BUFFER ; i++){
				char conteudo[20]="";
				sprintf(conteudo,"Leitura %ld: %f\n", ++leitura, buffer1[i]);
				fputs(conteudo,arquivo);
			}
		}else{
			int i;
			for (i =0 ; i < TAM_MEU_BUFFER ; i++){
				char conteudo[20]="";
				sprintf(conteudo,"Leitura %ld: %f\n", ++leitura, buffer2[i]);
				fputs(conteudo,arquivo);
			}
		}
		fclose(arquivo);
		buffer_cheio = 0;
		pthread_mutex_unlock(&em_buff); 
	}
}


int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		fprintf(stderr,"ERRO: Faltam parametros\n");
		fprintf(stderr,"Uso: endereço porta\n");
		fprintf(stderr,"Ex.: baker.das.ufsc.br 1234\n");
		exit(FALHA);
	}
	socket_local = cria_socket_local();
	endereco_destino = cria_endereco_destino(argv[1], atoi(argv[2]));
	system("clear");
	inicializacao();
	
	pthread_create(&temperatura, NULL, (void *) controle_temperatura, NULL);
	pthread_create(&nivel, NULL, (void *) controle_nivel, NULL);
	pthread_create(&tela, NULL, (void *) update_tela, NULL);
	pthread_create(&buffer,NULL,(void *) bufferduplo,NULL);
	pthread_create(&teclado, NULL, (void *) escreve_teclado, NULL);
	pthread_create(&escrevearquivo, NULL, (void *) escreve_arquivo, NULL);
	pthread_create(&alarme, NULL, (void *)toca_alarme, NULL);
	
	pthread_join(temperatura, NULL);
	pthread_join(nivel, NULL);
	pthread_join(tela, NULL);
	pthread_join(buffer,NULL);
	pthread_join(escrevearquivo,NULL);
	pthread_join(teclado,NULL);
	pthread_join(alarme,NULL);

}



