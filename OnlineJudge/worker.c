#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>


#define READ_END 0
#define WRITE_END 1

pid_t child;

void timeout_handler(int sig) {
	kill(child, SIGKILL);
	child = -1;
}

void buildup(char *filepath, char *filename, int *p1, int *p2) {
	printf("Building...\n");
	char command[50];
	snprintf(command, 50, "gcc -o %s %s", filename, filepath);
	
	dup2(p2[READ_END], 0);
	dup2(p1[WRITE_END], 1);
	
	if(system(command) != 0) // if a shell command is failed
		exit(0);
	execl(filename, filename, NULL); // otherwise, execute a binary file
}

void child_worker(int new_socket) {
	int p1[2], p2[2]; // p1 for reading output from a program
					// p2 for writing input to a program
	char buf[1024];
	char *input, *fbuf;
	char command[50];
	char *data = 0x0;
	int len = 0;
	int s, i;
	
	int pid = getpid();
	FILE *fp;
	char filepath[30];
	char filename[30];
	
	/* create pipes so that parent and child can communicate easily via those pipes */
	if(pipe(p1) == -1 || pipe(p2) == -1) {
		perror("pipe failed : ");
		exit(0);
	}
	
	/* set an interval for checking if execution time limit is exceeded  */
	struct itimerval t;
	signal(SIGALRM, timeout_handler);
	t.it_value.tv_sec = 3;
	t.it_value.tv_usec = 100000;
	t.it_interval = t.it_value;
	setitimer(ITIMER_REAL, &t, 0x0);
	
	/* receive data */
	data = 0x0;
	len = 0;
	while((s = recv(new_socket, buf, sizeof(buf) - 1, 0)) > 0) {
		buf[s] = 0x0;
		if (data == 0x0) {
			data = strdup(buf);
			len = s;
		}
		else {
			data = realloc(data, len + s + 1);
			strncpy(data + len, buf, s);
			data[len + s] = 0x0;
			len += s;
		}
	}
	shutdown(new_socket, SHUT_RD);
	
	/* extract input data */
	i = 0;
	while(data[i++] != ':' && i < strlen(data));
	input = (char*)malloc(sizeof(char) * i);
	memset(input, 0, sizeof(input));
	strncpy(input, data, i - 1);
	input[i - 2] = '\0';
	
	/* extract a source code */
	fbuf = (char*)malloc(sizeof(char) * strlen(data));
	memset(fbuf, 0, sizeof(fbuf));
	strcpy(fbuf, data + i);
	i = 0;
	while(fbuf[i++] != ':' && i < strlen(fbuf));
	strncpy(data, fbuf, i - 1);
	data[i - 2] = '\0';
	
	/* create a source code file */
	snprintf(filename, 30, "./target/%d", pid);
	snprintf(filepath, 30, "%s.c", filename);
	
	fp = fopen(filepath, "w");
	fputs(data, fp);
	fclose(fp);
	
	if(data != 0x0)
		free(data);
	
	/* make a child process to build and execute a file */
	child = fork();
	
	if(child == 0) { // child process
		close(new_socket);
		close(p1[READ_END]); // it won't use p1 for reading output
		close(p2[WRITE_END]); // it won't use p2 for writing input
		
		buildup(filepath, filename, p1, p2);

		close(p1[WRITE_END]);
		close(p2[READ_END]);
		free(fbuf);
		free(input);
		_exit(0);
	}
	else { // parent process
		close(p1[WRITE_END]); // it won't use p1 for writing output
		close(p2[READ_END]); // it won't use p2 for reading input
		write(p2[WRITE_END], input, sizeof(int) * 8192); // write input to the pipe
                                                    // the default pipe capacity is 65,536 byte
                                                    // in case of changing the size, use fcntl()
		close(p2[WRITE_END]);
		waitpid(child, NULL, 0);
		
		/* check if build is failed using existence of file */
		fp = fopen(filename, "r");
		if (fp == NULL) {
			printf("Build Failed\n");
			data = "BF";
		}
		else {
			memset(buf, 0, sizeof(buf));
			read(p1[READ_END], buf, sizeof(buf));
			close(p1[READ_END]);
			data = buf;
			
			if(child == -1) {
				printf("Timeout!\n");
				data = "TO";
			}
		}
		
		/* delete a source file and a binary file */
		snprintf(command, 50, "rm %s", filepath);
		system(command);
		
		/* send data */
		len = strlen(data);
		while(len > 0 && (s = send(new_socket, data, len, 0)) > 0) {
			data += s;
			len -= s;
		}
		
		free(fbuf);
		free(input);
		close(new_socket);
		exit(0);
	}
}


int main(int argc, char *argv[]) {
	if(argc != 3) {
		fprintf(stderr, "Invalid argument\n");
		exit(0);
	}
	
	int opt;
	char ip[15];
	char dir[100]; // a path to a testcase directory
	int portNum; // to listen
	int i = 0;
	
	int listen_fd, new_socket;
	struct sockaddr_in address;
	int addrlen = sizeof(address);
	
	while((opt = getopt(argc, argv, "p:")) != -1) {
		switch(opt) {
			case 'p':
			portNum = atoi(optarg);
			break;
			
			case '?':
			printf("Unknown flag.\n");
				exit(0);
		}
	}

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);

	if(listen_fd == 0) {
		perror("socket failed : ");
		exit(EXIT_FAILURE);
	}

	memset(&address, '0', sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY; // localhost
	address.sin_port = htons(portNum);
	if(bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed : ");
		exit(EXIT_FAILURE);
	}
	
	while (1) {
		if(listen(listen_fd, 100 /* the size of waiting queue*/) < 0) {
			perror("listen failed : ");
			exit(EXIT_FAILURE);
		}
		
		/* Send it to another port because 'instagrapd' only has to listen to requests.
		 * One socket is needed for per session.
		 * A server needs to create a new socket for each remote endpoint(client)
		 * as long as every socket created before is being used.
		 */
		
		new_socket = accept(listen_fd, (struct sockaddr *) &address, (socklen_t*)&addrlen);
		
		if(new_socket < 0) {
			perror("accept");
			exit(EXIT_FAILURE);
		}

		if(fork() == 0)
			child_worker(new_socket);
		else {
			close(new_socket);
			waitpid(-1, NULL, WNOHANG);
		}
	}
}

