%{
#include <stdio.h>
#include "limbaj.tab.h"
#include <string>
using namespace std;
%}

%option noyywrap
%%

"int"       { return INT; }
"float"     { return FLOAT; }
"string"    { return STRING; }
"bool"      { return BOOL; }
"class"     { return CLASS; }
"main"      { return MAIN; }
"Print"     { return PRINT; }
"if"        { return IF; }
"else"      { return ELSE; }
"while"     { return WHILE; }
"for"       { return FOR; }
"return"    { return RETURN; }
"void"      { return VOID; }

"="         { return '='; }
"=="        { return EQ; }
"!="        { return NEQ; }
"<="        { return LE; }
">="        { return GE; }
"&&"        { return AND; }
"||"        { return OR; }
"!"         { return NOT; }

"true"  { yylval.val = 1; return BOOL_VAL; }
"false" { yylval.val = 0; return BOOL_VAL; }

[_a-zA-Z][_a-zA-Z0-9]* { yylval.Str = new string(yytext); return ID; }

[0-9]+\.[0-9]+ { yylval.floatVal = atof(yytext); return FLOAT_VAL; }

[0-9]+  { yylval.val = atoi(yytext); return INT_VAL; }


\"[^\"]*\" { yylval.Str = new string(yytext); return STRING_VAL; }

"//".* { } // comentarii asa
"/*"([^*]|\*+[^*/])*\*+"/" { 
    for (char* p = yytext; *p; ++p) 
        if (*p == '\n') yylineno++;
}/* comentarii asa*/
[ \t\r] ; 
\n {yylineno++;}
. {return yytext[0];}

%%
