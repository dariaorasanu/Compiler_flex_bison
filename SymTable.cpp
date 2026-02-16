#include "SymTable.h"
extern std::vector<SymTable*> allTables; 

DataType stringToType(const std::string &s)
{
    if (s == "int")
        return TYPE_INT;
    if (s == "float")
        return TYPE_FLOAT;
    if (s == "string")
        return TYPE_STRING;
    if (s == "bool")
        return TYPE_BOOL;
    if (s == "void")
        return TYPE_VOID;
    return TYPE_CLASS;
}

std::string typeToString(DataType t)
{
    switch (t)
    {
    case TYPE_INT:
        return "int";
    case TYPE_FLOAT:
        return "float";
    case TYPE_STRING:
        return "string";
    case TYPE_BOOL:
        return "bool";
    case TYPE_VOID:
        return "void";
    case TYPE_CLASS:
        return "class";
    default:
        return "unknown";
    }
}

SymTable::SymTable(string name, SymTable *parent)
{
    this->contextName = name;
    this->parentScope = parent;
}

string SymTable::getScopeName()
{
    return this->contextName;
}

SymTable *SymTable::getParent()
{
    return this->parentScope;
}

void SymTable::addVar(string *type, string *name)
{

    if (elements.find(*name) != elements.end())
    {
        cerr << "[Eroare] Variabila '" << *name << "' este deja declarata in acest scope.\n";
        return;
    }

    SymbolData data;
    DataType t = stringToType(*type);
    data.type = t;

    if (t == TYPE_CLASS)
    {
        data.customType = *type;
    }

    data.value.type = data.type;
    data.value.i_val = 0;
    data.value.f_val = 0.0f;
    data.value.s_val = "";
    data.value.b_val = false;

    elements[*name] = data;
}

SymbolData *SymTable::lookup(string *name)
{
    auto it = elements.find(*name);
    if (it != elements.end())
    {
        return &it->second;
    }

    if (parentScope != nullptr)
    {
        return parentScope->lookup(name);
    }
    return nullptr;
}

void SymTable::setValue(string *name, float val)
{

    if (elements.find(*name) != elements.end())
    {
        SymbolData &data = elements[*name];

        if (data.type == TYPE_INT)
            data.value.i_val = (int)val;
        else if (data.type == TYPE_FLOAT)
            data.value.f_val = val;
        else if (data.type == TYPE_BOOL)
            data.value.b_val = (bool)val;

        return;
    }

    if (parentScope != nullptr)
    {
        parentScope->setValue(name, val);
        return;
    }

    cerr << "[Eroare] Variabila '" << *name << "' nu a fost declarata!" << endl;
}

void SymTable::printVars()
{
    cout << "=== Scope: " << contextName << " ===" << endl;

    if (parentScope)
        cout << "Parent: " << parentScope->contextName << endl;

    for (auto &it : elements)
    {
        cout << "ID: " << it.first;

        if (it.second.type == TYPE_FUNC)
        {
            cout << " | Function return: " << it.second.customType;
            cout << " | Params: (";

            for (size_t i = 0; i < it.second.paramTypes.size(); i++)
            {
                cout << typeToString(it.second.paramTypes[i]);
                if (i + 1 < it.second.paramTypes.size())
                    cout << ", ";
            }
            cout << ")";
        }
        else
        {
            if (it.second.type == TYPE_CLASS)
            {
                if (it.second.customType == "class")
                {
                    cout << " | Type: class " << it.first;
                }
                else
                {
                    cout << " | Type: class " << it.second.customType;
                }
            }
            else if (it.second.type == TYPE_INT)
                cout << " | Value: " << it.second.value.i_val;
            else if (it.second.type == TYPE_FLOAT)
                cout << " | Value: " << it.second.value.f_val;
            else if (it.second.type == TYPE_BOOL)
                cout << " | Value: "
                     << (it.second.value.b_val ? "true" : "false");
            else if (it.second.type == TYPE_STRING)
                cout << " | Value: " << it.second.value.s_val;
        }
        cout << endl;
    }
    cout << endl;
}

bool SymTable::existsLocal(string *name)
{
    return elements.find(*name) != elements.end();
}

bool SymTable::hasFunctionName(const std::string &name)
{
    for (auto &it : elements)
    {
        if (it.first.rfind(name + "#", 0) == 0)
            return true;
    }
    return false;
}
void SymTable::setParamValues(const vector<Value> &args)
{
    size_t i = 0;

    for (auto &it : elements)
    {
        // luăm doar variabilele (parametrii)
        if (it.second.type != TYPE_FUNC)
        {
            if (i < args.size())
            {
                it.second.value = args[i];
                i++;
            }
        }
    }
}
SymbolData *SymTable::lookupFunction(const std::string &name)
{
    // cautăm funcția după prefix: name#
    for (auto &it : elements)
    {
        if (it.first.rfind(name + "#", 0) == 0)
        {
            return &it.second;
        }
    }

    // dacă nu e în scope-ul curent, căutăm în părinte
    if (parentScope)
    {
        return parentScope->lookupFunction(name);
    }

    return nullptr;
}



static std::string makeFuncKey(const std::string &name, const std::vector<DataType> &params)
{
    std::string key = name;
    for (auto t : params)
    {
        key += "#";
        key += typeToString(t);
    }
    return key;
}

void SymTable::addFunction(string *name,
                           DataType returnType,
                           const vector<DataType> &params,
                           const vector<std::string> &paramNames,
                           ASTNode *body)
{
    std::string key = makeFuncKey(*name, params);

    if (elements.find(key) != elements.end())
    {
        cerr << "[Eroare] Functie redeclarata: " << key << endl;
        return;
    }

    SymbolData data;
    data.type = TYPE_FUNC;
    data.customType = typeToString(returnType);
    data.paramTypes = params;
    data.paramNames = paramNames; 
    data.body = body;

    elements[key] = data;
}
SymbolData *SymTable::lookupFunctionBySignature(const std::string &name,
                                                const std::vector<DataType> &argTypes)
{
    std::string key = makeFuncKey(name, argTypes);
    auto it = elements.find(key);
    if (it != elements.end())
        return &it->second;
    if (parentScope) 
        return parentScope->lookupFunctionBySignature(name, argTypes);
    else
        return nullptr;
}

SymbolData *SymTable::lookupFunctionBySignatureLocal(const std::string &name,
                                                     const std::vector<DataType> &argTypes)
{
    std::string key = makeFuncKey(name, argTypes);
    auto it = elements.find(key);
    if (it != elements.end())
        return &it->second;
    return nullptr;
}
SymTable* SymTable::findClassTable(const std::string& className) {
    std::string scopeName = "class_" + className;
    for (auto* t : allTables) {
        if (t && t->getScopeName() == scopeName) return t;
    }
    return nullptr;
}
SymTable* SymTable::findDeclScope(SymTable* start, const std::string& varName)
{
    std::string tmp = varName;
    for (SymTable* s = start; s; s = s->getParent())
    {
        if (s->existsLocal(&tmp))
            return s; // aici e declarat local
    }
    return start; 
}

SymTable::~SymTable()
{
    elements.clear();
}