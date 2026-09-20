import { describe, it, expect } from "vitest";
import {
  newGame,
  BASE_CARDS,
  HEROES,
  act,
  makeUnit,
  token,
  clone,
  beginRound,
  validateCards,
  contentHash,
  buff,
  triple,
} from "./engine";
import { combat, resolveRound, recruitPresentation } from "./combat";
import { recruitAI } from "./ai";
const id = (name: string) => BASE_CARDS.find((c) => c.name === name)!.id;
function setup() {
  const g = newGame(123, "patchwerk");
  g.players.forEach((p) => {
    p.board = [];
    p.hand = [];
  });
  return g;
}
describe("recruitment and state", () => {
  it("lists individual aura sources and keeps named permanent buffs after the aura leaves", () => {
    const g=setup(),p=g.players[0];
    p.board=[makeUnit(g,id("Murloc Tidehunter")),makeUnit(g,id("Murloc Warleader")),makeUnit(g,id("Murloc Warleader"),true)];
    buff(p.board[0],1,1,"Banana");buff(p.board[0],1,1,"Banana");
    const view=recruitPresentation(p);
    expect(view.board[0].auraEffects).toEqual([{source:"Murloc Warleader",sourceUid:p.board[1].uid,attack:2,health:0},{source:"Murloc Warleader",sourceUid:p.board[2].uid,attack:4,health:0}]);
    expect(view.board[0].enchantments).toEqual([{source:"Banana",attack:2,health:2}]);
    expect(view.board[0].attack).toBe(10);
    p.board.splice(1);
    expect(recruitPresentation(p).board[0]).toMatchObject({attack:4,auraEffects:[],enchantments:[{source:"Banana",attack:2,health:2}]});
  });
  it("shows Warleader auras immediately without persisting or doubling them in combat", () => {
    const g=setup(),p=g.players[0];
    p.board=[makeUnit(g,id("Murloc Tidehunter")),token(g,"Amalgam",1,1,"ALL"),makeUnit(g,id("Murloc Warleader"))];
    const saved=clone(p),view=recruitPresentation(p);
    expect(view.board.map(u=>u.attack)).toEqual([4,3,3]);
    expect(p).toEqual(saved);
    expect(recruitPresentation(p)).toEqual(view);
    expect(combat(clone(g),p,g.players[1]).frames[0].left.map(u=>u.attack)).toEqual([4,3,3]);
    act(g,0,{type:"sell",index:2});
    expect(recruitPresentation(p).board.map(u=>u.attack)).toEqual([2,1]);
  });
  it("updates adjacency and golden health auras without changing permanent stats", () => {
    const g=setup(),p=g.players[0];
    p.board=[makeUnit(g,id("Alleycat")),makeUnit(g,id("Dire Wolf Alpha")),makeUnit(g,id("Alleycat")),makeUnit(g,id("Alleycat"))];
    expect(recruitPresentation(p).board.map(u=>u.attack)).toEqual([2,2,2,1]);
    act(g,0,{type:"move",from:1,to:3});
    expect(recruitPresentation(p).board.map(u=>u.attack)).toEqual([1,1,2,2]);
    p.board=[makeUnit(g,id("Vulgar Homunculus")),makeUnit(g,id("Mal'Ganis"),true)];
    const base=clone(p.board[0]);
    expect(recruitPresentation(p).board[0]).toMatchObject({attack:base.attack+4,health:base.health+4,maxHealth:base.maxHealth+4});
    p.board.pop();
    expect(recruitPresentation(p).board[0]).toMatchObject(base);
  });
  it("presents entry before Battlecry summons without changing simulation results", () => {
    const g=setup(),p=g.players[0];p.hand=[makeUnit(g,id("Alleycat"))];p.board=[makeUnit(g,id("Brann Bronzebeard"))];
    const plain=clone(g),steps:{kind:string;names:string[]}[]=[];
    act(g,0,{type:"play",index:0},(kind,player)=>steps.push({kind,names:player.board.map(u=>u.name)}));
    act(plain,0,{type:"play",index:0});
    expect(g).toEqual(plain);
    expect(steps.map(s=>s.kind)).toEqual(["enter","battlecry","battlecry"]);
    expect(steps[0].names).toEqual(["Brann Bronzebeard","Alleycat"]);
    expect(steps[1].names.filter(n=>n==="Tabbycat")).toHaveLength(1);
    expect(steps[2].names.filter(n=>n==="Tabbycat")).toHaveLength(2);
  });
  it("reproduces shops with the same seed", () => {
    const a = newGame(123, "curator"),
      b = newGame(123, "curator");
    expect(a.players).toEqual(b.players);
    expect(a.rng).toBe(b.rng);
  });
  it("pays for purchases and rejects insufficient gold", () => {
    const g = setup(),
      p = g.players[0],
      first = p.shop[0];
    act(g, 0, { type: "buy", index: 0 });
    expect(p.gold).toBe(0);
    expect(p.hand[0].uid).toBe(first.uid);
    expect(() => act(g, 0, { type: "buy", index: 0 })).toThrow();
  });
  it("freezes the shop for one round", () => {
    const g = setup(),
      p = g.players[0],
      ids = p.shop.map((u) => u.uid);
    act(g, 0, { type: "freeze" });
    beginRound(g);
    expect(p.shop.map((u) => u.uid)).toEqual(ids);
    expect(p.frozen).toBe(false);
  });
  it("refills a partially purchased frozen shop without replacing its minions", () => {
    const g = setup(), p = g.players[0];
    act(g, 0, {type:"buy", index:0});
    const kept = p.shop.map(u => u.uid);
    act(g, 0, {type:"freeze"});
    beginRound(g);
    expect(p.shop).toHaveLength(3);
    expect(p.shop.slice(0,2).map(u => u.uid)).toEqual(kept);
    expect(p.frozen).toBe(false);
  });
  it("keeps magnetic copies out of the pool until the host is sold, including triples", () => {
    const g = setup(), p = g.players[0];
    const hostId = id("Harvest Golem"), magnetId = id("Replicating Menace");
    const before = g.pool[magnetId];
    g.pool[magnetId]--;
    p.board = [makeUnit(g, hostId)];
    p.hand = [makeUnit(g, magnetId)];
    act(g, 0, {type:"play",index:0,target:p.board[0].uid,magnetic:true});
    expect(g.pool[magnetId]).toBe(before-1);
    expect(p.board[0].absorbed?.[magnetId]).toBe(1);
    p.hand = [makeUnit(g, hostId),makeUnit(g, hostId)];
    triple(g,p);
    expect(p.hand[0].golden).toBe(true);
    expect(p.hand[0].absorbed?.[magnetId]).toBe(1);
    act(g,0,{type:"play",index:0});
    act(g,0,{type:"discover",index:0});
    act(g,0,{type:"sell",index:0});
    expect(g.pool[magnetId]).toBe(before);
  });
  it("combines triples with enchantments and discovers on play", () => {
    const g = setup(),
      p = g.players[0];
    p.hand = [
      makeUnit(g, id("Alleycat")),
      makeUnit(g, id("Alleycat")),
      makeUnit(g, id("Alleycat")),
    ];
    buff(p.hand[0], 2, 3);
    triple(g, p);
    expect(p.hand).toHaveLength(1);
    expect(p.hand[0].attack).toBe(4);
    expect(p.hand[0].health).toBe(5);
    act(g, 0, { type: "play", index: 0 });
    expect(p.discovers).toHaveLength(1);
    expect(p.board[1].attack).toBe(2);
  });
  it("repeats battlecries using Brann without triggering them on summons", () => {
    const g = setup(),
      p = g.players[0];
    p.board = [
      makeUnit(g, id("Brann Bronzebeard")),
      makeUnit(g, id("Murloc Tidecaller")),
    ];
    p.hand = [makeUnit(g, id("Coldlight Seer"))];
    act(g, 0, { type: "play", index: 0 });
    expect(p.board[1].health).toBe(6);
  });
  it("preserves the card pack in a serialized match", () => {
    const cards = clone(BASE_CARDS);
    cards[0].attack = 18;
    const g = newGame(9, "patchwerk", cards);
    const loaded = JSON.parse(JSON.stringify(g));
    expect(loaded.cards[0].attack).toBe(18);
    expect(contentHash(loaded.cards)).toBe(g.contentHash);
  });
  it("rejects bad card packs", () => {
    const cards = clone(BASE_CARDS);
    cards[0].health = -1;
    expect(() => validateCards(cards)).toThrow();
    cards[0].health = 1;
    cards[0].effect = "evil()";
    expect(() => validateCards(cards)).toThrow();
  });
});
describe("combat interactions", () => {
  it("applies simultaneous damage and preserves recruit boards", () => {
    const g = setup();
    g.players[0].board = [token(g, "A", 4, 4)];
    g.players[1].board = [token(g, "B", 4, 4)];
    const before = clone(g.players);
    const r = combat(g, g.players[0], g.players[1]);
    expect(r.winner).toBeNull();
    expect(g.players).toEqual(before);
  });
  it("shield absorbs poisonous damage", () => {
    const g = setup();
    g.players[0].board = [token(g, "Poison", 1, 1, "BEAST", ["POISONOUS"])];
    g.players[1].board = [token(g, "Shield", 2, 8, "NONE", ["DIVINE_SHIELD"])];
    const r = combat(g, g.players[0], g.players[1]);
    expect(r.winner).toBe(1);
    expect(r.frames.at(-1)!.right[0].health).toBe(8);
  });
  it("taunt restricts targets", () => {
    const g = setup();
    g.players[0].board = [token(g, "Attacker", 20, 40)];
    const taunt = token(g, "Taunt", 0, 10, "NONE", ["TAUNT"]);
    g.players[1].board = [token(g, "Other", 0, 10), taunt];
    const r = combat(g, g.players[0], g.players[1]);
    expect(r.frames.find((f) => f.attacker)?.target).toBe(taunt.uid);
  });
  it("deathrattle summons can win after a simultaneous kill", () => {
    const g = setup();
    g.players[0].board = [makeUnit(g, id("Mecharoo"))];
    g.players[1].board = [token(g, "Opponent", 1, 1)];
    const r = combat(g, g.players[0], g.players[1]);
    expect(r.winner).toBe(0);
    expect(r.frames.at(-1)!.left[0].name).toBe("Jo-E Bot");
  });
  it("cleave strikes adjacent targets", () => {
    const g = setup();
    const hydra = makeUnit(g, id("Cave Hydra"));
    hydra.attack = 20;
    hydra.health = 50;
    g.players[0].board = [hydra];
    g.players[1].board = [
      token(g, "A", 0, 1),
      token(g, "B", 0, 1, "NONE", ["TAUNT"]),
      token(g, "C", 0, 1),
    ];
    const r = combat(g, g.players[0], g.players[1]);
    expect(r.winner).toBe(0);
    expect(r.frames.filter((f) => f.attacker)).toHaveLength(1);
  });
  it("reborn returns once, at one health", () => {
    const g = setup();
    const u = token(g, "Reborn", 1, 1, "NONE", ["REBORN"]);
    g.players[0].board = [u];
    g.players[1].board = [token(g, "A", 1, 2)];
    const r = combat(g, g.players[0], g.players[1]);
    expect(
      r.frames.some((f) => f.left.some((v) => v.rebornUsed && v.health === 1)),
    ).toBe(true);
    expect(r.winner).toBeNull();
  });
  it("zero-attack boards tie without reaching the guard", () => {
    const g = setup();
    g.players[0].board = [token(g, "A", 0, 20)];
    g.players[1].board = [token(g, "B", 0, 20)];
    const r = combat(g, g.players[0], g.players[1]);
    expect(r.winner).toBeNull();
    expect(r.capped).toBe(false);
  });
  it("runs every minion through combat in normal and golden form", () => {
    for (const c of BASE_CARDS)
      for (const golden of [false, true]) {
        const g = setup();
        g.players[0].board = [
          makeUnit(g, c.id, golden),
          makeUnit(g, id("Mecharoo")),
        ];
        g.players[1].board = [
          token(g, "Target", 8, 15),
          token(g, "Other", 3, 4),
        ];
        const r = combat(g, g.players[0], g.players[1]);
        expect(r.capped, c.name).toBe(false);
        for (const f of r.frames) {
          expect(f.left.length).toBeLessThanOrEqual(7);
          expect(f.right.length).toBeLessThanOrEqual(7);
        }
      }
  });
});
describe("full matches", () => {
  it("finishes seeded eight-player games across heroes", () => {
    for (let seed = 1; seed <= 20; seed++) {
      const g = newGame(seed, HEROES[seed % HEROES.length].key);
      for (let turn = 0; turn < 80 && g.phase !== "finished"; turn++) {
        for (const p of g.players) recruitAI(g, p);
        resolveRound(g, false);
        for (const p of g.players) {
          expect(p.board.length).toBeLessThanOrEqual(7);
          expect(p.hand.length).toBeLessThanOrEqual(10);
        }
        expect(g.combats.some((c) => c.capped)).toBe(false);
        if ((g.phase as string) !== "finished") beginRound(g);
      }
      expect(g.phase, `seed ${seed}`).toBe("finished");
      expect(g.players.filter((p) => p.placement === 1)).toHaveLength(1);
    }
  });
});
