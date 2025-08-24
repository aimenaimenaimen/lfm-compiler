#include <iostream>
#include "driver.hpp"
#include <fstream>
#include <string>

extern LLVMContext *context;
extern Module *module;
extern IRBuilder<> *builder;

driver drv;

void help() {
  std::cout << "USO\n\t./lfmc [opzioni] <filename>\n";
  std::cout << "OPZIONI\n\t-v Stampa su stdout una rappresentazione lineare degli AST\n";
  std::cout << "\t-l Produce un file <filename>.tex che può essere compilato con pdflatex\n";
  std::cout << "\t   per \"disegnare\" gli AST\n";
  std::cout << "\t-c Produce su stderr il codice intermedio (LLVM-IR) corrispondente al programma sorgente\n";
  std::cout << "\t-p Abilita il tracing del processo di parsing\n";
  std::cout << "\t-s Abilita il tracing del processo di scanning/lexing\n";
  std::cout << "\t-h Stampa questo messaggio di help\n";
}

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool latex = false;
  bool gencode = false;
  static std::ofstream outfile;
  std::string latex_preamble = R"PREAMBLE(
\documentclass[12pt]{article}
\usepackage{synttree}
\begin{document}
)PREAMBLE";
  if (argc==1) {
    help();
    return 0;
  }
  for (int i = 1; i < argc; ++i)
    if (argv[i] == std::string ("-h")) {
      help();
      return 0;
    }
    else if (argv[i] == std::string ("-p"))
      drv.trace_parsing = true;
    else if (argv[i] == std::string ("-s"))
      drv.trace_scanning = true;
    else if  (argv[i] == std::string("-v"))
      verbose = true;
    else if (argv[i] == std::string("-l"))
      latex = true;
    else if (argv[i] == std::string("-c"))
      gencode = true;
    else if (!drv.parse (argv[i])) {
      std::cout << "Parse successful\n";
      if (latex or verbose) {
      	if (latex) {
          drv.toLatex = true;
          drv.opening = "[$";
          drv.closing = "$]";
          outfile.open(argv[i]+std::string(".tex"));
          drv.outputTarget = &outfile;
          *drv.outputTarget << latex_preamble << std::endl;
          *drv.outputTarget << "{\\bf\\LARGE Foresta AST di " << argv[i] << "}\n";
          *drv.outputTarget << "\\vspace{1cm}\n\n";
        } else { 
          drv.outputTarget = &std::cout;
        }
        for (DefAST* tree: drv.root) {
          if (latex) *drv.outputTarget << "\\synttree";
          tree->visit();
          if (latex) *drv.outputTarget << "\n\\par\\vspace{1cm}\n";
          else std::cout << std::endl;
        }
        if (latex) *drv.outputTarget << "\\end{document}";
        outfile.close();
      }
      if (gencode) {
      	drv.codegen();
      }
    } else return 1;
  return 0;
}
