export type Keyword =
  "TAUNT" | "DIVINE_SHIELD" | "POISONOUS" | "WINDFURY" | "REBORN" | "MAGNETIC";
export interface Card {
  id: string;
  name: string;
  tier: number;
  attack: number;
  health: number;
  tribe: string;
  text: string;
  goldenText: string;
  goldenId: string;
  keywords: Keyword[];
  effect: string;
  enabled: boolean;
  pool: number;
  art: string;
  cost: number;
  rarity: string;
  buffAttack: number;
  buffHealth: number;
}
export interface Hero {
  id: string;
  key: string;
  name: string;
  health: number;
  power: string;
  cost: number;
  text: string;
  passive: boolean;
  art: string;
}
export interface Unit {
  enchantments?: {source: string; attack: number; health: number}[];
  auraEffects?: {source: string; sourceUid: number; attack: number; health: number}[];
  auraAttack?: number;
  auraHealth?: number;
  auraSource?: boolean;
  uid: number;
  cardId: string;
  name: string;
  attack: number;
  health: number;
  maxHealth: number;
  tribe: string;
  keywords: Keyword[];
  golden: boolean;
  tier: number;
  effect: string;
  art: string;
  copies: number;
  absorbed?: Record<string, number>;
  extraDeaths: string[];
  rebornUsed?: boolean;
  poisoned?: boolean;
  attacks?: number;
  shifter?: boolean;
}
export interface Player {
  id: number;
  name: string;
  hero: Hero;
  health: number;
  tier: number;
  gold: number;
  upgrade: number;
  board: Unit[];
  hand: Unit[];
  shop: Unit[];
  frozen: boolean;
  powerUsed: boolean;
  powerActive: boolean;
  played: Record<string, number>;
  placement?: number;
  lastOpponent?: number;
  coins: number;
  bananas: number;
  ratTribe: string;
  discovers: string[][];
  aiReason: string;
}
export interface CombatFrame {
  left: Unit[];
  right: Unit[];
  text: string;
  attacker?: number;
  target?: number;
}
export interface CombatResult {
  leftId: number;
  rightId: number;
  winner: number | null;
  damage: number;
  frames: CombatFrame[];
  capped: boolean;
}
export interface Log {
  seq: number;
  round: number;
  kind: string;
  text: string;
}
export interface Game {
  version: 1;
  seed: number;
  rng: number;
  nextId: number;
  round: number;
  phase: "recruit" | "combat" | "finished";
  players: Player[];
  pool: Record<string, number>;
  cards: Card[];
  contentHash: string;
  log: Log[];
  combats: CombatResult[];
  lastCombat?: CombatResult;
  ghost?: Player;
  modified: boolean;
  difficulty: "casual" | "standard" | "expert";
  createdAt: string;
  history: RoundSnapshot[];
}
export interface RoundSnapshot {
  round: number;
  players: Player[];
  combats: CombatResult[];
}
export type Command =
  | { type: "buy"; index: number }
  | {
      type: "play";
      index: number;
      position?: number;
      target?: number;
      magnetic?: boolean;
    }
  | { type: "sell"; index: number }
  | { type: "move"; from: number; to: number }
  | { type: "refresh" }
  | { type: "freeze" }
  | { type: "upgrade" }
  | { type: "power"; target?: number }
  | { type: "discover"; index: number }
  | { type: "coin" }
  | { type: "banana"; target: number };
