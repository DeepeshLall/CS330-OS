#include <stdio.h> 
#include <dirent.h> 
#include <string.h> 
#include<unistd.h> 
#include<stdlib.h> 
#include <sys/types.h> 
#include <sys/stat.h>
#include <fcntl.h> 

void match_pattern(char *argv[],char *arr) 
{ 
    int fd,r,j=0; 
    char temp,line[1000]; 
    if((fd=open(arr,O_RDONLY)) != -1) 
    { 
        while((r=read(fd,&temp,sizeof(char)))!= 0) 
        { 
            if(temp!='\n') 
            { 
                line[j]=temp; 
                j++; 
            } 
            else 
            { 
                line[j]='\0';
                if(strstr(line,argv[1])!=NULL) 
                    printf("%s:%s\n",arr,line); 
                // memset(line,0,sizeof(line)); 
                j=0; 
            } 
        }
    }
    return;
} 

void route(char *argv[],char * arr){
    struct dirent *de;

    DIR *dr = opendir(arr);

    if (dr == NULL)
    { 
        struct stat sb;
        stat(arr,&sb);
        if(S_ISREG(sb.st_mode)){
            match_pattern(argv,arr);
        }
        return;
    } 

    char path[1000],temp[1000];
    strcpy(temp,arr);

    while ((de = readdir(dr)) != NULL) {
        if(*de->d_name == '.'){
            continue;
        }
        strcpy(path,temp);
        strcat(path,"/");
        strcat(path,de->d_name);
        route(argv,path);
    } 
  
    closedir(dr); 
    
}

main(int argc,char *argv[]) 
{ 
    struct dirent *de;  
   
    DIR *dr = opendir(argv[2]); 

    char path[1000],temp[1000];
    strcpy(temp,argv[2]);
  
    while ((de = readdir(dr)) != NULL) {
        if(*de->d_name == '.'){
            continue;
        }
        strcpy(path,temp);
        strcat(path,"/");
        strcat(path,de->d_name);
        route(argv,path);
    } 
  
    closedir(dr); 
} 