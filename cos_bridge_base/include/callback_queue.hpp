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

#ifndef COS_BRIDGE_CALLBACK_QUEUE_HPP
#define COS_BRIDGE_CALLBACK_QUEUE_HPP

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "websocket_logging.hpp"

namespace cos_bridge_base {

    class CallbackQueue {
    public:
        explicit CallbackQueue(LogCallback log_callback, size_t num_threads = 1)
                : _log_callback(std::move(log_callback)), _quit(false) {
            for (size_t i = 0; i < num_threads; ++i) {
                _worker_threads.emplace_back(&CallbackQueue::do_work, this);
            }
        }

        ~CallbackQueue() {
            stop();
        }

        void stop() {
            _quit = true;
            _cv.notify_all();
            for (auto &thread: _worker_threads) {
                thread.join();
            }
        }

        void add_callback(std::function<void(void)> cb) {
            if (_quit) {
                return;
            }
            std::unique_lock <std::mutex> lock(_mutex);
            _callback_queue.push_back(cb);
            _cv.notify_one();
        }


    private:
        void do_work() {
            while (!_quit) {
                std::unique_lock <std::mutex> lock(_mutex);
                _cv.wait(lock, [this] {
                    return (_quit || !_callback_queue.empty());
                });
                if (_quit) {
                    break;
                } else if (!_callback_queue.empty()) {
                    std::function<void(void)> cb = _callback_queue.front();
                    _callback_queue.pop_front();
                    lock.unlock();
                    try {
                        cb();
                    } catch (const std::exception &ex) {
                        // Should never get here if we catch all exceptions in the callbacks.
                        const std::string msg =
                                std::string("Caught unhandled exception in callback_queue") + ex.what();
                        _log_callback(WebSocketLogLevel::Error, msg.c_str());
                    } catch (...) {
                        _log_callback(WebSocketLogLevel::Error, "Caught unhandled exception in calback_queue");
                    }
                }
            }
        }

    private:
        LogCallback _log_callback;
        std::atomic<bool> _quit;
        std::mutex _mutex;
        std::condition_variable _cv;
        std::deque <std::function<void(void)>> _callback_queue;
        std::vector <std::thread> _worker_threads;
    };
}
#endif //COS_BRIDGE_CALLBACK_QUEUE_HPP
