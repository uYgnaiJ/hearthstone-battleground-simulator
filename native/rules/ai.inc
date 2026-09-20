double value(const U &u, const Player &p, const std::string &difficulty) const {
  double score = n(u, "attack") + n(u, "health") * .8 +
                 (has(u, "DIVINE_SHIELD") ? n(u, "attack") * .7 + 3 : 0) +
                 (has(u, "POISONOUS") ? 10 : 0) + (has(u, "WINDFURY") ? n(u, "attack") * .35 : 0);
  if (difficulty == "casual")
    return score;
  int allies = 0, pairs = 0;
  for (auto &v : p.board)
    if (s(u, "tribe") != "NONE" && tribe(v, s(u, "tribe")))
      allies++;
  for (auto &v : joined(p.board, p.hand))
    if (s(v, "cardId") == s(u, "cardId") && !b(v, "golden"))
      pairs++;
  score += allies * 1.3;
  if (pairs >= 2)
    score += 20;
  auto e = s(u, "effect");
  if (e == "Mama Bear" || e == "Lightfang Enforcer" || e == "Mal'Ganis" ||
      e == "Brann Bronzebeard" || e == "Baron Rivendare")
    score += p.board.size() * 2;
  if (e == "Rat Pack" || e == "Savannah Highmane" || e == "Mechano-Egg" || e == "Ghastcoiler")
    score += 8;
  if (difficulty == "expert") {
    score += allies * 1.8;
    if (e == "Wrath Weaver" || e == "Murloc Tidecaller" || e == "Scavenging Hyena" ||
        e == "Junkbot")
      score += allies * 2;
  }
  return score;
}
void recruitAI(J &g, int id) const {
  if (n(g["players"][id], "health") <= 0 ||
      (s(g["players"][id]["hero"], "key") == "afkay" && n(g, "round") < 3))
    return;
  auto run = [&](J command) {
    try {
      act(g, id, command);
      return true;
    } catch (const std::exception &) {
      return false;
    }
  };
  auto difficulty = s(g, "difficulty");
  for (int step = 0; step < 65; step++) {
    Player p(g["players"][id]);
    auto weakest = [&]() {
      int best = 0;
      for (int i = 1; i < int(p.board.size()); i++)
        if (value(p.board[i], p, difficulty) < value(p.board[best], p, difficulty))
          best = i;
      return best;
    };
    auto &d = p.data;
    if (!d["discovers"].empty()) {
      if (p.hand.size() >= 10) {
        if (p.board.size() >= 7)
          run({{"type", "sell"}, {"index", weakest()}});
        if (!run({{"type", "play"}, {"index", 0}}))
          break;
        continue;
      }
      int best = 0, max = -1;
      for (int i = 0; i < int(d["discovers"][0].size()); i++) {
        auto c = card(g, d["discovers"][0][i].get<std::string>());
        require(c, "Invalid discovery card");
        int score = n(*c, "attack") + n(*c, "health") + n(*c, "tier") * 3;
        if (score > max) {
          max = score;
          best = i;
        }
      }
      run({{"type", "discover"}, {"index", best}});
      continue;
    }
    if (!p.hand.empty()) {
      auto u = p.hand[0];
      if (p.board.size() >= 7) {
        run({{"type", "sell"}, {"index", weakest()}});
        p = Player(g["players"][id]);
      }
      auto targets = filter(
          p.board, [&](auto &v) { return tribe(v, s(u, "tribe")) || s(u, "tribe") == "NONE"; });
      std::stable_sort(targets.begin(), targets.end(), [&](auto &a, auto &b) {
        return value(a, p, difficulty) > value(b, p, difficulty);
      });
      J cmd = {{"type", "play"}, {"index", 0}};
      if (!targets.empty())
        cmd["target"] = n(targets[0], "uid");
      if (!run(cmd))
        break;
      continue;
    }
    if (n(d, "bananas") && !p.board.empty()) {
      run({{"type", "banana"}, {"target", n(p.board[0], "uid")}});
      continue;
    }
    if (n(d, "coins") && n(d, "gold") < 10) {
      run({{"type", "coin"}});
      continue;
    }
    if (!b(d, "powerUsed") && !b(d["hero"], "passive") && n(d, "gold") >= n(d["hero"], "cost") &&
        !p.board.empty() && (p.hero() == "yogg" || n(d, "gold") < 3 || n(d, "gold") >= 6)) {
      auto targets = filter(p.board, [](auto &u) { return !has(u, "DIVINE_SHIELD"); });
      std::stable_sort(targets.begin(), targets.end(),
                       [](auto &a, auto &b) { return n(a, "attack") > n(b, "attack"); });
      J cmd = {{"type", "power"}};
      if (!targets.empty())
        cmd["target"] = n(targets[0], "uid");
      if (run(cmd))
        continue;
    }
    int level = std::array{0, 2, 5, 7, 9, 12, 99}.at(n(d, "tier"));
    if (n(d, "tier") < 6 && n(g, "round") >= level && n(d, "gold") >= n(d, "upgrade") &&
        (int(p.board.size()) >= std::min(5, n(g, "round")) || n(d, "upgrade") <= 1)) {
      run({{"type", "upgrade"}});
      g["players"][id]["aiReason"] = "Invested in a higher tavern tier for stronger shops.";
      continue;
    }
    if (n(d, "gold") < 3)
      break;
    int best = -1;
    double bestValue = -std::numeric_limits<double>::infinity(),
           weak = std::numeric_limits<double>::infinity();
    for (int i = 0; i < int(p.shop.size()); i++) {
      double score = value(p.shop[i], p, difficulty);
      if (score > bestValue) {
        bestValue = score;
        best = i;
      }
    }
    for (auto &u : p.board)
      weak = std::min(weak, value(u, p, difficulty));
    if (best >= 0 && (p.board.size() < 7 || bestValue > weak + 2)) {
      auto name = s(p.shop[best], "name");
      run({{"type", "buy"}, {"index", best}});
      std::ostringstream out;
      out << std::fixed << std::setprecision(1) << bestValue;
      g["players"][id]["aiReason"] = "Bought " + name + ": value " + out.str() +
                                     ", considering stats, keywords, tribe synergy and triples.";
      continue;
    }
    if (!run({{"type", "refresh"}}))
      break;
  }
  Player p(g["players"][id]);
  auto score = [](const U &u) {
    auto e = s(u, "effect");
    return n(u, "attack") + (e == "Spawn of N'Zoth" ? 40 : 0) + (e == "Selfless Hero" ? -15 : 0) -
           ((e == "Baron Rivendare" || e == "Mama Bear" || e == "Scavenging Hyena" ||
             e == "Soul Juggler" || e == "Junkbot")
                ? 30
                : 0);
  };
  std::stable_sort(p.board.begin(), p.board.end(),
                   [&](auto &a, auto &b) { return score(a) > score(b); });
  for (auto &u : p.shop) {
    int count = 0;
    for (auto &v : joined(p.board, p.hand))
      if (s(v, "cardId") == s(u, "cardId") && !b(v, "golden"))
        count++;
    if (count >= 2)
      p.data["frozen"] = true;
  }
  g["players"][id] = p.save();
}
