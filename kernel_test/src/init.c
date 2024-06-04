#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
pid_t getpid(void);

int main(int argc,char **argv){
    int my_pid=getpid();
    int my_ppid=getppid();
    printf("Hello world\n");
    printf("my pid: %d\n",my_pid);
    if(my_pid==1){
        printf("Yes! i am init\n");
    }else{
        printf("No!  i am other\n");
    }
	printf("my ppid: %d\n",my_ppid);
    printf("loop...\n");
    while(1){
        char buf[24];
        printf("input test: \n");
        scanf("%s",buf);
        printf("%s",buf);
    };
    return 0;
}