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

#define LEN 100
#define BUF_SIZE 200000
#define WELL_KNOWN_PORT 7734

typedef struct peer {
	char hostname[LEN];
	int port;
} peer;

typedef struct rfc {
	int number;
	char title[LEN];
	char peerHostname[LEN];
} rfc;

struct peerList {
	peer item;
	struct peerList* next;
};

struct rfcList {
	rfc item;
	struct rfcList* next;
};

struct peerList *peerHead = NULL;
struct peerList *peerTail = NULL;
struct rfcList *rfcHead = NULL;
struct rfcList *rfcTail = NULL;

struct peerList* createPeerList(peer item)
{
    printf("\n creating list with headnode [%s]\n",val->item.hostname);
    struct peerList *ptr = (struct peerList*)malloc(sizeof(struct peerList));
    if(ptr == NULL)
    {
        printf("\n Node creation failed \n");
        return NULL;
    }
    ptr->item = item;
    ptr->next = NULL;

    peerHead = peerTail = ptr;
    return ptr;
}
struct rfcList* createRfcList(rfc item)
{
    printf("\n creating list with headnode as [%d]\n",val->item.number);
    struct rfcList *ptr = (struct rfcList*)malloc(sizeof(struct rfcList));
    if(ptr == NULL)
    {
        printf("\n Node creation failed \n");
        return NULL;
    }
    ptr->item = item;
    ptr->next = NULL;

    rfcHead = rfcTail = ptr;
    return ptr;
}

struct peerList* addToPeerList(peer item)
{
    if(peerHead == NULL)
    {
        return (createPeerList(item));
    }

    struct peerList *ptr = (struct peerList*)malloc(sizeof(struct peerList));
    if(ptr == NULL)
    {
        printf("\n Node creation failed \n");
        return NULL;
    }
    ptr->item = item;
    ptr->next = NULL;

	// Put it at the end of the linked list
    peerTail->next = ptr;
    peerTail = ptr;

    return ptr;
}
struct rfcList* addToRfcList(rfc item)
{
    if(rfcHead == NULL)
    {
        return (createRfcList(item));
    }

    struct rfcList *ptr = (struct rfcList*)malloc(sizeof(struct rfcList));
    if(ptr == NULL)
    {
        printf("\n Node creation failed \n");
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

    printf("\n Searching the list for value [%s] \n", host);

    while(ptr != NULL)
    {
        if(strcmp(ptr->item.hostname, host) == 0)
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
struct rfcList* searchInRfcList(int rfcNum, rfcList** prev)
{
    struct rfcList *ptr = rfcHead;
    struct rfcList *tmp = NULL;
    bool found = false;

    printf("\n Searching the list for value [%d] \n", rfcNum);

    while(ptr != NULL)
    {
        if(ptr->item.number == rfcNum)
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
struct rfcList* searchPeerInRfcList(char* host, rfcList** prev)
{
    struct rfcList *ptr = rfcHead;
    struct rfcList *tmp = NULL;
    bool found = false;

    printf("\n Searching the list for value [%s] \n", host);

    while(ptr != NULL)
    {
        if(strcmp(ptr->item.peerHostname, host) == 0)
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

int deleteFromPeerList(char* host)
{
    struct peerList *prev = NULL;
    struct peerList *del = NULL;

    printf("\n Deleting value [%s] from list\n", host);

    del = searchInPeerList(host, &prev);
    if(del == NULL)
    {
        return -1;
    }
    else
    {
        if(prev != NULL)
            prev->next = del->next;

        if(del == peerTail)
        {
            peerTail = prev;
        }
        else if(del == peerHead)
        {
            peerHead = del->next;
        }
    }

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

    printf("\n Deleting all values [%s] from list\n", host);

    del = searchPeerInRfcList(host, &prev);
    while (del != NULL)
    {
    	found = true;

        if(prev != NULL)
            prev->next = del->next;

        if(del == rfcTail)
        {
            rfcTail = prev;
        }
        else if(del == rfcHead)
        {
            rfcHead = del->next;
        }
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

typedef struct playerInfo {
    char host[LEN];
    int playerSocket;
    struct sockaddr_in playerSin;
    struct sockaddr_in playerIncoming;
} playerInfo;

int isNumeric(char* String) {
    char* ptr = String;
    while(*ptr && isdigit(*ptr))
        ptr++;
    return(*ptr ? 0 : 1);
}

/** Returns 1 on success, or 0 if there was an error */
int setSocketBlockingEnabled(int fd, int blocking)
{
   if (fd < 0) return 0;

   int flags = fcntl(fd, F_GETFL, 0);
   if (flags < 0) return 0;
   flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
   return (fcntl(fd, F_SETFL, flags) == 0) ? 1 : 0;
}

void closeAllSockets(int s, playerInfo playerData[], int numPlayers) {
    char str[LEN];
	int len;
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
}

main (int argc, void *argv[])
{
    char buf[LEN];
    char host[LEN];
    char str[LEN];
    char potato[BUF_SIZE];
    int s, p, fp, rc, len, port, numPlayers, numHops, a, maxfd, flags, result;
    struct hostent *hp, *ihp;
    struct sockaddr_in sin, incoming;
    fd_set readset, tempset;
    struct timeval tv;
    int on=1;

    memset(&sin, 0, sizeof(sin));
    memset(&incoming, 0, sizeof(incoming));
    memset(&potato, 0, sizeof(potato));
    
    port = WELL_KNOWN_PORT;
    
    /* fill in hostent struct for self */
    gethostname(host, sizeof host);
    hp = gethostbyname(host);
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
    s = socket(AF_INET, SOCK_STREAM, 0);
    if ( s < 0 ) {
        perror("socket:");
        exit(s);
    }
    
    // The setsockopt() function is used so the local address
    // can be reused when the server is restarted before the required
    // wait time expires
	if((rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))) < 0)
	{
		perror("setsockopt() error");
		close(s);
		exit (-1);
	}
	
    /* set up the address and port */
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    memcpy(&sin.sin_addr, hp->h_addr_list[0], hp->h_length);
    
    /* bind socket s to address sin */
    rc = bind(s, (struct sockaddr *)&sin, sizeof(sin));
    if ( rc < 0 ) {
        perror("bind:");
        exit(rc);
    }
    
    rc = listen(s, 5);
    if ( rc < 0 ) {
        perror("listen:");
        exit(rc);
    }
    
    /* accept connections */
    int i, j;
    for (i=1; i < numPlayers+1; i++) {
    // Loop through and listen for all players
        //printf("Accept %d on socket %d\n", i, s);
        memset(&playerData[i-1].playerSin, 0, sizeof(playerData[i-1].playerSin));
        memset(&playerData[i-1].playerIncoming, 0, sizeof(playerData[i-1].playerIncoming));
        memset(&playerData[i-1].host, 0, sizeof(playerData[i-1].host));
        len = sizeof(playerData[i-1].playerIncoming);
        playerData[i-1].playerSocket = accept(s, (struct sockaddr *)&playerData[i-1].playerIncoming, &len);
        if ( playerData[i-1].playerSocket < 0 ) {
            perror("accept:");
            exit(rc);
        }
        ihp = gethostbyaddr((char *)&playerData[i-1].playerIncoming.sin_addr, 
                            sizeof(struct in_addr), AF_INET);
        printf("player %d is on %s\n", i-1, ihp->h_name);
        memcpy(&playerData[i-1].host, ihp->h_name, LEN);
        
        
        // Send player their number
        memset(&str, 0, sizeof(str));
        sprintf(str, "%d", i);
        len = send(playerData[i-1].playerSocket, str, strlen(str), MSG_NOSIGNAL);
        if ( len != strlen(str) ) {
            perror("send");
            exit(1);
        }
        // Wait for ack
        len = recv(playerData[i-1].playerSocket, buf, 32, 0);
        if ( len < 0 ) {
            perror("recv");
            exit(1);
        }
        
        // Send player total number of players
        memset(&str, 0, sizeof(str));
        sprintf(str, "%d", numPlayers);
        len = send(playerData[i-1].playerSocket, str, strlen(str), MSG_NOSIGNAL);
        if ( len != strlen(str) ) {
            perror("send");
            exit(1);
        }
        // Wait for ack
        len = recv(playerData[i-1].playerSocket, buf, 32, 0);
        if ( len < 0 ) {
            perror("recv");
            exit(1);
        }
        
        // Send player number of hops
        memset(&str, 0, sizeof(str));
        sprintf(str, "%d", numHops);
        len = send(playerData[i-1].playerSocket, str, strlen(str), MSG_NOSIGNAL);
        if ( len != strlen(str) ) {
            perror("send");
            exit(1);
        }
        // Wait for ack
        len = recv(playerData[i-1].playerSocket, buf, 32, 0);
        if ( len < 0 ) {
            perror("recv");
            exit(1);
        }
    }
    
  	//printf("Debug: All players\n");
    // Loop through all players and tell them to set up the communications between each other
    // Send host name of player on their right
    for (i=0; i<numPlayers; i++) {
        memset(&str, 0, sizeof(str));
        //printf("Size of host %s = %d, %c\n", playerData[i].host, strlen(playerData[i].host), (unsigned)strlen(playerData[i].host));
        if (i == numPlayers-1) {
            // Send first player's host if we are the last player
            sprintf(str, "%s", playerData[0].host);
        }
        else {
            sprintf(str, "%s", playerData[i+1].host);
        }
        //printf("Host to send: [%s]\n", str);
        len = send(playerData[i].playerSocket, str, strlen(str), MSG_NOSIGNAL);
        if ( len != strlen(str) ) {
            perror("send");
            exit(1);
        }
        // Wait for ack
        len = recv(playerData[i].playerSocket, buf, 32, 0);
        if ( len < 0 ) {
            perror("recv");
            exit(1);
        }
        //printf("Debug: Received Ack from hostname\n");
    }
    
    sleep(1);
    if (numHops == 0) {
        printf("All players present.  Not sending potato\n");
        closeAllSockets(s, playerData, numPlayers);
        exit(0);
    }

    int luckyPlayer = rand()%numPlayers;
    printf("All players present, sending potato to player %d\n", luckyPlayer);
    // Send player the potato
    memset(&str, 0, sizeof(str));
	str[0] = 1;
    len = send(playerData[luckyPlayer].playerSocket, str, strlen(str), MSG_NOSIGNAL);
    if ( len != strlen(str) ) {
        perror("send");
        exit(1);
    }
    
    // Set up our sockets to use select
    FD_ZERO(&readset);
    FD_ZERO(&tempset);
    maxfd = 0;
    for (i=0; i<numPlayers; i++) {
        FD_SET(playerData[i].playerSocket, &readset);
        // Keep track of the largest file descriptor
        maxfd = (playerData[i].playerSocket > maxfd) ? playerData[i].playerSocket : maxfd;
        //printf("Adding socket %d to readset\n", playerData[i].playerSocket);
    }
    
    do {
        memcpy(&tempset, &readset, sizeof(tempset));
        tv.tv_sec = 30;
        tv.tv_usec = 0;
        result = select(maxfd + 1, &tempset, NULL, NULL, &tv);
        //printf(".");

        if (result == 0) { // select timed out
        } 
        else if (result < 0 && errno != EINTR) {
            perror("select");
            exit(1);
        }
        else if (result > 0) {
            for (i=0; i<numPlayers; i++) {
                if (FD_ISSET(playerData[i].playerSocket, &tempset)) {
                    // receive potato and end game
                    //printf("Debug: Received potato from player %d\n", i);

                	memset(&potato, '\0', BUF_SIZE);
                	setSocketBlockingEnabled(playerData[i].playerSocket, 0);
                	int totalLen = 0;
                
                	while (1)
                	{
                		int err;
                		memset(&buf, '\0', LEN);
                 		len = recv(playerData[i].playerSocket, buf, LEN-1, 0);
                		err = errno; // save off errno
                		//printf("Debug: recv = %d", len);
                		if ( len < 0 ) {
                			if (( err == EAGAIN ) || (err == EWOULDBLOCK)) { // No more data
                				//printf("Debug: EWOULDBLOCK\n");
                				printf("Trace of potato:\n");
                				for (j=0; j<totalLen; j++) {
                					memset(&buf, '\0', LEN);
                					sprintf(buf, "%d", potato[j]);
                					//printf("%s", buf);
                					if (j == totalLen-1) printf("%d\n", atoi(buf)-1); // no comma after last one
                					else printf("%d,", atoi(buf)-1);
                				}
                				setSocketBlockingEnabled(playerData[i].playerSocket, 1);
                    			closeAllSockets(s, playerData, numPlayers);
                    			exit(0);
                			}
                     		perror("recv");
                    		exit(1);
                		}
                		else if (len == 0) {
                			// We got a close
                			//printf("Debug: Close from left\n");
                			closeAllSockets(s, playerData, numPlayers);
                			exit(1);
                			break;
                		}
                		else {
                			memcpy(&potato[totalLen], &buf, len);
                			totalLen += len;
                			//printf(" len[%d]\n", totalLen);
                		}
                    } // while
                }
            }
            
        }
        
    } while(1);

        
    close(s);
    exit(0);
}
