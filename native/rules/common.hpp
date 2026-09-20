#pragma once
#include "../vendor/json.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace bg {
using J = nlohmann::ordered_json;
inline std::filesystem::path utf8Path(const std::string &text) {
  return std::filesystem::path(
      std::u8string(reinterpret_cast<const char8_t *>(text.data()), text.size()));
}
inline std::string pathText(const std::filesystem::path &path) {
  auto text = path.u8string();
  return std::string(reinterpret_cast<const char *>(text.data()), text.size());
}
inline J arr() {
  return J::array();
}
inline int n(const J &j, const std::string &k, int fallback = 0) {
  return j.is_object() && j.contains(k) && j[k].is_number() ? j[k].get<int>() : fallback;
}
inline std::string s(const J &j, const std::string &k, const std::string &fallback = "") {
  return j.is_object() && j.contains(k) && j[k].is_string() ? j[k].get<std::string>() : fallback;
}
inline bool b(const J &j, const std::string &k) {
  return j.is_object() && j.contains(k) && j[k].is_boolean() && j[k].get<bool>();
}
inline void add(J &j, const std::string &k, int delta) {
  j[k] = n(j, k) + delta;
}
inline void require(bool condition, const std::string &error) {
  if (!condition)
    throw std::runtime_error(error);
}
inline J readJson(const std::filesystem::path &file) {
  std::ifstream in(file, std::ios::binary);
  require(bool(in), "Cannot read " + file.string());
  return J::parse(in);
}
inline std::string timestamp() {
  auto now = std::chrono::system_clock::now();
  auto t = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
#ifdef _WIN32
  gmtime_s(&utc, &t);
#else
  gmtime_r(&t, &utc);
#endif
  std::ostringstream out;
  out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(3) << std::setfill('0')
      << (std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() %
          1000)
      << 'Z';
  return out.str();
}
inline double random(J &g) {
  uint32_t x = g.at("rng").get<uint32_t>();
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  g["rng"] = x;
  return x / 4294967296.0;
}
template <class T> inline T pick(J &g, const std::vector<T> &a) {
  require(!a.empty(), "No eligible choices");
  return a[static_cast<size_t>(std::floor(random(g) * a.size()))];
}
template <class T> inline std::vector<T> shuffle(J &g, std::vector<T> a) {
  for (int i = int(a.size()) - 1; i > 0; i--) {
    int j = int(std::floor(random(g) * (i + 1)));
    std::swap(a[i], a[j]);
  }
  return a;
}
template <class T, class F> inline std::vector<T> filter(const std::vector<T> &a, F f) {
  std::vector<T> out;
  for (const auto &v : a)
    if (f(v))
      out.push_back(v);
  return out;
}
template <class T> inline bool contains(const std::vector<T> &a, const T &value) {
  return std::find(a.begin(), a.end(), value) != a.end();
}
template <class T> inline int indexOf(const std::vector<T> &a, const T &value) {
  auto i = std::find(a.begin(), a.end(), value);
  return i == a.end() ? -1 : int(i - a.begin());
}
using U = std::shared_ptr<J>;
using Units = std::vector<U>;
inline int n(const U &u, const std::string &k, int d = 0) {
  return n(*u, k, d);
}
inline std::string s(const U &u, const std::string &k, const std::string &d = "") {
  return s(*u, k, d);
}
inline bool b(const U &u, const std::string &k) {
  return bg::b(*u, k);
}
inline void add(const U &u, const std::string &k, int d) {
  add(*u, k, d);
}
inline Units units(const J &j) {
  Units out;
  for (const auto &u : j)
    out.push_back(std::make_shared<J>(u));
  return out;
}
inline J serialize(const Units &v) {
  J out = arr();
  for (const auto &u : v)
    out.push_back(*u);
  return out;
}
inline U copy(const U &u) {
  return std::make_shared<J>(*u);
}
inline bool has(const U &u, const std::string &k) {
  for (const auto &v : (*u)["keywords"])
    if (v == k)
      return true;
  return false;
}
inline void keyword(const U &u, const std::string &k) {
  if (!has(u, k))
    (*u)["keywords"].push_back(k);
}
inline void removeKeyword(const U &u, const std::string &k) {
  auto &v = (*u)["keywords"];
  v.erase(std::remove(v.begin(), v.end(), J(k)), v.end());
}
inline int factor(const U &u) {
  return b(u, "golden") ? 2 : 1;
}
inline bool tribe(const U &u, const std::string &t) {
  return s(u, "tribe") == t || s(u, "tribe") == "ALL";
}
inline bool tribe(const J &u, const std::string &t) {
  return s(u, "tribe") == t || s(u, "tribe") == "ALL";
}
inline bool effect(const Units &a, const std::string &name) {
  return std::any_of(a.begin(), a.end(), [&](const U &u) { return s(u, "effect") == name; });
}
inline U findUid(const Units &a, int uid) {
  for (const auto &u : a)
    if (n(u, "uid") == uid)
      return u;
  return nullptr;
}
inline Units joined(Units a, const Units &other) {
  a.insert(a.end(), other.begin(), other.end());
  return a;
}
struct Player {
  J data;
  Units board, hand, shop;
  explicit Player(const J &p)
      : data(p), board(units(p.at("board"))), hand(units(p.at("hand"))), shop(units(p.at("shop"))) {
  }
  J save() const {
    J out = data;
    out["board"] = serialize(board);
    out["hand"] = serialize(hand);
    out["shop"] = serialize(shop);
    return out;
  }
  std::string hero() const { return s(data["hero"], "key"); }
};
struct Content {
  J cards, heroes, tokenArt;
  explicit Content(const std::filesystem::path &directory)
      : cards(readJson(directory / "cards.json")), heroes(readJson(directory / "heroes.json")),
        tokenArt(readJson(directory / "token-art.json")) {}
};
inline const J *card(const J &g, const std::string &id) {
  for (const auto &c : g.at("cards"))
    if (s(c, "id") == id)
      return &c;
  return nullptr;
}
inline void emit(J &g, const std::string &kind, const std::string &text) {
  auto &log = g["log"];
  int seq = log.empty() ? 1 : n(log.back(), "seq") + 1;
  log.push_back({{"seq", seq}, {"round", n(g, "round")}, {"kind", kind}, {"text", text}});
  if (log.size() > 1500)
    log.erase(log.begin(), log.begin() + 500);
}
inline std::string contentHash(const J &cards) {
  uint32_t h = 2166136261u;
  auto text = cards.dump();
  for (size_t i = 0; i < text.size();) {
    uint32_t c = static_cast<unsigned char>(text[i++]);
    if (c >= 0xC0) {
      int remaining = c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
      c &= remaining == 1 ? 31 : remaining == 2 ? 15 : 7;
      while (remaining-- && i < text.size())
        c = (c << 6) | (static_cast<unsigned char>(text[i++]) & 63);
    }
    if (c > 0xffff)
      c = 0xd800 + ((c - 0x10000) >> 10);
    h = (h ^ c) * 16777619u;
  }
  std::ostringstream out;
  out << std::hex << h;
  return out.str();
}
inline J validateCards(const J &value, const Content &content) {
  require(value.is_array() && !value.empty() && value.size() <= 2000,
          "Expected a card pack with 1-2000 cards");
  std::set<std::string> ids, effects = {""};
  for (const auto &c : content.cards)
    effects.insert(s(c, "effect"));
  std::set<std::string> keywords = {"TAUNT",    "DIVINE_SHIELD", "POISONOUS",
                                    "WINDFURY", "REBORN",        "MAGNETIC"},
                        tribes = {"NONE",  "BEAST", "MECHANICAL", "MURLOC",
                                  "DEMON", "ALL",   "DRAGON"};
  for (const auto &c : value) {
    require(c.is_object() && c.contains("id") && c["id"].is_string() &&
                ids.insert(s(c, "id")).second,
            "Invalid or duplicate card ID");
    require(!s(c, "name").empty() && effects.contains(s(c, "effect")) && c.contains("effect"),
            "Unknown effect or missing name");
    for (const auto &field :
         std::vector<std::tuple<std::string, int, int>>{{"attack", 0, 999},
                                                        {"health", 1, 999},
                                                        {"tier", 1, 6},
                                                        {"pool", 1, 100},
                                                        {"buffAttack", 0, 100},
                                                        {"buffHealth", 0, 100}}) {
      auto [key, lo, hi] = field;
      require(c.contains(key) && c[key].is_number_integer() && n(c, key) >= lo && n(c, key) <= hi,
              "Invalid " + key + " on " + s(c, "name"));
    }
    require(c.contains("keywords") && c["keywords"].is_array(), "Invalid keyword");
    for (const auto &k : c["keywords"])
      require(k.is_string() && keywords.contains(k.get<std::string>()), "Invalid keyword");
    require(std::regex_match(s(c, "art"), std::regex("art/[a-zA-Z0-9_-]+\\.(webp|png)")),
            "Invalid local artwork path");
    require(tribes.contains(s(c, "tribe")), "Invalid tribe");
    require(c.contains("enabled") && c["enabled"].is_boolean() && c.contains("text") &&
                c["text"].is_string(),
            "Invalid card fields");
  }
  for (int tier = 1; tier <= 6; tier++) {
    bool found = false;
    for (const auto &c : value)
      if (b(c, "enabled") && n(c, "tier") == tier)
        found = true;
    require(found, "Enable at least one card in tier " + std::to_string(tier));
  }
  return value;
}
inline J parseGame(const J &g, const Content &content) {
  require(g.is_object() && n(g, "version") == 1 &&
              (s(g, "phase") == "recruit" || s(g, "phase") == "combat" ||
               s(g, "phase") == "finished") &&
              g.contains("players") && g["players"].is_array() && g["players"].size() == 8 &&
              g.contains("history") && g["history"].is_array() && g.contains("log") &&
              g["log"].is_array() && g.contains("rng") && g["rng"].is_number_integer() &&
              n(g, "round") >= 1 && n(g, "round") <= 1000,
          "Unsupported or damaged save file");
  validateCards(g.at("cards"), content);
  require(contentHash(g["cards"]) == s(g, "contentHash"),
          "Content checksum does not match the save");
  for (const auto &p : g["players"]) {
    require(p.contains("hero") && p["hero"].is_object() && p.contains("health") &&
                p["health"].is_number() && n(p, "tier") >= 1 && n(p, "tier") <= 6,
            "Invalid player state");
    for (const auto *zone : {"board", "hand", "shop", "discovers"})
      require(p.contains(zone) && p[zone].is_array(), "Invalid player state");
    require(p["board"].size() <= 7 && p["hand"].size() <= 10, "Invalid player state");
    for (const auto *zone : {"board", "hand", "shop"})
      for (const auto &u : p[zone])
        require(u.is_object() && u.contains("attack") && u["attack"].is_number() &&
                    u.contains("health") && u["health"].is_number() && u.contains("keywords") &&
                    u["keywords"].is_array() && u.contains("uid") && u["uid"].is_number_integer(),
                "Invalid minion state");
  }
  return g;
}
} // namespace bg
