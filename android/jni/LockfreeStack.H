/*
    Author and Copyright: Johannes Gajdosik, 2014

    This file is part of MandelSplit.

    MandelSplit is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    MandelSplit is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with MandelSplit.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef LOCKFREE_STACK_H_
#define LOCKFREE_STACK_H_

template<class Node> // requires Node *Node::next
class LockfreeStack {
#ifdef __x86_64__
typedef __int128 DoubleInt;
typedef long long int SingleInt;
#else
typedef long long int DoubleInt;
typedef int SingleInt;
#endif
public:
  LockfreeStack(void) : head_and_counter(0) {}
  ~LockfreeStack(void) {
//    if (GetNode(head_and_counter)) {int *p=0;*p=0;}
////    for (;;) {
////      Node *const node = popNonThreadSafe();
////      if (node == 0) return;
////      delete node;
////    }
  }
  void clearNonThreadSafe(void) {SetNode(head_and_counter,0);}
  bool empty(void) const {return (GetNode(head_and_counter) == 0);}
  void pushNonThreadSafe(Node *node) {
    node->next = GetNode(head_and_counter);
    SetNode(head_and_counter,node);
  }
  Node *popNonThreadSafe(void) {
    Node *const node = GetNode(head_and_counter);
    if (node) {SetNode(head_and_counter,node->next);}
    return node;
  }
  void pushList(Node *list_head,Node *list_tail) {
    DoubleInt new_head_and_counter;
    SetNode(new_head_and_counter,list_head);
    for (;;) {
      const DoubleInt old_head_and_counter = head_and_counter;
      list_tail->next = GetNode(old_head_and_counter);
      SetCounter(new_head_and_counter,GetCounter(old_head_and_counter)+1);
      if (__sync_bool_compare_and_swap(
              &head_and_counter,
              old_head_and_counter,
              new_head_and_counter)) {
        return;
      }
    }
  }
  void push(Node *node) {
    DoubleInt new_head_and_counter;
    SetNode(new_head_and_counter,node);
    for (;;) {
      const DoubleInt old_head_and_counter = head_and_counter;
      node->next = GetNode(old_head_and_counter);
      SetCounter(new_head_and_counter,GetCounter(old_head_and_counter)+1);
      if (__sync_bool_compare_and_swap(
              &head_and_counter,
              old_head_and_counter,
              new_head_and_counter)) {
        return;
      }
    }
  }
  Node *pop(void) {
    for (;;) {
      const DoubleInt old_head_and_counter = head_and_counter;
      if (GetNode(old_head_and_counter) == 0) return 0;
      DoubleInt new_head_and_counter;
      SetNode(new_head_and_counter,GetNode(old_head_and_counter)->next);
      SetCounter(new_head_and_counter,GetCounter(old_head_and_counter)+1);
      if (__sync_bool_compare_and_swap(
              &head_and_counter,
              old_head_and_counter,
              new_head_and_counter)) {
        return GetNode(old_head_and_counter);
      }
    }
  }
private:
  static Node *GetNode(const DoubleInt &head_and_counter) {
    return reinterpret_cast<Node*const*>(&head_and_counter)[0];
  }
  static SingleInt GetCounter(const DoubleInt &head_and_counter) {
    return reinterpret_cast<const SingleInt*>(&head_and_counter)[1];
  }
  static void SetNode(DoubleInt &head_and_counter,Node *node) {
    reinterpret_cast<void**>(&head_and_counter)[0] = node;
  }
  static void SetCounter(DoubleInt &head_and_counter,SingleInt counter) {
    reinterpret_cast<SingleInt*>(&head_and_counter)[1] = counter;
  }
private:
  DoubleInt head_and_counter; // must be 16byte-aligned
} __attribute__((aligned(64))) ; // 64byte cacheline size on android

#endif
