#ifndef DRIVER_HH
# define DRIVER_HH
# include "parser.hpp"
/************************* Moduli specifici di IR ***************************/
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
using namespace llvm;
/**************** Stutture dati C++ usati dal compilatore *******************/
# include <string>
# include <map>
# include <variant>
# include <vector>
# include <iterator>
# include <set>
/******* Tipo di dato "valore lessicale" per numeri e identificatori ********/
typedef std::variant<std::string,int> lexval;
/********** Classe driver per gestire il processo di compilazione ***********/
class driver
{
public:
  driver();                     // Costruttore
  void scan_begin();            // Implementata nello scanner
  void scan_end();              // Implementata nello scanner
  int parse (const std::string& f); // Inizializza ed esegue il processo di parsing
  void codegen();               // Produce il codice intermedio visitando la Abstract
                                // Syntaax Forest (ASF)

  std::map<std::string, AllocaInst*> NamedValues;
                                // Tabella associativa per implementare i meccanismi di scope
  std::vector<DefAST*> root;    // Vettore cdi AST, uno per ogni definizione nel file sorgente
  yy::location location;        // Utilizzata dallo scanner per localizzare i token
  std::string file;             // File sorgente
  std::ostream* outputTarget;   // Stream di output per gli AST in Latex
  bool toLatex;                 // Abilita la  scrittura su file degli AST in Latex
  std::string opening, closing; // Parentesi per la visualizzazione degli AST.  Nel caso
                                // venga scritto il file latex sono rispettivamente 
                                // seguite e precedute da dal simbolo $
  //std::map<char,std::string> *operators;
  bool trace_parsing;           // Se true, abilita le tracce di debug nel parser
  bool trace_scanning;          // Se true, abilita le tracce di debug nello scanner
};

/***************************************************************************/
/****************** Gerarchia delle classi che definiscono *****************/
/************************ i costrutti del linguaggio ***********************/
/***************************************************************************/

/// RootAST - Classe base della gerarchia 
class RootAST {
public:
  virtual ~RootAST() {};
  virtual void visit() {};
  virtual Value *codegen(driver& drv) {return nullptr;};
};

/// DefAST - Classe base per tutti i nodi definizione
class DefAST : public RootAST {
public:
  virtual ~DefAST() {};
};

/// ExprAST - Classe base per tutti i nodi espressione
class ExprAST : public RootAST {
public:
  virtual ~ExprAST() {};
};

/// NumberExprAST - Classe per la rappresentazione di costanti numeriche
class NumberExprAST : public ExprAST {
private:
  int Val;

public:
  NumberExprAST(int Val);
  void visit() override;
  virtual lexval getLexVal() const;  // Getter, inserito per completezza
  Constant *codegen(driver& drv) override;
};

/// BoolConstAST - Classe per la rappresentazione di costanti booleane
class BoolConstAST : public ExprAST {
private:
  int boolVal;  // Val deve essere 0 o 1

public:
  BoolConstAST(int Val);
  void visit() override;
  lexval getLexVal() const;  // Getter, inserito per completezza
  Constant *codegen(driver& drv) override;
};

/// IdeExprAST - Classe per la rappresentazione di riferimenti a identificatori
class IdeExprAST : public ExprAST {
private:
  std::string Name;
  
public:
  IdeExprAST(std::string &Name);
  lexval getLexVal() const;
  void visit() override;
  Value *codegen(driver& drv) override;
};

/// BinaryExprAST - Classe per la rappresentazione di operatori binary
class BinaryExprAST : public ExprAST {
private:
  std::string Op;
  ExprAST* LHS;
  ExprAST* RHS;

public:
  BinaryExprAST(std::string Op, ExprAST* LHS, ExprAST* RHS);
  void visit() override;
  Value *codegen(driver& drv) override;
};

/// UnaryExprAST - Classe per la rappresentazione di operatori unari
class UnaryExprAST : public ExprAST {
private:
  std::string Op;
  ExprAST* RHS;

public:
  UnaryExprAST(std::string Op, ExprAST* RHS);
  void visit() override;
  Value *codegen(driver& drv) override;
};

/// CallExprAST - Classe per la rappresentazione di chiamate di funzione
class CallExprAST : public ExprAST {
private:
  std::string Callee;
  std::vector<ExprAST*> Args;  // ASTs per la valutazione degli argomenti

public:
  CallExprAST(std::string Callee, std::vector<ExprAST*> Args);
  lexval getLexVal() const;
  void visit() override;
  Value *codegen(driver& drv) override;
};

/// IfExprAST - Classe che rappresenta il costrutto "condizionale"
class IfExprAST : public ExprAST {
private:
  std::vector<std::pair<ExprAST*, ExprAST*>> IfThenSeq;
public:
  IfExprAST(std::vector<std::pair<ExprAST*, ExprAST*>> IfThenSeq);
  void visit() override;
  Value *codegen(driver& drv) override;
};

/// LetExprAST - Classe per la rappresentazione di espressioni con definizione
/// di un ambiente locale
class LetExprAST : public ExprAST {
private:
  std::vector<std::pair<std::string, ExprAST*>> Bindings; 
  ExprAST* Body;
public:
  LetExprAST(std::vector<std::pair<std::string, ExprAST*>> Bindings, ExprAST* Body);
  void visit() override;
  Value *codegen(driver& drv) override;
};

/// PrototypeAST - Classe per la rappresentazione dei prototipi di funzione
/// (nome, numero e nome dei parametri; in questo caso il tipi è implicito
/// perché unico)
class PrototypeAST : public DefAST {
private:
  std::string Name;
  bool External;
  std::vector<std::string> Params;

public:
  PrototypeAST(std::string Name, std::vector<std::string> Params);
  lexval getLexVal() const;
  void setext();
  const std::vector<std::string> &getParams() const; 
  void visit() override;
  int paramssize();
  Function *codegen(driver& drv) override;
};

/// FunctionAST - Classe che rappresenta la definizione di una funzione
class FunctionAST : public DefAST {
private:
  PrototypeAST* Proto;
  ExprAST* Body;
  bool external;
  
public:
  FunctionAST(PrototypeAST* Proto, ExprAST* Body);
  Function *codegen(driver& drv) override;
  void visit() override;
  int nparams();
};

/// AssignExprAST - Classe per la rappresentazione dell'operatore di assegnazione `:=`
class AssignExprAST : public ExprAST {
private:
  std::string Name;  // Identificatore
  ExprAST* ValueExpr;  // Espressione da assegnare

public:
  AssignExprAST(std::string Name, ExprAST* ValueExpr);
  void visit() override;
  Value *codegen(driver& drv) override;
};


#endif // ! DRIVER_HH
