import { newGame, HEROES, beginRound } from "../src/game/engine";
import { recruitAI } from "../src/game/ai";
import { resolveRound } from "../src/game/combat";
import fs from "node:fs";
const count = Number(process.argv[2] || 1000);
let rounds = 0,
  combats = 0;
const started = Date.now();
for (let seed = 1; seed <= count; seed++) {
  const g = newGame(
    seed,
    HEROES[seed % HEROES.length].key,
    undefined,
    seed % 3 === 0 ? "expert" : seed % 3 === 1 ? "standard" : "casual",
  );
  try {
    for (let turn = 0; turn < 100 && g.phase !== "finished"; turn++) {
      for (const p of g.players) recruitAI(g, p);
      resolveRound(g, false);
      rounds++;
      combats += g.combats.length;
      if (g.combats.some((c) => c.capped))
        throw Error("Combat guard triggered");
      for (const p of g.players) {
        if (
          p.board.length > 7 ||
          p.hand.length > 10 ||
          p.gold < 0 ||
          p.tier < 1 ||
          p.tier > 6
        )
          throw Error("Player invariant");
        for (const u of [...p.board, ...p.hand])
          if (!Number.isFinite(u.attack) || !Number.isFinite(u.health))
            throw Error("Invalid stats");
      }
      if (Object.values(g.pool).some((n) => n < 0))
        throw Error("Negative pool");
      if ((g.phase as string) !== "finished") beginRound(g);
    }
    if (g.phase !== "finished") throw Error("Match did not terminate");
    if (g.players.filter((p) => p.placement === 1).length !== 1)
      throw Error("Invalid winner");
  } catch (error) {
    fs.mkdirSync("test-results", { recursive: true });
    fs.writeFileSync(
      `test-results/failed-seed-${seed}.json`,
      JSON.stringify(g),
    );
    throw error;
  }
  if (seed % 100 === 0) console.log(`${seed}/${count} matches complete`);
}
const report = {
  matches: count,
  rounds,
  combats,
  seconds: (Date.now() - started) / 1000,
  failures: 0,
};
fs.mkdirSync("test-results", { recursive: true });
fs.writeFileSync("test-results/soak.json", JSON.stringify(report, null, 2));
console.log(report);
