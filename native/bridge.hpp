#pragma once
#include "platform.hpp"
#include "rules/host.hpp"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
using json = nlohmann::json;

// Serial worker keeps simulation and disk IO off raylib's render thread.
// Both sides are native C++; JSON remains the editable data/snapshot format.
class RulesBridge {
  std::unique_ptr<bg::Host> host;
  std::thread worker;
  std::mutex mutex;
  std::condition_variable wake;
  std::deque<json> outgoing, incoming;
  std::atomic<bool> alive{false};
  bool stopping = false;
  int sequence = 0;

public:
  bool start(const std::filesystem::path &directory) {
    try {
      host = std::make_unique<bg::Host>(directory / "data");
      alive = true;
      worker = std::thread([this] {
        for (;;) {
          json message;
          {
            std::unique_lock lock(mutex);
            wake.wait(lock, [this] { return stopping || !outgoing.empty(); });
            if (outgoing.empty() && stopping)
              break;
            message = std::move(outgoing.front());
            outgoing.pop_front();
          }
          auto reply = host->handle(bg::J::parse(message.dump()));
          {
            std::lock_guard lock(mutex);
            incoming.push_back(json::parse(reply.dump()));
          }
        }
        alive = false;
      });
      return true;
    } catch (const std::exception &) {
      alive = false;
      return false;
    }
  }
  bool running() const { return alive; }
  int send(json message) {
    std::lock_guard lock(mutex);
    if (!alive || stopping)
      return -1;
    message["id"] = ++sequence;
    outgoing.push_back(std::move(message));
    wake.notify_one();
    return sequence;
  }
  bool poll(json &message) {
    std::lock_guard lock(mutex);
    if (incoming.empty())
      return false;
    message = std::move(incoming.front());
    incoming.pop_front();
    return true;
  }
  ~RulesBridge() {
    {
      std::lock_guard lock(mutex);
      stopping = true;
    }
    wake.notify_one();
    if (worker.joinable())
      worker.join();
  }
};
