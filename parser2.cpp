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

  std::unique_ptr<Node> unit(new Node(AST_UNIT));
  for (;;) {
    unit->append_kid(parse_Stmt());
    if (m_lexer->peek() == nullptr)
      break;
  }

  return unit.release();
}

Node *Parser2::parse_Stmt() {
  std::unique_ptr<Node> stmt(new Node(AST_STATEMENT));

  // Peek at the next token to decide what kind of statement we are parsing
  Node *next_tok = m_lexer->peek();
  if (next_tok == nullptr) {
    SyntaxError::raise(m_lexer->get_current_loc(), "Unexpected end of input looking for statement");
  }

  // Check if the next token is 'var' (indicating a variable declaration)
  if (next_tok->get_tag() == TOK_VAR) {
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
    stmt->append_kid(var_def_node.release());              // Append the variable definition node to the statement

  } else {
    // Handle other statements, like assignments or expressions
    // Stmt → A ;
    stmt->append_kid(parse_A());                           // Parse the A non-terminal (assignment or expression)
    expect_and_discard(TOK_SEMICOLON);                     // Consume the semicolon at the end of the statement
  }

  return stmt.release();  // Return the constructed statement node
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

  Node *next_tok = m_lexer->peek();
  if (next_tok == nullptr) {
    error_at_current_loc("Unexpected end of input looking for primary expression");
  }

  int tag = next_tok->get_tag();
  if (tag == TOK_INTEGER_LITERAL || tag == TOK_IDENTIFIER) {
    // F -> ^ number
    // F -> ^ ident
    std::unique_ptr<Node> tok(expect(static_cast<enum TokenKind>(tag)));
    int ast_tag = tag == TOK_INTEGER_LITERAL ? AST_INT_LITERAL : AST_VARREF;
    std::unique_ptr<Node> ast(new Node(ast_tag));
    ast->set_str(tok->get_str());
    ast->set_loc(tok->get_loc());
    return ast.release();
  } else if (tag == TOK_LPAREN) {
    // F -> ^ ( E )
    expect_and_discard(TOK_LPAREN);
    // std::unique_ptr<Node> ast(parse_E());
    std::unique_ptr<Node> ast(parse_A()); // F -> ( A ) instead of ( E )
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
