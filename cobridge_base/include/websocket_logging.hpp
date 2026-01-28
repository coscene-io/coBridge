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
#ifndef WEBSOCKET_LOGGING_HPP_
#define WEBSOCKET_LOGGING_HPP_

#include <websocketpp/common/asio.hpp>
#include <websocketpp/logger/levels.hpp>

#include <string>
#include <functional>
#include "common.hpp"

namespace cobridge_base
{

using LogCallback = std::function<void (WebSocketLogLevel, char const *)>;

inline std::string ip_address_to_string(const websocketpp::lib::asio::ip::address & addr)
{
  if (addr.is_v6()) {
    return "[" + addr.to_string() + "]";
  }
  return addr.to_string();
}

inline void no_op_log_callback(WebSocketLogLevel, char const *)
{}

class CallbackLogger
{
public:
  using channel_type_hint = websocketpp::log::channel_type_hint;

  explicit CallbackLogger(channel_type_hint::value hint = channel_type_hint::access)
  : _static_channels(0xffffffff), _dynamic_channels(0), _channel_type_hint(hint),
    _callback(no_op_log_callback)
  {}

  explicit CallbackLogger(
    websocketpp::log::level channels,
    channel_type_hint::value hint = channel_type_hint::access)
  : _static_channels(channels), _dynamic_channels(0), _channel_type_hint(hint),
    _callback(no_op_log_callback)
  {}


  void set_callback(LogCallback callback)
  {
    _callback = callback;
  }

  void set_channels(websocketpp::log::level channels)
  {
    if (channels == 0) {
      clear_channels(0xffffffff);
      return;
    }

    _dynamic_channels |= (channels & _static_channels);
  }

  void clear_channels(websocketpp::log::level channels)
  {
    _dynamic_channels &= ~channels;
  }

  void write(websocketpp::log::level channel, std::string const & msg)
  {
    write(channel, msg.c_str());
  }

  void write(websocketpp::log::level channel, char const * msg)
  {
    if (!this->dynamic_test(channel)) {
      return;
    }

    if (_channel_type_hint == channel_type_hint::access) {
      _callback(WebSocketLogLevel::Info, msg);
    } else {
      if (channel == websocketpp::log::elevel::devel) {
        _callback(WebSocketLogLevel::Debug, msg);
      } else if (channel == websocketpp::log::elevel::library) {
        _callback(WebSocketLogLevel::Debug, msg);
      } else if (channel == websocketpp::log::elevel::info) {
        _callback(WebSocketLogLevel::Info, msg);
      } else if (channel == websocketpp::log::elevel::warn) {
        _callback(WebSocketLogLevel::Warn, msg);
      } else if (channel == websocketpp::log::elevel::rerror) {
        _callback(WebSocketLogLevel::Error, msg);
      } else if (channel == websocketpp::log::elevel::fatal) {
        _callback(WebSocketLogLevel::Critical, msg);
      }
    }
  }

  bool static_test(websocketpp::log::level channel) const
  {
    return (channel & _static_channels) != 0;
  }

  bool dynamic_test(websocketpp::log::level channel)
  {
    return (channel & _dynamic_channels) != 0;
  }

private:
  websocketpp::log::level const _static_channels;
  websocketpp::log::level _dynamic_channels;
  channel_type_hint::value _channel_type_hint;
  LogCallback _callback;
};
}  // namespace cobridge_base

#endif  // WEBSOCKET_LOGGING_HPP_
