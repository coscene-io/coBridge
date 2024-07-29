//////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////

#ifndef cobridge_PARAMETER_HPP
#define cobridge_PARAMETER_HPP

#include <any>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace cobridge_base {
    enum class ParameterSubscriptionOperation {
        SUBSCRIBE,
        UNSUBSCRIBE,
    };

    enum class ParameterType {
        PARAMETER_NOT_SET,
        PARAMETER_BOOL,
        PARAMETER_INTEGER,
        PARAMETER_DOUBLE,
        PARAMETER_STRING,
        PARAMETER_ARRAY,
        PARAMETER_STRUCT,      // ROS 1 only
        PARAMETER_BYTE_ARRAY,  // ROS 2 only
    };

    class ParameterValue {
    public:
        ParameterValue();

        ParameterValue(bool value);

        ParameterValue(int value);

        ParameterValue(int64_t value);

        ParameterValue(double value);

        ParameterValue(const std::string &value);

        ParameterValue(const char *value);

        ParameterValue(const std::vector<unsigned char> &value);

        ParameterValue(const std::vector <ParameterValue> &value);

        ParameterValue(const std::unordered_map <std::string, ParameterValue> &value);

        [[nodiscard]] inline ParameterType getType() const {
            return _type;
        }

        template<typename T>
        inline const T &getValue() const {
            return std::any_cast<const T &>(_value);
        }

    private:
        ParameterType _type;
        std::any _value;
    };

    class Parameter {
    public:
        Parameter();

        Parameter(const std::string &name);

        Parameter(const std::string &name, const ParameterValue &value);

        [[nodiscard]] inline const std::string &get_name() const {
            return _name;
        }

        [[nodiscard]] inline ParameterType get_type() const {
            return _value.getType();
        }

        [[nodiscard]] inline const ParameterValue &get_value() const {
            return _value;
        }

    private:
        std::string _name;
        ParameterValue _value;
    };
}

#endif //cobridge_PARAMETER_HPP
