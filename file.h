/*
*VR447002
*Karol Sebastian Pasieczny
*9/01/2021
*Forza 4
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <malloc.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
//config constants

#define SIZE_MIN 5
#define SIZE 12
#define MAX_BUF 1024
//shared memory keys

#define KEY_BRD 73616
#define KEY_PID 34261
#define KEY_P1 28523
#define KEY_P2 12512
#define KEY_DIM 25932
#define KEY_SEM1 83753
#define KEY_SEM2 23594
#define MAX 12

//ansi colors
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

//structures
struct player {
	pid_t pid;
        char symb;
        char name[MAX];
	//2 flags: one for solo players, the other to check win
	int solo;
	//win 1, lose 0. If draw both players win
	int win;
	int score;
};

struct dimensions {
	int rows;
	int cols;
	pid_t pid;
	
};
