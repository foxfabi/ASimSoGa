#include <stdio.h>
#include <winsock2.h>
#include <windows.h>
#include <time.h>
#include <stdlib.h>

#define MAX_CLIENTS 50
#define MAX_STANDING 30
#define MAXLINELENGTH 256

/*
 * Add libwsock32.a to Project build options ...
 * http://www.pronix.de/pronix-1197.html
 *
 * Based on http://www.c-worker.ch/tuts/wstut_op.php
 * Multiple Clients: http://www.c-worker.ch/tuts/select.php
 */

WSADATA wsaData;
char servername[255+1];
char serverip[255+1];
int serverPort = 3345; // defaultPort
char logmessage[255+1];
int running = 0;
int connections = 0;

// Settings from roboter.conf
int cfgMaxClients;   // amount of players
int cfgMaxStanding;  // amount of players standing together
int cfgAllStanding;  // minimal movements for a player
int cfgSpeed;        // speed of each movements

struct RoboterClient {
  SOCKET clientsock;
  int standing;   // true/false - client standing or not
  int counter;    // counter - amount of moves
  int completed;  // true/false - has completed challenge
};

struct RoboterClient RoboterClients[MAX_CLIENTS];

int writeLogEntry(char * message) {
  time_t currtime, now;
  struct tm tim;
  size_t i;
  char s[50];
  now = time(&currtime);                  /* get current time */
  tim = *(localtime(&now));
  i = strftime(s,50,"[%Y%m%d] [%H:%M:%S]",&tim);
  FILE * fp=fopen("server.log","a");
  fprintf(fp,"%s [%s]\n", s, message);
  fclose(fp);
  return 1;
}

void readConfig() {
  char line[MAXLINELENGTH];
  int len, error;
  int linenum=0;
  char name[256], value[256];
  char message[256];
  char* token;
  FILE * file=fopen("roboter.conf","r"); // !! UNIX Format
  error = 0;
  if ( file != NULL ) {
    while(fgets ( line, MAXLINELENGTH, file ) != NULL) {
      linenum++;
      //printf("Line %d: %s\n", linenum, line);
      if(line[0] == '#') continue; // comments
      len = strlen(line);
      if (len < 2) {
        continue;
      }
      token = strtok( line, "\t =\n\r" ) ;
      if( token != NULL && token[0] != '#' ) {
        sprintf(name, "%s", token ) ;
        token = strtok( NULL, "\t =\n\r" ) ;
        sprintf(value, "%s", token ) ;
      }

      if (strcmp(name,"speed") == 0) {
        cfgSpeed = atoi(value);
      }
      if (strcmp(name,"moves") == 0) {
        cfgAllStanding = atoi(value);
      }
      if (strcmp(name,"updown") == 0) {
        cfgMaxStanding = atoi(value);
      }
      if (strcmp(name,"players") == 0) {
        cfgMaxClients = atoi(value);
      }
    }
    fclose(file);
    // check settings
    if (cfgMaxClients > MAX_CLIENTS) {
      sprintf(message,"Error: Check settings MAX_CLIENTS: %d <= %d\n", cfgMaxClients, MAX_CLIENTS);
      error = 1;
    }
    if (cfgMaxStanding > MAX_STANDING) {
      sprintf(message,"Error: Check settings MAX_STANDING: %d <= %d\n", cfgMaxStanding, MAX_STANDING);
      error = 1;
    }
    if (error) {
      writeLogEntry(message);
      exit(0);
    }
  }
}

void initRoboterClients() {
  int i;
  for(i=0;i<cfgMaxClients;i++) {
    RoboterClients[i].clientsock = INVALID_SOCKET;
    RoboterClients[i].counter = 0;
    RoboterClients[i].completed = 0;
    RoboterClients[i].standing = 0;
  }
}

int startWinsock() {
    WORD wVersionRequested;
    // Using MAKEWORD macro, Winsock version request 2.2
    wVersionRequested = MAKEWORD(2, 2);
    // Release WinSock DLL
    WSACleanup();
    return WSAStartup(wVersionRequested,&wsaData);

}

int hostIP(char * hostname, char* ip) {
  struct hostent *he;
  struct in_addr **addr_list;
  int i;
  if ( (he = gethostbyname(hostname) ) == NULL) {
    writeLogEntry("Error: gethostbyname not found");
    return 1;
  }
  addr_list = (struct in_addr **) he->h_addr_list;
  for (i = 0; addr_list[i] != NULL; i++) {
    strcpy(ip, inet_ntoa(*addr_list[i]));
    return 0;
  }
  return 1;
}

int getRand(int min, int max) {
  static int init = 0;
  int rc;
  time_t currtime;

  if (init == 0) {
    srand(time(&currtime));
    init = 1;
  }
  rc = (rand() % (max - min + 1) + min);
  return (rc);
}

int randStandingClient() {
  int i, k;
  for( i = 0; i < cfgMaxStanding; i++ ) {
    k = getRand(0,(cfgMaxClients - 1));
    // check if slot is used
    if(RoboterClients[k].standing) {
      i--;  // already picked.
    } else {
      RoboterClients[k].standing = 1; // hasn't been picked yet.  Assign
      RoboterClients[k].counter++;
      sprintf(logmessage, "Client [%d] will be informed ... # %d", k, RoboterClients[k].counter);
      writeLogEntry(logmessage);
      if ((RoboterClients[k].counter >= cfgAllStanding ) && (!RoboterClients[k].completed)) {
        RoboterClients[k].completed = 1;
        sprintf(logmessage, "Client [%d] has completed challenge ... # %d", k, RoboterClients[k].counter);
        writeLogEntry(logmessage);
      }
    }
  }
  return 1;
}

int checkStanding() {
  int i, c;
  c = 0;
  for( i = 0; i < cfgMaxClients; i++ ) {
    if (RoboterClients[i].standing==1) {
      c++;
    }
  }
  if (c == cfgMaxStanding) {
    return 1;
  } else {
    sprintf(logmessage,"Error: Standing rule broken: %d", c);
    writeLogEntry(logmessage);
    // init stand counter
    for(i=0;i<cfgMaxClients;i++) {
        RoboterClients[i].standing=0;
    }
    randStandingClient(0);
    return 0;
  }
  return 1;
}

int checkCompleted() {
  int i;
  int completed = 0;
  for(i=0;i<cfgMaxClients;i++) {
    sprintf(logmessage,"Checking Client [%d] for fitness (%d)", i, RoboterClients[i].completed);
    writeLogEntry(logmessage);
    if(RoboterClients[i].clientsock==INVALID_SOCKET) {
      continue; // invalid socket,
    }
    if (RoboterClients[i].completed == 1) {
      completed++;
    } else {
    }
  }
  sprintf(logmessage,"Checking Challenge completed (%d/%d)", completed,cfgMaxClients);
  writeLogEntry(logmessage);
  if (completed == cfgMaxClients) {
    return 1;
  }
  return 0;
}

void initClients() {
  int i;
  for(i=0;i<cfgMaxClients;i++) {
    RoboterClients[i].clientsock = INVALID_SOCKET;
  }
}

void initStanding() {
  int i;
  // init standing socks array
  for(i=0;i<cfgMaxClients;i++) {
    RoboterClients[i].standing = 0;
  }
}

void initCompleteCounter() {
  int i;
  // init stand counter
  for(i=0;i<cfgMaxClients;i++) {
    RoboterClients[i].completed = 0;
  }
}

void initStandCounter() {
  int i;
  // init stand counter
  for(i=0;i<cfgMaxClients;i++) {
    RoboterClients[i].counter = 0;
  }
}

void closeServer(SOCKET sock) {
  writeLogEntry(".................................");
  closesocket(sock);
  WSACleanup();
  writeLogEntry("Stopped.");
  exit(1);
}

int main() {
    SOCKET sock;
    SOCKADDR_IN addr;
    int wsaerr, i;
    long rc;
    FD_SET socketSet;
    char input[256];
    char output[256];
    int daemon = 1;
    time_t currtime;
    clock_t start;
    int elapsed;

    FILE * fp=fopen("server.log","w");
    fclose(fp);
    readConfig();
    writeLogEntry(".................................");
    printf("Config: port=%d;speed=%d;players=%d;moves=%d;updown=%d;\n",cfgPort,cfgSpeed,cfgMaxClients,cfgAllStanding,cfgMaxStanding);
    sprintf(logmessage,"Config: port=%d,speed=%d;players=%d;moves=%d;updown=%d;",cfgPort,cfgSpeed,cfgMaxClients,cfgAllStanding,cfgMaxStanding);
    writeLogEntry(logmessage);
    writeLogEntry(".................................");
    FD_ZERO(&socketSet); // clean fd_set
    wsaerr = startWinsock();
    if (wsaerr != 0) {
        /* Tell the user that we could not find a usable WinSock DLL.*/
        writeLogEntry("Server: The Winsock dll not found!");
        sprintf(logmessage,"Server: error code: %d",wsaerr);
        writeLogEntry(logmessage);
        return 0;
    }

    // Create the socket connection over TCP / IP
    sock = socket( AF_INET, SOCK_STREAM, 0 );
    if (sock < 0) {
        // Failed to create the socket
        writeLogEntry("Server: Could not create socket!!");
        return 0;
    } else {
        //writeLogEntry("Server: Socket created!");
        // bind socket
        memset(&addr,0,sizeof(SOCKADDR_IN));
        addr.sin_family=AF_INET;
        addr.sin_port=htons(serverPort);
        addr.sin_addr.s_addr=ADDR_ANY;
        rc = bind(sock,(SOCKADDR*)&addr,sizeof(SOCKADDR_IN));
        if(rc == SOCKET_ERROR) {
            sprintf(logmessage,"Error: bind!! Error code: %d",WSAGetLastError());
            writeLogEntry(logmessage);
            closeServer(sock);
            return 0;
        } else {
            // In listen mode
            rc = listen(sock,10);
            if(rc == SOCKET_ERROR) {
                sprintf(logmessage,"Error: listen! Error code %d", WSAGetLastError());
                writeLogEntry(logmessage);
            } else {
                initRoboterClients();
                if(gethostname(servername, sizeof serverip) != 0) {
                    writeLogEntry("Error.... Hostname not found");
                    closeServer(sock);
                }
                hostIP(servername, serverip);
                printf("Info: Socket is ready on IP %s\n", serverip);
                sprintf(logmessage,"Info: Socket is ready on IP %s:%d", serverip, serverPort);
                writeLogEntry(logmessage);
                printf("Info: Waiting for %d clients\n",cfgMaxClients);
                sprintf(logmessage,"Info: Waiting for %d clients",cfgMaxClients);
                writeLogEntry(logmessage);

                start = clock();
                while (daemon) {
                    time(&currtime);                  /* get current time */
                    /* after 5 seconds renew standog clients */
                    elapsed = (((int)clock() - start) / 1);
                    //elapsed = (((int)clock() - start));
                    if (elapsed >= cfgSpeed*CLOCKS_PER_SEC) {
                        sprintf(logmessage, "Informing clients ... # %d", elapsed);
                        writeLogEntry(logmessage);
                        initStanding();
                        randStandingClient();
                        checkStanding();
                        start = clock(); // reset timer
                    }

                    FD_ZERO(&socketSet); // clean fd_set
                    FD_SET(sock,&socketSet); // Adds the socket of the connections
                    // Add all valid client sockets (only those that are not INVALID_SOCKET)
                    for(i=0;i<cfgMaxClients;i++) {
                        if(RoboterClients[i].clientsock!=INVALID_SOCKET) {
                        //if(clients[i]!=INVALID_SOCKET) {
                            FD_SET(RoboterClients[i].clientsock,&socketSet);
                        }
                    }

                    rc = select(0,&socketSet,NULL,NULL,NULL);

                    if(rc == SOCKET_ERROR) {
                        sprintf(logmessage,"Error: select, error code: %d",WSAGetLastError());
                        writeLogEntry(logmessage);
                        return 1;
                    }
                    if(rc == -1) {
                        sprintf(logmessage,"Error: %s", strerror(errno));
                        writeLogEntry(logmessage);
                    }
                    if(rc == 0) {
                        sprintf(logmessage,"Error: %s", strerror(errno));
                        writeLogEntry(logmessage);
                    }
                    // AcceptSocket is in fd_set? => Accept connection (if it has space)
                    if(FD_ISSET(sock,&socketSet)) {
                        // Find a free space for the new client, and accept the connection
                        for(i=0;i<cfgMaxClients;i++) {
                            if(RoboterClients[i].clientsock==INVALID_SOCKET) {
                            //if(clients[i]==INVALID_SOCKET)  {
                                RoboterClients[i].clientsock = accept(sock,NULL,NULL); // If accept fails, INVALID_SOCKET is returned.
                                sprintf(logmessage,"Info: New Client connected (%d)", i);
                                writeLogEntry(logmessage);
                                connections++;
                                // wait for cfgMaxClients.. no break
                                //break;
                            }
                        }
                    }

                    if (connections != cfgMaxClients) {
                        sprintf(logmessage,"Info: Waiting for clients %d / %d", connections, cfgMaxClients);
                        writeLogEntry(logmessage);
                        initStanding();
                        initStandCounter();
                        continue;
                    }

                    // Check which client sockets are in fd_set
                    for(i=0;i<cfgMaxClients;i++) {
                        //if(clients[i]==INVALID_SOCKET) {
                        if(RoboterClients[i].clientsock==INVALID_SOCKET) {
                            continue; // Invalid socket,
                        }
                        //if(FD_ISSET(clients[i],&socketSet)) {
                        if(FD_ISSET(RoboterClients[i].clientsock,&socketSet)) {
                            strcpy(input,"");
                            strcpy(output,"");
                            sprintf(output,"%d",0);
                            rc=recv(RoboterClients[i].clientsock,input,256,0);
                            // Check whether the connection was closed or an error occurred
                            if(rc==0 || rc==SOCKET_ERROR) {
                                sprintf(logmessage, "Client [%d] has closed connection", i);
                                writeLogEntry(logmessage);
                                closesocket(RoboterClients[i].clientsock); // Close the socket
                                RoboterClients[i].clientsock=INVALID_SOCKET;
                                RoboterClients[i].counter=0;
                                connections--;
                            } else {
                                input[rc]='\0';
                                // check if should sent Stand up command ..
                                //if (standing[i]==1) {
                                if (RoboterClients[i].standing == 1) {
                                    sprintf(output,"%d",1);
                                }
                                rc=send(RoboterClients[i].clientsock,output,(int)strlen(output),0);
                                if(rc==0 || rc==SOCKET_ERROR) {
                                    sprintf(logmessage, "Client [%d] has closed connection", i);
                                    writeLogEntry(logmessage);
                                    closesocket(RoboterClients[i].clientsock); // close socket
                                    RoboterClients[i].clientsock=INVALID_SOCKET;
                                    //standcounter[i]=0;
                                    RoboterClients[i].counter = 0;
                                    connections--;
                                }
                            }
                        }
                    }
                    elapsed = (((int)clock() - start) / 1);
                    if (elapsed >= cfgSpeed*CLOCKS_PER_SEC) {
                        if (checkCompleted()) {
                            Sleep(cfgSpeed*100);
                            sprintf(logmessage,"Closing Server ...........");
                            writeLogEntry(logmessage);
                            closeServer(sock);
                        }
                    }
                }
            }

        }
    }
    closeServer(sock);
    return 1;
}
