%{
#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>
#include <fstream>

#include "SymTable.h"
#include "AST.h"

using namespace std;

extern int yylex();
extern int yylineno;
extern char* yytext;
extern FILE* yyin;

void yyerror(const char * s);

// scope-uri
SymTable* root = nullptr;
SymTable* activeScope = nullptr;

//toate tabelele, sa le putem dupa printa
vector<SymTable*> allTables;

// tipurile + numele parametrilor
vector<DataType> currentParamTypes;
vector<std::string> currentParamNames; 

// AST
ASTNode* mainBlock = nullptr;
ASTNode* globalInit = nullptr;

extern DataType stringToType(const std::string&);

%}

%code requires {
  #include <string>
  #include <iostream>
  #include "AST.h"
  using namespace std;
}

%union {
     string* Str;
     int val;
     float floatVal;
     ASTNode* node;
}

%token <Str> ID
%token <Str> STRING_VAL
%token <val> INT_VAL
%token <val> BOOL_VAL
%token <floatVal> FLOAT_VAL

%type <node> expr valoare
%type <node> stmt lista_stmts block control atribuire apel_func
%type <node> args decl_locala
%type <node> apel_args 
%type <node> bloc_cod lista_instr instr_unica
%type <Str> tip_date

%token INT FLOAT STRING BOOL CLASS MAIN PRINT IF ELSE WHILE FOR RETURN VOID
%token AND OR NOT EQ NEQ LE GE

%left OR
%left AND
%left EQ NEQ
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%right NOT

%start program
%%

program : bloc_global main
        ;

bloc_global : bloc_global element
            | element
            ;

element : def_clasa
        | def_functie
        | decl_globala
        ;

//clase
def_clasa : CLASS ID
{
    if (activeScope->existsLocal($2)) {
        cerr << "Eroare linia " << yylineno  << ": clasa '" << *$2 << "' redeclarata\n";
    } else {
        activeScope->addVar(new string("class"), $2);
    }
    activeScope = new SymTable("class_" + *$2, activeScope);
    allTables.push_back(activeScope);
}
'{' lista_membri '}'
{
    activeScope = activeScope->getParent();
}
;

lista_membri : lista_membri membru
             | /*epsilon*/
             ;

membru
    : tip_date ID ';'
      {
          if (activeScope->existsLocal($2)) {
              cerr << "Eroare linia " << yylineno << ": membrul de clasa '" << *$2  << "' este redeclarat\n";
          } else {
              activeScope->addVar($1, $2);
          }
      }
    | def_functie
    ;

//functii
def_functie : tip_date ID
      
    {
    currentParamTypes.clear();
    currentParamNames.clear();   
    activeScope = new SymTable("func_" + *$2, activeScope);
    allTables.push_back(activeScope);
    }
      
    '(' lista_param ')'  '{' bloc_cod '}'
    {
        //functie+signatura
        activeScope->getParent()->addFunction($2, stringToType(*$1), currentParamTypes, currentParamNames, $8);

        activeScope = activeScope->getParent();
    }
;


//declaratii globale 
decl_globala : tip_date ID ';'
{
    if (activeScope->existsLocal($2)) {
        cerr << "Eroare linia " << yylineno   << ": variabila globala '" << *$2   << "' redeclarata\n";
    } else {
        activeScope->addVar($1, $2);
    }
}
| tip_date ID '=' expr ';'
{
    if (activeScope->existsLocal($2)) {
        cerr << "Eroare linia " << yylineno << ": variabila globala '" << *$2 << "' redeclarata\n";
    } else {
        activeScope->addVar($1, $2);

        // inițializare prin AST (execuția e în evaluate)
        ASTNode* asg = new ASTNode("=");
        asg->addChild(new ASTNode("ID", *$2));
        asg->addChild($4);
        if (!globalInit) globalInit = new ASTNode("BLOCK");
        globalInit->addChild(asg);
    }
}
;

tip_date : INT    { $$ = new string("int"); }
         | FLOAT  { $$ = new string("float"); }
         | STRING { $$ = new string("string"); }
         | BOOL   { $$ = new string("bool"); }
         | ID     { $$ = $1; }              
         | VOID   { $$ = new string("void"); }
         ;

//parametrii din scope ul functiei
lista_param : parametri
            | /*epsilon*/
            ;

parametri : parametri ',' un_param
          | un_param
          ;

un_param : tip_date ID
{
    currentParamTypes.push_back(stringToType(*$1));
    currentParamNames.push_back(*$2);

    if (activeScope->existsLocal($2)) {
        cerr << "Eroare linia " << yylineno<< ": parametrul '" << *$2<< "' este redeclarat\n";
    } else {
        activeScope->addVar($1, $2);
    }
}
;

//corp functie
bloc_cod : lista_instr 
     {
        $$=$1;
     }
    ;

lista_instr
    : lista_instr instr_unica
      {
          $$ = $1;
          if ($2) $$->addChild($2);
      }
    | /*epsilon*/
      {
          $$ = new ASTNode("BLOCK");
      }
    ;

instr_unica
    : decl_locala   { $$ = $1; }  
    | stmt          { $$ = $1; }
    ;

//decl locale
decl_locala
    : tip_date ID ';'
      {
          // semantic: nu ai voie în main
          if (activeScope->getScopeName() == "main_scope") {
              yyerror("Restricted: Cannot declare variables in Main block!");
              exit(1);
          }

          // semantic: redeclarare în același scope
          if (activeScope->existsLocal($2)) {
              cerr << "Eroare linia " << yylineno << ": variabila '" << *$2
                   << "' redeclarata in acelasi scope\n";
          } else {
              activeScope->addVar($1, $2);
          }

          // AST care va crea variabila 
          $$ = new ASTNode("DECL", *$1);
          $$->addChild(new ASTNode("ID", *$2));
      }
    | tip_date ID '=' expr ';'
      {
          if (activeScope->getScopeName() == "main_scope") {
              yyerror("Restricted: Cannot declare variables in Main block!");
              exit(1);
          }

          if (activeScope->existsLocal($2)) {
              cerr << "Eroare linia " << yylineno << ": variabila '" << *$2
                   << "' redeclarata in acelasi scope\n";
          } else {
              activeScope->addVar($1, $2);
          }

          $$ = new ASTNode("DECL", *$1);
          $$->addChild(new ASTNode("ID", *$2));
          $$->addChild($4); // init
      }
    ;


//main
main : MAIN 
{
    activeScope = new SymTable("main_scope", activeScope);
    allTables.push_back(activeScope);
}
'{' lista_stmts '}'
{
    mainBlock = $4;
    activeScope = activeScope->getParent();
}
;

lista_stmts
    : lista_stmts stmt
      {
          $$ = $1;
          $$->addChild($2);
      }
    | /*epsilon*/
      {
          $$ = new ASTNode("BLOCK");
      }
    ;

stmt
    : atribuire ';'                 { $$ = $1; }
    | control                       { $$ = $1; }
    | apel_func ';'                 { $$ = $1; }
    | PRINT '(' expr ')' ';'
      {
          $$ = new ASTNode("PRINT");
          $$->addChild($3);
      }
    | RETURN expr ';'
      {
          $$ = new ASTNode("RETURN");
          $$->addChild($2);
      }
    | error ';'                     
    { 
        yyerror("invalid statement"); yyerrok; $$ = nullptr;
    }
    ;

// atribuire 
atribuire
    : ID '=' expr
      {
          SymbolData* sym = activeScope->lookup($1);
          if (!sym) {
              cerr << "Eroare linia " << yylineno<< ": identificatorul '" << *$1<< "' nu a fost declarat\n";
          }

          $$ = new ASTNode("=");
          $$->addChild(new ASTNode("ID", *$1));
          $$->addChild($3);
      }
    | ID '.' ID '=' expr
      {
          SymbolData* obj = activeScope->lookup($1);
          if (!obj) {
              cerr << "Eroare linia " << yylineno  << ": obiectul '" << *$1 << "' nu este declarat\n";
          } 
          else if (obj->type != TYPE_CLASS) {
              cerr << "Eroare linia " << yylineno << ": '" << *$1 << "' nu este obiect de clasa\n";
          }

          ASTNode* field = new ASTNode("FIELD");
          field->addChild(new ASTNode("ID", *$1));
          field->addChild(new ASTNode("ID", *$3));

          $$ = new ASTNode("=");
          $$->addChild(field);
          $$->addChild($5);
      }
    ;

// control
control
    : IF '(' expr ')'  block 
      {
          $$ = new ASTNode("IF");
          $$->addChild($3);
          $$->addChild($5);
      }
    | IF '(' expr ')' block ELSE block
      {
          $$ = new ASTNode("IF");
          $$->addChild($3);
          $$->addChild($5);
          $$->addChild($7);
      }
    | WHILE '(' expr ')' block
      {
          $$ = new ASTNode("WHILE");
          $$->addChild($3);
          $$->addChild($5);
      }
    | FOR '(' ID '=' expr ';' expr ';' ID '=' expr ')' block
      {
          $$ = new ASTNode("FOR");

          // i = 0;
          ASTNode* init = new ASTNode("=");
          init->addChild(new ASTNode("ID", *$3));
          init->addChild($5);
          $$->addChild(init);

          // cond i < 5
          $$->addChild($7);

          // pas i = i + 1
          ASTNode* step = new ASTNode("=");
          step->addChild(new ASTNode("ID", *$9));
          step->addChild($11);
          $$->addChild(step);

          // corp
          $$->addChild($13);
      }
    ;

block
    : '{' lista_stmts '}'
      {
          $$ = $2;
      }
    ;

// apel func
apel_func
    : ID '(' apel_args ')'
    {
         if (!activeScope->lookupFunction(*$1)) {
        cerr << "Eroare linia " << yylineno
         << ": functia '" << *$1 << "' nu este declarata\n";
        }

          $$ = new ASTNode("CALL", *$1);
          $$->addChild($3);
    }
    | ID '.' ID '(' apel_args ')'
      {
          SymbolData* obj = activeScope->lookup($1);
          if (!obj) {
              cerr << "Eroare linia " << yylineno << ": obiectul '" << *$1 << "' nu este declarat\n";
          }
          else if (obj->type != TYPE_CLASS) {
              cerr << "Eroare linia " << yylineno << ": '" << *$1 << "' nu este obiect de clasa\n";
          }

          ASTNode* call = new ASTNode("CALL_METHOD", *$3); //sum
          call->addChild(new ASTNode("ID", *$1)); // p
          call->addChild($5);                    // 3,5
          $$ = call;
      }
    ;

apel_args
    : /*epsilon*/ { $$ = new ASTNode("ARGS"); }
    | args        { $$ = $1; }
    ;

args
    : expr
      {
          $$ = new ASTNode("ARGS");
          $$->addChild($1);
      }
    | args ',' expr
      {
          $$ = $1;
          $$->addChild($3);
      }
    ;

// expresii
expr
    : expr OR expr       { $$ = new ASTNode("||"); $$->addChild($1); $$->addChild($3); }
    | expr AND expr      { $$ = new ASTNode("&&"); $$->addChild($1); $$->addChild($3); }
    | expr EQ expr       { $$ = new ASTNode("=="); $$->addChild($1); $$->addChild($3); }
    | expr NEQ expr      { $$ = new ASTNode("!="); $$->addChild($1); $$->addChild($3); }
    | expr '<' expr      { $$ = new ASTNode("<");  $$->addChild($1); $$->addChild($3); }
    | expr '>' expr      { $$ = new ASTNode(">");  $$->addChild($1); $$->addChild($3); }
    | expr LE expr       { $$ = new ASTNode("<="); $$->addChild($1); $$->addChild($3); }
    | expr GE expr       { $$ = new ASTNode(">="); $$->addChild($1); $$->addChild($3); }
    | expr '+' expr      { $$ = new ASTNode("+");  $$->addChild($1); $$->addChild($3); }
    | expr '-' expr      { $$ = new ASTNode("-");  $$->addChild($1); $$->addChild($3); }
    | expr '*' expr      { $$ = new ASTNode("*");  $$->addChild($1); $$->addChild($3); }
    | expr '/' expr      { $$ = new ASTNode("/");  $$->addChild($1); $$->addChild($3); }
    | expr '%' expr      { $$ = new ASTNode("%");  $$->addChild($1); $$->addChild($3); }
    | NOT expr           { $$ = new ASTNode("!");  $$->addChild($2); }
    | '(' expr ')'       { $$ = $2; }
    | valoare            { $$ = $1; }
    | apel_func          { $$ = $1; }
    ;

valoare
    : INT_VAL   { $$ = new ASTNode("CONST", to_string($1), TYPE_INT); }
    | FLOAT_VAL { $$ = new ASTNode("CONST", to_string($1), TYPE_FLOAT); }
    | STRING_VAL 
      { 
        std::string s = *$1;
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);
        $$ = new ASTNode("CONST", s, TYPE_STRING); 
      }
    | BOOL_VAL  { $$ = new ASTNode("CONST", ($1 ? "true" : "false"), TYPE_BOOL); }
    | ID        
      { 
         SymbolData* sym = activeScope->lookup($1);
         if (!sym) yyerror("Eroare: Variabila folosita fara declarare.");
         $$ = new ASTNode("ID", *$1); 
      }
    | ID '.' ID
      {
          ASTNode* field = new ASTNode("FIELD");
          field->addChild(new ASTNode("ID", *$1));
          field->addChild(new ASTNode("ID", *$3));
          $$ = field;
      }
    ;
%%

void yyerror(const char * s) {
     cout << "Eroare Sintaxa (Linia " << yylineno << "): " << s << " (token: " << (yytext ? yytext : "") << ")" << endl;
}

int main(int argc, char** argv) {
    root = new SymTable("global");
    activeScope = root;
    allTables.push_back(root);

    if (argc > 1) yyin = fopen(argv[1], "r");
    globalInit = new ASTNode("BLOCK");
    yyparse();


    
    // execută inițializările globale
if (globalInit) {
    globalInit->evaluate(root);
}

    if (mainBlock) {
    SymTable* runtimeMain = new SymTable("main_scope", root);
    allTables.push_back(runtimeMain);
    mainBlock->evaluate(runtimeMain);
}

std::ofstream out("tables.txt");
    std::streambuf* oldBuf = std::cout.rdbuf(out.rdbuf());

    for (auto* t : allTables) {
        if (t) t->printVars();
    }

    std::cout.rdbuf(oldBuf);
    out.close();
std::cout << "Program parsat cu succes!" << std::endl;
return 0;

}
