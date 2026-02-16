#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <iostream>

class SymTable;
struct Value;

extern int yylineno;

enum DataType
{
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_VOID,
    TYPE_CLASS,
    TYPE_FUNC,
    TYPE_UNKNOWN
};

class ASTNode
{
public:
    std::string op;
    std::vector<ASTNode *> children;
    std::string val;
    DataType constType;
    int line; // linia (pt erori semantice)

    ASTNode(const std::string &o,
            const std::string &v = "",
            DataType t = TYPE_UNKNOWN,
            int ln = -1)
        : op(o), val(v), constType(t),
          line(ln >= 0 ? ln : yylineno) {}

    void addChild(ASTNode *n)
    {
        if (n)
            children.push_back(n);
    }

    Value evaluate(SymTable *scope);
};

#endif
