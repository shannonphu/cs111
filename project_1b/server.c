#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <mcrypt.h>

// Pipes
int shellR_terminalW[2];
int terminalR_shellW[2];

// Note: pipe[0] for reading from pipe
//       pipe[1] for writing to pipe
#define TERMINALR  terminalR_shellW[0]
#define TERMINALW shellR_terminalW[1]
#define SHELLR   shellR_terminalW[0]
#define SHELLW  terminalR_shellW[1]

pid_t shellPID;
int newsockfd, sockfd;

pthread_t t1;
pthread_t t2;

int encryptOn;
#define IV "\034\098\306\151\163\121\377\112\354\051\315\272\253\362\373\343\106"
#define KEY_SIZE 16

void setupSockets(int portno);

void* readFromShell(void* arg);
void* writeToShell(void *arg);
void waitForShell(void);
void closePipes();

void sigpipe_handler(int signo);
void sigterm_handler(int signo);

void error(char *msg);

MCRYPT setupMcrypt(char *key);
void cleanupMcrypt(MCRYPT *td);

int main (int argc, char **argv) {
  atexit(closePipes);
  
  int ifd = 0;
  int ofd = 1;

  // Get arguments
  static struct option long_options[] =
  {
    {"port", required_argument, 0, 'p'},
    {"encrypt", no_argument, 0, 'e'},
    {0, 0, 0, 0}
  };

  char arg;
  pid_t pid;
  int portno;
  encryptOn = 0;
  
  while ((arg = getopt_long_only(argc, argv, "", long_options, NULL)) != -1) {
    switch (arg)
      {
      case '?':
	exit(-1);
	break;
      case 'e':
        encryptOn = 1;
        break;
      case 'p':
	portno = atoi(optarg);
	break;
      
      }
  } // end getting arguments

	setupSockets(portno);
	
	pipe(shellR_terminalW);
	pipe(terminalR_shellW);
	
	pid = fork();
	  
	// If forked process is child
	// - redirect pipes and stream
	// - exec shell
	// #define TERMINALR  terminalR_shellW[0]
	// #define TERMINALW shellR_terminalW[1]
	// #define SHELLR   shellR_terminalW[0]
	// #define SHELLW  terminalR_shellW[1]
	if (pid == 0) {
	  shellPID = getpid();

	  // Read from whats typed into terminal to process bash output
	  dup2(SHELLR, STDIN_FILENO);
	  // Send stdout and stderr to terminal to print out
	  dup2(SHELLW, STDOUT_FILENO);
	  dup2(SHELLW, STDERR_FILENO);
    
	  closePipes();
	  
	  execl("/bin/bash", "bash", NULL);
	}
	// Make threads to echo commands to terminal and read from
	// shell the bash output
	else {
	  // Handle SIGPIPE/SIGHUP signals
	  signal(SIGPIPE, sigpipe_handler);
	  signal(SIGTERM, sigterm_handler);
	  
	  close(SHELLR);
	  close(SHELLW);
	  

	  // Read shell's command output and write to terminal
	  pthread_create(&t1, NULL, readFromShell, NULL);
	  // Echo stdin to terminal and also pass command to shell
	  pthread_create(&t2, NULL, writeToShell, NULL);
	  
	  pthread_join(t1, NULL);
	  pthread_join(t2, NULL);
	}
  
  return 0;
}

// Read shell's output and pass to client
void* readFromShell(void* arg) {
  char buffer_char;
  int ret;

  // Set up encryption before sending to client
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
  
  while(ret = read(TERMINALR, &buffer_char, 1)) {
    // SIGPIPE reached (ie: in bash command was 'exit')
    if (ret == 0) {
      break;
    }
    // server received EOF from shell
    if (buffer_char == '\004') {
      exit(1);
    }
    else {
      // Deal with encryption                                                                                                                           
      if (encryptOn) {
        mcrypt_generic(md, &buffer_char, 1);
      }
      
      write(newsockfd, &buffer_char, 1);
    }
  }
}

// Read from client and send chars to shell process as input
void* writeToShell(void *arg) {
  int n;
  char foo;

  // Handle decryption set up
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
  
  while(1) {
    n = read(newsockfd, &foo, 1);

    if (encryptOn) {
      mdecrypt_generic(md, &foo, 1);
    }
    
    if (n <= 0 || foo == '\004') {
      close(TERMINALW);
      kill(shellPID, SIGTERM);
      exit(1);
    }
    else {
      write(TERMINALW, &foo, 1);
    }
  }

  if (encryptOn)
    cleanupMcrypt(&md);
}

void closePipes(void) {
  close(TERMINALR);
  close(TERMINALW);

  close(SHELLR);
  close(SHELLW);
}

void waitForShell(void) {
  // Wait for child process to exit
  int status;
  waitpid(-1, &status, 0);
  closePipes();
}

void sigpipe_handler(int signo) {
  kill(shellPID, SIGTERM);
  exit(2);
}

void sigterm_handler(int signo) {
  close(sockfd);
  close(newsockfd);
  return;
}

void setupSockets(int portno) {
  // Handle sockets
  int clilen;

  struct sockaddr_in serv_addr, cli_addr;
  int n;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket");
  bzero((char *) &serv_addr, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    error("ERROR on binding");
  listen(sockfd,5);
  clilen = sizeof(cli_addr);
  
  // Make the socket connection
  newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
  if (newsockfd < 0)
    error("ERROR on accept");
}

void error(char *msg)
{
  perror(msg);
  exit(1);
}

// Encryption methods                                                                                                                                    
MCRYPT setupMcrypt(char *key) {
  MCRYPT td;
  int n;

  td = mcrypt_module_open(MCRYPT_TWOFISH, NULL, MCRYPT_CFB, NULL );
  if (td == MCRYPT_FAILED) {
    exit(1);
  }
  n = mcrypt_enc_get_iv_size(td);

  if ((mcrypt_generic_init(td, key, KEY_SIZE, IV)) < 0) {
    exit(1);
  }

  return td;
}

void cleanupMcrypt(MCRYPT *td) {
  if (mcrypt_module_close(td) < 0) {
    exit(1);
  }
}
