/*
*VR447002
*Karol Sebastian Pasieczny
*9/01/2021
*Forza 4
*/

#include "file.h"

int win = 0;
int finished = 0; //used to distinguish if player left during game or after
pid_t serverpid;
int shmid, shmdim, shmp1, shmp2, shmsem1, shmsem2;
static const char CPU[] = "cpu";
//implementation used by visual studio of a linux getch
char getch(void)

{
    int buf = 0;
    struct termios old = {0};
    fflush(stdout);
    if(tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if(tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    if(read(0, &buf, 1) < 0)
        perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if(tcsetattr(0, TCSADRAIN, &old) < 0)
        perror("tcsetattr ~ICANON");
    return buf;
}

//SIGINT
//both handlers only set the flags (set as global variables)
void sig_handler(int sig){
	//notifies server of disconnecting
	kill(serverpid, SIGUSR1);
        //default CTRL_C interrupt
	signal(SIGINT, SIG_DFL);
	kill(getpid(), SIGINT);
}

//SIGUSR1
void disconnect_handler(int sig){
	if(finished)
		printf("\nThe other player exit. Exiting game..");
	else
		printf("\nThe other player left. You won");
	fflush(stdout);

	signal(SIGINT, SIG_DFL);
	kill(getpid(), SIGINT);
}
//SIGUSR2
void win_handler(int sig){
	win = 1;	
}


void print_board(char **board, struct dimensions *dims, int selectedCol){
	int i, j;
	for (i=dims->rows-1; i>=0; i--){
		printf("\nI");
		for(j=0; j<dims->cols; j++)
			if(j==selectedCol){
				if(board[dims->rows-1][j] == ' ')
					//highlight column
					printf(ANSI_COLOR_GREEN "I %c I" ANSI_COLOR_RESET,board[i][j]);
				else
					//red highlight cause col is full
					printf(ANSI_COLOR_RED "I %c I" ANSI_COLOR_RESET,board[i][j]);
			}else{		
				printf("I %c I",board[i][j]);
			}
		printf("I\n");
		
	}
	printf("-");
	for(i=0; i<dims->cols; i++)
		printf("-----");
	printf("-\n\n");
}

int action(char **board, struct dimensions *dims, int column, struct player *p)
{
	int i;
	for(i=0; i<dims->rows; i++){
		if(board[i][column] == ' '){
			board[i][column] = p->symb;
			return 1;
		}
	}
	return 0;
}

void clean_stdin(void)
{
    int c;
    do {
        c = getchar();
    } while (c != '\n' && c != EOF);
}

int main(int argc, char *argv[], char *env[]){
	
	if(argc<2){
		printf("Access through ./client name\n\n");
		exit(1);
	}
	//single player mode
	int solo = 0;
	if(argv[2] != NULL)
		if(strcmp(argv[2], CPU) == 0){
			solo = 1;
			printf("solo");
	}
	
	struct player *p1, *p2;
	struct dimensions *dims;
	char **board;
	char *board_data;

	struct sembuf sem1,sem2;
	
	signal(SIGINT, sig_handler);
	signal(SIGUSR1, disconnect_handler);
	signal(SIGUSR2, win_handler);
	
	//CREATE FTOK KEYS: keys are also generated in the server, but having ftok(path/file,id) with same path and id will generate the same key
        key_t key_dim = ftok("/home/elsire23/syscall/file.h",0);
        key_t key_board = ftok("/home/elsire23/syscall/file.h",1);
        key_t key_p1 = ftok("/home/elsire23/syscall/file.h",2);
        key_t key_p2 = ftok("/home/elsire23/syscall/file.h",3);
        key_t key_sem1 = ftok("/home/elsire23/syscall/file.h",4);
        key_t key_sem2 = ftok("/home/elsire23/syscall/file.h",5);
	
	
	//Semaphores memory
	shmsem1 = semget(key_sem1, 1, 0740);
        if(shmsem1 == -1){
                perror("semget failed. Couldn't get semaphore memory id");
                exit(1);
        }else{
                printf("semget: semget returned %d\n",shmsem1);
        }

        shmsem2 = semget(key_sem2, 2, 0740);
        if(shmsem2 == -1){
                perror("semget failed. Couldn't get semaphore memory id");
                exit(1);
        }else{
                printf("semget: semget returned %d\n",shmsem2);
        }


	//dimensions memory
        shmdim = shmget(key_dim, sizeof(struct dimensions), 0740);
        if(shmdim == -1){
                perror("Shmget failed. Couldn't access dimensions memory");
                exit(1);
        }else{
                printf("shmget: shmget returned %d\n", shmdim);
        }
        dims = (struct dimensions*) shmat(shmdim, NULL, 0);
	//store pid in global variable for signals
	serverpid=dims->pid;
	//shmget to obtain associated ids
	shmid = shmget(key_board, dims->rows * dims->cols * sizeof board_data[0], IPC_EXCL | 0740);
        if(shmid == -1){
                perror("Shmget failed. Couldn't access board memory");
                exit(1); //exit with error
        }else{
                printf("shmget: shmget returned %d\n", shmid);
        }
        board_data = (char *)shmat(shmid, NULL, 0);

        board = malloc(dims->cols * sizeof board[0]);
        int x;
        for(x = 0; x < dims->cols; x++)
        {
            board[x] = board_data + x * dims->rows;
        }

        //USERNAMES SHARED MEMORY
        //
        //Player 1 memory
        shmp1 = shmget(key_p1, sizeof(struct player), IPC_EXCL | 0740);
        if(shmp1 == -1){
                perror("Shmget failed. Couldn't get player memory id");
                exit(1);
        }else{
                printf("shmget: shmget returned %d\n", shmp1);
        }
        p1 = (struct player*) shmat(shmp1, NULL, 0);
	
	if(solo)
		p1->solo = 1;
	else
		p1->solo = 0;
   	//Player 2 memory
        shmp2 = shmget(key_p2, sizeof(struct player), IPC_EXCL | 0740);
        if(shmp1 == -1){
                perror("Shmget failed. Couldn't get player memory id");
                exit(1);
        }else{
                printf("shmget: shmget returned %d\n", shmp2);
        }
        p2 = (struct player*) shmat(shmp2, NULL, 0);
	
	
	//block if 2 players are in the game already
	
	if(p1->pid != getpid() && p2->pid != getpid() && p1->pid != 0 && p2->pid != 0){
                printf("\n 2 Players are playing the game already!\n");
                exit(0); //upon exit, all the shared memory is deatched
        }
	
	//prepare operation
	sem1.sem_num=0;
        sem1.sem_op=-1;
        sem1.sem_flg=0;
	int err;

	if(!solo){
		sem2.sem_num=0;
		sem2.sem_op=-1;
		sem2.sem_flg=0;
	}
	system("clear");
	
	//first process entering (can also use fork)
	if(p1->pid == 0){
       	        p1->pid = getpid();
		strcpy(p1->name,argv[1]);
                err =semop(shmsem1,&sem1,1); //unlocks semaphore in the server
	        if(err == -1){
                        printf("Error unlocking the semaphore");
			exit(1);
                }
		printf("Hello %s, you're waiting for your opponent to connect.\nYour symbol is %c",p1->name,p1->symb);
		err =semop(shmsem2,&sem2,1);
	}
	
	//check if pid exists and if it's the second connection (getpid() is now different from the first getpid())
	if(p2->pid == 0 && p1->pid != getpid() && !solo){
		p2->pid = getpid();
		strcpy(p2->name,argv[1]);
		err =semop(shmsem1,&sem1,1); //unlocks semaphore in the server
                if(err == -1){
                        printf("Error unlocking the semaphore");
		exit(1);
                }
	}

	/*
	// 2 players max if it's a standard game
	if(p1->pid != getpid() && p2->pid != getpid() && p1->pid != 0 && p2->pid != 0){
		printf("\n 2 Players are playing the game already.\n");
		exit(0); //upon exit, all the shared memory is deatched
	}
	*/	
	// 1 player max for single player game
	if(solo && p1->pid != getpid() && p1->pid != 0){
			printf("\nGame already used.\n");
			exit(0);
	}


	//wait for the other client
	//the pressed key caught by the getch
	
	char key;
	//variable I  used to set a specific column a color to 'highlight' it.
	//it can go from 0 up to dims->cols -1 (since arrays in C start from 0) and it is passed as a parameter to the board printing function print_board
	int highlightIndex = 0;
	int i=0;


	//get server pid with the IPC_STAT flag with shmctl	
	int did;

	//check if first iteration
	int entry=1;	
	char another=' ';

	
	did=0;
	while(1){	
		while(!win){

			system("clear");
			printf("Waiting for the other player to play.. \n\n\n\n\n");
			print_board(board,dims,SIZE); //this way print board doesn't show any color
				
			//block if this process is player 1 and if this is not the first move
			if(p1->pid==getpid() && entry==0 && !win){
				sem2.sem_num=0;
				sem2.sem_op=-1;
				semop(shmsem2,&sem2,1);			
			}
			//block if this process is player 2. Here !win is to make 
			if (p2->pid==getpid() && !solo && !win){
				sem2.sem_num=1;
				sem2.sem_op=-1;
				semop(shmsem2,&sem2,1);
			}
				
				
			//locks client semaphores. && !win is to stop client waiting for move from entering
			while(did == 0 && !win){
				system("clear");
				if(p1->pid==getpid())
					printf("Your symbol is: %c",p1->symb);
				if(p2->pid==getpid())
					printf("Your symbol is %c",p2->symb);
				
				printf("\n\na) Move left  <- \nb) Move right -> \n\nPress spacebar to select your choice.\n");
				print_board(board,dims,highlightIndex);
					
				//flush buffer
				tcflush(0,TCIFLUSH);
				//get value from keyboard without prompting
				key=getch();
				switch(key){
					
					case 'a': 
						if(highlightIndex>0)
							highlightIndex--;	
						break;
					case 'd' :
						if(highlightIndex<dims->cols -1)
							highlightIndex++;
						break;
					case ' ' :
						//check if column has space
						if(board[dims->rows-1][highlightIndex] == ' ')
							if(p1->pid == getpid())
								//put coin in the highlighted column	
								action(board, dims, highlightIndex,p1);
							else
								action(board, dims, highlightIndex,p2);			
						else
							continue;
						//exit from cycle
						did=1;
						entry=0;
						//unlock server
						sem1.sem_op=1;
						sem1.sem_num=0;
						semop(shmsem1, &sem1, 1);
						break;
					default : 
						//flush data
						tcflush(0,TCIFLUSH);
						break;
				}
			
			}
			did=0;
		}
		
		system("clear");	
		print_board(board, dims, SIZE);
		//print win
		if(p1->pid == getpid())		
			if(p1->win)
				printf("You won.\n\n");
			else if(p1->win==p2->win && p1->win==1)
				printf("Board full. No one won, draw\n\n");
			else
				printf("You lost\n\n");
		else
			if(p2->win)
				printf("You won.\n\n");
			else if(p1->win==p2->win && p1->win==1)
				printf("Board full. No one won, draw.\n\n");
			else
				printf("You lost\n\n");
		//PLAY ANOTHER GAME
		//asks both players if play another game
		//
		another = ' ';
		finished = 1;
		if(p1->pid==getpid())
			printf("\nYour score: %d\n%s score:%d",p1->score,p2->name,p2->score);
		if(p2->pid==getpid())
                        printf("\nYour score: %d\n%s score:%d",p2->score,p1->name,p1->score);

		while(another != 'y' && another != 'n'){
			printf("\nPlay another game? y for yes, n for no:");
	                scanf(" %c",&another);
			tcflush(0,TCIFLUSH);
	
			if(p1->pid==getpid() && another == 'y'){
				if(solo){
					//if solo increment semval by two
					sem1.sem_op=2;
					semop(shmsem1,&sem1,1);
				}else{
					//if not, increment by 1 in both p1 and p2
					sem1.sem_op=1;
					semop(shmsem1,&sem1,1);
				}
				//then block cli
				sem2.sem_num=0;
				sem2.sem_op=-1;
				semop(shmsem2,&sem2,1);
				//set variables
				entry=1;
				win=0;
				finished=0;
			}else if (p2->pid==getpid() && another == 'y'){
				//+1 server
				sem1.sem_op=1;
				semop(shmsem1,&sem1,1);
				//block client
				sem2.sem_num=1;
				sem2.sem_op=-1;
				semop(shmsem2,&sem2,1);
				entry=1;
				win=0;
				finished=0;
			}	
			else if (another == 'n'){
				//kills this process in costintent way
				printf("\nTerminating program."); 
				kill(getpid(), SIGINT);
			}else
				printf("\nPlease, reply with yes or no.");
				
			
		}
	}
	return 0;
}

