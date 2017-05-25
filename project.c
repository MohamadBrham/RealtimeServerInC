#include "stdio.h"
#include "stdlib.h"
#include "netinet/in.h"
#include "string.h"
#include "queue.h"
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include "fcntl.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include "signal.h"
#include <errno.h>
// Signals constatnts
#define NO_SIGNAL 0                   // no signal Constant
int Signal = NO_SIGNAL;               // will carry the value of the current signal recieved
#define SIG_OFF 2                     // shutdown  --> SIGTERM
#define SIG_UPDATE 1                  // update    --> SIGUSR1
#define SIG_RESET 3                   // reset     --> SIGHUP

//old signal handlers
void (*  old_handler_off)(int);       // old SIGTERM handler
void (*  old_handler_update)(int);    // old SIGUSR1 handler
void (*  old_handler_reset)(int);     // old SIGHUP handler

// main signals initializer
void mainSignalCreate();              //Set the handlers to the required functions for the child processes
// signals Handlers main process (BLOCK OTHERS)
void mainOffSignal();                 // shutdown signal handler  -> SIGTERM
void mainUpdateSignal();              // update signal handler    -> SIGUSR1
void mainResetSignal();               // reset signal handler     -> SIGHUP

// child signals initializers
void signalCreate();                  // set the handlers to the required signals for the main process (set and Block)
// signals Handlers childs (BLOCK NOTHING)
void resetSignal();                   // reset signal handler     -> SIGHUP
void updateSignal();                  // update signal handler    -> SIGUSR1
void offSignal();                     // shutdown signal handler  -> SIGTERM


// configurations, Socket, Workers,serve
void configuration();       // create the configuration file
void socketCreate();        // create the socket server
void setnonblocking(int);   // set the server socket as nonblocking fd
void createWorkers();       // create the worker processes
void waitCildProcess(int);  // wait the child processes
int serve(int);             // serve the connection


int send_page(int,FILE*);   // send the page if found
int send_not_found(int);    // send not found page

// define some global variables
int nbWorker;         // specify the number of worker processes
int maxConn;          // specify the number of max connections
int port;             // specify the server socket port number
char pagesPath[50];   // specify the page directory directory
int serverX;          // indicating the server socket
int *PIDs;	 		 // Store the pids of the workers

// semaphor operations
int semid;
struct sembuf acquire = {0,-1,SEM_UNDO};  // acquire semaphore struct
struct sembuf release = {0,1,SEM_UNDO};   // release semaphore struct


char catch[1024];            // simple catch buffer
char catch_name[50]="\0";    // page name in the catch

char rsp[1024] = "HTTP/1.1 200 ok\nDate: Mon, 27 Jul 2009 12:28:53 GMT\nServer: Apache/2.2.14 (Win32)\nLast-Modified: Wed, 22 Jul 2009 19:15:56 GMT\nContent-Type: text/html\nConnection: Closed\n\n\0";
char rsp_not_found[1024] = "HTTP/1.1 404 Not Found\nDate: Mon, 27 Jul 2009 12:28:53 GMT\nServer: Apache/2.2.14 (Win32)\nLast-Modified: Wed, 22 Jul 2009 19:15:56 GMT\nContent-Length: 88\nContent-Type: text/html\nConnection: Closed\n\n\0";
char rsp_err[1024] = "HTTP/1.1 500 Not Found\nDate: Mon, 27 Jul 2009 12:28:53 GMT\nServer: Apache/2.2.14 (Win32)\nLast-Modified: Wed, 22 Jul 2009 19:15:56 GMT\nContent-Length: 88\nContent-Type: text/html\nConnection: Closed\n\n\0";


int main(int argc, char const *argv[])
{
	//main process to be the server and its worker processes
    printf("%s %d\n","The main Process ID is: ",getpid() );

	//create semaphore to control the accept between processes
	semid = semget(500,1,IPC_CREAT|0666);
	semctl(semid,0,SETVAL,1);

  // get server configurations from the configuration file to the global variables
	configuration();

  // define array to carry childs ID's
  PIDs = (int*) malloc(sizeof(int)*nbWorker);

  //set the signal handlers for the main process ( server )
	mainSignalCreate();

  // create the server socket with the previous configurations
	socketCreate();
	printf("Main Process ID (SERVER): %d \n On Port: %d \n With # Workers : %d \nNo. of connections: %d\n",getpid(),port,nbWorker,maxConn);
	printf("Start Listening . .\n");

  // Create child processes
  createWorkers(); //create # of worker process as in config. file

	while(1){ //loop until an signal recevied

		if(Signal == SIG_OFF){ //if shutdown process received
		    exit(0);
		}
		if(Signal == SIG_RESET){ //if reset signal received
			Signal = NO_SIGNAL;
    }
		if(Signal == SIG_UPDATE){ //if update signal received
			Signal = NO_SIGNAL;
      //execvp("./pro");
		}
    // sleep 0.1 seconds (not to make the while loop intensive)
    sleep(0.1);
	}
 	return 0;
}
void mainUpdateSignal(){
	(void)signal(SIGUSR1,mainUpdateSignal);
	Signal = SIG_UPDATE;
	printf("The SIG is : %d\n", Signal);
	int i = 0;
	for(i = 0; i < nbWorker; i++){
		kill(PIDs[i], SIGUSR1);
    wait();                     // exit in order
  }
	sleep(1);
  int tmp = serverX;
  configuration();
  if(tmp != serverX){
      close(serverX);            // close the server socket
      printf("Wait 150 seconds until server restart ...\n");
      sleep(150);
      socketCreate();
  }else{
      printf("Main Process ID (SERVER): %d \n On Port: %d \n With # Workers : %d \nNo. of connections: %d\n",getpid(),port,nbWorker,maxConn);
      printf("Start Listening . .\n");
  }
  PIDs = (int*) malloc(sizeof(int)*nbWorker); // Store the new ID of the processes
  createWorkers();
}
void mainOffSignal(){

	(void)signal(SIGTERM,mainOffSignal);
	Signal = SIG_OFF;
	printf("The SIG is : %d\n", Signal);
	for(i = 0; i < nbWorker; i++){
    kill(PIDs[i], SIGTERM);
    wait();                     // exit in order
  }


    waitCildProcess(nbWorker); // wait until all workers finish
    close(serverX);            // close the server socket
    //free(PIDs);

}
void mainResetSignal(){


	(void)signal(SIGHUP,mainResetSignal);
	Signal = SIG_RESET;
	printf("The SIG is : %d\n", Signal);
	for(i = 0; i < nbWorker; i++){
		kill(PIDs[i], SIGHUP);
    wait();
  }
    close(serverX);            // close the server socket
    printf("Wait 60 seconds until server restart ...\n");
    sleep(60);

    //main();

	     execvp("./pro");

	//new remove BLOCKING signal
    //printf("Listening again to incoming Connections ...\n");
}
void mainSignalCreate(){

  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGHUP);
  sigaddset(&set, SIGUSR1);
  sigaddset(&set, SIGTERM);

	//_handler_update = signal (SIGUSR1, mainUpdateSignal);
  struct sigaction act_update;
  memset(&act_update,0,sizeof act_update);

  act_update.sa_mask = set;
  act_update.sa_handler = mainUpdateSignal;
  act_update.sa_flags = SA_RESTART;

  sigaction(SIGUSR1, &act_update, NULL);
  //old_handler_reset = signal (SIGTERM, mainOffSignal);
  struct sigaction act_term;
  memset(&act_term,0,sizeof act_term);

  act_term.sa_mask = set;
  act_term.sa_handler = mainOffSignal;
  act_term.sa_flags = SA_RESTART;

  sigaction(SIGTERM, &act_term, NULL);
	//old_handler_off = signal (SIGHUP, mainResetSignal);

  struct sigaction act;


  memset(&act,0,sizeof act);
  sigemptyset(&set);
  sigaddset(&set, SIGHUP);
  sigaddset(&set, SIGUSR1);
  sigaddset(&set, SIGTERM);
  act.sa_mask = set;
  act.sa_handler = mainResetSignal;
  act.sa_flags = SA_RESTART;

  sigaction(SIGHUP, &act, NULL);
}

// childs handlers
void updateSignal(){
	(void)signal(SIGUSR1,updateSignal);
	Signal = SIG_UPDATE;
}
void offSignal(){
	(void)signal(SIGTERM,offSignal);
	Signal = SIG_OFF;
}
void resetSignal(){
	(void)signal(SIGHUP,resetSignal);
	Signal = SIG_RESET;
}
void signalCreate(){
  /* set the signal handlers to the required functions
  */
	old_handler_update = signal (SIGUSR1, updateSignal);
	old_handler_reset = signal (SIGHUP, resetSignal);
	old_handler_off = signal (SIGTERM, offSignal);
}


void socketCreate(){ //create the TCP server socket
	if ((serverX = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // Create AF_INET Socket
		perror("socket");
		exit(1);
	}

	struct sockaddr_in srv; /* used by bind() */
	/* create the socket */
	srv.sin_family = AF_INET; /* use the Internet addr family */
	srv.sin_port = htons(port); // Set server port to 80
	srv.sin_addr.s_addr = htonl(INADDR_ANY);// set server to accept any connection

	if(bind(serverX, (struct sockaddr*) &srv, sizeof(srv)) < 0) { // Attach the socket to the program
		perror("bind");
		exit(2);
	}

	if(listen(serverX, 100000) < 0) { // Start listening to new connections
		perror("listen");
		exit(3);
	}
  // set the server socket to be nonblocking
	setnonblocking(serverX);  // set the server socket to NON BLOCKING descipter
}


void waitCildProcess(int num){ // wait for (num)processes to finish
	int i;
	for (i = 0; i < num; ++i)
	{
		wait(NULL);
	}
}

void configuration(){  // read the config. file

	FILE* config = fopen("config2.txt","rw"); // Open configuration file
	if(!config){
		printf("\nConfiguration File cannot be found\n");
		exit(1);
	}
	char buffer[1024];
	if(!fgets(buffer,1024,config)){ // Check if the file is empty
		printf("\nFile is Empty\n");
		exit(2);
	}
	if(!fscanf(config,"#Worker:%d\n",&nbWorker)){
		printf("Error : Worker number Invalid\n");
		exit(3);
	}
	if(!fscanf(config,"#Max Connection:%d\n",&maxConn)){
		printf("Error : Max Connection number Invalid\n");
		exit(4);
	}
	if(!fscanf(config,"#Port:%d\n",&port)){
		printf("Error : Port number Invalid\n");
		exit(5);
	}
	if(!fscanf(config,"#Pages Path:%s",pagesPath)){
		printf("Error : Pages Path Invalid\n");
		exit(6);
	}
	close(config);
}

void createWorkers(){  // create the worker processes

	int i;
	struct Queue *q[nbWorker] ; // create a qeueue for each worker

	for(i=0;i<nbWorker;i++){
		q[i] = (struct Queue*)malloc(sizeof (struct Queue));

	}

	for(i = 0;i < nbWorker;i++){ //create nbWorker
		int pid = fork();
		if(pid < 0){
			perror("fork");
			exit(7);
		}
		else if(pid == 0 ){
			signalCreate();
			chid_process(q[i],i+1);
			exit(0);
		}
		else{
			PIDs[i] = pid;
		}
	}
}

void setnonblocking(int sock) { //add O_NONBLOCK flag to the socket

    int opt;
    opt = fcntl(sock, F_GETFL);

    if (opt < 0) {
        printf("fcntl(F_GETFL) fail.");
    }
    opt |= O_NONBLOCK;
    if (fcntl(sock, F_SETFL, opt) < 0) {
        printf("fcntl(F_SETFL) fail.");
    }
}

int chid_process(struct Queue *q,int id){ // the Worker process

  // set the worker qeueu to the max connections
	init(q,maxConn);

	struct sockaddr_in cli; //create the client socket
	int cli_len = sizeof(cli);
	int srvSocket;

	int fd;
	int flag = 1;
	while(Signal == NO_SIGNAL){ // loop until any signal received

		while(!QueueIsFull(q) && flag){ // accept connection while the queue is not full and the previous accept success

			semop(semid,&acquire,1); // acquire semaphore to accept the connection
			srvSocket=accept(serverX,(struct sockaddr*) &cli,&cli_len); // accept the connection

			if(srvSocket>0){
				char *ip = inet_ntoa(cli.sin_addr);
				printf("\nClient: %s:%d connected to Server(Worker %d)\n",ip,cli.sin_port,id);

				if(QueueInsert(q,srvSocket)<0){ // insert the desciptor to the queue
					printf("Error: Insert to the queue from %s\n",ip );
					close(srvSocket);
				}
			}
			else
				flag = 0; // the accpet not successed

			semop(semid,&release,1); //release the semaphore
		}

		flag = 1; // reset the flag
		fd = QueueRetrieve(q); //serve one connection form queue if exist
		if(fd>0){
			serve(fd);
		}
	}

	while(!QueueIsEmpty(q)){ // serve all remaining connections

		fd = QueueRetrieve(q);
		if(fd>0){
			serve(fd);
		}
	}
	close(serverX);
	printf("\n Worker %d finished it's Job \n",id);
	exit(0);
}

int serve(int fd){ // serve the connection on desciptor fd
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds); /* add s1 to the fd set */
	char request[1023];
	char buffer1[1023];
	char buffer2[1023];
	request[0] = '\0';
	buffer1[0] = '\0';
	buffer2[0] = '\0';
	struct sockaddr_in addr;
	char pageName[50];

    int addr_len = sizeof(struct sockaddr_in);
    getpeername(fd, (struct sockaddr *)&addr, &addr_len); // to get the connection ip
    char *ip = inet_ntoa(addr.sin_addr);


	read(fd,request,sizeof(request));
 	char *temp = strtok(request," "); // read the request instruction
 	if(strcmp(temp,"GET")){
	 	printf("Error: Invalid request from %s:%d\n",ip,addr.sin_port);
	 	return -1;
 	}

 	temp = strtok(NULL," "); // read the requested page
 	temp++;
 	if(strlen(temp)==0) // if there is not send the main page
 		strcpy(temp,"main.html");
 	if(strlen(temp)<5 || (strlen(temp)>=5 && strcmp(&temp[strlen(temp)-5],".html\0")!=0)) // add .html if not found
 		strcat(temp,".html");

 	if(strcmp(temp,catch_name) == 0){
 		printf("Client: %s:%d Request Page=\"%s\" Catch Hit\n",ip,addr.sin_port,temp);
 		if(write(fd,catch,strlen(catch),0)<0){
	 		perror("write");
	 	}
	 	printf("Client: %s:%d Page=\"%s\" Sent\n",ip,addr.sin_port,temp);
 	}
 	else{
 		strcpy(pageName,pagesPath);
	 	strcat(pageName,temp); // add the page name to the page folder directory
		//printf("page name is %s/n", pageName);
	 	printf("Client: %s:%d Request Page=\"%s\" Catch Miss\n",ip,addr.sin_port,temp);

	 	FILE *inp = fopen(pageName,"r"); // open the page file
	 	if(inp){ // if the file found
	 		strcat(buffer1,rsp);
	 		while(fgets(buffer2,1023,inp))
	 	    	strcat(buffer1,buffer2);

	 	    strcpy(catch,buffer1);
	 	    strcpy(catch_name,temp);
		 	setnonblocking(fd);
		 	if(write(fd,buffer1,strlen(buffer1),0)<0){
		 		perror("write");
		 	}
		 	close(inp);

	 		printf("Client: %s:%d Page=\"%s\" Sent\n",ip,addr.sin_port,temp);
	 	}
	 	else if(errno == 2){ // file not found
	 		strcpy(pageName,pagesPath);
	 		strcat(pageName,"not_found.html");
	 		FILE *inp = fopen(pageName,"r"); // open the not_found page file
	 		if(inp){ // if the file found
		 		strcat(buffer1,rsp_not_found);
		 		while(fgets(buffer2,1023,inp))
		 	    	strcat(buffer1,buffer2);

			 	setnonblocking(fd);
			 	if(write(fd,buffer1,strlen(buffer1),0)<0){
			 		perror("write");
			 	}
			 	close(inp);
		 		printf("Client: %s:%d Page=\"%s\" Not Found\n",ip,addr.sin_port,temp);
		 	}
	 	}
	 	else{
	 		setnonblocking(fd);
	 		if(write(fd,rsp_err,strlen(rsp_err),0)<0){
		 		perror("write");
		 	}
	 		printf("Client: %s:%d Page=\"%s\" Error\n",ip,addr.sin_port,temp);
	 	}
 	}
 	close(fd);
}
