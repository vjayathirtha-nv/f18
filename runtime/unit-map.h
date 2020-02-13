//===-- runtime/unit-map.h --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Maps Fortran unit numbers to their ExternalFileUnit instances.
// A simple hash table with forward-linked chains per bucket.

#ifndef FORTRAN_RUNTIME_UNIT_MAP_H_
#define FORTRAN_RUNTIME_UNIT_MAP_H_

#include "lock.h"
#include "memory.h"
#include "unit.h"

namespace Fortran::runtime::io {

class UnitMap {
public:
  ExternalFileUnit *LookUp(int n) {
    CriticalSection critical{lock_};
    return Find(n);
  }

  ExternalFileUnit &LookUpOrCreate(
      int n, const Terminator &terminator, bool *wasExtant) {
    CriticalSection critical{lock_};
    auto *p{Find(n)};
    if (wasExtant) {
      *wasExtant = p != nullptr;
    }
    if (p) {
      return *p;
    }
    return Create(n, terminator);
  }

  ExternalFileUnit &NewUnit(const Terminator &terminator) {
    CriticalSection critical{lock_};
    return Create(nextNewUnit_--, terminator);
  }

  // To prevent races, the unit is removed from the map if it exists,
  // and put on the closing_ list until DestroyClosed() is called.
  ExternalFileUnit *LookUpForClose(int);

  void DestroyClosed(ExternalFileUnit &);
  void CloseAll(IoErrorHandler &);

private:
  struct Chain {
    explicit Chain(int n) : unit{n} {}
    ExternalFileUnit unit;
    Chain *next{nullptr};
  };

  static constexpr int buckets_{1031};  // must be prime
  int Hash(int n) { return n % buckets_; }

  ExternalFileUnit *Find(int n) {
    Chain *previous{nullptr};
    int hash{Hash(n)};
    for (Chain *p{bucket_[hash]}; p; previous = p, p = p->next) {
      if (p->unit.unitNumber() == n) {
        if (previous) {
          // Move found unit to front of chain for quicker lookup next time
          previous->next = p->next;
          p->next = bucket_[hash];
          bucket_[hash] = p;
        }
        return &p->unit;
      }
    }
    return nullptr;
  }

  ExternalFileUnit &Create(int, const Terminator &);

  Lock lock_;
  Chain *bucket_[buckets_]{};  // all owned by *this
  int nextNewUnit_{-1000};  // see 12.5.6.12 in Fortran 2018
  Chain *closing_{nullptr};  // units during CLOSE statement
};
}
#endif  // FORTRAN_RUNTIME_UNIT_MAP_H_
