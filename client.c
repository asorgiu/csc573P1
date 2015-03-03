/******************************************************************************
 *
 *  File Name........: client.c
 *
 *  Description......:
 *	Create a process that talks to the server.c program.  After 
 *  connecting, each line of input is sent to listen.
 *
 *  This program takes two arguments.  The first is the host name on which
 *  the listen process is running. (Note: listen must be started first.)
 *  The second is the port number on which listen is accepting connections.
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

#define LEN	100
#define BUF_SIZE 200000

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

// Compensating for storing player numbers 1, 2, 3... while reporting them as 0, 1, 2...
int getLeftPlayer(int thisPlayer, int numPlayers)
{
	if (thisPlayer == 1)
		return numPlayers-1;
	else
		return thisPlayer-2;
}

int getRightPlayer(int thisPlayer, int numPlayers)
{
	if (thisPlayer == numPlayers)
		return 0;
	else
		return thisPlayer;
}


void sendPotato(char* outgoing, int s)
{
	int len=0;
	
	len = send(s, outgoing, strlen(outgoing), 0);
   	if ( len != strlen(outgoing) ) {
       	perror("send");
       	exit(1);
   	}
}

main (int argc, void *argv[])
{
    int s, rc, len, port, iPlayer, numHops, numPlayers, sLeft, sRight, sLeftIncoming, maxfd, result, i;
    char host[LEN], str[LEN], buf[LEN], hostRight[LEN];
    char potato[BUF_SIZE];
    size_t recv_len = 0;
    struct hostent *hp, *hpRight, *hpLeft;
    struct sockaddr_in sin, sinRight, sinLeft, sinLeftIncoming;
    fd_set readset, tempset;
    struct timeval tv;
    int on=1;

	srand(time(NULL));
    
    memset(&sin, 0, sizeof(sin));
    memset(&potato, 0, sizeof(potato));
    
    /* read host and port number from command line */
    if ( argc != 3 ) {
        fprintf(stderr, "Usage: %s <master-machine-name> <port-number>\n", argv[0]);
        exit(1);
    }
    
    /* fill in hostent struct */
    hp = gethostbyname(argv[1]); 
    if ( hp == NULL ) {
        fprintf(stderr, "%s: host not found (%s)\n", argv[0], argv[1]);
        exit(1);
    }
    port = atoi(argv[2]);
    
    /* create and connect to a socket */
    
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
    
    /* connect to socket at above addr and port */
    rc = connect(s, (struct sockaddr *)&sin, sizeof(sin));
    if ( rc < 0 ) {
        perror("connect:");
        exit(rc);
    }
    
    // Get our info from the Master
    memset(&buf, 0, LEN);
    len = recv(s, buf, 32, 0);
    if ( len < 0 ) {
        perror("recv");
        exit(1);
    }
    buf[len] = '\0';
    iPlayer = atoi(buf);
    str[0] = 'A';
    str[1] = '\0';
    len = send(s, str, strlen(str), 0);
    if ( len != strlen(str) ) {
        perror("send");
        exit(1);
    }
    
    memset(&buf, 0, LEN);
    len = recv(s, buf, 32, 0);
    if ( len < 0 ) {
        perror("recv");
        exit(1);
    }
    buf[len] = '\0';
    numPlayers = atoi(buf);
    len = send(s, str, strlen(str), 0);
    if ( len != strlen(str) ) {
        perror("send");
        exit(1);
    }

    memset(&buf, 0, LEN);
    len = recv(s, buf, 32, 0);
    if ( len < 0 ) {
        perror("recv");
        exit(1);
    }
    buf[len] = '\0';
    numHops = atoi(buf);
    len = send(s, str, strlen(str), 0);
    if ( len != strlen(str) ) {
        perror("send");
        exit(1);
    }

    // Create server socket for player on the left to connect to
    /* use address family INET and STREAMing sockets (TCP) */
    //printf("Debug: Create server socket\n");
    sLeftIncoming = socket(AF_INET, SOCK_STREAM, 0);
    if ( sLeftIncoming < 0 ) {
        perror("socket:");
        exit(sLeftIncoming);
    }
    
   	if((rc = setsockopt(sLeftIncoming, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))) < 0)
	{
		perror("setsockopt() error");
		close(s);
		exit (-1);
	}

    
    /* fill in hostent struct for self */
    gethostname(host, sizeof host);
    hpLeft = gethostbyname(host);
    if ( hpLeft == NULL ) {
        fprintf(stderr, "%s: host not found (%s)\n", argv[0], host);
        exit(1);
    }

    
    /* set up the address and port */
    memset(&sinLeftIncoming, 0, sizeof(sinLeftIncoming)); 
    sinLeftIncoming.sin_family = AF_INET;
    sinLeftIncoming.sin_port = htons(port+iPlayer);
    memcpy(&sinLeftIncoming.sin_addr, hpLeft->h_addr_list[0], hpLeft->h_length);
    
    /* bind socket s to address sin */
    rc = bind(sLeftIncoming, (struct sockaddr *)&sinLeftIncoming, sizeof(sinLeftIncoming));
    if ( rc < 0 ) {
        perror("bind:");
        exit(rc);
    }
    
    rc = listen(sLeftIncoming, 5);
    if ( rc < 0 ) {
        perror("listen:");
        exit(rc);
    }
        
    printf("Connected as player %d\n", iPlayer-1);
    
    
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
    
    	/* set up the address and port */
    	memset(&sinRight, 0, sizeof(sinRight)); 
    	sinRight.sin_family = AF_INET;
    	if (iPlayer == numPlayers) {
        	sinRight.sin_port = htons(port+1);
    	} else {
        	sinRight.sin_port = htons(port+iPlayer+1);
    	}
    	memcpy(&sinRight.sin_addr, hpRight->h_addr_list[0], hpRight->h_length);
    
    	/* connect to socket at above addr and port */
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
    
    	/* set up the address and port */
    	memset(&sinRight, 0, sizeof(sinRight)); 
    	sinRight.sin_family = AF_INET;
        sinRight.sin_port = htons(port+1);
    	memcpy(&sinRight.sin_addr, hpRight->h_addr_list[0], hpRight->h_length);
    
    	/* connect to socket at above addr and port */
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
}
