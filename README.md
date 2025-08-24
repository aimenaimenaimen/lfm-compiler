LFM – Minimal Functional Language Compiler
Overview
LFM (Minimal Functional Language) is a toy compiler for a small functional programming language, developed using Flex, Bison, and C++ with LLVM as backend.
It demonstrates the main stages of compiler design: lexical analysis, parsing, abstract syntax tree (AST) construction, semantic checks, and code generation in LLVM-IR.
The project was created with educational purposes under academic supervision.
Features
Lexer implemented with Flex.
Parser implemented with Bison.
Abstract Syntax Tree (AST) built in C++.
LLVM-based code generation (x86_64 target).
Functional programming constructs:
Integer and boolean constants
Arithmetic and logical operations
Unary and binary operators
Function definitions and calls
let bindings
Conditional expressions (if ... else)
Variable assignment
External function calls for I/O (readval, printval).
