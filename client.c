#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PIPESERVER "pipe"
#define MAXUSERS 30
#define SHMKEY	 8879
#define SMKEY 5678
#define SEMKEY 1234

typedef struct {
	char destino[21];
	char texto[30];
	char remetente[21];
}MSG;
typedef struct {
	char login[21];
	char pass[21];
	pid_t pid,servpid;
}USER;

// Variaveis Globais
int pipeFDserv,semaforo,semshm,shmid,pipeFDinfo,pipeFDwrite;
int i,si,uslog;
char piperead[12],pipewrite[12],last[30],info[30],str[61],self[21],comand[8],dest[21],text[30];
USER *users,reg;
struct sembuf pede,devolve;



void editstring(){
	i=0;
	while((str[i]!=' ') && (str[i]!='\0')){
		comand[i]=str[i];
		i++;
	}
	comand[i]='\0';
	si=0;
	i++;
	while((str[i]!=' ') && (str[i]!='\0')){
		dest[si]=str[i];
		i++;
		si++;
	}
	dest[si]='\0';
	si=0;
	i++;
	while(str[i]!='\0'){
		text[si]=str[i];
		i++;
		si++;
	}
	text[si]='\0';
}
void fazlogin(){
	reg.pid=getpid();
	strcpy(reg.login,dest);
	strcpy(reg.pass,text);
	pipeFDserv = open(PIPESERVER, O_WRONLY|O_NONBLOCK);
	write(pipeFDserv, &reg, sizeof(USER));
	close(pipeFDserv);	
	kill(users[0].pid,SIGUSR2);
	
	sleep(3);
	strcpy(last,info);
	read(pipeFDinfo, &info, sizeof(info));
	printf("\n%s\n",info);
	if((0!=strcmp(info,last))&&(0!=strcmp(info,""))&&(0!=strcmp(info,"[server]password errada\n\0"))&&(0!=strcmp(info,"[server]nick nao autorizado\n\0"))&&(0!=strcpy(info,"[server]user já em uso\n\0"))&&(0!=strcpy(info,"[server]limite de users atingido\n\0"))){
		strcpy(self,dest);
		uslog=1;
	}
}
void logoff(char op[21]){
	strcpy(reg.login,self);
	strcpy(reg.pass,op);
	
	pipeFDserv = open(PIPESERVER, O_WRONLY|O_NONBLOCK);
	write(pipeFDserv, &reg, sizeof(USER));
	close(pipeFDserv);	
	kill(users[0].pid,SIGCHLD);
	sleep(2);
	uslog=0;
}
void userslogados(){
	i=1;
	if(uslog==1){
		printf("User's online:\n\n");
		while((users[i].pid>=0)){
			if((users[i].pid>0)){
				if(0!=strcmp(users[i].login,self))
					printf("  [%s]\n",users[i].login);
			}		
			i++;
		}
		printf("\n");
	}else
		printf("[system] Nao esta logado\n");
}
void sendmsg(){
	MSG msg;
	if(0==strcmp(dest,self)){
		printf("[system] quem fala sozinho é maluco :P\n");
		return;
	}
	strcpy(msg.remetente,self);
	strcpy(msg.destino,dest);
	strcpy(msg.texto,text);
	pipeFDwrite = open(pipewrite, O_WRONLY);
	write(pipeFDwrite, &msg, sizeof(MSG));
	close(pipeFDwrite);
}
void mostracomandos()
{
	printf("[System] Comandos\n\n who ---------------------------------- Mostrar os utilizadores logados\n login <username> <password> ---------- Fazer login/ registar utilizador\n logoff ------------------------------- Fazer logoff\n remove ------------------------------- Remover-se do sitema\n write <All/destino> <mensagem> ------- Enviar mensagem\n clear -------------------------------- Limpar tela\n exit --------------------------------- Sair do programa\n");
}

void tratacomando(){
	if(0==strcmp(comand,"clear\0"))
		system("clear");
	if(0==strcmp(comand,"login\0"))
		if(uslog==1)
			printf("\n[system] Utilizador já logado\n");
		else
			fazlogin();
	if(0==strcmp(comand,"who\0"))
		userslogados();
	if(0==strcmp(comand,"logoff\0"))
		if(uslog==1)
			logoff("log");
		else
			printf("\n[system] Utilizador nao logado\n");	
	if(0==strcmp(comand,"remove\0"))
		if(uslog==1)
			logoff("rem");
		else
			printf("\n[system] Utilizador nao logado\n");
	if(0==strcmp(comand,"write\0"))
		if(uslog==1)
			sendmsg();
		else
			printf("\n[system] Fazer Login\n");
	if(0==strcmp(comand,"help\0"))
		mostracomandos();
		
}

void getcomando(){
	char rsp;
	do{
		do{
			if(uslog==1){
				printf("[%s]>> ",self);
			}else
				printf(">> ");			
			gets(str);
			editstring();
			tratacomando();		
		}while(0!=strcmp("exit\0",str));
		printf("\n tem a certeza? ('s' ou 'S' para sim)\n:");
		scanf("%c",&rsp);
	}while((rsp!='s')&&(rsp!='S'));	
	if(uslog==1)
		logoff("log");
}

void inicia(){
	uslog=0;
	if((semaforo=semget(SEMKEY,1,0))==-1){		
		printf("\n[System] Servidor não esta a correr!!!\n");
		exit(-1);
	}
	sprintf(piperead,"read%d",getpid());
	if((mkfifo(piperead, O_CREAT|O_EXCL|0777))<0){
		printf("[System] Erro a criar pipe...\n");
		exit(-1);
	}
	if((pipeFDinfo= open(piperead, O_RDONLY|O_NONBLOCK))<0){
		printf("[System] Erro a abrir pipe...\n");		
		exit(-1);
	}
	if((shmid = shmget(SHMKEY,MAXUSERS*sizeof( USER), 0)) == -1){
		printf("[System] Erro a pedir segmento de memória\n");
		exit(-1);
	}
	if((users=(USER *) shmat(shmid,0 ,0))== (USER *)-1){
		printf("[System] Erro a mapear memória\n");
		exit(-1);
	}
	sprintf(pipewrite,"write%d",getpid());
	if((mkfifo(pipewrite, O_CREAT|O_EXCL|0777))<0){
		printf("[System] Erro a criar pipe para mensagens...\n");
		exit(-1);
	}
	i=getpid();
	pipeFDserv=open(PIPESERVER, O_WRONLY|O_NONBLOCK);	
	write(pipeFDserv, &i, sizeof(int));		
	close(pipeFDserv);
	kill(users[0].pid,SIGUSR1);
}	



void desliga(int signal){
	if(uslog==1)
		logoff("log");
	i=getpid();
	i=i*(-1);
	pipeFDserv=open(PIPESERVER, O_WRONLY|O_NONBLOCK);	
	write(pipeFDserv, &i, sizeof(int));
	close(pipeFDserv);
	kill(users[0].pid,SIGUSR1);
	close(pipeFDwrite);
	unlink(pipewrite);
	shmdt(users);
	close(pipeFDinfo);
	unlink(piperead);
	exit(0);	
}

void readinfo(int signal){
	read(pipeFDinfo, &info, sizeof(info));
	printf("\n%s\n",info);

}
void readmsg(int signal){
	MSG msg;
	read(pipeFDinfo, &msg, sizeof(MSG));
	printf("\n[%s] diz:\n \t%s\n",msg.remetente,msg.texto);

}
void main(){
	signal(SIGINT,desliga);
	signal(SIGUSR1,readinfo);
	signal(SIGUSR2,readmsg);
	system("clear");
	inicia();
	
	getcomando();
	desliga(0);
}




