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
typedef struct {
	char login[21];
	char pass[21];
}FUSER;
union semun {
	int val;    /* Value for SETVAL */
	struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
	unsigned short  *array;  /* Array for GETALL, SETALL */
	struct seminfo  *__buf;  /* Buffer for IPC_INFO   (Linux-specific) */
};


// Variaveis Globais
int pipeFDr,pipeFDmsg,semaforo,semshm,shmid,pipeFDinfo,pipeFDcli;
int f=0,flag=0,nuser=1,i,clientes[MAXUSERS],nclis=0;
char pipename[12],pipewrite[12],info[30];
USER *users,reg;
struct sembuf pede,devolve;
union semun arg;

void guardausers(){
	FILE* f;
	FUSER aux;
	
	f=fopen("users.dat","wb");
	if(f==NULL){
		printf("Erro no acesso ao ficheiro\n");
		return;
	}
	else{	
		for(i=1;i<nuser;i++){
			strcpy(aux.login,users[i].login);
			strcpy(aux.pass,users[i].pass);
			fwrite(&aux,sizeof(FUSER),1,f);
		}
	fclose(f);
	}
}

void carregausers(){
	FILE* f;
	FUSER aux;
	f=fopen("users.dat","rb");

	if(f==NULL){
		printf("Erro no acesso ao ficheiro\n");
		return;
	}
	else{
		i=1;
		while((fread(&aux,sizeof(FUSER),1,f)==1)&&(i!=(MAXUSERS-1))){
			strcpy(users[i].login,aux.login);
			strcpy(users[i].pass,aux.pass);
			users[i].pid=0;
			users[i].servpid=0;
			i++;
		}
		nuser=i;
		for(i;i<MAXUSERS;i++){
			users[i].pid=-1;
			users[i].servpid=-1;
		}
		fclose(f);
	}
}



void desligar(int sinal){
	guardausers();
	for(i=0;i<nclis;i++)
		kill(clientes[i],SIGINT);
	sleep(5);
	fflush(stdout);
	semctl(semaforo,0,IPC_RMID);
	semctl(semshm,0,IPC_RMID);	
	close(pipeFDr);
	unlink(PIPESERVER);
	shmdt(users);
	shmctl(shmid, IPC_RMID, NULL);
	printf("[System] Server SHUTDOWN!\n");
	exit(0);	
}


void inicia(){	
	pede.sem_num=0;
	pede.sem_op=-1;
	pede.sem_flg=0;	
	devolve.sem_num=0;
	devolve.sem_op=1;
	devolve.sem_flg=0;
 	//************** cria semaforo************* **************
	if((semaforo=semget(SEMKEY, 1, IPC_CREAT | IPC_EXCL | 0777))==-1)	{
		  printf("[System] Servidor já se encontra em funcionamento...\n");
		  exit(1);	
        }
	semctl(semaforo, 0, SETVAL, 1);
	//***********cria semafro da memória**************************
	if((semshm=semget(SMKEY, 1, IPC_CREAT | IPC_EXCL | 0777))==-1)	{
		  printf("[System] Semaforo da memoria em funcionamento...\n");
		  exit(1);	
        }
	arg.val=1;
	if(semctl(semshm,0,SETVAL,arg) == -1){
     		perror("[System] Erro na inicialização do semáforo....");
     		exit(1);
  	}
	//************** cria PIPE ******************************
	if((mkfifo(PIPESERVER, O_CREAT|O_EXCL|0777))<0){
		printf("[System] Erro a criar pipe...\n");
		desligar(0);
	}
	if((pipeFDr= open(PIPESERVER, O_RDONLY|O_NONBLOCK)) == -1){
		printf("[System] Erro a abrir pipe...\n");
		desligar(0);
	}
	//******************Memoria partilhada********************
	if((shmid = shmget(SHMKEY,MAXUSERS*sizeof( USER), IPC_CREAT|IPC_EXCL|0777)) == -1){
		printf("[System] Erro a pedir segmento de memória\n");
		desligar(0);
	}
 	if((users=(USER *)shmat(shmid, NULL, 0)) == (USER *)-1){
		printf("[System] Erro a mapear memória\n");
		desligar(0);
	}
	strcpy(users[0].login,"servidor\0");
	strcpy(users[0].pass,"servidor\0");
	users[0].pid=getpid();
	users[0].servpid=0;
	carregausers();	
	system("rm users.dat");
}
void atendecliente(){
	MSG msg;
	sprintf(pipewrite,"write%d",reg.pid);
	if((pipeFDmsg = open(pipewrite, O_RDONLY))<0){
		printf("[System] Erro a abrir pipe para mensagens...\n");		
		exit(-1);
	}
	read(pipeFDmsg, &msg, sizeof(MSG));
	if(0==strcmp(msg.destino,"all")){
		i=1;
		while(users[i].pid>=0){	
			if(users[i].pid>0){
				if(0!=strcmp(msg.remetente,users[i].login)){
					sprintf(pipename,"read%d",users[i].pid);
					pipeFDcli = open(pipename, O_WRONLY|O_NONBLOCK);
					write(pipeFDcli, &msg, sizeof(MSG));
					kill(users[i].pid,SIGUSR2);
					close(pipeFDcli);
					f=1;
				}
			}
			i++;
		}		
		if(f==0){
			f=3;
			strcpy(info,"[server] Ninguem online\n\0");
		}			
	}else{
		i=1;
		while(users[i].pid>=0){
			if(0==strcmp(msg.destino,users[i].login)){
				if(users[i].pid>0){
					sprintf(pipename,"read%d",users[i].pid);
					pipeFDcli = open(pipename, O_WRONLY|O_NONBLOCK);
					write(pipeFDcli, &msg, sizeof(MSG));
					kill(users[i].pid,SIGUSR2);
					close(pipeFDcli);
					f=1;
				}else{
					strcpy(info,"[server] User offline\n\0");
					f=3;
				}
			}
			i++;
		}

	}
	if(f==0){
		strcpy(info,"[server] User nao existe\n\0");
		f=3;
	}
	close(pipeFDmsg);
	i=1;
	if(f==3)
	while(users[i].pid>=0){	
		i++;
		if(0==strcmp(msg.remetente,users[i].login)){
			sprintf(pipename,"read%d",users[i].pid);
			pipeFDcli = open(pipename, O_WRONLY|O_NONBLOCK);
			write(pipeFDcli, &info, sizeof(info));
			close(pipeFDcli);
			kill(users[i].pid,SIGUSR1);
		}
	}
}


void logoff(int signal){
	read(pipeFDr, &reg, sizeof(USER));
	sprintf(pipename,"read%d",reg.pid);
	pipeFDinfo = open(pipename, O_WRONLY);
	semop(semshm,&pede,1);
	if(strcmp(reg.pass,"log")==0){
		for(i=1;i<nuser;i++)
			if((0==strcmp(reg.login,users[i].login))&&(users[i].servpid!=0)){
				strcpy(info,"[server]user logoff\n\0");
				write(pipeFDinfo, &info, sizeof(info));
				kill(users[i].pid,SIGUSR1);
				users[i].pid=0;	
				kill(users[i].servpid,SIGURG);
				users[i].servpid=0;
			}
	}else if(strcmp(reg.pass,"rem")==0){
		for(i=1;i<nuser;i++)
			if((0==strcmp(reg.login,users[i].login))&&(users[i].servpid!=0)){
				if(i==(nuser-1))				
					nuser--;
				else{
					users[i]=users[nuser-1];
					nuser--;
				}
				strcpy(info,"[server]user apagado\n\0");
				write(pipeFDinfo, &info, sizeof(info));
				kill(users[i].servpid,SIGURG);
				kill(users[i].pid,SIGUSR1);
				users[nuser-1].pid=-1;
				users[nuser-1].servpid=-1;
			}
	}
	semop(semshm,&devolve,1);
}
void logout(){
	exit(0);
}
void regista(){
	pid_t pid;
	read(pipeFDr, &reg, sizeof(USER));
	sprintf(pipename,"read%d",reg.pid);
	pipeFDinfo = open(pipename, O_WRONLY);
	semop(semshm,&pede,1);
	if((0!=strcmp(reg.login,"servidor"))&&(0!=strcmp(reg.login,"all"))){
		for(i=1;i<nuser;i++)
			if(0==strcmp(reg.login,users[i].login)){
				if(0==strcmp(reg.pass,users[i].pass)){
					if(users[i].pid>0){
						strcpy(info,"[server]user já em uso\n\0");
						f=3;
					}else{
						strcpy(info,"[server]user logado\n\0");
						users[i].pid=reg.pid;
						f=2;
						pid=fork();
						if(pid){
							users[i].servpid=pid;
						}else if(pid==0){
							reg.servpid=getpid();
							signal(SIGINT,  SIG_IGN);
							signal(SIGUSR1, SIG_IGN);
							signal(SIGUSR2, SIG_IGN);
							signal(SIGCHLD, SIG_IGN);
							signal(SIGURG, logout);	
							do{
								atendecliente();
							}while(1);
							exit(0);
						}else{
							printf("\n [System] Erro a criar fork...");
							exit(1);
						}
					}
				}else{
					strcpy(info,"[server]password errada\n\0");
					f=1;
				}
			}
		if (i==(MAXUSERS))
			strcpy(info,"[server]limite de users atingido\n\0");
		else if(f==0){
			users[nuser].pid=reg.pid;
			strcpy(users[nuser].login,reg.login);
			strcpy(users[nuser].pass,reg.pass);
			strcpy(info,"[server]novo user logado\n\0");
			nuser++;
			pid=fork();
			if(pid){
				users[i].servpid=pid;
			}else if(pid==0){
				reg.servpid=getpid();
				signal(SIGINT,  SIG_IGN);
				signal(SIGUSR1, SIG_IGN);
				signal(SIGUSR2, SIG_IGN);
				signal(SIGCHLD, SIG_IGN);
				signal(SIGURG, logout);				
				do{
					atendecliente();
				}while(1);
				exit(0);
			}else{
				printf("\n [System] Erro a criar fork...");
				exit(1);
			}
		}
	}else{
		strcpy(info,"[server]nick nao autorizado\n\0");
		
	}
	write(pipeFDinfo, &info, sizeof(info));
	close(pipeFDinfo);
	semop(semshm,&devolve,1);
}


void ghost(){
	int pid;
	read(pipeFDr, &pid, sizeof(int));
	sprintf(pipename,"read%d",pid);
	pipeFDinfo = open(pipename, O_WRONLY|O_NONBLOCK);
	if(pid>0){	
		clientes[nclis]=pid;
		nclis++;
		strcpy(info,"[system] Server online!\n\0");
		write(pipeFDinfo, &info, sizeof(info));
		kill(pid,SIGUSR1);
		close(pipeFDinfo);
	}else{
		pid=pid*(-1);
		for(i=0;i<nclis;i++)
			if(clientes[i]==pid){
				clientes[i]=clientes[nclis-1];
				nclis--;	
			}
	}	
}
void printstuff(){
	for(i=0;i<nuser;i++)
		printf("\n%s//%d//%d\n",users[i].login,users[i].pid,users[i].servpid);
	for(i=0;i<nclis;i++)
		printf("\n%d\n",clientes[i]);
}
void main(){	
	pid_t pid;

	pid=fork();
	if(pid){
		exit(0);
	}else if(pid==0){
		signal(SIGINT,  desligar);
		signal(SIGUSR1, ghost);
		signal(SIGUSR2, regista);
		signal(SIGCHLD, logoff);
		signal(SIGURG, printstuff);
		inicia();
		do{
			sleep(60);
		}while(1);
	}else{
		printf("\n [System] Erro a criar fork...");
		exit(1);
	}
	exit(0);
}
