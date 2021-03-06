//===-- runtime/file.h ------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Raw system I/O wrappers

#ifndef FORTRAN_RUNTIME_FILE_H_
#define FORTRAN_RUNTIME_FILE_H_

#include "io-error.h"
#include "lock.h"
#include "memory.h"
#include <cinttypes>
#include <optional>

namespace Fortran::runtime::io {

enum class OpenStatus { Old, New, Scratch, Replace, Unknown };
enum class CloseStatus { Keep, Delete };
enum class Position { AsIs, Rewind, Append };

class OpenFile {
public:
  using FileOffset = std::int64_t;

  Lock &lock() { return lock_; }
  const char *path() const { return path_.get(); }
  void set_path(OwningPtr<char> &&, std::size_t bytes);
  std::size_t pathLength() const { return pathLength_; }
  bool mayRead() const { return mayRead_; }
  void set_mayRead(bool yes) { mayRead_ = yes; }
  bool mayWrite() const { return mayWrite_; }
  void set_mayWrite(bool yes) { mayWrite_ = yes; }
  bool mayAsynchronous() const { return mayAsynchronous_; }
  void set_mayAsynchronous(bool yes) { mayAsynchronous_ = yes; }
  bool mayPosition() const { return mayPosition_; }
  void set_mayPosition(bool yes) { mayPosition_ = yes; }
  FileOffset position() const { return position_; }
  bool isTerminal() const { return isTerminal_; }

  bool IsOpen() const { return fd_ >= 0; }
  void Open(OpenStatus, Position, IoErrorHandler &);
  void Predefine(int fd);
  void Close(CloseStatus, IoErrorHandler &);

  // Reads data into memory; returns amount acquired.  Synchronous.
  // Partial reads (less than minBytes) signify end-of-file.  If the
  // buffer is larger than minBytes, and extra returned data will be
  // preserved for future consumption, set maxBytes larger than minBytes
  // to reduce system calls  This routine handles EAGAIN/EWOULDBLOCK and EINTR.
  std::size_t Read(FileOffset, char *, std::size_t minBytes,
      std::size_t maxBytes, IoErrorHandler &);

  // Writes data.  Synchronous.  Partial writes indicate program-handled
  // error conditions.
  std::size_t Write(FileOffset, const char *, std::size_t, IoErrorHandler &);

  // Truncates the file
  void Truncate(FileOffset, IoErrorHandler &);

  // Asynchronous transfers
  int ReadAsynchronously(FileOffset, char *, std::size_t, IoErrorHandler &);
  int WriteAsynchronously(
      FileOffset, const char *, std::size_t, IoErrorHandler &);
  void Wait(int id, IoErrorHandler &);
  void WaitAll(IoErrorHandler &);

private:
  struct Pending {
    int id;
    int ioStat{0};
    OwningPtr<Pending> next;
  };

  // lock_ must be held for these
  void CheckOpen(const Terminator &);
  bool Seek(FileOffset, IoErrorHandler &);
  bool RawSeek(FileOffset);
  bool RawSeekToEnd();
  int PendingResult(const Terminator &, int);

  Lock lock_;
  int fd_{-1};
  OwningPtr<char> path_;
  std::size_t pathLength_;
  bool mayRead_{false};
  bool mayWrite_{false};
  bool mayPosition_{false};
  bool mayAsynchronous_{false};
  FileOffset position_{0};
  std::optional<FileOffset> knownSize_;
  bool isTerminal_{false};

  int nextId_;
  OwningPtr<Pending> pending_;
};
}
#endif  // FORTRAN_RUNTIME_FILE_H_
