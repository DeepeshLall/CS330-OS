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
    pipe->pipe_buff = buffer;
    return pipe;
}


void free_pipe_info(struct pipe_info *p_info)
{   
    //printk("inside free_pipe_info\n");
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
   // cyclic implementation
    //printk("inside pipe_read\n");
    if(!filep){
        //printk("filep is NULL\n");
        return -EINVAL;
    }else if(filep->mode!=O_READ){  // mode of pipe should be O_READ
        //printk("mode of pipe should be O_READ but mode is %d\n", filep->mode);
        return -EACCES;
    }
    int i=0;
    while(i<count){
        if(filep->pipe->buffer_offset == 0){
            //printk("nothing to read in pipe, empty buffer\n");
            break; // memory empty, nothing to read
        }else{
            if(filep->pipe->read_pos == 4096){ // exceed => cyclicity introduced
                //printk("index exceeding 4096\n");
                filep->pipe->read_pos = 0;
            }
            //common updation in both cases
            buff[i] = filep->pipe->pipe_buff[filep->pipe->read_pos];
            i++;
            filep->pipe->buffer_offset--;
            filep->pipe->read_pos++;
        }
    }
    return i; // for checking
}


int pipe_write(struct file *filep, char *buff, u32 count)
{
    /**
    *  TODO:: Implementation of Pipe Read
    *  Write the content from   the buff(argument 2);  and write to buff(pipe_info -> pipe_buff)
    *  Validate size of buff, the mode of pipe (pipe_info->mode),etc
    *  Incase of Error return valid Error code 
    */
   // cyclic implementation
   //printk("inside pipe_write\n");
    //  Writing more than 4KB should return a error. 
    if(!filep){
        //printk("filep does not exist\n");
        return -EINVAL;
    }else if(filep->mode!=O_WRITE){  // mode of pipe should be O_WRITE
        //printk("mode of pipe should be O_WRITE but mode is %d\n", filep->mode);
        return -EACCES;
    }
    int i=0;
    while(i<count){
        if(buff[i]=='\0'){ // buff exhausted
            //printk("buff exhausted\n");
            break;
        }
        if(filep->pipe->buffer_offset == 4096){
            return -EOTHERS; // memory full
        }else{
            if(filep->pipe->write_pos == 4096){ // exceed => cyclicity introduced
                filep->pipe->write_pos = 0;
            }
            //common updation in both cases
            filep->pipe->pipe_buff[filep->pipe->write_pos] = buff[i];
            i++;
            filep->pipe->buffer_offset++;
            filep->pipe->write_pos++;
        }
    }
   return i; // for checking
}
// one pipe => one file structure and one pipe_ifo structure
int create_pipe(struct exec_context *current, int *fd)
{
    /**
    *  TODO:: Implementation of Pipe Create
    *  Create file struct by invoking the alloc_file() function, 
    *  Create pipe_info struct by invoking the alloc_pipe_info() function
    *  fill the valid file descriptor in *fd param
    *  Incase of Error return valid Error code 
    */

    // s1 : find free fd
    int i=3, fd_read, fd_write; // looking from index 3  
    while(current->files[i]){
      i++;
    }
    fd_read=i;
    i++;
    while(current->files[i]){
        i++;
    }
    fd_write=i;
    if(fd_read > 32 || fd_write > 32){
        //printk("all fds are occupied\n");
        return -EOTHERS;
    }
    //TODO maximum can be 32 only
    // pipe structure
    struct pipe_info *pipe1 = alloc_pipe_info();
    if(!pipe1){
        return -ENOMEM;
    }
    // s2 : allocate file object to read and write end of pipe
    // Read file object
    // assuming no error
    // s3: allocating a file object 
    struct file * filep_read = alloc_file();
    if(!filep_read){
        return -ENOMEM;
    }
     // Read file object
    // assuming no error
    // s3: allocating a file object 
    struct file * filep_write = alloc_file();
    if(!filep_write){
        return -ENOMEM;
    }
    pipe1->is_ropen=1;
    pipe1->is_wopen=1;
    pipe1->read_pos=0;
    pipe1->write_pos=0;
    pipe1->buffer_offset=0;

    
    // s4: filling the fields
    filep_read->inode = NULL;
    filep_read->mode = O_READ; // assigning mode of opening 
    filep_read->type = PIPE;
    filep_read->ref_count=1; // needed in the implementation when deleting fds
    filep_read->offp = 0;
    filep_read->fops->read = pipe_read;
    filep_read->fops->write = pipe_write;
    filep_read->fops->close = generic_close;
    filep_read->pipe = pipe1;
    


   
    // s4: filling the fields
    filep_write->inode = NULL;
    filep_write->mode = O_WRITE; // assigning mode of opening
    filep_write->type = PIPE;
    filep_write->ref_count=1; // needed in the implementation when deleting fds
    filep_write->offp = 0;
    filep_write->fops->read = pipe_read;
    filep_write->fops->write = pipe_write;
    filep_write->fops->close = generic_close;
    filep_write->pipe = pipe1;

    // assigning file structures
    current->files[fd_read] = filep_read;
    current->files[fd_write] = filep_write;

    // assigning fds
    fd[0]=fd_read;
    fd[1]=fd_write;
    int ret_fd = 0;  // error unknown
    return 1;
}
