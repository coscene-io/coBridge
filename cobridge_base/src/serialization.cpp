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

#include <base64.hpp>
#include <serialization.hpp>

#include <string>
#include <vector>
#include <unordered_map>

namespace cobridge_base
{

void to_json(nlohmann::json & json_obj, const Channel & chan)
{
  json_obj = {
    {"id", chan.id},
    {"topic", chan.topic},
    {"encoding", chan.encoding},
    {"schemaName", chan.schema_name},
    {"schema", chan.schema},
  };

  if (chan.schema_encoding.has_value()) {
    json_obj["schemaEncoding"] = chan.schema_encoding.value();
  }
}

void from_json(const nlohmann::json & json_obj, Channel & chan)
{
  const auto schema_encoding =
    json_obj.find("schemaEncoding") == json_obj.end() ?
    optional<std::string>(nullopt) :
    optional<std::string>(json_obj["schemaEncoding"].get<std::string>());

  ChannelWithoutId channel_without_id{
    json_obj["topic"].get<std::string>(),
    json_obj["encoding"].get<std::string>(),
    json_obj["schemaName"].get<std::string>(),
    json_obj["schema"].get<std::string>(),
    schema_encoding
  };
  chan = Channel(json_obj["id"].get<ChannelId>(), channel_without_id);
}


void to_json(nlohmann::json & json_obj, const ParameterValue & param_val)
{
  const auto paramType = param_val.getType();
  if (paramType == ParameterType::PARAMETER_BOOL) {
    json_obj = param_val.getValue<bool>();
  } else if (paramType == ParameterType::PARAMETER_INTEGER) {
    json_obj = param_val.getValue<int64_t>();
  } else if (paramType == ParameterType::PARAMETER_DOUBLE) {
    json_obj = param_val.getValue<double>();
  } else if (paramType == ParameterType::PARAMETER_STRING) {
    json_obj = param_val.getValue<std::string>();
  } else if (paramType == ParameterType::PARAMETER_BYTE_ARRAY) {
    const auto & paramValue = param_val.getValue<std::vector<unsigned char>>();
    const string_view strValue(reinterpret_cast<const char *>(paramValue.data()),
      paramValue.size());
    json_obj = base64_encode(strValue);
  } else if (paramType == ParameterType::PARAMETER_STRUCT) {
    json_obj = param_val.getValue<std::unordered_map<std::string, ParameterValue>>();
  } else if (paramType == ParameterType::PARAMETER_ARRAY) {
    json_obj = param_val.getValue<std::vector<ParameterValue>>();
  } else if (paramType == ParameterType::PARAMETER_NOT_SET) {
    // empty value.
  }
}

void from_json(const nlohmann::json & json_obj, ParameterValue & param_val)
{
  const auto jsonType = json_obj.type();

  if (jsonType == nlohmann::detail::value_t::string) {
    param_val = ParameterValue(json_obj.get<std::string>());
  } else if (jsonType == nlohmann::detail::value_t::boolean) {
    param_val = ParameterValue(json_obj.get<bool>());
  } else if (jsonType == nlohmann::detail::value_t::number_integer) {
    param_val = ParameterValue(json_obj.get<int64_t>());
  } else if (jsonType == nlohmann::detail::value_t::number_unsigned) {
    param_val = ParameterValue(json_obj.get<int64_t>());
  } else if (jsonType == nlohmann::detail::value_t::number_float) {
    param_val = ParameterValue(json_obj.get<double>());
  } else if (jsonType == nlohmann::detail::value_t::object) {
    param_val = ParameterValue(json_obj.get<std::unordered_map<std::string, ParameterValue>>());
  } else if (jsonType == nlohmann::detail::value_t::array) {
    param_val = ParameterValue(json_obj.get<std::vector<ParameterValue>>());
  }
}

void to_json(nlohmann::json & json_obj, const Parameter & param)
{
  to_json(json_obj["value"], param.get_value());
  json_obj["name"] = param.get_name();
  if (param.get_type() == ParameterType::PARAMETER_BYTE_ARRAY) {
    json_obj["type"] = "byte_array";
  } else if (param.get_type() == ParameterType::PARAMETER_DOUBLE) {
    json_obj["type"] = "float64";
  } else if (param.get_type() == ParameterType::PARAMETER_ARRAY) {
    const auto & vec = param.get_value().getValue<std::vector<ParameterValue>>();
    if (!vec.empty() && vec.front().getType() == ParameterType::PARAMETER_DOUBLE) {
      json_obj["type"] = "float64_array";
    }
  }
}

void from_json(const nlohmann::json & json_obj, Parameter & param)
{
  const auto name = json_obj["name"].get<std::string>();

  if (json_obj.find("value") == json_obj.end()) {
    param = Parameter(name);  // Value is not set (undefined).
    return;
  }

  ParameterValue param_val;
  from_json(json_obj["value"], param_val);
  const auto type_iter = json_obj.find("type");
  const std::string type = type_iter != json_obj.end() ? type_iter->get<std::string>() : "";

  if (param_val.getType() == ParameterType::PARAMETER_STRING && type == "byte_array") {
    param = Parameter(name, ParameterValue(base64_decode(param_val.getValue<std::string>())));
  } else if (param_val.getType() == ParameterType::PARAMETER_INTEGER && type == "float64") {
    param = Parameter(name, ParameterValue(static_cast<double>(param_val.getValue<int64_t>())));
  } else if (param_val.getType() == ParameterType::PARAMETER_ARRAY && type == "float64_array") {
    auto values = param_val.getValue<std::vector<ParameterValue>>();
    for (ParameterValue & value : values) {
      if (value.getType() == ParameterType::PARAMETER_INTEGER) {
        value = ParameterValue(static_cast<double>(value.getValue<int64_t>()));
      } else if (value.getType() != ParameterType::PARAMETER_DOUBLE) {
        throw std::runtime_error(
                "Parameter '" + name +
                "' (float64_array) contains non-numeric elements.");
      }
    }
    param = Parameter(name, ParameterValue(values));
  } else {
    param = Parameter(name, param_val);
  }
}

void to_json(nlohmann::json & json_obj, const Service & service)
{
  json_obj = {
    {"id", service.id},
    {"name", service.name},
    {"type", service.type},
    {"requestSchema", service.request_schema},
    {"responseSchema", service.response_schema},
  };
}

void from_json(const nlohmann::json & json_obj, Service & service)
{
  service.id = json_obj["id"].get<ServiceId>();
  service.name = json_obj["name"].get<std::string>();
  service.type = json_obj["type"].get<std::string>();
  service.request_schema = json_obj["requestSchema"].get<std::string>();
  service.response_schema = json_obj["responseSchema"].get<std::string>();
}

void ServiceResponse::read(const uint8_t * data, size_t data_length)
{
  // Validate minimum message size
  if (data_length < 12) {  // 4 + 4 + 4 = service_id + call_id + encoding_length
    throw std::runtime_error(
            "ServiceResponse message too short: " + std::to_string(
              data_length) + " bytes (minimum 12)");
  }

  size_t offset = 0;
  this->service_id = read_uint32_LE(data + offset);
  offset += 4;
  this->call_id = read_uint32_LE(data + offset);
  offset += 4;
  const size_t encoding_length = static_cast<size_t>(read_uint32_LE(data + offset));
  offset += 4;

  // Validate encoding length is reasonable and fits in remaining data
  // Reasonable max for encoding strings like "cdr", "json", etc.
  constexpr size_t MAX_ENCODING_LENGTH = 256;
  if (encoding_length > MAX_ENCODING_LENGTH) {
    throw std::runtime_error(
            "ServiceResponse encoding length too large: " + std::to_string(encoding_length) +
            " bytes (max " + std::to_string(MAX_ENCODING_LENGTH) + ")");
  }

  if (offset + encoding_length > data_length) {
    throw std::runtime_error(
            "ServiceResponse encoding length " + std::to_string(encoding_length) +
            " exceeds remaining data " + std::to_string(data_length - offset) + " bytes");
  }

  this->encoding = std::string(reinterpret_cast<const char *>(data + offset), encoding_length);
  offset += encoding_length;

  // Validate remaining data for payload
  if (offset > data_length) {
    throw std::runtime_error(
            "ServiceResponse offset " + std::to_string(offset) +
            " exceeds data length " + std::to_string(data_length));
  }

  const auto payload_length = data_length - offset;
  this->serv_data.resize(payload_length);
  std::memcpy(this->serv_data.data(), data + offset, payload_length);
}

void ServiceResponse::write(uint8_t * data) const
{
  size_t offset = 0;
  write_uint32_LE(data + offset, this->service_id);
  offset += 4;
  write_uint32_LE(data + offset, this->call_id);
  offset += 4;
  write_uint32_LE(data + offset, static_cast<uint32_t>(this->encoding.size()));
  offset += 4;
  std::memcpy(data + offset, this->encoding.data(), this->encoding.size());
  offset += this->encoding.size();
  std::memcpy(data + offset, this->serv_data.data(), this->serv_data.size());
}
}  // namespace cobridge_base
