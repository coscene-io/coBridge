// Copyright 2024 coScene
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STANDARD_HPP_
#define STANDARD_HPP_

#include <cstring>
#include <string>
#include <typeinfo>
#include <utility>

template<typename T>
struct remove_cv
{
  typedef T type;
};

template<typename T>
struct remove_cv<const T>
{
  typedef T type;
};

template<typename T>
struct remove_cv<volatile T>
{
  typedef T type;
};

template<typename T>
struct remove_cv<const volatile T>
{
  typedef T type;
};

template<typename T>
struct remove_reference
{
  typedef T type;
};

template<typename T>
struct remove_reference<T &>
{
  typedef T type;
};

template<typename T>
struct decay
{
  typedef typename remove_cv<typename remove_reference<T>::type>::type type;
};

class bad_any_cast : public std::bad_cast
{
public:
  virtual const char * what() const throw()
  {
    return "bad any_cast";
  }
};

class any
{
private:
  struct _Storage_base
  {
    virtual ~_Storage_base() = default;
    virtual _Storage_base * _M_clone() const = 0;
    virtual const std::type_info & _M_type() const = 0;
  };

  template<typename _Tp>
  struct _Storage : _Storage_base
  {
    _Tp M_value;

    template<typename _Up>
    explicit _Storage(const _Up & _value)
    : M_value(_value) {}

    virtual _Storage_base * _M_clone() const
    {
      return new _Storage(M_value);
    }

    virtual const std::type_info & _M_type() const
    {
      return typeid(_Tp);
    }
  };

  _Storage_base * M_storage;

public:
  any()
  : M_storage(nullptr) {}

  any(const any & _other)
  : M_storage(_other.M_storage ? _other.M_storage->_M_clone() : nullptr) {}

  template<typename _Tp>
  explicit any(const _Tp & _value)
  : M_storage(new _Storage<typename decay<_Tp>::type>(_value)) {}

  ~any()
  {
    delete M_storage;
  }

  any & operator=(const any & _rhs)
  {
    if (this != &_rhs) {
      delete M_storage;
      M_storage = _rhs.M_storage ? _rhs.M_storage->_M_clone() : nullptr;
    }
    return *this;
  }

  template<typename Tp>
  any & operator=(const Tp & _rhs)
  {
    delete M_storage;
    M_storage = new _Storage<typename decay<Tp>::type>(_rhs);
    return *this;
  }

  bool has_value() const
  {
    return M_storage != nullptr;
  }

  const std::type_info & type() const
  {
    return M_storage ? M_storage->_M_type() : typeid(void);
  }

  void reset()
  {
    delete M_storage;
    M_storage = nullptr;
  }

  void swap(any & _other)
  {
    _Storage_base * _tmp = M_storage;
    M_storage = _other.M_storage;
    _other.M_storage = _tmp;
  }

  template<typename _Tp>
  friend _Tp * any_cast(any * _operand);

  template<typename _Tp>
  friend const _Tp * any_cast(const any * _operand);
};

template<typename Tp>
Tp * any_cast(any * _operand)
{
  if (_operand && _operand->type() == typeid(Tp)) {
    typedef any::_Storage<Tp> _Storage_type;
    _Storage_type * _storage = static_cast<_Storage_type *>(_operand->M_storage);
    return &_storage->M_value;
  }
  return nullptr;
}

template<typename Tp>
const Tp * any_cast(const any * _operand)
{
  return any_cast<Tp>(const_cast<any *>(_operand));
}

template<typename Tp>
Tp any_cast(const any & _operand)
{
  typedef typename remove_reference<Tp>::type Up;
  const Up * _result = any_cast<Up>(&_operand);
  if (!_result) {
    throw bad_any_cast();
  }
  return static_cast<Tp>(*_result);
}

template<typename Tp>
Tp any_cast(any & _operand)
{
  typedef typename remove_reference<Tp>::type _Up;
  _Up * _result = any_cast<_Up>(&_operand);
  if (!_result) {
    throw bad_any_cast();
  }
  return static_cast<Tp>(*_result);
}

class string_view
{
private:
  const char * data_;
  std::size_t size_;

public:
  constexpr string_view()
  : data_(nullptr), size_(0) {}
  constexpr string_view(const char * str)
  : data_(str), size_(str ? strlen_constexpr(str) : 0) {}
  explicit string_view(const std::string & str)
  : data_(str.data()), size_(str.size()) {}
  constexpr string_view(const char * data, std::size_t size)
  : data_(data), size_(size) {}

  constexpr const char * data() const {return data_;}
  constexpr std::size_t size() const {return size_;}
  constexpr std::size_t length() const {return size_;}
  constexpr bool empty() const {return size_ == 0;}

  constexpr const char * begin() const {return data_;}
  constexpr const char * end() const {return data_ + size_;}

  constexpr char operator[](std::size_t pos) const {return data_[pos];}

  constexpr string_view substr(std::size_t pos = 0, std::size_t len = std::string::npos) const
  {
    return string_view(
      data_ + (pos > size_ ? size_ : pos),
      len > size_ - (pos > size_ ? size_ : pos) ? size_ - (pos > size_ ? size_ : pos) : len
    );
  }

  std::string to_string() const {return std::string(data_, size_);}

private:
  static constexpr std::size_t strlen_constexpr(const char * str)
  {
    return *str ? 1 + strlen_constexpr(str + 1) : 0;
  }
};


struct nullopt_t
{
  explicit constexpr nullopt_t(int) {}
};
constexpr nullopt_t nullopt{0};

template<typename T>
class optional
{
private:
  bool has_value_;
  T value_;

public:
  optional()
  : has_value_(false) {}
  explicit optional(const T & value)
  : has_value_(true), value_(value) {}
  explicit optional(nullopt_t)
  : has_value_(false) {}

  optional & operator=(nullopt_t)
  {
    has_value_ = false;
    return *this;
  }

  optional & operator=(const T & value)
  {
    has_value_ = true;
    value_ = value;
    return *this;
  }

  bool has_value() const {return has_value_;}
  const T & value() const {return value_;}
  T & value() {return value_;}

  bool operator==(const optional<T> & other) const
  {
    if (has_value_ != other.has_value_) {return false;}
    if (!has_value_) {return true;}
    return value_ == other.value_;
  }
};

#endif  // STANDARD_HPP_
