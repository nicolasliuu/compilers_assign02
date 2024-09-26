#include "cpputil.h"
#include "exceptions.h"
#include "valrep.h"
#include "function.h"
#include "value.h"

Value::Value(int ival)
  : m_kind(VALUE_INT) {
  m_atomic.ival = ival;
}

Value::Value(Function *fn)
  : m_kind(VALUE_FUNCTION)
  , m_rep(fn) {
  m_rep = fn;
}

Value::Value(IntrinsicFn intrinsic_fn)
  : m_kind(VALUE_INTRINSIC_FN) {
  m_atomic.intrinsic_fn = intrinsic_fn;
}

Value::Value(const Value &other)
    : m_kind(other.m_kind) {
    if (other.is_dynamic()) {
        m_rep = other.m_rep;
        m_rep->add_ref();
    } else {
        m_atomic = other.m_atomic;
    }
}

Value::~Value() {
    if (is_dynamic()) {
        m_rep->remove_ref();
        if (m_rep->get_num_refs() == 0) {
            delete m_rep;
        }
    }
}

Value &Value::operator=(const Value &rhs) {
    if (this != &rhs) {
        if (is_dynamic()) {
            m_rep->remove_ref();
            if (m_rep->get_num_refs() == 0) {
                delete m_rep;
            }
        }
        m_kind = rhs.m_kind;
        if (rhs.is_dynamic()) {
            m_rep = rhs.m_rep;
            m_rep->add_ref();
        } else {
            m_atomic = rhs.m_atomic;
        }
    }
    return *this;
}

Function *Value::get_function() const {
  assert(m_kind == VALUE_FUNCTION);
  return m_rep->as_function();
}

std::string Value::as_str() const {
  switch (m_kind) {
  case VALUE_INT:
    return cpputil::format("%d", m_atomic.ival);
  case VALUE_FUNCTION:
    return cpputil::format("<function %s>", m_rep->as_function()->get_name().c_str());
  case VALUE_INTRINSIC_FN:
    return "<intrinsic function>";
  default:
    // this should not happen
    RuntimeError::raise("Unknown value type %d", int(m_kind));
  }
}

// TODO: implementations of additional member functions
