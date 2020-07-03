#include<pipe.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
/***********************************************************************
 * Use this function to allocate pipe info && Don't Modify below function
 ***********************************************************************/
struct pipe_info* alloc_pipe_info()
{
    struct pipe_info *pipe = (struct pipe_info*)os_page_alloc(OS_DS_REG);
    char* buffer = (char*) os_page_alloc(OS_DS_REG);
    pipe ->pipe_buff = buffer;
    return pipe;
}


void free_pipe_info(struct pipe_info *p_info)
{
    if(p_info)
    {
        os_page_free(OS_DS_REG ,p_info->pipe_buff);
        os_page_free(OS_DS_REG ,p_info);
    }
}
/*************************************************************************/
/*************************************************************************/


int pipe_read(struct file *filep, char *buff, u32 count)
{
    /**
    *  TODO:: Implementation of Pipe Read
    *  Read the contect from buff (pipe_info -> pipe_buff) and write to the buff(argument 2);
    *  Validate size of buff, the mode of pipe (pipe_info->mode),etc
    *  Incase of Error return valid Error code 
    */
    if(!filep){//if file is present or not.
        return -EINVAL;
    }
    if(filep->mode!=O_READ){//permission check
        return -EACCES;
    }
    char * pipe_buff = filep->pipe->pipe_buff;
    int last_read_pos = filep->pipe->read_pos;
    int i=last_read_pos;
    int j=0;
    while(j<count){//Implemented Queue
        if(filep->pipe->buffer_offset==4096){//If queue length is more than 4096
            return -EACCES;
        }
        if(i==4096){
            i=0;
        }
        if(!pipe_buff[i]){
            break;
        }
        buff[j]=pipe_buff[i];
        i++;
        j++;
        filep->pipe->buffer_offset--;
    }
    filep->pipe->read_pos=i;
    return j;
}


int pipe_write(struct file *filep, char *buff, u32 count)
{
    /**
    *  TODO:: Implementation of Pipe Read
    *  Write the contect from   the buff(argument 2);  and write to buff(pipe_info -> pipe_buff)
    *  Validate size of buff, the mode of pipe (pipe_info->mode),etc
    *  Incase of Error return valid Error code 
    */

    if(!filep){//If file doesn't exist.
        return -EINVAL;
    }
    if(filep->mode!=O_WRITE){//If mode mismatch
        return -EACCES;
    }
    char * pipe_buff = filep->pipe->pipe_buff;
    int last_write_pos = filep->pipe->write_pos;
    int i=last_write_pos;
    int j=0;
    while(j<count){//Implemented Queue
        if(filep->pipe->buffer_offset == 4096){//Error if queue lenght>4096
            return -EACCES;
        }
        if(i==4096){
            i=0;
        }
        if(!buff[j]){
            break;
        }
        pipe_buff[i]=buff[j];
        i++;
        j++;
        filep->pipe->buffer_offset++;
    }
    filep->pipe->write_pos=i;
    return j;
}

int create_pipe(struct exec_context *current, int *fd)
{
    /**
    *  TODO:: Implementation of Pipe Create
    *  Create file struct by invoking the alloc_file() function, 
    *  Create pipe_info struct by invoking the alloc_pipe_info() function
    *  fill the valid file descriptor in *fd param
    *  Incase of Error return valid Error code 
    */
    struct pipe_info * pipe_ptr = alloc_pipe_info();
    if(!pipe_ptr){
        return -ENOMEM;
    }//If alloc_file_info returned error
    pipe_ptr->read_pos=0;//Initalize pipe
    pipe_ptr->write_pos=0;
    pipe_ptr->is_ropen=1;
    pipe_ptr->is_wopen=1;
    pipe_ptr->buffer_offset=0;
    struct file * filep1 = alloc_file();
    struct file * filep2 = alloc_file();
    if(!filep1){//If alloc_file() returned error.
        return -ENOMEM;
    }
    if(!filep2){
        return -ENOMEM;
    }

    filep1->type=PIPE;//Initialize file ptr
    filep1->mode=O_READ;
    filep1->offp=0;
    filep1->ref_count=1;
    filep1->pipe=pipe_ptr;
    filep2->type=PIPE;
    filep2->mode=O_WRITE;
    filep2->offp=0;
    filep2->ref_count=1;
    filep2->pipe=pipe_ptr;

    filep1->fops->read=pipe_read;
    filep2->fops->read=pipe_read;
    filep1->fops->write=pipe_write;
    filep2->fops->write=pipe_write;
    filep1->fops->close=generic_close;
    filep2->fops->close=generic_close;
    
    int i=0;
    while(current->files[i]){//find open fd
        i++;
    }
    fd[0]=i;
    current->files[fd[0]]=filep1;
    if(i>MAX_OPEN_FILES){//check if fd<32
        return -EOTHERS;
    }
    i=0;
    while(current->files[i]){//find open fd
        i++;
    }
    fd[1]=i;
    if(i>MAX_OPEN_FILES){//check if fd<32
        return -EOTHERS;
    }
    current->files[fd[1]]=filep2;

    return 0;
}

