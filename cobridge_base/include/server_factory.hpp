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

#ifndef COBRIDGE_SERVER_FACTORY_HPP
#define COBRIDGE_SERVER_FACTORY_HPP

#include <memory>
#include <string>
#include <websocketpp/common/connection_hdl.hpp>

#include "common.hpp"
#include "server_interface.hpp"

namespace cobridge {

    class ServerFactory {
    public:
        template<typename ConnectionHandle>
        static std::unique_ptr <ServerInterface<ConnectionHandle>> create_server(
                const std::string &name, const std::function<void(WebSocketLogLevel, char const *)> &log_handler,
                const ServerOptions &options);
    };
}

#endif //COBRIDGE_SERVER_FACTORY_HPP
