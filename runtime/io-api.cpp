//===-- runtime/io-api.cpp --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Implements the I/O statement API

#include "io-api.h"
#include "environment.h"
#include "format.h"
#include "io-stmt.h"
#include "memory.h"
#include "numeric-input.h"
#include "numeric-output.h"
#include "terminator.h"
#include "tools.h"
#include "unit.h"
#include <cstdlib>
#include <memory>

namespace Fortran::runtime::io {

template<bool isInput>
Cookie BeginInternalArrayListIO(const Descriptor &descriptor,
    void ** /*scratchArea*/, std::size_t /*scratchBytes*/,
    const char *sourceFile, int sourceLine) {
  Terminator oom{sourceFile, sourceLine};
  return &New<InternalListIoStatementState<isInput>>{}(
      oom, descriptor, sourceFile, sourceLine)
              .ioStatementState();
}

Cookie IONAME(BeginInternalArrayListOutput)(const Descriptor &descriptor,
    void **scratchArea, std::size_t scratchBytes, const char *sourceFile,
    int sourceLine) {
  return BeginInternalArrayListIO<false>(
      descriptor, scratchArea, scratchBytes, sourceFile, sourceLine);
}

Cookie IONAME(BeginInternalArrayListInput)(const Descriptor &descriptor,
    void **scratchArea, std::size_t scratchBytes, const char *sourceFile,
    int sourceLine) {
  return BeginInternalArrayListIO<true>(
      descriptor, scratchArea, scratchBytes, sourceFile, sourceLine);
}

template<bool isInput>
Cookie BeginInternalArrayFormattedIO(const Descriptor &descriptor,
    const char *format, std::size_t formatLength, void ** /*scratchArea*/,
    std::size_t /*scratchBytes*/, const char *sourceFile, int sourceLine) {
  Terminator oom{sourceFile, sourceLine};
  return &New<InternalFormattedIoStatementState<isInput>>{}(
      oom, descriptor, format, formatLength, sourceFile, sourceLine)
              .ioStatementState();
}

Cookie IONAME(BeginInternalArrayFormattedOutput)(const Descriptor &descriptor,
    const char *format, std::size_t formatLength, void **scratchArea,
    std::size_t scratchBytes, const char *sourceFile, int sourceLine) {
  return BeginInternalArrayFormattedIO<false>(descriptor, format, formatLength,
      scratchArea, scratchBytes, sourceFile, sourceLine);
}

Cookie IONAME(BeginInternalArrayFormattedInput)(const Descriptor &descriptor,
    const char *format, std::size_t formatLength, void **scratchArea,
    std::size_t scratchBytes, const char *sourceFile, int sourceLine) {
  return BeginInternalArrayFormattedIO<true>(descriptor, format, formatLength,
      scratchArea, scratchBytes, sourceFile, sourceLine);
}

template<bool isInput>
Cookie BeginInternalFormattedIO(char *internal, std::size_t internalLength,
    const char *format, std::size_t formatLength, void ** /*scratchArea*/,
    std::size_t /*scratchBytes*/, const char *sourceFile, int sourceLine) {
  Terminator oom{sourceFile, sourceLine};
  return &New<InternalFormattedIoStatementState<isInput>>{}(oom, internal,
      internalLength, format, formatLength, sourceFile, sourceLine)
              .ioStatementState();
}

Cookie IONAME(BeginInternalFormattedOutput)(char *internal,
    std::size_t internalLength, const char *format, std::size_t formatLength,
    void **scratchArea, std::size_t scratchBytes, const char *sourceFile,
    int sourceLine) {
  Terminator oom{sourceFile, sourceLine};
  return BeginInternalFormattedIO<false>(internal, internalLength, format,
      formatLength, scratchArea, scratchBytes, sourceFile, sourceLine);
}

Cookie IONAME(BeginInternalFormattedInput)(char *internal,
    std::size_t internalLength, const char *format, std::size_t formatLength,
    void **scratchArea, std::size_t scratchBytes, const char *sourceFile,
    int sourceLine) {
  Terminator oom{sourceFile, sourceLine};
  return BeginInternalFormattedIO<true>(internal, internalLength, format,
      formatLength, scratchArea, scratchBytes, sourceFile, sourceLine);
}

template<bool isInput>
Cookie BeginExternalListIO(
    ExternalUnit unitNumber, const char *sourceFile, int sourceLine) {
  Terminator terminator{sourceFile, sourceLine};
  if (unitNumber == DefaultUnit) {
    unitNumber = isInput ? 5 : 6;
  }
  ExternalFileUnit &unit{
      ExternalFileUnit::LookUpOrCrash(unitNumber, terminator)};
  if (unit.access == Access::Direct) {
    terminator.Crash("List-directed I/O attempted on direct access file");
    return nullptr;
  }
  if (unit.isUnformatted) {
    terminator.Crash("List-directed I/O attempted on unformatted file");
    return nullptr;
  }
  IoStatementState &io{
      unit.BeginIoStatement<ExternalListIoStatementState<isInput>>(
          unit, sourceFile, sourceLine)};
  if constexpr (isInput) {
    io.AdvanceRecord();
  }
  return &io;
}

Cookie IONAME(BeginExternalListOutput)(
    ExternalUnit unitNumber, const char *sourceFile, int sourceLine) {
  return BeginExternalListIO<false>(unitNumber, sourceFile, sourceLine);
}

Cookie IONAME(BeginExternalListInput)(
    ExternalUnit unitNumber, const char *sourceFile, int sourceLine) {
  return BeginExternalListIO<true>(unitNumber, sourceFile, sourceLine);
}

template<bool isInput>
Cookie BeginExternalFormattedIO(const char *format, std::size_t formatLength,
    ExternalUnit unitNumber, const char *sourceFile, int sourceLine) {
  Terminator terminator{sourceFile, sourceLine};
  if (unitNumber == DefaultUnit) {
    unitNumber = isInput ? 5 : 6;
  }
  ExternalFileUnit &unit{
      ExternalFileUnit::LookUpOrCrash(unitNumber, terminator)};
  if (unit.isUnformatted) {
    terminator.Crash("Formatted I/O attempted on unformatted file");
    return nullptr;
  }
  IoStatementState &io{
      unit.BeginIoStatement<ExternalFormattedIoStatementState<isInput>>(
          unit, format, formatLength, sourceFile, sourceLine)};
  if constexpr (isInput) {
    io.AdvanceRecord();
  }
  return &io;
}

Cookie IONAME(BeginExternalFormattedOutput)(const char *format,
    std::size_t formatLength, ExternalUnit unitNumber, const char *sourceFile,
    int sourceLine) {
  return BeginExternalFormattedIO<false>(
      format, formatLength, unitNumber, sourceFile, sourceLine);
}

Cookie IONAME(BeginExternalFormattedInput)(const char *format,
    std::size_t formatLength, ExternalUnit unitNumber, const char *sourceFile,
    int sourceLine) {
  return BeginExternalFormattedIO<true>(
      format, formatLength, unitNumber, sourceFile, sourceLine);
}

template<bool isInput>
Cookie BeginUnformattedIO(
    ExternalUnit unitNumber, const char *sourceFile, int sourceLine) {
  Terminator terminator{sourceFile, sourceLine};
  ExternalFileUnit &file{
      ExternalFileUnit::LookUpOrCrash(unitNumber, terminator)};
  if (!file.isUnformatted) {
    terminator.Crash("Unformatted output attempted on formatted file");
  }
  IoStatementState &io{
      file.BeginIoStatement<UnformattedIoStatementState<isInput>>(
          file, sourceFile, sourceLine)};
  if constexpr (isInput) {
    io.AdvanceRecord();
  } else {
    if (file.access == Access::Sequential && !file.recordLength.has_value()) {
      // Create space for (sub)record header to be completed by
      // UnformattedIoStatementState<false>::EndIoStatement()
      io.Emit("\0\0\0\0", 4);  // placeholder for record length header
    }
  }
  return &io;
}

Cookie IONAME(BeginUnformattedOutput)(
    ExternalUnit unitNumber, const char *sourceFile, int sourceLine) {
  return BeginUnformattedIO<false>(unitNumber, sourceFile, sourceLine);
}

Cookie IONAME(BeginUnformattedInput)(
    ExternalUnit unitNumber, const char *sourceFile, int sourceLine) {
  return BeginUnformattedIO<true>(unitNumber, sourceFile, sourceLine);
}

Cookie IONAME(BeginOpenUnit)(  // OPEN(without NEWUNIT=)
    ExternalUnit unitNumber, const char *sourceFile, int sourceLine) {
  bool wasExtant{false};
  Terminator terminator{sourceFile, sourceLine};
  ExternalFileUnit &unit{
      ExternalFileUnit::LookUpOrCreate(unitNumber, terminator, &wasExtant)};
  return &unit.BeginIoStatement<OpenStatementState>(
      unit, wasExtant, sourceFile, sourceLine);
}

Cookie IONAME(BeginOpenNewUnit)(  // OPEN(NEWUNIT=j)
    const char *sourceFile, int sourceLine) {
  Terminator terminator{sourceFile, sourceLine};
  return IONAME(BeginOpenUnit)(
      ExternalFileUnit::NewUnit(terminator), sourceFile, sourceLine);
}

Cookie IONAME(BeginClose)(
    ExternalUnit unitNumber, const char *sourceFile, int sourceLine) {
  if (ExternalFileUnit * unit{ExternalFileUnit::LookUpForClose(unitNumber)}) {
    return &unit->BeginIoStatement<CloseStatementState>(
        *unit, sourceFile, sourceLine);
  } else {
    // CLOSE(UNIT=bad unit) is just a no-op
    Terminator oom{sourceFile, sourceLine};
    return &New<NoopCloseStatementState>{}(oom, sourceFile, sourceLine)
                .ioStatementState();
  }
}

// Control list items

void IONAME(EnableHandlers)(Cookie cookie, bool hasIoStat, bool hasErr,
    bool hasEnd, bool hasEor, bool hasIoMsg) {
  IoErrorHandler &handler{cookie->GetIoErrorHandler()};
  if (hasIoStat) {
    handler.HasIoStat();
  }
  if (hasErr) {
    handler.HasErrLabel();
  }
  if (hasEnd) {
    handler.HasEndLabel();
  }
  if (hasEor) {
    handler.HasEorLabel();
  }
  if (hasIoMsg) {
    handler.HasIoMsg();
  }
}

static bool YesOrNo(const char *keyword, std::size_t length, const char *what,
    IoErrorHandler &handler) {
  static const char *keywords[]{"YES", "NO", nullptr};
  switch (IdentifyValue(keyword, length, keywords)) {
  case 0: return true;
  case 1: return false;
  default:
    handler.SignalError(IoErrorInKeyword, "Invalid %s='%.*s'", what,
        static_cast<int>(length), keyword);
    return false;
  }
}

bool IONAME(SetAdvance)(
    Cookie cookie, const char *keyword, std::size_t length) {
  IoStatementState &io{*cookie};
  ConnectionState &connection{io.GetConnectionState()};
  connection.nonAdvancing =
      !YesOrNo(keyword, length, "ADVANCE", io.GetIoErrorHandler());
  if (connection.nonAdvancing && connection.access == Access::Direct) {
    io.GetIoErrorHandler().SignalError(
        "Non-advancing I/O attempted on direct access file");
  }
  return true;
}

bool IONAME(SetBlank)(Cookie cookie, const char *keyword, std::size_t length) {
  IoStatementState &io{*cookie};
  ConnectionState &connection{io.GetConnectionState()};
  static const char *keywords[]{"NULL", "ZERO", nullptr};
  switch (IdentifyValue(keyword, length, keywords)) {
  case 0: connection.modes.editingFlags &= ~blankZero; return true;
  case 1: connection.modes.editingFlags |= blankZero; return true;
  default:
    io.GetIoErrorHandler().SignalError(IoErrorInKeyword, "Invalid BLANK='%.*s'",
        static_cast<int>(length), keyword);
    return false;
  }
}

bool IONAME(SetDecimal)(
    Cookie cookie, const char *keyword, std::size_t length) {
  IoStatementState &io{*cookie};
  ConnectionState &connection{io.GetConnectionState()};
  static const char *keywords[]{"COMMA", "POINT", nullptr};
  switch (IdentifyValue(keyword, length, keywords)) {
  case 0: connection.modes.editingFlags |= decimalComma; return true;
  case 1: connection.modes.editingFlags &= ~decimalComma; return true;
  default:
    io.GetIoErrorHandler().SignalError(IoErrorInKeyword,
        "Invalid DECIMAL='%.*s'", static_cast<int>(length), keyword);
    return false;
  }
}

bool IONAME(SetDelim)(Cookie cookie, const char *keyword, std::size_t length) {
  IoStatementState &io{*cookie};
  ConnectionState &connection{io.GetConnectionState()};
  static const char *keywords[]{"APOSTROPHE", "QUOTE", "NONE", nullptr};
  switch (IdentifyValue(keyword, length, keywords)) {
  case 0: connection.modes.delim = '\''; return true;
  case 1: connection.modes.delim = '"'; return true;
  case 2: connection.modes.delim = '\0'; return true;
  default:
    io.GetIoErrorHandler().SignalError(IoErrorInKeyword, "Invalid DELIM='%.*s'",
        static_cast<int>(length), keyword);
    return false;
  }
}

bool IONAME(SetPad)(Cookie cookie, const char *keyword, std::size_t length) {
  IoStatementState &io{*cookie};
  ConnectionState &connection{io.GetConnectionState()};
  connection.modes.pad =
      YesOrNo(keyword, length, "PAD", io.GetIoErrorHandler());
  return true;
}

bool IONAME(SetPos)(Cookie cookie, std::int64_t pos) {
  IoStatementState &io{*cookie};
  ConnectionState &connection{io.GetConnectionState()};
  if (connection.access != Access::Stream) {
    io.GetIoErrorHandler().SignalError(
        "REC= may not appear unless ACCESS='STREAM'");
    return false;
  }
  if (pos < 1) {
    io.GetIoErrorHandler().SignalError(
        "POS=%zd is invalid", static_cast<std::intmax_t>(pos));
    return false;
  }
  connection.offsetInFile = pos;
  return true;
}

bool IONAME(SetRec)(Cookie cookie, std::int64_t rec) {
  IoStatementState &io{*cookie};
  ConnectionState &connection{io.GetConnectionState()};
  if (connection.access != Access::Direct) {
    io.GetIoErrorHandler().SignalError(
        "REC= may not appear unless ACCESS='DIRECT'");
    return false;
  }
  if (!connection.recordLength) {
    io.GetIoErrorHandler().SignalError("RECL= was not specified");
    return false;
  }
  if (rec < 1) {
    io.GetIoErrorHandler().SignalError(
        "REC=%zd is invalid", static_cast<std::intmax_t>(rec));
    return false;
  }
  connection.currentRecordNumber = rec;
  connection.offsetInFile = rec * *connection.recordLength;
  return true;
}

bool IONAME(SetRound)(Cookie cookie, const char *keyword, std::size_t length) {
  IoStatementState &io{*cookie};
  ConnectionState &connection{io.GetConnectionState()};
  static const char *keywords[]{"UP", "DOWN", "ZERO", "NEAREST", "COMPATIBLE",
      "PROCESSOR_DEFINED", nullptr};
  switch (IdentifyValue(keyword, length, keywords)) {
  case 0: connection.modes.round = decimal::RoundUp; return true;
  case 1: connection.modes.round = decimal::RoundDown; return true;
  case 2: connection.modes.round = decimal::RoundToZero; return true;
  case 3: connection.modes.round = decimal::RoundNearest; return true;
  case 4: connection.modes.round = decimal::RoundCompatible; return true;
  case 5:
    connection.modes.round = executionEnvironment.defaultOutputRoundingMode;
    return true;
  default:
    io.GetIoErrorHandler().SignalError(IoErrorInKeyword, "Invalid ROUND='%.*s'",
        static_cast<int>(length), keyword);
    return false;
  }
}

bool IONAME(SetSign)(Cookie cookie, const char *keyword, std::size_t length) {
  IoStatementState &io{*cookie};
  ConnectionState &connection{io.GetConnectionState()};
  static const char *keywords[]{"PLUS", "YES", "PROCESSOR_DEFINED", nullptr};
  switch (IdentifyValue(keyword, length, keywords)) {
  case 0: connection.modes.editingFlags |= signPlus; return true;
  case 1:
  case 2:  // processor default is SS
    connection.modes.editingFlags &= ~signPlus;
    return true;
  default:
    io.GetIoErrorHandler().SignalError(IoErrorInKeyword, "Invalid SIGN='%.*s'",
        static_cast<int>(length), keyword);
    return false;
  }
}

bool IONAME(SetAccess)(Cookie cookie, const char *keyword, std::size_t length) {
  IoStatementState &io{*cookie};
  auto *open{io.get_if<OpenStatementState>()};
  if (!open) {
    io.GetIoErrorHandler().Crash(
        "SetAccess() called when not in an OPEN statement");
  }
  ConnectionState &connection{open->GetConnectionState()};
  Access access{connection.access};
  static const char *keywords[]{"SEQUENTIAL", "DIRECT", "STREAM", nullptr};
  switch (IdentifyValue(keyword, length, keywords)) {
  case 0: access = Access::Sequential; break;
  case 1: access = Access::Direct; break;
  case 2: access = Access::Stream; break;
  default:
    open->SignalError(IoErrorInKeyword, "Invalid ACCESS='%.*s'",
        static_cast<int>(length), keyword);
  }
  if (access != connection.access) {
    if (open->wasExtant()) {
      open->SignalError("ACCESS= may not be changed on an open unit");
    }
    connection.access = access;
  }
  return true;
}

bool IONAME(SetAction)(Cookie cookie, const char *keyword, std::size_t length) {
  IoStatementState &io{*cookie};
  auto *open{io.get_if<OpenStatementState>()};
  if (!open) {
    io.GetIoErrorHandler().Crash(
        "SetAction() called when not in an OPEN statement");
  }
  bool mayRead{true};
  bool mayWrite{true};
  static const char *keywords[]{"READ", "WRITE", "READWRITE", nullptr};
  switch (IdentifyValue(keyword, length, keywords)) {
  case 0: mayWrite = false; break;
  case 1: mayRead = false; break;
  case 2: break;
  default:
    open->SignalError(IoErrorInKeyword, "Invalid ACTION='%.*s'",
        static_cast<int>(length), keyword);
    return false;
  }
  if (mayRead != open->unit().mayRead() ||
      mayWrite != open->unit().mayWrite()) {
    if (open->wasExtant()) {
      open->SignalError("ACTION= may not be changed on an open unit");
    }
    open->unit().set_mayRead(mayRead);
    open->unit().set_mayWrite(mayWrite);
  }
  return true;
}

bool IONAME(SetAsynchronous)(
    Cookie cookie, const char *keyword, std::size_t length) {
  IoStatementState &io{*cookie};
  auto *open{io.get_if<OpenStatementState>()};
  if (!open) {
    io.GetIoErrorHandler().Crash(
        "SetAsynchronous() called when not in an OPEN statement");
  }
  static const char *keywords[]{"YES", "NO", nullptr};
  switch (IdentifyValue(keyword, length, keywords)) {
  case 0: open->unit().set_mayAsynchronous(true); return true;
  case 1: open->unit().set_mayAsynchronous(false); return true;
  default:
    open->SignalError(IoErrorInKeyword, "Invalid ASYNCHRONOUS='%.*s'",
        static_cast<int>(length), keyword);
    return false;
  }
}

bool IONAME(SetEncoding)(
    Cookie cookie, const char *keyword, std::size_t length) {
  IoStatementState &io{*cookie};
  auto *open{io.get_if<OpenStatementState>()};
  if (!open) {
    io.GetIoErrorHandler().Crash(
        "SetEncoding() called when not in an OPEN statement");
  }
  bool isUTF8{false};
  static const char *keywords[]{"UTF-8", "DEFAULT", nullptr};
  switch (IdentifyValue(keyword, length, keywords)) {
  case 0: isUTF8 = true; break;
  case 1: isUTF8 = false; break;
  default:
    open->SignalError(IoErrorInKeyword, "Invalid ENCODING='%.*s'",
        static_cast<int>(length), keyword);
  }
  if (isUTF8 != open->unit().isUTF8) {
    if (open->wasExtant()) {
      open->SignalError("ENCODING= may not be changed on an open unit");
    }
    open->unit().isUTF8 = isUTF8;
  }
  return true;
}

bool IONAME(SetForm)(Cookie cookie, const char *keyword, std::size_t length) {
  IoStatementState &io{*cookie};
  auto *open{io.get_if<OpenStatementState>()};
  if (!open) {
    io.GetIoErrorHandler().Crash(
        "SetEncoding() called when not in an OPEN statement");
  }
  bool isUnformatted{false};
  static const char *keywords[]{"FORMATTED", "UNFORMATTED", nullptr};
  switch (IdentifyValue(keyword, length, keywords)) {
  case 0: isUnformatted = false; break;
  case 1: isUnformatted = true; break;
  default:
    open->SignalError(IoErrorInKeyword, "Invalid FORM='%.*s'",
        static_cast<int>(length), keyword);
  }
  if (isUnformatted != open->unit().isUnformatted) {
    if (open->wasExtant()) {
      open->SignalError("FORM= may not be changed on an open unit");
    }
    open->unit().isUnformatted = isUnformatted;
  }
  return true;
}

bool IONAME(SetPosition)(
    Cookie cookie, const char *keyword, std::size_t length) {
  IoStatementState &io{*cookie};
  auto *open{io.get_if<OpenStatementState>()};
  if (!open) {
    io.GetIoErrorHandler().Crash(
        "SetPosition() called when not in an OPEN statement");
  }
  static const char *positions[]{"ASIS", "REWIND", "APPEND", nullptr};
  switch (IdentifyValue(keyword, length, positions)) {
  case 0: open->set_position(Position::AsIs); return true;
  case 1: open->set_position(Position::Rewind); return true;
  case 2: open->set_position(Position::Append); return true;
  default:
    io.GetIoErrorHandler().SignalError(IoErrorInKeyword,
        "Invalid POSITION='%.*s'", static_cast<int>(length), keyword);
  }
  return true;
}

bool IONAME(SetRecl)(Cookie cookie, std::size_t n) {
  IoStatementState &io{*cookie};
  auto *open{io.get_if<OpenStatementState>()};
  if (!open) {
    io.GetIoErrorHandler().Crash(
        "SetRecl() called when not in an OPEN statement");
  }
  if (n <= 0) {
    io.GetIoErrorHandler().SignalError("RECL= must be greater than zero");
  }
  if (open->wasExtant() && open->unit().recordLength.has_value() &&
      *open->unit().recordLength != static_cast<std::int64_t>(n)) {
    open->SignalError("RECL= may not be changed for an open unit");
  }
  open->unit().recordLength = n;
  return true;
}

bool IONAME(SetStatus)(Cookie cookie, const char *keyword, std::size_t length) {
  IoStatementState &io{*cookie};
  if (auto *open{io.get_if<OpenStatementState>()}) {
    static const char *statuses[]{
        "OLD", "NEW", "SCRATCH", "REPLACE", "UNKNOWN", nullptr};
    switch (IdentifyValue(keyword, length, statuses)) {
    case 0: open->set_status(OpenStatus::Old); return true;
    case 1: open->set_status(OpenStatus::New); return true;
    case 2: open->set_status(OpenStatus::Scratch); return true;
    case 3: open->set_status(OpenStatus::Replace); return true;
    case 4: open->set_status(OpenStatus::Unknown); return true;
    default:
      io.GetIoErrorHandler().SignalError(IoErrorInKeyword,
          "Invalid STATUS='%.*s'", static_cast<int>(length), keyword);
    }
    return false;
  }
  if (auto *close{io.get_if<CloseStatementState>()}) {
    static const char *statuses[]{"KEEP", "DELETE", nullptr};
    switch (IdentifyValue(keyword, length, statuses)) {
    case 0: close->set_status(CloseStatus::Keep); return true;
    case 1: close->set_status(CloseStatus::Delete); return true;
    default:
      io.GetIoErrorHandler().SignalError(IoErrorInKeyword,
          "Invalid STATUS='%.*s'", static_cast<int>(length), keyword);
    }
    return false;
  }
  if (io.get_if<NoopCloseStatementState>()) {
    return true;  // don't bother validating STATUS= in a no-op CLOSE
  }
  io.GetIoErrorHandler().Crash(
      "SetStatus() called when not in an OPEN or CLOSE statement");
}

bool IONAME(SetFile)(
    Cookie cookie, const char *path, std::size_t chars, int kind) {
  IoStatementState &io{*cookie};
  if (auto *open{io.get_if<OpenStatementState>()}) {
    open->set_path(path, chars, kind);
    return true;
  }
  io.GetIoErrorHandler().Crash(
      "SetFile() called when not in an OPEN statement");
  return false;
}

static bool SetInteger(int &x, int kind, int value) {
  switch (kind) {
  case 1: reinterpret_cast<std::int8_t &>(x) = value; return true;
  case 2: reinterpret_cast<std::int16_t &>(x) = value; return true;
  case 4: x = value; return true;
  case 8: reinterpret_cast<std::int64_t &>(x) = value; return true;
  default: return false;
  }
}

bool IONAME(GetNewUnit)(Cookie cookie, int &unit, int kind) {
  IoStatementState &io{*cookie};
  auto *open{io.get_if<OpenStatementState>()};
  if (!open) {
    io.GetIoErrorHandler().Crash(
        "GetNewUnit() called when not in an OPEN statement");
  }
  if (!SetInteger(unit, kind, open->unit().unitNumber())) {
    open->SignalError("GetNewUnit(): Bad INTEGER kind(%d) for result");
  }
  return true;
}

// Data transfers
// TODO: Input

bool IONAME(OutputDescriptor)(Cookie cookie, const Descriptor &) {
  IoStatementState &io{*cookie};
  io.GetIoErrorHandler().Crash(
      "OutputDescriptor: not yet implemented");  // TODO
}

bool IONAME(OutputUnformattedBlock)(
    Cookie cookie, const char *x, std::size_t length) {
  IoStatementState &io{*cookie};
  if (auto *unf{io.get_if<UnformattedIoStatementState<false>>()}) {
    return unf->Emit(x, length);
  }
  io.GetIoErrorHandler().Crash("OutputUnformatted() called for an I/O "
                               "statement that is not unformatted output");
  return false;
}

bool IONAME(OutputInteger64)(Cookie cookie, std::int64_t n) {
  IoStatementState &io{*cookie};
  if (!io.get_if<OutputStatementState>()) {
    io.GetIoErrorHandler().Crash(
        "OutputInteger64() called for a non-output I/O statement");
    return false;
  }
  return EditIntegerOutput(io, io.GetNextDataEdit(), n);
}

bool IONAME(InputInteger64)(Cookie cookie, std::int64_t &n, int kind) {
  IoStatementState &io{*cookie};
  if (!io.get_if<InputStatementState>()) {
    io.GetIoErrorHandler().Crash(
        "InputInteger64() called for a non-output I/O statement");
    return false;
  }
  return EditIntegerInput(io, io.GetNextDataEdit(), n, kind);
}

bool IONAME(OutputReal64)(Cookie cookie, double x) {
  IoStatementState &io{*cookie};
  if (!io.get_if<OutputStatementState>()) {
    io.GetIoErrorHandler().Crash(
        "OutputReal64() called for a non-output I/O statement");
    return false;
  }
  return RealOutputEditing<53>{io, x}.Edit(io.GetNextDataEdit());
}

bool IONAME(OutputComplex64)(Cookie cookie, double r, double z) {
  IoStatementState &io{*cookie};
  if (io.get_if<ListDirectedStatementState<false>>()) {
    DataEdit real, imaginary;
    real.descriptor = DataEdit::ListDirectedRealPart;
    imaginary.descriptor = DataEdit::ListDirectedImaginaryPart;
    return RealOutputEditing<53>{io, r}.Edit(real) &&
        RealOutputEditing<53>{io, z}.Edit(imaginary);
  }
  return IONAME(OutputReal64)(cookie, r) && IONAME(OutputReal64)(cookie, z);
}

bool IONAME(OutputAscii)(Cookie cookie, const char *x, std::size_t length) {
  IoStatementState &io{*cookie};
  if (!io.get_if<OutputStatementState>()) {
    io.GetIoErrorHandler().Crash(
        "OutputAscii() called for a non-output I/O statement");
    return false;
  }
  bool ok{true};
  if (auto *list{io.get_if<ListDirectedStatementState<false>>()}) {
    // List-directed default CHARACTER output
    ok &= list->EmitLeadingSpaceOrAdvance(io, length, true);
    MutableModes &modes{io.mutableModes()};
    ConnectionState &connection{io.GetConnectionState()};
    if (modes.delim) {
      ok &= io.Emit(&modes.delim, 1);
      for (std::size_t j{0}; j < length; ++j) {
        if (list->NeedAdvance(connection, 2)) {
          ok &= io.Emit(&modes.delim, 1) && io.AdvanceRecord() &&
              io.Emit(&modes.delim, 1);
        }
        if (x[j] == modes.delim) {
          ok &= io.EmitRepeated(modes.delim, 2);
        } else {
          ok &= io.Emit(&x[j], 1);
        }
      }
      ok &= io.Emit(&modes.delim, 1);
    } else {
      std::size_t put{0};
      while (put < length) {
        auto chunk{std::min(length - put, connection.RemainingSpaceInRecord())};
        ok &= io.Emit(x + put, chunk);
        put += chunk;
        if (put < length) {
          ok &= io.AdvanceRecord() && io.Emit(" ", 1);
        }
      }
      list->lastWasUndelimitedCharacter = true;
    }
  } else {
    // Formatted default CHARACTER output
    DataEdit edit{io.GetNextDataEdit()};
    if (edit.descriptor != 'A' && edit.descriptor != 'G') {
      io.GetIoErrorHandler().SignalError(IoErrorInFormat,
          "Data edit descriptor '%c' may not be used with a CHARACTER data "
          "item",
          edit.descriptor);
      return false;
    }
    int len{static_cast<int>(length)};
    int width{edit.width.value_or(len)};
    ok &= io.EmitRepeated(' ', std::max(0, width - len)) &&
        io.Emit(x, std::min(width, len));
  }
  return ok;
}

bool IONAME(OutputLogical)(Cookie cookie, bool truth) {
  IoStatementState &io{*cookie};
  if (!io.get_if<OutputStatementState>()) {
    io.GetIoErrorHandler().Crash(
        "OutputLogical() called for a non-output I/O statement");
    return false;
  }
  if (auto *unf{io.get_if<UnformattedIoStatementState<false>>()}) {
    char x = truth;
    return unf->Emit(&x, 1);
  }
  bool ok{true};
  if (auto *list{io.get_if<ListDirectedStatementState<false>>()}) {
    ok &= list->EmitLeadingSpaceOrAdvance(io, 1);
  } else {
    DataEdit edit{io.GetNextDataEdit()};
    if (edit.descriptor != 'L' && edit.descriptor != 'G') {
      io.GetIoErrorHandler().SignalError(IoErrorInFormat,
          "Data edit descriptor '%c' may not be used with a LOGICAL data item",
          edit.descriptor);
      return false;
    }
    ok &= io.EmitRepeated(' ', std::max(0, edit.width.value_or(1) - 1));
  }
  return ok && io.Emit(truth ? "T" : "F", 1);
}

enum Iostat IONAME(EndIoStatement)(Cookie cookie) {
  IoStatementState &io{*cookie};
  return static_cast<enum Iostat>(io.EndIoStatement());
}
}
