// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// lib/Parse/ParseDecl.cpp — declaration parsers for `lang.ebnf §§3–7`
// (FR-007, FR-008, FR-009, FR-010, FR-011, FR-012, FR-016).
//
// The structure mirrors the EBNF section ordering:
//   - §3   parseStructDecl
//   - §3.1 parseTopLevelParam
//   - §4   parseDeclareBlock + parseDeclareItem (parameter / port /
//          control-terminal forms — flat dispatch)
//   - §5   parseModuleBlock
//   - §6   parseInternalDecl (dispatched by leading keyword)
//   - §7   parseFuncDefn / parseProcDefn / parseStateDefn (S26 /
//          FR-016 accepts both `func` and `function`; N7 dotted name
//          handled via parseScopedName)

#include "ParserImpl.h"

#include "nsl/AST/DeclareBlock.h"
#include "nsl/AST/Expr.h"
#include "nsl/AST/FirstStateDecl.h"
#include "nsl/AST/FuncDefn.h"
#include "nsl/AST/FuncSelfDecl.h"
#include "nsl/AST/IntegerDecl.h"
#include "nsl/AST/MemDecl.h"
#include "nsl/AST/ModuleBlock.h"
#include "nsl/AST/PortDecl.h"
#include "nsl/AST/ProcDefn.h"
#include "nsl/AST/ProcNameDecl.h"
#include "nsl/AST/RegDecl.h"
#include "nsl/AST/StateDefn.h"
#include "nsl/AST/StateNameDecl.h"
#include "nsl/AST/Stmt.h"
#include "nsl/AST/StructDecl.h"
#include "nsl/AST/StructInstDecl.h"
#include "nsl/AST/SubmoduleDecl.h"
#include "nsl/AST/TopLevelParamDecl.h"
#include "nsl/AST/VariableDecl.h"
#include "nsl/AST/WireDecl.h"

#include <memory>
#include <utility>
#include <vector>

namespace nsl::parse {

namespace {

bool isInternalDeclStart(TokenKind k) {
  switch (k) {
  case TokenKind::tk_wire:
  case TokenKind::tk_reg:
  case TokenKind::tk_func_self:
  case TokenKind::tk_proc_name:
  case TokenKind::tk_state_name:
  case TokenKind::tk_first_state:
  case TokenKind::tk_mem:
  case TokenKind::tk_integer:
  case TokenKind::tk_variable:
    return true;
  default:
    return false;
  }
}

bool isFuncDefStart(TokenKind k) {
  return k == TokenKind::tk_func || k == TokenKind::tk_function;
}

} // namespace

// ---------- §3 struct_declaration ----------

std::unique_ptr<ast::Decl> Parser::parseStructDecl() {
  Token struct_tok;
  if (!expect(TokenKind::tk_struct_, "'struct'", &struct_tok)) {
    return nullptr;
  }
  Token name_tok;
  if (!expect(TokenKind::tk_identifier, "struct name", &name_tok)) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_lbrace, "'{' after struct name")) {
    return nullptr;
  }
  std::vector<ast::StructMember> members;
  while (!check(TokenKind::tk_rbrace) && !check(TokenKind::tk_eof)) {
    consumeLineMarkers();
    if (check(TokenKind::tk_rbrace)) {
      break;
    }
    Token mem_tok;
    if (!expect(TokenKind::tk_identifier, "struct member name", &mem_tok)) {
      return nullptr;
    }
    std::unique_ptr<ast::Expr> width;
    if (check(TokenKind::tk_lbracket)) {
      consume();
      width = parseExpr();
      if (!width) {
        return nullptr;
      }
      if (!expect(TokenKind::tk_rbracket, "']' after struct member width")) {
        return nullptr;
      }
    }
    if (!expect(TokenKind::tk_semicolon, "';' after struct member")) {
      return nullptr;
    }
    members.push_back({mem_tok.spelling(), std::move(width)});
  }
  Token rbr;
  if (!expect(TokenKind::tk_rbrace, "'}' to close struct", &rbr)) {
    return nullptr;
  }
  Token semi;
  if (!expect(TokenKind::tk_semicolon, "';' after struct declaration", &semi)) {
    return nullptr;
  }
  return std::make_unique<ast::StructDecl>(
      rangeFromTo(struct_tok.range().begin(), semi.range().end()),
      name_tok.spelling(), std::move(members));
}

// ---------- §3.1 top_level_parameter ----------

std::unique_ptr<ast::Decl> Parser::parseTopLevelParam() {
  Token kw;
  ast::TopLevelParamDecl::ParamKind kind = ast::TopLevelParamDecl::ParamKind::Int;
  if (check(TokenKind::tk_param_int)) {
    kw = consume();
    kind = ast::TopLevelParamDecl::ParamKind::Int;
  } else if (check(TokenKind::tk_param_str)) {
    kw = consume();
    kind = ast::TopLevelParamDecl::ParamKind::Str;
  } else {
    errorAtPeek("expected 'param_int' or 'param_str'");
    return nullptr;
  }
  Token name_tok;
  if (!expect(TokenKind::tk_identifier, "parameter name", &name_tok)) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_assign, "'=' in top-level parameter")) {
    return nullptr;
  }
  auto init = parseExpr();
  if (!init) {
    return nullptr;
  }
  Token semi;
  if (!expect(TokenKind::tk_semicolon, "';' after top-level parameter",
              &semi)) {
    return nullptr;
  }
  return std::make_unique<ast::TopLevelParamDecl>(
      rangeFromTo(kw.range().begin(), semi.range().end()), kind,
      name_tok.spelling(), std::move(init));
}

// ---------- §4 declare_block ----------

std::unique_ptr<ast::Decl> Parser::parseDeclareBlock() {
  Token decl_tok;
  if (!expect(TokenKind::tk_declare, "'declare'", &decl_tok)) {
    return nullptr;
  }
  ast::Identifier name;
  if (check(TokenKind::tk_identifier)) {
    Token name_tok = consume();
    name = name_tok.spelling();
  }
  ast::DeclareBlock::Modifier mod = ast::DeclareBlock::Modifier::None;
  if (check(TokenKind::tk_interface)) {
    consume();
    mod = ast::DeclareBlock::Modifier::Interface;
  } else if (check(TokenKind::tk_simulation)) {
    consume();
    mod = ast::DeclareBlock::Modifier::Simulation;
  }
  if (!expect(TokenKind::tk_lbrace, "'{' to begin declare block")) {
    return nullptr;
  }
  std::vector<std::unique_ptr<ast::Decl>> headerParams;
  std::vector<std::unique_ptr<ast::Decl>> ports;
  while (!check(TokenKind::tk_rbrace) && !check(TokenKind::tk_eof)) {
    consumeLineMarkers();
    if (check(TokenKind::tk_rbrace)) {
      break;
    }
    if (!parseDeclareItem(headerParams, ports)) {
      return nullptr;
    }
  }
  Token rbr;
  if (!expect(TokenKind::tk_rbrace, "'}' to close declare block", &rbr)) {
    return nullptr;
  }
  // The data-model treats declare-block port vector as
  // `vector<unique_ptr<PortDecl>>`. Cast each Decl* to PortDecl on
  // the way out — the `parseDeclareItem` helper guarantees the type.
  std::vector<std::unique_ptr<ast::PortDecl>> port_list;
  port_list.reserve(ports.size());
  for (auto &p : ports) {
    auto *raw = static_cast<ast::PortDecl *>(p.release());
    port_list.emplace_back(raw);
  }
  return std::make_unique<ast::DeclareBlock>(
      rangeFromTo(decl_tok.range().begin(), rbr.range().end()), name, mod,
      std::move(headerParams), std::move(port_list));
}

bool Parser::parseDeclareItem(
    std::vector<std::unique_ptr<ast::Decl>> &headerParams,
    std::vector<std::unique_ptr<ast::Decl>> &ports) {
  TokenKind k = peekKind();

  // parameter_declaration: "param_int" identifier { "," identifier } ";"
  if (k == TokenKind::tk_param_int || k == TokenKind::tk_param_str) {
    Token kw_tok = consume();
    ast::TopLevelParamDecl::ParamKind pk =
        (kw_tok.kind() == TokenKind::tk_param_int)
            ? ast::TopLevelParamDecl::ParamKind::Int
            : ast::TopLevelParamDecl::ParamKind::Str;
    // Multiple identifiers per declaration → split into one
    // `TopLevelParamDecl` per name (the AST surface only holds one
    // name per node; parser-side splitting keeps the per-decl shape
    // consistent across header-params and top-level-params).
    Token first;
    if (!expect(TokenKind::tk_identifier, "parameter name", &first)) {
      return false;
    }
    headerParams.push_back(std::make_unique<ast::TopLevelParamDecl>(
        rangeFromTo(kw_tok.range().begin(), first.range().end()), pk,
        first.spelling(), nullptr));
    while (check(TokenKind::tk_comma)) {
      consume();
      Token nxt;
      if (!expect(TokenKind::tk_identifier, "parameter name after ','", &nxt)) {
        return false;
      }
      headerParams.push_back(std::make_unique<ast::TopLevelParamDecl>(
          nxt.range(), pk, nxt.spelling(), nullptr));
    }
    if (!expect(TokenKind::tk_semicolon, "';' after parameter declaration")) {
      return false;
    }
    return true;
  }

  // data_terminal_declaration: data_direction signal_declarator … ";"
  if (k == TokenKind::tk_input || k == TokenKind::tk_output ||
      k == TokenKind::tk_inout) {
    Token dir_tok = consume();
    ast::PortDecl::Direction dir = ast::PortDecl::Direction::Input;
    switch (dir_tok.kind()) {
    case TokenKind::tk_input:
      dir = ast::PortDecl::Direction::Input;
      break;
    case TokenKind::tk_output:
      dir = ast::PortDecl::Direction::Output;
      break;
    case TokenKind::tk_inout:
      dir = ast::PortDecl::Direction::Inout;
      break;
    default:
      break;
    }
    // signal_declarator { "," signal_declarator } ";"
    bool first = true;
    do {
      if (!first) {
        consume(); // `,`
      }
      first = false;
      Token name_tok;
      if (!expect(TokenKind::tk_identifier, "port name", &name_tok)) {
        return false;
      }
      std::unique_ptr<ast::Expr> width;
      if (check(TokenKind::tk_lbracket)) {
        consume();
        width = parseExpr();
        if (!width) {
          return false;
        }
        if (!expect(TokenKind::tk_rbracket, "']' after port width")) {
          return false;
        }
      }
      SourceLocation begin = dir_tok.range().begin();
      SourceLocation end = name_tok.range().end();
      if (width) {
        end = width->loc().end();
      }
      ports.push_back(std::make_unique<ast::PortDecl>(
          rangeFromTo(begin, end), dir, name_tok.spelling(), std::move(width),
          std::vector<ast::Identifier>{}, ast::Identifier{}));
    } while (check(TokenKind::tk_comma));
    if (!expect(TokenKind::tk_semicolon, "';' after port declaration")) {
      return false;
    }
    return true;
  }

  // control_terminal_declaration: control_direction identifier
  //   [ "(" [ dummy_arg_list ] ")" ] [ ":" identifier ] ";"
  if (k == TokenKind::tk_func_in || k == TokenKind::tk_func_out) {
    Token dir_tok = consume();
    ast::PortDecl::Direction dir = (dir_tok.kind() == TokenKind::tk_func_in)
                                       ? ast::PortDecl::Direction::FuncIn
                                       : ast::PortDecl::Direction::FuncOut;
    Token name_tok;
    if (!expect(TokenKind::tk_identifier, "control-terminal name", &name_tok)) {
      return false;
    }
    std::vector<ast::Identifier> dummyArgs;
    if (check(TokenKind::tk_lparen)) {
      consume();
      if (!check(TokenKind::tk_rparen)) {
        Token first_arg;
        if (!expect(TokenKind::tk_identifier, "dummy-arg identifier",
                    &first_arg)) {
          return false;
        }
        dummyArgs.push_back(first_arg.spelling());
        while (check(TokenKind::tk_comma)) {
          consume();
          Token nxt;
          if (!expect(TokenKind::tk_identifier,
                      "dummy-arg identifier after ','", &nxt)) {
            return false;
          }
          dummyArgs.push_back(nxt.spelling());
        }
      }
      if (!expect(TokenKind::tk_rparen, "')' after dummy-arg list")) {
        return false;
      }
    }
    ast::Identifier returnTerminal;
    if (check(TokenKind::tk_colon)) {
      consume();
      Token ret_tok;
      if (!expect(TokenKind::tk_identifier, "return-terminal identifier",
                  &ret_tok)) {
        return false;
      }
      returnTerminal = ret_tok.spelling();
    }
    Token semi;
    if (!expect(TokenKind::tk_semicolon, "';' after control-terminal", &semi)) {
      return false;
    }
    ports.push_back(std::make_unique<ast::PortDecl>(
        rangeFromTo(dir_tok.range().begin(), semi.range().end()), dir,
        name_tok.spelling(), nullptr, std::move(dummyArgs), returnTerminal));
    return true;
  }

  errorAtPeek("expected 'param_int' / 'input' / 'output' / 'inout' / "
              "'func_in' / 'func_out' in declare-item");
  return false;
}

// ---------- §5 module_block ----------

std::unique_ptr<ast::Decl> Parser::parseModuleBlock() {
  Token mod_tok;
  if (!expect(TokenKind::tk_module, "'module'", &mod_tok)) {
    return nullptr;
  }
  Token name_tok;
  if (!expect(TokenKind::tk_identifier, "module name", &name_tok)) {
    return nullptr;
  }
  if (!expect(TokenKind::tk_lbrace, "'{' to begin module body")) {
    return nullptr;
  }
  std::vector<std::unique_ptr<ast::Decl>> internals;
  std::vector<std::unique_ptr<ast::Stmt>> actions;
  std::vector<std::unique_ptr<ast::Decl>> funcs;
  std::vector<std::unique_ptr<ast::Decl>> procs;
  while (!check(TokenKind::tk_rbrace) && !check(TokenKind::tk_eof)) {
    consumeLineMarkers();
    if (check(TokenKind::tk_rbrace)) {
      break;
    }
    if (!parseModuleItem(internals, actions, funcs, procs)) {
      return nullptr;
    }
  }
  Token rbr;
  if (!expect(TokenKind::tk_rbrace, "'}' to close module", &rbr)) {
    return nullptr;
  }
  return std::make_unique<ast::ModuleBlock>(
      rangeFromTo(mod_tok.range().begin(), rbr.range().end()),
      name_tok.spelling(), std::move(internals), std::move(actions),
      std::move(funcs), std::move(procs));
}

bool Parser::parseModuleItem(std::vector<std::unique_ptr<ast::Decl>> &internals,
                             std::vector<std::unique_ptr<ast::Stmt>> &actions,
                             std::vector<std::unique_ptr<ast::Decl>> &funcs,
                             std::vector<std::unique_ptr<ast::Decl>> &procs) {
  TokenKind k = peekKind();

  if (isInternalDeclStart(k)) {
    auto d = parseInternalDecl();
    if (!d) {
      return false;
    }
    internals.push_back(std::move(d));
    return true;
  }
  if (isFuncDefStart(k)) {
    auto d = parseFuncDefn();
    if (!d) {
      return false;
    }
    funcs.push_back(std::move(d));
    return true;
  }
  if (k == TokenKind::tk_proc) {
    auto d = parseProcDefn();
    if (!d) {
      return false;
    }
    procs.push_back(std::move(d));
    return true;
  }
  if (k == TokenKind::tk_state) {
    auto d = parseStateDefn();
    if (!d) {
      return false;
    }
    procs.push_back(std::move(d));
    return true;
  }
  // Submodule instance: module-item starting with an identifier (the
  // template name) followed by another identifier, optional `[N]`
  // and optional `(args)`. Distinct from struct-instance: that has
  // `(reg|wire)` between the type-name and instance-name.
  if (k == TokenKind::tk_identifier) {
    // Need to peek-ahead to disambiguate struct-instance vs submodule.
    Token next = peekAhead(1);
    if (next.kind() == TokenKind::tk_reg || next.kind() == TokenKind::tk_wire) {
      auto d = parseInternalDecl();
      if (!d) {
        return false;
      }
      internals.push_back(std::move(d));
      return true;
    }
    if (next.kind() == TokenKind::tk_identifier) {
      // submodule_declaration. parseInternalDecl handles it.
      auto d = parseInternalDecl();
      if (!d) {
        return false;
      }
      internals.push_back(std::move(d));
      return true;
    }
  }

  // Otherwise it's a common_action_statement. Parse and append to
  // actions.
  auto s = parseActionStatement();
  if (!s) {
    return false;
  }
  actions.push_back(std::move(s));
  return true;
}

// ---------- §6 internal_declaration dispatch ----------

std::unique_ptr<ast::Decl> Parser::parseInternalDecl() {
  TokenKind k = peekKind();

  // wire signal_declarator { "," signal_declarator } ";"
  if (k == TokenKind::tk_wire) {
    Token kw = consume();
    Token name_tok;
    if (!expect(TokenKind::tk_identifier, "wire name", &name_tok)) {
      return nullptr;
    }
    std::unique_ptr<ast::Expr> width;
    if (check(TokenKind::tk_lbracket)) {
      consume();
      width = parseExpr();
      if (!width) {
        return nullptr;
      }
      if (!expect(TokenKind::tk_rbracket, "']' after wire width")) {
        return nullptr;
      }
    }
    // Discard any further comma-separated declarators at M2 — the
    // first one wins (a known parser-shape simplification per
    // SeqBlock-internal tradeoffs).
    while (check(TokenKind::tk_comma)) {
      consume();
      Token nxt;
      if (!expect(TokenKind::tk_identifier, "wire name after ','", &nxt)) {
        return nullptr;
      }
      if (check(TokenKind::tk_lbracket)) {
        consume();
        auto extra_width = parseExpr();
        if (!extra_width) {
          return nullptr;
        }
        if (!expect(TokenKind::tk_rbracket, "']' after wire width")) {
          return nullptr;
        }
      }
    }
    Token semi;
    if (!expect(TokenKind::tk_semicolon, "';' after wire declaration", &semi)) {
      return nullptr;
    }
    return std::make_unique<ast::WireDecl>(
        rangeFromTo(kw.range().begin(), semi.range().end()),
        name_tok.spelling(), std::move(width));
  }

  // reg register_declarator { "," register_declarator } ";"
  if (k == TokenKind::tk_reg) {
    Token kw = consume();
    Token name_tok;
    if (!expect(TokenKind::tk_identifier, "register name", &name_tok)) {
      return nullptr;
    }
    std::unique_ptr<ast::Expr> width;
    if (check(TokenKind::tk_lbracket)) {
      consume();
      width = parseExpr();
      if (!width) {
        return nullptr;
      }
      if (!expect(TokenKind::tk_rbracket, "']' after register width")) {
        return nullptr;
      }
    }
    std::unique_ptr<ast::Expr> init;
    if (check(TokenKind::tk_assign)) {
      consume();
      init = parseExpr();
      if (!init) {
        return nullptr;
      }
    }
    while (check(TokenKind::tk_comma)) {
      consume();
      Token nxt;
      if (!expect(TokenKind::tk_identifier, "register name after ','", &nxt)) {
        return nullptr;
      }
      if (check(TokenKind::tk_lbracket)) {
        consume();
        auto extra_width = parseExpr();
        if (!extra_width) {
          return nullptr;
        }
        if (!expect(TokenKind::tk_rbracket, "']' after register width")) {
          return nullptr;
        }
      }
      if (check(TokenKind::tk_assign)) {
        consume();
        auto extra_init = parseExpr();
        if (!extra_init) {
          return nullptr;
        }
      }
    }
    Token semi;
    if (!expect(TokenKind::tk_semicolon, "';' after register declaration",
                &semi)) {
      return nullptr;
    }
    return std::make_unique<ast::RegDecl>(
        rangeFromTo(kw.range().begin(), semi.range().end()),
        name_tok.spelling(), std::move(width), std::move(init));
  }

  // func_self identifier [(args)] [: id] ;
  if (k == TokenKind::tk_func_self) {
    Token kw = consume();
    Token name_tok;
    if (!expect(TokenKind::tk_identifier, "func_self name", &name_tok)) {
      return nullptr;
    }
    std::vector<ast::Identifier> dummyArgs;
    if (check(TokenKind::tk_lparen)) {
      consume();
      if (!check(TokenKind::tk_rparen)) {
        Token first;
        if (!expect(TokenKind::tk_identifier, "dummy-arg identifier",
                    &first)) {
          return nullptr;
        }
        dummyArgs.push_back(first.spelling());
        while (check(TokenKind::tk_comma)) {
          consume();
          Token nxt;
          if (!expect(TokenKind::tk_identifier,
                      "dummy-arg identifier after ','", &nxt)) {
            return nullptr;
          }
          dummyArgs.push_back(nxt.spelling());
        }
      }
      if (!expect(TokenKind::tk_rparen, "')' after dummy-arg list")) {
        return nullptr;
      }
    }
    ast::Identifier returnTerminal;
    if (check(TokenKind::tk_colon)) {
      consume();
      Token ret_tok;
      if (!expect(TokenKind::tk_identifier, "return-terminal identifier",
                  &ret_tok)) {
        return nullptr;
      }
      returnTerminal = ret_tok.spelling();
    }
    Token semi;
    if (!expect(TokenKind::tk_semicolon, "';' after func_self declaration",
                &semi)) {
      return nullptr;
    }
    return std::make_unique<ast::FuncSelfDecl>(
        rangeFromTo(kw.range().begin(), semi.range().end()),
        name_tok.spelling(), std::move(dummyArgs), returnTerminal);
  }

  // proc_name proc_declarator { "," proc_declarator } ";"
  if (k == TokenKind::tk_proc_name) {
    Token kw = consume();
    Token first_name;
    if (!expect(TokenKind::tk_identifier, "proc_name identifier",
                &first_name)) {
      return nullptr;
    }
    std::vector<ast::Identifier> regArgs;
    if (check(TokenKind::tk_lparen)) {
      consume();
      if (!check(TokenKind::tk_rparen)) {
        Token a;
        if (!expect(TokenKind::tk_identifier, "proc_name reg-arg", &a)) {
          return nullptr;
        }
        regArgs.push_back(a.spelling());
        while (check(TokenKind::tk_comma)) {
          consume();
          Token nxt;
          if (!expect(TokenKind::tk_identifier, "proc_name reg-arg after ','",
                      &nxt)) {
            return nullptr;
          }
          regArgs.push_back(nxt.spelling());
        }
      }
      if (!expect(TokenKind::tk_rparen, "')' after proc_name reg-args")) {
        return nullptr;
      }
    }
    // Discard extra comma-separated proc_declarators at M2 (one
    // proc_name node per parsed leading declarator).
    while (check(TokenKind::tk_comma)) {
      consume();
      Token nxt;
      if (!expect(TokenKind::tk_identifier, "proc_name identifier after ','",
                  &nxt)) {
        return nullptr;
      }
      if (check(TokenKind::tk_lparen)) {
        consume();
        if (!check(TokenKind::tk_rparen)) {
          Token a;
          if (!expect(TokenKind::tk_identifier, "proc_name reg-arg", &a)) {
            return nullptr;
          }
          while (check(TokenKind::tk_comma)) {
            consume();
            Token rest;
            if (!expect(TokenKind::tk_identifier,
                        "proc_name reg-arg after ','", &rest)) {
              return nullptr;
            }
          }
        }
        if (!expect(TokenKind::tk_rparen, "')' after proc_name reg-args")) {
          return nullptr;
        }
      }
    }
    Token semi;
    if (!expect(TokenKind::tk_semicolon, "';' after proc_name declaration",
                &semi)) {
      return nullptr;
    }
    return std::make_unique<ast::ProcNameDecl>(
        rangeFromTo(kw.range().begin(), semi.range().end()),
        first_name.spelling(), std::move(regArgs));
  }

  // state_name identifier { "," identifier } ";"
  if (k == TokenKind::tk_state_name) {
    Token kw = consume();
    Token first;
    if (!expect(TokenKind::tk_identifier, "state name", &first)) {
      return nullptr;
    }
    std::vector<ast::Identifier> names;
    names.push_back(first.spelling());
    while (check(TokenKind::tk_comma)) {
      consume();
      Token nxt;
      if (!expect(TokenKind::tk_identifier, "state name after ','", &nxt)) {
        return nullptr;
      }
      names.push_back(nxt.spelling());
    }
    Token semi;
    if (!expect(TokenKind::tk_semicolon, "';' after state_name declaration",
                &semi)) {
      return nullptr;
    }
    return std::make_unique<ast::StateNameDecl>(
        rangeFromTo(kw.range().begin(), semi.range().end()), std::move(names));
  }

  // first_state identifier ";"
  if (k == TokenKind::tk_first_state) {
    Token kw = consume();
    Token target;
    if (!expect(TokenKind::tk_identifier, "first_state target", &target)) {
      return nullptr;
    }
    Token semi;
    if (!expect(TokenKind::tk_semicolon, "';' after first_state declaration",
                &semi)) {
      return nullptr;
    }
    return std::make_unique<ast::FirstStateDecl>(
        rangeFromTo(kw.range().begin(), semi.range().end()), target.spelling());
  }

  // mem identifier "[" const_expr "]" "[" const_expr "]" [= "{" ... "}"] ";"
  if (k == TokenKind::tk_mem) {
    Token kw = consume();
    Token name_tok;
    if (!expect(TokenKind::tk_identifier, "mem name", &name_tok)) {
      return nullptr;
    }
    if (!expect(TokenKind::tk_lbracket, "'[' for mem depth")) {
      return nullptr;
    }
    auto depth = parseExpr();
    if (!depth) {
      return nullptr;
    }
    if (!expect(TokenKind::tk_rbracket, "']' after mem depth")) {
      return nullptr;
    }
    if (!expect(TokenKind::tk_lbracket, "'[' for mem width")) {
      return nullptr;
    }
    auto width = parseExpr();
    if (!width) {
      return nullptr;
    }
    if (!expect(TokenKind::tk_rbracket, "']' after mem width")) {
      return nullptr;
    }
    std::vector<std::unique_ptr<ast::Expr>> init;
    if (check(TokenKind::tk_assign)) {
      consume();
      if (!expect(TokenKind::tk_lbrace, "'{' for mem initializer list")) {
        return nullptr;
      }
      if (!check(TokenKind::tk_rbrace)) {
        auto first = parseExpr();
        if (!first) {
          return nullptr;
        }
        init.push_back(std::move(first));
        while (check(TokenKind::tk_comma)) {
          consume();
          auto nxt = parseExpr();
          if (!nxt) {
            return nullptr;
          }
          init.push_back(std::move(nxt));
        }
      }
      if (!expect(TokenKind::tk_rbrace, "'}' to close mem initializer list")) {
        return nullptr;
      }
    }
    Token semi;
    if (!expect(TokenKind::tk_semicolon, "';' after mem declaration", &semi)) {
      return nullptr;
    }
    return std::make_unique<ast::MemDecl>(
        rangeFromTo(kw.range().begin(), semi.range().end()),
        name_tok.spelling(), std::move(depth), std::move(width),
        std::move(init));
  }

  // integer identifier { "," identifier } ";"
  if (k == TokenKind::tk_integer) {
    Token kw = consume();
    Token name_tok;
    if (!expect(TokenKind::tk_identifier, "integer name", &name_tok)) {
      return nullptr;
    }
    while (check(TokenKind::tk_comma)) {
      consume();
      Token nxt;
      if (!expect(TokenKind::tk_identifier, "integer name after ','", &nxt)) {
        return nullptr;
      }
    }
    Token semi;
    if (!expect(TokenKind::tk_semicolon, "';' after integer declaration",
                &semi)) {
      return nullptr;
    }
    return std::make_unique<ast::IntegerDecl>(
        rangeFromTo(kw.range().begin(), semi.range().end()),
        name_tok.spelling());
  }

  // variable identifier [ "[" width "]" ] { "," ... } ";"
  if (k == TokenKind::tk_variable) {
    Token kw = consume();
    Token name_tok;
    if (!expect(TokenKind::tk_identifier, "variable name", &name_tok)) {
      return nullptr;
    }
    std::unique_ptr<ast::Expr> width;
    if (check(TokenKind::tk_lbracket)) {
      consume();
      width = parseExpr();
      if (!width) {
        return nullptr;
      }
      if (!expect(TokenKind::tk_rbracket, "']' after variable width")) {
        return nullptr;
      }
    }
    while (check(TokenKind::tk_comma)) {
      consume();
      Token nxt;
      if (!expect(TokenKind::tk_identifier, "variable name after ','", &nxt)) {
        return nullptr;
      }
      if (check(TokenKind::tk_lbracket)) {
        consume();
        auto extra_width = parseExpr();
        if (!extra_width) {
          return nullptr;
        }
        if (!expect(TokenKind::tk_rbracket, "']' after variable width")) {
          return nullptr;
        }
      }
    }
    Token semi;
    if (!expect(TokenKind::tk_semicolon, "';' after variable declaration",
                &semi)) {
      return nullptr;
    }
    return std::make_unique<ast::VariableDecl>(
        rangeFromTo(kw.range().begin(), semi.range().end()),
        name_tok.spelling(), std::move(width));
  }

  // identifier-led: struct_instance OR submodule_declaration
  if (k == TokenKind::tk_identifier) {
    Token type_tok = consume();
    // struct_instance: identifier ( "reg" | "wire" ) struct_inst_declarator ...
    if (check(TokenKind::tk_reg) || check(TokenKind::tk_wire)) {
      Token storage = consume();
      ast::StructInstDecl::StorageKind sk =
          (storage.kind() == TokenKind::tk_reg)
              ? ast::StructInstDecl::StorageKind::Reg
              : ast::StructInstDecl::StorageKind::Wire;
      Token inst_tok;
      if (!expect(TokenKind::tk_identifier, "struct-instance name",
                  &inst_tok)) {
        return nullptr;
      }
      std::unique_ptr<ast::Expr> arraySize;
      if (check(TokenKind::tk_lbracket)) {
        consume();
        arraySize = parseExpr();
        if (!arraySize) {
          return nullptr;
        }
        if (!expect(TokenKind::tk_rbracket,
                    "']' after struct-instance array size")) {
          return nullptr;
        }
      }
      std::vector<std::unique_ptr<ast::Expr>> init;
      if (check(TokenKind::tk_assign)) {
        consume();
        if (check(TokenKind::tk_lbrace)) {
          consume();
          if (!check(TokenKind::tk_rbrace)) {
            auto first = parseExpr();
            if (!first) {
              return nullptr;
            }
            init.push_back(std::move(first));
            while (check(TokenKind::tk_comma)) {
              consume();
              auto nxt = parseExpr();
              if (!nxt) {
                return nullptr;
              }
              init.push_back(std::move(nxt));
            }
          }
          if (!expect(TokenKind::tk_rbrace,
                      "'}' to close struct-instance initializer")) {
            return nullptr;
          }
        } else {
          auto scalar = parseExpr();
          if (!scalar) {
            return nullptr;
          }
          init.push_back(std::move(scalar));
        }
      }
      // Skip subsequent comma-separated declarators (M2 simplification).
      while (check(TokenKind::tk_comma)) {
        consume();
        Token nxt;
        if (!expect(TokenKind::tk_identifier,
                    "struct-instance name after ','", &nxt)) {
          return nullptr;
        }
        if (check(TokenKind::tk_lbracket)) {
          consume();
          auto extra_arr = parseExpr();
          if (!extra_arr) {
            return nullptr;
          }
          if (!expect(TokenKind::tk_rbracket,
                      "']' after struct-instance array size")) {
            return nullptr;
          }
        }
        if (check(TokenKind::tk_assign)) {
          consume();
          if (check(TokenKind::tk_lbrace)) {
            consume();
            if (!check(TokenKind::tk_rbrace)) {
              auto first = parseExpr();
              if (!first) {
                return nullptr;
              }
              while (check(TokenKind::tk_comma)) {
                consume();
                auto more = parseExpr();
                if (!more) {
                  return nullptr;
                }
              }
            }
            if (!expect(TokenKind::tk_rbrace,
                        "'}' to close struct-instance initializer")) {
              return nullptr;
            }
          } else {
            auto scalar = parseExpr();
            if (!scalar) {
              return nullptr;
            }
          }
        }
      }
      Token semi;
      if (!expect(TokenKind::tk_semicolon,
                  "';' after struct-instance declaration", &semi)) {
        return nullptr;
      }
      return std::make_unique<ast::StructInstDecl>(
          rangeFromTo(type_tok.range().begin(), semi.range().end()),
          type_tok.spelling(), inst_tok.spelling(), sk, std::move(arraySize),
          std::move(init));
    }
    // submodule_declaration: identifier submodule_instance { "," ... } ";"
    if (check(TokenKind::tk_identifier)) {
      std::vector<ast::SubmoduleDecl::Instance> instances;
      std::vector<ast::SubmoduleDecl::ParamAssign> paramAssigns;
      // first instance
      Token first_inst;
      if (!expect(TokenKind::tk_identifier, "submodule-instance name",
                  &first_inst)) {
        return nullptr;
      }
      std::unique_ptr<ast::Expr> arraySize;
      if (check(TokenKind::tk_lbracket)) {
        consume();
        arraySize = parseExpr();
        if (!arraySize) {
          return nullptr;
        }
        if (!expect(TokenKind::tk_rbracket,
                    "']' after submodule-instance array size")) {
          return nullptr;
        }
      }
      // optional "(" param_assignment_list ")"
      if (check(TokenKind::tk_lparen)) {
        consume();
        if (!check(TokenKind::tk_rparen)) {
          for (;;) {
            Token pa_name;
            if (!expect(TokenKind::tk_identifier,
                        "parameter-assignment name", &pa_name)) {
              return nullptr;
            }
            if (!expect(TokenKind::tk_assign,
                        "'=' in parameter-assignment")) {
              return nullptr;
            }
            // value: constant_expression OR string_literal
            std::unique_ptr<ast::Expr> value = parseExpr();
            if (!value) {
              return nullptr;
            }
            paramAssigns.push_back({pa_name.spelling(), std::move(value)});
            if (check(TokenKind::tk_comma)) {
              consume();
              continue;
            }
            break;
          }
        }
        if (!expect(TokenKind::tk_rparen,
                    "')' after parameter-assignment list")) {
          return nullptr;
        }
      }
      instances.push_back({first_inst.spelling(), std::move(arraySize)});
      // additional comma-separated submodule_instance entries
      while (check(TokenKind::tk_comma)) {
        consume();
        Token nxt_name;
        if (!expect(TokenKind::tk_identifier,
                    "submodule-instance name after ','", &nxt_name)) {
          return nullptr;
        }
        std::unique_ptr<ast::Expr> nxt_arr;
        if (check(TokenKind::tk_lbracket)) {
          consume();
          nxt_arr = parseExpr();
          if (!nxt_arr) {
            return nullptr;
          }
          if (!expect(TokenKind::tk_rbracket,
                      "']' after submodule-instance array size")) {
            return nullptr;
          }
        }
        if (check(TokenKind::tk_lparen)) {
          // M2 simplification: trailing per-instance param-assigns are
          // accepted but ignored beyond the first declarator.
          consume();
          while (!check(TokenKind::tk_rparen) && !check(TokenKind::tk_eof)) {
            consume();
          }
          if (!expect(TokenKind::tk_rparen,
                      "')' after parameter-assignment list")) {
            return nullptr;
          }
        }
        instances.push_back({nxt_name.spelling(), std::move(nxt_arr)});
      }
      Token semi;
      if (!expect(TokenKind::tk_semicolon,
                  "';' after submodule declaration", &semi)) {
        return nullptr;
      }
      return std::make_unique<ast::SubmoduleDecl>(
          rangeFromTo(type_tok.range().begin(), semi.range().end()),
          type_tok.spelling(), std::move(instances), std::move(paramAssigns));
    }
    errorAtPeek("expected 'reg' / 'wire' (struct-instance) or identifier "
                "(submodule-instance)");
    return nullptr;
  }

  errorAtPeek("expected internal_declaration");
  return nullptr;
}

// ---------- §7 function/procedure/state definitions ----------

std::unique_ptr<ast::Decl> Parser::parseFuncDefn() {
  Token kw;
  if (check(TokenKind::tk_func)) {
    kw = consume();
  } else if (check(TokenKind::tk_function)) {
    // FR-016 / S26: `function` is an alias for `func` at parser level.
    // The Sema-time canonicalization warning is M3's.
    kw = consume();
  } else {
    errorAtPeek("expected 'func' or 'function'");
    return nullptr;
  }

  // N7: dotted scoped name (`func ic.ready { ... }`).
  SourceRange name_range;
  ast::ScopedName name = parseScopedName(name_range);
  if (name.parts.empty()) {
    return nullptr;
  }
  auto body = parseActionStatement();
  if (!body) {
    return nullptr;
  }
  SourceLocation end_loc = body->loc().end();
  return std::make_unique<ast::FuncDefn>(
      rangeFromTo(kw.range().begin(), end_loc), std::move(name), std::move(body));
}

std::unique_ptr<ast::Decl> Parser::parseProcDefn() {
  Token kw;
  if (!expect(TokenKind::tk_proc, "'proc'", &kw)) {
    return nullptr;
  }
  Token name_tok;
  if (!expect(TokenKind::tk_identifier, "proc name", &name_tok)) {
    return nullptr;
  }
  auto body = parseActionStatement();
  if (!body) {
    return nullptr;
  }
  SourceLocation end_loc = body->loc().end();
  return std::make_unique<ast::ProcDefn>(
      rangeFromTo(kw.range().begin(), end_loc), name_tok.spelling(),
      std::move(body));
}

std::unique_ptr<ast::Decl> Parser::parseStateDefn() {
  Token kw;
  if (!expect(TokenKind::tk_state, "'state'", &kw)) {
    return nullptr;
  }
  Token name_tok;
  if (!expect(TokenKind::tk_identifier, "state name", &name_tok)) {
    return nullptr;
  }
  auto body = parseActionStatement();
  if (!body) {
    return nullptr;
  }
  SourceLocation end_loc = body->loc().end();
  return std::make_unique<ast::StateDefn>(
      rangeFromTo(kw.range().begin(), end_loc), name_tok.spelling(),
      std::move(body));
}

} // namespace nsl::parse
