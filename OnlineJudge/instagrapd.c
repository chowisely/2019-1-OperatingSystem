#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>

#define MAX 50

typedef struct _student {
	char stu_id[20];
	char password[20];
	char *target;
	int correct;
	int done;
	int isFailed;
	int isTimeout;
}student;


student *stu;
int numOfStu = 0;
int workerPort;
char dir[30]; // a path to a testcase directory

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t n = PTHREAD_MUTEX_INITIALIZER;


void extractAddress(char *optarg, char *ipAddr) {
	int i = 0;
	char tmp[10];

	while(optarg[i++] != ':' && i < strlen(optarg));

	strncpy(ipAddr, optarg, i - 1);
	strcpy(tmp, optarg + i);
	workerPort = atoi(tmp);
}

void *sendToWorker(void* arg) {
	int idx;

	int new_socket;
	struct sockaddr_in serv_addr;
	int len = 0;
	int s, i;
	char *data = 0x0;
	char *message;
	char buf[100];
	char *in, *out;

	FILE *fp;
	int fsize;
	char input[30];
	char output[30];

	pthread_t tid[50];

	pthread_mutex_lock(&n);

	if(stu[idx].isFailed == 1 || stu[idx].done == 10)
		pthread_exit(0);

	for(i = 0; i < 10; i++) {
		new_socket = socket(AF_INET, SOCK_STREAM, 0);

		if(new_socket <= 0) {
			perror("socket failed : ");
			exit(EXIT_FAILURE);
		}

		memset(&serv_addr, '0', sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(workerPort);
		if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
			perror("inet_pton failed : ");
			exit(EXIT_FAILURE);
		}

		if(connect(new_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
			perror("connect failed : ");
			exit(EXIT_FAILURE);
		}

		idx = (int)(int*)arg;

		/* read input data */
		snprintf(input, sizeof(input), "%s/%d.in", dir, i + 1);
		fp = fopen(input, "r");
		fseek(fp, 0, SEEK_END);
		fsize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		if(fsize > sizeof(int) * 8196)
			fsize = sizeof(int) * 8196;
		in = (char*)malloc(sizeof(char) * fsize);
		fread(in, fsize, 1, fp);
		fclose(fp);

		data = (char*)malloc(strlen(stu[idx].target) + strlen(in) + 10);
		snprintf(data, strlen(stu[idx].target) + strlen(in) + 10, "%s:%s:", in, stu[idx].target);
		free(in);

		/* send data */
		len = strlen(data);
		while(len > 0 && (s = send(new_socket, data, len, 0)) > 0) {
			data += s;
			len -= s;
		}
		shutdown(new_socket, SHUT_WR);

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
		close(new_socket);

		/* read output data */
		snprintf(output, sizeof(output), "%s/%d.out", dir, i + 1);
		fp = fopen(output, "r");
		fseek(fp, 0, SEEK_END);
		fsize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		out = (char*)malloc(sizeof(char) * fsize);
		fread(out, fsize, 1, fp);
		fclose(fp);

		/* remove a new line character at the end of the result */
		s = strlen(out) - 1;
		while(out[s--] != '\n' && s > 0);
		out[s] = '\0';

		if(strcmp(data, "BF") == 0) {
			stu[idx].isFailed = 1;
			break;
		}
		else if(strcmp(data, "TO") == 0) {
			stu[idx].isTimeout = 1;
			break;
		}
		else {
			if(strcmp(out, data) == 0)
				stu[idx].correct++;
			stu[idx].done++;

			printf("ID: %s, correct output:%s, received output: %s\n", stu[idx].stu_id, out, data);
			printf("ID: %s, correct: %d, done: %d\n\n", stu[idx].stu_id, stu[idx].correct, stu[idx].done);
		}

		free(out);
		free(message);
	}

	pthread_mutex_unlock(&n);
	pthread_exit(0);
}

void *child_thread(void *arg) {
	int new_socket;
	char *data = 0x0;
	int len = 0;
	int s, idx;
	int j = 0;
	int errcode;
	pthread_t thread;

	FILE *fp;
	char buf[1024]; // data received from a client
	char id[20]; // for student's id
	char pwd[20]; // for student's password
	char *fbuf; // for source code file

	pthread_mutex_lock(&m);

	new_socket = (int)(int*)arg;

	/* first, receive a stu_id, password and source code combined into one */
	while((s = recv(new_socket, buf, sizeof(buf) - 1, 0)) > 0 ) {
		buf[s] = 0x0 ;
		if (data == 0x0) {
			data = strdup(buf);
			len = s;
		}
		else {
			data = realloc(data, len + s + 1) ;
			strncpy(data + len, buf, s) ;
			data[len + s] = 0x0 ;
			len += s ;
		}
	}

	shutdown(new_socket, SHUT_RD);

	memset(id, 0, sizeof(id));
	memset(pwd, 0, sizeof(pwd));
	strncpy(id, buf, 8); // extract a stu_id
	strncpy(pwd, buf + 9, 8); // extract a password

	/* check if this student info is already stored */
	for(idx = 1; idx < MAX; idx++) {
		if(strcmp(id, stu[idx].stu_id) == 0) {
			if(strcmp(pwd, stu[idx].password) != 0)
				errcode = 1; // if verification failure occurs, reject a submitter
			else if(strcmp(pwd, stu[idx].password) == 0) // if student info already exists, send a feedback to a submitter
				errcode = 0;
			break;
		}

		if(strcmp(id, stu[idx].stu_id) != 0 && idx == numOfStu + 1) {  // if student info doesn't exist, save it
			strcpy(stu[idx].stu_id, id);
			strcpy(stu[idx].password, pwd);
			stu[idx].correct = 0;
			stu[idx].done = 0;
			stu[idx].isFailed = 0;
			stu[idx].isTimeout = 0;
			fbuf = buf + 18; // extract a source code
			stu[idx].target = (char*)malloc(sizeof(char) * strlen(fbuf));
			strcpy(stu[idx].target, fbuf);
			errcode = -1;

			numOfStu++;
			break;
		}
	}

	/* according to a errcode, set data sent to a submitter */
	switch(errcode) {
		case -1: // if a student is newly saved, it needs to send his info to 'worker'
		pthread_create(&thread, NULL, sendToWorker, (void*)idx);
		data = "sent";
		break;

		case 0:
		if(stu[idx].isFailed == 1)
			data = "BF";
		else if(stu[idx].isTimeout == 1)
			data = "TO";
		else {
			if(stu[idx].done < 10)
				data = "not yet";
			else
				snprintf(data, sizeof(stu[idx].correct), "%d", stu[idx].correct);
		}
		break;

		case 1:
		data = "VF";
	}

	/* send data */
	len = strlen(data);
	while(len > 0 && (s = send(new_socket, data, len, 0)) > 0) {
		data += s;
		len -= s;
	}

	close(new_socket);
	pthread_mutex_unlock(&m);
	pthread_exit(0);
}


int main(int argc, char *argv[]) {
	if(argc != 6) {
		fprintf(stderr, "Invalid argument\n");
		exit(0);
	}

	int opt;
	char ipAddr[15];
	int portNum; // to listen

	int listen_fd;
	pthread_t thread;
	int *new_socket;
	struct sockaddr_in address;
	int addrlen = sizeof(address);

	while((opt = getopt(argc, argv, "p:w:")) != -1) {
		switch(opt) {
			case 'p':
			portNum = atoi(optarg);
			break;

			case 'w':
			extractAddress(optarg, ipAddr);
			strcpy(dir, argv[5]); // copy a filepath given from a user to dir
			break;

			case '?':
			printf("Unknown flag.\n");
			exit(0);
		}
	}

	stu = (student*)malloc(sizeof(student) * MAX); // allocate memory to save student info

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd == 0)  {
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

		if(numOfStu >= MAX)
			break;

		/* Send it to another port because 'instagrapd' only has to listen to requests.
		 * One socket is needed for per session.
		 * A server needs to create a new socket for each remote endpoint(client)
		 * as long as every socket created before is being used.
		 */
		new_socket = (int*)malloc(sizeof(int));
		new_socket = accept(listen_fd, (struct sockaddr *) &address, (socklen_t*)&addrlen);

		if(new_socket < 0) {
			perror("accept");
			exit(EXIT_FAILURE);
		}

		if((pthread_create(&thread, NULL, child_thread, (void*)new_socket)) != 0) {
			perror("thread creation failed : ") ;
			exit(0);
		}
	}

	free(stu);
}

