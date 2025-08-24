#include <fstream>
#include "driver.hpp"
#include "parser.hpp"

// Generazione di un'istanza per ciascuna delle classi LLVMContext,
// Module e IRBuilder. Nel caso di singolo modulo è sufficiente
LLVMContext *context = new LLVMContext;
Module *module = new Module("LFMCompiler", *context);
IRBuilder<> *builder = new IRBuilder(*context);

/************************ Funzioni di utilità ***************************/
Value *LogErrorV(const std::string Str) {
  std::cerr << Str << std::endl;
  return nullptr;
}
static AllocaInst *MakeAlloca(Function *fun, StringRef ide,
			      Type* T = IntegerType::get(*context,32)) {
/* Questa funzione C++ sulle prime non è semplice da comprendere.
   Essa definisce una utility con due parametri:
   1) la rappresentazione di una funzione LLVM IR, e
   2) il nome per un registro SSA
   La chiamata di questa utility restituisce un'istruzione IR che alloca un i32 in
   memoria (stack) e ne memorizza il puntatore in un registro SSA cui viene attribuito
   il nome passato come secondo parametro. L'istruzione verrà scritta all'inizio
   dell'entry block della funzione passata come primo parametro.
   Si ricordi che le istruzioni sono generate da un builder. Per non
   interferire con il builder globale, la generazione viene dunque effettuata
   con un builder temporaneo TmpB
*/
  IRBuilder<> TmpB(&fun->getEntryBlock(), fun->getEntryBlock().begin());
  return TmpB.CreateAlloca(T, nullptr, ide);
}

/************ Implementazione dei metodi della classe driver ************/
driver::driver(): trace_parsing(false), trace_scanning(false), 
                  toLatex(false), opening("["), closing("]") {};

int driver::parse (const std::string &f)
{
  file = f;
  location.initialize(&file);
  scan_begin();
  yy::parser parser(*this);
  parser.set_debug_level(trace_parsing);
  int res = parser.parse();
  scan_end();
  return res;
}

void driver::codegen() {
  // Il metodo codegen effettua una "semplice" chiamata al metodo 
  // omonimo presente nel nodo root generato dal parser.
  fprintf(stderr, "target triple = \"x86_64-pc-linux-gnu\"\n\n");
  for (DefAST* tree: root) {
    tree->codegen(*this);
  }
};

extern driver drv;

/************* Implementazione dei metodi della classi AST *************/

/// NumberExprAST
NumberExprAST::NumberExprAST(int Val): Val(Val) {};
void NumberExprAST::visit() {
  *drv.outputTarget << "[" << Val << "]";
};
lexval NumberExprAST::getLexVal() const {
  lexval lval = Val;
  return lval;
};
Constant *NumberExprAST::codegen(driver& drv) {  
  return ConstantInt::get(*context, APInt(32,Val));
};

/// BoolConstAST
/* Differisce da NumberExprAST perché limitata alle costanti 0 e 1
   la cui rappresentazione è un int1 */
BoolConstAST::BoolConstAST(int Val): boolVal(Val) {
  if (Val != 0) boolVal = 1;
};
void BoolConstAST::visit() {
  *drv.outputTarget << "[" << boolVal << "]";
};
lexval BoolConstAST::getLexVal() const {
  lexval lval = boolVal;
  return lval;
};
Constant *BoolConstAST::codegen(driver& drv) {  
  return ConstantInt::get(*context, APInt(1,boolVal));
};

/// IdeExprAST
IdeExprAST::IdeExprAST(std::string &Name): Name(Name) {};
lexval IdeExprAST::getLexVal() const {
  lexval lval = Name;
  return lval;
};
void IdeExprAST::visit() {
  *drv.outputTarget << drv.opening << Name << drv.closing;
};
Value *IdeExprAST::codegen(driver& drv) {
  AllocaInst *L = drv.NamedValues[Name];
  if (L) {  // Controllo semantico
    Value *V = builder->CreateLoad(Type::getInt32Ty(*context), 
                                   L, Name); 
    return V;
  };
  return LogErrorV("Variabile "+Name+" non definita"); 
};

/// BinaryExprAST
BinaryExprAST::BinaryExprAST(std::string Op, ExprAST* LHS, ExprAST* RHS): 
                             Op(Op), LHS(LHS), RHS(RHS) {};
void BinaryExprAST::visit() {
  *drv.outputTarget << drv.opening << Op;
  if (drv.toLatex)
     *drv.outputTarget << "$ ";
  else
     *drv.outputTarget << " ";
  LHS->visit();
  RHS->visit();
  *drv.outputTarget << "]";
};
Value *BinaryExprAST::codegen(driver& drv) {
  /* E' forse il codice più semplice da comprendere.
     Ricorsivamente viene generato il codice per il LHS e il RHS
     dell'operatore e quindi viene generato il codice per
     l'operazione specificata.
  */
  Value *L = LHS->codegen(drv);
  Value *R = RHS->codegen(drv);
  if (!L || !R) {
     return nullptr;  
  }
  if (Op=="+") return builder->CreateNSWAdd(L,R,"sum");
  else if (Op=="-") return builder->CreateNSWSub(L,R,"diff");
  else if (Op=="*") return builder->CreateNSWMul(L,R,"prod");
  else if (Op=="/") return builder->CreateSDiv(L,R,"quot");
  else if (Op=="%") return builder->CreateSRem(L,R,"rem");
  else if (Op=="<") return builder->CreateICmpSLT(L,R,"cmplt");
  else if (Op=="<=") return builder->CreateICmpSLE(L,R,"cmple");
  else if (Op==">") return builder->CreateICmpSGT(L,R,"cmpgt");
  else if (Op==">=") return builder->CreateICmpSGE(L,R,"cmpge");
  else if (Op=="==") return builder->CreateICmpEQ(L,R,"cmpeq");
  else if (Op=="<>") return builder->CreateICmpNE(L,R,"cmpne");
  else if (Op=="and") return builder->CreateAnd(L,R,"and");
  else if (Op=="or") return builder->CreateOr(L,R,"or");
  else {
    return LogErrorV("Operatore binario "+Op+" non supportato");
  }
};

/// UnaryExprAST
UnaryExprAST::UnaryExprAST(std::string Op, ExprAST* RHS): Op(Op), RHS(RHS) {};
void UnaryExprAST::visit() {
  *drv.outputTarget << drv.opening << Op;
  if (drv.toLatex)
     *drv.outputTarget << "$ ";
  else
     *drv.outputTarget << " ";
  RHS->visit();
  *drv.outputTarget << "]";
};
Value *UnaryExprAST::codegen(driver& drv) {
  /* Il solo operando è memorizzato in RHS. Il meno unario
     viene "tradotto" come sottrazione in cui il minuendoè 0.
     Il not logico viene tradotto come XOR con la costante logica 1 */
  if (Op=="-") {
     Value *L = ConstantInt::get(*context, APInt(32,0));
     Value *R = RHS->codegen(drv);
     return builder->CreateNSWSub(L,R,"diff");
  } else if (Op=="not") {
     Value *L = ConstantInt::get(*context, APInt(1,1));
     Value *R = RHS->codegen(drv);
     return builder->CreateXor(L,R,"not");
  } else {

     return LogErrorV("Operatore unario "+Op+" non supportato");
  }
};

/// CallExprAST
CallExprAST::CallExprAST(std::string Callee, std::vector<ExprAST*> Args): Callee(Callee), 
                         Args(std::move(Args)) {};
lexval CallExprAST::getLexVal() const {
  lexval lval = Callee;
  return lval;
};
void CallExprAST::visit() {
  *drv.outputTarget << drv.opening << Callee;
    if (drv.toLatex)
     *drv.outputTarget << "$ ";
  else
     *drv.outputTarget << " ";
  for (ExprAST* arg : Args) {
    arg->visit();
  };
  *drv.outputTarget << "]";
};
Value *CallExprAST::codegen(driver& drv) {
  // La generazione del codice corrispondente ad una chiamata di funzione
  // inizia cercando nel modulo corrente (l'unico, nel nostro caso) una funzione
  // il cui nome coincide con il nome memorizzato nel nodo dell'AST
  // Se la funzione non viene trovata (e dunque non è stata precedentemente definita)
  // viene generato un errore. Controllo semantico!
  Function *CalleeF = module->getFunction(Callee);
  if (!CalleeF)
     return LogErrorV("Funzione non definita");
  // Il secondo controllo semantico è che la funzione recuperata abbia 
  // tanti parametri quanti sono gi argomenti previsti nel nodo AST
  if (CalleeF->arg_size() != Args.size())
     return LogErrorV("Numero di argomenti non corretto");
  // Passato con successo anche il secondo controllo, viene predisposta
  // ricorsivamente la valutazione degli argomenti presenti nella chiamata 
  // (si ricordi che gli argomenti possono essere espressioni arbitarie)
  // I risultati delle valutazioni degli argomenti (registri SSA, come sempre)
  // vengono inseriti in un vettore, dove "se li aspetta" il metodo CreateCall
  // del builder, che viene chiamato subito dopo per la generazione dell'istruzione
  // IR di chiamata
  std::vector<Value*> ArgsV;
  for (auto arg : Args) {
     ArgsV.push_back(arg->codegen(drv));
  }
  return builder->CreateCall(CalleeF, ArgsV, "callfun");
};

/// IfExprAST
IfExprAST::IfExprAST(std::vector<std::pair<ExprAST*, ExprAST*>> IfThenSeq):
  IfThenSeq(std::move(IfThenSeq)) {};
void IfExprAST::visit() {
  *drv.outputTarget << "[if ";
  for (unsigned i=0, e=IfThenSeq.size(); i<e; i++) {
     *drv.outputTarget << "[alt ";
     IfThenSeq[i].first->visit();
     IfThenSeq[i].second->visit();
     *drv.outputTarget << "]";
  }
  *drv.outputTarget << "]";
};
Value *IfExprAST::codegen(driver& drv) {
   /* E' il codice piu' complesso, anche a causa della scelta progettuale
      di dotare l'istruzione di un numero arbitrario di alternative.
      Chiaramente, quindi, una parte consistente della generazione del codice
      dovrà essere eseguita all'interno di un ciclo.
      Prima di entrare nel ciclo generiamo, nel "blocco corrente", il codice 
      per il primo test e creiamo il blocco con la corrispondente espressione.
      Terminato il ciclo, dovremo generare ancora l'alternativa default (eseguita
      solo se il programmatore specifica alternative non esaustive). All'interno
      del ciclo dobbiamo quindi creare (n-1) coppie (se n è il numero di alternative)
      test/espressione: T2/E2,T3/E3,...,Tn/En.
      La scrittura dei blocchi (piuttosto che la creazione) segue un ordine 
      leggermente diverso. Si noti infatti che, per completare la scrittura del 
      blocco di test Ti serve aver "creato" sia il blocco Ei sia sia il blocco
      Ti+1 (che sono le due possibili destinazioni del salto condizionato che
      chiude il blocco Ti. Se però la coppia Ti+1/Ei+1 viene generata nell'iterazione
      (i+1)-esima, è evidente che alla fine dell'iterazione i-esima tale salto
      non può ancora essere generato e dunque il builder è ancora posizionato sul
      blocco Ti e (dunque, ancora) la scrittura del blocco Ei non è neppure
      iniziata.
      Ne consegue che nel ciclo (i+1)-esimo:
      1) viene completata la scrittura del blocco Ti. 
      2) viene scritto tutto il blocco Ei
      3) viene scritto tutto il blocco Ti+1 tranne branch finale
   */
   int numpairs = IfThenSeq.size();
   Value* CondV = IfThenSeq.at(0).first->codegen(drv); //Primo test
   if (!CondV)
      return nullptr;
   Function *function = 
          builder->GetInsertBlock()->getParent();
   BasicBlock *ExprBB = 
	         BasicBlock::Create(*context,"expr1");
   BasicBlock *ExitBB = 
	         BasicBlock::Create(*context,"exitblock");
   BasicBlock *CondBB;  // Definito (ma non ancora creato) qui 
                        // solo per ragioni di visibilità (scope)	      
   std::vector<std::pair<Value*,BasicBlock*>> phipairs;
   for (int j=1; j<numpairs; j++) {
      CondBB = BasicBlock::Create(*context,
	                     "test"+std::to_string(j+1));
      builder->CreateCondBr(CondV, ExprBB, CondBB);
      function->insert(function->end(), ExprBB);
      builder->SetInsertPoint(ExprBB);
      Value *ExprV = IfThenSeq.at(j-1).second->codegen(drv);
      if (!ExprV)
         return nullptr;
      builder->CreateBr(ExitBB);
      ExprBB = builder->GetInsertBlock(); //Perché potrebbe essere
                                          //cambiato
      phipairs.insert(phipairs.end(),{ExprV,ExprBB});
      function->insert(function->end(), CondBB);
      builder->SetInsertPoint(CondBB);
      CondV = IfThenSeq.at(j).first->codegen(drv);
      if (!CondV)
         return nullptr;
      ExprBB = BasicBlock::Create(*context,
                             "expr"+std::to_string(j+1));
   }
   builder->CreateCondBr(CondV, ExprBB, ExitBB);
   function->insert(function->end(), ExprBB);
   builder->SetInsertPoint(ExprBB);
   Value *ExprV = IfThenSeq.at(numpairs-1).second->codegen(drv);
      if (!ExprV)
         return nullptr;
   builder->CreateBr(ExitBB);
   ExprBB = builder->GetInsertBlock(); 
   phipairs.insert(phipairs.end(),{ExprV,ExprBB});   
   function->insert(function->end(),ExitBB);
   builder->SetInsertPoint(ExitBB);
   PHINode *PN = builder->CreatePHI(Type::getInt32Ty(*context),
                            numpairs, "condval");
   for (int j=0; j<numpairs; j++) {
      Value* val = phipairs.at(j).first;
      BasicBlock* incoming = phipairs.at(j).second;
      PN->addIncoming(val,incoming);
   }
   PN->addIncoming(ConstantInt::get(*context,
                                    APInt(32,0)),CondBB);	
   return PN;
};
 
/// LetExprAST
LetExprAST::LetExprAST(std::vector<std::pair<std::string, ExprAST*>> Bindings, ExprAST* Body):
  Bindings(std::move(Bindings)), Body(Body) {};
void LetExprAST::visit() {
  *drv.outputTarget << "[let [bindings ";
  for (unsigned i=0, e=Bindings.size(); i<e; i++) {
    *drv.outputTarget << "[= " << drv.opening << Bindings[i].first << drv.closing;
    Bindings[i].second->visit();
    *drv.outputTarget << "]";
  };
  *drv.outputTarget << "][in ";
  Body->visit();
  *drv.outputTarget << "]]";
}; 
Value *LetExprAST::codegen(driver& drv) {
   // Recuperiamo dapprima il puntatore alla funzione in cui stiamo generando codice
   Function *function = builder->GetInsertBlock()->getParent();
   // Ora creiamo un symbol table temporanea per memorizzare gli eventuali
   // identificatori che vengono ``oscurati'' (overriden) da definizioni
   // locali. La visibilità dovrà essere ripristinata all'uscita del blocco let
   std::map<std::string,AllocaInst*> AllocaTmp;   
   std::map<std::string,AllocaInst*>::iterator it;
   // Il ciclo dipende chiaramente dal numero di identificatori
   // locali preenti nel codice sorgente lfm
   std::set<std::string> locals{};
   for (int j=0, e=Bindings.size(); j<e; j++) {
     // Recuperiamo il nome simbolico e generiamo il codice
     // per l'espressione associata (bound)
       std::string ide = Bindings.at(j).first;
       // Controlliamo che ide non sia già stato usato localmente
       if (locals.count(ide)>0) { // Solo da C++ 20 esiste il predicato contains
	  LogErrorV("Assegnamento all'identificatore locale " + ide + " non consentito");
	  return nullptr;
       }
       locals.insert(ide);
       Value *boundval = Bindings.at(j).second->codegen(drv);
       if (!boundval) 
	  return nullptr;
       // Come per gli argomenti della funzione, scriviamo
       // un'istruzione che salva il valore in memoria
       AllocaInst *BInst = MakeAlloca(function, ide);
       builder->CreateStore(boundval, BInst);
       // Ora controlliamo che l'identificatore non sia
       // già definito (come parametro formale dell'istruzione
       // o come variabile di un altro blocco let
       // In tal caso, salviamo l'istruzione di alocazione
       // nella symbol table temporanea
       it = drv.NamedValues.find(ide);
       if(it != drv.NamedValues.end()) {
          AllocaTmp[ide] = drv.NamedValues[ide];
       }
       // In ogni caso, insieriamo la nuova istruzione di
       // allocazione  nella symbol table
       drv.NamedValues[ide] = BInst;
   }
   // Ora viene la parte più ``semplice'', ovvero
   // la generazione del codice dell'espressione che
   // forma il body del costrutto let
   Value *letVal = Body->codegen(drv);
   // Infine, ripristiniamo i valori dalla symbol table temporanea
   for (int j=0, e=Bindings.size(); j<e; j++) {
       std::string ide = Bindings.at(j).first;
       it = AllocaTmp.find(ide);
       if (it != AllocaTmp.end()) {
          drv.NamedValues[ide] = AllocaTmp[ide];
       } else {
          it = drv.NamedValues.find(ide);
          drv.NamedValues.erase(it);
       }
   }
   return letVal;
};

/// PrototypeAST
PrototypeAST::PrototypeAST(std::string Name, std::vector<std::string> Params):
                           Name(Name), Params(std::move(Params)) {External=false;};
lexval PrototypeAST::getLexVal() const {
  lexval lval = Name;
  return lval;
};
void PrototypeAST::setext() { External = true; };
const std::vector<std::string>& PrototypeAST::getParams() const {
  return Params;
};
void PrototypeAST::visit() {
  if (External) *drv.outputTarget << "[extern ";
  *drv.outputTarget << drv.opening << Name << drv.closing << "[params ";
  for (auto it=Params.begin(); it!=Params.end(); ++it) {
    *drv.outputTarget << drv.opening << *it << drv.closing;
  };
  *drv.outputTarget << "]";
  if (External) *drv.outputTarget << "]";
};
int PrototypeAST::paramssize() {
  return Params.size();
};
Function *PrototypeAST::codegen(driver& drv) {
   /* Genera il codice per il prototipo di una funzione.
      Come primo passo deve creare un'opportuna struttura
      che definisce il "tipo completo" della funzione,
      ovvero il tipo di ritorno e il tipo dei parametri.
      Nel nostro caso l'unico tipo di base sono gli interi
  */
  std::vector<Type*> intarray(Params.size(), IntegerType::get(*context,32));
  FunctionType *FT = FunctionType::get(IntegerType::get(*context,32), 
                                       intarray, false);
  // Ora possiamo defnire la funzione
  Function *F = Function::Create(FT, Function::ExternalLinkage, Name, *module);
  
  // Ad ogni parametro della funzione F (che, è bene ricordare, è la rappresentazione 
  // llvm di una funzione, non è una funzione C++) attribuiamo ora il nome specificato 
  // dal programmatore e presente nel nodo AST relativo al prototipo
  unsigned Idx = 0;
  for (auto &Arg : F->args())
    Arg.setName(Params[Idx++]);
    
  // Il codice viene emesso solo se si tratta del prototipo di una funzione
  // esterna. Se invece il prototipo fa parte della definizione di una funzione
  // interna (prototipo+body) allora l'emissione viene fatta dallla codegen
  // della funzione
  if (External) {
    F->print(errs());
    fprintf(stderr, "\n");
  };
  return F;
}

/// FunctionAST
FunctionAST::FunctionAST(PrototypeAST* Proto, ExprAST* Body): 
                                Proto(Proto), Body(Body) {};
void FunctionAST::visit() {
  *drv.outputTarget << "[function ";
  Proto->visit();
  Body->visit();
  *drv.outputTarget << "]";
};
int FunctionAST::nparams() {
  return Proto->paramssize();
};
Function *FunctionAST::codegen(driver& drv) {
  // Verifica che la funzione non sia già presente nel modulo e che dunque che 
  // non stiamo "tentando" una ridefinizione
  
  std::string FunName = std::get<std::string>(Proto->getLexVal());
  Function *function = module->getFunction(FunName);
  if (function) {
     // Se la funzione è già definita si dà un messaggio di errore e si esce
     LogErrorV("Funzione " + FunName + " già definita");
     return nullptr;
  }
  // La funzione non è definita.
  // Generiamo dapprima il codice del prototipo, che verrà emesso dopo
  // aver generato anche.
  function = Proto->codegen(drv);
  
  // Se per qualche ragione la generazione del prototipo fallisce
  // diamo un messaggio di errore e "rimbalziamo" il fallimento al chiamante
  if (!function) {
     LogErrorV("Definizione della funzione " + FunName + " fallita");
     return nullptr;
  }
  
  // La funzione è stata definita e, al momento, "contiene" solo il codice
  // per generare il suo prototipo. E' quindi il momento di generare 
  // il resto della funzione
  // Come prima cosa definiamo un Basic Block in cui inserire il codice
  BasicBlock *BB = BasicBlock::Create(*context, "entry", function);
  builder->SetInsertPoint(BB);
 
  // Come seconda cosa dobbiamo occuparci dei parametri formali ai quali sarà
  // poi fatto riferimento nel body (pena, altrimenti, di essere inutili).
  // I parametri vengono inseriti in una symbol table. La chiave di accesso
  // sarà il l'dentificatore usato dal programmatore. 
  // Il valore sarà invece l'area di memoria dove verrà memorizzato l'argomento
  // al momento della chiamata. 
  for (auto &Arg : function->args()) {
      // Per pgni paramentro formale allochiamo uno spazio sullo stack
      AllocaInst *Alloca = MakeAlloca(function, Arg.getName());
      // ... e creiamo un'istruzione che memorizza in un registro il 
      // puntatore all'area allocata
      builder->CreateStore(&Arg, Alloca);
      // ... e registriamo lo stesso indirizzo nella symbol table
      drv.NamedValues[std::string(Arg.getName())] = Alloca;
  } 
  
  // Ora può finalente essere generato il codice corrispondente al body (che potrà
  // fare riferimento alla symbol table)
  if (Value *RetVal = Body->codegen(drv)) {
    // Se la generazione termina senza errori, ciò che rimane da fare è
    // di generare l'istruzione return, che ("a tempo di esecuzione") prenderà
    // il valore lasciato nel registro RetVal 
    builder->CreateRet(RetVal);
    // Effettuiamo anche una validazione del codice e un controllo di consistenza
    verifyFunction(*function);
 
    // ... ed infine emettiamo il codice della funzione su stderr
    function->print(errs());
    fprintf(stderr, "\n");
    return function;
  }

  // Se invece il body ha provocato errore, cancelliamo dal modulo 
  // la definizione della funzione.
  function->eraseFromParent();
  return nullptr;
};

/// AssignExprAST
AssignExprAST::AssignExprAST(std::string Name, ExprAST *ValueExpr)
    : Name(Name), ValueExpr(ValueExpr) {}

void AssignExprAST::visit() {
  *drv.outputTarget << "[assegna " << drv.opening << Name << drv.closing;
  ValueExpr->visit();
  *drv.outputTarget << "]";
}

Value *AssignExprAST::codegen(driver& drv) {
  // Genera il codice per l'espressione da assegnare
  Value *Val = ValueExpr->codegen(drv);
  if (!Val) return ConstantInt::get(*context, APInt(1, 0)); // Restituisce false se Val è nullo

  // Recupera la funzione corrente
  Function *function = builder->GetInsertBlock()->getParent();

  // Alloca memoria per l'identificatore se non già presente
  AllocaInst *Alloca = drv.NamedValues[Name];
  if (!Alloca) {
    Alloca = MakeAlloca(function, Name);
    drv.NamedValues[Name] = Alloca;
  }

  // Memorizza il valore nella memoria allocata
  builder->CreateStore(Val, Alloca);

  // Casta l'intero a booleano (1 bit)
  Value *BoolVal = builder->CreateICmpNE(Val, ConstantInt::get(*context, APInt(32, 0)), "boolcast");

  // Restituisce il valore booleano
  return BoolVal;
}
