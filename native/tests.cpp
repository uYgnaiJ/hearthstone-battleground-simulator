#include "rules/host.hpp"
#include <iostream>
using namespace bg;
namespace {
void check(bool ok, const std::string &message) {
  require(ok, "Test failed: " + message);
}
void clean(J &value) {
  if (value.is_object()) {
    value.erase("createdAt");
    for (auto &v : value)
      clean(v);
  } else if (value.is_array())
    for (auto &v : value)
      clean(v);
}
std::string digest(J value) {
  clean(value);
  auto sorted = nlohmann::json::parse(value.dump());
  return contentHash(J::parse(sorted.dump()));
}
struct Suite {
  Content content{"src/data"};
  Rules rules{content};
  std::string id(const std::string &name) {
    for (auto &c : content.cards)
      if (s(c, "name") == name)
        return s(c, "id");
    throw std::runtime_error("Missing card " + name);
  }
  J setup() {
    auto g = rules.newGame(123, "patchwerk", content.cards);
    for (auto &p : g["players"]) {
      p["board"] = arr();
      p["hand"] = arr();
    }
    return g;
  }
  U minion(J &g, const std::string &name, bool golden = false) {
    return rules.makeUnit(g, id(name), golden);
  }
  void parity() {
    auto fixtures = readJson("native/tests/legacy-parity.json");
    J match;
    int activeSeed = -1;
    for (auto &f : fixtures) {
      auto op = s(f, "op");
      J actual;
      if (op == "new")
        actual = rules.newGame(n(f, "seed"), s(f, "hero"), content.cards);
      else if (op == "combat") {
        auto g = setup();
        g["players"][0]["board"] =
            serialize({rules.makeUnit(g, s(f, "cardId"), b(f, "golden")), minion(g, "Mecharoo")});
        g["players"][1]["board"] =
            serialize({rules.token(g, "Target", 8, 15), rules.token(g, "Other", 3, 4)});
        auto result = rules.combat(g, g["players"][0], g["players"][1]);
        actual = {{"game", g}, {"result", result}};
      } else if (op == "action") {
        auto g = setup();
        g["players"][0]["board"] =
            serialize({minion(g, "Brann Bronzebeard"), rules.token(g, "Amalgam", 1, 1, "ALL"),
                       minion(g, "Murloc Tidecaller"), minion(g, "Wrath Weaver")});
        g["players"][0]["hand"] = serialize({rules.makeUnit(g, s(f, "cardId"), b(f, "golden"))});
        J steps = arr();
        rules.act(g, 0, {{"type", "play"}, {"index", 0}}, [&](auto &kind, auto &p, int source) {
          steps.push_back({{"kind", kind}, {"player", p.save()}, {"source", source}});
        });
        actual = {{"game", g}, {"steps", steps}};
      } else if (op == "power") {
        auto g = setup();
        for (auto &h : content.heroes)
          if (s(h, "key") == s(f, "hero"))
            g["players"][0]["hero"] = h;
        g["players"][0]["gold"] = 10;
        g["players"][0]["board"] =
            serialize({rules.token(g, "Amalgam", 3, 6, "ALL"), minion(g, "Imp Gang Boss")});
        rules.act(g, 0, {{"type", "power"}, {"target", g["players"][0]["board"][0]["uid"]}});
        auto result = rules.combat(g, g["players"][0], g["players"][1]);
        actual = {{"game", g}, {"result", result}};
      } else if (op == "round") {
        int seed = n(f, "seed");
        if (seed != activeSeed) {
          activeSeed = seed;
          match = rules.newGame(
              seed, s(content.heroes[seed % content.heroes.size()], "key"), content.cards,
              std::array<std::string, 3>{"casual", "standard", "expert"}[seed % 3]);
        }
        check(n(match, "round") == n(f, "round"), "fixture round sequence");
        for (int i = 0; i < 8; i++)
          rules.recruitAI(match, i);
        rules.resolveRound(match, false);
        if (s(match, "phase") != "finished")
          rules.beginRound(match);
        actual = match;
      }
      auto hash = digest(actual);
      if (hash != s(f, "hash")) {
        std::ofstream("test-results/parity-failure.json") << actual.dump(2);
        throw std::runtime_error("Parity mismatch: " + s(f, "name") + " expected " + s(f, "hash") +
                                 " got " + hash);
      }
    }
    std::cout << fixtures.size() << " legacy parity cases passed\n";
  }
  void regressions() {
    auto g = setup();
    Player p(g["players"][0]);
    p.board = {minion(g, "Murloc Tidehunter"), minion(g, "Murloc Warleader"),
               minion(g, "Murloc Warleader", true)};
    buff(p.board[0], 1, 1, "Banana");
    buff(p.board[0], 1, 1, "Banana");
    auto view = rules.recruitPresentation(p.save());
    check(n(view["board"][0], "attack") == 10 && view["board"][0]["auraEffects"].size() == 2,
          "stacked auras");
    check(n(p.board[0], "attack") == 4, "aura not persisted");
    p.board.resize(1);
    check(n(rules.recruitPresentation(p.save())["board"][0], "attack") == 4,
          "aura removal preserves buffs");
    g = setup();
    rules.act(g, 0, {{"type", "buy"}, {"index", 0}});
    auto kept = g["players"][0]["shop"];
    rules.act(g, 0, {{"type", "freeze"}});
    rules.beginRound(g);
    check(g["players"][0]["shop"].size() == 3 && g["players"][0]["shop"][0] == kept[0] &&
              !b(g["players"][0], "frozen"),
          "frozen refill");
    g = setup();
    p = Player(g["players"][0]);
    auto hostId = id("Harvest Golem"), magnetId = id("Replicating Menace");
    int before = n(g["pool"], magnetId);
    add(g["pool"], magnetId, -1);
    p.board = {rules.makeUnit(g, hostId)};
    p.hand = {rules.makeUnit(g, magnetId)};
    g["players"][0] = p.save();
    rules.act(
        g, 0,
        {{"type", "play"}, {"index", 0}, {"target", n(p.board[0], "uid")}, {"magnetic", true}});
    p = Player(g["players"][0]);
    p.hand = {rules.makeUnit(g, hostId), rules.makeUnit(g, hostId)};
    rules.triple(g, p);
    check(b(p.hand[0], "golden") && n((*p.hand[0])["absorbed"], magnetId) == 1,
          "magnetic triple accounting");
    g["players"][0] = p.save();
    rules.act(g, 0, {{"type", "play"}, {"index", 0}});
    rules.act(g, 0, {{"type", "discover"}, {"index", 0}});
    rules.act(g, 0, {{"type", "sell"}, {"index", 0}});
    check(n(g["pool"], magnetId) == before, "magnetic pool return");
    g = setup();
    g["players"][0]["board"] = serialize({rules.token(g, "Reborn", 1, 1, "NONE", J{"REBORN"})});
    g["players"][1]["board"] = serialize({rules.token(g, "A", 1, 2)});
    auto result = rules.combat(g, g["players"][0], g["players"][1]);
    bool reborn = false;
    for (auto &frame : result["frames"])
      for (auto &u : frame["left"])
        if (b(u, "rebornUsed") && n(u, "health") == 1)
          reborn = true;
    check(reborn && result["winner"].is_null(), "reborn once at one health");
    g = setup();
    g["players"][0]["board"] = serialize({rules.token(g, "Poison", 1, 1, "BEAST", J{"POISONOUS"})});
    g["players"][1]["board"] =
        serialize({rules.token(g, "Shield", 2, 8, "NONE", J{"DIVINE_SHIELD"})});
    result = rules.combat(g, g["players"][0], g["players"][1]);
    check(result["winner"] == 1 && n(result["frames"].back()["right"][0], "health") == 8,
          "shield blocks poison");
    auto bad = content.cards;
    bad[0]["health"] = -1;
    bool rejected = false;
    try {
      validateCards(bad, content);
    } catch (...) {
      rejected = true;
    }
    check(rejected, "invalid pack rejected");
    auto saved = rules.newGame(9, "patchwerk", content.cards);
    check(parseGame(J::parse(saved.dump()), content) == saved, "save round trip");
    saved["contentHash"] = "bad";
    rejected = false;
    try {
      parseGame(saved, content);
    } catch (...) {
      rejected = true;
    }
    check(rejected, "corrupt save checksum rejected");
    std::cout << "Gameplay regression checks passed\n";
  }
  void host() {
    auto dir = std::filesystem::absolute("test-results") /
               ("native-host-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto instance = std::make_unique<Host>("src/data", dir);
    auto call = [&](std::string type, J args = J::object()) {
      args["type"] = type;
      auto m = instance->handle(args);
      check(b(m, "ok"), type + ": " + s(m, "error"));
      return m;
    };
    auto m = call("boot");
    check(m["cards"].size() == 80 && m["heroes"].size() == 23, "boot content");
    call("new", {{"seed", 35747}, {"hero", "curator"}});
    call("debug", {{"command", "give " + id("Murloc Warleader")}});
    m = call("action", {{"command", {{"type", "play"}, {"index", 0}}}});
    check(n(m["game"]["players"][0]["board"][0], "attack") == 3, "host aura presentation");
    check(n(readJson(dir / "save.json")["players"][0]["board"][0], "attack") == 1,
          "host stores base stats");
    instance = std::make_unique<Host>("src/data", dir);
    m = call("boot");
    check(n(m["game"]["players"][0]["board"][0], "attack") == 3, "restart reapplies aura once");
    call("action", {{"command", {{"type", "sell"}, {"index", 1}}}});
    m = call("new", {{"seed", 35747}, {"hero", "curator"}});
    auto hash = m["game"]["contentHash"];
    call("action", {{"command", {{"type", "buy"}, {"index", 0}}}});
    call("action", {{"command", {{"type", "play"}, {"index", 0}}}});
    call("end");
    m = call("advance");
    check(n(m["game"], "round") == 2, "host round advance");
    auto c = content.cards[0];
    add(c, "attack", 10);
    m = call("card.save", {{"card", c}});
    check(m["game"]["contentHash"] == hash, "editing leaves active pack pinned");
    call("pack.export");
    call("debug", {{"command", "gold 10"}});
    m = call("rewind");
    check(n(m["game"]["players"][0], "gold") != 10, "undo");
    check(!b(instance->handle({{"type", "save.import"}, {"path", (dir / "cards.json").string()}}),
             "ok"),
          "invalid import rejected");
    call("lab.new");
    call("lab.add", {{"side", 0}, {"cardId", c["id"]}});
    call("lab.add", {{"side", 1}, {"cardId", c["id"]}, {"golden", true}});
    m = call("lab.odds", {{"samples", 100}, {"seed", 42}});
    check(n(m["odds"], "wins") + n(m["odds"], "ties") + n(m["odds"], "losses") == 100, "lab odds");
    m = call("lab.run", {{"seed", 42}});
    check(!m["replay"]["result"]["frames"].empty(), "lab replay");
    call("lab.export");
    call("lab.import", {{"path", (dir / "scenario.json").string()}});
    call("save.export");
    instance = std::make_unique<Host>("src/data", dir);
    m = call("boot");
    check(n(m["game"], "round") == 2 && m["cards"][0]["attack"] == c["attack"],
          "save and edits survive restart");
    for (int turn = 0; turn < 100 && s(m["game"], "phase") != "finished"; turn++)
      m = call("autoplay");
    check(s(m["game"], "phase") == "finished", "complete host match");
    m = call("history");
    check(m["history"].size() == 1, "history entry");
    auto match = m["history"][0];
    m = call("replay", {{"matchId", match["id"]}, {"round", n(match, "rounds") - 1}});
    check(n(m["replay"], "round") == n(match, "rounds"), "finished replay");
    m = call("card.reset", {{"cardId", c["id"]}});
    check(n(m["cards"][0], "attack") == n(c, "attack") - 10, "card reset");
    call("settings", {{"settings", {{"volume", .2}, {"speed", 2}}}});
    std::ofstream(dir / "save.json") << "broken";
    instance = std::make_unique<Host>("src/data", dir);
    m = call("boot");
    check(!m["game"].is_null(), "backup recovery");
    std::cout << "Native host integration passed\n";
  }
  void soak(int count) {
    for (int seed = 1; seed <= count; seed++) {
      auto g =
          rules.newGame(seed, s(content.heroes[seed % content.heroes.size()], "key"), content.cards,
                        std::array<std::string, 3>{"casual", "standard", "expert"}[seed % 3]);
      for (int turn = 0; turn < 80 && s(g, "phase") != "finished"; turn++) {
        for (int i = 0; i < 8; i++)
          rules.recruitAI(g, i);
        rules.resolveRound(g, false);
        for (auto &p : g["players"])
          check(p["board"].size() <= 7 && p["hand"].size() <= 10,
                "zone limits seed " + std::to_string(seed));
        for (auto &c : g["combats"])
          check(!b(c, "capped"), "combat limit seed " + std::to_string(seed));
        if (s(g, "phase") != "finished")
          rules.beginRound(g);
      }
      check(s(g, "phase") == "finished", "unfinished seed " + std::to_string(seed));
      int winners = 0;
      for (auto &p : g["players"])
        if (n(p, "placement") == 1)
          winners++;
      check(winners == 1, "one winner");
      if (seed % 100 == 0)
        std::cout << seed << " matches passed" << std::endl;
    }
    std::cout << count << " full matches passed\n";
  }
};
} // namespace
int main(int argc, char **argv) {
  try {
    Suite suite;
    if (argc > 1 && std::string(argv[1]) == "--soak") {
      suite.soak(argc > 2 ? std::stoi(argv[2]) : 1000);
      return 0;
    }
    suite.parity();
    suite.regressions();
    suite.host();
    std::cout << "All native checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
