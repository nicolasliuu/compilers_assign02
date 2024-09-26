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
  : m_ast(ast_to_adopt), m_env(new Environment(nullptr)) {

    // Bind intrinsic functions
    m_env->define_variable("print", Value(&Interpreter::intrinsic_print));
    m_env->define_variable("println", Value(&Interpreter::intrinsic_println));
}

Interpreter::Interpreter(Node *ast_to_adopt, Environment *env)
  : m_ast(ast_to_adopt), m_env(new Environment(env)) {

    // Bind intrinsic functions
    m_env->define_variable("print", Value(&Interpreter::intrinsic_print));
    m_env->define_variable("println", Value(&Interpreter::intrinsic_println));
}

Interpreter::~Interpreter() {
  delete m_ast;
  delete m_env;
}

void Interpreter::analyze_node(Node* node, Environment& env) {
    if (!node) return;

    switch (node->get_tag()) {
        case AST_VARDEF: {
            Node* var_name_node = node->get_kid(0);
            std::string var_name = var_name_node->get_str();
            if (env.is_defined_in_current(var_name)) {
               EvaluationError::raise(node->get_loc(), "Variable '%s' already defined in this scope.", var_name.c_str());
            }
            env.define_variable(var_name, Value(0)); // Define with default value 0
            return;
        }
        case AST_VARREF: {
            std::string var_name = node->get_str();
            if (!env.is_defined(var_name)) {
                SemanticError::raise(node->get_loc(), "Variable '%s' referenced before definition.", var_name.c_str());
            }
            return;
        }
        case AST_STATEMENT_LIST: {
            Environment block_env(&env); // New scope
            for (unsigned i = 0; i < node->get_num_kids(); ++i) {
                analyze_node(node->get_kid(i), block_env);
            }
            return;
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
    // Create a new environment for analysis phase
    Environment analysis_env(m_env);
    analyze_node(m_ast, analysis_env);
}

// Helper function to evaluate expressions
Value Interpreter::evaluate(Node* node, Environment& env) {
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
            if (!env.is_defined(var_name)) {
                RuntimeError::raise("Undefined variable '%s' during execution.", var_name.c_str());
            }
            return env.get_variable(var_name);
        }
        case AST_VARDEF: {
            Node* var_name_node = node->get_kid(0);
            assert(var_name_node->get_tag() == AST_VARREF);
            std::string var_name = var_name_node->get_str();
            if (env.is_defined_in_current(var_name)) {
                EvaluationError::raise(node->get_loc(), "Variable '%s' already defined in this scope.", var_name.c_str());
            }
            env.define_variable(var_name, Value(0)); // Define with default value 0
            return Value(0);
        }
        case AST_ASSIGN: {
            Node* var_ref_node = node->get_kid(0);
            Node* expr_node = node->get_kid(1);
            std::string var_name = var_ref_node->get_str();
            Value expr_val = evaluate(expr_node, env);
            if (!env.is_defined(var_name)) {
                SemanticError::raise(node->get_loc(), "Assignment to undefined variable '%s'.", var_name.c_str());
            }
            env.set_variable(var_name, expr_val);
            return expr_val;
        }

        // Binary operations
        case AST_ADD: {
            Node* left_node = node->get_kid(0);
            Node* right_node = node->get_kid(1);
            Value left_val = evaluate(left_node, env);
            Value right_val = evaluate(right_node, env);
            return Value(left_val.get_ival() + right_val.get_ival());
        }
        case AST_SUB: {
            Node* left_node = node->get_kid(0);
            Node* right_node = node->get_kid(1);
            Value left_val = evaluate(left_node, env);
            Value right_val = evaluate(right_node, env);
            return Value(left_val.get_ival() - right_val.get_ival());
        }
        case AST_MULTIPLY: {
            Node* left_node = node->get_kid(0);
            Node* right_node = node->get_kid(1);
            Value left_val = evaluate(left_node, env);
            Value right_val = evaluate(right_node, env);
            return Value(left_val.get_ival() * right_val.get_ival());
        }
        case AST_DIVIDE: {
            Node* left_node = node->get_kid(0);
            Node* right_node = node->get_kid(1);
            Value left_val = evaluate(left_node, env);
            Value right_val = evaluate(right_node, env);
            if (right_val.get_ival() == 0) {
                EvaluationError::raise(node->get_loc(), "Division by zero.");
            }
            return Value(left_val.get_ival() / right_val.get_ival());
        }
        case AST_LOGICAL_AND: {
            Value left_val = evaluate(node->get_kid(0), env);
            if (!left_val.is_int()) {
                EvaluationError::raise(node->get_loc(), "Operand must be an integer.");
            }

            if (left_val.get_ival() == 0) {
                return Value(0); // Short-circuit
            } else {
                Value right_val = evaluate(node->get_kid(1), env);
                if (!right_val.is_int()) {
                EvaluationError::raise(node->get_loc(), "Operand must be an integer.");
                }
                return Value(right_val.get_ival() != 0 ? 1 : 0);
            }
        }
        case AST_LOGICAL_OR: {
            Value left_val = evaluate(node->get_kid(0), env);
            if (!left_val.is_int()) {
                EvaluationError::raise(node->get_loc(), "Operand must be an integer.");
            }

            if (left_val.get_ival() != 0) {
                return Value(1); // Short-circuit
            } else {
                Value right_val = evaluate(node->get_kid(1), env);
                if (!right_val.is_int()) {
                EvaluationError::raise(node->get_loc(), "Operand must be an integer.");
                }
                return Value(right_val.get_ival() != 0 ? 1 : 0);
            }
        }
        case AST_GREATER: {
            Node* left_node = node->get_kid(0);
            Node* right_node = node->get_kid(1);
            Value left_val = evaluate(left_node, env);
            Value right_val = evaluate(right_node, env);
            return Value(left_val.get_ival() > right_val.get_ival() ? 1 : 0);
        }
        case AST_GREATER_EQUAL: {
            Node* left_node = node->get_kid(0);
            Node* right_node = node->get_kid(1);
            Value left_val = evaluate(left_node, env);
            Value right_val = evaluate(right_node, env);
            return Value(left_val.get_ival() >= right_val.get_ival() ? 1 : 0);
        }
        case AST_LESS: {
            Node* left_node = node->get_kid(0);
            Node* right_node = node->get_kid(1);
            Value left_val = evaluate(left_node, env);
            Value right_val = evaluate(right_node, env);
            return Value(left_val.get_ival() < right_val.get_ival() ? 1 : 0);
        }
        case AST_LESS_EQUAL: {
            Node* left_node = node->get_kid(0);
            Node* right_node = node->get_kid(1);
            Value left_val = evaluate(left_node, env);
            Value right_val = evaluate(right_node, env);
            return Value(left_val.get_ival() <= right_val.get_ival() ? 1 : 0);
        }
        case AST_EQUAL: {
            Node* left_node = node->get_kid(0);
            Node* right_node = node->get_kid(1);
            Value left_val = evaluate(left_node, env);
            Value right_val = evaluate(right_node, env);
            return Value(left_val.get_ival() == right_val.get_ival() ? 1 : 0);
        }
        case AST_NOT_EQUAL: {
            Node* left_node = node->get_kid(0);
            Node* right_node = node->get_kid(1);
            Value left_val = evaluate(left_node, env);
            Value right_val = evaluate(right_node, env);
            return Value(left_val.get_ival() != right_val.get_ival() ? 1 : 0);
        }
        case AST_STATEMENT: {
            Node* stmt_node = node->get_kid(0);
            return evaluate(stmt_node, env);
        }
        case AST_UNIT: {
            Value last_val(0);
            for (unsigned i = 0; i < node->get_num_kids(); ++i) {
                last_val = evaluate(node->get_kid(i), env);
            }
            return last_val;
        }
        case AST_FNCALL: {
            Node* func_varref_node = node->get_kid(0);
            std::string func_name = func_varref_node->get_str();
            Value func_val = env.get_variable(func_name);

            std::vector<Value> arg_values;
            if (node->get_num_kids() > 1) {
                Node* arg_list_node = node->get_kid(1);
                for (unsigned i = 0; i < arg_list_node->get_num_kids(); ++i) {
                    Value arg_val = evaluate(arg_list_node->get_kid(i), env);
                    arg_values.push_back(arg_val);
                }
            }

            if (func_val.is_intrinsic_fn()) {
                IntrinsicFn intrinsic_fn = func_val.get_intrinsic_fn();
                return intrinsic_fn(arg_values.data(), arg_values.size(), node->get_loc(), this);
            } else if (func_val.get_kind() == VALUE_FUNCTION) {
                Function* user_fn = func_val.get_function();
                const std::vector<std::string>& param_names = user_fn->get_params();

                if (arg_values.size() != param_names.size()) {
                    EvaluationError::raise(node->get_loc(), "Incorrect number of arguments for function '%s'.", func_name.c_str());
                }

                // Create function call environment with parent as the function's defining environment
                Environment fn_env(user_fn->get_parent_env());

                // Bind arguments to parameters in the function's environment
                for (size_t i = 0; i < arg_values.size(); ++i) {
                    fn_env.define_variable(param_names[i], arg_values[i]);
                }

                // Evaluate the function body in the new environment
                Value result = evaluate(user_fn->get_body(), fn_env);
                return result;
            } else {
                EvaluationError::raise(node->get_loc(), "'%s' is not a function.", func_name.c_str());
            }
        }
        case AST_IF: {
            Node* condition_node = node->get_kid(0);
            Node* true_branch_node = node->get_kid(1);
            Node* false_branch_node = node->get_num_kids() > 2 ? node->get_kid(2) : nullptr;

            Value condition_val = evaluate(condition_node, env);
            if (!condition_val.is_int()) {
                EvaluationError::raise(node->get_loc(), "Condition must evaluate to an integer");
            }

            if (condition_val.get_ival() != 0) {
                // True branch
                Environment true_env(&env);
                evaluate(true_branch_node, true_env);
            } else if (false_branch_node != nullptr) {
                // False branch
                Environment false_env(&env);
                evaluate(false_branch_node, false_env);
            }
            return Value(0); // Control flow statements evaluate to 0
        }
        case AST_WHILE: {
            Node* condition_node = node->get_kid(0);
            Node* body_node = node->get_kid(1);

            while (true) {
                Value condition_val = evaluate(condition_node, env);
                if (!condition_val.is_int()) {
                    EvaluationError::raise(node->get_loc(), "Condition must evaluate to an integer");
                }

                if (condition_val.get_ival() == 0) {
                    break;
                }

                // Loop body
                Environment body_env(&env);
                evaluate(body_node, body_env);
            }
            return Value(0); // Control flow statements evaluate to 0
        }
        case AST_STATEMENT_LIST: {
            Value last_val(0);
            Environment block_env(&env); 
            for (unsigned i = 0; i < node->get_num_kids(); ++i) {
                Node* stmt_node = node->get_kid(i);
                last_val = evaluate(stmt_node, block_env);
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
    return evaluate(m_ast, *m_env);
}

Value Interpreter::intrinsic_print(Value args[], unsigned num_args, const Location &loc, Interpreter *interp) {
    if (num_args != 1) {
        EvaluationError::raise(loc, "print expects exactly one argument");
    }
    printf("%s", args[0].as_str().c_str());
    return Value(0); // Return 0 as specified
}

Value Interpreter::intrinsic_println(Value args[], unsigned num_args, const Location &loc, Interpreter *interp) {
    if (num_args != 1) {
        EvaluationError::raise(loc, "println expects exactly one argument");
    }
    printf("%s\n", args[0].as_str().c_str());
    return Value(0); // Return 0 as specified
}
