/*******************************************************************************
 * Copyright (c) 1996, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/***************************************************************************/
/*                                                                         */
/*  File name:  allocator.h                                                */
/*  Purpose:    Definition of the CS2 base allocator                       */
/*                                                                         */
/***************************************************************************/

#ifndef CS2_ALLOCATOR_H
#define CS2_ALLOCATOR_H

#include <stdio.h>
#include <memory.h>
#include "cs2/cs2.h"
#include "env/TypedAllocator.hpp"

namespace CS2 {

  // Basic CS2 allocator class
  // CS2 allocators are per-instance, and should not have any non-static
  // data. Only exception is that they may have a reference to an actual
  // memory pool. Copying an allocator only copies that reference and both
  // objects will share the same memory pool
  class malloc_allocator {
  public:
    void *allocate(size_t size, const char *name=NULL) {
      return malloc(size);
    }
    void deallocate(void *pointer, size_t size, const char *name=NULL) {
      free(pointer);
    }
    void *reallocate(size_t newsize, void *pointer, size_t size, const char *name=NULL) {
      return realloc(pointer, newsize);
    }

    template <class ostr, class allocator> ostr& stats(ostr &o, allocator &a) { return o;}
  };

  template <class base_allocator>
  class shared_allocator {
    base_allocator &base;
  public:
    shared_allocator(base_allocator &b) : base(b) {}

    void *allocate(size_t size, const char *name = NULL) {
        return base.allocate(size);
    }

    void deallocate(void *pointer, size_t size, const char *name = NULL) {
      return base.deallocate(pointer, size);
    }

    void *reallocate(size_t newsize, void *pointer, size_t size, const char *name=NULL) {
      return base.reallocate(newsize, pointer, size);
    }

    shared_allocator & operator = (const shared_allocator & a2 ) {
      // no need to copy the allocator being shared
      return *this;
    }

    // Enable automatic conversion into a form compatible with C++ standard library containers
    template <typename T>
    operator TR::typed_allocator<T, shared_allocator>() { return TR::typed_allocator<T, shared_allocator>(*this); }

    friend bool operator ==(const shared_allocator &left, const shared_allocator &right) { return &left.base == &right.base; }

    friend bool operator !=(const shared_allocator &left, const shared_allocator &right) { return !(operator ==(left, right)); }
  };

  template <size_t segmentsize = 65536, class base_allocator = ::CS2::malloc_allocator>
  class arena_allocator : private base_allocator {

    struct Segment {
      struct Segment *next;
      size_t size;
    };

  public:
    arena_allocator(base_allocator b) : base_allocator(b), segment(NULL), allocated(0) {}
    ~arena_allocator() {
      Segment *s = segment;

      while (s) {
        Segment *next = s->next;
        base_allocator::deallocate(s, s->size);
        s = next;
      }
    }

    static size_t arena_size() { return segmentsize - sizeof(Segment);}

    void *allocate(size_t size, const char *name=NULL) {
      if (size % sizeof(size_t)) size = (size/sizeof(size_t)+1)*sizeof(size_t);

      void *ret;
      if (segment && size>=arena_size()) {
        Segment *new_segment = (Segment *)base_allocator::allocate(sizeof(Segment)+size, name);
        new_segment->size = sizeof(Segment)+size;
        new_segment->next = segment->next;
        segment->next=new_segment;

        ret = (void *)((char *)new_segment + sizeof(Segment));
      } else if (segment==NULL || allocated+size>arena_size()) {

        Segment *new_segment = (Segment *)base_allocator::allocate(segmentsize, name);
        new_segment->size = segmentsize;
        new_segment->next = segment;

        segment = new_segment;
        ret = (void*) ((char *)new_segment + sizeof(Segment));
        allocated = size;
      } else {
        ret = (void*) ((char *)segment + sizeof(Segment) + allocated);
        allocated+=size;
      }
      return ret;
    }

    void deallocate(void *pointer, size_t size, const char *name=NULL) {
      // no deallocation
    }

    void *reallocate(size_t newsize, void *pointer, size_t size, const char *name=NULL) {
      if (newsize<=size) return pointer;
      void *ret = allocate(newsize, name);
      memcpy(ret, pointer, size);
      return ret;
    }

    arena_allocator & operator = (const arena_allocator & a2 ) {
      // no need to copy the allocator being shared
      return *this;
    }

  private:
    Segment *segment;
    size_t allocated;
  };
}

#endif // CS2_ALLOCATOR_H
