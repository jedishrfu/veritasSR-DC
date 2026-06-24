#include <string>
#include <vector>
#include <set>

#include "../include/ast_nodes.h"
#include "../include/util_code.h"
#include "../include/expr_array.h"
#include "../include/expr_parser.h"
#include "../include/var_table.h"

extern VarTable varTable;

// Build an ExprArray by parsing a list of text-based infix expressions.
// Example input strings:
//   "x"
//   "sin(x)"
//   "2 * x + 1"
//   "sin(x) + cos(x)"
ExprArray* generateBasicExpressionsFromText(
    const std::vector<std::string>& expressionTexts)
{
    ensureDefaultVariables();

    auto* result = new ExprArray();
    std::set<std::string> seen;

    std::vector<std::string> vars = makeParserVariableNames();

    logPrint("VarCount = %d", varTable.getCount());

    for (const std::string& exprText : expressionTexts) {
        Node* root = parseExpression(exprText, vars);

        if (!root) {
            logWarn(
                "Parser rejected expression '%s': %s",
                exprText.c_str(),
                getLastParseError().c_str());
            continue;
        }

        addUniqueTree(result, root, seen);
    }

    logPrint(
        "\nInitial pool generated from text expressions: %d expressions",
        result->size());

    return result;
}

// Default seed-expression generator.
// This keeps the old public interface, but now the actual AST construction
// happens through generateBasicExpressionsFromText().
ExprArray* generateBasicExpressions()
{
    ensureDefaultVariables();

    std::vector<std::string> vars = makeParserVariableNames();
    std::vector<std::string> expressionTexts;

    for (const std::string& x : vars) {
        expressionTexts.push_back(x);
        expressionTexts.push_back("sin(" + x + ")");
        expressionTexts.push_back("cos(" + x + ")");
        expressionTexts.push_back("exp(" + x + ")");
        expressionTexts.push_back("log(" + x + ")");

        expressionTexts.push_back(x + " + 1");
        expressionTexts.push_back(x + " - 1");
        expressionTexts.push_back("1 - " + x);
        expressionTexts.push_back(x + " * 2");
        expressionTexts.push_back(x + " / 2");

        expressionTexts.push_back("2 * " + x);
        expressionTexts.push_back("0.5 * " + x);
        expressionTexts.push_back("2 * " + x + " + 1");
        expressionTexts.push_back("0.5 * " + x + " + 1");

        expressionTexts.push_back(x + " * " + x);
        expressionTexts.push_back(x + " * " + x + " + " + x);
        expressionTexts.push_back("sin(" + x + ") + " + x);
        expressionTexts.push_back("cos(" + x + ") + " + x);
        expressionTexts.push_back("sin(" + x + ") * " + x);
        expressionTexts.push_back("cos(" + x + ") * " + x);
        expressionTexts.push_back("exp(" + x + ") + " + x);
        expressionTexts.push_back("log(" + x + ") + " + x);

        expressionTexts.push_back("sin(" + x + ") + cos(" + x + ")");
        expressionTexts.push_back("sin(" + x + ") * cos(" + x + ")");
    }

    if (vars.size() >= 2) {
        for (size_t i = 0; i + 1 < vars.size(); i++) {
            const std::string& x = vars[i];
            const std::string& y = vars[i + 1];

            expressionTexts.push_back(x + " + " + y);
            expressionTexts.push_back(x + " - " + y);
            expressionTexts.push_back(x + " * " + y);
            expressionTexts.push_back(x + " / " + y);

            expressionTexts.push_back("sin(" + x + ") + cos(" + y + ")");
            expressionTexts.push_back("sin(" + x + ") * exp(" + y + ")");
            expressionTexts.push_back("(" + x + " + " + y + ") * (" + x + " - " + y + ")");
            expressionTexts.push_back("2 * " + x + " + 3 * " + y);
        }
    }

    return generateBasicExpressionsFromText(expressionTexts);
}
