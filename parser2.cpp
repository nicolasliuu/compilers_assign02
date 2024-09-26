#include <cassert>
#include <map>
#include <string>
#include <memory>
#include "token.h"
#include "ast.h"
#include "exceptions.h"
#include "parser2.h"
#include <iostream>

////////////////////////////////////////////////////////////////////////
// Parser2 implementation
// This version of the parser builds an AST directly,
// rather than first building a parse tree.
////////////////////////////////////////////////////////////////////////

// This is the grammar (Unit is the start symbol):
//
// Unit -> Stmt
// Unit -> Stmt Unit
// Stmt -> E ;
// E -> T E'
// E' -> + T E'
// E' -> - T E'
// E' -> epsilon
// T -> F T'
// T' -> * F T'
// T' -> / F T'
// T' -> epsilon
// F -> number
// F -> ident
// F -> ( E )

Parser2::Parser2(Lexer *lexer_to_adopt)
  : m_lexer(lexer_to_adopt)
  , m_next(nullptr) {
}

Parser2::~Parser2() {
  delete m_lexer;
}

Node *Parser2::parse() {
  return parse_Unit();
}
Node *Parser2::parse_Unit() {
  // note that this function produces a "flattened" representation
  // of the unit

  // new production Unit -> TStmt
  // new production Unit -> TStmt Unit

  std::unique_ptr<Node> unit(new Node(AST_UNIT));
  for (;;) {
    unit->append_kid(parse_TStmt());
    if (m_lexer->peek() == nullptr)
      break;
  }

  return unit.release();
}

Node *Parser2::parse_TStmt() {
  // Peek at the next token to decide what kind of statement we are parsing
  Node *next_tok = m_lexer->peek();
  if (next_tok == nullptr) {
    SyntaxError::raise(m_lexer->get_current_loc(), "Unexpected end of input looking for statement");
  }

  // Check if the next token is 'function' (indicating a function definition)
  if (next_tok->get_tag() == TOK_FUNCTION) {
    // TStmt → Func
    return parse_Func();  // Parse the function definition
  } else {
    // TStmt → Stmt
    return parse_Stmt();  // Parse a regular statement
  }
}

Node *Parser2::parse_Stmt() {
  std::unique_ptr<Node> stmt(new Node(AST_STATEMENT));

  // Peek at the next token to decide what kind of statement we are parsing
  Node *next_tok = m_lexer->peek();
  if (next_tok == nullptr) {
    SyntaxError::raise(m_lexer->get_current_loc(), "Unexpected end of input looking for statement");
  }

  // Check if the next token is 'var' (indicating a variable declaration)
  switch(next_tok->get_tag()) {
    case TOK_VAR: 
      stmt->append_kid(parse_varDec());  // parse var declaration
      break;
    case TOK_IF: 
      stmt->append_kid(parse_If());      // parse if statement
      break;
    case TOK_WHILE:
      stmt->append_kid(parse_While());   // parse while statement
      break;
    default: 
      // Handle other statements, like assignments or expressions
      // Stmt → A ;
      stmt->append_kid(parse_A());                           // Parse the A non-terminal (assignment or expression)
      expect_and_discard(TOK_SEMICOLON);                     // Consume the semicolon at the end of the statement
      break;
  }
  return stmt.release();  // Return the constructed statement node
}

// Parse variable decl helper
Node *Parser2::parse_varDec() {
  // Stmt → var ident ;
  std::unique_ptr<Node> var_decl(expect(TOK_VAR));       // Consume 'var'
  std::unique_ptr<Node> ident(expect(TOK_IDENTIFIER));   // Consume identifier (e.g., 'a')
  
  // Create an AST_VARREF node for the variable reference
  std::unique_ptr<Node> var_ref(new Node(AST_VARREF));
  var_ref->set_str(ident->get_str());                    // Set the identifier string
  var_ref->set_loc(ident->get_loc());                    // Set the location info

  expect_and_discard(TOK_SEMICOLON);                     // Consume ';'

  // Create the AST_VARDEF node and append the VARREF node as a child
  std::unique_ptr<Node> var_def_node(new Node(AST_VARDEF, {var_ref.release()}));
  var_def_node->set_loc(var_decl->get_loc());            // Set the location to 'var'
  return var_def_node.release();                         // Return the constructed variable declaration node
}

Node *Parser2::parse_If() {
  //   Stmt →       if ( A ) { SList }                     -- if stmt
  // Stmt →       if ( A ) { SList } else { SList }      -- if/else stmt 
  std::unique_ptr<Node> if_tok(expect(TOK_IF));
  expect_and_discard(TOK_LPAREN);
  std::unique_ptr<Node> condition(parse_A());
  expect_and_discard(TOK_RPAREN);

  expect_and_discard(TOK_LBRACE);
  std::unique_ptr<Node> slist(parse_SList()); // parse statementlist
  expect_and_discard(TOK_RBRACE);

  Node *else_block = nullptr;

  // Check else
  Node *next_tok = m_lexer->peek();
  if (next_tok != nullptr && next_tok->get_tag() == TOK_ELSE) {
    std::unique_ptr<Node> else_tok(expect(TOK_ELSE));
    expect_and_discard(TOK_LBRACE);
    std::unique_ptr<Node> else_block_node(parse_SList());
    expect_and_discard(TOK_RBRACE);
    else_block = else_block_node.release();
  }

  // Create AST_IF nmode
  std::vector<Node *> children = {condition.release(), slist.release()};
  if (else_block != nullptr) {
    children.push_back(else_block);
  }
  std::unique_ptr<Node> if_node(new Node(AST_IF, children));
  if_node->set_loc(if_tok->get_loc());

  return if_node.release();
}

Node *Parser2::parse_While() {
  // Stmt → while ( A ) { SList }
  std::unique_ptr<Node> while_tok(expect(TOK_WHILE));

  expect_and_discard(TOK_LPAREN);
  std::unique_ptr<Node> condition(parse_A());
  expect_and_discard(TOK_RPAREN);

  expect_and_discard(TOK_LBRACE);
  std::unique_ptr<Node> slist(parse_SList());
  expect_and_discard(TOK_RBRACE);

  // Create AST_WHILE node
  std::vector<Node *> children = { condition.release(), slist.release() };
  std::unique_ptr<Node> while_node(new Node(AST_WHILE, children));
  while_node->set_loc(while_tok->get_loc());

  return while_node.release();
}

Node *Parser2::parse_SList() {
  std::unique_ptr<Node> slist (new Node(AST_STATEMENT_LIST));
  while (true) {
    Node *next_tok = m_lexer->peek();
    if (next_tok == nullptr || next_tok->get_tag() == TOK_RBRACE) {
      break;
    }
    slist->append_kid(parse_Stmt());
  }

  return slist.release();
}

Node *Parser2::parse_Func() {
  // Func → function ident ( OptPList ) { SList }
  std::unique_ptr<Node> func_tok(expect(TOK_FUNCTION));

  std::unique_ptr<Node> ident(expect(TOK_IDENTIFIER));
  Location func_loc = func_tok->get_loc();

  // create varref node
  std::unique_ptr<Node> func_name_node(new Node(AST_VARREF));
  func_name_node->set_str(ident->get_str());
  func_name_node->set_loc(ident->get_loc());

  expect_and_discard(TOK_LPAREN);
  std::unique_ptr<Node> parameter_list(parse_OptPList());
  expect_and_discard(TOK_RPAREN);

  expect_and_discard(TOK_LBRACE);
  std::unique_ptr<Node> slist(parse_SList());
  expect_and_discard(TOK_RBRACE);

  // Build the list of child nodes for the FUNCTION node
  std::vector<Node *> children;

  // add varref as first child
  children.push_back(func_name_node.release());

  if (parameter_list != nullptr) {
    children.push_back(parameter_list.release());
  }

  children.push_back(slist.release());

  std::unique_ptr<Node> func_node(new Node(AST_FUNCTION, children));
  func_node->set_loc(func_loc);

  return func_node.release();
}
  
Node *Parser2::parse_OptPList() {
  Node *next_tok = m_lexer->peek();
  if (next_tok != nullptr && next_tok->get_tag() == TOK_IDENTIFIER) {
    return parse_PList();
  }
  return nullptr; // epsilon
}

Node *Parser2::parse_PList() {
  std::unique_ptr<Node> plist(new Node(AST_PARAMETER_LIST));

  // parse first ident
  std::unique_ptr<Node> ident(expect(TOK_IDENTIFIER));
  std::unique_ptr<Node> var_ref(new Node(AST_VARREF));
  var_ref->set_str(ident->get_str());
  var_ref->set_loc(ident->get_loc());
  plist->append_kid(var_ref.release());

  // parse remaining params if any
  while (true) {
    Node *next_tok = m_lexer->peek();
    if (next_tok != nullptr && next_tok->get_tag() == TOK_COMMA) {
      expect_and_discard(TOK_COMMA);
      std::unique_ptr<Node> ident(expect(TOK_IDENTIFIER));
      std::unique_ptr<Node> var_ref(new Node(AST_VARREF));
      var_ref->set_str(ident->get_str());
      var_ref->set_loc(ident->get_loc());
      plist->append_kid(var_ref.release());
    } else {
      break;
    }
  }

  return plist.release();
}

Node *Parser2::parse_OptArgList() {
  Node *next_tok = m_lexer->peek();
  if (next_tok != nullptr && can_start_expression(next_tok)) {
    return parse_ArgList();
  }
  return nullptr; // epsilon
}

// Helper function to determine if a token can start an expression
bool Parser2::can_start_expression(Node *tok) {
  int tag = tok->get_tag();
  return tag == TOK_IDENTIFIER || tag == TOK_INTEGER_LITERAL || tag == TOK_LPAREN;
}

Node *Parser2::parse_ArgList() {
  std::unique_ptr<Node> arglist(new Node(AST_ARGLIST));

  // parse frist arg (L)
  std::unique_ptr<Node> arg(parse_L());
  arglist->append_kid(arg.release());

  // parse remaining args if any
  while (true) {
    Node *next_tok = m_lexer->peek();
    if (next_tok != nullptr && next_tok->get_tag() == TOK_COMMA) {
      expect_and_discard(TOK_COMMA);
      std::unique_ptr<Node> arg(parse_L()); //next arg
      arglist->append_kid(arg.release());
    } else {
      break; // no more args
    }
  }

  return arglist.release();
}


Node *Parser2::parse_E() {
  // E -> ^ T E'

  // Get the AST corresponding to the term (T)
  Node *ast = parse_T();

  // Recursively continue the additive expression
  return parse_EPrime(ast);
}

// This function is passed the "current" portion of the AST
// that has been built so far for the additive expression.
Node *Parser2::parse_EPrime(Node *ast_) {
  // E' -> ^ + T E'
  // E' -> ^ - T E'
  // E' -> ^ epsilon

  std::unique_ptr<Node> ast(ast_);

  // peek at next token
  Node *next_tok = m_lexer->peek();
  if (next_tok != nullptr) {
    int next_tok_tag = next_tok->get_tag();
    if (next_tok_tag == TOK_PLUS || next_tok_tag == TOK_MINUS)  {
      // E' -> ^ + T E'
      // E' -> ^ - T E'
      std::unique_ptr<Node> op(expect(static_cast<enum TokenKind>(next_tok_tag)));

      // build AST for next term, incorporate into current AST
      Node *term_ast = parse_T();
      ast.reset(new Node(next_tok_tag == TOK_PLUS ? AST_ADD : AST_SUB, {ast.release(), term_ast}));

      // copy source information from operator node
      ast->set_loc(op->get_loc());

      // continue recursively
      return parse_EPrime(ast.release());
    }
  }

  // E' -> ^ epsilon
  // No more additive operators, so just return the completed AST
  return ast.release();
}

Node *Parser2::parse_T() {
  // T -> F T'

  // Parse primary expression
  Node *ast = parse_F();

  // Recursively continue the multiplicative expression
  return parse_TPrime(ast);
}

Node *Parser2::parse_TPrime(Node *ast_) {
  // T' -> ^ * F T'
  // T' -> ^ / F T'
  // T' -> ^ epsilon

  std::unique_ptr<Node> ast(ast_);

  // peek at next token
  Node *next_tok = m_lexer->peek();
  if (next_tok != nullptr) {
    int next_tok_tag = next_tok->get_tag();
    if (next_tok_tag == TOK_TIMES || next_tok_tag == TOK_DIVIDE)  {
      // T' -> ^ * F T'
      // T' -> ^ / F T'
      std::unique_ptr<Node> op(expect(static_cast<enum TokenKind>(next_tok_tag)));

      // build AST for next primary expression, incorporate into current AST
      Node *primary_ast = parse_F();
      ast.reset(new Node(next_tok_tag == TOK_TIMES ? AST_MULTIPLY : AST_DIVIDE, {ast.release(), primary_ast}));

      // copy source information from operator node
      ast->set_loc(op->get_loc());

      // continue recursively
      return parse_TPrime(ast.release());
    }
  }

  // T' -> ^ epsilon
  // No more multiplicative operators, so just return the completed AST
  return ast.release();
}

Node *Parser2::parse_F() {
  // F -> ^ number
  // F -> ^ ident
  // F -> ^ ( E )
  // F →          ident ( OptArgList )             -- function call

  Node *next_tok = m_lexer->peek();
  if (next_tok == nullptr) {
    error_at_current_loc("Unexpected end of input looking for primary expression");
  }

  int tag = next_tok->get_tag();
  
  if (tag == TOK_IDENTIFIER) {
    // Could be function call or var reference
    std::unique_ptr<Node> ident(expect(TOK_IDENTIFIER));
    Node *next_tok = m_lexer->peek();

    if (next_tok != nullptr && next_tok->get_tag() == TOK_LPAREN) {
      // Function call
      expect_and_discard(TOK_LPAREN);
      std::unique_ptr<Node> arglist(parse_OptArgList());
      expect_and_discard(TOK_RPAREN);

      // AST_FNCALL
      std::vector<Node *> children;
      std::unique_ptr<Node> var_ref(new Node(AST_VARREF));
      var_ref->set_str(ident->get_str());
      var_ref->set_loc(ident->get_loc());
      children.push_back(var_ref.release());

      if (arglist != nullptr) {
        children.push_back(arglist.release());
      }

      std::unique_ptr<Node> fncall(new Node(AST_FNCALL, children));
      fncall->set_loc(ident->get_loc());

      return fncall.release();

    } else {
      // Variable reference identifier
      std::unique_ptr<Node> var_ref(new Node(AST_VARREF));
      var_ref->set_str(ident->get_str());
      var_ref->set_loc(ident->get_loc());
      return var_ref.release();
    }
  } else if (tag == TOK_INTEGER_LITERAL) {
    // F -> number
    std::unique_ptr<Node> tok(expect(TOK_INTEGER_LITERAL));
    std::unique_ptr<Node> ast(new Node(AST_INT_LITERAL));

    ast->set_str(tok->get_str());
    ast->set_loc(tok->get_loc());

    return ast.release();
  } else if (tag == TOK_LPAREN) {
    // F -> ( E )
    expect_and_discard(TOK_LPAREN);
    std::unique_ptr<Node> ast(parse_A());
    expect_and_discard(TOK_RPAREN);
    return ast.release();
  } else {
    SyntaxError::raise(next_tok->get_loc(), "Invalid primary expression");
  }
}

// Milestone 1 grammar:
// Stmt → var ident ;
// A    → ident = A
// A    → L
// L    → R || R
// L    → R && R
// L    → R
// R    → E < E
// R    → E <= E
// R    → E > E
// R    → E >= E
// R    → E == E
// R    → E != E
// R    → E

Node *Parser2::parse_A() {
  // A    → ident = A
  // A    → L
  Node *next_tok = m_lexer->peek();
  if (next_tok == nullptr) {
    error_at_current_loc("Unexpected end of input looking for assignment or expression");
  }

  if (next_tok->get_tag() == TOK_IDENTIFIER) {
    // Peek two tokens ahead to check for the assignment operator (`=`)
    Node *next_next_tok = m_lexer->peek(2);
    
    if (next_next_tok != nullptr && next_next_tok->get_tag() == TOK_EQUAL) {
      // If the second token is '=', it's an assignment: A → ident = A
      std::unique_ptr<Node> ident(expect(TOK_IDENTIFIER));  // Consume identifier
      
      // Create an AST_VARREF node for the left-hand side variable reference
      std::unique_ptr<Node> var_ref(new Node(AST_VARREF));
      var_ref->set_str(ident->get_str());
      var_ref->set_loc(ident->get_loc());
      
      std::unique_ptr<Node> assign_op(expect(TOK_EQUAL));   // Consume '='
      std::unique_ptr<Node> rhs(parse_A());                 // Parse the right-hand side of the assignment

      // Create AST node for assignment operation
      std::unique_ptr<Node> assign_node(new Node(AST_ASSIGN, {var_ref.release(), rhs.release()}));
      assign_node->set_loc(assign_op->get_loc());            // Set location info from the assignment operator
      return assign_node.release();
    }
  }

  return parse_L(); // Not an assignment, so handle it as an expression
}


Node *Parser2::parse_L() {
  // L    → R || R
  // L    → R && R
  // L    → R
  std::unique_ptr<Node> ast(parse_R());

  Node *next_tok = m_lexer->peek();
  if (next_tok != nullptr) {
    if (next_tok->get_tag() == TOK_DOUBLE_PIPE || next_tok->get_tag() == TOK_DOUBLE_AMPERSAND) {
      std::unique_ptr<Node> logical_op(expect(next_tok->get_tag() == TOK_DOUBLE_PIPE ? TOK_DOUBLE_PIPE : TOK_DOUBLE_AMPERSAND));
      std::unique_ptr<Node> rhs(parse_R());
      int ast_tag = (next_tok->get_tag() == TOK_DOUBLE_PIPE) ? AST_LOGICAL_OR : AST_LOGICAL_AND;
      std::unique_ptr<Node> logical_node(new Node(ast_tag, {ast.release(), rhs.release()}));
      logical_node->set_loc(logical_op->get_loc());
      return logical_node.release();
    }
  }

  return ast.release();
}

Node *Parser2::parse_R() {
  std::unique_ptr<Node> ast(parse_E());

  Node *next_tok = m_lexer->peek();

  if (next_tok != nullptr) {
    int tok_tag = next_tok->get_tag();
    if (tok_tag == TOK_LESS || tok_tag == TOK_LESS_EQUAL || tok_tag == TOK_GREATER || tok_tag == TOK_GREATER_EQUAL || 
        tok_tag == TOK_DOUBLE_EQUAL || tok_tag == TOK_NOT_EQUAL) {
      std::unique_ptr<Node> rel_op(expect(static_cast<TokenKind>(tok_tag)));
      std::unique_ptr<Node> rhs(parse_E());
      int ast_tag; 
      switch (tok_tag) {
        case TOK_LESS: ast_tag = AST_LESS; break;
        case TOK_LESS_EQUAL: ast_tag = AST_LESS_EQUAL; break;
        case TOK_GREATER: ast_tag = AST_GREATER; break;
        case TOK_GREATER_EQUAL: ast_tag = AST_GREATER_EQUAL; break;
        case TOK_DOUBLE_EQUAL: ast_tag = AST_EQUAL; break;
        case TOK_NOT_EQUAL: ast_tag = AST_NOT_EQUAL; break;
        default: error_at_current_loc("Unexpected relational operator");
      } 

      std::unique_ptr<Node> rel_node(new Node(ast_tag, {ast.release(), rhs.release()}));
      rel_node->set_loc(rel_op->get_loc());
      return rel_node.release();
    }
  }

  return ast.release();
}

Node *Parser2::expect(enum TokenKind tok_kind) {
  std::unique_ptr<Node> next_terminal(m_lexer->next());
  if (next_terminal->get_tag() != tok_kind) {
    SyntaxError::raise(next_terminal->get_loc(), "Unexpected token '%s'", next_terminal->get_str().c_str());
  }
  return next_terminal.release();
}

void Parser2::expect_and_discard(enum TokenKind tok_kind) {
  Node *tok = expect(tok_kind);
  delete tok;
}

void Parser2::error_at_current_loc(const std::string &msg) {
  SyntaxError::raise(m_lexer->get_current_loc(), "%s", msg.c_str());
}
