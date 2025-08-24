%{ /* -*- C++ -*- */
# include <cerrno>
# include <climits>
# include <cstdlib>
# include <string>
# include <cmath>
# include "parser.hpp"
# include "driver.hpp"
%}

%option noyywrap nounput batch debug noinput

id      [a-zA-Z_][a-zA-Z_0-9]*
num     0|[1-9][0-9]*
blank   [ \t]

%{
  // Codice eseguito ad ogni match con una regexp
  # define YY_USER_ACTION loc.columns(yyleng);
%}
%%
%{
  // La location è memorizzata nel driver ma è utile
  // potervi fare riferimento in modo più succinto
  // con una variabile locale
  yy::location& loc = drv.location;
  
  // Codice eseguito ogni volta che yylex viene chiamata
  loc.step ();
%}
{blank}+   loc.step ();
[\n]+      loc.lines (yyleng); loc.step ();

":"      return yy::parser::make_COLON     (loc);
";"      return yy::parser::make_SEMICOLON (loc);
","      return yy::parser::make_COMMA     (loc);
"-"      return yy::parser::make_MINUS     (loc);
"+"      return yy::parser::make_PLUS      (loc);
"*"      return yy::parser::make_STAR      (loc);
"/"      return yy::parser::make_SLASH     (loc);
"%"      return yy::parser::make_MOD       (loc);
"("      return yy::parser::make_LPAREN    (loc);
")"      return yy::parser::make_RPAREN    (loc);
"|"      return yy::parser::make_ALT       (loc);
"<="     return yy::parser::make_LE        (loc);
"<>"     return yy::parser::make_NEQ       (loc);
"<"      return yy::parser::make_LT        (loc);
"=="     return yy::parser::make_EQ        (loc);
">="     return yy::parser::make_GE        (loc);
">"      return yy::parser::make_GT        (loc);
"="      return yy::parser::make_BIND      (loc);
"true"   return yy::parser::make_TRUE      (loc);
"false"  return yy::parser::make_FALSE     (loc);
"external" return yy::parser::make_EXTERN  (loc);
"function" return yy::parser::make_DEF     (loc);
"if"     return yy::parser::make_IF        (loc);
"and"    return yy::parser::make_AND       (loc);
"or"     return yy::parser::make_OR        (loc);
"not"    return yy::parser::make_NOT       (loc);
"let"    return yy::parser::make_LET       (loc);
"in"     return yy::parser::make_IN        (loc);
"end"    return yy::parser::make_END       (loc);
":="     return yy::parser::make_ASSIGN    (loc);
{num}    { errno = 0;
           int n;
           try {
             n = std::stoi(yytext, NULL, 10);
           }  
           catch(std::out_of_range& e){ 
             std::cerr << "Out of Range error: " << e.what() << '\n';
             throw yy::parser::syntax_error(loc, "Integer number too big: "
                                            + std::string(yytext));
           };
           return yy::parser::make_NUMBER(n, loc);
         }

{id}     { return yy::parser::make_IDENTIFIER (yytext, loc); }

.        { throw yy::parser::syntax_error
               (loc, "invalid character: " + std::string(yytext));
         }
         
<<EOF>>  { return yy::parser::make_EOF (loc); }
%%

void driver::scan_begin () {
  yy_flex_debug = trace_scanning;
  if (file.empty () || file == "-")
    yyin = stdin;
  else if (!(yyin = fopen (file.c_str (), "r")))
    {
      std::cerr << "cannot open " << file << ": " << strerror(errno) << '\n';
      exit (EXIT_FAILURE);
    }
}

void
driver::scan_end ()
{
  fclose (yyin);
}
