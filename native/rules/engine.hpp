#pragma once
#include "common.hpp"

namespace bg {
inline void buff(const U &u, int a, int h, const std::string &source = "Stat bonus") {
  int applied = std::max(0, n(u, "attack") + a) - n(u, "attack");
  if (applied || h) {
    if (!u->contains("enchantments"))
      (*u)["enchantments"] = arr();
    bool found = false;
    for (auto &e : (*u)["enchantments"])
      if (s(e, "source") == source) {
        add(e, "attack", applied);
        add(e, "health", h);
        found = true;
        break;
      }
    if (!found)
      (*u)["enchantments"].push_back({{"source", source}, {"attack", applied}, {"health", h}});
  }
  (*u)["attack"] = std::max(0, n(u, "attack") + a);
  add(u, "health", h);
  add(u, "maxHealth", h);
}
struct Rules {
  const Content &content;
  explicit Rules(const Content &c) : content(c) {}
  U makeUnit(J &g, const std::string &id, bool golden = false, int copies = 1) const {
    auto c = card(g, id);
    require(c, "Unknown card " + id);
    int f = golden ? 2 : 1, uid = n(g, "nextId");
    add(g, "nextId", 1);
    return std::make_shared<J>(J{{"uid", uid},
                                 {"cardId", id},
                                 {"name", (*c)["name"]},
                                 {"attack", n(*c, "attack") * f},
                                 {"health", n(*c, "health") * f},
                                 {"maxHealth", n(*c, "health") * f},
                                 {"tribe", (*c)["tribe"]},
                                 {"keywords", (*c)["keywords"]},
                                 {"golden", golden},
                                 {"tier", (*c)["tier"]},
                                 {"effect", (*c)["effect"]},
                                 {"art", (*c)["art"]},
                                 {"copies", copies},
                                 {"extraDeaths", arr()},
                                 {"shifter", s(*c, "effect") == "Shifter Zerus"}});
  }
  U token(J &g, const std::string &name, int a, int h, const std::string &t = "NONE",
          J keys = arr()) const {
    int uid = n(g, "nextId");
    add(g, "nextId", 1);
    return std::make_shared<J>(J{{"uid", uid},
                                 {"cardId", "token:" + name},
                                 {"name", name},
                                 {"attack", a},
                                 {"health", h},
                                 {"maxHealth", h},
                                 {"tribe", t},
                                 {"keywords", keys},
                                 {"golden", false},
                                 {"tier", 1},
                                 {"effect", ""},
                                 {"art", s(content.tokenArt, name)},
                                 {"copies", 0},
                                 {"extraDeaths", arr()}});
  }
  void returnUnit(J &g, const U &u) const {
    auto &pool = g["pool"];
    auto id = s(u, "cardId");
    if (n(u, "copies") && pool.contains(id))
      add(pool, id, n(u, "copies"));
    if (u->contains("absorbed"))
      for (auto it = (*u)["absorbed"].begin(); it != (*u)["absorbed"].end(); ++it)
        if (pool.contains(it.key()))
          add(pool, it.key(), it.value().get<int>());
  }
  void refresh(J &g, Player &p, bool higher = false, bool fillOnly = false) const {
    if (!fillOnly) {
      for (auto &u : p.shop)
        returnUnit(g, u);
      p.shop.clear();
    }
    int count = std::array{0, 3, 4, 4, 5, 5, 6}.at(n(p.data, "tier"));
    for (int i = int(p.shop.size()); i < count; i++) {
      std::vector<J> eligible;
      int total = 0;
      for (const auto &c : g["cards"])
        if (b(c, "enabled") && n(g["pool"], s(c, "id")) > 0 &&
            ((higher && i == count - 1) ? n(c, "tier") == std::min(6, n(p.data, "tier") + 1)
                                        : n(c, "tier") <= n(p.data, "tier"))) {
          eligible.push_back(c);
          total += n(g["pool"], s(c, "id"));
        }
      if (!total)
        continue;
      double roll = random(g) * total;
      for (auto &c : eligible)
        if ((roll -= n(g["pool"], s(c, "id"))) < 0) {
          auto id = s(c, "id");
          add(g["pool"], id, -1);
          auto u = makeUnit(g, id);
          if (p.hero() == "millificent" && tribe(u, "MECHANICAL"))
            buff(u, 1, 1, s(p.data["hero"], "power"));
          p.shop.push_back(u);
          break;
        }
    }
    p.data["frozen"] = false;
  }
  J newGame(uint32_t seed, const std::string &heroKey, const J &cards,
            const std::string &difficulty = "standard") const {
    J pool = J::object();
    for (auto &c : cards)
      if (b(c, "enabled"))
        pool[s(c, "id")] = c["pool"];
    J g = {{"version", 1},
           {"seed", seed ? seed : 1},
           {"rng", seed ? seed : 1},
           {"nextId", 1},
           {"round", 1},
           {"phase", "recruit"},
           {"players", arr()},
           {"pool", pool},
           {"cards", cards},
           {"contentHash", contentHash(cards)},
           {"log", arr()},
           {"combats", arr()},
           {"modified", false},
           {"difficulty", difficulty},
           {"createdAt", timestamp()},
           {"history", arr()}};
    J hero = content.heroes[0];
    for (auto &h : content.heroes)
      if (s(h, "key") == heroKey)
        hero = h;
    std::vector<J> others;
    for (auto &h : content.heroes)
      if (s(h, "key") != s(hero, "key"))
        others.push_back(h);
    others = shuffle(g, others);
    others.insert(others.begin(), hero);
    const char *names[] = {"You", "Ember", "Moss", "Copper", "Juniper", "Flint", "Willow", "Ash"};
    for (int id = 0; id < 8; id++) {
      auto h = others[id];
      J p = {{"id", id},
             {"name", names[id]},
             {"hero", h},
             {"health", h["health"]},
             {"tier", 1},
             {"gold", 3},
             {"upgrade", s(h, "key") == "bartendotron" ? 4 : 5},
             {"board", arr()},
             {"hand", arr()},
             {"shop", arr()},
             {"frozen", false},
             {"powerUsed", false},
             {"powerActive", false},
             {"played", J::object()},
             {"coins", 0},
             {"bananas", 0},
             {"ratTribe", "BEAST"},
             {"discovers", arr()},
             {"aiReason", ""}};
      g["players"].push_back(p);
    }
    for (auto &value : g["players"]) {
      Player p(value);
      if (p.hero() == "curator")
        p.board.push_back(token(g, "Amalgam", 1, 1, "ALL"));
      if (p.hero() == "afkay")
        p.data["gold"] = 0;
      refresh(g, p);
      value = p.save();
    }
    emit(g, "system", "Origins lobby created · seed " + std::to_string(seed ? seed : 1));
    return g;
  }
  bool summonRecruit(J &, Player &p, const U &u, int position = -1) const {
    if (p.board.size() >= 7)
      return false;
    if (position < 0)
      position = int(p.board.size());
    p.board.insert(p.board.begin() + std::clamp(position, 0, int(p.board.size())), u);
    for (auto &v : p.board) {
      if (n(v, "uid") == n(u, "uid"))
        continue;
      int f = factor(v);
      auto e = s(v, "effect");
      if (tribe(u, "BEAST")) {
        if (e == "Pack Leader")
          buff(u, 3 * f, 0, s(v, "name"));
        if (e == "Mama Bear")
          buff(u, 5 * f, 5 * f, s(v, "name"));
      }
      if (tribe(u, "MURLOC") && e == "Murloc Tidecaller")
        buff(v, f, 0, s(v, "name"));
      if (tribe(u, "MECHANICAL") && e == "Cobalt Guardian")
        keyword(v, "DIVINE_SHIELD");
    }
    return true;
  }
  void discover(J &g, Player &p, int tier, const std::string &t = "") const {
    std::vector<J> cs;
    for (auto &c : g["cards"])
      if (b(c, "enabled") && (t.empty() ? n(c, "tier") == tier : tribe(c, t)) &&
          n(g["pool"], s(c, "id")) > 0)
        cs.push_back(c);
    cs = shuffle(g, cs);
    J choice = arr();
    for (int i = 0; i < std::min(3, int(cs.size())); i++)
      choice.push_back(cs[i]["id"]);
    if (!choice.empty())
      p.data["discovers"].push_back(choice);
  }
  void absorb(const U &target, const U &source) const {
    if (source->contains("absorbed"))
      for (auto it = (*source)["absorbed"].begin(); it != (*source)["absorbed"].end(); ++it) {
        if (!target->contains("absorbed"))
          (*target)["absorbed"] = J::object();
        add((*target)["absorbed"], it.key(), it.value().get<int>());
      }
  }
  void triple(J &g, Player &p) const {
    for (auto &c : g["cards"]) {
      auto found = filter(joined(p.board, p.hand),
                          [&](auto &u) { return s(u, "cardId") == s(c, "id") && !b(u, "golden"); });
      if (found.size() < 3)
        continue;
      found.resize(3);
      int copies = 0;
      for (auto &u : found)
        copies += n(u, "copies");
      auto gold = makeUnit(g, s(c, "id"), true, copies);
      for (auto &u : found) {
        buff(gold, n(u, "attack") - n(c, "attack"), n(u, "maxHealth") - n(c, "health"),
             "Inherited triple buffs");
        for (auto &k : (*u)["keywords"])
          keyword(gold, k.get<std::string>());
        for (auto &d : (*u)["extraDeaths"])
          (*gold)["extraDeaths"].push_back(d);
        absorb(gold, u);
      }
      p.board = filter(p.board, [&](auto &u) { return !contains(found, u); });
      p.hand = filter(p.hand, [&](auto &u) { return !contains(found, u); });
      p.hand.push_back(gold);
      emit(g, "triple", s(p.data, "name") + " combined a golden " + s(c, "name"));
      triple(g, p);
      return;
    }
  }
  void battlecry(J &g, Player &p, const U &u, int target = -1) const {
    int f = factor(u);
    auto other = filter(p.board, [&](auto &v) { return n(v, "uid") != n(u, "uid"); });
    auto def = card(g, s(u, "cardId"));
    int ca = def ? n(*def, "buffAttack", 1) : 1, ch = def ? n(*def, "buffHealth", 1) : 1;
    auto e = s(u, "effect"), name = s(u, "name");
    auto give = [&](std::string t, int a, int h, bool taunt = false) {
      auto choices = filter(other, [&](auto &v) { return t.empty() || tribe(v, t); });
      auto v = findUid(choices, target);
      if (!v && !choices.empty())
        v = pick(g, choices);
      if (v) {
        buff(v, a * f * ca, h * f * ch, name);
        if (taunt)
          keyword(v, "TAUNT");
      }
    };
    auto all = [&](std::string t, int a, int h, bool taunts = false) {
      for (auto &v : other)
        if ((t.empty() || tribe(v, t)) && (!taunts || has(v, "TAUNT")))
          buff(v, a * f * ca, h * f * ch, name);
    };
    auto summons = [&](std::string name, int a, int h, std::string t) {
      int mult = 1;
      for (auto &v : other)
        if (s(v, "effect") == "Khadgar")
          mult = std::max(mult, b(v, "golden") ? 3 : 2);
      for (int i = 0; i < mult; i++)
        summonRecruit(g, p, token(g, name, a * f, h * f, t));
    };
    if (e == "Alleycat")
      summons("Tabbycat", 1, 1, "BEAST");
    else if (e == "Murloc Tidehunter")
      summons("Murloc Scout", 1, 1, "MURLOC");
    else if (e == "Vulgar Homunculus") {
      if (!effect(p.board, "Mal'Ganis"))
        add(p.data, "health", -2);
    } else if (e == "Rockpool Hunter")
      give("MURLOC", 1, 1);
    else if (e == "Nathrezim Overseer")
      give("DEMON", 2, 2);
    else if (e == "Metaltooth Leaper")
      all("MECHANICAL", 2, 0);
    else if (e == "Coldlight Seer")
      all("MURLOC", 0, 2);
    else if (e == "Crystalweaver")
      all("DEMON", 1, 1);
    else if (e == "Houndmaster")
      give("BEAST", 2, 2, true);
    else if (e == "Screwjank Clunker")
      give("MECHANICAL", 2, 2);
    else if (e == "Virmen Sensei")
      give("BEAST", 2, 2);
    else if (e == "Strongshell Scavenger")
      all("", 2, 2, true);
    else if (e == "Zoobot" || e == "Menagerie Magician")
      for (auto t : {"BEAST", "DRAGON", "MURLOC"})
        give(t, e == "Zoobot" ? 1 : 2, e == "Zoobot" ? 1 : 2);
    else if (e == "Defender of Argus") {
      int i = indexOf(p.board, u);
      for (int j : {i - 1, i + 1})
        if (j >= 0 && j < int(p.board.size())) {
          buff(p.board[j], f, f, name);
          keyword(p.board[j], "TAUNT");
        }
    } else if (e == "Pogo-Hopper") {
      int bonus = 2 * f * n(p.data["played"], s(u, "cardId"));
      buff(u, bonus, bonus, name);
    } else if (e == "Annihilan Battlemaster")
      buff(u, 0, std::max(0, n(p.data["hero"], "health") - n(p.data, "health")) * f, name);
    else if (e == "Primalfin Lookout") {
      if (std::any_of(other.begin(), other.end(), [](auto &v) { return tribe(v, "MURLOC"); }))
        for (int i = 0; i < f; i++)
          discover(g, p, n(p.data, "tier"), "MURLOC");
    } else if (e == "Gentle Megasaur")
      for (int i = 0; i < f; i++) {
        auto adapt = pick(g, std::vector<std::string>{"attack", "health", "stats", "shield",
                                                      "poison", "taunt", "wind"});
        for (auto &v : other)
          if (tribe(v, "MURLOC")) {
            if (adapt == "attack")
              buff(v, 3, 0, name + " (Adapt)");
            else if (adapt == "health")
              buff(v, 0, 3, name + " (Adapt)");
            else if (adapt == "stats")
              buff(v, 1, 1, name + " (Adapt)");
            else
              keyword(v, adapt == "shield"   ? "DIVINE_SHIELD"
                         : adapt == "poison" ? "POISONOUS"
                         : adapt == "taunt"  ? "TAUNT"
                                             : "WINDFURY");
          }
        emit(g, "effect", "Murlocs adapted: " + adapt);
      }
  }
  using Presentation = std::function<void(const std::string &, const Player &, int)>;
  void act(J &g, int id, const J &cmd, Presentation presentation = {}) const {
    require(id >= 0 && id < int(g["players"].size()) && n(g["players"][id], "health") > 0 &&
                s(g, "phase") == "recruit",
            "Not in recruitment");
    Player p(g["players"][id]);
    auto type = s(cmd, "type");
    auto &d = p.data;
    auto hero = p.hero();
    int idx = n(cmd, "index", -1);
    auto selected = [](const Units &a, int i, const std::string &error) {
      require(i >= 0 && i < int(a.size()), error);
      return a[i];
    };
    auto target = findUid(p.board, n(cmd, "target", -1));
    require(d["discovers"].empty() || type == "discover", "Choose your discovery first");
    require(hero != "afkay" || n(g, "round") >= 3, "A. F. Kay skips the first two turns");
    if (type == "buy") {
      auto u = selected(p.shop, idx, "Select a shop minion");
      require(n(d, "gold") >= 3, "Buying costs 3 gold");
      require(p.hand.size() < 10, "Your hand is full");
      add(d, "gold", -3);
      p.shop.erase(p.shop.begin() + idx);
      if (hero == "ratking" && tribe(u, s(d, "ratTribe")))
        buff(u, 1, 2, s(d["hero"], "power"));
      p.hand.push_back(u);
      triple(g, p);
      emit(g, "buy", s(d, "name") + " bought " + s(u, "name"));
    } else if (type == "play") {
      auto u = selected(p.hand, idx, "Select a card in hand");
      bool magnetic = b(cmd, "magnetic");
      require(!magnetic || (has(u, "MAGNETIC") && target && tribe(target, "MECHANICAL")),
              "Magnetic requires a friendly Mech");
      require(magnetic || p.board.size() < 7, "Your warband is full");
      p.hand.erase(p.hand.begin() + idx);
      if (magnetic) {
        buff(target, n(u, "attack"), n(u, "maxHealth"), s(u, "name") + " (Magnetic)");
        for (auto &k : (*u)["keywords"])
          if (k != "MAGNETIC")
            keyword(target, k.get<std::string>());
        if (s(u, "effect") == "Replicating Menace")
          (*target)["extraDeaths"].push_back(b(u, "golden") ? "Replicating Menace:gold"
                                                            : "Replicating Menace");
        if (!target->contains("absorbed"))
          (*target)["absorbed"] = J::object();
        add((*target)["absorbed"], s(u, "cardId"), n(u, "copies"));
        absorb(target, u);
      } else {
        summonRecruit(g, p, u,
                      std::clamp(n(cmd, "position", int(p.board.size())), 0, int(p.board.size())));
        if (presentation)
          presentation("enter", p, n(u, "uid"));
        int repeats = b(d, "powerActive") && hero == "shudderwock" ? 2 : 1;
        for (auto &v : p.board)
          if (v != u && s(v, "effect") == "Brann Bronzebeard")
            repeats = std::max(repeats, b(v, "golden") ? 3 : 2);
        for (int i = 0; i < repeats; i++) {
          battlecry(g, p, u, n(cmd, "target", -1));
          if (presentation)
            presentation("battlecry", p, n(u, "uid"));
        }
      }
      auto def = card(g, s(u, "cardId"));
      bool bc = def && s(*def, "text").find("Battlecry") != std::string::npos;
      for (auto &v : p.board)
        if (v != u) {
          if (bc && s(v, "effect") == "Crowd Favorite")
            buff(v, factor(v), factor(v), s(v, "name"));
          if (tribe(u, "DEMON") && s(v, "effect") == "Wrath Weaver") {
            buff(v, 2 * factor(v), 2 * factor(v), s(v, "name"));
            if (!effect(p.board, "Mal'Ganis"))
              add(d, "health", -1);
          }
        }
      if (bc && hero == "shudderwock")
        d["powerActive"] = false;
      add(d["played"], s(u, "cardId"), 1);
      if (b(u, "golden"))
        discover(g, p, std::min(6, n(d, "tier") + 1));
      emit(g, "play", s(d, "name") + " played " + s(u, "name"));
    } else if (type == "sell") {
      auto u = selected(p.board, idx, "Select a warband minion");
      p.board.erase(p.board.begin() + idx);
      d["gold"] = std::min(10, n(d, "gold") + 1);
      returnUnit(g, u);
      if (hero == "deryl" && !p.shop.empty())
        for (int i = 0; i < 2; i++)
          buff(pick(g, p.shop), 1, 1, s(d["hero"], "power"));
      if (hero == "mukla" && tribe(u, "BEAST"))
        add(d, "bananas", 1);
      emit(g, "sell", s(d, "name") + " sold " + s(u, "name"));
    } else if (type == "move") {
      int from = n(cmd, "from", -1), to = n(cmd, "to", -1);
      auto u = selected(p.board, from, "Invalid position");
      require(to >= 0 && to < int(p.board.size()), "Invalid position");
      p.board.erase(p.board.begin() + from);
      p.board.insert(p.board.begin() + to, u);
    } else if (type == "refresh") {
      require(n(d, "gold") >= 1, "Refresh costs 1 gold");
      add(d, "gold", -1);
      refresh(g, p);
    } else if (type == "freeze")
      d["frozen"] = !b(d, "frozen");
    else if (type == "upgrade") {
      require(n(d, "tier") < 6, "Maximum tavern tier");
      require(n(d, "gold") >= n(d, "upgrade"), "Not enough gold to upgrade");
      add(d, "gold", -n(d, "upgrade"));
      add(d, "tier", 1);
      d["upgrade"] = std::max(0, std::array{0, 0, 7, 8, 9, 10, 0}.at(n(d, "tier")) -
                                     (hero == "bartendotron" ? 1 : 0));
      emit(g, "upgrade", s(d, "name") + " reached tavern tier " + std::to_string(n(d, "tier")));
    } else if (type == "discover") {
      auto &ds = d["discovers"];
      require(!ds.empty() && idx >= 0 && idx < int(ds[0].size()), "Invalid discovery");
      require(p.hand.size() < 10, "Make room in your hand");
      auto choice = ds[0][idx].get<std::string>();
      ds.erase(ds.begin());
      int copies = n(g["pool"], choice) > 0 ? 1 : 0;
      if (copies)
        add(g["pool"], choice, -1);
      p.hand.push_back(makeUnit(g, choice, false, copies));
      triple(g, p);
    } else if (type == "coin") {
      require(n(d, "coins") > 0 && n(d, "gold") < 10, "No usable coin");
      add(d, "coins", -1);
      add(d, "gold", 1);
    } else if (type == "banana") {
      require(n(d, "bananas") > 0 && target, "Choose a minion for your banana");
      add(d, "bananas", -1);
      buff(target, 1, 1, "Banana");
    } else if (type == "power") {
      auto &h = d["hero"];
      require(!b(h, "passive"), "This hero power is passive");
      require(!b(d, "powerUsed"), "Hero power already used");
      require(n(d, "gold") >= n(h, "cost"), "Not enough gold for hero power");
      require(hero != "george" || (target && !has(target, "DIVINE_SHIELD")),
              "Select a minion without Divine Shield");
      require(hero != "yogg" || (!p.shop.empty() && p.hand.size() < 10),
              "Need a shop minion and hand space");
      add(d, "gold", -n(h, "cost"));
      d["powerUsed"] = true;
      d["powerActive"] = true;
      auto source = s(h, "power");
      if (hero == "pyramad" && !p.board.empty())
        buff(pick(g, p.board), 0, 2, source);
      if (hero == "jaraxxus")
        for (auto &u : p.board)
          if (tribe(u, "DEMON"))
            buff(u, 1, 1, source);
      if (hero == "wagtoggle")
        for (auto t : {"MECHANICAL", "DEMON", "MURLOC", "BEAST"}) {
          auto a = filter(p.board, [&](auto &u) { return tribe(u, t); });
          if (!a.empty())
            buff(pick(g, a), 0, 1, source);
        }
      if (hero == "george")
        keyword(target, "DIVINE_SHIELD");
      if (hero == "toki")
        refresh(g, p, true);
      if (hero == "yogg") {
        auto u = pick(g, p.shop);
        p.shop = filter(p.shop, [&](auto &v) { return v != u; });
        buff(u, 1, 1, source);
        p.hand.push_back(u);
        triple(g, p);
      }
      if (hero == "gallywix")
        add(d, "coins", 1);
      if (hero == "lichbaz") {
        if (!effect(p.board, "Mal'Ganis"))
          add(d, "health", -3);
        add(d, "coins", 1);
      }
      emit(g, "power", s(d, "name") + " used " + source);
    } else
      throw std::runtime_error("Unknown action " + type);
    g["players"][id] = p.save();
  }
  void endRecruit(J &g, Player &p) const {
    for (auto &u : p.board) {
      int f = factor(u);
      if (s(u, "effect") == "Iron Sensei") {
        auto a = filter(p.board, [&](auto &v) { return v != u && tribe(v, "MECHANICAL"); });
        if (!a.empty())
          buff(pick(g, a), 2 * f, 2 * f, s(u, "name"));
      }
      if (s(u, "effect") == "Lightfang Enforcer")
        for (auto t : {"MECHANICAL", "MURLOC", "DEMON", "BEAST"}) {
          auto a = filter(p.board, [&](auto &v) { return v != u && tribe(v, t); });
          if (!a.empty())
            buff(pick(g, a), 2 * f, 2 * f, s(u, "name"));
        }
    }
    if (b(p.data, "frozen") && p.hero() == "sindragosa")
      for (auto &u : p.shop)
        buff(u, 1, 1, s(p.data["hero"], "power"));
  }
  void beginRound(J &g) const {
    add(g, "round", 1);
    g["phase"] = "recruit";
    for (auto &value : g["players"])
      if (n(value, "health") > 0) {
        Player p(value);
        auto &d = p.data;
        d["gold"] = std::min(10, n(g, "round") + 2);
        d["upgrade"] = std::max(0, n(d, "upgrade") - 1);
        d["powerUsed"] = false;
        d["powerActive"] = false;
        d["ratTribe"] = pick(g, std::vector<std::string>{"BEAST", "MECHANICAL", "MURLOC", "DEMON"});
        refresh(g, p, false, b(d, "frozen"));
        for (auto &u : p.board)
          if (s(u, "effect") == "Micro Machine")
            buff(u, factor(u), 0, s(u, "name"));
        for (auto &u : p.hand)
          if (b(u, "shifter")) {
            std::vector<J> cs;
            for (auto &c : g["cards"])
              if (b(c, "enabled"))
                cs.push_back(c);
            auto c = pick(g, cs);
            auto v = makeUnit(g, s(c, "id"), b(u, "golden"), 0);
            (*v)["shifter"] = true;
            returnUnit(g, u);
            u = v;
          }
        if (p.hero() == "afkay") {
          if (n(g, "round") < 3)
            d["gold"] = 0;
          if (n(g, "round") == 3) {
            discover(g, p, 3);
            discover(g, p, 4);
          }
        }
        value = p.save();
      }
    emit(g, "round", "Recruitment · round " + std::to_string(n(g, "round")));
  }

#include "ai.inc"
#include "combat.inc"
};
} // namespace bg
