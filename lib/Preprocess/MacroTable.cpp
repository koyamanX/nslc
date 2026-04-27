// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Preprocess/MacroTable.cpp — implementation of the insertion-
// ordered macro table (T054). See `MacroTable.h` for design rationale.
//
// All macro names and bodies are owned by the table (`std::string`)
// so they survive include-stack pops that would otherwise invalidate
// `StringRef`s into popped buffers.

#include "nsl/Preprocess/MacroTable.h"

#include "nsl/Basic/SourceLocation.h"

#include "llvm/ADT/StringRef.h"

#include <string>
#include <utility>

namespace nsl::preprocess {

bool MacroTable::insert(llvm::StringRef name, llvm::StringRef body,
                        SourceRange defining_loc) {
  std::string key = name.str();
  auto it = entries_.find(key);
  if (it != entries_.end()) {
    return false;
  }
  MacroDef def;
  def.name = key;
  def.body = body.str();
  def.defining_loc = defining_loc;
  entries_.insert({std::move(key), std::move(def)});
  return true;
}

void MacroTable::redefine(llvm::StringRef name, llvm::StringRef body,
                          SourceRange defining_loc,
                          SourceRange *out_previous_loc) {
  std::string key = name.str();
  auto it = entries_.find(key);
  if (it == entries_.end()) {
    if (out_previous_loc != nullptr) {
      *out_previous_loc = SourceRange();
    }
    MacroDef def;
    def.name = key;
    def.body = body.str();
    def.defining_loc = defining_loc;
    entries_.insert({std::move(key), std::move(def)});
    return;
  }
  if (out_previous_loc != nullptr) {
    *out_previous_loc = it->second.defining_loc;
  }
  it->second.body = body.str();
  it->second.defining_loc = defining_loc;
}

const MacroDef *MacroTable::lookup(llvm::StringRef name) const {
  std::string key = name.str();
  auto it = entries_.find(key);
  if (it == entries_.end()) {
    return nullptr;
  }
  return &it->second;
}

MacroDef *MacroTable::lookup(llvm::StringRef name) {
  std::string key = name.str();
  auto it = entries_.find(key);
  if (it == entries_.end()) {
    return nullptr;
  }
  return &it->second;
}

bool MacroTable::undef(llvm::StringRef name) {
  std::string key = name.str();
  auto it = entries_.find(key);
  if (it == entries_.end()) {
    return false;
  }
  entries_.erase(it);
  return true;
}

void MacroTable::predefine(llvm::StringRef name, llvm::StringRef body) {
  // -D macros are inserted with an invalid defining_loc; redefinition
  // by source treats them no differently from any other.
  std::string key = name.str();
  if (entries_.find(key) != entries_.end()) {
    // First-definition-wins for -D ordering: a prior -D with the same
    // name keeps its body.
    return;
  }
  MacroDef def;
  def.name = key;
  def.body = body.str();
  def.defining_loc = SourceRange();
  entries_.insert({std::move(key), std::move(def)});
}

} // namespace nsl::preprocess
