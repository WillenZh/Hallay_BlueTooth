#include <stdio.h>  
#include <stdlib.h>  
  
/**
 * [main 用于连接C200输液警报器]
 * @author 张 炜 2017-12-01 10：48
 */
int main()  
{
    FILE *in;
    FILE *inkernal;

    int flag = 0;
    int flag_kernal = 0;

    char array[1];
    int fd = 0;
    int *buf = NULL;
 
    if ((in = fopen("kernel_file","w+")) == NULL) //app输出文件  
    {
        printf("canot find the test_txt file!\n");
        return;
    }

    if ((inkernal = fopen("test_txt","w+")) == NULL) //kernel输出文件  
    {
        printf("canot find the test_txt file!\n");
        return;
    }

    while(1){
        system("/bin/gatttool -b 02:4F:08:E9:7C:C0 --char-write-req -a 0x000f -n 0100 --listen & > /tmp/02:4F:08:E9:7C:C0");
        sleep(2);

        fscanf(in,"%d",&flag);
        fscanf(inkernal,"%d",&flag_kernal);
        //printf("8880 flag=%d\n", flag);  
        while(flag_kernal == 1){
            flag_kernal = 0;
            printf("99990123 flag_kernal=%d\n", flag_kernal);   
            array[0] = flag_kernal;
            sprintf(&array[0], "%d", flag_kernal);
            sleep(5);
            fscanf(in,"%d",&flag);
            if(flag == 54){
                sleep(20);
                system(insmod /mnt/zxwork/ngb_w_nw3100/HiSTBLinuxV100R005C00SPC041/software/HiSTBLinuxV100R005C00SPC041/out/hi3798cv200/hi3798cv2dmo/kmod/rtk_8723btusb.ko);  
                sleep(1);
                system(rmmod /mnt/zxwork/ngb_w_nw3100/HiSTBLinuxV100R005C00SPC041/software/HiSTBLinuxV100R005C00SPC041/out/hi3798cv200/hi3798cv2dmo/kmod/rtk_8723btusb.ko);  
                break;
            }
        }
        system("kill -9 `ps -w | grep -v grep | grep gatttool59 | awk '{print $1}'`");
        sleep(1);
        //printf("8882 flag=%d\n",flag); 
    }

    free(buf);
    fclose(in);
    fflush(NULL);
    return 0;
}