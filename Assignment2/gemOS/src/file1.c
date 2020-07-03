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
    for(int j=0;j<MAX_OPEN_FILES;j++){
       if(child->files[j]){//increase ref_count of all the open fd.
         child->files[j]->ref_count++;
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
    return -EINVAL;
  }//if file already closed
  if(filep->type == REGULAR){
      if(filep->ref_count==1){
        filep->inode->ref_count--;
        filep->ref_count--;
        free_file_object(filep);//closing file
      }else{
        filep->ref_count--;
      }
      return 0;
  }
  else if(filep->type == PIPE){
      if(filep->ref_count==1){
        if(filep->mode == O_READ){
          filep->pipe->is_ropen=0;
        }
        if(filep->mode == O_WRITE){
          filep->pipe->is_wopen=0;
        }
        filep->ref_count--;
        if(filep->pipe->is_ropen==0 && filep->pipe->is_wopen==0){
          free_pipe_info(filep->pipe);//closing pipe
        }
        free_file_object(filep);//closing file
      }else{
        filep->ref_count--;
      }
      return 0;
  }else{
    if(filep->ref_count==1){
        filep->inode->ref_count--;
        filep->ref_count--;
        free_file_object(filep);//closing file
      }else{
        filep->ref_count--;
      }
      return 0;
  }
}

void do_file_exit(struct exec_context *ctx)
{
   /*TODO the process is exiting. Adjust the ref_count
     of files*/
     long tmp;
     for(int i=0;i<MAX_OPEN_FILES;i++){
       if(ctx->files[i]){//close all the open fd.
         tmp = generic_close(ctx->files[i]);
         ctx->files[i]=NULL;
       }
     }
}

static int do_read_regular(struct file *filep, char * buff, u32 count)
{
   /** TODO Implementation of File Read, 
    *  You should be reading the content from File using file system read function call and fill the buf
    *  Validate the permission, file existence, Max length etc
    *  Incase of Error return valid Error code 
    * */
    int byte_read=0;
    if(!filep){
     return -EINVAL;
    }//If file doesn't exist return error
    if(filep->inode->mode & O_READ != O_READ){
      return -EACCES;
    }
    if( filep->mode & O_READ == O_READ){
      u32 offset = filep->offp;
      byte_read=flat_read(filep->inode,buff,count,&offset);
      filep->offp+=byte_read; 
    }else{//If permission didn't matched
      return -EACCES;
    }
    return byte_read;
}


static int do_write_regular(struct file *filep, char * buff, u32 count)
{
    /** TODO Implementation of File write, 
    *   You should be writing the content from buff to File by using File system write function
    *   Validate the permission, file existence, Max length etc
    *   Incase of Error return valid Error code 
    * */
   int byte_write=0;
   if(!filep){
     return -EINVAL;
   }//if file doesnt exist then report
   if(filep->inode->mode & O_WRITE != O_WRITE){
      return -EACCES;
    }
    if( filep->mode & O_WRITE == O_WRITE){
      u32 offset = filep->offp;
      byte_write=flat_write(filep->inode,buff,count,&offset);
      filep->offp+=byte_write; 
    }else{//If mode is not matching
      return -EACCES;
    }
    return byte_write;
}

static long do_lseek_regular(struct file *filep, long offset, int whence)
{
    /** TODO Implementation of lseek 
    *   Set, Adjust the ofset based on the whence
    *   Incase of Error return valid Error code 
    * */
    if(!filep){
      return -EINVAL;
    }
    if(filep->type != REGULAR){
      return -EINVAL;
    }
    if(whence == SEEK_CUR){
      u32 offp = filep->offp;
      offset+=offp;
    }
    if(whence == SEEK_END){
      long offp = filep->inode->file_size;
      offset+=offp;
    }
    if(!(whence == SEEK_CUR) && !(whence == SEEK_END) && !(whence == SEEK_SET)){
      return -EINVAL;
    }//if whence  is not among question mention cond. 
    if(offset > filep->inode->file_size || offset < 0){
      return -EINVAL;
    }//If file offset exceed limit of file return error
    filep->offp=offset;
    return filep->offp;//return file offset.
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
    if((flags & O_CREAT)==O_CREAT){/*If file does not exist and need to be created */
      if(lookup_inode(filename)){
        return -EINVAL;
      }//If already exist then give error
      struct file * file_ptr=alloc_file();
      if(!file_ptr){//if file ptr is not valid.
        return -ENOMEM;
      }
      struct inode * inodeFile = create_inode(filename,mode);
      if(!inodeFile){//if create_inode returned error.
        return -ENOMEM;
      }
      int i=3;
      while(ctx->files[i]){//checking for next valid empty fd.
        i++;
      }
      if(i>MAX_OPEN_FILES){//if its more than MAX_OPEN_FILES
        return -EOTHERS;
      }
      file_ptr->type=REGULAR;//Initializing the file object
      file_ptr->mode=flags;
      file_ptr->ref_count=1;
      file_ptr->inode=inodeFile;
      file_ptr->fops->read=do_read_regular;
      file_ptr->fops->write=do_write_regular;
      file_ptr->fops->lseek=do_lseek_regular;
      file_ptr->fops->close=generic_close;      
      ctx->files[i]=file_ptr;
      return i;
    }else{/*If file exists */
      struct file * file_ptr=alloc_file();
      if(!file_ptr){//If alloc_file() returned error.
        return -ENOMEM;
      }
      struct inode * inodeFile = lookup_inode(filename);
      if( !inodeFile ){//if lookup_inode() returned  error.
        return -EINVAL;
      }
      u64 mode_prev_file = inodeFile->mode;
      if( ( flags & mode_prev_file ) == flags ){
        /*If permission is verified. */
          inodeFile->ref_count++;
          int i=3;//check for empty fd.
          while(ctx->files[i]){
            i++;
          }
          if(i>MAX_OPEN_FILES){//error for fd>32
            return -EOTHERS;
          }
          file_ptr->type=REGULAR;//Initializing file object
          file_ptr->mode=flags;
          file_ptr->ref_count=1;
          file_ptr->inode=inodeFile;
          file_ptr->fops->read=do_read_regular;
          file_ptr->fops->write=do_write_regular;
          file_ptr->fops->lseek=do_lseek_regular;
          file_ptr->fops->close=generic_close;      
          ctx->files[i]=file_ptr;
          return i;
      }else{//If permission didn't matched.
        return -EACCES;
      }
    } 
}

int fd_dup(struct exec_context *current, int oldfd)
{
     /** TODO Implementation of dup 
      *  Read the man page of dup and implement accordingly 
      *  return the file descriptor,
      *  Incase of Error return valid Error code 
      * */
    if(!current->files[oldfd]){//if there's no file at given fd
      return -EINVAL;
    }
    int i=0;//check for least free fd.
    while(current->files[i]){
      i++;
    }
    if(i>MAX_OPEN_FILES){//check for fd<32
      return -EOTHERS;
    }
    current->files[oldfd]->ref_count++;//increases the ref count after dup
    current->files[i]=current->files[oldfd];
    return i;//retunr new fd
}


int fd_dup2(struct exec_context *current, int oldfd, int newfd)
{
  /** TODO Implementation of the dup2 
    *  Read the man page of dup2 and implement accordingly 
    *  return the file descriptor,
    *  Incase of Error return valid Error code 
    * */
    if(!current->files[oldfd]){//if there's no file at given fd
      return -EINVAL;
    }
    if(oldfd==newfd){//if same param then just return same fd
      return oldfd;
    }
    current->files[oldfd]->ref_count++;//increases the ref count after dup2
    long tmp=generic_close(current->files[newfd]);//close the already opened file at newfd
    current->files[newfd]=current->files[oldfd];
    return newfd;//return new fd.
}
