#pragma once

#include <string>

struct VarInfo
{
  std::string name;
  double value;

  VarInfo()
    : name(""), value(0.0)
  {
  }

  VarInfo(const std::string& vname, double vvalue)
    : name(vname), value(vvalue)
  {
  }
};

class VarTable
{
private:
  static const int MAX_VARS = 10;

  VarInfo vars[MAX_VARS];
  int count;

public:
  VarTable()
    : count(1)
  {
    vars[0] = VarInfo("x", 0.0);
  }

  void clear()
  {
    count = 0;
  }

  void resetToDefault()
  {
    count = 1;
    vars[0] = VarInfo("x", 0.0);
  }

  bool addVar(const std::string& name, double value = 0.0)
  {
    if (count >= MAX_VARS)
      return false;

    vars[count++] = VarInfo(name, value);
    return true;
  }

  bool setVarsFromColonList(const std::string& spec)
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

  int getCount() const
  {
    return count;
  }

  const std::string& getName(int index) const
  {
    static const std::string empty = "";

    if (index < 0 || index >= count)
      return empty;

    return vars[index].name;
  }

  double getValue(int index) const
  {
    if (index < 0 || index >= count)
      return 0.0;

    return vars[index].value;
  }

  void setValue(int index, double value)
  {
    if (index < 0 || index >= count)
      return;

    vars[index].value = value;
  }
};

