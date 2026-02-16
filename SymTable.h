#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include "AST.h"
using namespace std;

// functiile pentru conversii
DataType stringToType(const std::string &s);
string typeToString(DataType t);

// pentru variabile
struct Value
{
    DataType type;
    int i_val;
    float f_val;
    std::string s_val;
    bool b_val;
};


//casuta tables.txt
struct SymbolData
{
    DataType type;
    std::string customType;
    Value value;

    // pentru functii
    vector<DataType> paramTypes;
    vector<std::string> paramNames;
    ASTNode *body;
};

class SymTable
{
private:
    SymTable *parentScope;            // pointer la scope ul parinte
    map<string, SymbolData> elements; // mapul pentru o tabela
    string contextName;               // nume scope
public:
    SymTable(string name, SymTable *parent = nullptr);

    string getScopeName();
    SymTable *getParent();
    SymbolData *lookup(string *name);
    void addVar(string *type, string *name);
    void setValue(string *name, float val);
    void addFunction(string *name,
                     DataType returnType,
                     const vector<DataType> &params,
                     const vector<std::string> &paramNames,
                     ASTNode *body);
        SymbolData* lookupFunctionBySignature(const std::string& name,
                                                const std::vector<DataType>& argTypes);
    SymbolData* lookupFunctionBySignatureLocal(const std::string& name,
                                           const std::vector<DataType>& argTypes);

    bool existsLocal(string *name);
    bool hasFunctionName(const std::string &name);
    void printVars();
    void setParamValues(const vector<Value> &args);
    SymbolData *lookupFunction(const std::string &name);
    static SymTable* findClassTable(const std::string& className);
    static SymTable* findDeclScope(SymTable* start, const std::string& varName);
    ~SymTable();
};

#endif