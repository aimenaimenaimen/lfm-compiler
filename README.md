# LFM – Minimal Functional Language Compiler

## Overview
**LFM (Minimal Functional Language)** is a compiler for a small functional programming language, developed using **Flex**, **Bison**, and **C++** with **LLVM** as backend.  
It demonstrates the main stages of compiler construction: lexical analysis, parsing, abstract syntax tree (AST) construction, semantic checks, and code generation in LLVM-IR.  
The project was developed with educational purposes under academic supervision.

## Features
- Lexer implemented with **Flex**  
- Parser implemented with **Bison**  
- Abstract Syntax Tree (AST) in **C++**  
- LLVM-based **code generation** (x86_64 target)  
- Core language constructs:
  - Integer and boolean constants  
  - Arithmetic and logical operations  
  - Unary and binary operators  
  - Function definitions and calls  
  - `let` bindings  
  - Conditional expressions (`if ... else`)  
  - Variable assignment  
- External I/O functions (`readval`, `printval`)  

## Project Structure
- **lfmc.cpp** – Main entry point, command-line options, and driver invocation  
- **driver.cpp / driver.hpp** – Compiler driver, AST classes, and code generation with LLVM  
- **parser.yy** – Grammar specification (Bison)  
- **scanner.ll** – Lexical analyzer (Flex)  
- **IO.cpp** – External I/O functions for interaction with the compiled programs  

## Build Instructions
Requirements:
- **C++17 or later**  
- **LLVM development libraries**  
- **Flex** and **Bison**  


