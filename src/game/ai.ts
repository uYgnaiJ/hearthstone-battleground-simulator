import type { Game, Player, Unit, Command } from "./types";
import { act, has, isTribe, makeUnit } from "./engine";
export function value(u: Unit, p: Player, difficulty: Game["difficulty"]) {
  let s =
    u.attack +
    u.health * 0.8 +
    (has(u, "DIVINE_SHIELD") ? u.attack * 0.7 + 3 : 0) +
    (has(u, "POISONOUS") ? 10 : 0) +
    (has(u, "WINDFURY") ? u.attack * 0.35 : 0);
  if (difficulty === "casual") return s;
  const allies = p.board.filter(
    (v) => isTribe(v, u.tribe) && u.tribe !== "NONE",
  ).length;
  s += allies * 1.3;
  if (
    [...p.board, ...p.hand].filter((v) => v.cardId === u.cardId && !v.golden)
      .length >= 2
  )
    s += 20;
  if (
    [
      "Mama Bear",
      "Lightfang Enforcer",
      "Mal'Ganis",
      "Brann Bronzebeard",
      "Baron Rivendare",
    ].includes(u.effect)
  )
    s += p.board.length * 2;
  if (
    ["Rat Pack", "Savannah Highmane", "Mechano-Egg", "Ghastcoiler"].includes(
      u.effect,
    )
  )
    s += 8;
  if (difficulty === "expert") {
    s += allies * 1.8;
    if (
      [
        "Wrath Weaver",
        "Murloc Tidecaller",
        "Scavenging Hyena",
        "Junkbot",
      ].includes(u.effect)
    )
      s += allies * 2;
  }
  return s;
}
export function recruitAI(g: Game, p: Player) {
  if (p.health <= 0 || (p.hero.key === "afkay" && g.round < 3)) return;
  const run = (cmd: Command) => {
    try {
      act(g, p.id, cmd);
      return true;
    } catch {
      return false;
    }
  };
  for (let step = 0; step < 65; step++) {
    if (p.discovers.length) {
      if (p.hand.length >= 10) {
        if (p.board.length >= 7)
          run({
            type: "sell",
            index: p.board.reduce(
              (best, u, i) =>
                value(u, p, g.difficulty) <
                value(p.board[best], p, g.difficulty)
                  ? i
                  : best,
              0,
            ),
          });
        if (!run({ type: "play", index: 0 })) break;
        continue;
      }
      const options = p.discovers[0].map((id) => {
        const c = g.cards.find((c) => c.id === id)!;
        return c.attack + c.health + c.tier * 3;
      });
      run({ type: "discover", index: options.indexOf(Math.max(...options)) });
      continue;
    }
    if (p.hand.length) {
      const u = p.hand[0];
      if (p.board.length >= 7) {
        const scores = p.board.map((v) => value(v, p, g.difficulty));
        const weakest = scores.indexOf(Math.min(...scores));
        run({ type: "sell", index: weakest });
      }
      const target = p.board
        .filter((v) => isTribe(v, u.tribe) || u.tribe === "NONE")
        .sort(
          (a, b) => value(b, p, g.difficulty) - value(a, p, g.difficulty),
        )[0];
      if (!run({ type: "play", index: 0, target: target?.uid })) break;
      continue;
    }
    if (p.bananas && p.board.length) {
      run({ type: "banana", target: p.board[0].uid });
      continue;
    }
    if (p.coins && p.gold < 10) {
      run({ type: "coin" });
      continue;
    }
    if (
      !p.powerUsed &&
      !p.hero.passive &&
      p.gold >= p.hero.cost &&
      p.board.length &&
      (p.hero.key === "yogg" || p.gold < 3 || p.gold >= 6)
    ) {
      const target = p.board
        .filter((u) => !has(u, "DIVINE_SHIELD"))
        .sort((a, b) => b.attack - a.attack)[0];
      if (run({ type: "power", target: target?.uid })) continue;
    }
    const levelRound = [0, 2, 5, 7, 9, 12, 99][p.tier];
    if (
      p.tier < 6 &&
      g.round >= levelRound &&
      p.gold >= p.upgrade &&
      (p.board.length >= Math.min(5, g.round) || p.upgrade <= 1)
    ) {
      run({ type: "upgrade" });
      p.aiReason = "Invested in a higher tavern tier for stronger shops.";
      continue;
    }
    if (p.gold < 3) break;
    const ranked = p.shop
      .map((u, i) => ({ u, i, s: value(u, p, g.difficulty) }))
      .sort((a, b) => b.s - a.s);
    const best = ranked[0];
    const weakest = Math.min(...p.board.map((u) => value(u, p, g.difficulty)));
    if (best && (p.board.length < 7 || best.s > weakest + 2)) {
      run({ type: "buy", index: best.i });
      p.aiReason = `Bought ${best.u.name}: value ${best.s.toFixed(1)}, considering stats, keywords, tribe synergy and triples.`;
      continue;
    }
    if (!run({ type: "refresh" })) break;
  }
  // Put immediate damage and early deathrattles first; keep scaling support behind them.
  p.board.sort((a, b) => {
    const score = (u: Unit) =>
      u.attack +
      (u.effect === "Spawn of N'Zoth" ? 40 : 0) +
      (u.effect === "Selfless Hero" ? -15 : 0) -
      ([
        "Baron Rivendare",
        "Mama Bear",
        "Scavenging Hyena",
        "Soul Juggler",
        "Junkbot",
      ].includes(u.effect)
        ? 30
        : 0);
    return score(b) - score(a);
  });
  if (
    p.shop.some(
      (u) =>
        [...p.board, ...p.hand].filter(
          (v) => v.cardId === u.cardId && !v.golden,
        ).length >= 2,
    )
  )
    p.frozen = true;
}
