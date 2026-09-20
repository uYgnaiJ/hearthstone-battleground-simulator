import type { Game, Player, Unit, CombatResult, Keyword } from "./types";
import {
  clone,
  pick,
  random,
  shuffle,
  has,
  keyword,
  buff,
  factor,
  isTribe,
  token,
  makeUnit,
  emit,
  endRecruit,
  returnUnit,
} from "./engine";

// Shared by combat and recruitment presentation. Stored units exclude auras.
const auraNames = new Set(["Dire Wolf Alpha", "Murloc Warleader", "Phalanx Commander", "Siegebreaker", "Mal'Ganis", "Old Murk-Eye"]);
export function auraEffects(board: Unit[], index: number, all: Unit[] = board): NonNullable<Unit["auraEffects"]> {
  const u = board[index], effects: NonNullable<Unit["auraEffects"]> = [];
  for (let j=0;j<board.length;j++) {
    const v=board[j]; if(v===u || v.health<=0 || v.poisoned)continue;
    const f=factor(v);let attack=0,health=0;
    if(v.effect==="Dire Wolf Alpha" && Math.abs(index-j)===1)attack+=f;
    if(v.effect==="Murloc Warleader" && isTribe(u,"MURLOC"))attack+=2*f;
    if(v.effect==="Phalanx Commander" && has(u,"TAUNT"))attack+=2*f;
    if(v.effect==="Siegebreaker" && isTribe(u,"DEMON"))attack+=f;
    if(v.effect==="Mal'Ganis" && isTribe(u,"DEMON")){attack+=2*f;health+=2*f;}
    if(attack||health)effects.push({source:v.name,sourceUid:v.uid,attack,health});
  }
  if(u.effect==="Old Murk-Eye") {const attack=all.filter(v=>v!==u&&isTribe(v,"MURLOC")).length*factor(u);if(attack)effects.push({source:u.name,sourceUid:u.uid,attack,health:0});}
  return effects;
}
export function auraBonus(board: Unit[], index: number, all: Unit[] = board): [number,number] {
  return auraEffects(board,index,all).reduce<[number,number]>((sum,e)=>[sum[0]+e.attack,sum[1]+e.health],[0,0]);
}
export function recruitPresentation(player: Player): Player {
  const view=clone(player);
  view.board=player.board.map((u,i)=>{const effects=auraEffects(player.board,i);const [attack,health]=auraBonus(player.board,i);return {...clone(u),attack:u.attack+attack,health:u.health+health,maxHealth:u.maxHealth+health,auraAttack:attack,auraHealth:health,auraEffects:effects,auraSource:auraNames.has(u.effect)};});
  return view;
}

export function combat(
  g: Game,
  left: Player,
  right: Player,
  record = true,
): CombatResult {
  const boards = [clone(left.board), clone(right.board)];
  const deadMechs: Unit[][] = [[], []];
  const frames: CombatResult["frames"] = [];
  const aura = new Map<number, [number, number]>();
  let events = 0;
  const alive = (u: Unit) => u.health > 0 && !u.poisoned;
  const snapshot = (text: string, attacker?: number, target?: number) => {
    if (record)
      frames.push({
        left: clone(boards[0]),
        right: clone(boards[1]),
        text,
        attacker,
        target,
      });
  };
  function updateAuras() {
    for (const board of boards)
      for (const u of board) {
        const prev = aura.get(u.uid) || [0, 0];
        u.attack -= prev[0];
        u.health -= prev[1];
        u.maxHealth -= prev[1];
        aura.set(u.uid, [0, 0]);
      }
    for (const board of boards)
      for (let i = 0; i < board.length; i++) {
        const u = board[i];
        const [a, h] = auraBonus(board, i, boards.flat());
        u.attack += a;
        u.health += h;
        u.maxHealth += h;
        aura.set(u.uid, [a, h]);
        u.auraAttack=a;u.auraHealth=h;u.auraEffects=auraEffects(board,i,boards.flat());u.auraSource=auraNames.has(u.effect);
      }
  }
  function summon(
    side: number,
    u: Unit,
    index = boards[side].length,
    repeat = true,
  ) {
    const mult = repeat
      ? Math.max(
          1,
          ...boards[side]
            .filter((v) => alive(v) && v.effect === "Khadgar")
            .map((v) => (v.golden ? 3 : 2)),
        )
      : 1;
    for (let i = 0; i < mult; i++) {
      if (boards[side].length >= 7) return;
      const v = i === 0 ? u : { ...clone(u), uid: g.nextId++ };
      boards[side].splice(Math.min(index + i, boards[side].length), 0, v);
      for (const ally of boards[side]) {
        if (ally === v || !alive(ally)) continue;
        const f = factor(ally);
        if (isTribe(v, "BEAST")) {
          if (ally.effect === "Pack Leader") buff(v, 3 * f, 0, ally.name);
          if (ally.effect === "Mama Bear") buff(v, 5 * f, 5 * f, ally.name);
        }
        if (isTribe(v, "MURLOC") && ally.effect === "Murloc Tidecaller")
          buff(ally, f, 0, ally.name);
        if (isTribe(v, "MECHANICAL") && ally.effect === "Cobalt Guardian")
          keyword(ally, "DIVINE_SHIELD");
      }
    }
    updateAuras();
  }
  function damage(side: number, u: Unit, n: number, poison = false) {
    if (n <= 0) return;
    if (has(u, "DIVINE_SHIELD")) {
      u.keywords = u.keywords.filter((k) => k !== "DIVINE_SHIELD");
      for (const ally of boards[side])
        if (alive(ally) && ally.effect === "Bolvar, Fireblood")
          buff(ally, 2 * factor(ally), 0, ally.name);
      return;
    }
    u.health -= n;
    if (poison) u.poisoned = true;
    const f = factor(u);
    if (u.effect === "Imp Gang Boss")
      summon(side, token(g, "Imp", f, f, "DEMON"), boards[side].indexOf(u) + 1);
    if (u.effect === "Security Rover")
      summon(
        side,
        token(g, "Guard Bot", 2 * f, 3 * f, "MECHANICAL", ["TAUNT"]),
        boards[side].indexOf(u) + 1,
      );
  }
  function hitRandom(side: number, n: number) {
    const a = boards[side].filter(alive);
    if (a.length) damage(side, pick(g, a), n);
  }
  function deathEffect(
    side: number,
    u: Unit,
    index: number,
    effect = u.effect,
  ) {
    const f = factor(u),
      other = 1 - side;
    const spawn = (
      name: string,
      a: number,
      h: number,
      t = "NONE",
      keys: Keyword[] = [],
      count = 1,
    ) => {
      for (let i = 0; i < count; i++)
        summon(side, token(g, name, a * f, h * f, t, [...keys]), index + i);
    };
    const team = (a: number, h: number, t?: string) =>
      boards[side]
        .filter((v) => alive(v) && (!t || isTribe(v, t)))
        .forEach((v) => buff(v, a * f, h * f, u.name));
    const randomMinion = (
      filter: (c: Game["cards"][number]) => boolean,
      count: number,
    ) => {
      const cs = g.cards.filter((c) => c.enabled && filter(c));
      for (let i = 0; i < count && cs.length; i++)
        summon(side, makeUnit(g, pick(g, cs).id, false, 0), index + i);
    };
    switch (effect) {
      case "Mecharoo":
        spawn("Jo-E Bot", 1, 1, "MECHANICAL");
        break;
      case "Harvest Golem":
        spawn("Damaged Golem", 2, 1, "MECHANICAL");
        break;
      case "Kindly Grandmother":
        spawn("Big Bad Wolf", 3, 2, "BEAST");
        break;
      case "Infested Wolf":
        spawn("Spider", 1, 1, "BEAST", [], 2);
        break;
      case "Rat Pack":
        spawn("Rat", 1, 1, "BEAST", [], Math.min(7, Math.max(0, u.attack)));
        break;
      case "Replicating Menace":
        spawn("Microbot", 1, 1, "MECHANICAL", [], 3);
        break;
      case "Savannah Highmane":
        spawn("Hyena", 2, 2, "BEAST", [], 2);
        break;
      case "Sated Threshadon":
        spawn("Primalfin", 1, 1, "MURLOC", [], 3);
        break;
      case "Mechano-Egg":
        spawn("Robosaur", 8, 8, "MECHANICAL");
        break;
      case "Voidlord":
        spawn("Voidwalker", 1, 3, "DEMON", ["TAUNT"], 3);
        break;
      case "The Beast":
        summon(other, token(g, "Finkle Einhorn", 3, 3));
        break;
      case "Kaboom Bot":
        for (let i = 0; i < f; i++) hitRandom(other, 4);
        break;
      case "Spawn of N'Zoth":
        team(1, 1);
        break;
      case "Goldrinn, the Great Wolf":
        team(4, 4, "BEAST");
        break;
      case "Selfless Hero":
        for (let i = 0; i < f; i++) {
          const cs = boards[side].filter(
            (v) => alive(v) && !has(v, "DIVINE_SHIELD"),
          );
          if (cs.length) keyword(pick(g, cs), "DIVINE_SHIELD");
        }
        break;
      case "Tortollan Shellraiser": {
        const cs = boards[side].filter(alive);
        if (cs.length) buff(pick(g, cs), f, f, u.name);
        break;
      }
      case "Mounted Raptor":
        randomMinion((c) => c.cost === 1, f);
        break;
      case "Piloted Shredder":
        randomMinion((c) => c.cost === 2, f);
        break;
      case "Piloted Sky Golem":
        randomMinion((c) => c.cost === 4, f);
        break;
      case "Ghastcoiler":
        randomMinion(
          (c) => c.text.includes("Deathrattle") && c.effect !== "Ghastcoiler",
          2 * f,
        );
        break;
      case "Sneed's Old Shredder":
        randomMinion((c) => c.rarity === "LEGENDARY", f);
        break;
      case "Kangor's Apprentice":
        for (const m of deadMechs[side].slice(0, 2 * f)) {
          const d = g.cards.find((c) => c.id === m.cardId);
          summon(
            side,
            d
              ? makeUnit(g, d.id, m.golden, 0)
              : token(g, m.name, m.attack, m.maxHealth, m.tribe),
            index++,
          );
        }
        break;
    }
  }
  function deaths() {
    let guard = 0;
    while (guard++ < 100) {
      const dying = boards.map((b) => b.filter((u) => !alive(u)));
      if (!dying.flat().length) break;
      const multipliers = boards.map((b) =>
        Math.max(
          1,
          ...b
            .filter((v) => alive(v) && v.effect === "Baron Rivendare")
            .map((v) => (v.golden ? 3 : 2)),
        ),
      );
      const positions = dying.map((list, s) =>
        list.map((u) => boards[s].indexOf(u)),
      );
      for (let s = 0; s < 2; s++) {
        boards[s] = boards[s].filter(alive);
        for (const u of dying[s])
          if (isTribe(u, "MECHANICAL")) deadMechs[s].push(clone(u));
      }
      updateAuras();
      for (let s = 0; s < 2; s++)
        for (let i = 0; i < dying[s].length; i++) {
          const u = dying[s][i];
          for (const ally of [...boards[s]])
            if (alive(ally)) {
              const f = factor(ally);
              if (isTribe(u, "BEAST") && ally.effect === "Scavenging Hyena")
                buff(ally, 2 * f, f, ally.name);
              if (isTribe(u, "MECHANICAL") && ally.effect === "Junkbot")
                buff(ally, 2 * f, 2 * f, ally.name);
              if (isTribe(u, "DEMON") && ally.effect === "Soul Juggler")
                hitRandom(1 - s, 3 * f);
            }
          for (let n = 0; n < multipliers[s]; n++) {
            deathEffect(s, u, positions[s][i]);
            for (const e of u.extraDeaths)
              deathEffect(
                s,
                e.endsWith(":gold") ? { ...u, golden: true } : u,
                positions[s][i],
                e.replace(":gold", ""),
              );
          }
          if (has(u, "REBORN") && !u.rebornUsed) {
            const c = g.cards.find((c) => c.id === u.cardId);
            const v = c
              ? makeUnit(g, c.id, u.golden, 0)
              : token(g, u.name, u.attack, 1, u.tribe);
            v.health = 1;
            v.rebornUsed = true;
            v.keywords = v.keywords.filter((k) => k !== "REBORN");
            summon(s, v, positions[s][i], false);
          }
          snapshot(`${u.name} was destroyed`);
        }
      updateAuras();
      events++;
      if (events > 600) break;
    }
  }
  updateAuras();
  snapshot("Combat begins");
  for (let s = 0; s < 2; s++) {
    const p = [left, right][s];
    if (!p.powerActive) continue;
    const k = p.hero.key;
    if (k === "putricide" && boards[s][0]) buff(boards[s][0], 10, 0, p.hero.power);
    if (k === "lichking" && boards[s].length)
      keyword(boards[s].at(-1)!, "REBORN");
    if (k === "nefarian")
      for (const u of [...boards[1 - s]]) damage(1 - s, u, 1);
    if (k === "ragnaros" || k === "patches")
      for (let n = 0; n < 2; n++) hitRandom(1 - s, k === "ragnaros" ? 8 : 3);
    snapshot(`${p.hero.name}: ${p.hero.power}`);
    deaths();
  }
  let side =
    boards[0].length === boards[1].length
      ? random(g) < 0.5
        ? 0
        : 1
      : boards[0].length > boards[1].length
        ? 0
        : 1;
  let cursor = [0, 0];
  let turns = 0;
  while (
    boards[0].length &&
    boards[1].length &&
    turns++ < 300 &&
    events < 600
  ) {
    const own = boards[side],
      enemy = boards[1 - side];
    if (!boards.flat().some((u) => u.attack > 0)) break;
    const candidates = own.filter((u) => alive(u) && u.attack > 0);
    if (!candidates.length) {
      side = 1 - side;
      continue;
    }
    let attacker: Unit | undefined;
    for (let n = 0; n < own.length; n++) {
      const u = own[(cursor[side] + n) % own.length];
      if (u.attack > 0 && alive(u)) {
        attacker = u;
        break;
      }
    }
    if (!attacker) {
      side = 1 - side;
      continue;
    }
    const originalIndex = own.indexOf(attacker);
    const swings = has(attacker, "WINDFURY")
      ? attacker.golden && attacker.effect === "Zapp Slywick"
        ? 4
        : 2
      : 1;
    for (
      let swing = 0;
      swing < swings &&
      alive(attacker) &&
      boards[side].includes(attacker) &&
      boards[1 - side].length;
      swing++
    ) {
      const targets = boards[1 - side].filter(alive);
      if (!targets.length) break;
      const taunts = targets.filter((u) => has(u, "TAUNT"));
      const choices =
        attacker.effect === "Zapp Slywick"
          ? targets.filter(
              (u) => u.attack === Math.min(...targets.map((u) => u.attack)),
            )
          : taunts.length
            ? taunts
            : targets;
      const target = pick(g, choices),
        targetIndex = boards[1 - side].indexOf(target);
      const attack = Math.max(0, attacker.attack),
        retaliate = Math.max(0, target.attack);
      const neighbors = ["Cave Hydra", "Foe Reaper 4000"].includes(
        attacker.effect,
      )
        ? [
            boards[1 - side][targetIndex - 1],
            boards[1 - side][targetIndex + 1],
          ].filter(Boolean)
        : [];
      damage(1 - side, target, attack, has(attacker, "POISONOUS"));
      damage(side, attacker, retaliate, has(target, "POISONOUS"));
      for (const n of neighbors)
        damage(1 - side, n, attack, has(attacker, "POISONOUS"));
      if (
        alive(attacker) &&
        !alive(target) &&
        attacker.effect === "The Boogeymonster"
      )
        buff(attacker, 2 * factor(attacker), 2 * factor(attacker), attacker.name);
      if (
        alive(attacker) &&
        target.health < 0 &&
        attacker.effect === "Ironhide Direhorn"
      )
        summon(
          side,
          token(
            g,
            "Ironhide Runt",
            5 * factor(attacker),
            5 * factor(attacker),
            "BEAST",
          ),
          boards[side].indexOf(attacker) + 1,
        );
      for (const ally of boards[side])
        if (alive(ally) && ally.effect === "Festeroot Hulk")
          buff(ally, factor(ally), 0, ally.name);
      snapshot(
        `${attacker.name} attacks ${target.name}`,
        attacker.uid,
        target.uid,
      );
      deaths();
      updateAuras();
      events++;
    }
    cursor[side] = boards[side].includes(attacker)
      ? boards[side].indexOf(attacker) + 1
      : Math.min(originalIndex, boards[side].length);
    side = 1 - side;
  }
  const capped = turns >= 300 || events >= 600;
  const winner =
    boards[0].length && !boards[1].length
      ? left.id
      : boards[1].length && !boards[0].length
        ? right.id
        : null;
  const winningSide = winner === left.id ? 0 : 1;
  const damageTotal =
    winner === null
      ? 0
      : [left, right][winningSide].tier +
        boards[winningSide].reduce((n, u) => n + u.tier, 0);
  snapshot(
    capped
      ? "Combat halted at the event limit"
      : winner === null
        ? "Combat tied"
        : `${winner === left.id ? left.name : right.name} wins · ${damageTotal} damage`,
  );
  return {
    leftId: left.id,
    rightId: right.id,
    winner,
    damage: damageTotal,
    frames,
    capped,
  };
}
export function resolveRound(g: Game, record = true) {
  if (g.phase !== "recruit") throw Error("Round already resolved");
  if (g.players.some((p) => p.health > 0 && p.discovers.length))
    throw Error("Resolve discoveries before combat");
  for (const p of g.players.filter((p) => p.health > 0)) endRecruit(g, p);
  const before = g.players.filter((p) => p.placement === undefined);
  const participants = shuffle(
    g,
    before.filter((p) => p.health > 0),
  );
  const paired: Player[][] = [];
  while (participants.length > 1) {
    const a = participants.shift()!;
    const next = participants.findIndex((p) => p.id !== a.lastOpponent);
    const b = participants.splice(next < 0 ? 0 : next, 1)[0];
    paired.push([a, b]);
  }
  if (participants.length) {
    const a = participants[0];
    const ghost = g.ghost
      ? clone(g.ghost)
      : { ...clone(a), id: -1, name: "Ghost", board: [] };
    ghost.id = -1;
    paired.push([a, ghost]);
  }
  g.combats = [];
  for (const [a, b] of paired) {
    const result = combat(g, a, b, record);
    g.combats.push(result);
    a.lastOpponent = b.id;
    if (b.id >= 0) b.lastOpponent = a.id;
    if (result.winner !== null) {
      const loser = result.winner === a.id ? b : a;
      if (loser.id >= 0) loser.health -= result.damage;
    }
    emit(
      g,
      "combat",
      `${a.name} vs ${b.name}: ${result.winner === null ? "tie" : result.damage + " damage"}`,
    );
  }
  const eliminated = before
    .filter((p) => p.health <= 0)
    .sort((a, b) => a.health - b.health || a.id - b.id);
  let place = before.length;
  for (const p of eliminated) {
    p.placement = place--;
    g.ghost = clone(p);
    [...p.board, ...p.hand, ...p.shop].forEach((u) => returnUnit(g, u));
    p.shop = [];
    emit(g, "elimination", `${p.name} finished #${p.placement}`);
  }
  const live = g.players.filter((p) => p.health > 0);
  if (live.length === 1) live[0].placement = 1;
  g.lastCombat =
    g.combats.find((c) => c.leftId === 0 || c.rightId === 0) || g.combats[0];
  g.phase = live.length <= 1 ? "finished" : "combat";
  if (record)
    g.history.push({
      round: g.round,
      players: clone(g.players),
      combats: clone(g.combats),
    });
}
