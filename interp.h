#ifndef INTERP_H
#define INTERP_H

#include "value.h"
#include "environment.h"
class Node;
class Location;

class Interpreter {
private:
  Node *m_ast;
  Environment *m_env;

public:
  Interpreter(Node *ast_to_adopt);
  Interpreter(Node *ast_to_adopt, Environment *env);
  ~Interpreter();

  void analyze();
  Value execute();

private:
    // Helper functions for analysis
    void analyze_node(Node* node, Environment& env);

    // Helper functions for execution
    Value evaluate(Node* node, Environment& env);

    static Value intrinsic_print(Value args[], unsigned num_args, const Location &loc, Interpreter *interp);
    static Value intrinsic_println(Value args[], unsigned num_args, const Location &loc, Interpreter *interp);
};

#endif // INTERP_H
