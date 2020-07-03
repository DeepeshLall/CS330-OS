#include <stdio.h> 
#include <dirent.h> 
#include <string.h> 
#include <unistd.h> 
#include <stdlib.h> 
#include <sys/types.h> 
#include <sys/stat.h>
#include <fcntl.h> 

int match_pattern(char *argv[],char *arr) 
{ 
    int fd,r,j=0,count=0; 
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
                    count++;
                    // printf("%s:%s\n",arr,line); 
                // memset(line,0,sizeof(line)); 
                j=0; 
            } 
        }
    }
    return count;
} 

int match_pattern_direct(char *argv[],char *arr) 
{ 
    int fd,r,j=0,count=0; 
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
                    count++;
                    // printf("%s\n",line); 
                // memset(line,0,sizeof(line)); 
                j=0; 
            } 
        }
    }
    return count;
} 

int route(char *argv[],char * arr,int count){
    struct dirent *de;

    DIR *dr = opendir(arr);

    if (dr == NULL)
    { 
        // printf("%s\n",arr);
        int count1=0;
        struct stat sb;
        stat(arr,&sb);
        if(S_ISREG(sb.st_mode)){
            count1+=match_pattern(argv,arr);
        }
        // match_pattern(argv,arr);
        return count1;
    } 

    char path[1000],temp[1000];
    strcpy(temp,arr);
    int count2=count;

    while ((de = readdir(dr)) != NULL) {
        if(*de->d_name == '.'){
            continue;
        }
        strcpy(path,temp);
        strcat(path,"/");
        strcat(path,de->d_name);
        count2+=route(argv,path,count);
    } 
  
    closedir(dr); 
    return count2;
}

main(int argc,char *argv[]) 
{ 
    struct dirent *de;  
   
    DIR *dr = opendir(argv[2]); 

    char path[1000],temp[1000];
    strcpy(temp,argv[2]);
    int count=0;

    if(dr == NULL) {
        struct stat sb;
        stat(temp,&sb);
        if(S_ISREG(sb.st_mode)){
            count+=match_pattern_direct(argv,temp);
        }
        printf("%d\n",count);
        exit(0);
    }

    while ((de = readdir(dr)) != NULL) {
        if(*de->d_name == '.'){
            continue;
        }
        strcpy(path,temp);
        strcat(path,"/");
        strcat(path,de->d_name);
        count+=route(argv,path,0);
    } 
    
    printf("%d\n",count);

    closedir(dr); 
} 