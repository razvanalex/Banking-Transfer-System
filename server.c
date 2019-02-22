#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "ibank.h"

#define MAX_CLIENTS 5
#define BUFLEN 256

/* Write the message to stderr and exit with code 1 */
void error(char *msg)
{
	perror(msg);
	exit(1);
}

/* Check the number of arguments given as parameter to the program */
void checkArgs(int argc, char *argv[])
{
	if (argc == 3)
		return;

	fprintf(stderr, "Usage : %s <port_server> <users_data_file>\n", argv[0]);
	exit(1);
}

/* Init TCP socket. Returns the file descriptor for that socket. */
int initServerTCP(int portNum)
{
	struct sockaddr_in serv_addr;

	// Open socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	// Set socket address
	memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portNum);

	// Bind socket to fd
	int r = bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr));
	if (r < 0)
		error("ERROR on binding");

	// Listen for clients
	listen(sockfd, MAX_CLIENTS);

	// Return socket file descriptor
	return sockfd;
}

/* Init UDP socket. Returns the file descriptor for that socket. */
int initServerUDP(int portNum)
{
	struct sockaddr_in serv_addr;

	// Open socket
	int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	// Set socket address
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portNum);

	// Bind socket to port
	int r = bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr));
	if (r < 0)
		error("ERROR on binding");

	// Return socket file descriptor
	return sockfd;
}

/* Open data file and fill in out database. */
int openDB(char *filename, TDataBase *db)
{
	int n;

	// Open the file to read
	FILE *file = fopen(filename, "rt");
	if (!file)
		error("Error opening file");

	// Read the number of records
	fscanf(file, "%d", &n);

	// Init the database
	db->size = n;
	db->records = (TDBRecord *)calloc(n, sizeof(TDBRecord));
	if (!db->records)
		error("Error allocating memory");

	// Get data from file
	for (int i = 0; i < n; i++)
	{
		TDBRecord *record = &db->records[i];

		fscanf(file, "%s %s %s %s %s %lf\n", record->lastname,
			   record->firstname, record->cardNum, record->pin,
			   record->password, &record->sold);
	}

	// Close file descriptor, and exit from function
	fclose(file);
	return 0;
}

/* Initialize sessions to 0. */
TSession *initSessions(int numSessions)
{
	TSession *sessions = (TSession *)calloc(numSessions, sizeof(TSession));
	if (!sessions)
		return NULL;

	return sessions;
}

/* Resize the array of sessions, twice the current size. */
TSession *resizeSessions(TSession *sessions, int *numSessions)
{
	int i;
	int newSize = 2 * (*numSessions) * sizeof(TSession);

	// Realloc the array
	TSession *tmp = realloc(sessions, newSize);
	if (!tmp)
	{
		return NULL;
	}
	sessions = tmp;

	// Set all new memory to 0
	for (i = *numSessions; i < *numSessions * 2; i++)
		memset(&sessions[i], 0, sizeof(TSession));

	*numSessions *= 2;

	// Return the new array
	return sessions;
}

/* Accept a connection to TCP socket. Also, set the session for that connection. 
 */
int establishConnection(int sockfd, fd_set *read_fds, int fdmax,
						TSession **sessions, int *numSessions)
{
	int newsockfd;
	struct sockaddr_in cli_addr;

	// Accept the client
	socklen_t clilen = sizeof(cli_addr);
	newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

	if (newsockfd == -1)
		error("ERROR in accept");
	else
	{
		FD_SET(newsockfd, read_fds);
		if (newsockfd > fdmax)
			fdmax = newsockfd;
	}

	// Resize array of sessions if needed
	if (*numSessions < fdmax - 2)
	{
		TSession *tmp = resizeSessions(*sessions, numSessions);
		if (!tmp)
		{
			return fdmax;
		}
		*sessions = tmp;
	}

	printf("Noua conexiune de la %s, port %d, socket_client %d\n",
		   inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), newsockfd);

	// Reset session for new connection
	memset(&(*sessions)[newsockfd - 4], 0, sizeof(TSession));

	// Return the maximum value for file descriptors
	return fdmax;
}

/* Send a buffer (memory) to a client. */
void sendBuffer(int fd, char *buffer, size_t len)
{
	int n;
	if ((n = send(fd, buffer, len, 0)) <= 0)
	{
		if (n == 0)
			printf("server: socket %d hung up\n", fd);
		else
			error("ERROR in send");
	}
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

/* Send an error to client. */
void sendError(int fd, char err, char *msg)
{
	TRequest req;

	memset(&req, 0, sizeof(TRequest));
	strcpy(req.type, "EROR");
	sprintf(req.payload, "IBANK> %d : %s", err, msg);

	sendBuffer(fd, (char *)(&req), sizeof(req));
}

/* Extract the card number and the pin from buffer. */
int extractCredentials(char *params, char *cardNum, char *pin)
{
	int len;
	char *tmp = params;

	// Get first word and copy content to cardNum
	tmp = getNextWord(tmp, &len);
	strncpy(cardNum, tmp, len);
	tmp += len;

	if (len == 0)
		return -1;

	// Get second word and copy content to pin
	tmp = getNextWord(tmp, &len);
	strncpy(pin, tmp, len);

	if (len == 0)
		return -1;

	return 0;
}

/* Check if a session is opened for a given card number. */
int openedSession(TSession *sessions, int numSessions, char *cardNum)
{
	int i;
	for (i = 0; i < numSessions; i++)
		if (!strcmp(sessions[i].cardNum, cardNum) && sessions[i].isOpened)
			return 1;

	return 0;
}

/* Get the index of record for a given card number */
int getIndex(TDataBase *db, char *cardNum)
{
	int i;
	for (i = 0; i < db->size; i++)
		if (!strcmp(db->records[i].cardNum, cardNum))
			break;

	return (i != db->size) ? i : -1;
}

/* Extract the card number and the amount of money from buffer. */
int extractTransferData(char *params, char *cardNum, double *amount)
{
	int len;
	char *tmp = params;

	// Get first word and copy content to lastname
	tmp = getNextWord(tmp, &len);
	strncpy(cardNum, tmp, len);
	tmp += len;

	if (len == 0)
		return -1;

	// Get second word and copy content to firstname
	tmp = getNextWord(tmp, &len);
	*amount = atof(tmp);

	if (len == 0)
		return -1;

	return 0;
}

/* Get login request */
void loginReq(int fd, TRequest *req, TDataBase *db, 
			  TSession *sessions, int numSessions)
{
	int i, ID = fd - 4;
	char cardNum[CARDNUMLEN], pin[PINLEN];
	memset(cardNum, 0, CARDNUMLEN);
	memset(pin, 0, PINLEN);

	// Extract the credentials
	if (extractCredentials(req->payload, cardNum, pin) < 0)
	{
		sendError(fd, FAILED_OPER, "Operatie esuata");
		return;
	}

	// Check for existing card/opened session
	i = getIndex(db, cardNum);

	if (i == -1)
	{
		sendError(fd, CARD_INEXISTENT, "Numar card inexistent");
		return;
	}

	if (openedSession(sessions, numSessions, cardNum))
	{
		sendError(fd, SESSION_EXISTS, "Sesiune deja deschisa");
		return;
	}

	// Reset counters for new login credentials on current session
	if (strcmp(sessions[ID].cardNum, cardNum))
	{
		strcpy(sessions[ID].cardNum, cardNum);
		sessions[ID].numTries = 0;
	}

	// Increase number of wrong pins given
	if (sessions[ID].numTries <= 3)
		sessions[ID].numTries++;

	if (strcmp(pin, db->records[i].pin))
	{
		// Wrong pin, or worst, the card will be locked
		if (sessions[ID].numTries < 3 && !db->records[i].isLocked)
			sendError(fd, WRONG_PIN, "Pin gresit");
		else
		{
			sendError(fd, LOCKED_CARD, "Card blocat");
			db->records[i].isLocked = 1;
		}
		return;
	}
	else if (!db->records[i].isLocked)
	{
		// Open new login session and reset conters
		memset(req->payload, 0, sizeof(req->payload));
		sprintf(req->payload, "IBANK> Welcome %s %s",
				db->records[i].lastname, db->records[i].firstname);

		sendBuffer(fd, (char *)req, sizeof(TRequest));
		strcpy(sessions[ID].cardNum, cardNum);
		sessions[ID].numTries = 0;
		sessions[ID].isOpened = 1;
	}
	else
	{
		// The card is locked
		sendError(fd, LOCKED_CARD, "Card blocat");
		return;
	}
}

/* First part of transaction. Check for card number, money and send the 
 * confirmation to client.
 */
void transaction1(int fd, TRequest *req, TDataBase *db,
				  TSession *sessions)
{
	int index, ID = fd - 4;
	char cardNum[CARDNUMLEN];
	double amount = 0;
	memset(cardNum, 0, CARDNUMLEN);

	// Extract params from payload and get the index of the card
	extractTransferData(req->payload, cardNum, &amount);
	index = getIndex(db, cardNum);

	// The card does not exist.
	if (index < 0)
	{
		sendError(fd, CARD_INEXISTENT, "Numar card inexistent");
		return;
	}

	// Not enough money to do the transaction
	int crtCardIndex = getIndex(db, sessions[ID].cardNum);
	if (amount > db->records[crtCardIndex].sold)
	{
		sendError(fd, NO_MONEY, "Fonduri insuficiente");
		return;
	}

	// Set the confirmation question
	if ((int)amount == amount)
		sprintf(req->payload, "IBANK> Transfer %d catre %s %s? [y/n]",
			    (int)amount, db->records[index].lastname,
				db->records[index].firstname);
	else
		sprintf(req->payload, "IBANK> Transfer %.2lf catre %s %s? [y/n]",
				amount, db->records[index].lastname,
				db->records[index].firstname);

	// Set session to during transaction.
	sessions[ID].duringTransaction = 1;
	strcpy(sessions[ID].toCard, cardNum);
	sessions[ID].amount = amount;

	// Send the request
	sendBuffer(fd, (char *)req, sizeof(TRequest));
}

/* Interpret the client's agreement. */
void transaction2(int fd, TRequest *req, TDataBase *db,
				  TSession *sessions)
{
	int ID = fd - 4;

	// Check for during transaction
	if (sessions[ID].duringTransaction == 0)
	{
		sendError(fd, FAILED_OPER, "Operatie esuata");
		return;
	}

	if (req->payload[0] == 'y')
	{
		// The answare was yes
		int indexIn = getIndex(db, sessions[ID].toCard);
		int indexOut = getIndex(db, sessions[ID].cardNum);

		// Do the transaction
		db->records[indexOut].sold -= sessions[ID].amount;
		db->records[indexIn].sold += sessions[ID].amount;

		// Send the confirmation
		memset(req->payload, 0, sizeof(req->payload));
		sprintf(req->payload, "IBANK> %s", "Transfer realizat cu succes");
		sendBuffer(fd, (char *)req, sizeof(TRequest));
	}
	else
	{
		// The answare was no
		sendError(fd, OPER_CANCELED, "Operatie anulata");
	}

	// Close during transaction
	sessions[ID].duringTransaction = 0;
}

/* Get the request from the client and interpret it. */
void getRequest(int fd, TRequest *req, TDataBase *db,
				TSession *sessions, int numSessions)
{
	int index, ID = fd - 4;

	if (!strcmp(req->type, "LOGI"))
	{
		loginReq(fd, req, db, sessions, numSessions);
	}
	else if (!strcmp(req->type, "LOGO") || !strcmp(req->type, "QUIT"))
	{
		memset(&sessions[ID], 0, sizeof(TSession));

		memset(req->payload, 0, sizeof(req->payload));
		sprintf(req->payload, "IBANK> Clientul a fost deconectat");

		sendBuffer(fd, (char *)req, sizeof(TRequest));
	}
	else if (!strcmp(req->type, "SOLD"))
	{
		index = getIndex(db, sessions[ID].cardNum);
		memset(req->payload, 0, sizeof(req->payload));
		sprintf(req->payload, "IBANK> %.2f", db->records[index].sold);

		sendBuffer(fd, (char *)req, sizeof(TRequest));
	}
	else if (!strcmp(req->type, "TRAN"))
	{
		transaction1(fd, req, db, sessions);
	}
	else if (!strcmp(req->type, "ANYC"))
	{
		transaction2(fd, req, db, sessions);
	}
}

/* Close all connection of the server with all clients on TCP socket. */
void closeConnections(int sockfd, int fdmax, fd_set *read_fds,
					  TSession *sessions, int numSessions)
{
	TRequest req;
	int i;

	// Send the information to all clients
	memset(&req, 0, sizeof(req));
	strcpy(req.type, "QUIT");
	strcpy(req.payload, "IBANK> Serverul se inchide.");

	for (i = 0; i <= fdmax; i++)
	{
		if (!FD_ISSET(i, read_fds) || i == sockfd)
			continue;

		if (i != STDIN_FILENO)
			send(i, (char *)(&req), sizeof(TRequest), 0);

		close(i);
		FD_CLR(i, read_fds);
	}

	// Close the socket and release memory for sessions
	free(sessions);
	close(sockfd);
}

int checkUnlockSession(int numSessions, TSession *sessions, char *cardNum)
{
	int i;
	for (i = 0; i < numSessions; i++)
		if (sessions[i].duringUnlock && !strcmp(cardNum, sessions[i].cardNum))
			return 0;

	return 1;
}

/* Unlock phase one. Check for card number and send the respose. */
void unlock1(int sockfdUDP, int numSessions, TSession *sessions, TDataBase *db, 
			 TRequest *req, struct sockaddr_in *addr_client, socklen_t addrlen, 
			 int ID)
{
	char cardNum[CARDNUMLEN];
	memset(cardNum, 0, CARDNUMLEN);
	strncpy(cardNum, req->payload, CARDNUMLEN);

	int index = getIndex(db, cardNum);

	if (!checkUnlockSession(numSessions, sessions, cardNum))
	{
		memset(req, 0, sizeof(TRequest));
		sprintf(req->payload, "UNLOCK> %d : %s", FAILED_UNLOCK, 
			    "Deblocare esuata");

		sendto(sockfdUDP, req, sizeof(TRequest), 0,
				(struct sockaddr*)addr_client, addrlen);

		return;
	}

	//  Check if card number exists
	if (index < 0)
	{
		memset(req, 0, sizeof(TRequest));
		strcpy(req->type, "EROR");
		sprintf(req->payload, "UNLOCK> %d Cod eroare", CARD_INEXISTENT);

		sendto(sockfdUDP, req, sizeof(TRequest), 0,
				(struct sockaddr*)addr_client, addrlen);
		return;
	}

	if (db->records[index].isLocked)
	{
		// Card is locked. Send the request to give password
		memset(req, 0, sizeof(TRequest));
		sprintf(req->payload, "UNLOCK> %s", "Trimite parola secreta");

		sessions[ID].duringUnlock = 1;
		strcpy(sessions[ID].cardNum, cardNum);

		sendto(sockfdUDP, req, sizeof(TRequest), 0,
				(struct sockaddr*)addr_client, addrlen);
	}
	else
	{
		// Card is unlocked. Send error to client
		memset(req, 0, sizeof(TRequest));
		strcpy(req->type, "EROR");
		sprintf(req->payload, "UNLOCK> %s %d", "Cod eroare", FAILED_OPER);

		sendto(sockfdUDP, req, sizeof(TRequest), 0,
			   (struct sockaddr*)addr_client, 
			   sizeof(struct sockaddr_in));
	}
}

/* Unlock phase two. Check password to unlock the card. */
void unlock2(int sockfdUDP, int numSessions, TSession *sessions, TRequest *req,
			 TDataBase *db, struct sockaddr_in *addr_client, int ID)
{
	int i;
	char cardNum[CARDNUMLEN], secret[SECRETLEN];
	memset(cardNum, 0, CARDNUMLEN);
	memset(secret, 0, SECRETLEN);

	// Get card number and password from payload
	sscanf(req->payload, "%s %s", cardNum, secret);
	int index = getIndex(db, cardNum);

	// Check the password.
	if (!strcmp(db->records[index].password, secret))
	{
		// Unlock the card.
		for (i = 0; i < numSessions; i++)
			if (!strcmp(sessions[i].cardNum, cardNum))
				sessions[i].numTries = 0;

		db->records[index].isLocked = 0;
		memset(sessions[ID].cardNum, 0, CARDNUMLEN);

		memset(req, 0, sizeof(TRequest));

		sprintf(req->payload, "UNLOCK> %s", "Card deblocat");
		sendto(sockfdUDP, req, sizeof(TRequest), 0,
						(struct sockaddr *)addr_client, 
						sizeof(struct sockaddr_in));
	}
	else
	{
		// Wrong password. Keep card locked.
		sprintf(req->payload, "UNLOCK> %d : %s", FAILED_UNLOCK, 
				"Deblocare esuata");

		sendto(sockfdUDP, req, sizeof(TRequest), 0,
						(struct sockaddr *)addr_client, 
						sizeof(struct sockaddr_in));
	}

	sessions[ID].duringUnlock = 0;
}

/* Unlock the card session. */
void unlockCard(int sockfdUDP, int numSessions, TSession *sessions, 
				TDataBase *db, int ID)
{
	TRequest req;
	struct sockaddr_in addr_client;
	socklen_t addrlen = sizeof(struct sockaddr_in);

	memset(&req, 0, sizeof(TRequest));
	memset(&addr_client, 0, sizeof(addr_client));

	// Get the request to unlock the card
	int len = recvfrom(sockfdUDP, &req, sizeof(req), 0,
				   (struct sockaddr *)&addr_client, &addrlen);
	if (len < 0)
		error("ERROR in recvfrom");

	// Check phase of unlocking.
	if (!strcmp(req.type, "ULK1"))
	{
		unlock1(sockfdUDP, numSessions, sessions, db, &req, &addr_client, 
				addrlen, ID);
	}
	else if (!strcmp(req.type, "ULK2"))
	{
		unlock2(sockfdUDP, numSessions, sessions, &req, db, &addr_client, ID);
	}
}

/* Run server */
void runServer(int fdmax, int sockfdTCP, int sockfdUDP, fd_set *read_fds, 
			   TDataBase *db, int numSessions, TSession *sessions)
{
	int n, i;
	char buffer[BUFLEN];
	fd_set tmp_fds;
	
	FD_ZERO(&tmp_fds);

	// Run server forever
	while (1)
	{
		tmp_fds = *read_fds;
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
			error("ERROR in select");

		// For each file descriptor
		for (i = 0; i <= fdmax; i++)
		{
			if (!FD_ISSET(i, &tmp_fds))
				continue;

			if (i == sockfdTCP)
			{
				// Establish a connection with a new client
				fdmax = establishConnection(sockfdTCP, read_fds, fdmax,
											&sessions, &numSessions);
			}
			else if (i == sockfdUDP)
			{
				// Unlock the card
				unlockCard(sockfdUDP, numSessions, sessions, db, i - 4);
			}
			else if (i == STDIN_FILENO)
			{
				// Get text from stdin
				memset(buffer, 0, BUFLEN);
				fgets(buffer, BUFLEN - 1, stdin);

				// Quit or command does not exit.
				if (!strcmp(buffer, "quit\n"))
				{
					closeConnections(sockfdTCP, fdmax, read_fds,
									 sessions, numSessions);

					free(db->records);
					close(sockfdTCP);
					close(sockfdUDP);

					return;
				}
				else
					printf("Comanda nu exista!\n");
			}
			else
			{
				memset(buffer, 0, BUFLEN);
				if ((n = recv(i, buffer, sizeof(buffer), 0)) <= 0)
				{
					if (n == 0)
						printf("selectserver: socket %d hung up\n", i);
					else
						error("ERROR in recv");
					close(i);
					FD_CLR(i, read_fds);
				}
				else
				{
					// Interpret requests from clients
					getRequest(i, (TRequest *)buffer, db, sessions,
							   numSessions);
				}
			}
		}
	}
}

int main(int argc, char *argv[])
{
	int fdmax, sockfdTCP, sockfdUDP, portNum, numSessions;
	fd_set read_fds;
	TSession *sessions;
	TDataBase db;

	// Check args
	checkArgs(argc, argv);

	// Open DB
	openDB(argv[2], &db);

	// Set port number
	portNum = atoi(argv[1]);

	// Set socket
	sockfdTCP = initServerTCP(portNum);
	sockfdUDP = initServerUDP(portNum);

	// Clear descriptor set
	FD_ZERO(&read_fds);

	// Add sockets to file descriptor set
	FD_SET(sockfdTCP, &read_fds);
	FD_SET(sockfdUDP, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);

	fdmax = sockfdTCP;
	numSessions = fdmax - 2;

	// Init sessions to 0 (closed)
	sessions = initSessions(numSessions);
	if (!sessions)
	{
		free(db.records);
		return -1;
	}

	// Run server
	runServer(fdmax, sockfdTCP, sockfdUDP, &read_fds, &db, 
			  numSessions, sessions);

	return 0;
}
