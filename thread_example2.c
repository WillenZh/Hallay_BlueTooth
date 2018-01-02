
/**
 * author:  张 炜
 * write by 2017/12/18
 * compile: gcc thread_example2.c -lpthread -o thread_willen2
 */

#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>

#define MAX 10

pthread_t thread[2];
pthread_mutex_t mut;
int number=0, i;

void *thread1(void *arg)
{
        char* name = (char *)arg;
        printf ("thread1 : I'm %s\n", name);

        for (i = 0; i < MAX; i++)
        {

                printf("thread1 : number = %d\n",number);

                pthread_mutex_lock(&mut);
                number++;
                pthread_mutex_unlock(&mut);

                sleep(2);
        }

        printf("thread1 :主函数在等我完成任务吗？\n");
        pthread_exit(NULL);
}

void *thread2(void *arg)
{
        char* name = (char *)arg;
        printf("thread2 : I'm %s\n", name);

        for (i = 0; i < MAX; i++)
        {

                printf("thread2 : number = %d\n",number);
                pthread_mutex_lock(&mut);
                number++;
                pthread_mutex_unlock(&mut);
                sleep(3);
        }
 
        printf("thread2 :主函数在等我完成任务吗？\n");
        pthread_exit(NULL);
}

void thread_create(void)
{
        int temp;
        memset(&thread, 0, sizeof(thread));
        char* thread1_arg = "my thread1";
        char* thread2_arg = "my thread2";

        /*创建线程*/
        if((temp = pthread_create(&thread[0], NULL, thread1, thread1_arg)) != 0)
                printf("线程1创建失败!\n");
        else
                printf("线程1被创建\n");

        if((temp = pthread_create(&thread[1], NULL, thread2, thread2_arg)) != 0)
                printf("线程2创建失败");
        else
                printf("线程2被创建\n");

}

void thread_wait(void)
{

        /*等待线程结束*/
        if(thread[0] !=0) {

                pthread_join(thread[0],NULL);
                printf("线程1已经结束\n");

        }

        if(thread[1] !=0) {

                pthread_join(thread[1],NULL);
                printf("线程2已经结束\n");

        }
}


int main()
{

        /*用默认属性初始化互斥锁*/
        pthread_mutex_init(&mut,NULL);

        printf("我是主函数哦，我正在创建线程，呵呵\n");
        thread_create();
        printf("我是主函数哦，我正在等待线程完成任务阿，呵呵\n");

        thread_wait();
        return 0;
}
