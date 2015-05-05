/******************************************************************************
 *
 *  File Name........: client2.c
 *  Copyright 2015, Aaron Sorgius, All rights reserved.
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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <time.h>

#define LEN	200
#define BUF_SIZE 20000
#define SERVER_PORT 7734
#define PEER_PORT 7735
#define DEBUG printf
//#define DEBUG //
//#define DEBUG2 printf
#define DEBUG2 //

char myHostname[LEN];
char serverHostname[LEN];
char peerHostForRFC[LEN];
int peerPortForRFC;

/** Returns 1 on success, or 0 if there was an error */
int setSocketBlockingEnabled(int fd, int blocking)
{
   if (fd < 0) return 0;

   int flags = fcntl(fd, F_GETFL, 0);
   if (flags < 0) return 0;
   flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
   return (fcntl(fd, F_SETFL, flags) == 0) ? 1 : 0;
}

// We know the port value will be the last token in the response
// that we received from the LOOKUP command.
// Note: this will mean that we are always going to connect to the last peer
//       listed if there are multiple peers. Should not matter.
char* getPortValueFromLookup(char *data)
{
	char *datacopy = malloc(strlen(data) + 1); // strtok modifies the string
	char *result = NULL;
	char *s;
	DEBUG2("getPortValueFromLookup()\n");
	
	strcpy(datacopy, data);
	
	s = strtok(datacopy, " \n\r");
	
	while (s)
	{
		// Set the last token we parsed to the Result. 
		// When we have gone through all tokens, result
		// should be holding the last one that we want
		if (result == NULL)
		{
			result = malloc(strlen(s) +1);
			strcpy(result, s);
		}
		else
		{
			free(result);
			result = malloc(strlen(s) + 1);
			strcpy(result, s);
		}
		s = strtok(0, " \n\r");
	}
	free(datacopy);
	
	return result;
}

// We know the host value will be the second to last token in the response
// that we received from the LOOKUP command.
// Note: this will mean that we are always going to connect to the last peer
//       listed if there are multiple peers. Should not matter.
char* getHostValueFromLookup(char *data)
{
	char *datacopy = malloc(strlen(data) + 1); // strtok modifies the string
	char *result = NULL;
	char *lastToken = NULL;
	char *s;
	DEBUG2("getHostValueFromLookup()\n");
	
	strcpy(datacopy, data);
	
	s = strtok(datacopy, " \n\r");
	
	while (s)
	{
		// First set the last token to the first token we get. Then move
		// each new token to the last, and remember the second to last
		// one as our result we want to return. When we have gone through all
		// tokens, result should be holding the second to last one that we want
		if (lastToken == NULL)
		{
			lastToken = malloc(strlen(s) +1);
			strcpy(lastToken, s);
		}
		else
		{
			if (result != NULL) {
				free(result);
			}
			result = malloc(strlen(lastToken) + 1);
			strcpy(result, lastToken);
			free(lastToken);
			lastToken = malloc(strlen(s) +1);
			strcpy(lastToken, s);
		}
		s = strtok(0, " \n\r");
	}
	free(lastToken);
	free(datacopy);
	
	return result;
}

char* getTagValue(char *data, char *tag)
{
	char *datacopy = malloc(strlen(data) + 1); // strtok modifies the string
	char *result = 0;
	char *s;
	DEBUG2("getTagValue() - %s\n", tag);
	
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
			DEBUG2("      found value - [%s]\n", s);
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
	DEBUG2("getTagVersion() - version at %d\n", versionPosition);
	
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

char *replaceMask(char *command, char *orig, char *rep)
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

// For test purposes, this function will take as the last parameter
//    int fail - If 1, this will purposefully send an invalid command
//             - If 0, it will request the rfc as designed
void getRfc(int rfc, char* host, int peerPort, int fail)
{
	// Connect to another peer and send the GET command,
	// then receive the response
	DEBUG2("getRfc()\n");
	int peerServerSocket;
    struct hostent *pHostentPeerServer;
    struct sockaddr_in sinPeerServer;
    int on=1;
    int rc;
    int len;
    char rfcString[10];
    char request[BUF_SIZE];
    char response[BUF_SIZE];
    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));

    	pHostentPeerServer = gethostbyname(host); 
    	if ( pHostentPeerServer == NULL ) {
        	fprintf(stderr, "Host not found (%s)\n", host);
        	exit(1);
    	}
    
    	/* create and connect to a socket */
    
    	/* use address family INET and STREAMing sockets (TCP) */
    	peerServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    	if ( peerServerSocket < 0 ) {
        	perror("socket:");
        	exit(peerServerSocket);
    	}
    
    	// The setsockopt() function is used so the local address
    	// can be reused when the server is restarted before the required
    	// wait time expires
		if((rc = setsockopt(peerServerSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on))) < 0)
		{
			perror("setsockopt() error");
			close(peerServerSocket);
			exit (-1);
		}

    	// set up the address and port
    	sinPeerServer.sin_family = AF_INET;
    	sinPeerServer.sin_port = htons(peerPort);
    	memcpy(&sinPeerServer.sin_addr, pHostentPeerServer->h_addr_list[0], pHostentPeerServer->h_length);
    
    	// connect to socket at above addr and port
    	rc = connect(peerServerSocket, (struct sockaddr *)&sinPeerServer, sizeof(sinPeerServer));
    	if ( rc < 0 ) {
        	perror("connect:");
        	exit(rc);
    	}
    	
    	// And get the OS info
		struct utsname osbuf;
		uname(&osbuf);
    
    	// set up the request
    	sprintf(rfcString, "%d", rfc);
    	if (fail) { strcpy(request, "BLAH RFC "); }
    	else {      strcpy(request, "GET RFC "); }
    	strcat(request, rfcString);
    	strcat(request, " P2P-CI/1.0\r\nHost: ");
    	strcat(request, myHostname);
    	strcat(request, "\r\nOS: ");
		strcat(request, osbuf.sysname);
		strcat(request, " ");
		strcat(request, osbuf.release);
		strcat(request, "\r\n\r\n");
    	
    	// Send the server the GET request
    	len = send(peerServerSocket, request, strlen(request), 0);
    	if (len != strlen(request)) {
    		perror("send");
    		exit(1);
    	}
    	DEBUG("   Peer Client Sent:\n%s\n", request);
    	
    	// Wait for response
    	len = recv(peerServerSocket, response, sizeof(response)-1, 0);
    	if (len < 0) {
    		perror("recv:");
    	}
    	DEBUG("  Peer Client Received:\n%s\n", response);
    	
    	sleep(1);
    	close(peerServerSocket);
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
	DEBUG2("callServerCommands()\n");
	int len;
	char portStr[5]; // string of my port to include in commands to server
	char buf[BUF_SIZE];
	char *peerHostStr; // used to hold host of peer we will connect to get rfc from
	char *peerPortStr; // used to hold port of peer we will connect to get rfc from
	int peerPort;      // used to hold port of peer we will connect to get rfc from
	sprintf(portStr, "%d", port);
	memset(&buf, 0, sizeof(buf));
	
	DEBUG("\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
	DEBUG("Peer sending P2S commands to Server\n");
	
	//
	// Send ADD command
	//
	DEBUG("\n------------------------------------\n");
	DEBUG("Sending valid ADD command\n");
	char addCommand[BUF_SIZE];
	strcpy(addCommand, "ADD RFC 234 P2P-CI/1.0\n\rHost: ");
	strcat(addCommand, myHostname);
	strcat(addCommand, "\n\rPort: ");
	strcat(addCommand, portStr);
	strcat(addCommand, "\n\rTitle: A client2 test rfc\n\r\n\r");
	
	len = send(serverSocket, addCommand, strlen(addCommand), 0);
	if (len != strlen(addCommand)) {
    	perror("send");
    	exit(1);
    }
    DEBUG("Sent command to Server:\n%s\n", addCommand);
    // Wait for Ack
	recv(serverSocket, buf, sizeof(buf)-1, 0);
	DEBUG("Received from Server:\n%s\n", buf);
	DEBUG("\n------------------------------------\n");
    
	//
	// Send 2nd ADD command
	//
	DEBUG("\n------------------------------------\n");
	DEBUG("Sending valid ADD command\n");
	char addCommand2[BUF_SIZE];
	strcpy(addCommand2, "ADD RFC 456 P2P-CI/1.0\n\rHost: ");
	strcat(addCommand2, myHostname);
	strcat(addCommand2, "\n\rPort: ");
	strcat(addCommand2, portStr);
	strcat(addCommand2, "\n\rTitle: Just another rfc\n\r\n\r");
	
	len = send(serverSocket, addCommand2, strlen(addCommand2), 0);
	if (len != strlen(addCommand2)) {
    	perror("send");
    	exit(1);
    }
    DEBUG("Sent command to Server:\n%s\n", addCommand2);
    // Wait for Ack
	recv(serverSocket, buf, sizeof(buf)-1, 0);
	DEBUG("Received from Server:\n%s\n", buf);
	DEBUG("\n------------------------------------\n");
    
    //
    // Send LOOKUP command
    //
	DEBUG("\n------------------------------------\n");
	DEBUG("Sending valid LOOKUP command\n");
	memset(&buf, 0, sizeof(buf));
    char lookupCommand[BUF_SIZE];
	strcpy(lookupCommand, "LOOKUP RFC 123 P2P-CI/1.0\n\rHost: ");
	strcat(lookupCommand, myHostname);
	strcat(lookupCommand, "\n\rPort: ");
	strcat(lookupCommand, portStr);
	strcat(lookupCommand, "\n\rTitle: A test rfc\n\r\n\r");
	
	len = send(serverSocket, lookupCommand, strlen(lookupCommand), 0);
	if (len != strlen(lookupCommand)) {
    	perror("send");
    	exit(1);
    }
    DEBUG("Sent command to Server:\n%s\n", lookupCommand);
    // Wait for Ack
	recv(serverSocket, buf, sizeof(buf)-1, 0);
	DEBUG("Received from Server:\n%s\n", buf);
	DEBUG("\n------------------------------------\n");
	
	// Read host/port from response and save for call to getRfc
	peerHostStr = getHostValueFromLookup(&buf);  DEBUG2("   Host = %s\n", peerHostStr);
	peerPortStr = getPortValueFromLookup(&buf);  DEBUG2("   Port = %s\n", peerPortStr);
	strcpy(peerHostForRFC, peerHostStr);
	peerPortForRFC = atoi(peerPortStr);

	sleep(1);
	
	    
    //
    // Send LOOKUP command with BAD VERSION
    //
	DEBUG("\n------------------------------------\n");
	DEBUG("Sending valid LOOKUP command with BAD VERSION\n");
	memset(&buf, 0, sizeof(buf));
    char badVersionCommand[BUF_SIZE];
	strcpy(badVersionCommand, "LOOKUP RFC 123 P2P-CI/2.0\n\rHost: ");
	strcat(badVersionCommand, myHostname);
	strcat(badVersionCommand, "\n\rPort: ");
	strcat(badVersionCommand, portStr);
	strcat(badVersionCommand, "\n\rTitle: A test rfc\n\r\n\r");
	
	len = send(serverSocket, badVersionCommand, strlen(badVersionCommand), 0);
	if (len != strlen(badVersionCommand)) {
    	perror("send");
    	exit(1);
    }
    DEBUG("Sent command to Server:\n%s\n", badVersionCommand);
    // Wait for Ack
	recv(serverSocket, buf, sizeof(buf)-1, 0);
	DEBUG("Received from Server:\n%s\n", buf);
	DEBUG("\n------------------------------------\n");
	
	sleep(1);
	
	    
    //
    // Send LOOKUP command with RFC that does not exist
    //
	DEBUG("\n------------------------------------\n");
	DEBUG("Sending valid LOOKUP command with RFC that does not exist\n");
	memset(&buf, 0, sizeof(buf));
    char badRfcCommand[BUF_SIZE];
	strcpy(badRfcCommand, "LOOKUP RFC 999 P2P-CI/1.0\n\rHost: ");
	strcat(badRfcCommand, myHostname);
	strcat(badRfcCommand, "\n\rPort: ");
	strcat(badRfcCommand, portStr);
	strcat(badRfcCommand, "\n\rTitle: A test rfc\n\r\n\r");
	
	len = send(serverSocket, badRfcCommand, strlen(badRfcCommand), 0);
	if (len != strlen(badRfcCommand)) {
    	perror("send");
    	exit(1);
    }
    DEBUG("Sent command to Server:\n%s\n", badRfcCommand);
    // Wait for Ack
	recv(serverSocket, buf, sizeof(buf)-1, 0);
	DEBUG("Received from Server:\n%s\n", buf);
	DEBUG("\n------------------------------------\n");
	
		
    //
    // Send LIST command
    //
	DEBUG("\n------------------------------------\n");
	DEBUG("Sending valid LIST command\n");
	memset(&buf, 0, sizeof(buf));
    char listCommand[BUF_SIZE];
	strcpy(listCommand, "LIST ALL P2P-CI/1.0\n\rHost: ");
	strcat(listCommand, myHostname);
	strcat(listCommand, "\n\rPort: ");
	strcat(listCommand, portStr);
	strcat(listCommand, "\n\r\n\r");
	
	len = send(serverSocket, listCommand, strlen(listCommand), 0);
	if (len != strlen(listCommand)) {
    	perror("send");
    	exit(1);
    }
    DEBUG("Sent command to Server:\n%s\n", listCommand);
    // Wait for Ack
	recv(serverSocket, buf, sizeof(buf)-1, 0);
	DEBUG("Received from Server:\n%s\n", buf);
	DEBUG("\n------------------------------------\n");
	
	//
	// Send Invalid command
	DEBUG("\n------------------------------------\n");
	DEBUG("Sending Invalid command\n");
	memset(&buf, 0, sizeof(buf));
    char invalidCommand[BUF_SIZE];
	strcpy(invalidCommand, "BLAH ALL P2P-CI/1.0\n\rHost: ");
	strcat(invalidCommand, myHostname);
	strcat(invalidCommand, "\n\rPort: ");
	strcat(invalidCommand, portStr);
	strcat(invalidCommand, "\n\r\n\r");
	
	len = send(serverSocket, invalidCommand, strlen(invalidCommand), 0);
	if (len != strlen(invalidCommand)) {
    	perror("send");
    	exit(1);
    }
    DEBUG("Sent command to Server:\n%s\n", invalidCommand);
    // Wait for Ack
	recv(serverSocket, buf, sizeof(buf)-1, 0);
	DEBUG("Received from Server:\n%s\n", buf);
	DEBUG("\n------------------------------------\n");
	
	DEBUG("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n\n");

}

void callPeerCommands(int serverSocket)
{
	DEBUG2("callPeerCommands()\n");
	// This should change to a different client!
	DEBUG("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n");
	DEBUG("Peer Client sending P2P messages\n");
	DEBUG("\n------------------------------------\n");
	DEBUG("Peer Client sending GET request\n");
	getRfc(123, peerHostForRFC, peerPortForRFC, 0);
	DEBUG("\n------------------------------------\n");
	DEBUG("Peer Client sending GET request for non-existant RFC\n");
	getRfc(999, peerHostForRFC, peerPortForRFC, 0);
	DEBUG("\n------------------------------------\n");
	DEBUG("Peer Client sending Invalid command\n");
	getRfc(123, peerHostForRFC, peerPortForRFC, 1);
	DEBUG("\n------------------------------------\n");
	DEBUG("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n");
	
	
	while (1) {
		DEBUG(".");
		sleep(10);
	}
}

int isVersionOk(char *version) {
	DEBUG2("isVersionOk()\n");
	if (strcmp(version, "P2P-CI/1.0") == 0) {
		//they match
		return 1;
	}
	else {
		return 0;
	}
}

void send400(int peerSocket) {
	DEBUG2("send400()\n");
	char message[] = "P2P-CI/1.0 400 Bad Request\r\n\r\n";
	
	send(peerSocket, message, strlen(message), 0);
}

void send404(int peerSocket) {
	DEBUG2("send404()\n");
	char message[] = "P2P-CI/1.0 404 P2P-CI Not Found\r\n\r\n";
	
	send(peerSocket, message, strlen(message), 0);
}

void send505(int peerSocket) {
	DEBUG2("send505()\n");
	char message[] = "P2P-CI/1.0 505 P2P-CI Version Not Supported\r\n\r\n";
	
	send(peerSocket, message, strlen(message), 0);
}

void handlePeerDownload(int peerSocket)
{
	DEBUG2("handlePeerDownload()\n");
	char buf[256];
	memset(&buf, 0, sizeof(buf));
	int rfcNum;
	char *rfcNumString;
	char *host;
	char *version;
	char *os;
	char filename[100];
	char reply[BUF_SIZE];
	int fd;
	FILE *fp;
	struct stat file_stat;
	int offset, bytesRemaining;
	int len;
	char fileSize[200];
	time_t modifiedTime;
	memset(&reply, 0, sizeof(reply));

	// Get the download request
	recv(peerSocket, buf, sizeof(buf)-1, 0);
	DEBUG("===============================\n");
	DEBUG("Peer Server Received:\n%s\n", buf);
	
	// Check the command that was sent
	if (buf[0] != 'G' || buf[1] != 'E' || buf[2] != 'T') {
		// Invalid command
		send400(peerSocket);
		return;
	}
	
	rfcNumString = getTagValue(&buf, "RFC");    DEBUG2("   RFC = %s\n", rfcNumString);
	version      = getTagVersion(&buf, 4);      DEBUG2("   Version = %s\n", version);
	host         = getTagValue(&buf, "Host:");  DEBUG2("   Host = %s\n", host);
	os           = getTagValue(&buf, "OS:");    DEBUG2("   OS = %s\n", os);
	rfcNum = atoi(rfcNumString);

	// Check version
	if (!isVersionOk(version)) {
		send505(peerSocket);
		return;
	}
	
	strcpy(filename, "RFC");
	strcat(filename, rfcNumString);
	strcat(filename, ".txt");
	
	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		printf("Error opening file %s\n", filename);
		send404(peerSocket);
		return;
	}
	fp = fopen(filename, "r");
	if (!fp) {
		printf("Error opening file %s\n", filename);
		send404(peerSocket);
		return;
	}
	DEBUG2("File open\n");
	
	// Get date and time for our reply
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	char str_date[100];
	char str_mdate[100];
	sprintf(str_date, "%d-%d-%d %d:%d:%d", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	// These strftime() calls were hanging?! (was before I included time.h)
	//strftime(str_time, sizeof(str_time), "%H %M %S", tm);
	//strftime(str_date, sizeof(str_date), "%d %m %Y", tm);
	
	// And get the OS info
	struct utsname osbuf;
	uname(&osbuf);
	
	// And file info
	if (fstat(fd, &file_stat) < 0) {
		printf("Error fstat of file %s", filename);
		return;
	}

	bytesRemaining = file_stat.st_size;
	sprintf(fileSize, "%d", bytesRemaining);
	modifiedTime = file_stat.st_mtime;
	tm = *localtime(&modifiedTime);
	sprintf(str_mdate, "%d-%d-%d %d:%d:%d", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	//strftime(str_mtime, sizeof(str_mtime), "%H %M %S", tm);
	//strftime(str_mdate, sizeof(str_mdate), "%d %m %Y", tm);
	
	DEBUG2("Time ok\n");
	strcpy(reply, "P2P-CI/1.0 200 OK\r\nDate: ");
	strcat(reply, str_date);
	strcat(reply, "\r\nOS: ");
	strcat(reply, osbuf.sysname);
	strcat(reply, " ");
	strcat(reply, osbuf.release);
	strcat(reply, "\r\nLast-Modified: ");
	strcat(reply, str_mdate);
	strcat(reply, "\r\nContent-Length: ");
	strcat(reply, fileSize);
	strcat(reply, "\r\nContent-Type: text/text\r\n\r\n");
	
	// Not efficient way to do this, but taking the shortcut.
	// Should use sendfile() and do more error checking
	memset(&buf, 0, sizeof(buf));
	while(fgets(buf, 256, fp) != NULL)
	{
		DEBUG2("      read: %s", buf);
		strcat(reply, buf);
	}
	
	send(peerSocket, reply, strlen(reply), 0);
	DEBUG("Peer Server Sent:\n%s\n", reply);
	DEBUG("===============================\n");
	
	sleep(1);
	fclose(fd); // had to close the file AFTER sending for some reason or
	fclose(fp); //   it seemed to be closing our socket connection :(
	close(peerSocket);	
}

main (int argc, char *argv[])
{
    int serverSocket, rc, len, serverPort, peerPort, incomingSocket, maxfd, result, i;
    char str[LEN], buf[LEN], hostRight[LEN];
    char potato[BUF_SIZE];
    size_t recv_len = 0;
    struct hostent *pHostentServer, *pHostentIncoming;
    struct sockaddr_in sinServer, sinIncoming;
    fd_set readset, tempset;
    struct timeval tv;
    int on=1;
    memset(&myHostname, 0, sizeof(myHostname));
    memset(&serverHostname, 0, sizeof(serverHostname));
    memset(&peerHostForRFC, 0, sizeof(peerHostForRFC));

	srand(time(NULL));
    
    memset(&sinServer, 0, sizeof(sinServer));
    memset(&sinIncoming, 0, sizeof(sinIncoming));
    
    /* read host and port number from command line */
    if ( argc != 2 ) {
        fprintf(stderr, "Usage: %s <server-machine-name>\n", argv[0]);
        exit(1);
    }
    
    	DEBUG2("Debug: Create server socket\n");
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
    	gethostname(myHostname, sizeof(myHostname));
    	pHostentIncoming = gethostbyname(myHostname);
    	DEBUG("Hostname: %s\n", myHostname);
    	if ( pHostentIncoming == NULL ) {
        	fprintf(stderr, "%s: host not found (%s)\n", argv[0], myHostname);
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

    	DEBUG2("Child should not be exiting!\n");
    }
    else if (child_pid > 0) { // parent - connecting to server socket
    	/* fill in hostent struct */
    	close(incomingSocket);
    	strcpy(serverHostname, argv[1]);
    	printf("Server Hostname: %s", serverHostname);
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
    	len = send(serverSocket, myHostname, strlen(myHostname), 0);
    	if (len != strlen(myHostname)) {
    		perror("send");
    		exit(1);
    	}
    	DEBUG("\n101010101010101010101010101010101010\n");
    	DEBUG("Connected to Server! Registering with Server...\n");
    	DEBUG("   Sent host: %s\n", myHostname);
    	
    	// Wait for Ack
    	len = recv(serverSocket, buf, sizeof(buf)-1, 0);
    	if (len < 0) {
    		perror("recv:");
    	}
    	DEBUG2("  Ack\n");
    	
    	// Send port number
    	int32_t conv = htonl(peerPort);
    	len = send(serverSocket, &conv, sizeof(conv), 0);
    	if (len != sizeof(conv)) {
    		perror("send");
    		exit(1);
    	}
    	DEBUG("   Sent port: %d\n", peerPort);
    	
    	// Wait for Ack
    	len = recv(serverSocket, buf, sizeof(buf)-1, 0);
    	if (len < 0) {
    		perror("recv:");
    	}
    	DEBUG2("   Ack\n");
    	DEBUG("101010101010101010101010101010101010\n\n");
    	
    	callServerCommands(serverSocket, peerPort); // Contact server to give/get info
    	callPeerCommands(serverSocket);   // Contact peers to download RFCs
    	
    	DEBUG2("Parent should not be exiting!\n");
    	

    }
    else { // error forking
        printf("ERROR:  Could not fork a new process!\n");
    }
}
