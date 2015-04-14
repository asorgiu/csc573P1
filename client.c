/******************************************************************************
 *
 *  File Name........: client.c
 *
 *  Description......:
 *	When a peer wishes to join the system, it first instantiates an upload server 
 *  process listening to any available local port. It then creates a connection to
 *  the server at the well-known port 7734 and passes information about itself and
 *  its RFCs to the server, as we describe shortly. It keeps this connection open
 *  until it leaves the system. The peer may send requests to the server over this
 *  open connection and receive responses (e.g., the hostname and upload port of a
 *  server containing a particular RFC). When it wishes to download an RFC, it opens
 *  a connection to a remote peer at the specified upload port, requests the RFC,
 *  receives the file and stores it locally, and the closes this download connection
 *  to the peer.
 *
 *
 *****************************************************************************/

/*........................ Include Files ....................................*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>

#define LEN	200
#define BUF_SIZE 200000
#define SERVER_PORT 7734
#define PEER_PORT 7735
#define DEBUG printf
//#define DEBUG //

/** Returns 1 on success, or 0 if there was an error */
int setSocketBlockingEnabled(int fd, int blocking)
{
   if (fd < 0) return 0;

   int flags = fcntl(fd, F_GETFL, 0);
   if (flags < 0) return 0;
   flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
   return (fcntl(fd, F_SETFL, flags) == 0) ? 1 : 0;
}

int process_buffer(unsigned char *potato, size_t *len)
{        
    unsigned payload_len = potato[0];
        
    if (*len < 1 + payload_len) {
        /* Too short - haven't recieved whole payload yet */
        return 0;
    }
    /* Now shuffle the remaining data in the buffer back to the start */
    *len -= 1 + payload_len;
    if (*len > 0)
        memmove(potato, potato + 1 + payload_len, *len);
    return 1;
}

char *replacePort(char *command, char *orig, char *rep)
{
	static char buffer[4096];
	char *p;

	if(!(p = strstr(command, orig))) {
		return command;
	}

	strncpy(buffer, command, p - command); // Copy characters from 'str' start to 'orig'
	buffer[p - command] = '\0';

	sprintf(buffer + (p - command), "%s%s", rep, p + strlen(orig));

	return buffer;
}

// Here we are just going to throw some commands at the server
// to exercise it's functionality.
// Commands accepted by the Server are in the format:
//
// method <sp> RFC number <sp> version <cr> <lf>
// header field name <sp> value <cr> <lf>
// header field name <sp> value <cr> <lf>
// <cr> <lf>
//
// There are three methods:
// • ADD, to add a locally available RFC to the server’s index,
// • LOOKUP, to find peers that have the specified RFC, and
// • LIST, to request the whole index of RFCs from the server.
// Also, three header fields are defined:
// • Host: the hostname of the host sending the request,
// • Port: the port to which the upload server of the host is attached, and
// • Title: the title of the RFC
//
void callServerCommands(int serverSocket, int port)
{
	DEBUG("callServerCommands()\n");
	int len;
	char portStr[5];
	sprintf(portStr, "%d", port);
	
	//
	// Send ADD command
	//
	char addCommand[] = "ADD RFC 123 P2P-CI/1.0\n\rHost: engr-vcl-l-005.eos.ncsu.edu\n\rPort: xxxx\n\rTitle: A test rfc\n\r\n\r";
	char *fullCommand;
	
	fullCommand = replacePort(&addCommand, "xxxx", &portStr);
	
	len = send(serverSocket, fullCommand, strlen(fullCommand), 0);
	if (len != strlen(fullCommand)) {
    	perror("send");
    	exit(1);
    }
    DEBUG("Sent command: %s\n", fullCommand);
    
    //
    // Send LOOKUP command
    //
    char lookupCommand[] = "LOOKUP RFC 123 P2P-CI/1.0\n\rHost: engr-vcl-l-005.eos.ncsu.edu\n\rPort: xxxx\n\rTitle: A test rfc\n\r\n\r";
    fullCommand = replacePort(&lookupCommand, "xxxx", &portStr);
    len = send(serverSocket, fullCommand, strlen(fullCommand), 0);
	if (len != strlen(fullCommand)) {
    	perror("send");
    	exit(1);
    }
    DEBUG("Sent command: %s\n", fullCommand);

    //
    // Send LIST command
    //
    char listCommand[] = "LIST ALL P2P-CI/1.0\n\rHost: engr-vcl-l-005.eos.ncsu.edu\n\rPort: xxxx\n\r\n\r";
    fullCommand = replacePort(&listCommand, "xxxx", &portStr);
    len = send(serverSocket, fullCommand, strlen(fullCommand), 0);
	if (len != strlen(fullCommand)) {
    	perror("send");
    	exit(1);
    }
    DEBUG("Sent command: %s\n", fullCommand);
}

void callPeerCommands(int serverSocket)
{
	DEBUG("callPeerCommands()\n");
	while (1) {
		sleep(10);
	}
}


void handlePeerDownload(int peerSocket)
{
	DEBUG("handlePeerDownload()\n");
}

main (int argc, void *argv[])
{
    int serverSocket, rc, len, serverPort, peerPort, incomingSocket, maxfd, result, i;
    char host[LEN], str[LEN], buf[LEN], hostRight[LEN];
    char potato[BUF_SIZE];
    size_t recv_len = 0;
    struct hostent *pHostentServer, *pHostentIncoming;
    struct sockaddr_in sinServer, sinIncoming;
    fd_set readset, tempset;
    struct timeval tv;
    int on=1;

	srand(time(NULL));
    
    memset(&sinServer, 0, sizeof(sinServer));
    memset(&sinIncoming, 0, sizeof(sinIncoming));
    memset(&host, 0, sizeof(host));
    
    /* read host and port number from command line */
    if ( argc != 2 ) {
        fprintf(stderr, "Usage: %s <server-machine-name>\n", argv[0]);
        exit(1);
    }
    
    	DEBUG("Debug: Create server socket\n");
    	incomingSocket = socket(AF_INET, SOCK_STREAM, 0);
    	if ( incomingSocket < 0 ) {
        	perror("socket:");
        	exit(incomingSocket);
    	}
    
   		if((rc = setsockopt(incomingSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))) < 0)
		{
			perror("setsockopt() error");
			close(incomingSocket);
			exit (-1);
		}
    
    	/* fill in hostent struct for self */
    	gethostname(host, sizeof(host));
    	pHostentIncoming = gethostbyname(host);
    	DEBUG("Hostname: %s\n", host);
    	if ( pHostentIncoming == NULL ) {
        	fprintf(stderr, "%s: host not found (%s)\n", argv[0], host);
        	exit(1);
    	}

    
    	/* set up the address and port */
    	peerPort = PEER_PORT;
    	sinIncoming.sin_family = AF_INET;
    	sinIncoming.sin_port = htons(peerPort);
    	memcpy(&sinIncoming.sin_addr, pHostentIncoming->h_addr_list[0], pHostentIncoming->h_length);
    
    	/* bind socket incomingSocket to address sinIncoming */
    	// If port is in use, try the next one
    	do {
    		rc = bind(incomingSocket, (struct sockaddr *)&sinIncoming, sizeof(sinIncoming));
    		if (rc < 0) {
    			perror("bind:");
    			peerPort++;
    			sinIncoming.sin_port = htons(peerPort);
    		}
    	} while ( (rc < 0) && (peerPort < 8500) );
    	
    	if ( rc < 0 ) {
    		perror("bind:");
    		printf("Fatal Error! Unable to bind to any port below 8500\n");
    		exit(rc);
    	}
    
    /* 
     *  Fork to create:
     *    Child process - Server socket to accept incoming peer download requests
     *    Parent process - Connect to server and communicate with it
     */
    
    pid_t child_pid = fork(); 
    if (child_pid == 0) {  // child - create a server socket for peer downloads
    	int newPeerSocket;   	
    	
    	rc = listen(incomingSocket, 5);
    	if ( rc < 0 ) {
        	perror("listen:");
        	exit(rc);
    	}
        
		while (1)
		{
			newPeerSocket = accept(incomingSocket, NULL, NULL);
			if (newPeerSocket < 0) {
				perror("accept");
				exit(1);
			}
			
			pid_t pid = fork();
			if (pid == 0) { // child - do the peer download processing
				DEBUG("Someone connected for download!\n");
				close(incomingSocket);
				handlePeerDownload(newPeerSocket);
				exit(0);
			}
			else if (pid > 0) { // parent - go back to handling incoming peer connections
				close(newPeerSocket);
			}
			else { // error forking
        		printf("ERROR:  Could not fork a new process!\n");
    		}
    	}

    	DEBUG("Child should not be exiting!\n");
    }
    else if (child_pid > 0) { // parent - connecting to server socket
    	/* fill in hostent struct */
    	close(incomingSocket);
    	pHostentServer = gethostbyname(argv[1]); 
    	if ( pHostentServer == NULL ) {
        	fprintf(stderr, "%s: host not found (%s)\n", argv[0], argv[1]);
        	exit(1);
    	}
    
    	/* create and connect to a socket */
    
    	/* use address family INET and STREAMing sockets (TCP) */
    	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    	if ( serverSocket < 0 ) {
        	perror("socket:");
        	exit(serverSocket);
    	}
    
    	// The setsockopt() function is used so the local address
    	// can be reused when the server is restarted before the required
    	// wait time expires
		if((rc = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))) < 0)
		{
			perror("setsockopt() error");
			close(serverSocket);
			exit (-1);
		}

    	// set up the address and port
    	serverPort = SERVER_PORT;
    	sinServer.sin_family = AF_INET;
    	sinServer.sin_port = htons(serverPort);
    	memcpy(&sinServer.sin_addr, pHostentServer->h_addr_list[0], pHostentServer->h_length);
    
    	// connect to socket at above addr and port
    	rc = connect(serverSocket, (struct sockaddr *)&sinServer, sizeof(sinServer));
    	if ( rc < 0 ) {
        	perror("connect:");
        	exit(rc);
    	}
    
    	// Send the server our hostname and listening port so it can let
    	// other peers know how to connect to us
    	len = send(serverSocket, host, strlen(host), 0);
    	if (len != strlen(host)) {
    		perror("send");
    		exit(1);
    	}
    	DEBUG("Sent host: %s\n", host);
    	
    	// Wait for Ack
    	len = recv(serverSocket, buf, sizeof(buf)-1, 0);
    	if (len < 0) {
    		perror("recv:");
    	}
    	DEBUG("  Ack\n");
    	
    	// Send port number
    	int32_t conv = htonl(peerPort);
    	len = send(serverSocket, &conv, sizeof(conv), 0);
    	if (len != sizeof(conv)) {
    		perror("send");
    		exit(1);
    	}
    	DEBUG("Sent port: %d\n", peerPort);
    	
    	// Wait for Ack
    	len = recv(serverSocket, buf, sizeof(buf)-1, 0);
    	if (len < 0) {
    		perror("recv:");
    	}
    	DEBUG("   Ack\n");
    	
    	callServerCommands(serverSocket, peerPort); // Contact server to give/get info
    	callPeerCommands(serverSocket);   // Contact peers to download RFCs
    	
    	DEBUG("Parent should not be exiting!\n");
    	

    }
    else { // error forking
        printf("ERROR:  Could not fork a new process!\n");
    }

/*
    // Get host name of player to our right from host
    // This also signals us to start creating the ring of connections between players
    memset(&hostRight, 0, LEN);
    recv_len = 0;
    int complete = 0;
    len = recv(s, hostRight, LEN, 0);
    if ( len < 0 ) {
        perror("recv");
        exit(1);
    }
    hostRight[len] = '\0';
    //printf("Got test: %s\n", hostRight);
    len = send(s, str, strlen(str), 0);
    if ( len != strlen(str) ) {
        perror("send");
        exit(1);
    }
    hpRight = gethostbyname(hostRight); 
    if ( hpRight == NULL ) {
        fprintf(stderr, "%s: host not found (%s)\n", argv[0], hostRight);
        exit(1);
    }

    
    //
    // Then accept connection from player on left
    //
    // If we are the last player, we are the first to connect right THEN accept
    if (iPlayer != numPlayers) {
    	//printf("Debug: Accept player to Left <--\n");
        memset(&sinLeft, 0, sizeof(sinLeft));
        len = sizeof(sinLeft);
        sLeft = accept(sLeftIncoming, (struct sockaddr *)&sinLeft, &len);
        if ( sLeft < 0 ) {
            perror("accept:");
            exit(rc);
        }
        
        if (iPlayer == 1) {
        	sleep(1);
        }
        // Then connect to player on the right after the player on the left has connected
    	sRight = socket(AF_INET, SOCK_STREAM, 0);
    	if ( sRight < 0 ) {
      		perror("socket:");
        	exit(sLeft);
    	}
    
    	// set up the address and port
    	memset(&sinRight, 0, sizeof(sinRight)); 
    	sinRight.sin_family = AF_INET;
    	if (iPlayer == numPlayers) {
        	sinRight.sin_port = htons(port+1);
    	} else {
        	sinRight.sin_port = htons(port+iPlayer+1);
    	}
    	memcpy(&sinRight.sin_addr, hpRight->h_addr_list[0], hpRight->h_length);
    
    	// connect to socket at above addr and port
    	rc = connect(sRight, (struct sockaddr *)&sinRight, sizeof(sinRight));
    	if ( rc < 0 ) {
        	perror("connect:");
        	exit(rc);
    	}
    	//printf("Debug: Connected!\n");
    }
    else {
        // Last player must connect to first player (on right) first, then wait for accept
    	sRight = socket(AF_INET, SOCK_STREAM, 0);
    	if ( sRight < 0 ) {
      		perror("socket:");
        	exit(sLeft);
    	}
    
    	// set up the address and port
    	memset(&sinRight, 0, sizeof(sinRight)); 
    	sinRight.sin_family = AF_INET;
        sinRight.sin_port = htons(port+1);
    	memcpy(&sinRight.sin_addr, hpRight->h_addr_list[0], hpRight->h_length);
    
    	// connect to socket at above addr and port
    	rc = connect(sRight, (struct sockaddr *)&sinRight, sizeof(sinRight));
    	if ( rc < 0 ) {
        	perror("connect:");
        	exit(rc);
    	}
    	
    	// Now accept
    	//printf("Debug: Accept player to Left <--\n");
        memset(&sinLeft, 0, sizeof(sinLeft));
        len = sizeof(sinLeftIncoming);
        sLeft = accept(sLeftIncoming, (struct sockaddr *)&sinLeft, &len);
        if ( sLeft < 0 ) {
            perror("accept:");
            exit(rc);
        }
        //printf("Debug: Connected!\n");
    }
    
    
    // Now we wait for the potato and listen on the left, right, and master sockets

    
    // Set up our sockets to use select
    FD_ZERO(&readset);
    FD_ZERO(&tempset);
    maxfd = 0;
    FD_SET(sLeft, &readset);
    maxfd = (sLeft > maxfd) ? sLeft : maxfd;
    FD_SET(sRight, &readset);
    maxfd = (sRight > maxfd) ? sRight : maxfd;
    FD_SET(s, &readset);
    maxfd = (s > maxfd) ? s : maxfd;
    //printf("Debug: Adding sockets to readset\n");
    
    do {
        memcpy(&tempset, &readset, sizeof(tempset));
        tv.tv_sec = 3;
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
            if (FD_ISSET(s, &tempset)) {
                // received potato or end game message from Master
                //printf("Debug: received a message from Master\n");
                memset(&buf, '\0', LEN);
                memset(&potato, '\0', BUF_SIZE);
                len = recv(s, buf, LEN, 0);
                if ( len < 0 ) {
                    perror("recv");
                    exit(1);
                }
                else if (len == 0) {
                	// We got a close
                	//printf("Debug: Close from Master\n");
                	FD_CLR(s, &readset);
                	close(s);
                	exit(0);
                }
                if (buf[0] == 'S')
                {
                	// Stop command
                	//printf("Debug: Got STOP command!\n");
                	close(sRight);
                	close(s);
                	exit(0);
                }
                else {
                	//memcpy(&potato, &buf, len);
                	potato[0] = iPlayer;
                	potato[1] = '\0';
                	//printf("Starting Potato! len=%d\n", len);
                	// Check to see if the game should stop
                	if (numHops == 1)
                	{
                		//printf("Debug: FINISHED... hops=%d len=%d\n", numHops, len);
                		printf("I'm it\n");
                		FD_CLR(sLeft, &readset);
                		close(sLeft);
						sendPotato(&potato, s);
                	}
                	// Send potato to neighbor randomly
                	else if (rand()%2 == 1)
                	{
                		printf("Sending potato to %d\n", getLeftPlayer(iPlayer, numPlayers));
                		sendPotato(&potato, sLeft);
                	}
                	else
                	{
               			printf("Sending potato to %d\n", getRightPlayer(iPlayer, numPlayers));
                		sendPotato(&potato, sRight);
                	}
                }
            }
            else if (FD_ISSET(sLeft, &tempset))
            {
            	// received potato from player on left
                //printf("Debug: received potato from left\n");
                memset(&potato, '\0', BUF_SIZE);
                setSocketBlockingEnabled(sLeft, 0);
                int totalLen = 0;
                
                while (1)
                {
                	int err;
                	memset(&buf, '\0', LEN);
                	len = recv(sLeft, buf, LEN-1, 0);
                	err = errno; // save off errno
                	//printf("Debug: recv = %d", len);
                	if ( len < 0 ) {
                		if (( err == EAGAIN ) || (err == EWOULDBLOCK)) { // No more data
                			//printf("Debug: EWOULDBLOCK\n");
                			potato[totalLen] = iPlayer;
                			potato[totalLen+1] = '\0';
                			//printf("Trace of potato: ");
                			//for (i=0; i<totalLen; i++) {
                			//	memset(&buf, '\0', LEN);
                			//	sprintf(buf, "%d", potato[i]);
                			//	printf("%s", buf);
                			//}
                			//printf(" len[%d]\n", totalLen);
                			// Check to see if the game should stop
                			if (totalLen > numHops-2)
                			{
                				//printf("Debug: FINISHED... hops=%d len=%d\n", numHops, len);
                				printf("I'm it\n");
                				FD_CLR(sLeft, &readset);
                				close(sLeft);
								sendPotato(&potato, s);
                			}
                			// Send potato to neighbor randomly
                			else if (rand()%2 == 1)
                			{
                				printf("Sending potato to %d\n", getLeftPlayer(iPlayer, numPlayers));
                				sendPotato(&potato, sLeft);
                			}
                			else
                			{
                				printf("Sending potato to %d\n", getRightPlayer(iPlayer, numPlayers));
                				sendPotato(&potato, sRight);
                			}
                			break;//
                		}//
                    	perror("recv");
                    	exit(1);
                	}
                	else if (len == 0) {
                		// We got a close
                		//printf("Debug: Close from left\n");
                		FD_CLR(sLeft, &readset);
                		close(sLeft);
                		break;
                	}
                	else {
                		memcpy(&potato[totalLen], &buf, len);
                		totalLen += len;
                		//printf(" len[%d]\n", totalLen);
                	}
                } // while
            }
            else if (FD_ISSET(sRight, &tempset))
            {
            	// received potato from player on right
                //printf("Debug: received potato from right\n");
                memset(&potato, '\0', BUF_SIZE);
                setSocketBlockingEnabled(sRight, 0);
                int totalLen = 0;
                
                while (1)
                {
                	int err;
               		memset(&buf, '\0', LEN);
                	len = recv(sRight, buf, LEN-1, 0);
                	err = errno; // save off errno
                	//printf("Debug: recv = %d", len);
                	if ( len < 0 ) {
                		if (( err == EAGAIN ) || (err == EWOULDBLOCK)) { // No more data
                			//printf("Debug: EWOULDBLOCK\n");
                			potato[totalLen] = iPlayer;
                			potato[totalLen+1] = '\0';
                			//printf("Trace of potato: ");
                			//for (i=0; i<totalLen; i++) {
                			//	memset(&buf, '\0', LEN);
                			//	sprintf(buf, "%d", potato[i]);
                			//	printf("%s", buf);
                			//}
                			//printf(" len[%d]\n", totalLen);
                			// Check to see if the game should stop
                			if (totalLen > numHops-2)
                			{
                				//printf("Debug: FINISHED... hops=%d len=%d\n", numHops, len);
                				printf("I'm it\n");
                				FD_CLR(sLeft, &readset);
                				close(sLeft);
								sendPotato(&potato, s);
                			}
                			// Send potato to neighbor randomly
                			else if (rand()%2 == 1)
                			{
                				printf("Sending potato to %d\n", getLeftPlayer(iPlayer, numPlayers));
                				sendPotato(&potato, sLeft);
                			}
                			else
                			{
                				printf("Sending potato to %d\n", getRightPlayer(iPlayer, numPlayers));
                				sendPotato(&potato, sRight);
                			}
                			break;
                		}
                    	perror("recv");
                    	exit(1);
                	}
                	else if (len == 0) {
                		// We got a close
                		//printf("Debug: Close from right\n");
                		FD_CLR(sRight, &readset);
                		close(sRight);
                		break;
                	}
                	else {
                		memcpy(&potato[totalLen], &buf, len);
                		totalLen += len;
                		//printf(" len[%d]\n", totalLen);
                	}
                } // while
            }
            else {
            	//printf("Debug: ERROR - received from fd %d", result);
            }
        }
        setSocketBlockingEnabled(sLeft, 1);//
    } while(1);
*/
}
