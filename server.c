/*
*VR447002
*Karol Sebastian Pasieczny
*9/01/2021
*Forza 4 
*/

#include "file.h"

int shmid, shmdim, shmp1, shmp2, shmsem1, shmsem2;
static volatile int state = 0;
time_t t;
pid_t pla1,pla2;

void sig_handler(int sig){
	if(state){
		//terminate clients
		kill(pla1,SIGINT);
		kill(pla2,SIGINT);
		//close memory
		shmctl(shmid, IPC_RMID, NULL);
	        shmctl(shmdim, IPC_RMID, NULL);
        	shmctl(shmp1, IPC_RMID, NULL);
        	shmctl(shmp2, IPC_RMID, NULL);
        	semctl(shmsem1, 0, IPC_RMID);
        	semctl(shmsem2, 0, IPC_RMID);
        	signal(SIGINT,SIG_DFL);
		//terminate server
		//exit(0);
		signal(SIGINT, SIG_DFL);
		kill(getpid(),SIGINT);
	}else{
		printf("\nCTRL_C was pressed, On another press the program will close\n");
        	fflush(stdout);
	}
	state++;
}

//if a player leaves, the other wins
void disconnect_handler(int sig){
	//if process play1 exist send him a signal
	printf("\nClient disconnected. Closed game.\n");
	fflush(stdout);
	kill(pla1,SIGUSR1);
	kill(pla2,SIGUSR1);
	//close memory
	shmctl(shmid, IPC_RMID, NULL);
	shmctl(shmdim, IPC_RMID, NULL);
	shmctl(shmp1, IPC_RMID, NULL);
	shmctl(shmp2, IPC_RMID, NULL);
	semctl(shmsem1, 0, IPC_RMID);
	semctl(shmsem2, 0, IPC_RMID);
        signal(SIGINT,SIG_DFL);
	//terminate costintently
	signal(SIGINT,SIG_DFL);
	kill(getpid(),SIGINT);
		
}

int cpuMove(char **board, struct dimensions *dims, struct player *cpu, int cpuCol){
	int done=0;
	int randCol, i;
	/* Intializes random number generator */
	srand(time(NULL));
   	while(1){
		/* Print random numbers from 0 to board dimensions-1 */

		randCol = ((rand() % dims->cols) + cpuCol) / 2; //doing like this value should be random but closer to last move
		for(i=0; i<dims->rows; i++)
			if(board[i][randCol] == ' '){
                      		board[i][randCol] = cpu->symb;
				return randCol;
			}
                
        }
}
//the num of spaces max can be rows*dims
int isFull(struct dimensions *dims, int numSpaces){
	if(dims->rows*dims->cols == numSpaces)
		return 1;
	else
		return 0;
}

void initBoard(char **board, struct dimensions *dims){
	int i,j;
	for(i=0; i<dims->rows; i++){
		for(j=0; j<dims->cols; j++)
			board[i][j] = ' '; //0
	}
}
//returns 0 if no one won, the winning player if someone won 
char checkWin(char **board, struct dimensions *dims){
//iterate rows and columns to check values	
	int r,c;
	for (r = 0; r < dims->rows; r++) {
		for (c = 0; c < dims->cols; c++) { //iterate columns. board[0].length is the width of the board
            	//get value to check
		char player = board[r][c];
		
            	//break on the empty slots and move on the next iteration
		if (player == ' ')
                	continue;

		/* The win check algorithm checks for four in a row equal symbols
		 * In the first if clause the row is checked, in the second if vertical and
		 * vertical diagonal values are checked when needed (for example on the upper 3 
		 * rows there's no need for vertical and diagonal checks, since there wouldn't be 
		 * enough space to hold in 4 sequential equal symbols)
		 *
		*/
		
		//HORIZONTAL CHECK
           	if (c+3 < dims->cols && player == board[r][c+1] && player == board[r][c+2] && player == board[r][c+3])
           	     return player;

		//VERTICAL AND DIAGONAL CHECK
          	if (r+3 < dims->rows) {
              		if (player == board[r+1][c] && player == board[r+2][c] && player == board[r+3][c])
          	       		return player;
        		if (c + 3 < dims->cols && player == board[r+1][c+1] && player == board[r+2][c+2] && player == board[r+3][c+3])
                    		return player;
                	if (c - 3 >= 0 && player == board[r+1][c-1] && player == board[r+2][c-2] && player == board[r+3][c-3])
               			return player;
            }
        }
    }
    return ' '; // no winner found
}



int main(int argc, char *argv[], char *env[]){
	/* PARAMETERS CHECK */
	if(argc<5){
		printf("Not enough arguments. Correct syntax is: ./server <number of rows> <number of columns> <first symbol> <second symbol>\n");
		exit(1);
	}
	//check size of the board
	if(atoi(argv[1])<5 || atoi(argv[2])<5 || atoi(argv[1])>12 || atoi(argv[2])>12){
		printf("The numbers of rows and columns has to be %d to %d and %d to %d respectively",SIZE_MIN,SIZE,SIZE_MIN,SIZE);
		exit(1);
	}
	if(argv[3][0]==argv[4][0]){
		printf("Use different symbols.\n");
		exit(1);
	}

	/* MAIN VARIABLES  */
	//first I declare a pointer and a double pointer, to simulate a 2D array with systemV shared memory
	char *board_data;
	char **board;
	//dimensions is a structure made of 2 ints, each defining 2 dimensions of the 2D array
	struct dimensions *dims;
	//declare semaphore and player structures
	struct sembuf sem1, sem2;
	struct player *p1, *p2;
	


	//setup signal
	signal(SIGINT, sig_handler);
	signal(SIGUSR1, disconnect_handler);

	/* SHARED MEMORY */
	
	// CREATE FTOK KEYS to avoid collisions
	key_t key_dim = ftok("/home/elsire23/syscall/file.h",0);
	key_t key_board = ftok("/home/elsire23/syscall/file.h",1);
	key_t key_p1 = ftok("/home/elsire23/syscall/file.h",2);
	key_t key_p2 = ftok("/home/elsire23/syscall/file.h",3);
	key_t key_sem1 = ftok("/home/elsire23/syscall/file.h",4);
	key_t key_sem2 = ftok("/home/elsire23/syscall/file.h",5);


	//1. DELETE EXISTING MEMORU
        //the shmget call would fail and return -1, since the O_CREAT flag isn't used
        //I will call shmget to delete any, if existing, segment of memory. The call will return
        // ENOENT if the shm does not exist

	//2. CREATING SHARED MEMORY
	// every segment of memory is created with 0740 privileges. Keys are generated with ftok(),
	// while the IPC_EXCL flag is used with IPC_CREAT to throw an error if the key is already associated
	// with an existing shared memory 
	
	
	//1 Dims -> also contains server PID
	shmdim = shmget(key_dim, sizeof(struct dimensions),0740);
        if(shmdim != -1){
                shmctl(shmdim, IPC_RMID, NULL);
        }
	//2 Dims
	shmdim = shmget(key_dim, sizeof(struct dimensions), IPC_CREAT | IPC_EXCL | 0740);
        if(shmdim == -1){
                perror("Shmget failed. Couldn't generate dimensions memory"); //str: messaggio-di-errore \n
                exit(1);
        }else{
                printf("shmget: shmget returned %d\n", shmdim);
        }

	//setting dimensions
        dims = (struct dimensions*) shmat(shmdim, NULL, 0);
	dims->rows = atoi(argv[1]);
	dims->cols = atoi(argv[2]);

	//store PID of the server
	dims->pid=getpid();

	//1 Board  -->> in every shmget used to access the shared memory segments to check if they exist, 
	// I used the same size as in those used to create the shm for consistency.
	// But in all the calls I could use 0, since the only requirement in size to access the sh memory
	// is to have a lower size than the one of the already created segment (or it will fail with EINVAL errori).
	// As for the board both 0 and MIN * MIN * sizeof board_data[0] would work

        shmid = shmget(key_board, 0, 0740);
        if(shmid != -1){
                shmctl(shmid, IPC_RMID, NULL);
        }
		
	//2 Board
	shmid = shmget(key_board, dims->rows * dims->cols * sizeof board_data[0], IPC_CREAT | IPC_EXCL | 0740);
 	if(shmid == -1){
 		perror("Shmget failed. Couldn't generate board memory.");
 		exit(1); //exit with error
 	}else{
 		printf("shmget: shmget returned %d\n", shmid);
 	}
	board_data = (char*)shmat(shmid, NULL, 0);


	//dynamic allocation
	board = malloc(dims->rows * sizeof board[0]);
	int x;
	//then just distribute that pre-allocated second-level memory between the rows
	for(x = 0; x < dims->rows; x++)
	{
	    board[x] = board_data + x * dims->rows;    // board[] = c 
	}
	
	//initialize the board with values
	initBoard(board,dims);
	//players shared memory
	//
	//player 1 memory: every player has a name chosen and shared by the client, the associated Process ID of the client, and a symbol chosen and shared by the server
	//1
	shmp1 = shmget(key_p1, sizeof(struct player), 0740);
        if(shmp1 != -1){
                shmctl(shmp1, IPC_RMID, NULL);
        }
	//2
	shmp1 = shmget(key_p1, sizeof(struct player), IPC_CREAT | IPC_EXCL | 0740);
        if(shmp1 == -1){
                perror("Shmget failed. Couldn't generate player memory.");
                exit(1);
        }else{
		printf("shmget: shmget returned %d\n", shmp1);
	}
        p1 = (struct player*) shmat(shmp1, NULL, 0);
	p1->symb=argv[3][0];
	//initialize
	p1->pid=0;
	p1->win=0;
	p1->score=0;


	//player 2 memory: every player has a name chosen and shared by the client, the associated Process ID of the client, and a symbol chosen and shared by the server
        //1
	shmp2 = shmget(key_p2, sizeof(struct player), 0740);
        if(shmp2 != -1){
                shmctl(shmp2, IPC_RMID, NULL);
        }
	//2
	shmp2 = shmget(key_p2, sizeof(struct player), IPC_CREAT | IPC_EXCL | 0740);
        if(shmp1 == -1){
                perror("Shmget failed. Couldn't generate player memory");
                exit(1);
	}else{
		printf("shmget: shmget returned %d\n", shmp2);
	}
        p2 = (struct player*) shmat(shmp2, NULL, 0);
	p2->symb=argv[4][0];
	//initialize
	p2->pid=0;
	p2->win=0;
	p2->score=0;
	
	
	//1      semaphores
	shmsem1 = semget(key_sem1, 1, 0740);
        if(shmsem1 != -1){
                semctl(shmsem1, 0, IPC_RMID);
        }
	//2
	shmsem1 = semget(key_sem1, 1, IPC_CREAT | IPC_EXCL | 0740);
	if(shmsem1 == -1){
		perror("semget failed. Couldn't create semaphore");
		exit(1);
	}else{
		printf("semget: semget returned %d\n",shmsem1);
	}
	//1	
	shmsem2 = semget(key_sem2, 2, 0740);
        if(shmsem2 != -1){
                semctl(shmsem2, 0, IPC_RMID);
        }
	//2
	shmsem2 = semget(key_sem2, 2, IPC_CREAT | IPC_EXCL | 0740);
        if(shmsem2 == -1){
                perror("semget failed. Couldn't create semaphore");
                exit(1);
        }else{
                printf("semget: semget returned %d\n",shmsem2);
        }

	system("clear");	
		
	//initialize semaphores
	//set semaphore value
	semctl(shmsem1, 0, SETVAL, (int)1);

	//specify operation to be done (0, cause server waits for client)
	sem1.sem_op=0; 
	//both sets of semaphores have only one semaphore. The semaphores in the set start at 0, so I can initialize both sem_num at 0, the number of semaphore to be used
	sem1.sem_num=0;
	//no flags set
	sem1.sem_flg=0;

	// temporary variable used in every do while and error value
	int rval,tmp;
	//setvbuf (stdout, NULL, _IONBF, 0);
	do{	
		printf("----Waiting for the first player----\n");
		//printf outside of handler cause is non-async-signal-safe 
		tmp = state;
		//store return value of semop and handle errors
		rval = semop(shmsem1,&sem1, 1);
		if(rval == -1){
			//perror("error");

		}	
	//exit from the cycle if ctrl_c was pressed 2 times
	}while(state != tmp && p1->name[0] == '\0');
	
	printf("\nPlayer joined.\nPlayer 1 name: %s", p1->name);
	//make pid global for signals
	pla1=p1->pid;

	//set again semaphore value
	if(!p1->solo){
		sem1.sem_op=1;
		semop(shmsem1,&sem1,1);
		sem1.sem_op=0;
	
		sem2.sem_op=1;
		sem2.sem_num=0;
		sem2.sem_flg=0;
	
		do{
			printf("\n\n----Waiting for the second player----");
			fflush(stdout);
			tmp = state;
			rval = semop(shmsem1, &sem1, 1);
			if(rval == -1){
				perror("error");
				
	
			}
			//let the first client run again
		}while(state != tmp && p2->name[0] == '\0');	
		semop(shmsem2,&sem2,1);
		printf("\nPlayer 2 joined.\nPlayer 2 name: %s", p2->name);	
	}
	pla2=p2->pid;
	//exit and stated variable
	int exit=0;
	int cpuCol = 4; //starting column of cpu, can be initialized as rand
	printf("\n\nGame starts.");
	int counter = 0;
	//while the process exist
	while(1){
		while(!exit){
			sem1.sem_op=-1;
			sem1.sem_num=0;			
			
			printf("\n\nWait for move...");	
			do{
				//block server
				tmp=state;
				semop(shmsem1,&sem1,1);
			}while(state != tmp);
				
			system("clear");			
				
			//true if it's a standard player game when counter is even OR if it's a single player game
			if ((( (counter % 2) && !p1->solo ) || p1->solo )) //player 1 did the move
				printf("\n%s did the move. Checking win...",p1->name);
			else
				printf("\n%s did the move. Checking win...",p2->name);
			//increase moves counter
			counter++;
					
			//check wins
			if(checkWin(board, dims) == p1->symb){
				printf("%s won.",p1->name);
				p1->win=1;
				p1->score++;
				kill(p1->pid, SIGUSR2);
				//doesn't send lose message to p2 if solo game
				if(!p1->solo)
					kill(p2->pid, SIGUSR2);
				exit=1;
			}else if(checkWin(board, dims) == p2->symb){
				printf("%s won.",p2->name);
				p2->win=1;
				p2->score++;
				kill(p1->pid,SIGUSR2);
				//doesnt send win message to p2 if solo game 
				if(!p1->solo)
					kill(p2->pid, SIGUSR2);
				exit=1;
			}else if(isFull(dims, counter)){
				printf("\nTable full, No one won!");
 
				p1->win == 1;
				p2->win == 1;
				kill(p1->pid, SIGUSR2);
				if(!p1->solo)
					kill(p2->pid, SIGUSR2);
				exit=1;
			}else{
				printf("\nNo winners yet.");
			}
							
			//the server makes the move if it's a solo game
			if(p1->solo){
				//cpu makes a move
				//takes last cpu column as input to find combinations more easily
				cpuCol = cpuMove(board,dims,p2,cpuCol);
				//checks if cpu won
				if(checkWin(board,dims) == p2->symb){
					printf("\nCPU won.");
					p2->win=1;
					p2->score++;
					kill(p1->pid,SIGUSR2);
					exit=1;
				}
				if(isFull(dims, counter)){
					printf("\nTable full, No one won!");
					p1->win=1;
					p2->win=1;
					kill(p1->pid, SIGUSR2);
					exit=1;
				}
			}
			//let the waiting client start
			//unlock cli : the first to unlock is the second one
			//with solo don't unlock sem 2 but only 1
			sem2.sem_op=1;
			if(!p1->solo)
				sem2.sem_num= counter % 2;
			else
				sem2.sem_num= 0;	
			semop(shmsem2,&sem2,1);
		
		}

		//block the server: if 2 clients want to restart, restart
		sem1.sem_num=0;
		sem1.sem_op=-2;
		semop(shmsem1,&sem1,1);	
		
		//when server restarts, init board again
		initBoard(board, dims);
		p1->win=0;
		p2->win=0;
		exit=0;
		cpuCol=4;
		counter=0;

		//restart client 1
		sem2.sem_num=0;
		sem2.sem_op=1;
		semop(shmsem2,&sem2,1);
		//restart client 2, it will be blocked
		sem2.sem_num=1;
		sem2.sem_op=1;
		semop(shmsem2,&sem2,1);


	}
	shmctl(shmid, IPC_RMID, NULL);
	shmctl(shmdim, IPC_RMID, NULL);
	shmctl(shmp1, IPC_RMID, NULL);
	shmctl(shmp2, IPC_RMID, NULL);
	semctl(shmsem1, 0, IPC_RMID);
        semctl(shmsem2, 0, IPC_RMID);

	return 0;
}











