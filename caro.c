#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdarg.h>
#include <termios.h>
#include <signal.h>

#define UP 65
#define DOWN 66
#define LEFT 68
#define RIGHT 67

#define HOST_IP "127.0.0.1"
#define PORT 6996
#define MAX 1024
#define MAP_SIZE 8

#define lock pthread_mutex_lock
#define unlock pthread_mutex_unlock
#define xy(x, y) printf("\033[%d;%dH", x, y)
#define clear_eol(x) print(x, 12, "\033[K")

typedef struct
{
    int sockfd;
} thread_args;

typedef struct
{
    int x;
    int y;
} pos; //vi tri trong ban co

int sv_fd;
int cli_fd;
int game_mode;
int in_turn = 1;

char my_chess_man = 'x';
char op_chess_man = 'y';

pos cursor;
pos this_turn;
bool end_turn = 0;
bool game_event = 0;
int val[MAP_SIZE][MAP_SIZE]; //gia tri cac o trong ban co

pthread_mutex_t val_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t end_turn_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t game_event_mutex = PTHREAD_MUTEX_INITIALIZER;

int counter;
/*

*/

void game_exit()
{
    printf("Close socket...\n");
    if (1 == game_mode)
    {
        //printf("Sever close\n");
        counter=-1;
        end_turn=1;
        sleep(2);
        close(sv_fd);
    }
    else
    {
        counter =-1;
        end_turn=1;
        printf("Quit game\n");
        close(cli_fd);
    }
    system("reset");
    printf("Quit game\n");
    exit(EXIT_SUCCESS);
}
void game_signal_handler(int signo)
{
    game_exit();
}

char getch(void)
{
    char buf = 0;
    struct termios old = {0};
    fflush(stdout);
    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    if (read(0, &buf, 1) < 0)
        perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
        perror("tcsetattr ~ICANON");
    //printf("%c\n", buf);
    return buf;
}

void print(int y, int x, const char *fmt, ...)
{
    static pthread_mutex_t screen = PTHREAD_MUTEX_INITIALIZER;
    va_list ap;
    va_start(ap, fmt);

    lock(&screen);
    xy(y + 1, x + 1), vprintf(fmt, ap);
    fflush(stdout);
    unlock(&screen);
}

static void init_map()
{
    end_turn = 0;
    int i, j;
    for (i = 0; i < 4 * MAP_SIZE; i++)
        for (j = 0; j <= 2 * MAP_SIZE; j += 2)
            print(j, i, "--");
    for (i = 0; i <= 4 * MAP_SIZE; i += 4)
        for (j = 0; j <= 2 * MAP_SIZE; j++)
            print(j, i, "|");

    cursor.x = 18;
    cursor.y = 9;
    print(cursor.y, cursor.x, "");

    lock(&val_mutex);
    for (i = 0; i < MAP_SIZE; i++)
        for (j = 0; j < MAP_SIZE; j++)
            val[i][j] = 0;
    unlock(&val_mutex);

    for (i = 0; i < MAP_SIZE; i++)
        for (j = 0; j < MAP_SIZE; j++)
        {
            print(j * 2 + 1, i * 4 + 2, "");
        }
    print(cursor.y, cursor.x, "");
}

int vertical_check(int x, int y)
{
    int count = 1;
    int a = x + 1;
    while (a < MAP_SIZE && val[x][y] == val[a][y])
    {
        a++;
        count++;
    }
    a = x - 1;
    while (a >= 0 && val[x][y] == val[a][y])
    {
        a--;
        count++;
    }
    return (count == 5);
}

int horizontal_check(int x, int y)
{
    int count = 1;
    int a = y + 1;
    while (a < MAP_SIZE && val[x][y] == val[x][a])
    {
        a++;
        count++;
    }
    a = y - 1;
    while (a >= 0 && val[x][y] == val[x][a])
    {
        a--;
        count++;
    }
    return (count == 5);
}

int diagonal_check(int x, int y)
{
    int count1 = 1, count2 = 1;
    int a = x + 1;
    int b = y + 1;
    while (a < MAP_SIZE && b < MAP_SIZE && val[x][y] == val[a][b])
    {
        a++;
        b++;
        count1++;
    }
    a = x - 1;
    b = y - 1;
    while (a >= 0 && b >= 0 && val[x][y] == val[a][b])
    {
        a--;
        b--;
        count1++;
    }

    a = x + 1;
    b = y - 1;
    while (a < MAP_SIZE && b >= 0 && val[x][y] == val[a][b])
    {
        a++;
        b--;
        count2++;
    }
    a = x - 1;
    b = y + 1;
    while (a >= 0 && b < MAP_SIZE && val[x][y] == val[a][b])
    {
        a--;
        b++;
        count2++;
    }
    return (count1 == 5 || count2 == 5);
}

void check_win(int x, int y)
{
    int win = 0;
    if ((val[x][y] != 0) && (vertical_check(x, y) || horizontal_check(x, y) || diagonal_check(x, y)))
        win = val[x][y];
    else
        return;
    if (win == 1)
    {
        system("clear");
        print(0, 0, "You win");
        sleep(1);
        system("clear");
        init_map();
        counter = 0;
        end_turn = 1;
    }
    else if (win == -1)
    {
        system("clear");
        print(0, 0, "Opponent win");
        sleep(1);
        system("clear");
        init_map();
        counter = 0;
        end_turn = 0;
    }
}

void control()
{
    char input;
    if(end_turn==true) return;
    input = getch();
    if (input == UP)
    {
        if (cursor.y > 2)
        {
            cursor.x = cursor.x;
            cursor.y = cursor.y - 2;
        }
        else
            return;
    }
    else if (input == DOWN)
    {
        if (cursor.y < 2 * MAP_SIZE - 1)
        {
            cursor.x = cursor.x;
            cursor.y = cursor.y + 2;
        }
        else
            return;
    }
    else if (input == LEFT)
    {
        if (cursor.x > 2)
        {
            cursor.x = cursor.x - 4;
            cursor.y = cursor.y;
        }
        else
            return;
    }
    else if (input == RIGHT)
    {
        if (cursor.x < 4 * MAP_SIZE - 2)
        {
            cursor.x = cursor.x + 4;
            cursor.y = cursor.y;
        }
        else
            return;
    }
    print(cursor.y, cursor.x, "");

    int i = (cursor.x - 2) / 4;
    int j = (cursor.y - 1) / 2;
    if (input == my_chess_man)
    {
        if (end_turn == true)
            return;
        if (val[i][j] == 0)
        {
            lock(&val_mutex);
            val[i][j] = 1;
            unlock(&val_mutex);
            counter++;
            this_turn.x = i;
            this_turn.y = j;
            lock(&end_turn_mutex);
            end_turn = true;
            unlock(&end_turn_mutex);
        }
    }
}

static void game_display()
{
    int i, j;
    char s1[2], s2[2];
    s1[0] = my_chess_man;
    s1[1] = '\0';
    s2[0] = op_chess_man;
    s2[1] = '\0';
    while (1)
    {
        if (1 == game_event)
        {
            for (i = 0; i < MAP_SIZE; i++)
                for (j = 0; j < MAP_SIZE; j++)
                {
                    if (val[i][j] == 1)
                        print(j * 2 + 1, i * 4 + 2, s1);
                    if (val[i][j] == 0)
                        print(j * 2 + 1, i * 4 + 2, "");
                    if (val[i][j] == -1)
                        print(j * 2 + 1, i * 4 + 2, s2);
                }
            print(cursor.y, cursor.x, "");
            lock(&game_event_mutex);
            game_event = 0;
            unlock(&game_event_mutex);
            if(1 == end_turn)
            {
                print(2 * MAP_SIZE + 3, 0, "Op   turn: ");
            }
            else
            {
                print(2 * MAP_SIZE + 3, 0, "Your turn: ");
            }
            print(cursor.y, cursor.x, "");
        }
    }
}

/* Truong code*/
static void *socket_read(void *_args)
{
    thread_args *thr_args = (thread_args *)_args;
    char buff[MAX];
    int sockfd = thr_args->sockfd;
    while (1)
    {

        bzero(buff, sizeof(buff));
        read(sockfd, buff, sizeof(buff));
        if (strlen(buff) == 0)
        {
            continue;
        }
        else
        {
            lock(&game_event_mutex);
            game_event = 1;
            unlock(&game_event_mutex);
            sscanf(buff, "Op: %d, %d, %d", &counter, &this_turn.x, &this_turn.y);
            lock(&end_turn_mutex);
            if(counter==-1) 
            {
                system("reset");
                printf("Quit game\n");
                exit(EXIT_SUCCESS);
            } 
            end_turn = false;
            unlock(&end_turn_mutex);
            lock(&val_mutex);
            val[this_turn.x][this_turn.y] = -1;
            unlock(&val_mutex);
            print(2 * MAP_SIZE + 2, 0, "Competitor: ");
            print(2 * MAP_SIZE + 2, 12, buff);
            check_win(this_turn.x, this_turn.y);
        }
    }
}

static void *socket_write(void *_args)
{
    thread_args *thr_args = (thread_args *)_args;
    char buff[MAX];
    int sockfd = thr_args->sockfd;
    while (1)
    {
        control();
        if (1 == end_turn)
        {
            check_win(this_turn.x, this_turn.y);
            fflush(stdout);
            bzero(buff, sizeof(buff));
            sprintf(buff, "Op: %d, %d, %d\n", counter, this_turn.x, this_turn.y);
            send(sockfd, buff, strlen(buff), 0);
            lock(&game_event_mutex);
            game_event = 1;
            unlock(&game_event_mutex);
            
        }
    }
}

static void game_cheese_man_config()
{
    printf("Choose your cheese man, bro\n");
    scanf(" %c", &my_chess_man);
    fflush(stdin);
    printf("Choose your opponent's cheese man, bro\n");
    scanf(" %c", &op_chess_man);
    return;
}

static int game_intro()
{
    int game_mod;
    printf("1. Create game\n");
    printf("2. Join game\n");
    scanf(" %d", &game_mod);
    return game_mod;
}

static int game_create()
{
    int conn_fd;
    int len;
    int i;
    struct sockaddr_in sever_addr, client_addr;
    counter = 0;

    /* socket */
    sv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sv_fd < 0)
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    /* struct */
    bzero(&sever_addr, sizeof(sever_addr));
    sever_addr.sin_family = AF_INET;
    sever_addr.sin_addr.s_addr = INADDR_ANY;
    sever_addr.sin_port = htons(PORT);

    /* blind */
    if (bind(sv_fd, (struct sockaddr *)&sever_addr, sizeof(sever_addr)) < 0)
    {
        perror("Bind error");
        exit(EXIT_FAILURE);
    }

    /* listen */
    if (listen(sv_fd, 3) < 0)
    {
        perror("Listen error");
        exit(EXIT_FAILURE);
    }

    /* accept */
    len = sizeof(client_addr);
    conn_fd = accept(sv_fd, (struct sockaddr *)&client_addr, (socklen_t *)&len);
    if (conn_fd < 0)
    {
        perror("Accept error");
        exit(EXIT_FAILURE);
    }

    /* init game */
    system("clear");
    init_map();

    /* read & send */
    pthread_t threadID[2];
    thread_args thr[2];
    thr[0].sockfd = conn_fd;
    thr[1].sockfd = conn_fd;
    pthread_create(&threadID[0], NULL, socket_read, &thr[0]);
    pthread_create(&threadID[1], NULL, socket_write, &thr[1]);
    for (i = 0; i < 2; i++)
    {
        pthread_join(threadID[i], NULL);
    }

    close(sv_fd);
    return 0;
}

static int game_join()
{
    struct sockaddr_in sever_addr, client_addr;
    pthread_t threadID[2];
    char ip_addr[16];
    int i;
    counter = 0;

    printf("Ip address: \n");
    scanf(" %s", ip_addr);

    /* socket */
    cli_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (cli_fd < 0)
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    /* struct */
    bzero(&sever_addr, sizeof(sever_addr));
    sever_addr.sin_family = AF_INET;
    sever_addr.sin_addr.s_addr = inet_addr(ip_addr);
    sever_addr.sin_port = htons(PORT);

    /* connect */
    if (connect(cli_fd, (struct sockaddr *)&sever_addr, sizeof(sever_addr)) < 0)
    {
        perror("Connect error");
        exit(EXIT_FAILURE);
    }

    /* init game */
    system("clear");
    init_map();

    /* read & send */
    thread_args thr[2];
    thr[0].sockfd = cli_fd;
    thr[1].sockfd = cli_fd;
    pthread_create(&threadID[0], NULL, socket_read, &thr[0]);
    pthread_create(&threadID[1], NULL, socket_write, &thr[1]);
    for (i = 0; i < 2; i++)
    {
        pthread_join(threadID[i], NULL);
    }
    close(cli_fd);
    return 0;
}

static void game_processor()
{
    if (1 == game_mode)
    {
        printf("Waiting for other player ...\n");
        game_create();
    }
    else
    {
        printf("Connect server ...\n");
        game_join();
    }
}

int main()
{
    int i;
    pthread_t threadID[2];
    game_cheese_man_config();
    game_mode = game_intro();
    signal(SIGINT, game_signal_handler);
    signal(SIGKILL, game_signal_handler);
    signal(SIGQUIT, game_signal_handler);
    signal(SIGTSTP, game_signal_handler);

    pthread_create(&threadID[0], NULL, (void *)game_processor, NULL);
    pthread_create(&threadID[1], NULL, (void *)&game_display, NULL);
    for (i = 0; i < 2; i++)
    {
        pthread_join(threadID[i], NULL);
    }
    game_exit();
    return 0;
}