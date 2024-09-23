#ifndef INTERP_H
#define INTERP_H

#include "value.h"
#include "environment.h"
class Node;
class Location;

class Interpreter {
private:
  Node *m_ast;
  Environment m_env;

public:
  Interpreter(Node *ast_to_adopt);
  Interpreter(Node *ast_to_adopt, Environment env);
  ~Interpreter();

  void analyze();
  Value execute();

private:
    // Helper functions for analysis
    void analyze_node(Node* node, Environment& env);

    // Helper functions for execution
    Value evaluate(Node* node);
};

#endif // INTERP_H
