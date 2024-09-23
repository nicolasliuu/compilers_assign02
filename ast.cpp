#include "exceptions.h"
#include "ast.h"

ASTTreePrint::ASTTreePrint() {
}

ASTTreePrint::~ASTTreePrint() {
}

std::string ASTTreePrint::node_tag_to_string(int tag) const {
  switch (tag) {
  case AST_ADD:
    return "ADD";
  case AST_SUB:
    return "SUB";
  case AST_MULTIPLY:
    return "MULTIPLY";
  case AST_DIVIDE:
    return "DIVIDE";
  case AST_VARREF:
    return "VARREF";
  case AST_INT_LITERAL:
    return "INT_LITERAL";
  case AST_UNIT:
    return "UNIT";
  case AST_STATEMENT:
    return "STATEMENT";
  case AST_VARDEF: 
    return "VARDEF";
  case AST_ASSIGN: 
    return "ASSIGN";
  case AST_LOGICAL_OR: 
    return "LOGICAL_OR";
  case AST_LOGICAL_AND: 
    return "LOGICAL_AND";
  case AST_LESS: 
    return "LESS";
  case AST_LESS_EQUAL: 
    return "LESS_EQUAL";
  case AST_GREATER: 
    return "GREATER";
  case AST_GREATER_EQUAL: 
    return "GREATER_EQUAL";
  case AST_EQUAL: 
    return "EQUAL";
  case AST_NOT_EQUAL: 
    return "NOT_EQUAL";
  default:
    RuntimeError::raise("Unknown AST node type %d\n", tag);
  }
}
