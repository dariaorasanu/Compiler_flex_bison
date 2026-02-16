#include "AST.h"
#include "SymTable.h"
#include <vector>

extern std::vector<SymTable *> allTables;

static std::string typeNameOf(const SymbolData *s)
{
    if (!s)
        return "unknown";
    if (s->type == TYPE_CLASS)
        return s->customType; // nume clasa
    return typeToString(s->type);
}

Value ASTNode::evaluate(SymTable *scope)
{
    if (op == "DECL")
    {
        std::string typeName = val; // int
        std::string varName = children[0]->val;

        // creează variabila în scope-ul curent
        scope->addVar(new std::string(typeName), new std::string(varName));

        SymbolData *sym = scope->lookup(&varName);
        if (!sym)
            return {TYPE_UNKNOWN};

        // inițializare dacă există
        if (children.size() == 2)
        {
            Value init = children[1]->evaluate(scope);

            if (sym->type != init.type)
            {
                std::cerr << "Semantic error: init type mismatch for '" << varName
                          << "' (linia " << line << ")\n";
                return {TYPE_UNKNOWN};
            }

            sym->value = init;
        }

        return {TYPE_VOID};
    }

    if (op == "CALL_METHOD")
    {
        // children[0] = p
        // children[1] = args

        std::string objName = children[0]->val; // "p"
        std::string method = val;               // "sum"

        // 1) verificăm obiectul
        SymbolData *obj = scope->lookup(&objName);
        if (!obj)
        {
            std::cerr << "Semantic error: object '" << objName << "' not declared"
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }
        if (obj->type != TYPE_CLASS)
        {
            std::cerr << "Semantic error: '" << objName << "' is not an object"
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }

        // 2) găsim tabela clasei
        SymTable *cls = SymTable::findClassTable(obj->customType);
        if (!cls)
        {
            std::cerr << "Semantic error: class '" << obj->customType << "' not found"
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }

        // 3) evaluăm argumentele
        std::vector<Value> argVals;
        std::vector<DataType> argTypes;

        ASTNode *argsNode = children[1]; // args
        for (ASTNode *arg : argsNode->children)
        {
            Value v = arg->evaluate(scope);
            argVals.push_back(v);
            argTypes.push_back(v.type);
        }

        // 4) căutăm metoda în clasa după signatura
        SymbolData *m = cls->lookupFunctionBySignatureLocal(method, argTypes);
        if (!m)
        {
            std::cerr << "Semantic error: method '" << method
                      << "' not found in class '" << obj->customType << "'"
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }

        // 5) scope pentru apel + declarăm parametrii și le setăm valorile
        SymTable *callScope = new SymTable("call_" + objName + "." + method, scope);
        allTables.push_back(callScope);

        if (argVals.size() != m->paramTypes.size())
        {
            std::cerr << "Semantic error: wrong number of args for method " << method
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }

        for (size_t i = 0; i < argVals.size(); i++)
        {
            std::string t = typeToString(m->paramTypes[i]);
            std::string n = m->paramNames[i];

            callScope->addVar(new std::string(t), new std::string(n));
            SymbolData *ps = callScope->lookup(&n);
            if (!ps)
            {
                std::cerr << "Internal error: param var missing: " << n
                          << " (linia " << line << ")\n";
                return {TYPE_UNKNOWN};
            }

            if (ps->type != argVals[i].type)
            {
                std::cerr << "Semantic error: arg type mismatch for param '" << n
                          << "' in method call " << method
                          << " (linia " << line << ")\n";
                return {TYPE_UNKNOWN};
            }

            ps->value = argVals[i];
        }

        return m->body->evaluate(callScope);
    }

    if (op == "CALL")
    {

        // 1) evaluăm argumentele
        std::vector<Value> argVals;
        std::vector<DataType> argTypes;

        ASTNode *args = children[0]; // ARGS
        for (ASTNode *arg : args->children)
        {
            Value v = arg->evaluate(scope);
            argVals.push_back(v);
            argTypes.push_back(v.type);
        }

        // 2) găsim funcția după signatura

        SymbolData *func = scope->lookupFunctionBySignature(val, argTypes);
        if (!func)
        {
            std::cerr << "Semantic error: function '" << val
                      << "' with given signature not found"
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }

        // 3) scope pentru apel + declarăm parametrii
        SymTable *funcScope = new SymTable("call_" + val, scope);
        allTables.push_back(funcScope);

        if (argVals.size() != func->paramTypes.size())
        {
            std::cerr << "Semantic error: wrong number of args for " << val
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }

        for (size_t i = 0; i < argVals.size(); i++)
        {
            std::string t = typeToString(func->paramTypes[i]);
            std::string n = func->paramNames[i];

            // declarare param în scope-ul funcției apelate
            funcScope->addVar(new std::string(t), new std::string(n));

            // setare valoare param
            SymbolData *ps = funcScope->lookup(&n);
            if (!ps)
            {
                std::cerr << "Internal error: param var missing: " << n
                          << " (linia " << line << ")\n";
                return {TYPE_UNKNOWN};
            }

            if (ps->type != argVals[i].type)
            {
                std::cerr << "Semantic error: arg type mismatch for param '" << n
                          << "' in call " << val
                          << " (linia " << line << ")\n";
                return {TYPE_UNKNOWN};
            }

            ps->value = argVals[i];
        }
        // asa evaluam functia
        //  practic dupa ce fac evaluate voi avea rezultatul returnat de apelul functiei
        return func->body->evaluate(funcScope);
    }

    if (op == "RETURN")
    {
        Value v = children[0]->evaluate(scope);

        if (scope && scope->getScopeName() == "main_scope")
        {
            if (v.type == TYPE_INT)
                std::cout << v.i_val;
            if (v.type == TYPE_FLOAT)
                std::cout << v.f_val;
            if (v.type == TYPE_STRING)
                std::cout << v.s_val;
            if (v.type == TYPE_BOOL)
                std::cout << (v.b_val ? "true" : "false");
            std::cout << std::endl;
        }

        return v;
    }

    if (op == "CONST")
    {
        Value v{};
        v.type = constType;

        if (constType == TYPE_INT)
            v.i_val = std::stoi(val);
        if (constType == TYPE_FLOAT)
            v.f_val = std::stof(val);
        if (constType == TYPE_STRING)
            v.s_val = val;
        if (constType == TYPE_BOOL)
            v.b_val = (val == "true");

        return v;
    }

    if (op == "FIELD")
    {
        std::string objName = children[0]->val;   // p
        std::string fieldName = children[1]->val; // x

        // verific daca exista p si daca este obiect
        SymbolData *obj = scope->lookup(&objName);
        if (!obj)
        {
            std::cerr << "Semantic error: object '" << objName << "' not declared"
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }
        if (obj->type != TYPE_CLASS)
        {
            std::cerr << "Semantic error: '" << objName << "' is not an object"
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }
        // verific daca exista clasa in care am gasit ca se afla p
        SymTable *cls = SymTable::findClassTable(obj->customType);
        if (!cls)
        {
            std::cerr << "Semantic error: class '" << obj->customType << "' not found"
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }

        // verificăm că x există în clasă
        if (!cls->existsLocal(&fieldName))
        {
            std::cerr << "Semantic error: field '" << fieldName << "' not in class '"
                      << obj->customType << "'"
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }

        // variabila "p.x"
        std::string key = objName + "." + fieldName;

        // stocăm în scope-ul unde e declarat obiectul (p)
        SymTable *storageScope = SymTable::findDeclScope(scope, objName);

        SymbolData *fld = storageScope->lookup(&key);
        if (!fld)
        {
            // cautam daca exista deja p.x
            SymbolData *decl = cls->lookup(&fieldName);
            // daca nu, ii dam acelasi tip ca lui x
            std::string tname = typeNameOf(decl);
            // adaugam p.x in acel scop in care era si p
            storageScope->addVar(new std::string(tname), new std::string(key));
            fld = storageScope->lookup(&key);
        }

        if (fld)
        {
            return fld->value;
        }
        else
        {
            return Value{TYPE_UNKNOWN};
        }
    }

    if (op == "ID")
    {
        SymbolData *sym = scope->lookup(&val);
        if (!sym)
        {
            std::cerr << "Semantic error: variable '" << val << "' not declared"
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }
        return sym->value;
    }

    if (op == "=")
    {
        // right hand side
        // evaluez expr din dreapta indiferent ce am in stanga
        Value rhs = children[1]->evaluate(scope);

        //  cazul in care am p.x inainte de egal
        if (children[0]->op == "FIELD")
        {
            std::string objName = children[0]->children[0]->val;   // p
            std::string fieldName = children[0]->children[1]->val; // x

            SymbolData *obj = scope->lookup(&objName);
            if (!obj)
            {
                std::cerr << "Semantic error: object '" << objName << "' not declared"
                          << " (linia " << children[0]->line << ")\n";
                return {TYPE_UNKNOWN};
            }
            if (obj->type != TYPE_CLASS)
            {
                std::cerr << "Semantic error: '" << objName << "' is not an object"
                          << " (linia " << children[0]->line << ")\n";
                return {TYPE_UNKNOWN};
            }

            SymTable *cls = SymTable::findClassTable(obj->customType);
            if (!cls)
            {
                std::cerr << "Semantic error: class '" << obj->customType << "' not found"
                          << " (linia " << children[0]->line << ")\n";
                return {TYPE_UNKNOWN};
            }

            if (!cls->existsLocal(&fieldName))
            {
                std::cerr << "Semantic error: field '" << fieldName << "' not in class '"
                          << obj->customType << "'"
                          << " (linia " << children[0]->line << ")\n";
                return {TYPE_UNKNOWN};
            }

            // tipul câmpului îl luăm din clasa
            SymbolData *decl = cls->lookup(&fieldName);
            if (!decl)
            {
                std::cerr << "Internal error: field decl missing for '" << fieldName << "'"
                          << " (linia " << children[0]->line << ")\n";
                return {TYPE_UNKNOWN};
            }

            // verificare tip
            if (decl->type != rhs.type)
            {
                std::cerr << "Semantic error: not the same type (assig) with '"
                          << objName << "." << fieldName << "'"
                          << " (linia " << children[0]->line << ")\n";
                return {TYPE_UNKNOWN};
            }

            // stocare minimă: variabila "p.x" în scope
            std::string key = objName + "." + fieldName;

            // scriem în scope-ul unde e declarat obiectul (ca să persiste)
            SymTable *storageScope = SymTable::findDeclScope(scope, objName);

            SymbolData *fld = storageScope->lookup(&key);
            if (!fld)
            {
                std::string tname = typeNameOf(decl);
                storageScope->addVar(new std::string(tname), new std::string(key));
                fld = storageScope->lookup(&key);
            }

            if (!fld)
            {
                std::cerr << "Internal error: cannot create field storage '" << key << "'"
                          << " (linia " << children[0]->line << ")\n";
                return {TYPE_UNKNOWN};
            }

            fld->value = rhs;
            return rhs;
        }
        // caz normal x = expr
        std::string name = children[0]->val;
        SymbolData *sym = scope->lookup(&name);

        if (!sym)
        {
            std::cerr << "Semantic error: assignment to undeclared '" << name << "'"
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }
        // nu au acelasi tip
        if (sym->type != rhs.type)
        {
            std::cerr << "Semantic error: not the same type (assig) with '" << name << "'"
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }

        sym->value = rhs;
        // returnez ce aveam in dreapta adica expr ca sa pot evalua
        return rhs;
    }

    if (op == "PRINT")
    {
        Value v = children[0]->evaluate(scope);

        if (v.type == TYPE_INT)
            std::cout << v.i_val;
        if (v.type == TYPE_FLOAT)
            std::cout << v.f_val;
        if (v.type == TYPE_STRING)
            std::cout << v.s_val;
        if (v.type == TYPE_BOOL)
            std::cout << (v.b_val ? "true" : "false");

        std::cout << std::endl;
        return v;
    }

    if (op == "BLOCK")
    {
        Value last{TYPE_VOID};
        for (auto *c : children)
        {
            last = c->evaluate(scope);
        }
        return last;
    }

    if (op == "IF")
    {
        Value cond = children[0]->evaluate(scope);

        if (cond.type != TYPE_BOOL)
        {
            std::cerr << "Semantic error: IF condition must be bool"
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }
        // daca conditia se respecta
        if (cond.b_val)
            return children[1]->evaluate(scope);
        // daca avem un else -> intra pe el
        else if (children.size() == 3)
            return children[2]->evaluate(scope);

        return {TYPE_VOID};
    }

    if (op == "WHILE")
    {
        while (true)
        {
            Value cond = children[0]->evaluate(scope);
            if (cond.type != TYPE_BOOL)
            {
                cout <<cond.type << "\n";
                std::cerr << "Semantic error: WHILE condition must be bool"
                          << " (linia " << line << ")\n";
                
                return {TYPE_UNKNOWN};
            }
            // odata ce condiita nu mai este adevarata ies din while
            if (!cond.b_val)
                break;
            // daca conditia e ok execut ce am in while
            children[1]->evaluate(scope);
        }
        return {TYPE_VOID};
    }

    if (op == "FOR")
    {
        // copii: [0]=init, [1]=cond, [2]=step, [3]=body

        // 1) init
        children[0]->evaluate(scope);

        // 2) loop
        while (true)
        {
            Value cond = children[1]->evaluate(scope);

            // cond trebuie să fie bool
            if (cond.type != TYPE_BOOL)
            {
                std::cerr << "Semantic error: FOR condition must be bool"
                          << " (linia " << line << ")\n";
                return {TYPE_UNKNOWN};
            }

            // odata ce condiita nu mai este adevarata ies din for
            if (!cond.b_val)
                break;

            // 3) body
            children[3]->evaluate(scope);

            // 4) step
            children[2]->evaluate(scope);
        }

        return {TYPE_VOID};
    }

    if (op == "!")
    {
        Value v = children[0]->evaluate(scope);
        if (v.type != TYPE_BOOL)
        {
            std::cerr << "Semantic error: ! expects bool"
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }
        Value res;
        res.type = TYPE_BOOL;
        res.b_val = !v.b_val;
        return res;
    }

    Value a = children[0]->evaluate(scope);
    Value b = children[1]->evaluate(scope);

    if (a.type != b.type)
    {
        std::cerr << "Semantic error: not the same type (expr) "
                  << " (linia " << line << ")\n";
        return {TYPE_UNKNOWN};
    }
    if (op == "&&" || op == "||")
    {
        if (a.type != TYPE_BOOL)
        {
            std::cerr << "Semantic error: logical operators require bool operands"
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }
    }
    if (op == "<" || op == ">" || op == "<=" || op == ">=" || op == "+" || op == "-" || op == "*" || op == "/" || op == "%")
    {
        if (!(a.type == TYPE_INT || a.type == TYPE_FLOAT))
        {
            std::cerr << "Semantic error: comparison operators require int/float"
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }
    }

    Value r{};
    r.type = a.type;

    if (op == "+")
    {
        if (a.type == TYPE_INT)
            r.i_val = a.i_val + b.i_val;
        if (a.type == TYPE_FLOAT)
            r.f_val = a.f_val + b.f_val;
        if (a.type == TYPE_STRING)
            r.s_val = a.s_val + b.s_val;
    }
    else if (op == "-")
    {
        if (a.type == TYPE_INT)
            r.i_val = a.i_val - b.i_val;
        if (a.type == TYPE_FLOAT)
            r.f_val = a.f_val - b.f_val;
    }
    else if (op == "*")
    {
        if (a.type == TYPE_INT)
            r.i_val = a.i_val * b.i_val;
        if (a.type == TYPE_FLOAT)
            r.f_val = a.f_val * b.f_val;
    }
    else if (op == "/")
    {
        if (a.type == TYPE_INT)
            r.i_val = a.i_val / (b.i_val ? b.i_val : 1);
        if (a.type == TYPE_FLOAT)
            r.f_val = a.f_val / (b.f_val ? b.f_val : 1.0f);
    }
    else if (op == "%")
    {
        if (a.type == TYPE_INT)
            r.i_val = a.i_val % b.i_val;
    }
    else if (op == "<")
    {
        r.type = TYPE_BOOL;
        r.b_val = (a.type == TYPE_INT) ? (a.i_val < b.i_val) : (a.f_val < b.f_val);
    }
    else if (op == ">")
    {
        r.type = TYPE_BOOL;
        r.b_val = (a.type == TYPE_INT) ? (a.i_val > b.i_val) : (a.f_val > b.f_val);
    }
    else if (op == "<=")
    {
        r.type = TYPE_BOOL;
        r.b_val = (a.type == TYPE_INT) ? (a.i_val <= b.i_val) : (a.f_val <= b.f_val);
    }
    else if (op == ">=")
    {
        r.type = TYPE_BOOL;
        r.b_val = (a.type == TYPE_INT) ? (a.i_val >= b.i_val) : (a.f_val >= b.f_val);
    }

    else if (op == "==" || op == "!=")
    {
        r.type = TYPE_BOOL;
        bool eq = false;

        if (a.type == TYPE_INT)
            eq = (a.i_val == b.i_val);
        else if (a.type == TYPE_FLOAT)
            eq = (a.f_val == b.f_val);
        else if (a.type == TYPE_BOOL)
            eq = (a.b_val == b.b_val);
        else if (a.type == TYPE_STRING)
            eq = (a.s_val == b.s_val);
        else
        {
            std::cerr << "Semantic error: ==/!= not supported for this type"
                      << " (linia " << line << ")\n";
            return {TYPE_UNKNOWN};
        }

        r.b_val = (op == "==") ? eq : !eq;
    }

    else if (op == "&&")
    {
        r.type = TYPE_BOOL;
        if(a.type == TYPE_BOOL && b.type == TYPE_BOOL)
        r.b_val = a.b_val && b.b_val;
    }
    else if (op == "||")
    {
        r.type = TYPE_BOOL;
        if(a.type == TYPE_BOOL && b.type == TYPE_BOOL)
        r.b_val = a.b_val || b.b_val;
    }

    return r;
}
