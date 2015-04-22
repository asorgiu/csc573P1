/******************************************************************************
 *
 *  File Name........: server.c
 *
 *  Description......:
 *  The server waits for connections from the peers on the well-known port 7734. The server maintains two data structures: a list with information about the 
 *  currently active peers and the index of RFCs available at each peer. For simplicity, you will implement both these structures as linked lists; while such
 *  an implementation is obviously not scalable to very large number of peers and/or RFCs, it will do for this project.
 *
 *  Each item of the linked list of peers contains two elements:
 *    1. the hostname of the peer (of type string), and
 *    2. the port number (of type integer) to which the upload server of this peer is listening.
 *
 *  
Each item of the linked list representing the index of RFCs contains these elements:
 *    • the RFC number (of type integer),
 *    • the title of the RFC (of type string), and
 *    • the hostname of the peer containing the RFC (of type string).
 *
 *  Note that the index may contain multiple records of a given RFC, one record for each peer that contains the RFC.
 *  Initially (i.e., when the server starts up), both linked lists are empty (do not contain any records). The linked lists are updated as peers join and leave
 *  the system. When a peer joins the system, it provides its hostname and upload port to the server (as explained below) and the server creates a new peer
 *  record and inserts it at the front of the linked list. The peer also provides the server with a list of RFCs that it has. For each RFC, the server creates
 *  an appropriate record and inserts it at the front of the linked list representing the index.
 *  When a peer leaves the system (i.e., closes its connection to the server), the server searches both linked lists and removes all records associated with
 *  this peer. As we mentioned earlier, the server must be able to handle multiple simultaneous connections from peers. To this end, it has a main process that
 *  initializes the two linked-lists to empty and then listens to the well-known port 7734. When a connection from a peer is received, the main process spawns
 *  a new process that handles all communication with this peer. In particular, this process receives the information from the peer and updates the peer list    
 *  and index, and it also returns any information requested by the peer. When the peer closes the connection, this process removes all records associated with
 *  the peer and then terminates.
 *
 *****************************************************************************/

/*........................ Include Files ....................................*/
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#define DEBUG printf
//#define DEBUG //

#define LEN 200
#define MAX_MSG_SIZE 2000
#define WELL_KNOWN_PORT 7734
#define MAX_CLIENTS 100

typedef struct peer {
	char hostname[LEN];
	int port;
	int socket;
} peer;

typedef struct rfc {
	int number;
	int port;
	char title[LEN];
	char peerHostname[LEN];
} rfc;

typedef struct peerList {
	peer* item;
	struct peerList* next;
} peerList;

typedef struct rfcList {
	rfc* item;
	struct rfcList* next;
} rfcList;

struct peerList *peerHead = NULL;
struct peerList *peerTail = NULL;
struct rfcList *rfcHead = NULL;
struct rfcList *rfcTail = NULL;

int clientList[MAX_CLIENTS];  // Array of connected client sockets
fd_set readset;               // Set of sockets to 'select' on
int listenSocket;             // Socket to listen for incoming connections
int maxfd;                    // highest number socket for 'select'

struct peerList* createPeerList(peer* item)
{
	DEBUG("createPeerList()\n");
    printf("   creating list with headnode [%s]\n",item->hostname);
    struct peerList *ptr = (struct peerList*)malloc(sizeof(struct peerList));
    if(ptr == NULL)
    {
        printf("Node creation failed \n");
        return NULL;
    }
    ptr->item = item;
    ptr->next = NULL;

    peerHead = peerTail = ptr;
    return ptr;
}
struct rfcList* createRfcList(rfc* item)
{
	DEBUG("createRfcList()\n");
    printf("   creating list with headnode as [%d]\n",item->number);
    struct rfcList *ptr = (struct rfcList*)malloc(sizeof(struct rfcList));
    if(ptr == NULL)
    {
        printf("Node creation failed \n");
        return NULL;
    }
    ptr->item = item;
    ptr->next = NULL;

    rfcHead = rfcTail = ptr;
    return ptr;
}

struct peerList* addToPeerList(peer* item)
{
	DEBUG("addToPeerList()\n");
    if(peerHead == NULL)
    {
        return (createPeerList(item));
    }

    struct peerList *ptr = (struct peerList*)malloc(sizeof(struct peerList));
    if(ptr == NULL)
    {
        printf("Node creation failed \n");
        return NULL;
    }
    ptr->item = item;
    ptr->next = NULL;

	// Put it at the end of the linked list
    peerTail->next = ptr;
    peerTail = ptr;

    return ptr;
}
struct rfcList* addToRfcList(rfc* item)
{
	DEBUG("addToRfcList()\n");
    if(rfcHead == NULL)
    {
        return (createRfcList(item));
    }

    struct rfcList *ptr = (struct rfcList*)malloc(sizeof(struct rfcList));
    if(ptr == NULL)
    {
        printf("Node creation failed \n");
        return NULL;
    }
    ptr->item = item;
    ptr->next = NULL;

	// Put it at the end of the linked list
    rfcTail->next = ptr;
    rfcTail = ptr;

    return ptr;
}

struct peerList* searchInPeerList(char* host, peerList** prev)
{
    struct peerList *ptr = peerHead;
    struct peerList *tmp = NULL;
    bool found = false;
    DEBUG("searchInPeerList()\n");

    DEBUG("   Searching the list for value [%s] \n", host);

    while(ptr != NULL)
    {
        if(strcmp(ptr->item->hostname, host) == 0)
        {
        	DEBUG("      Found peer in peerList\n");
            found = true;
            break;
        }
        else
        {
            tmp = ptr;
            ptr = ptr->next;
        }
    }

    if(found == true)
    {
    	// save previous in case we need to delete the last item
        if(prev)
            *prev = tmp;
        return ptr;
    }
    else
    {
        return NULL;
    }
}
struct rfcList* searchInRfcList(int rfcNum, rfcList** prev)
{
    struct rfcList *ptr = rfcHead;
    struct rfcList *tmp = NULL;
    bool found = false;
    DEBUG("searchInRfcList()\n");

    DEBUG("   Searching the list for value [%d] \n", rfcNum);

    while(ptr != NULL)
    {
        if(ptr->item->number == rfcNum)
        {
        	DEBUG("      Found rfc in rfcList\n");
            found = true;
            break;
        }
        else
        {
            tmp = ptr;
            ptr = ptr->next;
        }
    }

    if(found == true)
    {
    	// save previous in case we need to delete the last item
        if(prev)
            *prev = tmp;
        return ptr;
    }
    else
    {
        return NULL;
    }
}
struct rfcList* searchPeerInRfcList(char* host, rfcList** prev)
{
    struct rfcList *ptr = rfcHead;
    struct rfcList *tmp = NULL;
    bool found = false;
    DEBUG("searchPeerInRfcList()\n");

    printf("Searching the list for value [%s] \n", host);

    while(ptr != NULL)
    {
        if(strcmp(ptr->item->peerHostname, host) == 0)
        {
            found = true;
            break;
        }
        else
        {
            tmp = ptr;
            ptr = ptr->next;
        }
    }

    if(found == true)
    {
    	// save previous in case we need to delete the last item
        if(prev)
            *prev = tmp;
        return ptr;
    }
    else
    {
        return NULL;
    }
}

struct peerList* findPeerBySocket(int peerSocket)
{
    struct peerList *ptr = peerHead;
    struct peerList *tmp = NULL;
    bool found = false;
    DEBUG("findPeerBySocket()\n");

    printf("Searching the peer list for socket [%d] \n", peerSocket);

    while(ptr != NULL)
    {
        if(ptr->item->socket == peerSocket)
        {
            found = true;
            break;
        }
        else
        {
            ptr = ptr->next;
        }
    }
    
    if (found == true)
    {
    	return ptr;
    }
    else {
    	return NULL;
    }

}

int deleteFromPeerList(char* host)
{
    struct peerList *prev = NULL;
    struct peerList *del = NULL;
    DEBUG("deleteFromPeerList()\n");

    printf("   Deleting value [%s] from list\n", host);

    del = searchInPeerList(host, &prev);
    if(del == NULL)
    {
        return -1;
    }
    else
    {
    	DEBUG("      Found peer to delete\n");
        if(prev != NULL) {
        	DEBUG("         Found a previous item in list\n");
            prev->next = del->next;
        }

        if(del == peerTail)
        {
        	DEBUG("         Deleting Tail\n");
            peerTail = prev;
        }
        
        if(del == peerHead)
        {
        	DEBUG("         Deleting Head\n");
            peerHead = del->next;
        }
    }

	free(del->item);
    free(del);
    del = NULL;

    return 0;
}
// This function will cycle through the rfcList and delete ALL items
// with the peerHostname of 'host'
int deletePeerFromRfcList(char* host)
{
    struct rfcList *prev = NULL;
    struct rfcList *del = NULL;
    bool found = false;
    DEBUG("deletePeerFromRfcList()\n");

    printf("   Deleting all values [%s] from list\n", host);

    del = searchPeerInRfcList(host, &prev);
    while (del != NULL)
    {
    	DEBUG("      Found RFC to delete\n");
    	found = true;

        if(prev != NULL)
            prev->next = del->next;

        if(del == rfcTail)
        {
            rfcTail = prev;
        }
        
        if(del == rfcHead)
        {
            rfcHead = del->next;
        }
        free(del->item);
        free(del);
        del = searchPeerInRfcList(host, &prev);
    }

    if(found == false)
    {
        return -1;
    }
	else
	{
    	return 0;
    }
}

int isNumeric(char* String) {
    char* ptr = String;
    while(*ptr && isdigit(*ptr))
        ptr++;
    return(*ptr ? 0 : 1);
}

/** Returns 1 on success, or 0 if there was an error */
int setSocketBlockingEnabled(int fd, int blocking)
{
	DEBUG("setSocketBlockingEnabled()\n");
	if (fd < 0) return 0;

	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) return 0;
	flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
	return (fcntl(fd, F_SETFL, flags) == 0) ? 1 : 0;
}

/*void closeAllSockets(int s, playerInfo playerData[], int numPlayers) {
    char str[LEN];
	int len;
	DEBUG("closeAllSockets()\n");
    if (&playerData[0] != NULL) {
        int i;
        for (i=0; i<numPlayers; i++) {
            // Send player the STOP message
        	memset(&str, 0, sizeof(str));
        	str[0] = 'S';
        	len = send(playerData[i].playerSocket, "S", 1, MSG_NOSIGNAL);
        	if ( len != strlen(str) ) {
        	    perror("send");
    	        exit(1);
	        }
			// Close player's socket
			//printf("Debug: Closing socket for player %d\n", i);
            close(playerData[i].playerSocket);
        }
        close(s);
    }
}*/

void handleClientDisconnect(int clientNum)
{
	struct peerList *tmpList;
	DEBUG("handleClientDisconnect()\n");
	
	tmpList = findPeerBySocket(clientNum);
	if (tmpList != NULL) {
		// Delete all of the disconnected peer's rfc data
		deletePeerFromRfcList(tmpList->item->hostname);
		// Now remove it from the list of connected peers
		deleteFromPeerList(tmpList->item->hostname);
		// Then close the connection to the peer
		close(clientList[clientNum]);
		// And remove it from the client list
		clientList[clientNum] = 0;
	}
	else {
		printf("   ERROR: Client not found!\n");
	}
}

// This updates the list of socket connections that select() will wake on
// Our listenSocket as well as any connected clients will be added
void updateSelectList()
{
	int i;
	DEBUG("updateSelectList()\n");
	FD_ZERO(&readset);
	FD_SET(listenSocket, &readset);
	maxfd = listenSocket;
	
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (clientList[i] != 0) {
			DEBUG("   Adding socket %d in position %d\n", clientList[i], i);
			FD_SET(clientList[i], &readset);
			maxfd = (clientList[i] > maxfd) ? clientList[i] : maxfd;
			DEBUG("   maxfd = %d\n", maxfd);
		}
	}
}

char* getTagValue(char *data, char *tag)
{
	char *datacopy = malloc(strlen(data) + 1); // strtok modifies the string
	char *result = 0;
	char *s;
	DEBUG("getTagValue() - %s\n", tag);
	
	strcpy(datacopy, data);
	
	s = strtok(datacopy, " \n\r");
	
	while (s)
	{
		if (strcmp(s, tag) == 0)
		{
			// When looking for title, it can have spaces, so do not
			// add " " as a delimiter.
			if (strcmp(tag, "Title:") == 0) {
				s = strtok(0, "\n\r");
			}
			else {
				s = strtok(0, " \n\r"); // the next token should be our value
			}
			DEBUG("      found value - [%s]\n", s);
			result = malloc(strlen(s) +1);
			strcpy(result, s);
			free(datacopy);
			return result;
		}
		s = strtok(0, " \n\r");
	}
	free(datacopy);
	
	return result;
}

char* getTagVersion(char *data, int versionPosition)
{
	char *datacopy = malloc(strlen(data) + 1); // strtok modifies the string
	char *version;
	char *result = 0;
	DEBUG("getTagVersion() - version at %d\n", versionPosition);
	
	strcpy(datacopy, data);
	
	version = strtok(datacopy, " \r\n");
	versionPosition--;
	
	// we know the version is the nth token position based on
	// versionPosition passed in
	while (versionPosition)
	{
		version = strtok(0, " \r\n");
		versionPosition--;
	}
	
	result = malloc(strlen(version) + 1);
	strcpy(result, version);
	free(datacopy);
	
	return result;
}

int isVersionOk(char *version) {
	DEBUG("isVersionOk()\n");
	if (strcmp(version, "P2P-CI/1.0") == 0) {
		//they match
		return 1;
	}
	else {
		return 0;
	}
}

void send400(int clientNum) {
	DEBUG("send400()\n");
	char message[] = "P2P-CI/1.0 400 Bad Request\r\n\r\n";
	
	send(clientList[clientNum], message, strlen(message), 0);
}

void send404(int clientNum) {
	DEBUG("send404()\n");
	char message[] = "P2P-CI/1.0 404 P2P-CI Not Found\r\n\r\n";
	
	send(clientList[clientNum], message, strlen(message), 0);
}

void send505(int clientNum) {
	DEBUG("send505()\n");
	char message[] = "P2P-CI/1.0 505 P2P-CI Version Not Supported\r\n\r\n";
	
	send(clientList[clientNum], message, strlen(message), 0);
}

void sendRfcQueryResponse(rfcList* resultList, int clientNum)
{
	DEBUG("sendRfcQueryResponse()\n");
	char replyMessage[MAX_MSG_SIZE];
	char rfcNumString[10];
	char portNumString[10];
	memset(&replyMessage, 0, MAX_MSG_SIZE);
	
	if (resultList == NULL) {
		// Nothing was found
		send404(clientNum);
	}
	else {
		strcpy(replyMessage, "P2P-CI/1.0 200 OK\r\n");
		while (resultList != NULL) {
			sprintf(rfcNumString, "%d", resultList->item->number);
			sprintf(portNumString, "%d", resultList->item->port);
			strcat(replyMessage, "RFC ");
			strcat(replyMessage, rfcNumString);
			strcat(replyMessage, " ");
			strcat(replyMessage, resultList->item->title);
			strcat(replyMessage, " ");
			strcat(replyMessage, resultList->item->peerHostname);
			strcat(replyMessage, " ");
			strcat(replyMessage, portNumString);
			strcat(replyMessage, "\r\n");
			resultList = resultList->next;
		}
		strcat(replyMessage, "\r\n");
		send(clientList[clientNum], replyMessage, strlen(replyMessage), 0);
	}
}

void add(char* data, int clientNum)
{
	DEBUG("add()\n");
	int rfcNum;
	char *rfcNumString;
	char *host;
	char *version;
	int port;
	char *portString;
	char *title;
	char replyMessage[MAX_MSG_SIZE];
	memset(&replyMessage, 0, MAX_MSG_SIZE);
	struct rfc* newRfc = (struct rfc*)malloc(sizeof(struct rfc));

	rfcNumString = getTagValue(data, "RFC");    DEBUG("   RFC = %s\n", rfcNumString);
	version      = getTagVersion(data, 4);      DEBUG("   Version = %s\n", version);
	host         = getTagValue(data, "Host:");  DEBUG("   Host = %s\n", host);
	portString   = getTagValue(data, "Port:");  DEBUG("   Port = %s\n", portString);
	title        = getTagValue(data, "Title:"); DEBUG("   Title = %s\n", title);
	
	// Check version
	if (!isVersionOk(version)) {
		send505(clientNum);
		return;
	}
	
	newRfc->number = atoi(rfcNumString);
	newRfc->port   = atoi(portString);
	strcpy(newRfc->peerHostname, host);
	strcpy(newRfc->title, title);
	
	addToRfcList(newRfc);
	
	// Send OK reply
	strcpy(replyMessage, "P2P-CI/1.0 200 OK\r\nRFC ");
	strcat(replyMessage, rfcNumString);
	strcat(replyMessage, " ");
	strcat(replyMessage, title);
	strcat(replyMessage, " ");
	strcat(replyMessage, host);
	strcat(replyMessage, " ");
	strcat(replyMessage, portString);
	strcat(replyMessage, "\r\n\r\n");
	send(clientList[clientNum], replyMessage, strlen(replyMessage), 0);
}

void lookup(char* data, int clientNum)
{
	DEBUG("lookup()\n");
	int rfcNum;
	char *rfcNumString;
	char *host;
	char *version;
	int port;
	char *portString;
	char *title;
	char reply[MAX_MSG_SIZE];
	rfcList *resultList = (struct rfcList*)malloc(sizeof(struct rfcList));
	rfcList *currList = NULL;

	rfcNumString = getTagValue(data, "RFC");    DEBUG("   RFC = %s\n", rfcNumString);
	version      = getTagVersion(data, 4);      DEBUG("   Version = %s\n", version);
	host         = getTagValue(data, "Host:");  DEBUG("   Host = %s\n", host);
	portString   = getTagValue(data, "Port:");  DEBUG("   Port = %s\n", portString);
	title        = getTagValue(data, "Title:"); DEBUG("   Title = %s\n", title);
	rfcNum = atoi(rfcNumString);

	// Check version
	if (!isVersionOk(version)) {
		send505(clientNum);
		return;
	}
	
	strcpy(&reply, version);
	// Look through the entire rfcList for this RFC
	struct rfcList *ptr = rfcHead;
    bool found = false;

    DEBUG("   Searching the list for value [%d] \n", rfcNum);

    while(ptr != NULL)
    {
        if(ptr->item->number == rfcNum)
        {
        	DEBUG("      Found rfc in rfcList\n");
        	// Add this rfc to the reply
        	if (found == false) {
        		// add as the first RFC found
        		resultList->item = ptr->item;
        		resultList->next = NULL;
        		currList = resultList;
        		found = true;
        	}
        	else {
        		currList->next = (struct rfcList*)malloc(sizeof(struct rfcList));
        		currList = currList->next;
        		currList->item = ptr->item;
        		currList->next = NULL;
        	}
            
            ptr = ptr->next;
        }
        else
        {
            ptr = ptr->next;
        }
    }

	if (found == false) {
		sendRfcQueryResponse(NULL, clientNum);
	}
	else 
	{
	    sendRfcQueryResponse(resultList, clientNum);
	}
}

void list(char* data, int clientNum)
{
	DEBUG("list()\n");
	char *version;
	char reply[MAX_MSG_SIZE];

	version      = getTagVersion(data, 3);      DEBUG("   Version = %s\n", version);

	// Check version
	if (!isVersionOk(version)) {
		send505(clientNum);
		return;
	}
	
	// Sending rfcHead will send ALL RFCs on the server
	sendRfcQueryResponse(rfcHead, clientNum);

}

void handleNewClient()
{
	int i, len;
	int socketSaved = 0;
	int newSocket; /* Socket file descriptor for incoming connections */

	DEBUG("handleNewClient()\n");
	// New client is connecting. Look for a spot in the clientList
	newSocket = accept(listenSocket, NULL, NULL);
	if (newSocket < 0) {
		perror("accept");
		exit(listenSocket);
	}
	for (i = 0; (i < MAX_CLIENTS) && (socketSaved == 0); i++) {
		if (clientList[i] == 0) {
			DEBUG("   Client accepted:   FD=%d; index=%d\n", newSocket, i);
			clientList[i] = newSocket;
			// Create a new peer, save data, and add to peerList
			socketSaved = 1;
		}
	}
	if (socketSaved != 1)
	{
		// Our array is full
		printf("   No space left for new client!\n");
		close(newSocket);
		return;
	}
	
	char buf[LEN];
	memset(&buf, 0, sizeof(buf));
	// Peer is going to send it's hostname and port number after connection
    struct peer* newPeer = (struct peer*)malloc(sizeof(struct peer));
	memset(newPeer, 0, sizeof(struct peer));
	//len = recv(newSocket, newPeer->hostname, LEN, 0);
	len = recv(newSocket, buf, LEN-1, 0);
	if ( len < 0 ) {
		perror("recv");
		exit(1);
	}
	memcpy(&newPeer->hostname[0], &buf, len);
	newPeer->hostname[strlen(buf)] = '\0';
	DEBUG("   Received host [%s]\n", newPeer->hostname);
	
	// Send Ack
	char str[2];  // Ack string
	str[0] = 'A';
    str[1] = '\0';
    len = send(newSocket, str, strlen(str), 0);
    if ( len != strlen(str) ) {
        perror("send");
        exit(1);
    }

	// Receive port number
	int32_t ret;
	len = recv(newSocket, &ret, sizeof(ret), 0);
    if ( len < 0 ) {
        perror("recv");
        exit(1);
    }
	newPeer->port = ntohl(ret);
	DEBUG("   Received port [%d]\n", newPeer->port);

	// Send Ack
    len = send(newSocket, str, strlen(str), 0);
    if ( len != strlen(str) ) {
        perror("send");
        exit(1);
    }
    
    // Add the new peer to the peerList
    addToPeerList(newPeer);
	
	setSocketBlockingEnabled(newSocket, 0);
}

void handleData(int clientNum) 
{
	char buf[LEN];
	char data[MAX_MSG_SIZE];
	int j, len, totalLen = 0;
	
	DEBUG("handleData() from client %d\n", clientNum);
	memset(&data, '\0', MAX_MSG_SIZE);
	
    while (1)
    {
        int err;
        memset(&buf, '\0', LEN);
        len = recv(clientList[clientNum], buf, LEN-1, 0);
        err = errno; // save off errno
        //printf("Debug: recv = %d", len);
        if ( len < 0 ) {
        	totalLen = 0;
            if (( err == EAGAIN ) || (err == EWOULDBLOCK)) { // No more data
                printf("   DEBUG: Data received -\n   [%s]\n", data);
                // Check to see which command was received
                // Valid methods: ADD, LOOKUP, LIST
                if (data[0] == 'A' && data[1] == 'D' && data[2] == 'D') {
                	add(&data, clientNum);
                } else if (data[0] == 'L' && data[1] == 'O' && data[2] == 'O') {
                	lookup(&data, clientNum);
                } else if (data[0] == 'L' && data[1] == 'I' && data[2] == 'S') {
                	list(&data, clientNum);
                } else {
                	printf("   ERROR: Invalid command - %s\n", data);
                }
				return;
            }
            perror("recv");
        }
        else if (len == 0) {
            // We got a close
            printf("   Debug: Close from client %d\n", clientNum);
            handleClientDisconnect(clientNum);
            break;
        }
        else {
            memcpy(&data[totalLen], &buf, len);
            totalLen += len;
            DEBUG("   Received [%s]\n", buf);
        }
    } // while


}

// Some socket is ready for reading. Handle it
void handleSocketRead() {
	int i;
	
	DEBUG("handleSocketRead()\n");
	// See if a new client is trying to connect to the listening socket
	if (FD_ISSET(listenSocket, &readset))
		handleNewClient();
		
	// Loop through clients to see if one of them is sending data
	for (i=0; i < MAX_CLIENTS; i++) {
		if (FD_ISSET(clientList[i], &readset)) {
			handleData(i);
		}
	}		
}

main (int argc, void *argv[])
{
    char buf[LEN];
    char host[LEN];
    char str[LEN];
    int p, fp, rc, len, port, numPlayers, numHops, a, flags, result;
    struct hostent *hp, *ihp;
    struct sockaddr_in sin, incoming;
    fd_set tempset;
    struct timeval tv;
    int on=1;

    memset(&sin, 0, sizeof(sin));
    memset(&incoming, 0, sizeof(incoming));
    
    port = WELL_KNOWN_PORT;
    
    /* fill in hostent struct for self */
    gethostname(host, sizeof(host));
    hp = gethostbyname(host);
    DEBUG("Hostname: %s\n", host);
    if ( hp == NULL ) {
        fprintf(stderr, "%s: host not found (%s)\n", argv[0], host);
        exit(1);
    }
    
    /* open a socket for listening
     * 4 steps:
     *	1. create socket
     *	2. bind it to an address/port
     *	3. listen
     *	4. accept a connection
     */
    
    /* use address family INET and STREAMing sockets (TCP) */
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if ( listenSocket < 0 ) {
        perror("socket:");
        exit(listenSocket);
    }
    
    // The setsockopt() function is used so the local address
    // can be reused when the server is restarted before the required
    // wait time expires
	if((rc = setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))) < 0)
	{
		perror("setsockopt() error");
		close(listenSocket);
		exit (-1);
	}
	
	setSocketBlockingEnabled(listenSocket, 0);
	
    /* set up the address and port */
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    memcpy(&sin.sin_addr, hp->h_addr_list[0], hp->h_length);
    
    /* bind socket s to address sin */
    rc = bind(listenSocket, (struct sockaddr *)&sin, sizeof(sin));
    if ( rc < 0 ) {
        perror("bind:");
        exit(rc);
    }
    
    rc = listen(listenSocket, 5);
    if ( rc < 0 ) {
        perror("listen:");
        exit(rc);
    }
    
    maxfd = listenSocket; // Only one so far
    memset((char *)&clientList, 0, sizeof(clientList));
    
    /* accept connections and handle data */
    int i, j;
    
    while (1) {
    	updateSelectList();
    	tv.tv_sec = 10;
        tv.tv_usec = 0;
        
        // select() returns the number of sockets that are ready for reading
        result = select(maxfd + 1, &readset, NULL, NULL, &tv);
        
        if (result == 0) { // select timed out
        } 
        else if (result < 0 && errno != EINTR) {
            perror("select");
            exit(1);
        }
        else {
        	handleSocketRead();
        }
    }
}
