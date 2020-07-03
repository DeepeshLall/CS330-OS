#include<types.h>
#include<mmap.h>
#include<cfork.h>
#include<page.h>
#include<entry.h>
#include<lib.h>
#include<context.h>
#include<memory.h>
#include<schedule.h>

/**
 * Function will invoked whenever there is page fault. (Lazy allocation)
 * 
 * For valid acess. Map the physical page 
 * Return 1
 * 
 * For invalid access,
 * Return -1. 
 */
int vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{
    //printk("\nInside vm_area_pagefault\n");
    int fault_fixed = 1;
    struct vm_area* ptr=current->vm_area;
    while(ptr){
        if(ptr->vm_start<=addr && ptr->vm_end>=addr+4096){
            break;
        }
        ptr=ptr->vm_next;
    }
    if(ptr==NULL){
        //printk("->Found invalid addr.\n");
        return -1;
    }

    asm volatile ("invlpg (%0);" 
                    :: "r"(addr) 
                    : "memory");
    if(error_code==4 && (ptr->access_flags & PROT_READ)==PROT_READ){
        //printk("->installing in code4\n");
        install_page_table(current,addr,ptr->access_flags);
    }
    if(error_code==6 && (ptr->access_flags & PROT_WRITE)==PROT_WRITE){
        //printk("->installing in code6\n");
        install_page_table(current,addr,ptr->access_flags);
    }
    if((error_code==4 && (ptr->access_flags & PROT_READ)!=PROT_READ) || (error_code==6 && (ptr->access_flags & PROT_WRITE)!=PROT_WRITE)){
        //printk("->Invalid access rights\n");
        return -1;
    }
    return fault_fixed;
}
void mmap_populate(struct exec_context *current, u64 addr, int length, int prot){
    //printk("\nCalled mmap populate\n");
    int loop_cnt=length/4096;
    for(int i=0;i<loop_cnt;i++){
        install_page_table(current,addr+i*4096,prot);
    }
}

void unmap_page(struct exec_context *current, u64 addr, int length){
    //printk("\nInside utility of unmap\n");
    int loop_cnt=length/4096;
    //printk("->loop_cnt : %d, addr: %x\n",loop_cnt, addr);
    int tmp=0;
    for(int i=0;i<loop_cnt;i++){
        u64 *pte = get_user_pte(current, addr+i*4096, 0);
        //printk("vaddr: %x\n", addr+i*4096);
        struct pfn_info * p = get_pfn_info((*pte & FLAG_MASK) >> PAGE_SHIFT);
        int ref_cnt = get_pfn_info_refcount(p);
        if(ref_cnt==1){
            //printk("->Ref_cnt=1\n");
            os_pfn_free(USER_REG, (*pte >> PTE_SHIFT) & 0xFFFFFFFF);
            *pte= 0;
        }else if(ref_cnt>1){
            //printk("->Ref_cnt > 1\n");
            decrement_pfn_info_refcount(p);
        }
        //printk("->ref_cnt : %d\n",ref_cnt);
        asm volatile ("invlpg (%0);" 
                        :: "r"(addr) 
                        : "memory");
    }
}

void mprotect_physical(struct exec_context *current, u64 addr, int length, int prot){
    int loop_cnt=length/4096;
    u64* pte;
    for (int i = 0; i < loop_cnt; i++)
    {
        pte=get_user_pte(current,addr+i*4096,0);
        if((prot & PROT_READ) == PROT_READ){
            (*pte)=(((*pte)>>2)<<2) | 0x01;
        }
        if((prot & PROT_WRITE) == PROT_WRITE){
            (*pte)=(((*pte)>>2)<<2) | 0x11;
        }
    asm volatile ("invlpg (%0);" 
                    :: "r"(addr + i*4096) 
                    : "memory");   // Flush TLB
    }
        
}

/**
 * mprotect System call Implementation.
 */
int vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{
    //printk("\nCallin mprotect\n");
    struct vm_area* ptr_itr=current->vm_area;
    struct vm_area* ptr_1;
    struct vm_area* ptr_2;
    struct vm_area* ptr_1_prev;
    struct vm_area* ptr_2_prev;
    int isValid=0;
    while(ptr_itr){//finding ptr_1 and ptr_2
        if(ptr_itr->vm_start<=addr && ptr_itr->vm_end>addr){
            ptr_1=ptr_itr;
        }
        if(ptr_itr->vm_start<=addr+length && ptr_itr->vm_end>addr+length){
            ptr_2=ptr_itr;
        }
        ptr_itr=ptr_itr->vm_next;
    }
    if(ptr_1==ptr_2){
        //printk("->new_vm_area is inside a single vm\n");
        struct vm_area* new_vm_area=alloc_vm_area();
        struct vm_area* new_vm_area_2=alloc_vm_area();
        new_vm_area->vm_start=addr;
        new_vm_area->vm_end=addr+length;
        new_vm_area->access_flags=prot;
        new_vm_area->vm_next=new_vm_area_2;
        new_vm_area_2->vm_start=addr+length;
        new_vm_area_2->vm_end=ptr_1->vm_end;
        new_vm_area_2->access_flags=ptr_1->access_flags;
        new_vm_area_2->vm_next=ptr_1->vm_next;
        ptr_1->vm_next=new_vm_area;
        ptr_1->vm_end=addr;

        mprotect_physical(current,addr,length,prot);

        return 0;
    }
    ptr_itr=current->vm_area;
    while(ptr_itr){//finding ptr_1_prev and ptr_2_prev
        if(ptr_itr->vm_next==ptr_1){
            ptr_1_prev=ptr_itr;
        }
        if(ptr_itr->vm_next==ptr_2){
            ptr_2_prev=ptr_itr;
        }
        ptr_itr=ptr_itr->vm_next;
    }
    ptr_itr=current->vm_area;
    if(addr==ptr_1->vm_start && addr+length==ptr_2->vm_start){//if both side of new_vm_area are at boundary
        //printk("->vm_area's both end are corner cases.\n");
        struct vm_area* ptr_itr_2=ptr_1_prev;
        while (ptr_itr_2->vm_end == ptr_itr_2->vm_next->vm_start && ptr_itr_2!=ptr_2)//checking for continuity
        {
            ptr_itr_2=ptr_itr_2->vm_next;
        }
        if(ptr_itr_2!=ptr_2){
            //printk("-->Discontinuose in between\n");
            return -1;
        }else{
            //printk("-->Allocating new_vm and removing old vm in range\n");
            struct vm_area* new_vm_area=alloc_vm_area();
            new_vm_area->vm_start=addr;
            new_vm_area->vm_end=addr+length;
            new_vm_area->access_flags=prot;
            new_vm_area->vm_next=ptr_2;
            struct vm_area* ptr_itr_3=ptr_1;
            ptr_1_prev->vm_next=new_vm_area;
            while (ptr_itr_3 && ptr_itr_3!=ptr_2)
            {
                struct vm_area* ptr_itr_temp=ptr_itr_3->vm_next;
                dealloc_vm_area(ptr_itr_3);
                ptr_itr_3=ptr_itr_temp;
            }
        }
    }else if(addr==ptr_1->vm_start){//If start of new_vm_area is at boundary
        //printk("->vm_area's start end is corner case\n");
        struct vm_area* ptr_itr_2=ptr_1_prev;
        while (ptr_itr_2->vm_end == ptr_itr_2->vm_next->vm_start && ptr_itr_2!=ptr_2)//checking for continuity
        {
            ptr_itr_2=ptr_itr_2->vm_next;
        }
        if(ptr_itr_2!=ptr_2){
            //printk("-->Discontinuose in between.\n");
            return -1;
        }else{
            //printk("-->Allocating new_vm and removing old vm in range\n");
            struct vm_area* new_vm_area=alloc_vm_area();
            new_vm_area->vm_start=addr;
            new_vm_area->vm_end=addr+length;
            new_vm_area->access_flags=prot;
            new_vm_area->vm_next=ptr_2;
            ptr_2->vm_start=addr+length;
            struct vm_area* ptr_itr_3=ptr_1;
            ptr_1_prev->vm_next=new_vm_area;
            while (ptr_itr_3 && ptr_itr_3!=ptr_2)
            {
                struct vm_area* ptr_itr_temp=ptr_itr_3->vm_next;
                dealloc_vm_area(ptr_itr_3);
                ptr_itr_3=ptr_itr_temp;
            }
        }
    }else if(addr+length==ptr_2->vm_start){//If end of new_vm_area is at boundary
        //printk("->End part of vm_area is a corner case\n");
        struct vm_area* ptr_itr_2=ptr_1;
        while (ptr_itr_2->vm_end == ptr_itr_2->vm_next->vm_start && ptr_itr_2!=ptr_2)//checking for continuity
        {
            ptr_itr_2=ptr_itr_2->vm_next;
        }
        if(ptr_itr_2!=ptr_2){
            //printk("-->Discontinuose in between\n");
            return -1;
        }else{
            //printk("-->Allocating new_vm and removing old vm in range\n");
            struct vm_area* new_vm_area=alloc_vm_area();
            new_vm_area->vm_start=addr;
            new_vm_area->vm_end=addr+length;
            new_vm_area->access_flags=prot;
            new_vm_area->vm_next=ptr_2;
            ptr_1->vm_end=addr;
            struct vm_area* ptr_itr_3=ptr_1->vm_next;
            ptr_1->vm_next=new_vm_area;
            while (ptr_itr_3 && ptr_itr_3!=ptr_2)
            {
                struct vm_area* ptr_itr_temp=ptr_itr_3->vm_next;
                dealloc_vm_area(ptr_itr_3);
                ptr_itr_3=ptr_itr_temp;
            }
        }
    }else{
        //printk("->new vmarea general case\n");
        struct vm_area* ptr_itr_2=ptr_1;//General
        while (ptr_itr_2->vm_end == ptr_itr_2->vm_next->vm_start && ptr_itr_2!=ptr_2)//checking for continuity
        {
            ptr_itr_2=ptr_itr_2->vm_next;
        }
        if(ptr_itr_2!=ptr_2){
            //printk("-->Discontinuose in between\n");
            return -1;
        }else{
            //printk("-->Allocating new_vm and removing old vm in range\n");
            struct vm_area* new_vm_area=alloc_vm_area();
            new_vm_area->vm_start=addr;
            new_vm_area->vm_end=addr+length;
            new_vm_area->access_flags=prot;
            new_vm_area->vm_next=ptr_2;
            ptr_1->vm_end=addr;
            ptr_2->vm_start=addr+length;
            struct vm_area* ptr_itr_3=ptr_1->vm_next;
            ptr_1->vm_next=new_vm_area;
            while (ptr_itr_3 && ptr_itr_3!=ptr_2)
            {
                struct vm_area* ptr_itr_temp=ptr_itr_3->vm_next;
                dealloc_vm_area(ptr_itr_3);
                ptr_itr_3=ptr_itr_temp;
            }
        }
    }
    //Merge vm_area with same prot
    //printk("->Merging all vms if possible\n");
    struct vm_area* ptr_itr_4=current->vm_area;
    while (ptr_itr_4->vm_next)
    {
        if(ptr_itr_4->vm_next->access_flags==ptr_itr_4->access_flags){
            //printk("-->Merging 2 vms\n");
            ptr_itr_4->vm_end=ptr_itr_4->vm_next->vm_end;
            struct vm_area* ptr_temp=ptr_itr_4->vm_next->vm_next;
            dealloc_vm_area(ptr_itr_4->vm_next);
            ptr_itr_4->vm_next=ptr_temp;
        }
        ptr_itr_4=ptr_itr_4->vm_next;
    }
    mprotect_physical(current,addr,length,prot);
    return isValid;
}
/**
 * mmap system call implementation.
 */
long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{
    long ret_addr;
    if(length%4096){
        length=(length/4096+1)*4096;
    }
    int mflag=0;
    if((flags & MAP_POPULATE) == MAP_POPULATE){
        mflag=1;
    }
    if(!current->vm_area){
        //printk("\nFirst allocation of vm\n");
        if(addr){
            //printk("->If addr is mentioned.\n");
            struct vm_area * first_vm_area=alloc_vm_area();
            first_vm_area->vm_start=addr;
            if( (addr+length) > MMAP_AREA_END){
                return -1;
            }
            first_vm_area->vm_end=addr+length;
            first_vm_area->access_flags=prot;
            first_vm_area->vm_next=NULL;
            current->vm_area=first_vm_area;
            ret_addr=first_vm_area->vm_start;
        }else{
            //printk("->If addr is not mentioned.\n");
            struct vm_area * first_vm_area=alloc_vm_area();
            first_vm_area->vm_start=MMAP_AREA_START;
            if( (MMAP_AREA_START+length) > MMAP_AREA_END){
                return -1;
            }
            first_vm_area->vm_end=MMAP_AREA_START+length;
            first_vm_area->access_flags=prot;
            first_vm_area->vm_next=NULL;
            current->vm_area=first_vm_area;
            ret_addr=first_vm_area->vm_start;
        }
        if(mflag){
            mmap_populate(current,ret_addr,length,prot); 
        }
        return ret_addr;
    }else{
        //printk("\nAllocation of new vm\n");
        if(!addr){
            //printk("->If addr is not mentioned.\n");
            struct vm_area* ptr=current->vm_area;
            while(ptr->vm_next && ((ptr->vm_next->vm_start)-(ptr->vm_end) < length )){
                ptr=ptr->vm_next;
            }
            if(ptr->vm_next){
                //printk("-->Inserted in middle.\n");
                struct vm_area* ptr2=ptr->vm_next;
                if(ptr2->access_flags == prot && ptr->access_flags == prot && (ptr2->vm_start - ptr->vm_end) == length){
                    //printk("--->Merge with both next and prev.\n");
                    ptr->vm_next=ptr2->vm_next;
                    ptr->vm_end=ptr2->vm_end;
                    dealloc_vm_area(ptr2);
                    if(mflag){
                        mmap_populate(current,ptr->vm_start,length,prot); 
                    }
                    return ptr->vm_start;
                }
                else if(ptr2->access_flags == prot && ptr2->access_flags != prot){
                    //printk("--->Merging with next vm.\n");
                    ptr2->vm_start=ptr2->vm_start-length;
                    if(mflag){
                        mmap_populate(current,ptr2->vm_start,length,prot); 
                    }
                    return ptr2->vm_start;
                }else{
                    //printk("--->Merging with previouse vm.\n");
                    ptr->vm_end=ptr->vm_end+length;
                    if(mflag){
                        mmap_populate(current,ptr->vm_start,length,prot); 
                    }
                    return ptr->vm_start;
                }
            }else{
                if(ptr->access_flags == prot){
                    //printk("-->Merging vm at the end.\n");
                    ptr->vm_end=ptr->vm_end+length;
                    if(mflag){
                        mmap_populate(current,ptr->vm_start,length,prot); 
                    }
                    return ptr->vm_start;
                }else{
                    //printk("-->Inserted a new vm at the end.\n");
                    if(ptr->vm_end+length>MMAP_AREA_END){
                        return -1;
                    }
                    struct vm_area * new_vm_area=alloc_vm_area();
                    ptr->vm_next=new_vm_area;
                    new_vm_area->vm_start=ptr->vm_end;
                    new_vm_area->vm_end=ptr->vm_end+length;
                    new_vm_area->access_flags=prot;
                    new_vm_area->vm_next=NULL;
                    if(mflag){
                        mmap_populate(current,new_vm_area->vm_start,length,prot); 
                    }
                    return new_vm_area->vm_start;
                }
            }
        }else{
            //printk("->If addr is mentioned.\n");
            if((flags & MAP_FIXED) == MAP_FIXED){
                //printk("-->MAP_FIXED as flags.\n");
                struct vm_area* ptr=current->vm_area;
                //Implement MAP_FIXED
                while(ptr->vm_next && ptr->vm_end<addr){
                    ptr=ptr->vm_next;
                }
                if(ptr->vm_start<=addr  && ptr->vm_end>addr){
                    //printk("--->Overlap in MAP_FIXED :: Error\n");
                    return -1;
                }
                struct vm_area* ptr2=current->vm_area;
                while(ptr2->vm_next == ptr){
                    ptr2=ptr2->vm_next;
                }
                struct vm_area * new_vm_area=alloc_vm_area();
                new_vm_area->vm_start=addr;
                new_vm_area->vm_end=addr+length;
                new_vm_area->vm_next=ptr;
                ptr2->vm_next=new_vm_area;
                if(mflag){
                    mmap_populate(current,new_vm_area->vm_start,length,prot); 
                }
                return new_vm_area->vm_start;
            }
            struct vm_area* ptr=current->vm_area;
            while(ptr && ptr->vm_end<addr){
                ptr=ptr->vm_next;
            }
            if(ptr->vm_start<=addr && ptr){//overlap of 2 vm area
                //printk("-->Overlap happening insert in 1st possible location.\n");
                struct vm_area* ptr2=current->vm_area;
                while(ptr2->vm_next && ((ptr2->vm_next->vm_start)-(ptr2->vm_end) < length )){
                    ptr2=ptr2->vm_next;
                }
                if(ptr2->vm_next){
                    //printk("--->Inserted in middle.\n");
                    struct vm_area* ptr3=ptr2->vm_next;
                    if(ptr3->access_flags == prot && ptr2->access_flags == prot && (ptr3->vm_start - ptr2->vm_end)==length){
                        //printk("---->Merge with both next and prev.\n");
                        ptr2->vm_next=ptr3->vm_next;
                        ptr2->vm_end=ptr3->vm_end;
                        dealloc_vm_area(ptr3);
                        if(mflag){
                            mmap_populate(current,ptr2->vm_start,length,prot); 
                        }
                        return ptr2->vm_start;
                    }
                    else if(ptr3->access_flags == prot && ptr2->access_flags != prot){
                        //printk("---->Merging with next vm.\n");
                        ptr3->vm_start=ptr3->vm_start-length;
                        if(mflag){
                            mmap_populate(current,ptr3->vm_start,length,prot); 
                        }
                        return ptr3->vm_start;
                    }else if(ptr2->access_flags == prot){
                        //printk("---->Merging with previouse vm.\n");
                        ptr2->vm_end=ptr2->vm_end+length;
                        if(mflag){
                            mmap_populate(current,ptr2->vm_start,length,prot); 
                        }
                        return ptr2->vm_start;
                    }else{
                        //printk("---->Inserted a new vm at the middle.\n");
                        if(ptr2->vm_end+length>MMAP_AREA_END){
                            return -1;
                        }
                        struct vm_area * new_vm_area=alloc_vm_area();
                        ptr2->vm_next=new_vm_area;
                        new_vm_area->vm_start=ptr2->vm_end;
                        new_vm_area->vm_end=ptr2->vm_end+length;
                        new_vm_area->access_flags=prot;
                        new_vm_area->vm_next=ptr3;
                        if(mflag){
                            mmap_populate(current,new_vm_area->vm_start,length,prot); 
                        }
                        return new_vm_area->vm_start;
                    }
                }else{
                    if(ptr2->access_flags == prot){
                        //printk("--->Merging vm at the end.\n");
                        ptr2->vm_end=ptr2->vm_end+length;
                        if(mflag){
                            mmap_populate(current,ptr2->vm_start,length,prot); 
                        }
                        return ptr2->vm_start;
                    }else{
                        //printk("--->Inserted a new vm at the end.\n");
                        if(ptr2->vm_end+length>MMAP_AREA_END){
                            return -1;
                        }
                        struct vm_area * new_vm_area=alloc_vm_area();
                        ptr2->vm_next=new_vm_area;
                        new_vm_area->vm_start=ptr2->vm_end;
                        new_vm_area->vm_end=ptr2->vm_end+length;
                        new_vm_area->access_flags=prot;
                        new_vm_area->vm_next=NULL;
                        if(mflag){
                            mmap_populate(current,new_vm_area->vm_start,length,prot); 
                        }
                        return new_vm_area->vm_start;
                    }
                }
            }
            while(ptr->vm_next && ptr->vm_end<addr){
                ptr=ptr->vm_next;
            }
            //printk("-->Making a new individual vm and insert it at addr.\n");
            struct vm_area * new_vm_area=alloc_vm_area();
            new_vm_area->vm_start=addr;
            new_vm_area->vm_end=addr+length;
            new_vm_area->access_flags=prot;
            new_vm_area->vm_next=ptr;
            struct vm_area * ptr_2=current->vm_area;
            if(ptr_2==ptr){
                ptr->vm_next=new_vm_area;
                if(mflag){
                    mmap_populate(current,new_vm_area->vm_start,length,prot); 
                }
                return new_vm_area->vm_start;
            }
            while(ptr_2->vm_next!=ptr){
                ptr_2=ptr_2->vm_next;
            }
            ptr_2->vm_next=new_vm_area;
            ret_addr=new_vm_area->vm_start;
            if(mflag){
                mmap_populate(current,ret_addr,length,prot); 
            }
            return ret_addr;
        }
    }
}
/**
 * munmap system call implemenations
 */

int vm_area_unmap(struct exec_context *current, u64 addr, int length)
{
    //printk("\nCalling munmap\n");
    if(length%4096){
        length=(length/4096+1)*4096;
    }
    unmap_page(current,addr,length);
    int isValid = -1;
    struct vm_area* ptr=current->vm_area;
    if(!ptr->vm_next && addr<=ptr->vm_start && addr+length>=ptr->vm_end){
        //printk("->Removing 1st vm\n");
        dealloc_vm_area(ptr);
        current->vm_area=NULL;
        return 0;
    }
    while(ptr){
        if(addr>ptr->vm_start && (addr+length)<ptr->vm_end){
            //printk("->VM partitioned.\n");
            struct vm_area* new_vm=alloc_vm_area();
            new_vm->vm_next=ptr->vm_next;
            ptr->vm_next=new_vm;
            new_vm->vm_start=addr+length;
            new_vm->vm_end=ptr->vm_end;
            new_vm->access_flags=ptr->access_flags;
            ptr->vm_end=addr;
            isValid=0;
            break;
        }else if(addr <= ptr->vm_start && addr+length < ptr->vm_end && addr+length > ptr->vm_start){
            //printk("->VM shrinked from forward.\n");
            ptr->vm_start=addr+length;
            isValid=0;
            break;
        }else if(addr+length >= ptr->vm_end && addr > ptr->vm_start && addr < ptr->vm_end){
            //printk("->VM shrinked from backward.\n");
            ptr->vm_end=addr;
            isValid=0;
        }else if(addr <= ptr->vm_start && addr+length >= ptr->vm_end){
            //printk("->Removing a vm.\n");
            struct vm_area* ptr2=current->vm_area;
            while (ptr2->vm_next!=ptr)
            {
                ptr2=ptr2->vm_next;
            }
            ptr2->vm_next=ptr->vm_next;
            dealloc_vm_area(ptr);
            isValid=0;
        }
        ptr=ptr->vm_next;
    }
    if((isValid==-1) && addr){
        //printk("->ERROR\n");
    }
    return isValid;
}