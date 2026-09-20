import type { Game } from "./types";
import { validateCards, contentHash } from "./engine";
export function parseGame(value: unknown): Game {
  const g = value as Game;
  if (
    !g ||
    g.version !== 1 ||
    !["recruit", "combat", "finished"].includes(g.phase) ||
    !Array.isArray(g.players) ||
    g.players.length !== 8 ||
    !Array.isArray(g.history) ||
    !Array.isArray(g.log) ||
    !Number.isInteger(g.rng) ||
    !Number.isInteger(g.round) ||
    g.round < 1 ||
    g.round > 1000
  )
    throw Error("Unsupported or damaged save file");
  validateCards(g.cards);
  if (contentHash(g.cards) !== g.contentHash)
    throw Error("Content checksum does not match the save");
  for (const p of g.players) {
    if (
      !Array.isArray(p.board) ||
      p.board.length > 7 ||
      !Array.isArray(p.hand) ||
      p.hand.length > 10 ||
      !Number.isFinite(p.health) ||
      p.tier < 1 ||
      p.tier > 6 ||
      !p.hero ||
      !Array.isArray(p.shop) ||
      !Array.isArray(p.discovers)
    )
      throw Error("Invalid player state");
    for (const u of [...p.board, ...p.hand, ...p.shop])
      if (
        !Number.isFinite(u.attack) ||
        !Number.isFinite(u.health) ||
        !Array.isArray(u.keywords) ||
        !Number.isInteger(u.uid)
      )
        throw Error("Invalid minion state");
  }
  return g;
}
