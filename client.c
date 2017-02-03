#include <stdio.h>
#include <winsock2.h>
#include <windows.h>
#include <time.h>

WSADATA wsaData;
int serverPort = 3345;
int pollIntervall = 1; // seconds
char serverip[20];

int startWinsock() {
  WORD wVersionRequested;
  // Using MAKEWORD macro, Winsock version request 2.2
  wVersionRequested = MAKEWORD(2, 2);
  // Release WinSock DLL
  WSACleanup();
  return WSAStartup(wVersionRequested,&wsaData);
}

int main(int argc, char *argv[]) {
  SOCKET sock;
  SOCKADDR_IN addr;
  int wsaerr;
  long rc;
  int daemon = 1;
  char input[256];
  char output[256];
  time_t currtime;
  HANDLE h = GetStdHandle( STD_OUTPUT_HANDLE);
  WORD wOldColorAttrs;
  CONSOLE_SCREEN_BUFFER_INFO csbiInfo;

  /* save current color informations */
  GetConsoleScreenBufferInfo(h, &csbiInfo);
  wOldColorAttrs = csbiInfo.wAttributes;

  wsaerr = startWinsock();

  if (argc != 2) { /* argc should be 2 for correct execution */
    printf("Roboter Server IP: ");
    scanf("%15s", serverip);
    printf("Server ip set to '%s'..\n", serverip);
  } else {
    // TODO: Set IP from command line argument
  }
  if (wsaerr != 0) {
    /* Tell the user that we could not find a usable WinSock DLL.*/
    printf("Client: The Winsock dll not found!\n");
    printf("Client: error code: %d\n", wsaerr);
    return 0;
  }
  // Create the socket connection over TCP / IP
  sock = socket( AF_INET, SOCK_STREAM, 0 );
  if (sock < 0) {
    // Failed to create the socket
    printf("Client: Could not create socket!!");
  } else {
    memset(&addr,0,sizeof(SOCKADDR_IN)); // initialize with 0
    addr.sin_family=AF_INET;
    addr.sin_port=htons(serverPort);
    addr.sin_addr.s_addr=inet_addr(serverip); // set destination address
    rc = connect(sock,(SOCKADDR*)&addr,sizeof(SOCKADDR));
    if(rc==SOCKET_ERROR) {
      printf("Error: could not connect to %s! Error code: %d\n", serverip, WSAGetLastError());
    } else {
      printf("Connected to %s..\n",serverip);
      while (daemon) {
        time(&currtime);                  /* get current time */
        strcpy(input,"");
        strcpy(output,"");

        sprintf(output,"%s",".");
        //printf("Senden:%s \n",output);
        rc=send(sock,output, (int)strlen(output), 0);
        if(rc==0 || rc==SOCKET_ERROR) {
          printf("Server has closed connection\n");
          closesocket(sock); // Close the socket
          WSACleanup();
          exit(1);
        } else {
          rc=recv(sock,input,256,0);
          input[rc] = '\0';
          if(rc==0 || rc==SOCKET_ERROR) {
            printf("Server has closed connection\n");
            closesocket(sock); // Close the socket
            WSACleanup();
            exit(1);
          } else {
            if (strcmp(input,"1")==0) {
              SetConsoleTextAttribute (h, BACKGROUND_GREEN | FOREGROUND_INTENSITY);
            } else {
              SetConsoleTextAttribute (h, wOldColorAttrs);
            }
            //printf("%s \n",input);
            Sleep(pollIntervall * 100); // time in milliseconds
            system("cls");
          }
        }
      }
    }
  }
  WSACleanup();
  exit(1);
}
