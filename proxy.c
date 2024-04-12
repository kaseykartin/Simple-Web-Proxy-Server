#include <stdio.h>

#include "csapp.h"


/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

// Header Keys
static const char *userKey = "User-Agent: ";
static const char *hostKey = "Host: ";
static const char *connectionKey = "Connection: ";
static const char *pConnectionKey = "Proxy-Connection: ";

// Function to check if a header has the specified key
int check_for_key(char *header, const char *key)
{
    int length = strlen(key);
    return strncmp(header, key, length) == 0;
}

// Function to parse URI
void parseURI(char *uri, char *hostname, unsigned int *port, char *query)
{
    char *temp;

    if (check_for_key(uri, "http://"))
    {
        temp = uri + 7; // To skip the http:// characters
    }
    else
    {
        temp = uri;
    }

    // Iterate until port or query
    while (*temp != ':' && *temp != '/')
    {
        *hostname = *temp;
        temp++;
        hostname++;
    }

    *hostname = '\0'; // Terminate string

    // Get port and query
    if (*temp == ':')
    {
        sscanf(temp + 1, "%d%s", port, query);
    }
    else
    {
        *port = 80; // Default to 80
        strcpy(query, temp);
    }
}

// Functin to check if we have a standard header
int otherHeader(char *header)
{
    return !(check_for_key(header, userKey) ||
    check_for_key(header, connectionKey) ||
    check_for_key(header, pConnectionKey));
}

// Function to crete our header string for the server request
void createHeader(rio_t rio, char *headers, char *hostname, char *query)
{
    char buf[MAXLINE], hostHeader[MAXLINE] = "";
    int hostFlag = 1, userFlag = 1;

    // HTTP request header
    sprintf(headers, "GET %s HTTP/1.0\r\n", query);

    // Default value headers
    strcat(headers, "Connection: close\r\n");
    strcat(headers, "Proxy-Connection: close\r\n");

    // Check if the client has sent any headers
    if (strcmp(rio.rio_bufptr, "") != 0)
    {
        // Iterate over existing headers
        while (Rio_readlineb(&rio, buf, MAXLINE) != 0)
        {
            if (strcmp(buf, "\r\n") == 0) // Reached the end of any headers
            {
                break;
            }
            else if (check_for_key(buf, hostKey)) // Brouwser overrides host header
            {
                strcat(headers, buf);
                hostFlag = 0;
            }
            else if (check_for_key(buf, userKey)) // Browser overrides user agent header
            {
                strcat(headers, buf);
                userFlag = 0;
            }
            else if (otherHeader(buf)) // Any other headers
            {
                strcat(headers, buf);
            }
        }
    }

    // If the host or user agent are not set by existing headers
    if (hostFlag)
    {
        sprintf(hostHeader, "Host: %s\r\n", hostname);
        strcat(headers, hostHeader);
    }

    if (userFlag)
    {
        strcat(headers, user_agent_hdr);
    }

    // End of request
    strcat(headers, "\r\n");
}

// Function for forwarding the client request to the server
void forwardRequest(int clientfd, rio_t rio, char *uri)
{
    int serverfd;
    char hostName[MAXLINE], portStr[8], query[MAXLINE], headers[MAXLINE] = "", response[MAXLINE] = "";
    unsigned int portNum;
    size_t length;

    parseURI(uri, hostName, &portNum, query);
    sprintf(portStr, "%d", portNum);

    // open connection to destination server
    if ((serverfd = open_clientfd(hostName, portStr)) < 0)
    {
        return;
    }

    // create our header string
    createHeader(rio, headers, hostName, query);

    // Write to server socket
    Rio_readinitb(&rio, serverfd);
    Rio_writen(serverfd, headers, strlen(headers));

    // read server response
    while ((length = Rio_readlineb(&rio, response, MAXLINE)) != 0)
    {
        rio_writen(clientfd, response, length); //write server response to client
    }

    Close(serverfd);
}

// Function for handling clients proxxy request
void handleRequest(int clientfd)
{
    char buf[MAXLINE], method[8], uri[MAXLINE], version[8];
    rio_t rio;

    Rio_readinitb(&rio, clientfd);
    memset(rio.rio_buf, 0, 8192);

    // Receive the HTTP request
    if (!Rio_readlineb(&rio, buf, MAXLINE))
    {
        return;
    }

    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET") != 0) //Discards HTTPS requests
    {
        return;
    }

    forwardRequest(clientfd, rio, uri);
}

int main(int argc, char **argv)
{
    int listenfd, connectfd;
    char hostName[MAXLINE], port[MAXLINE];
    socklen_t clientLen;
    struct sockaddr_storage clientAd;

    if (argc != 2) // Check for port input
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return 1;
    }

    if ((listenfd = open_listenfd(argv[1])) < 0)
    {
        printf("Unable to open port %s\n", argv[1]);
        return 1;
    }

    while (1) // Infinite Loop until user hits "ctrl + C"
    {
        clientLen = sizeof(clientAd);

        connectfd = Accept(listenfd, (SA *)&clientAd, &clientLen);

        Getnameinfo((SA *)&clientAd, clientLen, hostName, MAXLINE, port, MAXLINE, 0);

        printf("Accepted connection from (%s, %s)\n", hostName, port);

        handleRequest(connectfd);
        
        Close(connectfd);
    }

    return 0;
}
