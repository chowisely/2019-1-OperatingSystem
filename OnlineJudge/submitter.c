#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>


#define TRUE 1
#define FALSE 0


struct sockaddr_in serv_addr;
int sock_fd;
int portNum;
char ipAddr[15];

// student info  & target source code
char stu_id[20];
char password[20];
char filename[100];

void extractAddress(char *optarg) {
	int i = 0;
	char tmp[10];

	while(optarg[i++] != ':' && i < strlen(optarg));

	strncpy(ipAddr, optarg, i - 1);
	strcpy(tmp, optarg + i);
	portNum = atoi(tmp);
}

int checkStuID(char *optarg) {
	if(strlen(optarg) != 8)
		return FALSE;

	strcpy(stu_id, optarg);
	for(int i = 0; i < strlen(stu_id); i++) {
		if(!isdigit(stu_id[i]))
			return FALSE;
	}
	return TRUE;
}

int checkPassword(char *optarg) {
	if(strlen(optarg) != 8)
		return FALSE;

	strcpy(password, optarg);
	for(int i = 0; i < strlen(password); i++) {
		if(!isalnum(password[i]))
			return FALSE;
	}
	return TRUE;
}

void feedback_handler(int sig) {
	char buf[20] = {0};
	char tmp[20];
	char *data = 0x0;
	int s, len;

	/* try connecting to 'instagrapd' using 'portNum' and 'ipAddr' */
	sock_fd = socket(AF_INET, SOCK_STREAM, 0) ;
	if (sock_fd <= 0) {
		perror("socket failed : ") ;
		exit(EXIT_FAILURE) ;
	}

	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portNum);
	if (inet_pton(AF_INET, ipAddr, &serv_addr.sin_addr) <= 0) {
		perror("inet_pton failed : ") ;
		exit(EXIT_FAILURE) ;
	}

	if (connect(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("connect failed : ");
		exit(EXIT_FAILURE);
	}

	/* if it is connected successfully, send a stu_id and a password */
	memset(tmp, 0, sizeof(tmp));
	snprintf(tmp, sizeof(tmp), "%s:%s:", stu_id, password);

	data = tmp;
	len = strlen(data);
	s = 0;

	while(len > 0 && (s = send(sock_fd, data, len, 0)) > 0) {
		data += s;
		len -= s;
	}

	shutdown(sock_fd, SHUT_WR);

	/* receive data */
	data = 0x0;
	len = 0;
	while((s = recv(sock_fd, buf, 39, 0)) > 0 ) {
		buf[s] = 0x0 ;
		if (data == 0x0) {
			data = strdup(buf) ;
			len = s ;
		}
		else {
			data = realloc(data, len + s + 1) ;
			strncpy(data + len, buf, s) ;
			data[len + s] = 0x0 ;
			len += s ;
		}
	}

	close(sock_fd);

	/* according to a received message */
	if(strcmp(data, "not yet") == 0) {
		printf("Still Being Evaluated...\n");
	}
	else {
		if(strcmp(data, "VF") == 0)
			printf("Verification Failed.\n");
		else if(strcmp(data, "BF") == 0)
			printf("Build Failed.\n");
		else if(strcmp(data, "TO") == 0)
			printf("Timeout.\n");
		else
			printf("Total Number of Correct Cases : %s / 10\n", data);
		exit(0);
	}
}


int main(int argc, char *argv[]) {
	/* if the given arguments are inappropriate, terminate a program */
	if(argc != 8) {
		fprintf(stderr, "Invalid argument\n");
		exit(0);
	}

	int opt;

	FILE *fp;
	int s, len;
	char *data;
	char buf[100];

	char *fbuf;
	int fsize;

	while((opt = getopt(argc, argv, "n:u:k:")) != -1) {

		switch(opt) {
			case 'n':
			extractAddress(optarg);
			break;

			case 'u':
			if(!checkStuID(optarg)) {
				fprintf(stderr, "-u : Invalid argument\n");
				exit(0);
			}
			break;

			case 'k':
			if(!checkPassword(optarg)) {
				fprintf(stderr, "-k : Invalid arguement\n");
				exit(0);
			}
			strcpy(filename, argv[7]);
			break;

			case '?':
			printf("Unknown flag.\n");
			break;
		}
	}

	/* try connecting to 'instagrapd' using 'portNum' and 'ipAddr' */
	sock_fd = socket(AF_INET, SOCK_STREAM, 0) ;
	if (sock_fd <= 0) {
		perror("socket failed : ") ;
		exit(EXIT_FAILURE) ;
	}

	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portNum);
	if (inet_pton(AF_INET, ipAddr, &serv_addr.sin_addr) <= 0) {
		perror("inet_pton failed : ") ;
		exit(EXIT_FAILURE) ;
	}

	if (connect(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("connect failed : ");
		exit(EXIT_FAILURE);
	}

	/* if it is connected successfully, combine all the related data into one */
	fp = fopen(filename, "rb");
	if(fp == NULL) {
		printf("File doesn't exist.\n");
		exit(0);
	}

	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	fbuf = (char*)malloc(sizeof(char) * fsize);
	fread(fbuf, fsize, 1, fp);
	fclose(fp);

	s = strlen(fbuf) - 1;
	while(fbuf[s--] != '}' && s > 0);
	fbuf[s + 3] = '\0';

	int size = strlen(stu_id) + strlen(password) + strlen(fbuf) + 3;
	data = (char*)malloc(sizeof(char) * size);
	memset(data, 0, sizeof(data));
	snprintf(data, size, "%s:%s:%s", stu_id, password, fbuf);

	/* send data */
	len = strlen(data);
	s = 0;

	while(len > 0 && (s = send(sock_fd, data, len, 0)) > 0) {
		data += s;
		len -= s;
	}

	shutdown(sock_fd, SHUT_WR);
	free(fbuf);

	/* receive data */
	data = 0x0;
	len = 0;
	while((s = recv(sock_fd, buf, sizeof(buf), 0)) > 0) {
		buf[s] = 0x0;
		if (data == 0x0) {
			data = strdup(buf);
			len = s;
		}
		else {
			data = realloc(data, len + s + 1);
			strncpy(data + len, buf, s);
			data[len + s] = 0x0 ;
			len += s;
		}
	}

	close(sock_fd);

	/* set an interval for connecting with 'instagrapd' to receive a feedback */
	struct itimerval t;

	signal(SIGALRM, feedback_handler);
	t.it_value.tv_sec = 1;
	t.it_interval = t.it_value;

	setitimer(ITIMER_REAL, &t, 0x0);
	while (1);
}

