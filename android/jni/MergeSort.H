// Author and Copyright:  Johannes Gajdosik, 1998,2002
// License:               GNU Lesser General Public License
// Please get your copy of the GNU LGPL from http://www.gnu.org


#ifndef _MERGE_SORT_H_
#define _MERGE_SORT_H_

#include <iostream>

// NextListNode muss eine Membervariable next vom typ NextListNode* haben.
// Eine solche einfach verkettete Liste wird von dieser Funktion nach dem
// angegebenen Kriterium sortiert.
// Die Laenge der Liste wird zurueckgegeben.

template <class NextListNode>
unsigned int MergeSort(NextListNode *&list,
                       bool (&is_less)(const NextListNode&,
                                       const NextListNode&,
                                       const void *user_data),
                       const void *user_data) {
    // The first step of the sorting process
    // is different from the other steps:
  NextListNode *list1_head; // needs no initialization
  NextListNode **list1_tail_addr = &list1_head;
  NextListNode *list2_head = 0;
  NextListNode **list2_tail_addr = &list2_head;
  unsigned int length = 0;
  NextListNode *head = list;
  for (;;) {
    if (head == 0) break;
    NextListNode *p1 = head; head = head->next; ++length;
    if (head == 0) {
      *list1_tail_addr = p1; list1_tail_addr = &(p1->next);
      break;
    }
    NextListNode *p2 = head; head = head->next; ++length;
    if (is_less(*p1,*p2,user_data)) {
      *list1_tail_addr = p1; list1_tail_addr = &(p1->next);
      *list1_tail_addr = p2; list1_tail_addr = &(p2->next);
    } else {
      *list1_tail_addr = p2; list1_tail_addr = &(p2->next);
      *list1_tail_addr = p1; list1_tail_addr = &(p1->next);
    }
    if (head == 0) break;
    p1 = head; head = head->next; ++length;
    if (head == 0) {
      *list2_tail_addr = p1; list2_tail_addr = &(p1->next);
      break;
    }
    p2 = head; head = head->next; ++length;
    if (is_less(*p1,*p2,user_data)) {
      *list2_tail_addr = p1; list2_tail_addr = &(p1->next);
      *list2_tail_addr = p2; list2_tail_addr = &(p2->next);
    } else {
      *list2_tail_addr = p2; list2_tail_addr = &(p2->next);
      *list2_tail_addr = p1; list2_tail_addr = &(p1->next);
    }
  }
  *list1_tail_addr = 0;
  
  unsigned int ld_blocksize = 0;
  unsigned int block_size = 1;
  unsigned int total_whole_blocks = length;

//cout << "MergeSort: initial step done. length = " << total_whole_blocks
//     << endl;

  
    // Now we know the total number of NextListNodes: length.
    // The list has been split into 2 lists: list1, list2.
    // The length of list1 is (length+1)/2,
    // the length of list2 is length/2.
    // Both list1 and list2 contain sublists of length 2 which are sorted.
  
  while (list2_head) {
    ++ld_blocksize;
    block_size <<= 1;
    total_whole_blocks >>= 1;
      // Calculate how long list1 and list2 is.
      // This is better than continuousely incrementing counters.
    unsigned int list1_length,list2_length;
    if (total_whole_blocks & 1) {
      list1_length = ((total_whole_blocks+1)>>1) << ld_blocksize;
      list2_length = length - list1_length;
    } else {
      list2_length = (total_whole_blocks>>1) << ld_blocksize;
      list1_length = length - list2_length;
    }


//cout << "MergeSort: starting step " << ld_blocksize
//     << ". block_size = " << block_size
//     << ", total_whole_blocks = " << total_whole_blocks
//     << ", list1_length = " << list1_length
//     << ", list2_length = " << list2_length
//     << endl;

      // The list has been split into 2 lists: list1, list2.
      // The length of list1 is list1_length,
      // the length of list2 is list2_length.
      // Both list1 and list2 contain sublists of length block_size
      // which are sorted.

    NextListNode *list3_head; // needs no initialization
    NextListNode **list3_tail_addr = &list3_head;
    NextListNode *list4_head = 0;
    NextListNode **list4_tail_addr = &list4_head;
    
      // list1 is not shorter than list2,
      // list2 contains at least one node.

    for (;;) {
        // Sort the blocks into list3
      unsigned int remaining1 = (list1_length<block_size)
                              ? list1_length : block_size;
      list1_length -= remaining1;
      unsigned int remaining2 = (list2_length<block_size)
                              ? list2_length : block_size;
      list2_length -= remaining2;

//cout << "MergeSort step " << ld_blocksize << ": "
//     << "merging " << remaining1 << " nodes of list1 "
//     << " and " << remaining2 << " nodes of list2 "
//     << " into list3"
//     << endl;
        
        // merge the two blocks of lengths remaining1/remaining2
        // into one single block in list3:
      for (;;) {
          // remaining1>0 && list1_head!=0 && remaining2>0 && list2_head!=0
        if (is_less(*list1_head,*list2_head,user_data)) {
          *list3_tail_addr = list1_head;
          list3_tail_addr = &(list1_head->next);
          list1_head = *list3_tail_addr;
          if (--remaining1 == 0) {
            do {
              *list3_tail_addr = list2_head;
              list3_tail_addr = &(list2_head->next);
              list2_head = *list3_tail_addr;
            } while (--remaining2 > 0);
            break;
          }
        } else {
          *list3_tail_addr = list2_head;
          list3_tail_addr = &(list2_head->next);
          list2_head = *list3_tail_addr;
          if (--remaining2 == 0) {
            do {
              *list3_tail_addr = list1_head;
              list3_tail_addr = &(list1_head->next);
              list1_head = *list3_tail_addr;
            } while (--remaining1 > 0);
            break;
          }
        }
      }
//cout << "MergeSort step " << ld_blocksize << ": "
//     << "merging done: list1 " << list1_length
//     << ", list2 " << list2_length
//     << endl;
      if (list1_length == 0) break;
      if (list2_length == 0) {
          // Sort the remaining blocks of list1 into list4:
//cout << "MergeSort step " << ld_blocksize << ": "
//     << "copying " << list1_length << " nodes of list1 "
//     << " into list4"
//     << endl;
        do {
          *list4_tail_addr = list1_head;
          list4_tail_addr = &(list1_head->next);
          list1_head = *list4_tail_addr;
        } while (--list1_length > 0);
        break;
      }
      
        // Sort the blocks into list4
      remaining1 = (list1_length<block_size) ? list1_length : block_size;
      list1_length -= remaining1;
      remaining2 = (list2_length<block_size) ? list2_length : block_size;
      list2_length -= remaining2;

//cout << "MergeSort step " << ld_blocksize << ": "
//     << "merging " << remaining1 << " nodes of list1 "
//     << " and " << remaining2 << " nodes of list2 "
//     << " into list4"
//     << endl;
        
        // merge the two blocks of lengths remaining1/remaining2
        // into one single block in list4:
      for (;;) {
          // remaining1>0 && list1_head!=0 && remaining2>0 && list2_head!=0
        if (is_less(*list1_head,*list2_head,user_data)) {
          *list4_tail_addr = list1_head;
          list4_tail_addr = &(list1_head->next);
          list1_head = *list4_tail_addr;
          if (--remaining1 == 0) {
            do {
              *list4_tail_addr = list2_head;
              list4_tail_addr = &(list2_head->next);
              list2_head = *list4_tail_addr;
            } while (--remaining2 > 0);
            break;
          }
        } else {
          *list4_tail_addr = list2_head;
          list4_tail_addr = &(list2_head->next);
          list2_head = *list4_tail_addr;
          if (--remaining2 == 0) {
            do {
              *list4_tail_addr = list1_head;
              list4_tail_addr = &(list1_head->next);
              list1_head = *list4_tail_addr;
            } while (--remaining1 > 0);
            break;
          }
        }
      }
//cout << "MergeSort step " << ld_blocksize << ": "
//     << "merging done: list1 " << list1_length
//     << ", list2 " << list2_length
//     << endl;
      if (list1_length == 0) break;
      if (list2_length == 0) {
          // Sort the remaining blocks of list1 into list3:
//cout << "MergeSort step " << ld_blocksize << ": "
//     << "copying " << list1_length << " nodes of list1 "
//     << " into list3"
//     << endl;
        do {
          *list3_tail_addr = list1_head;
          list3_tail_addr = &(list1_head->next);
          list1_head = *list3_tail_addr;
        } while (--list1_length > 0);
        break;
      }
      
    }

    *list3_tail_addr = 0;
    list1_head = list3_head;
    list2_head = list4_head;
  }
  list = list1_head;
//cout << "MergeSort returns " << length << endl;
  return length;
}

template <class PrevNextListNode>
void AssignPrevAndTail(PrevNextListNode *head,PrevNextListNode *&tail) {
  if (head) {
    head->prev = 0;
    PrevNextListNode *t = head->next;
    while (t) {
      t->prev = head;
      head = t;
      t = head->next;
    }
  }
  tail = head;
}

#endif
