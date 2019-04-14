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

#define FALHA 1
#define	TAM_MEU_BUFFER	1000
#define NSEC_PER_SEC    (1000000000) // The number of nsecs per sec
#define num_ciclos 100000
#define S 4184
#define P 1000
#define B 4
#define R 0.001
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define Href 2

int porta_destino;
int socket_local;
int t0;
struct sockaddr_in endereco_destino;
float T, Ta, Ti;
float No, Nt, Ni, Nf, Na;
float Q, Qc, Qu, Qt;
float H, C;
float Tref, erroT, erroH;
float Kp, Ki, Kni;

char msg_enviada[TAM_MEU_BUFFER];
char valor_recebido[TAM_MEU_BUFFER];
unsigned long int tempo[num_ciclos];
int values;

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
float setaValores(char *string, float ganho)
{
	double leitura_obtida;
	char mensagem_controle[20];

	sprintf(mensagem_controle,"%s%.1f",string, ganho);
	envia_mensagem(socket_local, endereco_destino, mensagem_controle);
	values = recebe_mensagem(socket_local, valor_recebido, TAM_MEU_BUFFER);

	leitura_obtida = atof(valor_recebido + 3);
	return leitura_obtida;
}

float leValores(char *string)
{
	double leitura_obtida;

	envia_mensagem(socket_local,endereco_destino,string);
	values = recebe_mensagem(socket_local,valor_recebido,TAM_MEU_BUFFER);

	leitura_obtida = atof(valor_recebido + 3);
	return leitura_obtida;
}

void display()
{
	system("clear");
	system("clear");
	printf("Controle de uma Caldeira\n\n");
	printf("  H_ref = %d\t\n", Href);
	printf("  Tref = ");
	printf("%.2f\n", Tref);
	printf("  Valores lidos e setados no ciclo");
	printf("  H: %f\t\t\tT: %f\n", H, T);
	printf("  Ta: %f\t\t\tNa: %f\n", Ta, Na);
	printf("  Ti: %f\t\t\tNi: %f\n", Ti, Ni);
	printf("  No: %f\t\t\tNf: %f\n", No, Nf);
	printf("  Q: %f\n", Q);
	printf("  ErroT %f\n", erroT);
	printf("  Qt %f\n",Qt);
	printf("\n===========================================================");
}

//Main
int main(int argc, char *argv[])
{
	//Verificação
	if (argc < 2) {
		fprintf(stderr,"Uso: t1 endereço porta \n");
		fprintf(stderr,"onde o endereço é o endereço do simulador \n");
		fprintf(stderr,"porta é o número da porta do simulador \n");
		fprintf(stderr,"exemplo de uso:\n");
		fprintf(stderr,"t1 localhost 9000 \"algo\"\n");
		exit(FALHA);
	}

	//Variaveis
	int i = 0;
	porta_destino = atoi(argv[2]);
	socket_local = cria_socket_local();
	endereco_destino = cria_endereco_destino(argv[1], porta_destino);
	char mensagem_iniciar[1000];
	struct timespec t,t1;
	long int interval = 10000000; /* 10ms*/
	int tela_times = 0;
	int contador = 0;
	int ciclosT = 0;


	printf("Controle de Temperatura\n");
	printf("Defina o valor de temperatura desejado (min:0;max:50): \t");
	scanf("%f",&Tref);

	if (Tref < 0){
		Tref = 0.0;
	}
	else if (Tref > 50){
		Tref = 50.0;
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
		Ta = leValores("sta0"); //"sto0" lê valor de Ta
		T =  leValores("st-0");  //"st-0" lê valor de T
		Ti = leValores("sti0"); //"sti0" lê valor de Ti
		No = leValores("sno0"); //"sno0" lê valor de No
		H =   leValores("sh-0"); //"sh-0" lê valor de H

		erroT = Tref - T;
		erroH = Href -  H;
		C = S*P*B*H;
		t0 = 10;
		Kp = C/t0 + 40;
		Qc = Kp*erroT;
		Qt = Q + Ni*S*(Ti-T) + Na*S*(80-T) - (T-Ta)/R; // Q atuando no sistema

		if (ciclosT == 3) {
			if(erroT > 0) {
				Q = MIN(1000000,Qc - (Qt - Q));
				Qu = Q;
				Ni = 0;
				Nf = 0;
			}
			else {
				Ni = 10;
				Nf = 5;
				Q = 0;
				if(erroH > 0.05){
					Nf = 20;
					Ni = 12;
				}
				if(erroH < 0.05){
					Ni = 15;
					Nf = 10;
				}
		}

		//setaValores Controle
		setaValores("aq-",Q);
		setaValores("ani", Ni);
		setaValores("ana", Na);
		setaValores("anf", Nf);

		ciclosT = 0;
  }

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
		ciclosT++;

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
