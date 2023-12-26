#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <pty.h>
#include <termios.h>
#include <fcntl.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/select.h>
#include <sys/wait.h>

#define CLR  "\x1B[94m"
#define CLR2   "\x1B[97m"
#define RES  "\x1B[0m"

int reset=0;
int master;

char* getpath(char *program){
	const char* env=getenv("PATH");
	char *p=strdup(env);
	char *token;
	char path[PATH_MAX];
  token = strtok (p,":");
  token = strtok (NULL, ":"); /*skip first */

  while (token != NULL)
  {
    snprintf(path,PATH_MAX,"%s/%s",token,program);
		if(access(path, F_OK|X_OK) == 0){
			free(p);
			return strdup(path);
		}
    token = strtok (NULL, ":");
  }
  free(p);
  return NULL;
}

bool isspecial(char s){
  const char set[]={
  '(',')',
  '[',']',
  '{','}',
  '*','+','-','/',
  '.',',',';',':',
  '\\','!','"','#',
  '%','&','@','$',
  '=','?','*'};
  int i;
  for(i=0;i<strlen(set);i++){
		if(s==set[i])
  		return true;
  }
	return false;
}

void printcolor(char s){

  if(isdigit(s)){
    switch(reset){
    case 0:
  		write(STDOUT_FILENO,CLR,5);
  		break;
  	case 2:
    	write(STDOUT_FILENO,RES,5);
  		write(STDOUT_FILENO,CLR,5);
			break;
    }
  	reset=1;
  }else if(isspecial(s)){
    switch(reset){
    case 0:
  		write(STDOUT_FILENO,CLR2,5);
  		break;
  	case 1:
    	write(STDOUT_FILENO,RES,5);
  		write(STDOUT_FILENO,CLR2,5);
  		break;
    }
    reset=2;
  }else{
    if(reset)
    	write(STDOUT_FILENO,RES,5);
    reset=0;
  }
  
  write(STDOUT_FILENO,&s,1);
}

int ttySetRaw(int fd)
{
  struct termios t;

  if (tcgetattr(fd, &t) == -1)
      return -1;

  t.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO|ECHONL);
/*
  t.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR |
                    INPCK | ISTRIP | IXON | PARMRK);
  
  t.c_oflag &= ~OPOST;               
*/
  t.c_cc[VMIN] = 1;                   /* Character-at-a-time input */
  t.c_cc[VTIME] = 0;                  /* with blocking */

  if (tcsetattr(fd, TCSAFLUSH, &t) == -1)
      return -1;

  return 0;
}

void sighand(int sig){
	struct winsize w;
	ioctl(STDIN_FILENO, TIOCGWINSZ, &w);
	ioctl(master, TIOCSWINSZ, &w);
}

void spawn(int argc,char **argv,char *prog){
    char **child_argv;
    int i;

    child_argv = (char **)malloc(argc * sizeof(char *));
    child_argv[0]=prog;
    for (i = 1; i < argc; i++) {
      child_argv[i] = strdup(argv[i]);

    }
  	child_argv[i] = NULL;
    execvp(child_argv[0], child_argv);
}


int main(int argc,char *argv[])
{

  pid_t pid;
  struct winsize w;
  struct termios t;

	char *prog=getpath(argv[0]);
	if(!prog){
  	printf("couldnt find program: %s\n",argv[0]);
		return 1;
	}
// Should not fuck with pipes 
// doesnt work everytime ?
	if(!isatty(STDOUT_FILENO)){
		spawn(argc,argv,prog);
		exit(0);
	}
  tcgetattr(STDIN_FILENO, &t);
  ioctl(STDIN_FILENO, TIOCGWINSZ, &w);
	ttySetRaw(STDIN_FILENO);
	signal(SIGWINCH,sighand);

  pid = forkpty(&master, NULL, NULL, &w);
  // impossible to fork
  if (pid < 0) {
      return 1;
  }

  // child
  else if (pid == 0) {
		spawn(argc,argv,prog);
  }

  // parent
  else {
    // remove the echo
    struct termios tios;
    tcgetattr(master, &tios);
    tios.c_lflag &= ~(ECHO | ECHONL);

    tcsetattr(master, TCSAFLUSH, &tios);
    while (1) {

      fd_set read_fd;
      fd_set write_fd;
      fd_set except_fd;

      FD_ZERO(&read_fd);
      FD_ZERO(&write_fd);
      FD_ZERO(&except_fd);

      FD_SET(master, &read_fd);
      FD_SET(STDIN_FILENO, &read_fd);

      select(master+1, &read_fd, &write_fd, &except_fd, NULL);

      char input;
      char output;

      if (FD_ISSET(master, &read_fd))
      {
        if (read(master, &output, 1) != -1)
          printcolor(output);
        else
        	break;
      }

      if (FD_ISSET(STDIN_FILENO, &read_fd))
      {
        read(STDIN_FILENO, &input, 1);
        write(master, &input, 1);
      }
    }
  }
  tcsetattr(STDIN_FILENO, TCSANOW, &t);

  return 0;
}
