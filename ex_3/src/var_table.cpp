#include <string>
#include <vector>
#include <set>

#include "../include/var_table.h"

extern VarTable varTable;

void ensureDefaultVariables()
{
  if (varTable.getCount() == 0)
  {
    varTable.addVar("x", 0.0);
  }
}

std::vector<std::string> makeParserVariableNames() {
  std::vector<std::string> vars;

  static const char* defaultNames[] =
  {
    "x", "y", "z", "t", "u", "v", "w"
  };

  int count = varTable.getCount();

  for (int i = 0; i < count; i++)
  {
    if (i < 7)
      vars.emplace_back(defaultNames[i]);
    else
    {
      char buf[32];
      snprintf(buf, sizeof(buf), "x%d", i);
      vars.emplace_back(buf);
    }
  }

  return vars;
}

bool VarTable::setVarsFromColonList(const std::string& spec)

{
  clear();

  if (spec.empty())

  {
    resetToDefault();

    return true;
  }

  size_t start = 0;

  while (start < spec.size())

  {
    size_t end = spec.find(':', start);

    std::string name =

      (end == std::string::npos)

        ? spec.substr(start)

        : spec.substr(start, end - start);

    if (!name.empty())

    {
      if (!addVar(name, 0.0))
        return false;
    }

    if (end == std::string::npos)

      break;

    start = end + 1;
  }

  if (count == 0)

    resetToDefault();

  return true;
}