#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <getopt.h>
#include <termios.h>
#include <pthread.h>
#include <limits.h>
#include <signal.h>
#include <mcrypt.h>
#include <fcntl.h>

struct termios originalTerminalAttr;
int sockfd, logfd, logOptionOn;
pthread_t readThread, writeThread;

int encryptOn;
#define IV "\034\098\306\151\163\121\377\112\354\051\315\272\253\362\373\343\106"
#define KEY_SIZE 16

void setupTerminalMode(void);
void resetTerminal(void);
void setupSockets(int portno, int logfd, int logOptionOn);
void setupThreads(void);
void* writeToDisplay(void* arg);
void* readFromServer(void *arg);
void handleUserExit(int readRetVal);

MCRYPT setupMcrypt(char *key);
void cleanupMcrypt(MCRYPT *td);

int numPlaces (int n) {
  if (n < 0) return numPlaces ((n == INT_MIN) ? INT_MAX : -n);
  if (n < 10) return 1;
  return 1 + numPlaces (n / 10);
}

void error(char *msg)
{
  perror(msg);
}

int main(int argc, char *argv[])
{
  setupTerminalMode();
  
  // Get arguments
  
  static struct option long_options[] =
    {
      {"port=", required_argument, 0, 'p'},
      {"encrypt", no_argument, 0, 'e'},
      {"log=", required_argument, 0, 'l'},
      {0, 0, 0, 0}
    };

  char arg;
  int portno;
  logOptionOn = 0;
  encryptOn = 0;
    
  while ((arg = getopt_long_only(argc, argv, "", long_options, NULL)) != -1) {
    switch (arg)
      {
      case '?':
        fprintf(stderr, "Invalid argument.\n");
	break;
      case 'p':
	{
	  portno = atoi(optarg);
	  break;
	}
      case 'l':
	logfd = creat(optarg, 0666);
	if (logfd < 0) {
	  perror("Can't make file");
	  exit(-1);
	}
	logOptionOn = 1;
	break;
      case 'e':
	encryptOn = 1;
	break;
      }
  }

  setupSockets(portno, logfd, logOptionOn);
  setupThreads();
   
  return 0;
}

// Immediately (TCSANOW) reset stdin terminal to original settings
void resetTerminal(void) {
  tcsetattr(0, TCSANOW, &originalTerminalAttr);
}

void setupTerminalMode(void) {
  // Save original terminal attributes and at exit reset attr to original

 tcgetattr(0, &originalTerminalAttr);
 atexit(resetTerminal);

 struct termios newTerminalAttr;
 tcgetattr(0, &newTerminalAttr);

 // removes ICANON and ECHO for non-canonical and no-echo input mode
 // no-echo: input is not immediately re-echoed as output           
 newTerminalAttr.c_lflag &= ~(ICANON|ECHO);
 newTerminalAttr.c_iflag = ICRNL;
 newTerminalAttr.c_cc[VMIN] = 1;
 newTerminalAttr.c_cc[VTIME] = 0;

 int ret = tcsetattr(0, TCSANOW, &newTerminalAttr);
 if (ret != 0) {
   perror("Error in setting new terminal attributes");
 }
}

void setupThreads(void) {
  pthread_create(&writeThread, NULL, writeToDisplay, NULL);
  pthread_create(&readThread, NULL, readFromServer, NULL);

  pthread_join(writeThread, NULL);
  pthread_join(readThread, NULL);
}

void setupSockets(int portno, int logfd, int logOptionOn) {
  // Socket part
  int n;

  struct sockaddr_in serv_addr;
  struct hostent *server;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  // Get hostname
  server = gethostbyname("localhost");
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    exit(0);
  }
  
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, 
	(char *)&serv_addr.sin_addr.s_addr,
	server->h_length);
  serv_addr.sin_port = htons(portno);

  if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) 
    error("ERROR connecting");
}
  
void* writeToDisplay(void* arg) {
  // Read input and send
  int n;
  char buffer_char;

  MCRYPT md;
  if (encryptOn) {
    int fd = open("my.key", O_RDONLY);
    if (fd < 0) {
      error("Cannot open output file\n");
      exit(1);
    }
    char key[KEY_SIZE];
    read(fd, key, KEY_SIZE);
    md = setupMcrypt(key);
    close(fd);
  }

  while (1) {
    n = read(0, &buffer_char, 1);

    if (n <= 0 || buffer_char == '\004') {
      handleUserExit(n);
    } else {
      char nonEncrypted = buffer_char;
      
      // Deal with encryption
      if (encryptOn) {
	mcrypt_generic(md, &buffer_char, 1);
      }
      
      n = write(sockfd, &buffer_char, 1);
      if (n < 0) 
	error("ERROR writing to socket");

      write(1, &nonEncrypted, 1);

      if (logOptionOn) {
	write(logfd, "SENT 1 byte: ", 13);
	write(logfd, &buffer_char, 1);
	write(logfd, "\n", 1);
      }
    }
  }

  if (encryptOn)
    cleanupMcrypt(&md);
}

void* readFromServer(void *arg) {
  int n;
  char more;

  // Set up decryption from server
  MCRYPT md;
  if (encryptOn) {
    int fd = open("my.key", O_RDONLY);
    if (fd < 0) {
      error("Cannot open output file\n");
      exit(1);
    }
    char key[KEY_SIZE];
    read(fd, key, KEY_SIZE);
    md = setupMcrypt(key);
    close(fd);
  }
  
  while (1) {
    n = read(sockfd, &more, 1);
    if (n <= 0) {
      pthread_cancel(writeThread);
      close(sockfd);
      error("ERROR reading from socket");
      exit(1);
    }
    
    if (logOptionOn) {
      write(logfd, "RECEIVED 1 byte: ", 17);
      write(logfd, &more, 1);
      write(logfd, "\n", 1);
    }

    // Decrypt char
    if (encryptOn) {
      mdecrypt_generic(md, &more, 1);
    }

    write(1, &more, 1);

  }    
}

void handleUserExit(int readRetVal) {
  int n = readRetVal;
  
  pthread_cancel(readThread);
  close(sockfd);
 
  if (logOptionOn) {	
    close(logfd);
    }

  // Handle exit statuses
  if (n < 0)
    exit(1);
  else
    exit(0);
}

// Encryption methods
MCRYPT setupMcrypt(char *key) {
  MCRYPT td;
  int n;

  td = mcrypt_module_open(MCRYPT_TWOFISH, NULL, MCRYPT_CFB, NULL );
  if (td == MCRYPT_FAILED) {
    printf("fail to open module\n");
    exit(1);
  }
  n = mcrypt_enc_get_iv_size(td);

  if ((mcrypt_generic_init(td, key, KEY_SIZE, IV)) < 0) {
    printf("generic init error\n");
    exit(1);
  }

  return td;
}

void cleanupMcrypt(MCRYPT *td) { 
  if (mcrypt_module_close(td) < 0) {
    exit(1);
  }
}
