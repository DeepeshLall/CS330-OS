#include <stdio.h> 
#include <dirent.h> 
#include <string.h> 
#include <unistd.h> 
#include <stdlib.h> 
#include <sys/types.h> 
#include <sys/stat.h>
#include <fcntl.h> 
#include <wait.h>

long int dir_size(char *argv[],char * arr){
    struct dirent *de; 
  
    DIR *dr = opendir(arr); 
  
    if (dr == NULL)
    { 
        struct stat sb;
        stat(arr,&sb);
        if(S_ISREG(sb.st_mode)){
            /*If a regular file return the size of this file. */
            return sb.st_size;
            exit(0);
        }
    } 

    long int size=0;

    char path[1000],temp[1000];
    strcpy(temp,arr);
    
    while ((de = readdir(dr)) != NULL) {
        if(*de->d_name == '.') {
            continue;
        }
        /*if not a regular file fork and call the same function in child process and wait for it to return and pipe the returned
        size output to parent process. */
        strcpy(path,temp);
        strcat(path,"/");
        strcat(path,de->d_name);
        
        int fd[2];
        pipe(fd);

        if(fork() == 0){/*child process and it need to send its return value using pipe to its parent */
            close(fd[0]);
            long int n=0;
            n=dir_size(argv,path);
            write(fd[1],&n,sizeof(n));
            close(fd[1]);
            exit(0);
        }else{/*parent process would wait for child and take its size from pipe and keep doing so for all directory. */
            wait(NULL);
            close(fd[1]);
            long int size_sub=0;
            read(fd[0],&size_sub,sizeof(size_sub));

            size+=size_sub;
            close(fd[0]);
        }
    }
    closedir(dr);
    return size;
}

main(int argc,char *argv[]){

    struct dirent *de;  
  
    DIR *dr = opendir(argv[1]); 
  
    if (dr == NULL)  
    { 
        printf("%s %ld\n",argv[1],dir_size(argv,argv[1]));
        exit(0);
    } 
    
    char path[1000],temp[1000];
    strcpy(temp,argv[1]);
    
    printf("%s %ld\n",argv[1],dir_size(argv,argv[1]));

    while ((de = readdir(dr)) != NULL){ 
        if(*de->d_name == '.') {
            continue;
        }
        strcpy(path,temp);
        strcat(path,"/");
        strcat(path,de->d_name);
        printf("%s %ld\n",de->d_name,dir_size(argv,path)); 
    }
    closedir(dr); 
}