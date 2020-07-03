#include <stdio.h> 
#include <dirent.h> 
#include <string.h> 
#include <unistd.h> 
#include <stdlib.h> 
#include <sys/types.h> 
#include <sys/stat.h>
#include <fcntl.h> 
#include <wait.h>

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

void match_pattern_direct(char *argv[],char *arr) 
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
                    printf("%s\n",line); 
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
        // printf("%s\n",arr);
        struct stat sb;
        stat(arr,&sb);
        if(S_ISREG(sb.st_mode)){
            match_pattern(argv,arr);
        }
        // match_pattern(argv,arr);
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
    char path[1000],temp[1000];
    strcpy(temp,argv[2]);
    
    char buff1[100000];
    char buff2[100000];
    int fd[4];
    pipe(fd);
    pipe(fd+2);

    if(fork()!=0){
        /*Parent Process for executing grep */
        dup2(fd[1],1);
        close(fd[0]);
        close(fd[1]);
        close(fd[2]);
        close(fd[3]);

        struct dirent *de;  
        DIR *dr = opendir(argv[2]); 
        if(dr == NULL) {
            struct stat sb;
            stat(temp,&sb);
            if(S_ISREG(sb.st_mode)){
                match_pattern_direct(argv,temp);
            }
            exit(0);
        }

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
    }else{
        /*Child process for handling output of grep */
        wait(NULL);
        if(fork() != 0){
            /*Parent Process for tee comand */
            dup2(fd[0],0);
            dup2(fd[3],1);
            close(fd[0]);
            close(fd[1]);
            close(fd[2]);
            close(fd[3]);

            char *arr[3];
            arr[0]="tee";
            arr[1]=argv[3];
            // arr[2]=fd[0];
            arr[2]=NULL;
            execvp(arr[0],arr);/*Output to be written in file argv[3]. That is tee comand to be executed.*/
        }else{
            /*Child process for handling the output of tee comand and using argv[4] command execution */
            wait(NULL);
            dup2(fd[2],0);
            close(fd[0]);
            close(fd[1]);
            close(fd[2]);
            close(fd[3]);

            char *arr[2];
            arr[0]=argv[4];
            arr[1]=NULL;
            execvp(argv[4],arr);
        }
    }
} 