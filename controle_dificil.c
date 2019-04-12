
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define BUF_SIZE 20
#define NSEC_PER_SEC    (1000000000) /* The number of nsecs per sec. */
#define S 4184
#define P 1000
#define B 4
#define R 0.001
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define LIMITE 10000


int prepara_socket_cliente(char *host, char *porta);
float leSensor(int socket, char *comando);
void atua(int socket, char *atuador, float valor);

int main(int argc, char *argv[])
{
	int socket_cliente;

	if (argc != 2) {
        	fprintf(stderr, "Usage: %s port\n", argv[0]);
        	exit(EXIT_FAILURE);
    	}

	socket_cliente = prepara_socket_cliente("127.0.0.1", argv[1]);

	struct timespec t;
        int interval = 10000000; /* 10ms*/
        //int interval = 1000000000;

        clock_gettime(CLOCK_MONOTONIC ,&t);
        /* start after one second */
        t.tv_sec++;
	
	// Pega a referencia de nivel e temperatura
	float href = 0, tref = 0;
	system("clear");
	printf("Insira a referencia de nivel: ");
	scanf("%f", &href);
	while (href > 3 || href < 0.1)
	{
		printf("Nível não tolerado. O nível deve estar entre 0.1m e 3m\n");
		printf("Insira a referencia de nivel: ");
		scanf("%f", &href);
	}
	printf("\nInsira a referencia de temperatura: ");
	scanf("%f", &tref);
	
	FILE* arquivo = fopen("leituras_temp.txt", "w+");
	fclose(arquivo);

	int ciclosT = 0; // Quantos ciclos de 10ms se passaram
	int ciclosN = 0;
	int ciclosTela = 0;
	int ciclosArmazena = 0;
	float Ta, T, Ti, No, H, Q, Qu, Ni, Nic, Na, Nf, Ki, Qc, Nt, erroT, erroH, integralErro, Natu, Qt;
	integralErro=0;
	int leitura = 0;
	
	struct timespec tInicio, tTemperatura, tNivel;
	unsigned long int temposTemperatura[LIMITE];
	unsigned long int temposNivel[LIMITE];
	int indexT = 0;
	int indexN = 0;
	while(1) {
		/* wait until next shot */
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

                /* do the stuff */
		
		/*
		1 - Verifica se passou o tempo necessário (90ms-T 70ms-N)
		2 - Lê sensores
		3 - Calcula C, Kp e Kd da temperatura
		4 - 
		*/
		
		clock_gettime(CLOCK_MONOTONIC ,&tInicio); // Adquire o tempo de início da execução do loop
		
		int esfriando = 0;
		int enchendo = 0;
		
		if(ciclosT == 3)
		{
			float Kp, Kd, Kni;
			T = leSensor(socket_cliente, "st-0");
			H = leSensor(socket_cliente, "sh-0");
			No = leSensor(socket_cliente, "sno0");
			Ti = leSensor(socket_cliente, "sti0");
			Ta = leSensor(socket_cliente, "sta0");


			float C = S*P*B*H;
			int t0 = 10; // Tempo de 67% em segundos
		
			erroT = tref - T;
			erroH = href - H;
			Kp = C/t0 + 40;
			Ki = 10;
			Kni = 100*erroH;
			integralErro += erroT*0.03;
			
			Qc = Kp*erroT + Ki*integralErro;
			Nic = -50*erroT;
			Nt = (Ni + Na - Nf - No)/(B*P);

			// Qc =0;
			// Saturacoes dos atuadores:
			// Ni: 100
			// Na: 10
			// Nf: 100
			// Entrada maxima de agua: 110
			// Q: 1.000.000

			
			Qt = Q + Ni*S*(Ti-T) + Na*S*(80-T) - (T-Ta)/R; // Q atuando no sistema
			
			if(erroT > 0) // Deve-se aquecer
			{
				Q = MIN(1000000,Qc - (Qt - Q));
				Qu = Q;
				Ni =0;
			} else {
				Q = 0;
				Ni = MIN(100,100*Nic);
				printf("entrou Ni");
			}
			if(Q < 0) {
				Q = 0;
			}
			if(erroH < 0.001 && erroT>0) {
				Na=0;
			//	Ni=0;
			//	Nf=0;
				Nf=MIN(100,Nt);
			} if (erroH > 0.001) {
				Ni = MIN(100, Kni*Nic);
			}
			if (erroH < 0 && erroT < 0){
				Nf=MIN(100,4000*Nt);
			}



			atua(socket_cliente, "aq-", Q);
			atua(socket_cliente, "ani", Ni);
			atua(socket_cliente, "ana", Na);
			atua(socket_cliente, "anf", Nf);
			ciclosN = 0;
			ciclosT = 0;
			
			clock_gettime(CLOCK_MONOTONIC ,&tTemperatura); // Adquire o tempo ao fim da execução do bloco de controle de temperatura
			
			if(indexT < LIMITE){
				unsigned long int nanoSecsTemperatura = (tTemperatura.tv_sec*1000000000 + tTemperatura.tv_nsec) - (tInicio.tv_sec*1000000000 + tInicio.tv_nsec);
				temposTemperatura[indexT++] = nanoSecsTemperatura;
			}

			float Ki;
			Kp = 1000;
			Ki = 10;
			
			// // Nt = Kp*erroH + Ki*integralErro;
			// integralErro += erroH*0.07;
			
			// // Nt = Ni + Na - Nf - No
			// // No eh perturbacao
			// Natu = Ni + Na - Nf - No;
			
			// if(erroH > 0) // Deve-se encher
			// {
			// 	enchendo = 1;
			// 	Ni = MIN(100, Nt-Natu);
			// 	Nf = 0;
			// 	Natu = Ni + Na - Nf - No;
			// 	if(esfriando == 0)
			// 	{
			// 		// Ativa Na;
			// 		if(Natu == Nt){
			// 			if(Ni <= 10)
			// 			{
			// 				Na = Ni;
			// 				Ni = 0;
			// 			}
			// 			else {
			// 				Ni -= 10;
			// 				Na = 10;
			// 			}
			// 			Natu = Ni + Na - Nf - No;
			// 		} else {
			// 			Na = MIN(10, Natu);
			// 			Natu = Ni + Na - Nf - No;
			// 		}
			// 	} else Na = 0;
				
			// } else if(erroH < 0){ // Deve-se esvaziar
			// 	enchendo = 0;
			// 	Na = 0;
			// 	Ni = No;
			// 	Nf = MIN(100, Natu);
			// 	Natu = Ni + Na - Nf - No;
			// 	integralErro = 0;
			// } else {
			// 	enchendo = 0;
			// 	Na = 0;
			// 	Ni = 0;
			// 	Nf = 0;
			// 	Natu =  -No;
			// }
			
			// Saturacoes dos atuadores:
			// Ni: 100
			// Na: 10
			// Nf: 100
			// Entrada maxima de agua: 110
			// Q: 1.000.000

			
			clock_gettime(CLOCK_MONOTONIC ,&tNivel); // Adquire o tempo ao fim da execução do bloco de controle de nível
			
			if(indexN < LIMITE){
				unsigned long int nanoSecsNivel = (tNivel.tv_sec*1000000000 + tNivel.tv_nsec) - (tInicio.tv_sec*1000000000 + tInicio.tv_nsec);
				temposNivel[indexN++] = nanoSecsNivel;
			}
		}
		
		
		
		if(ciclosTela == 100)
		{
			system("clear");
			system("clear");
			printf("================== CONTROLE DO AQUECEDOR ==================\n\n");
			printf("  H_ref = %f\t\tT_ref = %f\n\n", href, tref);
			printf("  Valores atuais:\n");
			printf("  H: %f\t\t\tT: %f\n", H, T);
			printf("  Ta: %f\t\t\tNa: %f\n", Ta, Na);
			printf("  Ti: %f\t\t\tNi: %f\n", Ti, Ni);
			printf("  No: %f\t\t\tNf: %f\n", No, Nf);
			printf("  Q: %f\n", Q);
			printf("  Qu: %f\n", Qu);
			printf("  ErroH: %f\n", erroH);
			printf("  ErroT: %f\n", erroT);
			printf("  Nic: %f\n", Nic);

			printf("\n===========================================================");

			fflush(stdout);
			
			ciclosTela = 0;
		}
		
		if(ciclosArmazena == 500) // 5s a cada armazenagem de dados
		{
			FILE* arquivo = fopen("leituras_temp.txt", "a+");
			char conteudo[20];
			sprintf(conteudo, "Leitura %d: %f\n", ++leitura, T);
			fputs(conteudo, arquivo);
			fclose(arquivo);
			
			ciclosArmazena = 0;
		}


                /* calculate next shot */
                //clock_gettime(CLOCK_MONOTONIC ,&t);
                t.tv_nsec += interval;

                while (t.tv_nsec >= NSEC_PER_SEC) {
                       t.tv_nsec -= NSEC_PER_SEC;
                        t.tv_sec++;
                }
		ciclosT++;
		ciclosN++;
		ciclosTela++;
		ciclosArmazena++;
		
		
		if(indexT == LIMITE && indexN == LIMITE) break;
   	}
	
	// Grava os arquivos com os tempos
	arquivo = fopen("tempos_temperatura.txt", "a+");
	int i;
	for(i = 0; i<LIMITE; i++)
	{
		fprintf(arquivo, "%lu ", temposTemperatura[i]);
	}
	fclose(arquivo);
	
	arquivo = fopen("tempos_nivel.txt", "a+");
	for(i = 0; i<LIMITE; i++)
	{
		fprintf(arquivo, "%lu ", temposNivel[i]);
	}
	fclose(arquivo);
}

int prepara_socket_cliente(char *host, char *porta)
{ 
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s;

    /* Obtain address(es) matching host/port */

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    s = getaddrinfo(host, porta, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address. */

	for( rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;                  /* Success */

        close(sfd);
    }

    if (rp == NULL) {               /* No address succeeded */
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);           /* No longer needed */

	return sfd;
}

float leSensor(int socket, char *comando)
{
	// Esta função supõe que apenas comando de leitura de sensores e valores são enviados, e não comandos de atuadores
	ssize_t len;
    	ssize_t nread;
	char buf[BUF_SIZE];
	float valor;
	int i;
	for(i = 0; i < BUF_SIZE; i++)
	{
		buf[i] = '\0';
	}
	len = strlen(comando) + 1;
	
	//Escreve o comando de leitura
	if (write(socket, comando, len) != len){
            fprintf(stderr, "partial/failed write\n");
            exit(EXIT_FAILURE);
    	}
	
	//Recebe a resposta
	nread = read(socket, buf, BUF_SIZE);
    	if (nread == -1) {
		perror("read");
        exit(EXIT_FAILURE);
    	}
	
	// Trata a resposta
	char substring[BUF_SIZE];
	strncpy(substring, buf+3, BUF_SIZE);
	
	valor = atof(substring);

	return valor;
}

void atua(int socket, char *atuador, float valor)
{
	// Esta função supõe que apenas comando de leitura de sensores e valores são enviados, e não comandos de atuadores
	ssize_t len;
    	ssize_t nread;
	char buf[BUF_SIZE];
	int i;
	for(i = 0; i < BUF_SIZE; i++)
	{
		buf[i] = '\0';
	}
	char comando[30];
	sprintf(comando, "%s%f", atuador, valor);
	len = strlen(comando) + 1;
	
	//Escreve o comando de leitura
	if (write(socket, comando, len) != len){
            fprintf(stderr, "partial/failed write\n");
            exit(EXIT_FAILURE);
    }
	
	//Recebe a resposta
	nread = read(socket, buf, BUF_SIZE);
    if (nread == -1) {
		perror("read");
        exit(EXIT_FAILURE);
    }
}