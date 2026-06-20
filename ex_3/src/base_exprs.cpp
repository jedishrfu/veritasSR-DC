#include <string>
#include <vector>
#include <set>

#include "../include/ast_nodes.h"
#include "../include/util_code.h"
#include "../include/expr_array.h"

ExprArray* generateBasicExpressions()
{
    ensureDefaultVariables();

    ExprArray* result = new ExprArray();
    std::set<std::string> seen;

    std::vector<std::string> vars = makeParserVariableNames();

    logPrint("VarCount = %d", varTable.getCount());

    std::vector<std::string> templates;

    for (size_t i = 0; i < vars.size(); i++)
    {
        const std::string& x = vars[i];

        templates.push_back(x);
        templates.push_back("sin(" + x + ")");
        templates.push_back("cos(" + x + ")");
        templates.push_back("exp(" + x + ")");
        templates.push_back("log(" + x + ")");

        templates.push_back(x + " + 1");
        templates.push_back(x + " - 1");
        templates.push_back("1 - " + x);
        templates.push_back(x + " * 2");
        templates.push_back(x + " / 2");

        templates.push_back("2 * " + x);
        templates.push_back("0.5 * " + x);
        templates.push_back("2 * " + x + " + 1");
        templates.push_back("0.5 * " + x + " + 1");

        templates.push_back(x + " * " + x);
        templates.push_back(x + " * " + x + " + " + x);
        templates.push_back("sin(" + x + ") + " + x);
        templates.push_back("cos(" + x + ") + " + x);
        templates.push_back("sin(" + x + ") * " + x);
        templates.push_back("cos(" + x + ") * " + x);
        templates.push_back("exp(" + x + ") + " + x);
        templates.push_back("log(" + x + ") + " + x);

        templates.push_back("sin(" + x + ") + cos(" + x + ")");
        templates.push_back("sin(" + x + ") * cos(" + x + ")");
    }

    if (vars.size() >= 2)
    {
        for (size_t i = 0; i + 1 < vars.size(); i++)
        {
            const std::string& x = vars[i];
            const std::string& y = vars[i + 1];

            templates.push_back(x + " + " + y);
            templates.push_back(x + " - " + y);
            templates.push_back(x + " * " + y);
            templates.push_back(x + " / " + y);

            templates.push_back("sin(" + x + ") + cos(" + y + ")");
            templates.push_back("sin(" + x + ") * exp(" + y + ")");
            templates.push_back("(" + x + " + " + y + ") * (" + x + " - " + y + ")");
            templates.push_back("2 * " + x + " + 3 * " + y);
        }
    }

    for (size_t i = 0; i < templates.size(); i++)
    {
        Node* root = parseExpression(templates[i], vars);

        if (!root)
        {
            logWarning(
                "Parser rejected expression '%s': %s",
                templates[i].c_str(),
                getLastParseError().c_str());
            continue;
        }

        addUniqueTree(result, root, seen);
    }

    logPrint("\nInitial pool generated from infix templates: %d expressions", result->size());

    return result;
}
