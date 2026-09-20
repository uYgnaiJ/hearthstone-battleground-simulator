import rawCards from "../data/cards.json";
import rawHeroes from "../data/heroes.json";
import tokenArt from "../data/token-art.json";
import manifest from "../data/manifest.json";
import type { Card, Hero, Game, Player, Unit, Command, Keyword } from "./types";
export const BASE_CARDS = rawCards as Card[];
export const HEROES = rawHeroes as Hero[];
export { manifest };
export const clone = <T>(v: T): T => structuredClone(v);
export function random(g: Pick<Game, "rng">) {
  let x = g.rng | 0;
  x ^= x << 13;
  x ^= x >>> 17;
  x ^= x << 5;
  g.rng = x >>> 0;
  return g.rng / 4294967296;
}
export function pick<T>(g: Pick<Game, "rng">, a: T[]): T {
  if (!a.length) throw Error("No eligible choices");
  return a[Math.floor(random(g) * a.length)];
}
export function shuffle<T>(g: Pick<Game, "rng">, a: T[]): T[] {
  const out = [...a];
  for (let i = out.length - 1; i > 0; i--) {
    const j = Math.floor(random(g) * (i + 1));
    [out[i], out[j]] = [out[j], out[i]];
  }
  return out;
}
export const isTribe = (u: Pick<Unit, "tribe">, t: string) =>
  u.tribe === t || u.tribe === "ALL";
export const factor = (u: Unit) => (u.golden ? 2 : 1);
export const has = (u: Unit, k: Keyword) => u.keywords.includes(k);
export function buff(u: Unit, a: number, h: number, source = "Stat bonus") {
  const appliedAttack = Math.max(0, u.attack + a) - u.attack;
  if (appliedAttack || h) {
    u.enchantments ??= [];
    const prior = u.enchantments.find(e => e.source === source);
    if (prior) { prior.attack += appliedAttack; prior.health += h; }
    else u.enchantments.push({source, attack: appliedAttack, health: h});
  }
  u.attack = Math.max(0, u.attack + a);
  u.health += h;
  u.maxHealth += h;
}
export function keyword(u: Unit, k: Keyword) {
  if (!has(u, k)) u.keywords.push(k);
}
export function emit(g: Game, kind: string, text: string) {
  g.log.push({ seq: (g.log.at(-1)?.seq || 0) + 1, round: g.round, kind, text });
  if (g.log.length > 1500) g.log.splice(0, 500);
}
export function makeUnit(
  g: Game,
  id: string,
  golden = false,
  copies = 1,
): Unit {
  const c = g.cards.find((c) => c.id === id);
  if (!c) throw Error("Unknown card " + id);
  const f = golden ? 2 : 1;
  return {
    uid: g.nextId++,
    cardId: id,
    name: c.name,
    attack: c.attack * f,
    health: c.health * f,
    maxHealth: c.health * f,
    tribe: c.tribe,
    keywords: [...c.keywords],
    golden,
    tier: c.tier,
    effect: c.effect,
    art: c.art,
    copies,
    extraDeaths: [],
    shifter: c.effect === "Shifter Zerus",
  };
}
export function token(
  g: Game,
  name: string,
  a: number,
  h: number,
  tribe = "NONE",
  keys: Keyword[] = [],
): Unit {
  return {
    uid: g.nextId++,
    cardId: "token:" + name,
    name,
    attack: a,
    health: h,
    maxHealth: h,
    tribe,
    keywords: keys,
    golden: false,
    tier: 1,
    effect: "",
    art: (tokenArt as Record<string,string>)[name] || "",
    copies: 0,
    extraDeaths: [],
  };
}
export function returnUnit(g: Game, u: Unit) {
  if (u.copies && g.pool[u.cardId] !== undefined) g.pool[u.cardId] += u.copies;
  for (const [id,copies] of Object.entries(u.absorbed || {})) {
    if (g.pool[id] !== undefined) g.pool[id] += copies;
  }
}
export function refresh(g: Game, p: Player, higher = false, fillOnly = false) {
  if (!fillOnly) {
    p.shop.forEach((u) => returnUnit(g, u));
    p.shop = [];
  }
  const count = [0, 3, 4, 4, 5, 5, 6][p.tier];
  for (let i = p.shop.length; i < count; i++) {
    const eligible = g.cards.filter(
      (c) =>
        c.enabled &&
        g.pool[c.id] > 0 &&
        (higher && i === count - 1
          ? c.tier === Math.min(6, p.tier + 1)
          : c.tier <= p.tier),
    );
    const total = eligible.reduce((n, c) => n + g.pool[c.id], 0);
    if (!total) continue;
    let roll = random(g) * total;
    const c = eligible.find((c) => (roll -= g.pool[c.id]) < 0)!;
    g.pool[c.id]--;
    const u = makeUnit(g, c.id);
    if (p.hero.key === "millificent" && isTribe(u, "MECHANICAL")) buff(u, 1, 1, p.hero.power);
    p.shop.push(u);
  }
  p.frozen = false;
}
export function newGame(
  seed: number,
  heroKey: string,
  cards = BASE_CARDS,
  difficulty: Game["difficulty"] = "standard",
): Game {
  const g: Game = {
    version: 1,
    seed: seed >>> 0 || 1,
    rng: seed >>> 0 || 1,
    nextId: 1,
    round: 1,
    phase: "recruit",
    players: [],
    pool: Object.fromEntries(
      cards.filter((c) => c.enabled).map((c) => [c.id, c.pool]),
    ),
    cards: clone(cards),
    contentHash: contentHash(cards),
    log: [],
    combats: [],
    modified: false,
    difficulty,
    createdAt: new Date().toISOString(),
    history: [],
  };
  const hero = HEROES.find((h) => h.key === heroKey) || HEROES[0];
  const choices = [
    hero,
    ...shuffle(
      g,
      HEROES.filter((h) => h.key !== hero.key),
    ).slice(0, 7),
  ];
  const names = [
    "You",
    "Ember",
    "Moss",
    "Copper",
    "Juniper",
    "Flint",
    "Willow",
    "Ash",
  ];
  g.players = choices.map((h, id) => ({
    id,
    name: names[id],
    hero: clone(h),
    health: h.health,
    tier: 1,
    gold: 3,
    upgrade: h.key === "bartendotron" ? 4 : 5,
    board: [],
    hand: [],
    shop: [],
    frozen: false,
    powerUsed: false,
    powerActive: false,
    played: {},
    coins: 0,
    bananas: 0,
    ratTribe: "BEAST",
    discovers: [],
    aiReason: "",
  }));
  for (const p of g.players) {
    if (p.hero.key === "curator")
      p.board.push(token(g, "Amalgam", 1, 1, "ALL"));
    if (p.hero.key === "afkay") p.gold = 0;
    refresh(g, p);
  }
  emit(g, "system", `Origins lobby created · seed ${g.seed}`);
  return g;
}
export function contentHash(cards: Card[]) {
  let h = 2166136261;
  for (const c of JSON.stringify(cards)) {
    h ^= c.charCodeAt(0);
    h = Math.imul(h, 16777619);
  }
  return (h >>> 0).toString(16);
}
export function summonRecruit(
  g: Game,
  p: Player,
  u: Unit,
  position = p.board.length,
) {
  if (p.board.length >= 7) return false;
  p.board.splice(position, 0, u);
  for (const ally of p.board) {
    if (ally.uid === u.uid) continue;
    const f = factor(ally);
    if (isTribe(u, "BEAST")) {
      if (ally.effect === "Pack Leader") buff(u, 3 * f, 0, ally.name);
      if (ally.effect === "Mama Bear") buff(u, 5 * f, 5 * f, ally.name);
    }
    if (isTribe(u, "MURLOC") && ally.effect === "Murloc Tidecaller")
      buff(ally, f, 0, ally.name);
    if (isTribe(u, "MECHANICAL") && ally.effect === "Cobalt Guardian")
      keyword(ally, "DIVINE_SHIELD");
  }
  return true;
}
function discover(g: Game, p: Player, tier: number, tribe?: string) {
  const cs = shuffle(
    g,
    g.cards.filter(
      (c) =>
        c.enabled &&
        (tribe ? isTribe(c, tribe) : c.tier === tier) &&
        g.pool[c.id] > 0,
    ),
  ).slice(0, 3);
  if (cs.length) p.discovers.push(cs.map((c) => c.id));
}
export function triple(g: Game, p: Player) {
  for (const c of g.cards) {
    const units = [...p.board, ...p.hand].filter(
      (u) => u.cardId === c.id && !u.golden,
    );
    if (units.length < 3) continue;
    const found = units.slice(0, 3);
    const gold = makeUnit(
      g,
      c.id,
      true,
      found.reduce((n, u) => n + u.copies, 0),
    );
    for (const u of found) {
      buff(gold, u.attack - c.attack, u.maxHealth - c.health, "Inherited triple buffs");
      u.keywords.forEach((k) => keyword(gold, k));
      gold.extraDeaths.push(...u.extraDeaths);
      for (const [id,copies] of Object.entries(u.absorbed || {})) {
        gold.absorbed ||= {};
        gold.absorbed[id] = (gold.absorbed[id] || 0) + copies;
      }
    }
    p.board = p.board.filter((u) => !found.includes(u));
    p.hand = p.hand.filter((u) => !found.includes(u));
    p.hand.push(gold);
    emit(g, "triple", `${p.name} combined a golden ${c.name}`);
    triple(g, p);
    return;
  }
}
function pickOther(
  g: Game,
  p: Player,
  u: Unit,
  tribe?: string,
  target?: number,
) {
  const candidates = p.board.filter(
    (v) => v.uid !== u.uid && (!tribe || isTribe(v, tribe)),
  );
  return (
    candidates.find((v) => v.uid === target) ||
    (candidates.length ? pick(g, candidates) : undefined)
  );
}
function battlecry(g: Game, p: Player, u: Unit, target?: number) {
  const f = factor(u);
  const other = p.board.filter((v) => v.uid !== u.uid);
  const def = g.cards.find((c) => c.id === u.cardId);
  const customA = def?.buffAttack ?? 1,
    customH = def?.buffHealth ?? 1;
  const give = (
    tribe: string | undefined,
    a: number,
    h: number,
    taunt = false,
  ) => {
    const t = pickOther(g, p, u, tribe, target);
    if (t) {
      buff(t, a * f * customA, h * f * customH, u.name);
      if (taunt) keyword(t, "TAUNT");
    }
  };
  const all = (
    tribe: string | undefined,
    a: number,
    h: number,
    taunts = false,
  ) =>
    other
      .filter(
        (v) => (!tribe || isTribe(v, tribe)) && (!taunts || has(v, "TAUNT")),
      )
      .forEach((v) => buff(v, a * f * customA, h * f * customH, u.name));
  const summons = (name: string, a: number, h: number, t: string) => {
    const mult = Math.max(
      1,
      ...other
        .filter((v) => v.effect === "Khadgar")
        .map((v) => (v.golden ? 3 : 2)),
    );
    for (let n = 0; n < mult; n++)
      summonRecruit(g, p, token(g, name, a * f, h * f, t));
  };
  switch (u.effect) {
    case "Alleycat":
      summons("Tabbycat", 1, 1, "BEAST");
      break;
    case "Murloc Tidehunter":
      summons("Murloc Scout", 1, 1, "MURLOC");
      break;
    case "Vulgar Homunculus":
      if (!p.board.some((v) => v.effect === "Mal'Ganis")) p.health -= 2;
      break;
    case "Rockpool Hunter":
      give("MURLOC", 1, 1);
      break;
    case "Nathrezim Overseer":
      give("DEMON", 2, 2);
      break;
    case "Metaltooth Leaper":
      all("MECHANICAL", 2, 0);
      break;
    case "Coldlight Seer":
      all("MURLOC", 0, 2);
      break;
    case "Crystalweaver":
      all("DEMON", 1, 1);
      break;
    case "Houndmaster":
      give("BEAST", 2, 2, true);
      break;
    case "Screwjank Clunker":
      give("MECHANICAL", 2, 2);
      break;
    case "Virmen Sensei":
      give("BEAST", 2, 2);
      break;
    case "Strongshell Scavenger":
      all(undefined, 2, 2, true);
      break;
    case "Zoobot":
    case "Menagerie Magician":
      for (const t of ["BEAST", "DRAGON", "MURLOC"])
        give(t, u.effect === "Zoobot" ? 1 : 2, u.effect === "Zoobot" ? 1 : 2);
      break;
    case "Defender of Argus": {
      const i = p.board.indexOf(u);
      for (const v of [p.board[i - 1], p.board[i + 1]])
        if (v) {
          buff(v, f, f, u.name);
          keyword(v, "TAUNT");
        }
      break;
    }
    case "Pogo-Hopper":
      buff(
        u,
        2 * f * (p.played[u.cardId] || 0),
        2 * f * (p.played[u.cardId] || 0),
        u.name,
      );
      break;
    case "Annihilan Battlemaster":
      buff(u, 0, Math.max(0, p.hero.health - p.health) * f, u.name);
      break;
    case "Primalfin Lookout":
      if (other.some((v) => isTribe(v, "MURLOC")))
        for (let n = 0; n < f; n++) discover(g, p, p.tier, "MURLOC");
      break;
    case "Gentle Megasaur":
      for (let n = 0; n < f; n++) {
        const adapt = pick(g, [
          "attack",
          "health",
          "stats",
          "shield",
          "poison",
          "taunt",
          "wind",
        ]);
        for (const v of other.filter((v) => isTribe(v, "MURLOC"))) {
          if (adapt === "attack") buff(v, 3, 0, u.name + " (Adapt)");
          else if (adapt === "health") buff(v, 0, 3, u.name + " (Adapt)");
          else if (adapt === "stats") buff(v, 1, 1, u.name + " (Adapt)");
          else
            keyword(
              v,
              (
                {
                  shield: "DIVINE_SHIELD",
                  poison: "POISONOUS",
                  taunt: "TAUNT",
                  wind: "WINDFURY",
                } as const
              )[adapt as "shield"],
            );
        }
        emit(g, "effect", `Murlocs adapted: ${adapt}`);
      }
      break;
  }
}
export function act(g: Game, id: number, cmd: Command, onPresentation?: (kind: "enter" | "battlecry", player: Player, source: number) => void) {
  const p = g.players[id];
  if (!p || p.health <= 0 || g.phase !== "recruit")
    throw Error("Not in recruitment");
  if (p.discovers.length && cmd.type !== "discover")
    throw Error("Choose your discovery first");
  if (p.hero.key === "afkay" && g.round < 3)
    throw Error("A. F. Kay skips the first two turns");
  switch (cmd.type) {
    case "buy": {
      const u = p.shop[cmd.index];
      if (!u) throw Error("Select a shop minion");
      if (p.gold < 3) throw Error("Buying costs 3 gold");
      if (p.hand.length >= 10) throw Error("Your hand is full");
      p.gold -= 3;
      p.shop.splice(cmd.index, 1);
      if (p.hero.key === "ratking" && isTribe(u, p.ratTribe)) buff(u, 1, 2, p.hero.power);
      p.hand.push(u);
      triple(g, p);
      emit(g, "buy", `${p.name} bought ${u.name}`);
      break;
    }
    case "play": {
      const u = p.hand[cmd.index];
      if (!u) throw Error("Select a card in hand");
      const t = p.board.find((v) => v.uid === cmd.target);
      if (
        cmd.magnetic &&
        (!has(u, "MAGNETIC") || !t || !isTribe(t, "MECHANICAL"))
      )
        throw Error("Magnetic requires a friendly Mech");
      if (!cmd.magnetic && p.board.length >= 7)
        throw Error("Your warband is full");
      p.hand.splice(cmd.index, 1);
      if (cmd.magnetic && t) {
        buff(t, u.attack, u.maxHealth, u.name + " (Magnetic)");
        u.keywords
          .filter((k) => k !== "MAGNETIC")
          .forEach((k) => keyword(t, k));
        if (u.effect === "Replicating Menace")
          t.extraDeaths.push(
            u.golden ? "Replicating Menace:gold" : "Replicating Menace",
          );
        t.absorbed ||= {};
        t.absorbed[u.cardId] = (t.absorbed[u.cardId] || 0) + u.copies;
        for (const [id,copies] of Object.entries(u.absorbed || {})) t.absorbed[id] = (t.absorbed[id] || 0) + copies;
      } else {
        summonRecruit(
          g,
          p,
          u,
          Math.max(0, Math.min(p.board.length, cmd.position ?? p.board.length)),
        );
        onPresentation?.("enter", p, u.uid);
        const repeats = Math.max(
          1,
          ...p.board
            .filter((v) => v.uid !== u.uid && v.effect === "Brann Bronzebeard")
            .map((v) => (v.golden ? 3 : 2)),
          p.powerActive && p.hero.key === "shudderwock" ? 2 : 1,
        );
        for (let n = 0; n < repeats; n++) {
          battlecry(g, p, u, cmd.target);
          onPresentation?.("battlecry", p, u.uid);
        }
      }
      const isBattlecry = g.cards
        .find((c) => c.id === u.cardId)
        ?.text.includes("Battlecry");
      for (const v of p.board.filter((v) => v.uid !== u.uid)) {
        if (isBattlecry && v.effect === "Crowd Favorite")
          buff(v, factor(v), factor(v), v.name);
        if (isTribe(u, "DEMON") && v.effect === "Wrath Weaver") {
          buff(v, 2 * factor(v), 2 * factor(v), v.name);
          if (!p.board.some((v) => v.effect === "Mal'Ganis")) p.health--;
        }
      }
      if (isBattlecry && p.hero.key === "shudderwock") p.powerActive = false;
      p.played[u.cardId] = (p.played[u.cardId] || 0) + 1;
      if (u.golden) discover(g, p, Math.min(6, p.tier + 1));
      emit(g, "play", `${p.name} played ${u.name}`);
      break;
    }
    case "sell": {
      const u = p.board[cmd.index];
      if (!u) throw Error("Select a warband minion");
      p.board.splice(cmd.index, 1);
      p.gold = Math.min(10, p.gold + 1);
      returnUnit(g, u);
      if (p.hero.key === "deryl" && p.shop.length)
        for (let i = 0; i < 2; i++) buff(pick(g, p.shop), 1, 1, p.hero.power);
      if (p.hero.key === "mukla" && isTribe(u, "BEAST")) p.bananas++;
      emit(g, "sell", `${p.name} sold ${u.name}`);
      break;
    }
    case "move": {
      if (!p.board[cmd.from] || cmd.to < 0 || cmd.to >= p.board.length)
        throw Error("Invalid position");
      const [u] = p.board.splice(cmd.from, 1);
      p.board.splice(cmd.to, 0, u);
      break;
    }
    case "refresh":
      if (p.gold < 1) throw Error("Refresh costs 1 gold");
      p.gold--;
      refresh(g, p);
      break;
    case "freeze":
      p.frozen = !p.frozen;
      break;
    case "upgrade":
      if (p.tier >= 6) throw Error("Maximum tavern tier");
      if (p.gold < p.upgrade) throw Error("Not enough gold to upgrade");
      p.gold -= p.upgrade;
      p.tier++;
      p.upgrade =
        [0, 0, 7, 8, 9, 10, 0][p.tier] -
        (p.hero.key === "bartendotron" ? 1 : 0);
      p.upgrade = Math.max(0, p.upgrade);
      emit(g, "upgrade", `${p.name} reached tavern tier ${p.tier}`);
      break;
    case "discover": {
      const choice = p.discovers[0]?.[cmd.index];
      if (!choice) throw Error("Invalid discovery");
      if (p.hand.length >= 10) throw Error("Make room in your hand");
      p.discovers.shift();
      const copies = g.pool[choice] > 0 ? 1 : 0;
      if (copies) g.pool[choice]--;
      p.hand.push(makeUnit(g, choice, false, copies));
      triple(g, p);
      break;
    }
    case "coin":
      if (!p.coins || p.gold >= 10) throw Error("No usable coin");
      p.coins--;
      p.gold++;
      break;
    case "banana": {
      const u = p.board.find((v) => v.uid === cmd.target);
      if (!p.bananas || !u) throw Error("Choose a minion for your banana");
      p.bananas--;
      buff(u, 1, 1, "Banana");
      break;
    }
    case "power": {
      if (p.hero.passive) throw Error("This hero power is passive");
      if (p.powerUsed) throw Error("Hero power already used");
      if (p.gold < p.hero.cost) throw Error("Not enough gold for hero power");
      const k = p.hero.key;
      if (
        k === "george" &&
        !p.board.some((u) => u.uid === cmd.target && !has(u, "DIVINE_SHIELD"))
      )
        throw Error("Select a minion without Divine Shield");
      if (k === "yogg" && (!p.shop.length || p.hand.length >= 10))
        throw Error("Need a shop minion and hand space");
      p.gold -= p.hero.cost;
      p.powerUsed = true;
      p.powerActive = true;
      if (k === "pyramad" && p.board.length) buff(pick(g, p.board), 0, 2, p.hero.power);
      if (k === "jaraxxus")
        p.board
          .filter((u) => isTribe(u, "DEMON"))
          .forEach((u) => buff(u, 1, 1, p.hero.power));
      if (k === "wagtoggle")
        for (const t of ["MECHANICAL", "DEMON", "MURLOC", "BEAST"]) {
          const a = p.board.filter((u) => isTribe(u, t));
          if (a.length) buff(pick(g, a), 0, 1, p.hero.power);
        }
      if (k === "george")
        keyword(
          p.board.find((u) => u.uid === cmd.target)!,
          "DIVINE_SHIELD",
        );
      if (k === "toki") refresh(g, p, true);
      if (k === "yogg") {
        const u = pick(g, p.shop);
        p.shop = p.shop.filter((v) => v !== u);
        buff(u, 1, 1, p.hero.power);
        p.hand.push(u);
        triple(g, p);
      }
      if (k === "gallywix") p.coins++;
      if (k === "lichbaz") {
        if (!p.board.some((u) => u.effect === "Mal'Ganis")) p.health -= 3;
        p.coins++;
      }
      emit(g, "power", `${p.name} used ${p.hero.power}`);
      break;
    }
  }
}
export function endRecruit(g: Game, p: Player) {
  for (const u of p.board) {
    const f = factor(u);
    if (u.effect === "Iron Sensei") {
      const a = p.board.filter((v) => v !== u && isTribe(v, "MECHANICAL"));
      if (a.length) buff(pick(g, a), 2 * f, 2 * f, u.name);
    }
    if (u.effect === "Lightfang Enforcer")
      for (const t of ["MECHANICAL", "MURLOC", "DEMON", "BEAST"]) {
        const a = p.board.filter((v) => v !== u && isTribe(v, t));
        if (a.length) buff(pick(g, a), 2 * f, 2 * f, u.name);
      }
  }
  if (p.frozen && p.hero.key === "sindragosa")
    p.shop.forEach((u) => buff(u, 1, 1, p.hero.power));
}
export function beginRound(g: Game) {
  g.round++;
  g.phase = "recruit";
  for (const p of g.players.filter((p) => p.health > 0)) {
    p.gold = Math.min(10, g.round + 2);
    p.upgrade = Math.max(0, p.upgrade - 1);
    p.powerUsed = false;
    p.powerActive = false;
    p.ratTribe = pick(g, ["BEAST", "MECHANICAL", "MURLOC", "DEMON"]);
    if (!p.frozen) refresh(g, p);
    else refresh(g, p, false, true);
    for (const u of p.board)
      if (u.effect === "Micro Machine") buff(u, factor(u), 0, u.name);
    p.hand = p.hand.map((u) => {
      if (!u.shifter) return u;
      const c = pick(
        g,
        g.cards.filter((c) => c.enabled),
      );
      const v = makeUnit(g, c.id, u.golden, 0);
      v.shifter = true;
      returnUnit(g, u);
      return v;
    });
    if (p.hero.key === "afkay") {
      if (g.round < 3) p.gold = 0;
      if (g.round === 3) {
        discover(g, p, 3);
        discover(g, p, 4);
      }
    }
  }
  emit(g, "round", `Recruitment · round ${g.round}`);
}
export function validateCards(value: unknown): Card[] {
  if (!Array.isArray(value) || !value.length || value.length > 2000)
    throw Error("Expected a card pack with 1–2000 cards");
  const ids = new Set<string>();
  for (const c of value) {
    if (
      !c ||
      typeof c !== "object" ||
      typeof c.id !== "string" ||
      ids.has(c.id)
    )
      throw Error("Invalid or duplicate card ID");
    ids.add(c.id);
    if (
      typeof c.name !== "string" ||
      !c.name.trim() ||
      typeof c.effect !== "string" ||
      (!BASE_CARDS.some((b) => b.effect === c.effect) && c.effect !== "")
    )
      throw Error("Unknown effect or missing name");
    for (const [k, min, max] of [
      ["attack", 0, 999],
      ["health", 1, 999],
      ["tier", 1, 6],
      ["pool", 1, 100],
      ["buffAttack", 0, 100],
      ["buffHealth", 0, 100],
    ] as const)
      if (!Number.isInteger(c[k]) || c[k] < min || c[k] > max)
        throw Error(`Invalid ${k} on ${c.name}`);
    if (
      !Array.isArray(c.keywords) ||
      c.keywords.some(
        (k: string) =>
          ![
            "TAUNT",
            "DIVINE_SHIELD",
            "POISONOUS",
            "WINDFURY",
            "REBORN",
            "MAGNETIC",
          ].includes(k),
      )
    )
      throw Error("Invalid keyword");
    if (typeof c.art !== "string" || !/^art\/[a-zA-Z0-9_-]+\.(webp|png)$/.test(c.art))
      throw Error("Invalid local artwork path");
    if (
      typeof c.tribe !== "string" ||
      ![
        "NONE",
        "BEAST",
        "MECHANICAL",
        "MURLOC",
        "DEMON",
        "ALL",
        "DRAGON",
      ].includes(c.tribe)
    )
      throw Error("Invalid tribe");
    if (typeof c.enabled !== "boolean" || typeof c.text !== "string")
      throw Error("Invalid card fields");
  }
  for (let t = 1; t <= 6; t++)
    if (!value.some((c) => c.enabled && c.tier === t))
      throw Error(`Enable at least one card in tier ${t}`);
  return clone(value as Card[]);
}
