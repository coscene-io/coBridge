#pragma once

#include <regex>
#include <string>
#include <vector>

#include <xmlrpcpp/XmlRpc.h>

#include <parameter.hpp>

namespace cobridge {

    cobridge_base::Parameter fromRosParam(const std::string& name, const XmlRpc::XmlRpcValue& value);
    XmlRpc::XmlRpcValue toRosParam(const cobridge_base::ParameterValue& param);
    std::vector<std::regex> parseRegexPatterns(const std::vector<std::string>& strings);

}
