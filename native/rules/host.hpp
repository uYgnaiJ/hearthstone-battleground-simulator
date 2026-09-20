#pragma once
#include "../platform.hpp"
#include "engine.hpp"
#include <cstdlib>
#include <deque>

namespace bg {
class Host {
  Content content;
  Rules rules;
  std::filesystem::path dataDir;
  J cards, game = nullptr, history, lab, settings;
  std::deque<J> undo;
  J read(const std::string &name, const J &fallback) const {
    for (auto suffix : {".json", ".json.bak"})
      try {
        return readJson(dataDir / (name + suffix));
      } catch (const std::exception &) {
      }
    return fallback;
  }
  static void file(const std::filesystem::path &target, const J &value, int indent = -1) {
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    require(bool(out), "Cannot write " + target.string());
    out << value.dump(indent);
    out.flush();
    require(bool(out), "Failed to write " + target.string());
  }
  void write(const std::string &name, const J &value) const {
    auto target = dataDir / (name + ".json"), temp = dataDir / (name + ".json.tmp"),
         backup = dataDir / (name + ".json.bak");
    file(temp, value);
    if (std::filesystem::exists(target))
      std::filesystem::copy_file(target, backup, std::filesystem::copy_options::overwrite_existing);
#ifdef _WIN32
    require(MoveFileExW(temp.c_str(), target.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0,
            "Cannot replace " + target.string());
#else
    std::filesystem::rename(temp, target);
#endif
  }
  void persist(const J &next) {
    write("save", next);
    if (!game.is_null()) {
      undo.push_back(game);
      if (undo.size() > 30)
        undo.pop_front();
    }
    game = next;
    if (s(game, "phase") == "finished") {
      J h = arr();
      h.push_back(game);
      for (auto &old : history)
        if (s(old, "createdAt") != s(game, "createdAt") && h.size() < 12)
          h.push_back(old);
      write("history", h);
      history = std::move(h);
    }
  }
  J state() const {
    if (game.is_null())
      return nullptr;
    auto view = game;
    for (auto &p : view["players"])
      p = rules.recruitPresentation(p);
    view.erase("history");
    return view;
  }
  J metadata() const {
    J out = arr();
    for (auto &h : history) {
      if (!h.is_object() || !h.contains("players") || h["players"].empty())
        continue;
      J m = {{"id", h["createdAt"]},
             {"seed", h["seed"]},
             {"rounds", h["round"]},
             {"hero", h["players"][0]["hero"]},
             {"modified", h["modified"]}};
      if (h["players"][0].contains("placement"))
        m["placement"] = h["players"][0]["placement"];
      out.push_back(m);
    }
    return out;
  }
  J required() const {
    require(!game.is_null(), "Start a match first.");
    return game;
  }
  static uint32_t seed(const J &m, const std::string &field, uint32_t fallback = 0) {
    if (!m.contains(field) || !m[field].is_number())
      return fallback;
    double v = m[field].get<double>();
    if (!std::isfinite(v) || v < 0 || v > 4294967295.0)
      return fallback;
    return static_cast<uint32_t>(v);
  }
  static uint32_t strictSeed(const J &m) {
    require(m.contains("seed") && m["seed"].is_number_integer() && m["seed"].get<double>() >= 1 &&
                m["seed"].get<double>() <= 4294967295.0,
            "Seed must be between 1 and 4294967295.");
    return m["seed"].get<uint32_t>();
  }
  void newLab(uint32_t seedValue) {
    lab = rules.newGame(seedValue ? seedValue : 42, "patchwerk", cards);
    lab["players"][0]["board"] = arr();
    lab["players"][1]["board"] = arr();
    lab["modified"] = true;
  }
  static J replay(const J &round) {
    require(round.contains("combats") && !round["combats"].empty(), "No combat in this round");
    J result = round["combats"][0];
    for (auto &c : round["combats"])
      if (n(c, "leftId") == 0 || n(c, "rightId") == 0) {
        result = c;
        break;
      }
    return {{"result", result}, {"players", round["players"]}, {"round", round["round"]}};
  }
  J dispatch(const J &m) {
    J extra = J::object();
    auto type = s(m, "type");
    if (type == "boot")
      extra = {{"heroes", content.heroes}, {"cards", cards}, {"settings", settings},
               {"history", metadata()},    {"lab", lab},     {"dataDir", pathText(dataDir)}};
    else if (type == "new") {
      persist(rules.newGame(strictSeed(m), s(m, "hero"), cards, s(m, "difficulty", "standard")));
      undo.clear();
    } else if (type == "action") {
      auto next = required();
      J presentation = arr();
      std::string prior;
      rules.act(
          next, 0, m.at("command"), [&](const std::string &kind, const Player &player, int source) {
            auto p = player.save();
            auto signature = p.dump();
            if (signature != prior) {
              presentation.push_back(
                  {{"kind", kind}, {"player", rules.recruitPresentation(p)}, {"source", source}});
              prior = signature;
            }
          });
      if (!presentation.empty())
        presentation.push_back(
            {{"kind", "settle"}, {"player", rules.recruitPresentation(next["players"][0])}});
      persist(next);
      extra["presentation"] = presentation;
    } else if (type == "end") {
      auto next = required();
      require(next["players"][0]["discovers"].empty(), "Choose a discovery first.");
      for (int i = 1; i < 8; i++)
        rules.recruitAI(next, i);
      rules.resolveRound(next);
      persist(next);
      extra["history"] = metadata();
    } else if (type == "advance") {
      auto next = required();
      require(s(next, "phase") == "combat", "The round cannot advance now.");
      rules.beginRound(next);
      persist(next);
    } else if (type == "autoplay") {
      auto next = required();
      if (s(next, "phase") == "combat")
        rules.beginRound(next);
      if (s(next, "phase") == "recruit") {
        for (int i = 0; i < 8; i++)
          rules.recruitAI(next, i);
        rules.resolveRound(next);
      }
      persist(next);
      extra["history"] = metadata();
    } else if (type == "rewind") {
      require(!undo.empty(), "No earlier snapshot in this session.");
      auto prev = undo.back();
      prev["modified"] = true;
      write("save", prev);
      undo.pop_back();
      game = std::move(prev);
    } else if (type == "debug") {
      auto next = required();
      int id = n(m, "player");
      require(id >= 0 && id < 8, "Invalid player");
      Player p(next["players"][id]);
      std::istringstream in(s(m, "command"));
      std::string cmd, arg;
      in >> cmd >> arg;
      auto integer = [&](int64_t low, int64_t high) {
        size_t end = 0;
        int64_t value;
        try {
          value = std::stoll(arg, &end);
        } catch (...) {
          throw std::runtime_error("Invalid integer");
        }
        require(end == arg.size() && value >= low && value <= high,
                "Use a value from " + std::to_string(low) + " to " + std::to_string(high));
        return value;
      };
      if (cmd == "gold" || cmd == "health" || cmd == "tier") {
        int value = int(integer(cmd == "gold" ? 0 : 1, cmd == "tier"     ? 6
                                                       : cmd == "health" ? 999
                                                                         : 100));
        require(!p.data.contains("placement"), "Cannot edit an eliminated player");
        p.data[cmd] = value;
      } else if (cmd == "spawn") {
        require(p.board.size() < 7, "The warband is full");
        p.board.push_back(rules.makeUnit(next, arg, false, 0));
      } else if (cmd == "give") {
        require(p.hand.size() < 10, "Your hand is full");
        p.hand.push_back(rules.makeUnit(next, arg, false, 0));
      } else if (cmd == "clear") {
        for (auto &u : p.board)
          rules.returnUnit(next, u);
        p.board.clear();
      } else if (cmd == "seed")
        next["rng"] = uint32_t(integer(1, 4294967295LL));
      else
        throw std::runtime_error(
            "Commands: gold N, health N, tier N, spawn CARD_ID, give CARD_ID, clear, seed N");
      next["players"][id] = p.save();
      next["modified"] = true;
      emit(next, "debug", s(m, "command"));
      persist(next);
    } else if (type == "card.save") {
      auto next = cards;
      bool found = false;
      auto c = m.at("card");
      for (auto &existing : next)
        if (s(existing, "id") == s(c, "id")) {
          existing = c;
          found = true;
          break;
        }
      if (!found)
        next.push_back(c);
      next = validateCards(next, content);
      write("cards", next);
      cards = next;
      extra["cards"] = cards;
    } else if (type == "card.reset") {
      const J *base = nullptr;
      for (auto &c : content.cards)
        if (s(c, "id") == s(m, "cardId"))
          base = &c;
      require(base, "No base definition");
      auto next = cards;
      for (auto &c : next)
        if (s(c, "id") == s(m, "cardId"))
          c = *base;
      write("cards", next);
      cards = next;
      extra["cards"] = cards;
    } else if (type == "pack.import") {
      auto next = validateCards(readJson(utf8Path(s(m, "path"))), content);
      write("cards", next);
      cards = next;
      extra["cards"] = cards;
    } else if (type == "pack.export") {
      file(dataDir / "card-pack-export.json", cards, 2);
      extra["message"] = "Exported card-pack-export.json";
    } else if (type == "save.export") {
      require(!game.is_null(), "No match to export");
      auto name = "match-" + std::to_string(game["seed"].get<uint32_t>()) + ".json";
      file(dataDir / name, game);
      extra["message"] = "Exported " + name;
    } else if (type == "save.import")
      persist(parseGame(readJson(utf8Path(s(m, "path"))), content));
    else if (type == "history")
      extra["history"] = metadata();
    else if (type == "replay") {
      const J *saved = nullptr;
      for (auto &h : history)
        if (s(h, "createdAt") == s(m, "matchId"))
          saved = &h;
      require(saved, "Match not found");
      int index = n(m, "round");
      require(index >= 0 && index < int((*saved)["history"].size()), "Round not found");
      extra["replay"] = replay((*saved)["history"][index]);
    } else if (type == "settings") {
      auto next = settings;
      require(m.contains("settings") && m["settings"].is_object(), "Invalid settings");
      next.update(m["settings"]);
      write("settings", next);
      settings = next;
      extra["settings"] = settings;
    } else if (type == "lab.new") {
      newLab(seed(m, "seed", 42));
      extra["lab"] = lab;
    } else if (type == "lab.add") {
      auto next = lab;
      auto &board = next["players"][n(m, "side") == 1 ? 1 : 0]["board"];
      require(board.size() < 7, "A warband holds seven minions");
      auto u = rules.makeUnit(next, s(m, "cardId"), b(m, "golden"), 0);
      board.push_back(*u);
      lab = std::move(next);
      extra["lab"] = lab;
    } else if (type == "lab.remove") {
      auto &board = lab["players"][n(m, "side") == 1 ? 1 : 0]["board"];
      int i = n(m, "index", -1);
      require(i >= 0 && i < int(board.size()), "No minion selected");
      board.erase(board.begin() + i);
      extra["lab"] = lab;
    } else if (type == "lab.stat") {
      auto &board = lab["players"][n(m, "side") == 1 ? 1 : 0]["board"];
      int i = n(m, "index", -1);
      require(i >= 0 && i < int(board.size()), "No minion selected");
      require(m.contains("value") && m["value"].is_number_integer() &&
                  n(m, "value") >= (s(m, "stat") == "attack" ? 0 : 1) && n(m, "value") <= 9999,
              "Invalid stat");
      if (s(m, "stat") == "attack")
        board[i]["attack"] = m["value"];
      else {
        board[i]["health"] = m["value"];
        board[i]["maxHealth"] = m["value"];
      }
      extra["lab"] = lab;
    } else if (type == "lab.run") {
      auto next = lab;
      auto rng = seed(m, "seed", 42);
      next["rng"] = rng ? rng : 42;
      extra["replay"] = {{"result", rules.combat(next, next["players"][0], next["players"][1])},
                         {"players", lab["players"]},
                         {"round", 0}};
    } else if (type == "lab.odds") {
      auto next = lab;
      auto rng = seed(m, "seed", 42);
      next["rng"] = rng ? rng : 42;
      int wins = 0, ties = 0, capped = 0, samples = std::clamp(n(m, "samples", 100), 1, 1000);
      for (int i = 0; i < samples; i++) {
        auto c = rules.combat(next, next["players"][0], next["players"][1], false);
        if (c["winner"] == 0)
          wins++;
        if (c["winner"].is_null())
          ties++;
        if (b(c, "capped"))
          capped++;
      }
      extra["odds"] = {{"wins", wins},
                       {"ties", ties},
                       {"losses", samples - wins - ties},
                       {"samples", samples},
                       {"capped", capped}};
    } else if (type == "lab.export") {
      file(dataDir / "scenario.json", lab);
      extra["message"] = "Exported scenario.json";
    } else if (type == "lab.import") {
      lab = parseGame(readJson(utf8Path(s(m, "path"))), content);
      extra["lab"] = lab;
    } else
      throw std::runtime_error("Unknown request");
    if (extra.contains("lab"))
      for (auto &p : extra["lab"]["players"])
        p = rules.recruitPresentation(p);
    J result = {{"id", m.value("id", J())}, {"ok", true}, {"type", type}, {"game", state()}};
    result.update(extra);
    return result;
  }

public:
  static std::filesystem::path defaultDataDir() {
#ifdef _WIN32
    if (auto env = _wgetenv(L"BOBS_DATA_DIR"))
      return std::filesystem::path(env);
    if (auto env = _wgetenv(L"LOCALAPPDATA"))
      return std::filesystem::path(env) / "BobsBattlegrounds";
#else
    if (auto env = std::getenv("BOBS_DATA_DIR"))
      return utf8Path(env);
#ifdef __APPLE__
    if (auto env = std::getenv("HOME"))
      return utf8Path(env) / "Library" / "Application Support" / "BobsBattlegrounds";
#endif
    if (auto env = std::getenv("LOCALAPPDATA"))
      return utf8Path(env) / "BobsBattlegrounds";
#endif
    return std::filesystem::current_path() / "BobsBattlegrounds";
  }
  explicit Host(const std::filesystem::path &contentDir,
                const std::filesystem::path &saves = defaultDataDir())
      : content(contentDir), rules(content), dataDir(saves) {
    std::filesystem::create_directories(dataDir);
    cards = content.cards;
    try {
      cards = validateCards(read("cards", cards), content);
    } catch (const std::exception &) {
    }
    for (auto suffix : {".json", ".json.bak"})
      try {
        game = parseGame(readJson(dataDir / (std::string("save") + suffix)), content);
        break;
      } catch (const std::exception &) {
      }
    history = read("history", arr());
    if (!history.is_array())
      history = arr();
    settings = read("settings", J{{"volume", .4}, {"speed", 1}, {"fullscreen", false}});
    if (!settings.is_object())
      settings = {{"volume", .4}, {"speed", 1}, {"fullscreen", false}};
    newLab(42);
  }
  J handle(const J &m) {
    try {
      return dispatch(m);
    } catch (const std::exception &e) {
      return {{"id", m.is_object() ? m.value("id", J()) : J()}, {"ok", false}, {"error", e.what()}};
    }
  }
};
} // namespace bg
