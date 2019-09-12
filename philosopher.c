#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#define N 5

typedef struct argg_t
{
	int id;
	char mode;
}argg;

const char *namesw3[N] = { "Alice ", "Bob ", "Carol ", "Dan ", "Eve " };
pthread_mutex_t forks[N];
 
#define lock pthread_mutex_lock
#define unlock pthread_mutex_unlock
#define xy(x, y) printf("\033[%d;%dH", x, y)
#define clear_eol(x) print(x, 15, "\033[K")

void print(int y, int x, const char *fmt, ...)
{
	static pthread_mutex_t screen = PTHREAD_MUTEX_INITIALIZER;
	va_list ap;
	va_start(ap, fmt);
 
	lock(&screen);
	xy(y+1, x), vprintf(fmt, ap);
	xy(N + 1, 1), fflush(stdout);
	unlock(&screen);
}
 
void phil_eat(int id,char mode)
{
	int f[2], ration, i; /* forks */
	if(mode=='n')
 		f[0]=id%N;f[1]=(id+1)%N;
 	if(mode=='y')
 		{
 			f[0] = f[1] = id;
			f[id & 1] = (id + 1) % N;
		}
	//else return;
	clear_eol(id);
	print(id, 12, "<Hungry>");
 	//folk 1
 	sleep(1+rand()%8);
	lock(forks + f[0]);
	clear_eol(id);
 	print(id, 12, "pick fork%d, need fork%d", f[0],f[1]);
	//folk2
	sleep(1+rand()%8);
	lock(forks + f[1]);
	clear_eol(id);
 	print(id, 12, "pick fork%d,fork%d", f[0],f[1]);
	//eating
 	print(id,30,"Eating");
	for (i = 0, ration = 2 + rand() % 4; i < ration; i++)
		print(id, 36 + i, "."), sleep(1);
 
	/* done nomming, give up forks (order doesn't matter) */
	for (i = 0; i < 2; i++) unlock(forks + f[i]);
}
 
void phil_sleep(int id)
{
	int i;
	char buf[64] = {0};

		clear_eol(id);
 		sprintf(buf,"<Sleep>..zZ");

		for (i = 0; buf[i]; i++) {
			print(id, i+12, "%c", buf[i]);
		}
	 	sleep(1 + rand()% 7);
	 	clear_eol(id);
	 	print(id,12,"<Wake up>");
	 	sleep(1);
}

void* philosophize(void *a)
{
	 argg *tmp=(argg*)a;
	 int id = tmp->id;
	 char mode= tmp->mode;
	 print(id, 1, "%11s", namesw3[id]);
	 while(1) 
	 	{
	 		phil_sleep(id);
	 		phil_eat(id,mode);
	 	}
}

int main(int argc , char *argv[])
{	
	int i, id[N];
	pthread_t tid[N];
	argg **arg;
 	srand(time(NULL));
 	arg=(argg**)malloc(N*sizeof(argg *));
 	printf("%c",argv[1][0]);
 	for (i=0;i<N;i++)
 	{
 		arg[i]=(argg*)malloc(sizeof(argg));
 		arg[i]->id=i;
 		arg[i]->mode=argv[1][0];
 	}
	for (i = 0; i < N; i++)
		pthread_mutex_init(forks + (id[i] = i), 0);
	for (i = 0; i < N; i++)
		if(pthread_create(tid + i, 0, philosophize, (void*)arg[i])!=0)
			{
				printf("err");
			}
 
	/* wait forever: the threads don't actually stop */
	return pthread_join(tid[0], 0);
}