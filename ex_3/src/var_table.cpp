#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <set>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <cmath>

#include "../include/var_table.h"

static void ensureDefaultVariables()
{
  if (varTable.getCount() == 0)
  {
    varTable.addVar("x", 0.0);
  }
}

static std::vector<std::string> makeParserVariableNames()
{
  std::vector<std::string> vars;

  static const char* defaultNames[] =
  {
    "x", "y", "z", "t", "u", "v", "w"
  };

  int count = varTable.getCount();

  for (int i = 0; i < count; i++)
  {
    if (i < 7)
      vars.push_back(defaultNames[i]);
    else
    {
      char buf[32];
      snprintf(buf, sizeof(buf), "x%d", i);
      vars.push_back(buf);
    }
  }

  return vars;
}
VarInfo::VarInfo()

  : name(""), value(0.0)

{

}

VarInfo::VarInfo(const std::string& vname, double vvalue)

  : name(vname), value(vvalue)

{

}

VarTable::VarTable()

  : count(1)

{

  vars[0] = VarInfo("x", 0.0);

}

void VarTable::clear()

{

  count = 0;

}

void VarTable::resetToDefault()

{

  count = 1;

  vars[0] = VarInfo("x", 0.0);

}

bool VarTable::addVar(const std::string& name, double value)

{

  if (count >= MAX_VARS)

    return false;

  vars[count++] = VarInfo(name, value);

  return true;

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

int VarTable::getCount() const

{

  return count;

}

const std::string& VarTable::getName(int index) const

{

  static const std::string empty = "";

  if (index < 0 || index >= count)

    return empty;

  return vars[index].name;

}

double VarTable::getValue(int index) const

{

  if (index < 0 || index >= count)

    return 0.0;

  return vars[index].value;

}

void VarTable::setValue(int index, double value)

{

  if (index < 0 || index >= count)

    return;

  vars[index].value = value;

}

VarTable varTable;

void ensureDefaultVariables()

{

  if (varTable.getCount() == 0)

  {

    varTable.addVar("x", 0.0);

  }

}

std::vector<std::string> makeParserVariableNames()

{

  std::vector<std::string> vars;

  static const char* defaultNames[] =

  {

    "x", "y", "z", "t", "u", "v", "w"

  };

  int count = varTable.getCount();

  for (int i = 0; i < count; i++)

  {

    if (i < 7)

      vars.push_back(defaultNames[i]);

    else

    {

      char buf[32];

      snprintf(buf, sizeof(buf), "x%d", i);

      vars.push_back(buf);

    }

  }

  return vars;

}