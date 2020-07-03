#include "common.h"
static int atomic_add(long *ptr, long val)
{
       int ret = 0;
       asm volatile( 
                     "lock add %%rsi, (%%rdi);"
                     "pushf;"
                     "pop %%rax;" 
                     "movl %%eax, %0;"
                     : "=r" (ret)
                     : 
                     : "memory", "rax"
                    );

     
     if(ret & 0x80)
               ret = -1;
     else if(ret & 0x40)
               ret = 0;
     else
               ret = 1;
     return ret;
}
int read_op(struct input_manager *in, op_t *op, int tnum)
{
   pthread_mutex_lock(&in->lock);
   unsigned size = sizeof(op_t);
   memcpy(op, in->curr, size - sizeof(unsigned long));  //Copy till data ptr     
   if(op->op_type == GET || op->op_type == DEL){
       in->curr += size - sizeof(op->datalen) - sizeof(op->data);
   }else if(op->op_type == PUT){
       in->curr += size - sizeof(op->data);
       op->data = in->curr;
       in->curr += op->datalen;
   }else{
       assert(0);
   }
   if(in->curr > in->data + in->size){
        pthread_mutex_unlock(&in->lock);
        return -1;
   }
    int temp;
    do{
      temp = 0;
      for(int i=0; i<THREADS; i++)
        if((in->being_processed[i] != NULL) && (in->being_processed[i]->id < op->id) && (in->being_processed[i]->key == op->key))
          temp = 1;
      if(temp)
        pthread_cond_wait(&in->cond,&in->lock);
    }while(temp);
   in->being_processed[tnum] = op;
   pthread_mutex_unlock(&in->lock);
   return 0; 
}

void done_one(struct input_manager *in, int tnum)
{
  pthread_mutex_lock(&in->lock);
   in->being_processed[tnum] = NULL;
   pthread_cond_signal(&in->cond);
   pthread_mutex_unlock(&in->lock);
}

int lookup(hash_t *h, op_t *op)
{
  unsigned ctr;
  unsigned hashval = hashfunc(op->key, h->table_size);
  hash_entry_t *entry = h->table + hashval;
  pthread_mutex_lock(&entry->lock);
  ctr = hashval;
  while((entry->key || entry->id == (unsigned) -1) && 
         entry->key != op->key && ctr != hashval - 1){
      pthread_mutex_unlock(&entry->lock);
      ctr = (ctr + 1) % h->table_size;
      entry = h->table + ctr; 
      pthread_mutex_lock(&entry->lock);
  } 
 if(entry->key == op->key){
      op->datalen = entry->datalen;
      op->data = entry->data;
      pthread_mutex_unlock(&entry->lock);
      return 0;
 }
 pthread_mutex_unlock(&entry->lock);
 return -1;
}

int insert_update(hash_t *h, op_t *op)
{
   unsigned ctr;
   unsigned hashval = hashfunc(op->key, h->table_size);
   hash_entry_t *entry = h->table + hashval;
   pthread_mutex_lock(&entry->lock);
   assert(h && h->used < h->table_size);
   assert(op && op->key);

   ctr = hashval;

   while(entry->key && entry->key != op->key && ctr != hashval - 1){
         pthread_mutex_unlock(&entry->lock);
         ctr = (ctr + 1) % h->table_size;
         entry = h->table + ctr; 
         pthread_mutex_lock(&entry->lock);
   } 

  assert(ctr != hashval - 1);

  if(entry->key == op->key){  //It is an update
      entry->id = op->id;
      entry->datalen = op->datalen;
      entry->data = op->data;
      pthread_mutex_unlock(&entry->lock);
      return 0;
  }
  assert(!entry->key);   // Fresh insertion
 
  entry->id = op->id;
  entry->datalen = op->datalen;
  entry->key = op->key;
  entry->data = op->data;
  
  atomic_add(&h->used,1);
  pthread_mutex_unlock(&entry->lock);
  return 0; 
}

int purge_key(hash_t *h, op_t *op)
{
   unsigned ctr;
   unsigned hashval = hashfunc(op->key, h->table_size);
   hash_entry_t *entry = h->table + hashval;
   pthread_mutex_lock(&entry->lock);
   ctr = hashval;
   while((entry->key || entry->id == (unsigned) -1) && 
          entry->key != op->key && ctr != hashval - 1){
          pthread_mutex_unlock(&entry->lock);
         ctr = (ctr + 1) % h->table_size;
         entry = h->table + ctr;
          pthread_mutex_lock(&entry->lock);
   } 

   if(entry->key == op->key){  //Found. purge it
      entry->id = (unsigned) -1;  //Empty but deleted
      entry->key = 0;
      entry->datalen = 0;
      entry->data = NULL;
      atomic_add(&h->used,-1);
      pthread_mutex_unlock(&entry->lock);
      return 0;
   }
   pthread_mutex_unlock(&entry->lock);
  return -1;    // Bogus purge
}
