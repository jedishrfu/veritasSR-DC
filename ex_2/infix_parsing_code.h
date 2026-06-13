#include "ast_nodes.h"

#include <string>
#include <vector>
#include <cctype>
#include <cstdlib>

#ifndef INFIX_PARSING_CODE
#define INFIX_PARSING_CODE

//VarTable varTable;

static std::string g_parseError;

const std::string& getLastParseError()
{
    return g_parseError;
}

class ExprParser
{
private:
    const std::string& text;
    const std::vector<std::string>& vars;
    size_t pos;

public:
    ExprParser(const std::string& s,
        const std::vector<std::string>& validVars)
        : text(s), vars(validVars), pos(0)
    {
    }

    Node* parse()
    {
        Node* root = parseAddSub();
        skipSpaces();

        if (!root)
            return nullptr;

        if (pos != text.size())
        {
            setError("Unexpected text at position " + std::to_string(pos));
            delete root;
            return nullptr;
        }

        return root;
    }

private:
    void setError(const std::string& msg)
    {
        if (g_parseError.empty())
            g_parseError = msg;
    }

    void skipSpaces()
    {
        while (pos < text.size() &&
            std::isspace((unsigned char)text[pos]))
        {
            pos++;
        }
    }

    bool match(char c)
    {
        skipSpaces();

        if (pos < text.size() && text[pos] == c)
        {
            pos++;
            return true;
        }

        return false;
    }

    char peek()
    {
        skipSpaces();

        if (pos >= text.size())
            return '\0';

        return text[pos];
    }

    Node* parseAddSub()
    {
        Node* left = parseMulDiv();

        if (!left)
            return nullptr;

        while (true)
        {
            if (match('+'))
            {
                Node* right = parseMulDiv();

                if (!right)
                {
                    delete left;
                    setError("Missing right operand after +");
                    return nullptr;
                }

                left = Node::makeBinary(OP_ADD, left, right);
            }
            else if (match('-'))
            {
                Node* right = parseMulDiv();

                if (!right)
                {
                    delete left;
                    setError("Missing right operand after -");
                    return nullptr;
                }

                left = Node::makeBinary(OP_SUB, left, right);
            }
            else
            {
                break;
            }
        }

        return left;
    }

    Node* parseMulDiv()
    {
        Node* left = parseUnary();

        if (!left)
            return nullptr;

        while (true)
        {
            if (match('*'))
            {
                Node* right = parseUnary();

                if (!right)
                {
                    delete left;
                    setError("Missing right operand after *");
                    return nullptr;
                }

                left = Node::makeBinary(OP_MUL, left, right);
            }
            else if (match('/'))
            {
                Node* right = parseUnary();

                if (!right)
                {
                    delete left;
                    setError("Missing right operand after /");
                    return nullptr;
                }

                left = Node::makeBinary(OP_DIV, left, right);
            }
            else
            {
                break;
            }
        }

        return left;
    }

    Node* parseUnary()
    {
        skipSpaces();

        if (match('-'))
        {
            Node* child = parseUnary();

            if (!child)
            {
                setError("Missing operand after unary -");
                return nullptr;
            }

            return Node::makeBinary(
                OP_SUB,
                Node::makeCoeffValue(0.0),
                child
            );
        }

        if (std::isalpha((unsigned char)peek()) || peek() == '_')
        {
            size_t savePos = pos;
            std::string name = parseIdentifier();

            skipSpaces();

            if (match('('))
            {
                Node* arg = parseAddSub();

                if (!arg)
                {
                    setError("Missing argument for function " + name);
                    return nullptr;
                }

                if (!match(')'))
                {
                    delete arg;
                    setError("Missing closing parenthesis after function " + name);
                    return nullptr;
                }

                OpKind op = OP_NONE;

                if (name == "sin") op = OP_SIN;
                else if (name == "cos") op = OP_COS;
                else if (name == "exp") op = OP_EXP;
                else if (name == "log") op = OP_LOG;
                else
                {
                    delete arg;
                    setError("Unknown function: " + name);
                    return nullptr;
                }

                return Node::makeUnary(op, arg);
            }

            pos = savePos;
        }

        return parsePrimary();
    }

    Node* parsePrimary()
    {
        skipSpaces();

        if (match('('))
        {
            Node* inside = parseAddSub();

            if (!inside)
            {
                setError("Missing expression after '('");
                return nullptr;
            }

            if (!match(')'))
            {
                delete inside;
                setError("Mismatched parentheses: missing ')'");
                return nullptr;
            }

            return inside;
        }

        if (std::isdigit((unsigned char)peek()) || peek() == '.')
            return parseNumber();

        if (std::isalpha((unsigned char)peek()) || peek() == '_')
            return parseVariable();

        setError(
            "Expected number, variable, function, or '(' at position "
            + std::to_string(pos)
        );

        return nullptr;
    }

    Node* parseNumber()
    {
        skipSpaces();

        const char* start = text.c_str() + pos;
        char* end = nullptr;

        double value = std::strtod(start, &end);

        if (end == start)
        {
            setError("Invalid number at position " + std::to_string(pos));
            return nullptr;
        }

        pos += (size_t)(end - start);

        return Node::makeCoeffValue(value);
    }

    std::string parseIdentifier()
    {
        skipSpaces();

        size_t start = pos;

        if (pos < text.size() &&
            (std::isalpha((unsigned char)text[pos]) || text[pos] == '_'))
        {
            pos++;

            while (pos < text.size() &&
                (std::isalnum((unsigned char)text[pos]) ||
                    text[pos] == '_'))
            {
                pos++;
            }
        }

        return text.substr(start, pos - start);
    }

    Node* parseVariable()
    {
        std::string name = parseIdentifier();

        for (size_t i = 0; i < vars.size(); i++)
        {
            if (vars[i] == name)
                return Node::makeVariable((int)i);
        }

        setError("Unknown variable: " + name);
        return nullptr;
    }
};

Node* parseExpression(
    const std::string& exprText,
    const std::vector<std::string>& validVariableNames
)
{
    g_parseError.clear();

    ExprParser parser(exprText, validVariableNames);
    return parser.parse();
}

#if 0
int main()
{
    std::vector<std::string> vars;

    vars.push_back("x");
    vars.push_back("y");
    vars.push_back("z");

    const char* tests[] =
    {
        "x",
        "123",
        "x + 2",
        "x + y",
        "x - y",
        "x * y",
        "x / y",
        "sin(x)",
        "cos(y)",
        "exp(x)",
        "log(x)",
        "-x",
        "-(x+y)",
        "sin(x)+cos(y)",
        "x*(y+3)",
        "2*x+3*y",
        "sin(x)*exp(y)",
        "(x+y)*(x-y)",
        "((x))",
        0
    };

    varTable.clear();
    varTable.addVar("x", 2.0);
    varTable.addVar("y", 3.0);
    varTable.addVar("z", 4.0);

    printf("========================================\n");
    printf("Parser Test\n");
    printf("========================================\n");

    for (int i = 0; tests[i] != 0; i++)
    {
        const char* expr = tests[i];

        printf("\nInput : %s\n", expr);

        Node* root = parseExpression(expr, vars);

        if (!root)
        {
            printf("ERROR : %s\n",
                getLastParseError().c_str());
            continue;
        }

        printf("AST   : %s\n",
            root->toString().c_str());

        double result = root->eval();
        //double result = 123.0;
        printf("Value : %g\n", result);

        delete root;
    }

    printf("\n========================================\n");
    printf("Invalid Expression Tests\n");
    printf("========================================\n");

    const char* badTests[] =
    {
        "",
        "x+",
        "*x",
        "sin(",
        "sin()",
        "(x+y",
        "x+y)",
        "foo(x)",
        "unknownvar",
        "x++y",
        0
    };

    for (int i = 0; badTests[i] != 0; i++)
    {
        const char* expr = badTests[i];

        printf("\nInput : %s\n", expr);

        Node* root = parseExpression(expr, vars);

        if (root)
        {
            printf("UNEXPECTED SUCCESS\n");
            printf("AST : %s\n",
                root->toString().c_str());

            delete root;
        }
        else
        {
            printf("ERROR : %s\n",
                getLastParseError().c_str());
        }
    }

    return 0;
}
#endif // end of main 
#endif
