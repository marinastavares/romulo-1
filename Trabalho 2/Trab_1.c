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
#include <unistd.h>
#include <pthread.h>

#define FALHA 1
#define	TAM_MEU_BUFFER	1000
#define NSEC_PER_SEC    (1000000000) // The number of nsecs per sec
#define contador_ciclos 20000
#define S 4184
#define P 1000
#define B 4
#define R 0.001
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int portaDestino;
int socketLocal;
int t0;
struct sockaddr_in enderecoDestino;
float T, Ta, Ti;
float No, Nt, Ni, Nf, Na;
float Q, Qc, Qu, Qt;
float H, C;
float Tref, erroT, erroH;
float Href, erroH;
float Kp, Ki, Kni;

char mensagemEnviada[TAM_MEU_BUFFER];
char valorRecebido[TAM_MEU_BUFFER];
unsigned long int tempo[contador_ciclos];
int valores;

pthread_t thread_temperatura;

pthread_mutex_t mutex_temperatura = PTHREAD_MUTEX_INITIALIZER;

//Funções Dadas:
int cria_socketLocal(void){
	int socketLocal;		/* Socket usado na comunicacão */

	socketLocal = socket( PF_INET, SOCK_DGRAM, 0);
	if (socketLocal < 0) {
		perror("socket");
		return 0;
	}
	return socketLocal;
}

struct sockaddr_in cria_endereco_destino(char *destino, int portaDestino){
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
	servidor.sin_port = htons(portaDestino);

	return servidor;
}

void envia_mensagem(int socketLocal, struct sockaddr_in enderecoDestino, char *mensagem){
	/* Envia msg ao servidor */

	if (sendto(socketLocal, mensagem, strlen(mensagem)+1, 0, (struct sockaddr *) &enderecoDestino, sizeof(enderecoDestino)) < 0 )
	{
		perror("sendto");
		return;
	}
}

int recebe_mensagem(int socketLocal, char *buffer, int TAM_BUFFER){
	int bytes_recebidos;		/* Número de bytes recebidos */

	/* Espera pela msg de resposta do servidor */
	bytes_recebidos = recvfrom(socketLocal, buffer, TAM_BUFFER, 0, NULL, 0);
	if (bytes_recebidos < 0)
	{
		perror("recvfrom");
	}

	return bytes_recebidos;
}

float setaValores(char *string, float novoValor)
{
	double leituraObtida;
	char mensagemControle[20];

	sprintf(mensagemControle,"%s%.1f",string, novoValor);
	envia_mensagem(socketLocal, enderecoDestino, mensagemControle);
	valores = recebe_mensagem(socketLocal, valorRecebido, TAM_MEU_BUFFER); // por nsec

	leituraObtida = atof(valorRecebido + 3);
	return leituraObtida;
}

float leValores(char *string)
{
	double leituraObtida;

	envia_mensagem(socketLocal,enderecoDestino,string);
	valores = recebe_mensagem(socketLocal,valorRecebido,TAM_MEU_BUFFER);

	leituraObtida = atof(valorRecebido + 3);
	return leituraObtida;
}

void mostraTela()
{
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
	printf("\n");
}

void controle_temperatura(void) {

	Ta = leValores("sta0"); //"sto0" lê valor de Ta
	T =  leValores("st-0");  //"st-0" lê valor de T
	Ti = leValores("sti0"); //"sti0" lê valor de Ti
	H =   leValores("sh-0"); //"sh-0" lê valor de H

	pthread_mutex_lock(&mutex_temperatura);
	erroT = Tref - T;
	pthread_mutex_unlock(&mutex_temperatura);

	C = S*P*B*H;
	t0 = 10;
	Kp = C/t0 + 40;
	Qc = Kp*erroT;
	Qt = Q + Ni*S*(Ti-T) + Na*S*(80-T) - (T-Ta)/R;

	if(erroT > 0) {
		Q = MIN(1000000,Qc - (Qt - Q));
		Qu = Q;
	}
	else {
		Q = 0;
	}

	setaValores("aq-",Q);
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
		fprintf(stderr,"t1 localhost 9000 \"algo\"\n");
		exit(FALHA);
	}

	//Variaveis
	int i = 0;
	portaDestino = atoi(argv[2]);
	socketLocal = cria_socketLocal();
	enderecoDestino = cria_endereco_destino(argv[1], portaDestino);
	char mensagem_iniciar[1000];
	struct timespec t,t1;
	long int intervalo = 10000000; /* 10ms*/
	int contadorTela = 0;
	int contador = 0;
	int ciclosT = 0;


	printf("Controle de Temperatura e Nível\n");
	printf("Defina o valor de temperatura desejado (min:0;max:50): \t");
	scanf("%f",&Tref);

	if (Tref < 0){
		Tref = 0.0;
		printf("Somente valores positivos, setado para 0");
	}
	else if (Tref > 50){
		Tref = 50.0;
		printf("Somente valores até 50, setado para 50");
	}

	printf("Defina o valor de altura desejado (min:0;max:3): \t");
	scanf("%f",&Href);

	if (Href < 0){
		Tref = 0.0;
		printf("Somente valores positivos, setado para 0");
	}
	else if (Href > 3){
		Href = 3.0;
		printf("Somente valores até 3, setado para 3");
	}

	//Temporização
	clock_gettime(CLOCK_MONOTONIC ,&t);
	/* start after one second */
    t.tv_sec++;

    pthread_create(&thread_temperatura, NULL, (void *) controle_temperatura, NULL);


	//Principal
	while(1)
	{
		/* wait until next shot */
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

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
		Qt = Q + Ni*S*(Ti-T) + Na*S*(80-T) - (T-Ta)/R;

		if (ciclosT == 5) {

			pthread_join(thread_temperatura, NULL);

			ciclosT = 0;

		// Anotação do tempo de resposta
			clock_gettime(CLOCK_MONOTONIC ,&t1);
			if(contador < contador_ciclos)
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
  		}

		if(contadorTela == 0 || contadorTela >= 100)
		{
			mostraTela();
			contadorTela = 0;
		}
		contadorTela++;
		ciclosT++;

		t.tv_nsec += intervalo;

		while (t.tv_nsec >= NSEC_PER_SEC){
           t.tv_nsec -= NSEC_PER_SEC;
           t.tv_sec++;
        }
	}

	system("clear");
	printf("****\n");
	printf("*Simulacao Completa!*\n");
	printf("****\n");

	FILE *fp = fopen("temposobrecarga.csv","w");
	for(i=0;i<contador_ciclos;i++)
	{
		fprintf(fp, "%lu\n", tempo[i]);
	}
	fclose(fp);

	printf("Valores de tempo: \'temposobrecarga.csv\'\n");
	system("echo \"\033[12;1H\"");
}
