#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define MAX_LEN 40

void handleSegmentationFault(int);

int main (int argc, char **argv) {
  char ch;

  static struct option long_options[] =
    {
      {"input=", required_argument, 0, 'i'},
      {"output=", required_argument, 0, 'o'},
      {"segfault", no_argument, 0,'s'},
      {"catch", no_argument, 0,'c'},
      {0, 0, 0, 0}
    };

  int ifd = 0;
  int ofd = 1;
  int willSegfault = 0;
  
  int option_index = 0;
  while ((ch = getopt_long_only(argc, argv, "", long_options, NULL)) != -1)
    {
      // check to see if a single character or long option came through
      switch (ch)
	{
	case 'i':
	  {
	    ifd = open(optarg, O_RDONLY);
	    if (ifd == -1) {
	      perror("Cannot open file");
	      exit(1);
	    }
	    close(0);
	    dup(ifd);
	  }
	  break;
	  
	case 'o':
	  // Reset output file descriptor
	  ofd = creat(optarg, 0666);
	  if (ofd < 0) {
	    perror("Can't make file");
	    exit(2);
	  }
	  close(1);
	  dup(ofd);

	  break;
	  
	case 's':
	{
	  willSegfault = 1;
	  break;
	}
	case 'c':
	  signal(SIGSEGV, handleSegmentationFault);
	  break;
	  
	default:
	  printf("Invalid argument.\n");
	}
    } // endwhile

  // Handle --segfault and --catch args

  if (willSegfault) {
    char *crash = NULL;
    *crash = '@';
  }

  // Read a character at a time until reached EOF
  char *buffer;
  int charCount = 0;

    ssize_t file_size;
    char file_char;
    buffer = malloc(1);
	    
    while ((file_size = read(ifd, &file_char, 1)) > 0) {
      if (file_char == EOF) {
	break;
      }
      buffer[charCount++] = file_char;
      buffer = realloc(buffer, charCount);
    }
  


    int res = write(1, buffer, charCount);
    if (res == -1) {
      perror("Cannot write to file");
    }

    close(ifd);
    close(ofd);
    free(buffer);    
  
  return 0;
}

void handleSegmentationFault(int signum)
{
  fprintf(stderr, "Caught segmentation fault with signal number %d\n", signum);
  exit(3);
}
