#ifndef TOKEN_H
#define TOKEN_H

// This header file defines the tags used for tokens (i.e., terminal
// symbols in the grammar.)

enum TokenKind {
  TOK_IDENTIFIER,
  TOK_INTEGER_LITERAL,
  TOK_PLUS,
  TOK_MINUS,
  TOK_TIMES,
  TOK_DIVIDE,
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_SEMICOLON,
  TOK_AMPERSAND,       // Single '&'
  TOK_PIPE,            // Single '|'
  TOK_GREATER,         // Single '>'
  TOK_LESS,            // Single '<'
  TOK_EXCLAMATION,     // Single '!'
  TOK_EQUAL,           // Single '='
  TOK_VAR,
  TOK_DOUBLE_AMPERSAND, // '&&'
  TOK_DOUBLE_PIPE,      // '||'
  TOK_LESS_EQUAL,       // '<='
  TOK_GREATER_EQUAL,    // '>='
  TOK_DOUBLE_EQUAL,     // '=='
  TOK_NOT_EQUAL,         // '!='

  // Keyword tokens
  TOK_FUNCTION,
  TOK_IF,
  TOK_ELSE,
  TOK_WHILE,

  // Grouping/sequencing tokens
  TOK_LBRACE,
  TOK_RBRACE,
  TOK_COMMA
};

#endif // TOKEN_H
