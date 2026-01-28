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

#include "http_server.h"

#include <json.hpp>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cstring>

namespace http_server
{

bool get_dev_mac_addr(std::string& mac_addresses)
{
  mac_addresses.clear();
  std::vector<std::string> mac_addresses_vec;
  ifaddrs* ifaddr;
  
  if (getifaddrs(&ifaddr) == -1) {
    return false;
  }
  
  for (const ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == nullptr) {
      continue;
    }
    
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
      continue;
    }
    
    ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, ifa->ifa_name);
    
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) != -1) {
      const unsigned char* mac = reinterpret_cast<unsigned char*>(ifr.ifr_hwaddr.sa_data);
      
      bool is_zero = true;
      for (int i = 0; i < 6; i++) {
        if (mac[i] != 0) {
          is_zero = false;
          break;
        }
      }
      
      if (!is_zero) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (int i = 0; i < 6; i++) {
          ss << std::setw(2) << static_cast<int>(mac[i]);
          if (i < 5) {
            ss << ":";
          }
        }
        mac_addresses_vec.push_back(ss.str());
      }
    }
    
    close(fd);
  }
  
  freeifaddrs(ifaddr);
  
  std::sort(mac_addresses_vec.begin(), mac_addresses_vec.end());
  
  mac_addresses_vec.erase(
    std::unique(mac_addresses_vec.begin(), mac_addresses_vec.end()),
    mac_addresses_vec.end());
  
  for (size_t i = 0; i < mac_addresses_vec.size(); i++) {
    if (i > 0) {
      mac_addresses += ",";
    }
    mac_addresses += mac_addresses_vec[i];
  }
  
  return !mac_addresses.empty();
}

bool get_dev_ip_addrs(std::vector<std::string>& ip_addresses, std::string& colink_ip)
{
  ip_addresses.clear();
  colink_ip.clear();
  
  ifaddrs* ifaddr;
  
  if (getifaddrs(&ifaddr) == -1) {
    return false;
  }
  
  for (const ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == nullptr) {
      continue;
    }
    
    if (ifa->ifa_addr->sa_family == AF_INET) {
      const sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
      char ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
      std::string ip_str(ip);
      
      if (ip_str != "127.0.0.1") {
        if (std::string(ifa->ifa_name) == "colink") {
          // 特殊处理colink的IP - 将最后一个八位字节改为1
          size_t last_dot = ip_str.find_last_of('.');
          if (last_dot != std::string::npos) {
            colink_ip = ip_str.substr(0, last_dot + 1) + "1";
          }
        } else {
          ip_addresses.push_back(ip_str);
        }
      }
    }
  }
  
  freeifaddrs(ifaddr);
  
  std::sort(ip_addresses.begin(), ip_addresses.end());
  
  ip_addresses.erase(
    std::unique(ip_addresses.begin(), ip_addresses.end()),
    ip_addresses.end());
  
  return !ip_addresses.empty();
}

HttpServer::HttpServer(int port, 
                       const std::string & mac_addresses, 
                       const std::vector<std::string> & ip_addresses,
                       LogHandler log_handler)
: _port(port), 
  _mac_addresses(mac_addresses), 
  _ip_addresses(ip_addresses), 
  _running(false),
  _log_handler(log_handler)
{
  log(LogLevel::Info, "HTTP Server created on port " + std::to_string(port));
}

HttpServer::~HttpServer()
{
  stop();
  log(LogLevel::Info, "HTTP Server destroyed");
}

void HttpServer::log(LogLevel level, const char* message)
{
  if (_log_handler) {
    _log_handler(level, message);
  }
}

void HttpServer::log(LogLevel level, const std::string& message)
{
  log(level, message.c_str());
}

void HttpServer::start()
{
  if (!_running) {
    _running = true;
    _server_thread = std::thread(&HttpServer::run_server, this);
    log(LogLevel::Info, "HTTP Server thread started");
  } else {
    log(LogLevel::Warn, "HTTP Server already running");
  }
}

void HttpServer::stop()
{
  if (_running) {
    _running = false;
    if (_server_thread.joinable()) {
      _server_thread.join();
    }
    log(LogLevel::Info, "HTTP Server stopped");
  }
}

void HttpServer::run_server()
{
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    log(LogLevel::Error, std::string("HTTP server socket creation failed: ") + std::strerror(errno));
    return;
  }
  
  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    log(LogLevel::Error, std::string("HTTP server setsockopt failed: ") + std::strerror(errno));
    close(server_fd);
    return;
  }
  
  sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(_port);
  
  if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
    log(LogLevel::Error, std::string("HTTP server bind failed: ") + std::strerror(errno));
    close(server_fd);
    return;
  }
  
  if (listen(server_fd, 10) < 0) {
    log(LogLevel::Error, std::string("HTTP server listen failed: ") + std::strerror(errno));
    close(server_fd);
    return;
  }
  
  log(LogLevel::Info, "HTTP server listening on port " + std::to_string(_port));
  
  nlohmann::json response_json;
  response_json["macAddr"] = _mac_addresses;
  std::string json_str = response_json.dump();
  
  std::string http_response = 
    "HTTP/1.1 200 OK\n"
    "Content-Type: application/json\n"
    "Access-Control-Allow-Origin: *\n"
    "Connection: close\n"
    "Content-Length: " + std::to_string(json_str.length()) + "\n"
    "\n" +
    json_str;
  
  std::string not_found_response = 
    "HTTP/1.1 404 Not Found\n"
    "Content-Type: text/plain\n"
    "Connection: close\n"
    "Content-Length: 9\n"
    "\n"
    "Not Found";
  
  while (_running) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(server_fd, &read_fds);
    
    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    int activity = select(server_fd + 1, &read_fds, NULL, NULL, &timeout);
    
    if (activity < 0) {
      if (errno != EINTR) {
        log(LogLevel::Error, std::string("HTTP server select error: ") + std::strerror(errno));
      }
      continue;
    }
    
    if (activity == 0 || !_running) {
      continue;
    }
    
    if (FD_ISSET(server_fd, &read_fds)) {
      int new_socket;
      struct sockaddr_in client_addr;
      memset(&client_addr, 0, sizeof(client_addr));
      socklen_t addrlen = sizeof(client_addr);
      
      if ((new_socket = accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &addrlen)) < 0) {
        log(LogLevel::Error, std::string("HTTP server accept failed: ") + std::strerror(errno));
        continue;
      }
      
      char client_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
      log(LogLevel::Debug, std::string("New connection from ") + client_ip);
      
      struct timeval recv_timeout;
      memset(&recv_timeout, 0, sizeof(recv_timeout));
      recv_timeout.tv_sec = 5;
      recv_timeout.tv_usec = 0;
      setsockopt(new_socket, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));
      
      char buffer[4096] = {0};
      int valread = read(new_socket, buffer, sizeof(buffer) - 1);
      
      if (valread > 0) {
        std::string request(buffer);
        size_t path_start = request.find("GET ");
        if (path_start != std::string::npos) {
          path_start += 4; // skip "GET "
          
          size_t path_end = request.find(" HTTP/", path_start);
          if (path_end != std::string::npos && path_start < path_end) {
            std::string path = request.substr(path_start, path_end - path_start);
            
            log(LogLevel::Debug, "Received request for path: " + path);
            
            if (path == "/device-info") {
              send(new_socket, http_response.c_str(), http_response.length(), 0);
              log(LogLevel::Info, "Sent device info response: " + json_str);
            } else {
              send(new_socket, not_found_response.c_str(), not_found_response.length(), 0);
              log(LogLevel::Warn, "Path not found: " + path);
            }
          } else {
            send(new_socket, not_found_response.c_str(), not_found_response.length(), 0);
            log(LogLevel::Warn, "Invalid HTTP request format");
          }
        } else {
          send(new_socket, not_found_response.c_str(), not_found_response.length(), 0);
          log(LogLevel::Warn, "Not a GET request");
        }
      }
      
      close(new_socket);
    }
  }
  
  close(server_fd);
  log(LogLevel::Info, "HTTP server socket closed");
}

}  // namespace http_server
