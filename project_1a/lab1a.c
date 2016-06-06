#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>

// Pipes
int shellR_terminalW[2];
int terminalR_shellW[2];

// Note: pipe[0] for reading from pipe
//       pipe[1] for writing to pipe
#define TERMINALR  terminalR_shellW[0]
#define TERMINALW shellR_terminalW[1]
#define SHELLR   shellR_terminalW[0]
#define SHELLW  terminalR_shellW[1]
#define RET_PHRASE_NORM "Shell exited with return code: %d\n"
#define RET_PHRASE_SIG "Shell exited with signal code: %d\n"
pid_t shellPID;

struct termios originalTerminalAttr;

void* readFromShell(void* arg);
void* writeToShell(void *arg);
void resetTerminal(void);
void waitForShell(void);
void setupTerminalMode(void);
char* readAndWrite(int ifd, int ofd);
void closePipes();
void sigpipe_handler(int signo);
void sigint_handler(int signo);
void sighup_handler(int signo);


int main (int argc, char **argv) {
  setupTerminalMode();

  int ifd = 0;
  int ofd = 1;

  // Get arguments
  static struct option long_options[] =
  {
    {"shell", no_argument, 0, 's'},
    {0, 0, 0, 0}
  };

  char arg;
  pid_t pid;
  while ((arg = getopt_long_only(argc, argv, "s", long_options, NULL)) != -1) {
    switch (arg)
      {
      case '?':
	fprintf(stderr, "Invalid argument.\n");
	resetTerminal();
	exit(-1);
      break;
      case 's':
      {
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
	  resetTerminal();
	  perror("exec() failed");
	  _exit(127);
	}
	// Make threads to echo commands to terminal and read from
	// shell the bash output
	else {
	  // Handle SIGPIPE, SIGINT, SIGHUP signals
	  signal(SIGPIPE, sigpipe_handler);
	  signal(SIGINT, sigint_handler);
	  signal(SIGHUP, sighup_handler);
	  
	  close(SHELLR);
	  close(SHELLW);

	  atexit(waitForShell);
	  
	  pthread_t t1, t2;

	  // Read shell's command output and write to terminal
	  pthread_create(&t1, NULL, readFromShell, NULL);
	  // Echo stdin to terminal and also pass command to shell
	  pthread_create(&t2, NULL, writeToShell, NULL);
	  
	  pthread_join(t1, NULL);
	  pthread_join(t2, NULL);
	}
	break;
      }
      }
  } // end getting arguments
  
  readAndWrite(ifd, ofd);
  
  return 0;
}

char* readAndWrite(int ifd, int ofd) {
  // Read in stdin and write
  char buffer_char;
  while (1) {
    read(ifd, &buffer_char, 1);
    if (buffer_char == '\004') {
      exit(0);
    } else if (buffer_char == 10) { 
      // \n char mapped to \r\n 
      write(ofd, "\r\n", 2);
    } else {
      write(ofd, &buffer_char, 1);
    }
  }
}

// Read shell's output and write to terminal
void* readFromShell(void* arg) {
  char buffer_char;
  int ret;
  while(ret = read(TERMINALR, &buffer_char, 1)) {
    // SIGPIPE reached (ie: in bash command was 'exit')
    if (ret == 0) {
      break;
    }
    // terminal received EOF from shell
    if (buffer_char == '\004') {
      write(1, "\r\n", 2);
      printf(RET_PHRASE_NORM, 1);
      exit(1);
    }
    else
      write(1, &buffer_char, 1);
  }
}

// Write to terminal and send chars to shell process as input
void* writeToShell(void *arg) {
  char foo;
  while(read(0, &foo, 1)) {
    // shell received ^D EOF from terminal
    if (foo == '\004') {
      close(TERMINALW);
      kill(shellPID, SIGHUP);
    }
    // shell received ^C
    else if (foo == 3) {
      kill(shellPID, SIGINT);
    } else {
      write(1, &foo, 1);
      write(TERMINALW, &foo, 1);
    }
  }
}

void closePipes(void) {
  close(TERMINALR);
  close(TERMINALW);

  close(SHELLR);
  close(SHELLW);
}

// Immediately (TCSANOW) reset stdin terminal to original settings
void resetTerminal(void) {
  tcsetattr(0, TCSANOW, &originalTerminalAttr);
}

void waitForShell(void) {
  // Wait for child process to exit
  int status;
  waitpid(-1, &status, 0);
  
  if (WIFEXITED(status)) {
    printf(RET_PHRASE_NORM, WEXITSTATUS(status));
  } else if (WIFSIGNALED(status)) {
    printf(RET_PHRASE_SIG, WTERMSIG(status));
  }
  
  closePipes();
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

void sigpipe_handler(int signo) {
  exit(1);
}

void sigint_handler(int signo) {
  return;
}

void sighup_handler(int signo) {
  exit(0);
}
