 //Bibliotecas
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <time.h>

//Defines
#define FALHA 1
#define	TAM_MEU_BUFFER	1000
#define NSEC_PER_SEC    (1000000000) // The number of nsecs per sec
#define num_ciclos 10000

//Variaveis
int porta_destino;
int socket_local;
struct sockaddr_in endereco_destino;
float st,sta,sti,sno;
double SPtemp;
double Etemp;
double Ctemp;
unsigned long int tempo[num_ciclos];

// Controle
double Ktemp = 500000.0;
double Ktemp_max = 1000000.0;
double Ktemp_min = 0.0;

char msg_enviada[TAM_MEU_BUFFER];
char msg_recebida[TAM_MEU_BUFFER];
int nrec;

//Funções Dadas:
int cria_socket_local(void){
	int socket_local;		/* Socket usado na comunicacão */

	socket_local = socket( PF_INET, SOCK_DGRAM, 0);
	if (socket_local < 0) {
		perror("socket");
		return 0;
	}
	return socket_local;
}

struct sockaddr_in cria_endereco_destino(char *destino, int porta_destino){
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

void envia_mensagem(int socket_local, struct sockaddr_in endereco_destino, char *mensagem){
	/* Envia msg ao servidor */

	if (sendto(socket_local, mensagem, strlen(mensagem)+1, 0, (struct sockaddr *) &endereco_destino, sizeof(endereco_destino)) < 0 )
	{
		perror("sendto");
		return;
	}
}

int recebe_mensagem(int socket_local, char *buffer, int TAM_BUFFER){
	int bytes_recebidos;		/* Número de bytes recebidos */

	/* Espera pela msg de resposta do servidor */
	bytes_recebidos = recvfrom(socket_local, buffer, TAM_BUFFER, 0, NULL, 0);
	if (bytes_recebidos < 0)
	{
		perror("recvfrom");
	}

	return bytes_recebidos;
}

//Funções Criadas:
float aplicar(char *string, float ganho)
{
	double valor_lido;
	char mensagem_controle[20];

	sprintf(mensagem_controle,"%s%.1f",string, ganho);
	envia_mensagem(socket_local, endereco_destino, mensagem_controle);
	nrec = recebe_mensagem(socket_local, msg_recebida, TAM_MEU_BUFFER);

	valor_lido = atof(msg_recebida + 3);
	return valor_lido;
}

float leitura(char *string)
{
	double valor_lido;

	envia_mensagem(socket_local,endereco_destino,string);
	nrec = recebe_mensagem(socket_local,msg_recebida,TAM_MEU_BUFFER);

	valor_lido = atof(msg_recebida + 3);
	return valor_lido;
}

void display()
{
	//"Informações na tela sobre a situação corrente"
	//system("clear");

	printf("\n** Operacao **\n");

	printf("SPtemp:\n");
	printf("%.2f\n",SPtemp);

	printf("Ctemp:\n");
	printf("%.2f\n",Ctemp);

	printf("Etemp:\n");
	printf("%.2f\n",Etemp);

	printf("Temp:\n");
	printf("%.2f\n",st);

	printf("Ta:\n");
	printf("%.2f\n",sta);

	printf("Ti:\n");
	printf("%.2f\n", sti);

	printf("No:\n");
	printf("%.2f",sno);

	printf("\n****\n");
}

//Main
int main(int argc, char *argv[])
{
	//Verificação
	if (argc < 3) {
		fprintf(stderr,"Uso: t1 endereço porta \n");
		fprintf(stderr,"onde o endereço é o endereço do simulador \n");
		fprintf(stderr,"porta é o número da porta do simulador \n");
		fprintf(stderr,"exemplo de uso:\n");
		fprintf(stderr,"t1 localhost 9000 \"ola\"\n");
		exit(FALHA);
	}

	//Variaveis
	int i = 0;
	porta_destino = atoi(argv[2]);
	socket_local = cria_socket_local();
	endereco_destino = cria_endereco_destino(argv[1], porta_destino);
	char mensagem_iniciar[1000];
	struct timespec t,t1;
	long int interval = 50000000; /* 50ms*/
//	long int interval = 1000000000; /* 1000ms*/
	int tela_times = 0;
	int contador = 0;

	//Iniciar
//	sprintf(mensagem_iniciar,"%s%-.1f","anf",0.0);
//	printf("%s \n",mensagem_iniciar); //Teste
//	envia_mensagem(socket_local, endereco_destino, mensagem_iniciar);
//
//	sprintf(mensagem_iniciar,"%s%.1f","ani",0.0);
//	printf("%s \n",mensagem_iniciar); //Teste
//	envia_mensagem(socket_local, endereco_destino, mensagem_iniciar);
//
//	sprintf(mensagem_iniciar,"%s%.1f","ana",0.0);
//	printf("%s \n",mensagem_iniciar); //Teste
//	envia_mensagem(socket_local, endereco_destino, mensagem_iniciar);
//
//	sprintf(mensagem_iniciar,"%s%.1f","aq-",0.0);
//	printf("%s \n",mensagem_iniciar); //Teste
//	envia_mensagem(socket_local, endereco_destino, mensagem_iniciar);

	printf("** T1 - Controle de Temperatura **\n");
	printf("Defina SP Temp (min:0;max:50): \t");
	scanf("%lf",&SPtemp);

	//Verificação de Input
	if (SPtemp < 0){
		SPtemp = 0.0;
	}
	else if (SPtemp > 50){
		SPtemp = 50.0;
	}

	//Temporização
	clock_gettime(CLOCK_MONOTONIC ,&t);
	/* start after one second */
    t.tv_sec++;

	//Principal
	while(1)
	{
		/* wait until next shot */
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

		//Leitura
		sta = leitura("sta0"); //"sto0" lê valor de Ta
		st = leitura("st-0");  //"st-0" lê valor de T
		sti = leitura("sti0"); //"sti0" lê valor de Ti
		sno = leitura("sno0"); //"sh-0" lê valor de Ta

		//Lei de Controle (Controlador Proporcional)
		Etemp = SPtemp - st;
		Ctemp = Ktemp * Etemp;

		//'Saturador'
		if(Ctemp > Ktemp_max)
		{
			Ctemp = Ktemp_max;
		}
		else if(Ctemp < Ktemp_min){
			Ctemp = Ktemp_min;
		}

		//Aplicar Controle
		aplicar("aq-",Ctemp);

		// Anotação do tempo de resposta
		clock_gettime(CLOCK_MONOTONIC ,&t1);
		if(contador < num_ciclos)
		{
			long sec = (t1.tv_sec - t.tv_sec);
			long nsec = (t1.tv_nsec - t.tv_nsec);

        	tempo[contador] = sec*NSEC_PER_SEC + nsec;
			contador++;
		}
		else
		{
			break;
		}

		if(tela_times == 0 || tela_times >= 100)
		{
			display();
			tela_times = 0;
		}
		tela_times++;

		/* calculate next shot */
		t.tv_nsec += interval;

		while (t.tv_nsec >= NSEC_PER_SEC){
           t.tv_nsec -= NSEC_PER_SEC;
           t.tv_sec++;
        }
	}

	system("clear");
	printf("****\n");
	printf("*Simulacao Completa!*\n");
	printf("Gerando dados!\n");
	printf("****\n");

	FILE *fp = fopen("dados.csv","w");
	for(i=0;i<num_ciclos;i++)
	{
		fprintf(fp, "%lu\n", tempo[i]);
	}
	fclose(fp);

	printf("Dados salvos: \'dados.csv\'\n");
	system("echo \"\033[12;1H\"");
}
