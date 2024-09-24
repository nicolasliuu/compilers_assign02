#include <map>
#include <cassert>
#include <cctype>
#include <string>
#include "cpputil.h"
#include "token.h"
#include "exceptions.h"
#include "lexer.h"
#include <iostream>

////////////////////////////////////////////////////////////////////////
// Lexer implementation
////////////////////////////////////////////////////////////////////////

Lexer::Lexer(FILE *in, const std::string &filename)
  : m_in(in)
  , m_filename(filename)
  , m_line(1)
  , m_col(1)
  , m_eof(false) {
}

Lexer::~Lexer() {
  // delete any cached lookahead tokens
  for (auto i = m_lookahead.begin(); i != m_lookahead.end(); ++i) {
    delete *i;
  }
  fclose(m_in);
}

Node *Lexer::next() {
  fill(1);
  if (m_lookahead.empty()) {
    SyntaxError::raise(get_current_loc(), "Unexpected end of input");
  }
  Node *tok = m_lookahead.front();
  m_lookahead.pop_front();
  return tok;
}

Node *Lexer::peek(int how_many) {
  // try to get as many lookahead tokens as required
  fill(how_many);

  // if there aren't enough lookahead tokens,
  // then the input ended before the token we want
  if (int(m_lookahead.size()) < how_many) {
    return nullptr;
  }

  // return the pointer to the Node representing the token
  return m_lookahead.at(how_many - 1);
}

Location Lexer::get_current_loc() const {
  return Location(m_filename, m_line, m_col);
}

// Read the next character of input, returning -1 (and setting m_eof to true)
// if the end of input has been reached.
int Lexer::read() {
  if (m_eof) {
    return -1;
  }
  int c = fgetc(m_in);
  if (c < 0) {
    m_eof = true;
  } else if (c == '\n') {
    m_col = 1;
    m_line++;
  } else {
    m_col++;
  }
  return c;
}

// "Unread" a character.  Useful for when reading a character indicates
// that the current token has ended and the next one has begun.
void Lexer::unread(int c) {
  ungetc(c, m_in);
  if (c == '\n') {
    m_line--;
    m_col = 99;
  } else {
    m_col--;
  }
}

void Lexer::fill(int how_many) {
  assert(how_many > 0);
  while (!m_eof && int(m_lookahead.size()) < how_many) {
    Node *tok = read_token();
    if (tok != nullptr) {
      m_lookahead.push_back(tok);
    }
  }
}

Node *Lexer::read_token() {
  int c, line = -1, col = -1;

  // Skip whitespace characters until a non-whitespace character is read
  for (;;) {
    line = m_line;
    col = m_col;
    c = read();
    if (c < 0 || !isspace(c)) {
      break;
    }
  }

  if (c < 0) {
    // Reached end of file
    return nullptr;
  }

  std::string lexeme;
  lexeme.push_back(char(c));

  if (isalpha(c)) {
    // Handle identifiers and keywords
    
    return handle_identifier_or_keyword(c, lexeme, line, col);
  } else if (isdigit(c)) {
    // Handle integer literals
    return read_continued_token(TOK_INTEGER_LITERAL, lexeme, line, col, isdigit);
  } else {
    // Handle possible multi-character tokens and other single characters
    return handle_token(c, lexeme, line, col);
  }
}

// Helper function to create a Node object to represent a token.
Node *Lexer::token_create(enum TokenKind kind, const std::string &lexeme, int line, int col) {
  Node *token = new Node(kind, lexeme);
  Location source_info(m_filename, line, col);
  token->set_loc(source_info);
  return token;
}

// Read the continuation of a (possibly) multi-character token, such as
// an identifier or integer literal.  pred is a pointer to a predicate
// function to determine which characters are valid continuations.
Node *Lexer::read_continued_token(enum TokenKind kind, const std::string &lexeme_start, int line, int col, int (*pred)(int)) {
  std::string lexeme(lexeme_start);
  for (;;) {
    int c = read();
    if (c >= 0 && pred(c)) {
      // token has finished
      lexeme.push_back(char(c));
    } else {
      if (c >= 0) {
        unread(c);
      }
      return token_create(kind, lexeme, line, col);
    }
  }
}

Node *Lexer::handle_identifier_or_keyword(int c, std::string &lexeme, int line, int col) {
  // Read the full identifier or keyword (consisting of alphanumeric characters)
  Node *tok = read_continued_token(TOK_IDENTIFIER, lexeme, line, col, isalnum);

  // Check if "var"
  if (tok->get_str() == "var") {
    Node *var_tok = token_create(TOK_VAR, tok->get_str(), line, col);  // Create TOK_VAR token
    delete tok;  
    return var_tok;  
  } else if (tok->get_str() == "function") {
    Node *var_tok = token_create(TOK_FUNCTION, tok->get_str(), line, col);  // Create TOK_FUNCTION token
    delete tok; 
    return var_tok; 
  } else if (tok->get_str() == "if") {
    Node *var_tok = token_create(TOK_IF, tok->get_str(), line, col);  // Create TOK_IF token
    delete tok;  
    return var_tok; 
  } else if (tok->get_str() == "else") {
    Node *var_tok = token_create(TOK_ELSE, tok->get_str(), line, col);  // Create TOK_ELSE token
    delete tok;  
    return var_tok; 
  } else if (tok->get_str() == "while") {
    Node *var_tok = token_create(TOK_WHILE, tok->get_str(), line, col);  // Create TOK_WHILE token
    delete tok;  
    return var_tok; 
  }

  // Otherwise, just return the token as an identifier
  return tok;
}


Node *Lexer::handle_token(int c, std::string &lexeme, int line, int col) {
  switch (c) {
    case '+':
      return token_create(TOK_PLUS, lexeme, line, col);
    case '-':
      return token_create(TOK_MINUS, lexeme, line, col);
    case '*':
      return token_create(TOK_TIMES, lexeme, line, col);
    case '/':
      return token_create(TOK_DIVIDE, lexeme, line, col);
    case '(':
      return token_create(TOK_LPAREN, lexeme, line, col);
    case ')':
      return token_create(TOK_RPAREN, lexeme, line, col);
    case ';':
      return token_create(TOK_SEMICOLON, lexeme, line, col);
    case '&': {
      Node *token = check_and_create_double_char_token('&', TOK_DOUBLE_AMPERSAND, lexeme, line, col);
      if (token) return token;
      SyntaxError::raise(get_current_loc(), "Unexpected character '&' (expected '&&')");
      return nullptr;
    }
    case '|': {
      Node *token = check_and_create_double_char_token('|', TOK_DOUBLE_PIPE, lexeme, line, col);
      if (token) return token;
      SyntaxError::raise(get_current_loc(), "Unexpected character '|' (expected '||')");
      return nullptr;
    }
    case '=': {
      int next_char = read();
      if (next_char == '=') {
        lexeme.push_back(char(next_char));
        return token_create(TOK_DOUBLE_EQUAL, lexeme, line, col);
      } else {
        unread(next_char);
        return token_create(TOK_EQUAL, lexeme, line, col);
      }
    }
    case '<': {
      int next_char = read();
      if (next_char == '=') {
        lexeme.push_back(char(next_char));
        return token_create(TOK_LESS_EQUAL, lexeme, line, col);
      } else {
        unread(next_char);
        return token_create(TOK_LESS, lexeme, line, col);
      }
    }
    case '>': {
      int next_char = read();
      if (next_char == '=') {
        lexeme.push_back(char(next_char));
        return token_create(TOK_GREATER_EQUAL, lexeme, line, col);
      } else {
        unread(next_char);
        return token_create(TOK_GREATER, lexeme, line, col);
      }
    }
    case '!': {
      int next_char = read();
      if (next_char == '=') {
        lexeme.push_back(char(next_char));
        return token_create(TOK_NOT_EQUAL, lexeme, line, col);
      } else {
        unread(next_char);
        SyntaxError::raise(get_current_loc(), "Unexpected character '!' (expected '!=')");
        return nullptr;
      }
    }
    // Grouping/sequencing tokens
    case '{':
      return token_create(TOK_LBRACE, lexeme, line, col);
    case '}':
      return token_create(TOK_RBRACE, lexeme, line, col);
    case ',':
      return token_create(TOK_COMMA, lexeme, line, col);
    default:
      SyntaxError::raise(get_current_loc(), "Unrecognized character '%c'", c);
      return nullptr;
  }
}

Node *Lexer::check_and_create_double_char_token(char expected_next, TokenKind double_kind, std::string &lexeme, int line, int col) {
  int next_char = read();
  if (next_char == expected_next) {
    lexeme.push_back(char(next_char));
    return token_create(double_kind, lexeme, line, col);
  } else {
    unread(next_char);
    // Return nullptr when the double-character token is not formed.
    return nullptr;
  }
}