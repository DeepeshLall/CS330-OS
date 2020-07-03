#include <cfork.h>
#include <page.h>
#include <mmap.h>

#define FLAG_MASK 0x3ffffffff000UL 
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12


/* You need to implement cfork_copy_mm which will be called from do_cfork in entry.c. Don't remove copy_os_pts()*/
 void cfork_copy_mm(struct exec_context *child, struct exec_context *parent ){
    
    //printk("\ncfork_copy_mm is called\n");
    struct vm_area* ptr_1=parent->vm_area;
    if(!(parent->vm_area)){
      //printk("->No vm in parent.\n");
      child->vm_area=NULL;
    }else{
      //printk("->vm found in parent\n");
      struct vm_area* ptr_child=child->vm_area;
      child->vm_area=alloc_vm_area();
      child->vm_area->vm_start=parent->vm_area->vm_start;
      child->vm_area->vm_end=parent->vm_area->vm_end;
      child->vm_area->access_flags=parent->vm_area->access_flags;
      ptr_1=ptr_1->vm_next;
      while(ptr_1){
        struct vm_area* ptr_2=alloc_vm_area();
        ptr_2->vm_start=ptr_1->vm_start;
        ptr_2->vm_end=ptr_1->vm_end;
        ptr_2->access_flags=ptr_1->access_flags;
        ptr_child->vm_next=ptr_2;
        ptr_child=ptr_child->vm_next;
        ptr_1=ptr_1->vm_next;
      }
      ptr_child->vm_next=NULL;
    }
    void *os_addr;
    u64 vaddr; 
    struct mm_segment *seg;

    child->pgd = os_pfn_alloc(OS_PT_REG);
    os_addr = osmap(child->pgd);
    bzero((char *)os_addr, PAGE_SIZE);

    //CODE segment
    seg = &parent->mms[MM_SEG_CODE];
    for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
        u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
        if(parent_pte)
            install_ptable((u64) os_addr, seg, vaddr, (*parent_pte & FLAG_MASK) >> PAGE_SHIFT);   
    } 
    //RODATA segment
    
    seg = &parent->mms[MM_SEG_RODATA];
    for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
        u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
        if(parent_pte)
            install_ptable((u64)os_addr, seg, vaddr, (*parent_pte & FLAG_MASK) >> PAGE_SHIFT);   
    } 
    
    //DATA segment
    seg = &parent->mms[MM_SEG_DATA];
    for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
        u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
        
        if(parent_pte){
            struct pfn_info * pfn = get_pfn_info(*parent_pte>>PAGE_SHIFT);
            increment_pfn_info_refcount(pfn);
            u32 upfn = map_physical_page((u64)os_addr,vaddr,1,(*parent_pte & FLAG_MASK)>>PAGE_SHIFT);
            (*parent_pte)=((*parent_pte >>2)<<2)|0x1;
        }
    } 

    //MMAP segment
    struct vm_area* ptr_3=parent->vm_area;
    while(ptr_3){
      int loop_cnt=(ptr_3->vm_end-ptr_3->vm_start)/4096;
      for (int i = 0; i < loop_cnt; i++)
      {
        u64* parent_pte = get_user_pte(parent,ptr_3->vm_start+i*4096,0);
        if(parent_pte){
            struct pfn_info * pfn = get_pfn_info(*parent_pte>>PAGE_SHIFT);
            increment_pfn_info_refcount(pfn);
            u32 upfn = map_physical_page((u64)os_addr,ptr_3->vm_start+i*4096,1,(*parent_pte & FLAG_MASK)>>PAGE_SHIFT);
            (*parent_pte)=((*parent_pte >>2)<<2)|0x1;
        }
      }
     ptr_3=ptr_3->vm_next;
    }

    //STACK segment
    seg = &parent->mms[MM_SEG_STACK];
    for(vaddr = seg->end - PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE){
        u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
        
      if(parent_pte){
            u64 pfn = install_ptable((u64)os_addr, seg, vaddr, 0);  //Returns the blank page  
            pfn = (u64)osmap(pfn);
            memcpy((char *)pfn, (char *)(*parent_pte & FLAG_MASK), PAGE_SIZE); 
        }
    }

    copy_os_pts(parent->pgd, child->pgd);
    return;
    
} 

/* You need to implement cfork_copy_mm which will be called from do_vfork in entry.c.*/
void vfork_copy_mm(struct exec_context *child, struct exec_context *parent ){

  //printk("\nInside vfork\n");
  parent->state=WAITING;
  child->vm_area=parent->vm_area;
  void *os_addr;
    u64 vaddr; 
    struct mm_segment *seg;

    // child->pgd = os_fn_alloc(OS_PT_REG);
    os_addr = osmap(parent->pgd);
    // bzero((char *)os_addr, PAGE_SIZE);
  
  //STACK segment
  //printk("->stack\n");
    seg = &parent->mms[MM_SEG_STACK];
    u64 offset=seg->end-seg->next_free;
    for(vaddr = seg->end - PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE){
        u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
        
      if(parent_pte){
            u64 pfn = install_ptable((u64)os_addr, seg, vaddr-offset, 0);  //Returns the blank page  
            pfn = (u64)osmap(pfn);
            memcpy((char *)pfn, (char *)(*parent_pte & FLAG_MASK), PAGE_SIZE); 
        }
    }
    //printk("-->Shifting reg value by offset\n");
    struct mm_segment *seg_child;
    seg_child = &child->mms[MM_SEG_STACK];
    seg_child->end = seg->end - offset;
    seg_child->next_free = seg->next_free - offset;
    seg_child->start  = seg->start - offset;
    child->regs.rbp = parent->regs.rbp - offset;
    child->regs.entry_rsp = parent->regs.entry_rsp - offset;
    //printk("-->return\n");
  return;
    
}

/*You need to implement handle_cow_fault which will be called from do_page_fault 
incase of a copy-on-write fault

* For valid acess. Map the physical page 
 * Return 1
 * 
 * For invalid access,
 * Return -1. 
*/

void hcf_util(struct exec_context *current, u64 cr2){
  //printk("\nCalling hcf_util()\n");
  void *os_addr;
  u64 *pte =  get_user_pte(current, cr2, 0);
  u64 pfn1 = *pte;
  struct pfn_info * pfn_info = get_pfn_info(*pte>>PAGE_SHIFT);
  u8 ref_count=get_pfn_info_refcount(pfn_info);
  //printk("->ref_count: %d\n",ref_count);
  if(ref_count == 1){
    *pte = *pte | 0x3;
  }else if(ref_count > 1){
    os_addr = osmap(current->pgd);
    u64 pfn = map_physical_page((u64)os_addr, cr2, 3, 0);
    pfn = (u64)osmap(pfn);
    memcpy((char *)pfn, (char *)(pfn1 & FLAG_MASK), PAGE_SIZE); 
    decrement_pfn_info_refcount(pfn_info);
  }
  asm volatile ("invlpg (%0);" 
                    :: "r"(cr2) 
                    : "memory");

  return;
}


int handle_cow_fault(struct exec_context *current, u64 cr2){
  //printk("\nCalled handle_cow_fault\n");
  if(cr2<DATA_START || cr2>MMAP_AREA_END){
    //printk("->cr2 not in valid range\n");
    return -1;
  }else if(cr2>=DATA_START && cr2<MMAP_AREA_START){
    //printk("->in data range\n");
    hcf_util(current,cr2);
    return 1;
  }else{
    //printk("->in vm range\n");
    struct vm_area* ptr=current->vm_area;
    while(ptr){
      if(ptr->vm_start<=cr2 && ptr->vm_end>cr2){
        break;
      }
      ptr=ptr->vm_next;
    }
    if(!ptr){
      //printk("->ptr not found\n");
      return -1;
    }else{
      if((ptr->access_flags & PROT_WRITE)!=PROT_WRITE){
        //printk("->NO prot rights\n");
        return -1;
      }
      hcf_util(current,cr2);
      return 1;
    }
  }
  return -1;
}


/* You need to handle any specific exit case for vfork here, called from do_exit*/
void vfork_exit_handle(struct exec_context *ctx){
  //printk("\nEntrying vfork handle\n");
	struct exec_context * parent = get_ctx_by_pid(ctx->ppid);

  if(ctx->vm_area==NULL){
    //printk("completely deallocated vm_area\n");
  }
  if(!parent){
    //printk("->No parent\n");
    return;
  }
  if(parent->pgd != ctx->pgd){
    //printk("->Not vfork\n");
    return;
  }
  parent->vm_area = ctx->vm_area;
	  u64 vaddr;
    struct mm_segment *seg = &ctx->mms[MM_SEG_STACK];
    for(vaddr = seg->end - PAGE_SIZE; vaddr >= seg->next_free; vaddr-=PAGE_SIZE){
        do_unmap_user(ctx, vaddr);
    }
    ctx->state = EXITING;    
    parent->state = READY;
  return;
}