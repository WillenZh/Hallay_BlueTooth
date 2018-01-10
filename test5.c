#include <string.h>
#include <stdio.h>

int main(){
  char *ss = "[connected][ip@100.100.100.99][serve@100.100.100.111]";
  char *p=NULL;
  char *dd=NULL;
  
  char *s = strdup(ss); 
  p = strsep(&s, "]");
  while(p){
    printf("%s\n", ++p);
    dd = strtok(p, "@");
    while(dd){
      printf("----%s\n", dd);
      dd = strtok(NULL, "@");
    }
    p = strsep(&s, "]");
  }
  return 0;
}

