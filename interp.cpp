#include <cassert>
#include <algorithm>
#include <memory>
#include <unordered_set>
#include "ast.h"
#include "node.h"
#include "exceptions.h"
#include "function.h"
#include "interp.h"
#include "environment.h"


Interpreter::Interpreter(Node *ast_to_adopt)
  : m_ast(ast_to_adopt), m_env(Environment(nullptr)) {
}

Interpreter::~Interpreter() {
  delete m_ast;
}

void Interpreter::analyze_node(Node* node, Environment& env) {
    if (!node) return;

    switch (node->get_tag()) {
        case AST_VARDEF: {
            Node* var_name_node = node->get_kid(0);
            assert(var_name_node->get_tag() == AST_VARREF);
            std::string var_name = var_name_node->get_str();
            env.define_variable(var_name, Value(0)); // Just mark as defined
            break;
        }
        case AST_VARREF: {
            std::string var_name = node->get_str();
            if (!env.is_defined(var_name)) {
                SemanticError::raise(node->get_loc(), "Variable '%s' referenced before definition.", var_name.c_str());
            }
            break;
        }
        default: {
            for (unsigned i = 0; i < node->get_num_kids(); ++i) {
                analyze_node(node->get_kid(i), env);
            }
            break;
        }
    }
}

// Analyze the AST for variable usage by leveraging the Environment class
void Interpreter::analyze() {
    // Create a new environment for analysis phase (no parent environment for now)
    Environment analysis_env(nullptr);
    analyze_node(m_ast, analysis_env);
}

// Helper function to evaluate binary operations
Value get_operator_result(const std::string& op, const Value& left, const Value& right, const Location& loc) {
    int left_val = left.get_ival();
    int right_val = right.get_ival();

    if (op == "+") return Value(left_val + right_val);
    if (op == "-") return Value(left_val - right_val);
    if (op == "*") return Value(left_val * right_val);
    if (op == "/") {
        if (right_val == 0) {
            EvaluationError::raise(loc, "Division by zero.");
        }
        return Value(left_val / right_val);
    }
    if (op == "&&") return Value((left_val != 0 && right_val != 0) ? 1 : 0);
    if (op == "||") return Value((left_val != 0 || right_val != 0) ? 1 : 0);
    if (op == ">") return Value(left_val > right_val ? 1 : 0);
    if (op == ">=") return Value(left_val >= right_val ? 1 : 0);
    if (op == "<") return Value(left_val < right_val ? 1 : 0);
    if (op == "<=") return Value(left_val <= right_val ? 1 : 0);
    if (op == "==") return Value(left_val == right_val ? 1 : 0);
    if (op == "!=") return Value(left_val != right_val ? 1 : 0);

    // Unknown operator
    RuntimeError::raise("Unknown operator '%s'", op.c_str());
    return Value(0); // Unreachable
}

// Helper function to evaluate expressions
Value Interpreter::evaluate(Node* node) {
    if (!node) {
        RuntimeError::raise("Null node encountered during evaluation.");
    }

    switch (node->get_tag()) {
        case AST_INT_LITERAL: {
            int val = std::stoi(node->get_str());
            return Value(val);
        }
        case AST_VARREF: {
            std::string var_name = node->get_str();
            if (!m_env.is_defined(var_name)) {
                RuntimeError::raise("Undefined variable '%s' during execution.", var_name.c_str());
            }
            return m_env.get_variable(var_name);
        }
        case AST_VARDEF: {
            Node* var_name_node = node->get_kid(0);
            assert(var_name_node->get_tag() == AST_VARREF);
            std::string var_name = var_name_node->get_str();
            m_env.define_variable(var_name, Value(0)); // Define the variable with default value 0
            return Value(0);
        }
        case AST_ASSIGN: {
            Node* var_ref_node = node->get_kid(0);
            Node* expr_node = node->get_kid(1);
            assert(var_ref_node->get_tag() == AST_VARREF);
            std::string var_name = var_ref_node->get_str();
            Value expr_val = evaluate(expr_node);
            if (!m_env.is_defined(var_name)) {
                RuntimeError::raise("Assignment to undefined variable '%s'.", var_name.c_str());
            }
            m_env.set_variable(var_name, expr_val);
            return expr_val;
        }
        // Binary operations
        case AST_ADD:
        case AST_SUB:
        case AST_MULTIPLY:
        case AST_DIVIDE:
        case AST_LOGICAL_AND:
        case AST_LOGICAL_OR:
        case AST_GREATER:
        case AST_GREATER_EQUAL:
        case AST_LESS:
        case AST_LESS_EQUAL:
        case AST_EQUAL:
        case AST_NOT_EQUAL: {
            Node* left_node = node->get_kid(0);
            Node* right_node = node->get_kid(1);
            Value left_val = evaluate(left_node);
            Value right_val = evaluate(right_node);
            std::string op;

            switch (node->get_tag()) {
                case AST_ADD: op = "+"; break;
                case AST_SUB: op = "-"; break;
                case AST_MULTIPLY: op = "*"; break;
                case AST_DIVIDE: op = "/"; break;
                case AST_LOGICAL_AND: op = "&&"; break;
                case AST_LOGICAL_OR: op = "||"; break;
                case AST_GREATER: op = ">"; break;
                case AST_GREATER_EQUAL: op = ">="; break;
                case AST_LESS: op = "<"; break;
                case AST_LESS_EQUAL: op = "<="; break;
                case AST_EQUAL: op = "=="; break;
                case AST_NOT_EQUAL: op = "!="; break;
                default:
                    RuntimeError::raise("Unknown binary operator during evaluation.");
            }

            // Evaluate the operator and return the result
            return get_operator_result(op, left_val, right_val, node->get_loc());
        }
        case AST_STATEMENT: {
            Node* stmt_node = node->get_kid(0);
            return evaluate(stmt_node);
        }
        case AST_UNIT: {
            Value last_val(0);
            for (unsigned i = 0; i < node->get_num_kids(); ++i) {
                last_val = evaluate(node->get_kid(i));
            }
            return last_val;
        }
        default:
            RuntimeError::raise("Unknown AST node type %d during evaluation.", node->get_tag());
    }
    return Value(0); // Unreachable
}

// Execute the program
Value Interpreter::execute() {
    return evaluate(m_ast);
}