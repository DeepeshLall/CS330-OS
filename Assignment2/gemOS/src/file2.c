#include<types.h>
#include<context.h>
#include<file.h>
#include<lib.h>
#include<serial.h>
#include<entry.h>
#include<memory.h>
#include<fs.h>
#include<kbd.h>
#include<pipe.h>


/************************************************************************************/
/***************************Do Not Modify below Functions****************************/
/************************************************************************************/
void free_file_object(struct file *filep)
{
  //printk("inside free_file_object: freeing file struct object\n");
    if(filep)
    {
       os_page_free(OS_DS_REG ,filep);
       stats->file_objects--;
    }
}


struct file *alloc_file()
{
  
  struct file *file = (struct file *) os_page_alloc(OS_DS_REG); 
  file->fops = (struct fileops *) (file + sizeof(struct file)); 
  bzero((char *)file->fops, sizeof(struct fileops));
  stats->file_objects++;
  return file; 
}

static int do_read_kbd(struct file* filep, char * buff, u32 count)
{
  kbd_read(buff);
  return 1;
}

static int do_write_console(struct file* filep, char * buff, u32 count)
{
  struct exec_context *current = get_current_ctx();
  return do_write(current, (u64)buff, (u64)count);
}

struct file *create_standard_IO(int type)
{
  struct file *filep = alloc_file();
  filep->type = type;
  if(type == STDIN)
     filep->mode = O_READ;
  else
      filep->mode = O_WRITE;
  if(type == STDIN){
        filep->fops->read = do_read_kbd;
  }else{
        filep->fops->write = do_write_console;
  }
  filep->fops->close = generic_close;
  filep->ref_count = 1;
  return filep;
}

int open_standard_IO(struct exec_context *ctx, int type)
{
   int fd = type;
   struct file *filep = ctx->files[type];
   if(!filep){
        filep = create_standard_IO(type);
   }else{
         filep->ref_count++;
         fd = 3;
         while(ctx->files[fd])
             fd++; 
   }
   ctx->files[fd] = filep;
   return fd;
}
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/



void do_file_fork(struct exec_context *child)
{
   /*TODO the child fds are a copy of the parent. Adjust the refcount*/
   //printk("inside do_file_fork\n");
     for(int i=0; i<MAX_OPEN_FILES; i++){  // ERROR in case of 0, 1, 2????
       if(child->files[i]){ 
          //printk("file des %d is open, with ref_count: %d, increasing it by 1\n", i, child->files[i]->ref_count);
          child->files[i]->ref_count++;
       }
     }
}


long generic_close(struct file *filep)
{
  /** TODO Implementation of close (pipe, file) based on the type 
   * Adjust the ref_count, free file object
   * Incase of Error return valid Error code 
   * 
   */
  if(!filep){
    //printk("pointer does not exist\n");
    return -EINVAL;
  }
  //printk("inside generic_close\n");
  int l;
    if(filep->type == REGULAR){
      //printk("regular file type\n");
      // regular file
      if(filep->ref_count==1){ //this is the only file pointer, structure must be freed
        //printk("ref_count is 1, freeing file object\n");
        filep->inode->ref_count--;   // one structure decreased
        free_file_object(filep);
        filep=NULL;
      }else{  // some other file pointers point to the same file structure
        //printk("ref_count is %d, decreasing it by 1\n", filep->ref_count);
        filep->ref_count--;
        filep=NULL;
      }
    }else if(filep->type == PIPE){
      // pipe 
      //printk("pipe file type\n");
      if(filep->ref_count==1){ //this is the only file pointer, structure must be freed
        //printk("filep->ref_count is 1\n");
        if(filep->mode==O_READ){
          //printk("mode is O_READ, updating is_ropen to 0\n");
          filep->pipe->is_ropen=0;
          if(!(filep->pipe->is_wopen)){ // if write end is already closed then free pipe
            //printk("wopen is 0, freeing pipe object\n");
            free_pipe_info(filep->pipe);
          }
        }else if(filep->mode==O_WRITE){
          //printk("mode is O_WRITE, updating it to 0\n");
          filep->pipe->is_wopen=0;
          if(!(filep->pipe->is_ropen)){ // if read end is already closed then free pipe
            //printk("wopen is 0, freeing pipe object\n");
            free_pipe_info(filep->pipe);
          }
        }else{      // should not happen, if happens, initialising error or mode changed
          //printk("should not happen, if happens, initialising error or mode changed\n");
          return -EINVAL;
        }
        //printk("freeing file structure\n");
        free_file_object(filep);  // this is last pointer to either read or write end of pipe
        filep=NULL;
      }else{  // some other file pointers point to the same file structure
        //printk("ref_count is %d, decreasing it by 1\n", filep->ref_count);
        filep->ref_count--;
        filep=NULL;
      }
    }else{
      //printk("closing std fds\n");
      if(filep->ref_count==1){
        //printk("last pointer to file struct\n");
        free_file_object(filep);
      }else{
        //printk("some other pointers\n");
        filep->ref_count--;
      }
      filep=NULL;
    }
    int ret_fd = -EINVAL; 
    return 1; // for checking purposes
}

void do_file_exit(struct exec_context *ctx)
{
   /*TODO the process is exiting. Adjust the ref_count
     of files*/
     //printk("inside do_file_exit\n");
     long l;
     for(int i=0; i<MAX_OPEN_FILES; i++){
       if(ctx->files[i]){ // assuming ref_count is properly adjusted in generic_close
         //printk("file des %d is open, with ref_count: %d, calling generic_close\n", i, ctx->files[i]->ref_count);
         l = generic_close(ctx->files[i]); //may throw errors here
         ctx->files[i]=NULL;
       }
     }
}


static int do_read_regular(struct file *filep, char * buff, u32 count)
{
   /** TODO Implementation of File Read, 
    *  You should be reading the content from File using file system read function call and fill the buf
    *  Validate the permission, file existence, Max length etc  HOW??????
    *  Incase of Error return valid Error code HOW????????
    * */
    //printk("inside do_read_regular\n");
    if(!filep){
      return -EINVAL;
    }
    if(filep->inode->mode & O_READ != O_READ){
      //printk("file not created in read mode\n");
      return -EACCES;
    }else if(filep->mode & O_READ != O_READ){
      //printk("file not opened with reading mode\n");
      return -EACCES;
    }
    u32 ofst = filep->offp;
    int bytread = flat_read(filep->inode, buff, count, &ofst); // what if count is more than length of buff
    //printk("bytread is %d, filep->offp before is %d, after is %d\n", bytread, filep->offp, filep->offp + bytread);
    filep->offp=bytread+filep->offp;
    int ret_fd = bytread; 
    return ret_fd;
}


static int do_write_regular(struct file *filep, char * buff, u32 count)
{
    /** TODO Implementation of File write, 
    *   You should be writing the content from buff to File by using File system write function
    *   Validate the permission, file existence, Max length etc
    *   Incase of Error return valid Error code 
    * */
    //printk("inside do_write_regular\n");
    if(!filep){
      return -EINVAL;
    }
    if(filep->inode->mode & O_WRITE != O_WRITE){
      //printk("file not created in read mode\n");
      return -EACCES;
    }else if(filep->mode & O_WRITE != O_WRITE){
      //printk("file not opened with reading mode\n");
      return -EACCES;
    }
    struct inode * inode1 = filep->inode;
    u32 ofst = filep->offp;
    int bytwrite = flat_write(inode1, buff, count, &ofst);
    //printk("bytwrite is %d, filep->offp before is %d, after is %d\n", bytwrite, filep->offp, filep->offp + bytwrite);
    filep->offp = filep->offp + bytwrite;
    int ret_fd = bytwrite; 
    return ret_fd;
}

static long do_lseek_regular(struct file *filep, long offset, int whence)
{
    /** TODO Implementation of lseek 
    *   Set, Adjust the ofset based on the whence
    *   Incase of Error return valid Error code 
    * */

   //printk("inside do_lseek_regular\n");
   // throw error if offset becomes greater than File end means Max_File_Size (4KB).
   if(!filep || filep->type!=REGULAR){
     //printk("file is irreg or filep is NULL\n");
     return -EINVAL; //NULL ptr or non regular file
   }else if((filep->inode->e_pos - filep->inode->s_pos) < offset){
     //printk("offset is more than max file size\n");
     return -EOTHERS;
   }
   if(whence == SEEK_SET){
     //printk("SEEK_SET\n");
     filep->offp=offset;
   }else if(whence == SEEK_CUR){
     //printk("SEEK_CUR\n");
     if((filep->inode->e_pos - filep->inode->s_pos) < (offset + filep->offp)){
      //printk("offset + filep->offp is more than max file size\n");
      return -EOTHERS;
    }
     filep->offp+=offset;
   }else if(whence == SEEK_END){
     //printk("SEEK_END\n");
     filep->offp=filep->inode->file_size + offset;
   }else{
     //printk("some other seek operation, should not happpen\n");
     return -EINVAL;
   }
    if(filep->offp<0){
      //printk("overflow occured\n");
      return -EINVAL;
    }
    int ret_fd = filep->offp; 
    
    return ret_fd;
}

extern int do_regular_file_open(struct exec_context *ctx, char* filename, u64 flags, u64 mode)
{ 
  /**  TODO Implementation of file open, 
    *  You should be creating file(use the alloc_file function to creat file), 
    *  To create or Get inode use File system function calls, 
    *  Handle mode and flags 
    *  Validate file existence, Max File count is 32, Max Size is 4KB, etc
    *  Incase of Error return valid Error code 
    * */

    // s2: find free fd
    int i=3; // looking from index 3
    while(ctx->files[i]){
      i++;
      //printf("looking for free fd, ind is %d\n", i);
    }
    if(i>MAX_OPEN_FILES){
      //printk("all fds are occupied\n");
      return -EOTHERS;
    }
    //printf("free fd found: %d\n", i);
    int fd=i;


    
    
    struct inode * inode1;
    if((flags & O_CREAT) == O_CREAT){
      //printk("creating new file inode\n");
      // create a new file
      //printf("creating a new file\n");
      // s1: get inode
      if(lookup_inode(filename)){ // file with same name already exist
        // error occured
        //printk("error occured in opening file, inode already exist\n");
        return -EINVAL; // return appropriate exit code 
      }
      // if ((((flags & O_READ) == O_READ) && ((mode & O_READ) != O_READ)) || (((flags & O_WRITE) == O_WRITE) && ((mode & O_WRITE) != O_WRITE)) || (((flags & O_EXEC) == O_EXEC) && ((mode & O_EXEC) != O_EXEC) )) {
      //     // permission denied
      //     //printk("permission denied\n");
      //     return -EACCES;
      //   }
      inode1 = create_inode(filename, mode);
      if(!inode1){
        return -ENOMEM;
      }
      inode1->ref_count=1;
      
    }else{
      // open an existing file
      // s1: look for the inode
      //printk("opening an existing file\n");
      inode1 = lookup_inode(filename);
      if(inode1==NULL){
        // error occured
        //printk("error occured in opening file, inode is NULL\n");
        return -EINVAL; // return appropriate exit code 
      }else{
        // assuming no error
        u32 mode_inode;
        mode_inode = inode1->mode; // may be an error if mode is not assigned in inode
        // checking permissions
        if ((((flags & O_READ) == O_READ) && ((mode_inode & O_READ) != O_READ)) || (((flags & O_WRITE) == O_WRITE) && ((mode_inode & O_WRITE) != O_WRITE)) || (((flags & O_EXEC) == O_EXEC) && ((mode_inode & O_EXEC) != O_EXEC) )) {
          // permission denied
          //printk("permission denied\n");
          return -EACCES;
        }else{
          //printk("correct permissions, good to go\n");
          // filep->inode = inode2;
          inode1->ref_count++; // new struct object created
        }
      }
      
    }
    //printk("creating a new file object\n");
    // assuming no error
    // s3: allocating a file object 
    struct file * filep = alloc_file();
    if(!filep){
      return -ENOMEM;
    }
    // s4: filling the fields
    filep->inode = inode1;
    filep->pipe = NULL;
    filep->mode = flags; // assigning mode of opening :: ERROR mode = flags - OCREAT
    filep->type = REGULAR;
    filep->ref_count=1;
    filep->offp = 0;
    filep->fops->read = do_read_regular;
    filep->fops->write = do_write_regular;
    filep->fops->close = generic_close;
    filep->fops->lseek= do_lseek_regular;
    ctx->files[fd] = filep;
    int ret_fd = fd; 
    return ret_fd;
}

int fd_dup(struct exec_context *current, int oldfd)
{
     /** TODO Implementation of dup 
      *  Read the man page of dup and implement accordingly 
      *  return the file descriptor,
      *  Incase of Error return valid Error code 
      * */
     //printk("inside fd_dup\n");
     struct file * filep = current->files[oldfd];
     int ret_fd; 
     if(filep){
        // s2: find free fd
        int i=0; // looking from index 0
        while(current->files[i]){
          i++;
          //printf("looking for free fd, ind is %d\n", i);
        }
        if(i>MAX_OPEN_FILES){
          //printk("free fd found: %d\n", i);
          return -EOTHERS;
        }
        //printk("free fd found: %d\n", i);
        int fd=i;
        filep->ref_count++;
        current->files[fd] = filep;
        ret_fd=fd;
     }else{
       // oldfd does not exist
       //printk("oldfd does not exist\n");
       ret_fd=-EINVAL;
     }
    return ret_fd;
}


int fd_dup2(struct exec_context *current, int oldfd, int newfd)
{
  /** TODO Implementation of the dup2 
    *  Read the man page of dup2 and implement accordingly 
    *  return the file descriptor,
    *  Incase of Error return valid Error code 
    * */
   //printk("inside fd_dup2\n");
    if(!(current->files[oldfd])){
      //printk("oldfd does not exist\n");
      return -EINVAL;
    }
    if(oldfd==newfd){
      return oldfd;
    }else{
      struct file * filep = current->files[newfd];
      if(filep){
        long l = generic_close(filep);
        current->files[newfd]=NULL;
      }
      current->files[newfd]=current->files[oldfd];
      current->files[newfd]->ref_count++;
    } 
    return newfd;
}