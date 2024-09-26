Assignment 2 Milestone 1: 

Extended the interpreter by adding support for functions, control flow constructs, and nested environments. 
To achieve this, I enhanced the parser to handle new grammar rules for functions, if, else, and while statements. 
I introduced top-level function definitions and argument lists while ensuring that the Abstract Syntax Tree (AST) captures 
these structures efficiently. To manage scoping, I updated the Environment class to track variables in nested blocks
 and added methods to check variable definitions in both the current and parent scopes. 
 For function calls, I implemented a mechanism where each call creates a new environment, 
 binding arguments to parameters and evaluating the function body within this scope. 
 
I implemented short-circuiting logical operators (&&, ||), ensuring that unnecessary expressions are not evaluated.

Memory management was refined by updating the Value class to support dynamic memory for functions using reference counting. 
These changes allow the interpreter to handle more complex code with proper scope management
 and efficient execution of control flows and functions.