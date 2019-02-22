#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "ibank.h"

#define BUFLEN 256
#define CMDLEN 10

/* Write the message to stderr and exit with code 1 */
void error(char *msg)
{
	perror(msg);
	exit(1);
}

/* Check the number of arguments given as parameter to the program */
void checkArgs(int argc, char *argv[])
{
	if (argc != 3)
	{
		fprintf(stderr, "Usage : %s <server_address> <server_port>\n", argv[0]);
		exit(1);
	}
}

/* Initialize a TCP connection */
int initConnection(char *address, int port)
{
	struct sockaddr_in serv_addr;

	// Create a socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	// Fill in with data
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	inet_aton(address, &serv_addr.sin_addr);

	// Connect
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR connecting");

	return sockfd;
}

/* Get the next word from a string. Any space until that word will be ignored. 
 * It returns a pointer to the start of the word and, by side effect, the size
 * of the word.
 */
char *getNextWord(char *str, int *len)
{
	*len = 0;
	while (*str == ' ')
		str++;

	if (*str == '\n' || str == '\0')
		return str;

	char *begin = str;
	while (*str != ' ' && *str != '\0' && *str != '\n')
	{
		str++;
		(*len)++;
	}

	return begin;
}

/* Init file descripts sets */
void initFDs(int sockfd, fd_set *read_fds, fd_set *tmp_fds)
{
	// Clear descriptor set
	FD_ZERO(read_fds);
	FD_ZERO(tmp_fds);

	// Add socket to file descriptor set
	FD_SET(sockfd, read_fds);
	FD_SET(STDIN_FILENO, read_fds);
}

/* Send a login request to server */
void login(int sockfd, FILE *logfd, char *params, int *session, char *lastlogin)
{
	TRequest req;

	// Check for existing session on current client
	if (*session)
	{
		char *msg = "Sesiune deja deschisa";
		printf("IBANK> %d : %s\n", SESSION_EXISTS, msg);
		fprintf(logfd, "IBANK> %d : %s\n", SESSION_EXISTS, msg);
		return;
	}

	// Prepare request to login
	memset(&req, 0, sizeof(TRequest));
	strcpy(req.type, "LOGI");
	strcpy(req.payload, params);

	// Remember first word of login
	int len = 0;
	char *tmp = getNextWord(params, &len);
	memset(lastlogin, 0, CARDNUMLEN);
	strncpy(lastlogin, tmp, len);

	// Send request to login
	if (send(sockfd, (char *)(&req), sizeof(TRequest), 0) < 0)
		error("ERROR writing to socket");

	// Wait for response
	memset(&req, 0, sizeof(TRequest));
	if (recv(sockfd, (char *)(&req), sizeof(TRequest), 0) < 0)
		error("ERROR reading from socket");

	// Check response to activate login session
	if (!strcmp(req.type, "LOGI"))
		*session = 1;

	// Display and log the response
	printf("%s\n", req.payload);
	fprintf(logfd, "%s\n", req.payload);
}

/* Send a logout request to server */
void logout(int sockfd, FILE *logfd, int *session)
{
	TRequest req;

	// Check for existing session on current client
	if (!*session)
	{
		char *msg = "Clientul nu este autentificat";
		printf("%d : %s\n", NOT_LOGGED, msg);
		fprintf(logfd, "%d : %s\n", NOT_LOGGED, msg);
		return;
	}

	// Send request to login
	memset(&req, 0, sizeof(TRequest));
	strcpy(req.type, "LOGO");
	if (send(sockfd, (char *)(&req), sizeof(TRequest), 0) < 0)
		error("ERROR writing to socket");

	// Wait for response
	memset(&req, 0, sizeof(TRequest));
	if (recv(sockfd, (char *)(&req), sizeof(TRequest), 0) < 0)
		error("ERROR reading from socket");

	// Check response
	if (!strcmp(req.type, "LOGO"))
		*session = 0;

	// Display and log the response
	printf("%s\n", req.payload);
	fprintf(logfd, "%s\n", req.payload);
}

/* Send request to list sold */
void listsold(int sockfd, FILE *logfd, int *session)
{
	TRequest req;

	// Check for existing session on current client
	if (!*session)
	{
		char *msg = "Clientul nu este autentificat";
		printf("%d : %s\n", NOT_LOGGED, msg);
		fprintf(logfd, "%d : %s\n", NOT_LOGGED, msg);
		return;
	}

	// Send request to login
	memset(&req, 0, sizeof(TRequest));
	strcpy(req.type, "SOLD");
	if (send(sockfd, (char *)(&req), sizeof(TRequest), 0) < 0)
		error("ERROR writing to socket");

	// Wait for response
	memset(&req, 0, sizeof(TRequest));
	if (recv(sockfd, (char *)(&req), sizeof(TRequest), 0) < 0)
		error("ERROR reading from socket");

	// Display and log the response
	printf("%s\n", req.payload);
	fprintf(logfd, "%s\n", req.payload);
}

/* Send a transfer request to server. The function returns the 1 if the 
 * transaction is in progress, or 0 otherwise. 
 */
int transfer(int sockfd, FILE *logfd, char *params, int *session)
{
	TRequest req;

	// Check for existing session on current client
	if (!*session)
	{
		char *msg = "Clientul nu este autentificat";
		printf("%d : %s\n", NOT_LOGGED, msg);
		fprintf(logfd, "%d : %s\n", NOT_LOGGED, msg);
		return 0;
	}

	// Send request to transfer money
	memset(&req, 0, sizeof(TRequest));
	strcpy(req.type, "TRAN");
	strcpy(req.payload, params);
	if (send(sockfd, (char *)(&req), sizeof(TRequest), 0) < 0)
	{
		error("ERROR writing to socket");
		return 0;
	}

	// Wait for response
	memset(&req, 0, sizeof(TRequest));
	if (recv(sockfd, (char *)(&req), sizeof(TRequest), 0) < 0)
	{
		error("ERROR reading from socket");
		return 0;
	}

	// Display and log the response
	printf("%s\n", req.payload);
	fprintf(logfd, "%s\n", req.payload);

	// Check for errors
	if (!strcmp(req.type, "EROR"))
		return 0;

	// Continue transaction
	return 1;
}

/* Unlock the card */
void unlock(char *lastlogin, FILE *logfd, char *address, int port)
{
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	// Open socket
	int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	// Set socket address
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	inet_aton(address, &serv_addr.sin_addr);
	serv_addr.sin_port = htons(port);

	// Create a request to unlock
	TRequest req;
	memset(&req, 0, sizeof(TRequest));
	strcpy(req.type, "ULK1");
	strcpy(req.payload, lastlogin);

	// Send request
	socklen_t len = sizeof(struct sockaddr_in);
	sendto(sockfd, &req, sizeof(TRequest), 0,
		   (struct sockaddr *)&serv_addr, len);

	// Wait for response
	recvfrom(sockfd, &req, sizeof(TRequest), 0,
			 (struct sockaddr *)&serv_addr, &len);

	// Display and log the response
	printf("%s\n", req.payload);
	fprintf(logfd, "%s\n", req.payload);

	// For errors, quit from unlock session
	if (!strcmp(req.type, "EROR"))
	{
		close(sockfd);
		return;
	}

	// Get the secret from stdin
	memset(buffer, 0, BUFLEN);
	scanf("%s", buffer);
	fgetc(stdin);

	// log secret given from stdin
	fprintf(logfd, "%s\n", buffer);

	// Send request
	strcpy(req.type, "ULK2");
	sprintf(req.payload, "%s %s", lastlogin, buffer);
	sendto(sockfd, &req, sizeof(TRequest), 0,
		   (struct sockaddr *)&serv_addr, len);

	// Wait for response
	recvfrom(sockfd, &req, sizeof(TRequest), 0,
			 (struct sockaddr *)&serv_addr, &len);

	// Display and log the response
	printf("%s\n", req.payload);
	fprintf(logfd, "%s\n", req.payload);

	// Close UDP unlock connection
	close(sockfd);
}

/* Continue transaction. Send client's agreement to transfer money. */
void transaction(int sockfd, FILE *logfd, char *buffer, int *session,
				 int *transferStatus)
{
	// Check for existing session on current client
	if (!*session)
	{
		char *msg = "Clientul nu este autentificat";
		printf("%d : %s\n", NOT_LOGGED, msg);
		fprintf(logfd, "%d : %s\n", NOT_LOGGED, msg);
		return;
	}

	TRequest req;

	// Send request to transfer money
	memset(&req, 0, sizeof(TRequest));
	strcpy(req.type, "ANYC");
	strcpy(req.payload, buffer);
	if (send(sockfd, (char *)(&req), sizeof(TRequest), 0) < 0)
	{
		error("ERROR writing to socket");
		*transferStatus = 0;
		return;
	}

	// Wait for response
	memset(&req, 0, sizeof(TRequest));
	if (recv(sockfd, (char *)(&req), sizeof(TRequest), 0) < 0)
	{
		error("ERROR reading from socket");
		*transferStatus = 0;
		return;
	}

	// Display and log the response
	printf("%s\n", req.payload);
	fprintf(logfd, "%s\n", req.payload);
}

/* The command interpreter. All commands, excepting quit, are interpreted here.
 */
void commandInterpreter(int sockfd, FILE *logfd, char *buffer, int *session,
						int *transferStatus, char *lastlogin,
						char *address, int port)
{
	char cmd[CMDLEN], *tmp = buffer;
	int len;

	tmp = getNextWord(tmp, &len);
	memset(cmd, 0, CMDLEN);
	strncpy(cmd, tmp, len);
	tmp += len;

	// If we are durring a transaction
	if (*transferStatus)
	{
		transaction(sockfd, logfd, buffer, session, transferStatus);
		
		// close transaction
		*transferStatus = 0;
		return;
	}

	// Interpret commands
	if (!strcmp(cmd, "login"))
		login(sockfd, logfd, tmp, session, lastlogin);
	else if (!strcmp(cmd, "logout"))
	{
		logout(sockfd, logfd, session);
		*transferStatus = 0;
	}
	else if (!strcmp(cmd, "listsold"))
		listsold(sockfd, logfd, session);
	else if (!strcmp(cmd, "transfer"))
		*transferStatus = transfer(sockfd, logfd, tmp, session);
	else if (!strcmp(cmd, "unlock"))
		unlock(lastlogin, logfd, address, port);
}

/* Send a quit request to server. Client is shutting down. */
void sendQuitReq(int sockfd)
{
	TRequest req;
	strcpy(req.type, "QUIT");
	send(sockfd, (char *)(&req), sizeof(TRequest), 0);
	close(sockfd);
}

int main(int argc, char *argv[])
{
	int fdmax, sockfd, session = 0, transferStatus = 0, i;
	char buffer[BUFLEN], lastlogin[CARDNUMLEN], nameLog[BUFLEN];
	fd_set read_fds, tmp_fds;

	// Check arguments
	checkArgs(argc, argv);

	// Create socket and init fds
	sockfd = initConnection(argv[1], atoi(argv[2]));
	initFDs(sockfd, &read_fds, &tmp_fds);
	fdmax = sockfd;

	// Create log file
	sprintf(nameLog, "client-%d.log", getpid());
	FILE *logfd = fopen(nameLog, "wt");
	if (!logfd)
		error("ERROR opening file");

	while (1)
	{
		tmp_fds = read_fds;
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
			error("ERROR in select");

		for (i = 0; i <= fdmax; i++)
		{
			if (!FD_ISSET(i, &tmp_fds))
				continue;

			if (i == sockfd)
			{
				// Received message from server
				TRequest req;
				memset(&req, 0, BUFLEN);
				if (recv(sockfd, (char *)(&req), sizeof(TRequest), 0) < 0)
					error("ERROR reading from socket");

				// Received quit server
				if (!strcmp(req.type, "QUIT"))
				{
					printf("%s\n", req.payload);
					fprintf(logfd, "%s\n", req.payload);
					exit(0);
				}
			}
			else if (i == STDIN_FILENO)
			{
				// Get text from stdin
				memset(buffer, 0, BUFLEN);
				fgets(buffer, BUFLEN - 1, stdin);
				fprintf(logfd, "%s", buffer);

				// Interpret commands
				if (!strcmp(buffer, "quit\n"))
				{
					sendQuitReq(sockfd);
					close(i);
					close(sockfd);
					fclose(logfd);
					exit(0);
				}
				else
				{
					commandInterpreter(sockfd, logfd, buffer, &session,
									   &transferStatus, lastlogin,
									   argv[1], atoi(argv[2]));
				}
			}
		}
	}
	return 0;
}
