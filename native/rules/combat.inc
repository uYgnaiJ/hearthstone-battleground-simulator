// Included inside Rules. Combat operates on disposable copies of each warband.
bool auraSource(const U &u) const {
  static const std::set<std::string> names = {"Dire Wolf Alpha",   "Murloc Warleader",
                                              "Phalanx Commander", "Siegebreaker",
                                              "Mal'Ganis",         "Old Murk-Eye"};
  return names.contains(s(u, "effect"));
}
J auraEffects(const Units &board, int index, const Units &all) const {
  auto u = board[index];
  J effects = arr();
  for (int j = 0; j < int(board.size()); j++) {
    auto v = board[j];
    if (v == u || n(v, "health") <= 0 || b(v, "poisoned"))
      continue;
    int f = factor(v), a = 0, h = 0;
    auto e = s(v, "effect");
    if (e == "Dire Wolf Alpha" && std::abs(index - j) == 1)
      a += f;
    if (e == "Murloc Warleader" && tribe(u, "MURLOC"))
      a += 2 * f;
    if (e == "Phalanx Commander" && has(u, "TAUNT"))
      a += 2 * f;
    if (e == "Siegebreaker" && tribe(u, "DEMON"))
      a += f;
    if (e == "Mal'Ganis" && tribe(u, "DEMON")) {
      a += 2 * f;
      h += 2 * f;
    }
    if (a || h)
      effects.push_back(
          {{"source", s(v, "name")}, {"sourceUid", n(v, "uid")}, {"attack", a}, {"health", h}});
  }
  if (s(u, "effect") == "Old Murk-Eye") {
    int a = 0;
    for (auto &v : all)
      if (v != u && tribe(v, "MURLOC"))
        a += factor(u);
    if (a)
      effects.push_back(
          {{"source", s(u, "name")}, {"sourceUid", n(u, "uid")}, {"attack", a}, {"health", 0}});
  }
  return effects;
}
std::pair<int, int> auraBonus(const Units &board, int index, const Units &all) const {
  int a = 0, h = 0;
  for (auto &e : auraEffects(board, index, all)) {
    a += n(e, "attack");
    h += n(e, "health");
  }
  return {a, h};
}
J recruitPresentation(const J &player) const {
  Player p(player);
  auto original = p.board;
  p.board.clear();
  for (int i = 0; i < int(original.size()); i++) {
    auto u = copy(original[i]);
    auto [a, h] = auraBonus(original, i, original);
    add(u, "attack", a);
    add(u, "health", h);
    add(u, "maxHealth", h);
    (*u)["auraAttack"] = a;
    (*u)["auraHealth"] = h;
    (*u)["auraEffects"] = auraEffects(original, i, original);
    (*u)["auraSource"] = auraSource(u);
    p.board.push_back(u);
  }
  return p.save();
}
J combat(J &g, const J &left, const J &right, bool record = true) const {
  std::array<Units, 2> boards = {units(left.at("board")), units(right.at("board"))}, deadMechs;
  J frames = arr();
  std::map<int, std::pair<int, int>> aura;
  int events = 0;
  auto alive = [](const U &u) { return n(u, "health") > 0 && !b(u, "poisoned"); };
  auto snapshot = [&](const std::string &text, int attacker = -1, int target = -1) {
    if (!record)
      return;
    J f = {{"left", serialize(boards[0])}, {"right", serialize(boards[1])}, {"text", text}};
    if (attacker >= 0)
      f["attacker"] = attacker;
    if (target >= 0)
      f["target"] = target;
    frames.push_back(f);
  };
  auto updateAuras = [&]() {
    for (auto &board : boards)
      for (auto &u : board) {
        auto prev = aura[n(u, "uid")];
        add(u, "attack", -prev.first);
        add(u, "health", -prev.second);
        add(u, "maxHealth", -prev.second);
        aura[n(u, "uid")] = {0, 0};
      }
    for (auto &board : boards)
      for (int i = 0; i < int(board.size()); i++) {
        auto u = board[i];
        auto all = joined(boards[0], boards[1]);
        auto [a, h] = auraBonus(board, i, all);
        add(u, "attack", a);
        add(u, "health", h);
        add(u, "maxHealth", h);
        aura[n(u, "uid")] = {a, h};
        (*u)["auraAttack"] = a;
        (*u)["auraHealth"] = h;
        (*u)["auraEffects"] = auraEffects(board, i, all);
        (*u)["auraSource"] = auraSource(u);
      }
  };
  auto summon = [&](int side, U u, int index = -1, bool repeat = true) {
    if (index < 0)
      index = int(boards[side].size());
    int mult = 1;
    if (repeat)
      for (auto &v : boards[side])
        if (alive(v) && s(v, "effect") == "Khadgar")
          mult = std::max(mult, b(v, "golden") ? 3 : 2);
    for (int i = 0; i < mult; i++) {
      if (boards[side].size() >= 7)
        return;
      auto v = i == 0 ? u : copy(u);
      if (i) {
        (*v)["uid"] = n(g, "nextId");
        add(g, "nextId", 1);
      }
      boards[side].insert(boards[side].begin() + std::clamp(index + i, 0, int(boards[side].size())),
                          v);
      for (auto &ally : boards[side]) {
        if (ally == v || !alive(ally))
          continue;
        int f = factor(ally);
        auto e = s(ally, "effect");
        if (tribe(v, "BEAST")) {
          if (e == "Pack Leader")
            buff(v, 3 * f, 0, s(ally, "name"));
          if (e == "Mama Bear")
            buff(v, 5 * f, 5 * f, s(ally, "name"));
        }
        if (tribe(v, "MURLOC") && e == "Murloc Tidecaller")
          buff(ally, f, 0, s(ally, "name"));
        if (tribe(v, "MECHANICAL") && e == "Cobalt Guardian")
          keyword(ally, "DIVINE_SHIELD");
      }
    }
    updateAuras();
  };
  auto damage = [&](int side, U u, int amount, bool poison = false) {
    if (amount <= 0)
      return;
    if (has(u, "DIVINE_SHIELD")) {
      removeKeyword(u, "DIVINE_SHIELD");
      for (auto &ally : boards[side])
        if (alive(ally) && s(ally, "effect") == "Bolvar, Fireblood")
          buff(ally, 2 * factor(ally), 0, s(ally, "name"));
      return;
    }
    add(u, "health", -amount);
    if (poison)
      (*u)["poisoned"] = true;
    int f = factor(u);
    if (s(u, "effect") == "Imp Gang Boss")
      summon(side, token(g, "Imp", f, f, "DEMON"), indexOf(boards[side], u) + 1);
    if (s(u, "effect") == "Security Rover")
      summon(side, token(g, "Guard Bot", 2 * f, 3 * f, "MECHANICAL", J{"TAUNT"}),
             indexOf(boards[side], u) + 1);
  };
  auto hitRandom = [&](int side, int amount) {
    auto a = filter(boards[side], alive);
    if (!a.empty())
      damage(side, pick(g, a), amount);
  };
  auto deathEffect = [&](int side, U u, int index, std::string e) {
    int f = factor(u), other = 1 - side;
    auto spawn = [&](std::string name, int a, int h, std::string t = "NONE", J keys = arr(),
                     int count = 1) {
      for (int i = 0; i < count; i++)
        summon(side, token(g, name, a * f, h * f, t, keys), index + i);
    };
    auto team = [&](int a, int h, std::string t = "") {
      for (auto &v : boards[side])
        if (alive(v) && (t.empty() || tribe(v, t)))
          buff(v, a * f, h * f, s(u, "name"));
    };
    auto randomMinion = [&](std::function<bool(const J &)> predicate, int count) {
      std::vector<J> cs;
      for (auto &c : g["cards"])
        if (b(c, "enabled") && predicate(c))
          cs.push_back(c);
      for (int i = 0; i < count && !cs.empty(); i++) {
        auto c = pick(g, cs);
        summon(side, makeUnit(g, s(c, "id"), false, 0), index + i);
      }
    };
    if (e == "Mecharoo")
      spawn("Jo-E Bot", 1, 1, "MECHANICAL");
    else if (e == "Harvest Golem")
      spawn("Damaged Golem", 2, 1, "MECHANICAL");
    else if (e == "Kindly Grandmother")
      spawn("Big Bad Wolf", 3, 2, "BEAST");
    else if (e == "Infested Wolf")
      spawn("Spider", 1, 1, "BEAST", arr(), 2);
    else if (e == "Rat Pack")
      spawn("Rat", 1, 1, "BEAST", arr(), std::clamp(n(u, "attack"), 0, 7));
    else if (e == "Replicating Menace")
      spawn("Microbot", 1, 1, "MECHANICAL", arr(), 3);
    else if (e == "Savannah Highmane")
      spawn("Hyena", 2, 2, "BEAST", arr(), 2);
    else if (e == "Sated Threshadon")
      spawn("Primalfin", 1, 1, "MURLOC", arr(), 3);
    else if (e == "Mechano-Egg")
      spawn("Robosaur", 8, 8, "MECHANICAL");
    else if (e == "Voidlord")
      spawn("Voidwalker", 1, 3, "DEMON", J{"TAUNT"}, 3);
    else if (e == "The Beast")
      summon(other, token(g, "Finkle Einhorn", 3, 3));
    else if (e == "Kaboom Bot") {
      for (int i = 0; i < f; i++)
        hitRandom(other, 4);
    } else if (e == "Spawn of N'Zoth")
      team(1, 1);
    else if (e == "Goldrinn, the Great Wolf")
      team(4, 4, "BEAST");
    else if (e == "Selfless Hero") {
      for (int i = 0; i < f; i++) {
        auto cs =
            filter(boards[side], [&](auto &v) { return alive(v) && !has(v, "DIVINE_SHIELD"); });
        if (!cs.empty())
          keyword(pick(g, cs), "DIVINE_SHIELD");
      }
    } else if (e == "Tortollan Shellraiser") {
      auto cs = filter(boards[side], alive);
      if (!cs.empty())
        buff(pick(g, cs), f, f, s(u, "name"));
    } else if (e == "Mounted Raptor")
      randomMinion([](auto &c) { return n(c, "cost") == 1; }, f);
    else if (e == "Piloted Shredder")
      randomMinion([](auto &c) { return n(c, "cost") == 2; }, f);
    else if (e == "Piloted Sky Golem")
      randomMinion([](auto &c) { return n(c, "cost") == 4; }, f);
    else if (e == "Ghastcoiler")
      randomMinion(
          [](auto &c) {
            return s(c, "text").find("Deathrattle") != std::string::npos &&
                   s(c, "effect") != "Ghastcoiler";
          },
          2 * f);
    else if (e == "Sneed's Old Shredder")
      randomMinion([](auto &c) { return s(c, "rarity") == "LEGENDARY"; }, f);
    else if (e == "Kangor's Apprentice") {
      auto dead = deadMechs[side];
      for (int i = 0; i < std::min(2 * f, int(dead.size())); i++) {
        auto m = dead[i];
        auto c = card(g, s(m, "cardId"));
        auto v = c ? makeUnit(g, s(*c, "id"), b(m, "golden"), 0)
                   : token(g, s(m, "name"), n(m, "attack"), n(m, "maxHealth"), s(m, "tribe"));
        summon(side, v, index++);
      }
    }
  };
  auto deaths = [&]() {
    int guard = 0;
    while (guard++ < 100) {
      std::array<Units, 2> dying;
      std::array<std::vector<int>, 2> positions;
      int mult[2] = {1, 1};
      for (int side = 0; side < 2; side++) {
        dying[side] = filter(boards[side], [&](auto &u) { return !alive(u); });
        for (auto &v : boards[side])
          if (alive(v) && s(v, "effect") == "Baron Rivendare")
            mult[side] = std::max(mult[side], b(v, "golden") ? 3 : 2);
        for (auto &u : dying[side])
          positions[side].push_back(indexOf(boards[side], u));
      }
      if (dying[0].empty() && dying[1].empty())
        break;
      for (int side = 0; side < 2; side++) {
        boards[side] = filter(boards[side], alive);
        for (auto &u : dying[side])
          if (tribe(u, "MECHANICAL"))
            deadMechs[side].push_back(copy(u));
      }
      updateAuras();
      for (int side = 0; side < 2; side++)
        for (int i = 0; i < int(dying[side].size()); i++) {
          auto u = dying[side][i];
          auto allies = boards[side];
          for (auto &ally : allies)
            if (alive(ally)) {
              int f = factor(ally);
              auto e = s(ally, "effect");
              if (tribe(u, "BEAST") && e == "Scavenging Hyena")
                buff(ally, 2 * f, f, s(ally, "name"));
              if (tribe(u, "MECHANICAL") && e == "Junkbot")
                buff(ally, 2 * f, 2 * f, s(ally, "name"));
              if (tribe(u, "DEMON") && e == "Soul Juggler")
                hitRandom(1 - side, 3 * f);
            }
          for (int j = 0; j < mult[side]; j++) {
            deathEffect(side, u, positions[side][i], s(u, "effect"));
            for (auto &value : (*u)["extraDeaths"]) {
              auto e = value.get<std::string>();
              auto v = u;
              if (e.ends_with(":gold")) {
                v = copy(u);
                (*v)["golden"] = true;
                e.resize(e.size() - 5);
              }
              deathEffect(side, v, positions[side][i], e);
            }
          }
          if (has(u, "REBORN") && !b(u, "rebornUsed")) {
            auto c = card(g, s(u, "cardId"));
            auto v = c ? makeUnit(g, s(*c, "id"), b(u, "golden"), 0)
                       : token(g, s(u, "name"), n(u, "attack"), 1, s(u, "tribe"));
            (*v)["health"] = 1;
            (*v)["rebornUsed"] = true;
            removeKeyword(v, "REBORN");
            summon(side, v, positions[side][i], false);
          }
          snapshot(s(u, "name") + " was destroyed");
        }
      updateAuras();
      events++;
      if (events > 600)
        break;
    }
  };
  updateAuras();
  snapshot("Combat begins");
  std::array<J, 2> players = {left, right};
  for (int side = 0; side < 2; side++) {
    auto &p = players[side];
    if (!b(p, "powerActive"))
      continue;
    auto k = s(p["hero"], "key");
    if (k == "putricide" && !boards[side].empty())
      buff(boards[side][0], 10, 0, s(p["hero"], "power"));
    if (k == "lichking" && !boards[side].empty())
      keyword(boards[side].back(), "REBORN");
    if (k == "nefarian") {
      auto enemy = boards[1 - side];
      for (auto &u : enemy)
        damage(1 - side, u, 1);
    }
    if (k == "ragnaros" || k == "patches")
      for (int i = 0; i < 2; i++)
        hitRandom(1 - side, k == "ragnaros" ? 8 : 3);
    snapshot(s(p["hero"], "name") + ": " + s(p["hero"], "power"));
    deaths();
  }
  int side = boards[0].size() == boards[1].size() ? (random(g) < .5 ? 0 : 1)
                                                  : (boards[0].size() > boards[1].size() ? 0 : 1),
      cursor[2] = {0, 0}, turns = 0;
  while (!boards[0].empty() && !boards[1].empty() && turns++ < 300 && events < 600) {
    auto all = joined(boards[0], boards[1]);
    if (std::none_of(all.begin(), all.end(), [](auto &u) { return n(u, "attack") > 0; }))
      break;
    U attacker;
    for (int i = 0; i < int(boards[side].size()); i++) {
      auto u = boards[side][(cursor[side] + i) % boards[side].size()];
      if (n(u, "attack") > 0 && alive(u)) {
        attacker = u;
        break;
      }
    }
    if (!attacker) {
      side = 1 - side;
      continue;
    }
    int originalIndex = indexOf(boards[side], attacker),
        swings = has(attacker, "WINDFURY")
                     ? (b(attacker, "golden") && s(attacker, "effect") == "Zapp Slywick" ? 4 : 2)
                     : 1;
    for (int swing = 0; swing < swings && alive(attacker) && contains(boards[side], attacker) &&
                        !boards[1 - side].empty();
         swing++) {
      auto targets = filter(boards[1 - side], alive);
      if (targets.empty())
        break;
      auto taunts = filter(targets, [](auto &u) { return has(u, "TAUNT"); });
      auto choices = taunts.empty() ? targets : taunts;
      if (s(attacker, "effect") == "Zapp Slywick") {
        int min = std::numeric_limits<int>::max();
        for (auto &u : targets)
          min = std::min(min, n(u, "attack"));
        choices = filter(targets, [&](auto &u) { return n(u, "attack") == min; });
      }
      auto target = pick(g, choices);
      int ti = indexOf(boards[1 - side], target), attack = std::max(0, n(attacker, "attack")),
          retaliate = std::max(0, n(target, "attack"));
      Units neighbors;
      if (s(attacker, "effect") == "Cave Hydra" || s(attacker, "effect") == "Foe Reaper 4000")
        for (int j : {ti - 1, ti + 1})
          if (j >= 0 && j < int(boards[1 - side].size()))
            neighbors.push_back(boards[1 - side][j]);
      damage(1 - side, target, attack, has(attacker, "POISONOUS"));
      damage(side, attacker, retaliate, has(target, "POISONOUS"));
      for (auto &u : neighbors)
        damage(1 - side, u, attack, has(attacker, "POISONOUS"));
      if (alive(attacker) && !alive(target) && s(attacker, "effect") == "The Boogeymonster")
        buff(attacker, 2 * factor(attacker), 2 * factor(attacker), s(attacker, "name"));
      if (alive(attacker) && n(target, "health") < 0 &&
          s(attacker, "effect") == "Ironhide Direhorn")
        summon(side, token(g, "Ironhide Runt", 5 * factor(attacker), 5 * factor(attacker), "BEAST"),
               indexOf(boards[side], attacker) + 1);
      for (auto &ally : boards[side])
        if (alive(ally) && s(ally, "effect") == "Festeroot Hulk")
          buff(ally, factor(ally), 0, s(ally, "name"));
      snapshot(s(attacker, "name") + " attacks " + s(target, "name"), n(attacker, "uid"),
               n(target, "uid"));
      deaths();
      updateAuras();
      events++;
    }
    int index = indexOf(boards[side], attacker);
    cursor[side] = index >= 0 ? index + 1 : std::min(originalIndex, int(boards[side].size()));
    side = 1 - side;
  }
  bool capped = turns >= 300 || events >= 600;
  J winner = nullptr;
  if (!boards[0].empty() && boards[1].empty())
    winner = left["id"];
  else if (!boards[1].empty() && boards[0].empty())
    winner = right["id"];
  int winningSide = winner == left["id"] ? 0 : 1, total = 0;
  if (!winner.is_null()) {
    total = n(players[winningSide], "tier");
    for (auto &u : boards[winningSide])
      total += n(u, "tier");
  }
  snapshot(capped ? "Combat halted at the event limit"
           : winner.is_null()
               ? "Combat tied"
               : s(players[winningSide], "name") + " wins · " + std::to_string(total) + " damage");
  return {{"leftId", left["id"]}, {"rightId", right["id"]}, {"winner", winner},
          {"damage", total},      {"frames", frames},       {"capped", capped}};
}
void resolveRound(J &g, bool record = true) const {
  require(s(g, "phase") == "recruit", "Round already resolved");
  for (auto &value : g["players"])
    if (n(value, "health") > 0)
      require(value["discovers"].empty(), "Resolve discoveries before combat");
  for (auto &value : g["players"])
    if (n(value, "health") > 0) {
      Player p(value);
      endRecruit(g, p);
      value = p.save();
    }
  std::vector<int> before, participants;
  for (int i = 0; i < int(g["players"].size()); i++)
    if (!g["players"][i].contains("placement")) {
      before.push_back(i);
      if (n(g["players"][i], "health") > 0)
        participants.push_back(i);
    }
  participants = shuffle(g, participants);
  std::vector<std::pair<int, int>> paired;
  while (participants.size() > 1) {
    int a = participants[0];
    participants.erase(participants.begin());
    int next = 0;
    for (int i = 0; i < int(participants.size()); i++)
      if (participants[i] != n(g["players"][a], "lastOpponent", -99)) {
        next = i;
        break;
      }
    int other = participants[next];
    participants.erase(participants.begin() + next);
    paired.emplace_back(a, other);
  }
  J ghost;
  if (!participants.empty()) {
    int a = participants[0];
    ghost = g.contains("ghost") ? g["ghost"] : g["players"][a];
    if (!g.contains("ghost")) {
      ghost["name"] = "Ghost";
      ghost["board"] = arr();
    }
    ghost["id"] = -1;
    paired.emplace_back(a, -1);
  }
  g["combats"] = arr();
  for (auto [aid, bid] : paired) {
    auto &a = g["players"][aid];
    auto &other = bid < 0 ? ghost : g["players"][bid];
    auto result = combat(g, a, other, record);
    g["combats"].push_back(result);
    a["lastOpponent"] = bid;
    if (bid >= 0)
      other["lastOpponent"] = aid;
    if (!result["winner"].is_null()) {
      auto &loser = result["winner"] == aid ? other : a;
      if (n(loser, "id") >= 0)
        add(loser, "health", -n(result, "damage"));
    }
    emit(
        g, "combat",
        s(a, "name") + " vs " + s(other, "name") + ": " +
            (result["winner"].is_null() ? "tie" : std::to_string(n(result, "damage")) + " damage"));
  }
  auto eliminated = filter(before, [&](int id) { return n(g["players"][id], "health") <= 0; });
  std::stable_sort(eliminated.begin(), eliminated.end(), [&](int a, int b) {
    int ah = n(g["players"][a], "health"), bh = n(g["players"][b], "health");
    return ah == bh ? a < b : ah < bh;
  });
  int place = int(before.size());
  for (int id : eliminated) {
    auto &value = g["players"][id];
    value["placement"] = place--;
    J eliminatedPlayer = value;
    g["ghost"] = eliminatedPlayer;
    Player p(eliminatedPlayer);
    for (auto &u : joined(joined(p.board, p.hand), p.shop))
      returnUnit(g, u);
    g["players"][id]["shop"] = arr();
    emit(g, "elimination",
         s(eliminatedPlayer, "name") + " finished #" +
             std::to_string(n(eliminatedPlayer, "placement")));
  }
  std::vector<int> live;
  for (int i = 0; i < int(g["players"].size()); i++)
    if (n(g["players"][i], "health") > 0)
      live.push_back(i);
  if (live.size() == 1)
    g["players"][live[0]]["placement"] = 1;
  if (!g["combats"].empty()) {
    g["lastCombat"] = g["combats"][0];
    for (auto &c : g["combats"])
      if (n(c, "leftId") == 0 || n(c, "rightId") == 0) {
        g["lastCombat"] = c;
        break;
      }
  } else
    g.erase("lastCombat");
  g["phase"] = live.size() <= 1 ? "finished" : "combat";
  if (record)
    g["history"].push_back(
        {{"round", g["round"]}, {"players", g["players"]}, {"combats", g["combats"]}});
}
