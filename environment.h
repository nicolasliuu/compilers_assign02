#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <cassert>
#include <map>
#include <string>
#include "value.h"

class Environment {
private:
  Environment *m_parent;
  std::map<std::string, Value> variables; // Map of var names to their values

  // copy constructor and assignment operator prohibited
  Environment(const Environment &);
  Environment &operator=(const Environment &);

public:
  Environment(Environment *parent = nullptr);

  ~Environment();

  void define_variable(const std::string& name, const Value& value);

  bool is_defined(const std::string& name) const;

  Value get_variable(const std::string& name) const;

  void set_variable(const std::string& name, const Value& value);
};

#endif // ENVIRONMENT_H
