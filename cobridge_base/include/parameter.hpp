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

#ifndef PARAMETER_HPP_
#define PARAMETER_HPP_

// #include <any>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "standard.hpp"

namespace cobridge_base
{
enum class ParameterSubscriptionOperation
{
  SUBSCRIBE,
  UNSUBSCRIBE,
};

enum class ParameterType
{
  PARAMETER_NOT_SET,
  PARAMETER_BOOL,
  PARAMETER_INTEGER,
  PARAMETER_DOUBLE,
  PARAMETER_STRING,
  PARAMETER_ARRAY,
  PARAMETER_STRUCT,      // ROS 1 only
  PARAMETER_BYTE_ARRAY,  // ROS 2 only
};

class ParameterValue
{
public:
  ParameterValue();

  explicit ParameterValue(bool value);

  explicit ParameterValue(int value);

  explicit ParameterValue(int64_t value);

  explicit ParameterValue(double value);

  explicit ParameterValue(const std::string & value);

  explicit ParameterValue(const char * value);

  explicit ParameterValue(const std::vector<unsigned char> & value);

  explicit ParameterValue(const std::vector<ParameterValue> & value);

  explicit ParameterValue(const std::unordered_map<std::string, ParameterValue> & value);

  inline ParameterType getType() const
  {
    return _type;
  }

  template<typename T>
  inline const T & getValue() const
  {
    return any_cast<const T &>(_value);
  }

private:
  ParameterType _type;
  any _value;
};

class Parameter
{
public:
  Parameter();

  explicit Parameter(const std::string & name);

  Parameter(const std::string & name, const ParameterValue & value);

  inline const std::string & get_name() const
  {
    return _name;
  }

  inline ParameterType get_type() const
  {
    return _value.getType();
  }

  inline const ParameterValue & get_value() const
  {
    return _value;
  }

private:
  std::string _name;
  ParameterValue _value;
};
}  // namespace cobridge_base

#endif  // PARAMETER_HPP_
