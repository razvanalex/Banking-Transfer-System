/* Sizes defined by this protocol (ad-hoc manner) */
#define TYPELEN     		  5
#define PAYLOADLEN  		 59

/* Constants from constraints */
#define FNLEN       		 13
#define LNLEN       		 13
#define CARDNUMLEN  		  7
#define SECRETLEN   		  9
#define PINLEN      		  5

/* Error codes */
#define NOT_LOGGED			 -1
#define	SESSION_EXISTS		 -2
#define WRONG_PIN			 -3
#define CARD_INEXISTENT		 -4
#define LOCKED_CARD			 -5
#define FAILED_OPER			 -6
#define FAILED_UNLOCK		 -7
#define NO_MONEY			 -8
#define OPER_CANCELED		 -9
#define INTERNAL_ERROR		-10

/* Structure used for sending a request */
typedef struct __attribute__((__packed__)) {
	char type[TYPELEN];				// Type of the request
	char payload[PAYLOADLEN];		// The payload (message) of the request
} TRequest;

/* Structure for a record in our database */
typedef struct __attribute__((__packed__)) {
	char lastname[LNLEN];			// Last name of client
	char firstname[FNLEN];			// First name of client
	char cardNum[CARDNUMLEN];		// Card number of client
	char pin[PINLEN];				// Pin to login
	char password[SECRETLEN];		// Secret password to unlock the card
	double sold;					// Current amount of money
	char isLocked;					// State of card (locked/unlocked)
} TDBRecord;

/* A database */
typedef struct __attribute__((__packed__)) {
	int size;						// The size of database
	TDBRecord* records;				// An array of records
} TDataBase;

/* A structure for a session */
typedef struct __attribute__((__packed__)) {
	char numTries;					// Number of tries to login
	char isOpened;					// If session is open or not
	char cardNum[CARDNUMLEN];		// The card number of opened session
	char duringTransaction;			// If session is during a transaction
	char toCard[CARDNUMLEN];		// Destination of transaction
	double amount;					// Amount of money for current transaction
	char duringUnlock;				// If current client is in unlock session
} TSession;
