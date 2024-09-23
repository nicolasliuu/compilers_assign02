I implemented a basic interpreter with a lexer, parser, and environment to
handle basic calculator features with arithmetic and logical expressions. The lexer includes additional
capability to handle multi-character tokens like ==, !=, <=, >=, &&, ||, and variable declarations.
 The parser constructs an abstract syntax tree (AST) using a recursive descent approach, 
 managing operator precedence and associativity directly in the grammar. The interpreter uses the AST 
 to evaluate expressions and statements, centralizing the handling of binary operations 
 via a helper function (get_operator_result) for clarity and consistency. 

 I added an Environment field directly in the Interpreter class to manage definitions and values within a global environment 
 so it didn't need to be passed between functions. The environment is currently only a map of variable names to values,
 and it currently only supports one global environment. I also added new AST tags for the new node implementations.