import fs from "node:fs/promises";
import { createHash } from "node:crypto";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import { createRequire } from 'node:module';
const run = promisify(execFile);
const tiles = process.argv.includes('--tiles');
const renders = process.argv.includes('--renders');
const images = renders ? createRequire(import.meta.url)('hearthstone-card-images') : null;
const renderMap = new Map();
const build = 35747;
const source = JSON.parse(await fs.readFile("cards-source.json", "utf8"));
const tiers = [
  [
    "Alleycat",
    "Dire Wolf Alpha",
    "Mecharoo",
    "Micro Machine",
    "Murloc Tidecaller",
    "Murloc Tidehunter",
    "Righteous Protector",
    "Rockpool Hunter",
    "Selfless Hero",
    "Voidwalker",
    "Vulgar Homunculus",
    "Wrath Weaver",
  ],
  [
    "Annoy-o-Tron",
    "Harvest Golem",
    "Kaboom Bot",
    "Kindly Grandmother",
    "Metaltooth Leaper",
    "Murloc Warleader",
    "Nathrezim Overseer",
    "Nightmare Amalgam",
    "Old Murk-Eye",
    "Pogo-Hopper",
    "Rat Pack",
    "Scavenging Hyena",
    "Shielded Minibot",
    "Spawn of N'Zoth",
    "Zoobot",
    "Mounted Raptor",
  ],
  [
    "Cobalt Guardian",
    "Coldlight Seer",
    "Crowd Favorite",
    "Crystalweaver",
    "Houndmaster",
    "Imp Gang Boss",
    "Infested Wolf",
    "Khadgar",
    "Pack Leader",
    "Phalanx Commander",
    "Piloted Shredder",
    "Psych-o-Tron",
    "Replicating Menace",
    "Screwjank Clunker",
    "Shifter Zerus",
    "Soul Juggler",
    "Tortollan Shellraiser",
  ],
  [
    "Annoy-o-Module",
    "Bolvar, Fireblood",
    "Cave Hydra",
    "Defender of Argus",
    "Festeroot Hulk",
    "Iron Sensei",
    "Junkbot",
    "Menagerie Magician",
    "Piloted Sky Golem",
    "Security Rover",
    "Siegebreaker",
    "The Beast",
    "Virmen Sensei",
  ],
  [
    "Annihilan Battlemaster",
    "Baron Rivendare",
    "Brann Bronzebeard",
    "Goldrinn, the Great Wolf",
    "Ironhide Direhorn",
    "Lightfang Enforcer",
    "Mal'Ganis",
    "Mechano-Egg",
    "Primalfin Lookout",
    "Sated Threshadon",
    "Savannah Highmane",
    "Strongshell Scavenger",
    "The Boogeymonster",
    "Voidlord",
  ],
  [
    "Foe Reaper 4000",
    "Gentle Megasaur",
    "Ghastcoiler",
    "Kangor's Apprentice",
    "Maexxna",
    "Mama Bear",
    "Sneed's Old Shredder",
    "Zapp Slywick",
  ],
];
const clean = (t) =>
  (t || "")
    .replace(/<[^>]+>|\[x\]|\$/g, "")
    .replace(/\s+/g, " ")
    .trim();
const cards = tiers.flatMap((names, i) =>
  names.map((name) => {
    const candidates = source.filter(
      (c) =>
        c.name === name &&
        c.type === "MINION" &&
        !c.id.startsWith("TB_BaconUps"),
    );
    const c =
      candidates.find((c) => c.id.startsWith("BGS_")) ||
      candidates.find((c) => c.collectible) ||
      candidates[0];
    if (!c) throw Error("Missing " + name);
    const gold = source.find(
      (c) => c.name === name && c.id.startsWith("TB_BaconUps"),
    );
    return {
      id: c.id,
      name,
      tier: i + 1,
      attack: c.attack,
      health: c.health,
      tribe: c.race || "NONE",
      text: clean(c.text),
      goldenText: clean(gold?.text),
      goldenId: gold?.id || c.id,
      keywords: (c.mechanics || []).map(k => k === 'MODULAR' ? 'MAGNETIC' : k).filter((k) =>
        [
          "TAUNT",
          "DIVINE_SHIELD",
          "POISONOUS",
          "WINDFURY",
          "REBORN",
          "MAGNETIC",
        ].includes(k),
      ),
      effect: name,
      enabled: true,
      pool: [18, 15, 13, 11, 9, 6][i],
      art: `art/${c.id}.${tiles ? 'png' : 'webp'}`,
      cost: c.cost || 0,
      rarity: c.rarity || "COMMON",
      buffAttack: 1,
      buffHealth: 1,
    };
  }),
);
const heroSpecs = [
  ["TB_BaconShop_HERO_34", "TB_BaconShop_HP_035", "patchwerk"],
  ["TB_BaconShop_HERO_33", "TB_BaconShop_HP_033", "curator"],
  ["TB_BaconShop_HERO_17", "TB_BaconShop_HP_015", "millificent"],
  ["TB_BaconShop_HERO_31", "TB_BaconShop_HP_009", "bartendotron"],
  ["TB_BaconShop_HERO_37", "TB_BaconShop_HP_036", "jaraxxus"],
  ["TB_BaconShop_HERO_39", "TB_BaconShop_HP_040", "pyramad"],
  ["TB_BaconShop_HERO_30", "TB_BaconShop_HP_043", "nefarian"],
  ["TB_BaconShop_HERO_11", "TB_BaconShop_HP_019", "ragnaros"],
  ["TB_BaconShop_HERO_18", "TB_BaconShop_HP_027", "patches"],
  ["TB_BaconShop_HERO_20", "TB_BaconShop_HP_018", "putricide"],
  ["TB_BaconShop_HERO_22", "TB_BaconShop_HP_024", "lichking"],
  ["TB_BaconShop_HERO_15", "TB_BaconShop_HP_010", "george"],
  ["TB_BaconShop_HERO_35", "TB_BaconShop_HP_039", "yogg"],
  ["TB_BaconShop_HERO_27", "TB_BaconShop_HP_014", "sindragosa"],
  ["TB_BaconShop_HERO_36", "TB_BaconShop_HP_042", "deryl"],
  ["TB_BaconShop_HERO_14", "TB_BaconShop_HP_037a", "wagtoggle"],
  ["TB_BaconShop_HERO_12", "TB_BaconShop_HP_041a", "ratking"],
  ["TB_BaconShop_HERO_23", "TB_BaconShop_HP_022", "shudderwock"],
  ["TB_BaconShop_HERO_28", "TB_BaconShop_HP_028", "toki"],
  ["TB_BaconShop_HERO_16", "TB_BaconShop_HP_044", "afkay"],
  ["TB_BaconShop_HERO_10", "TB_BaconShop_HP_008", "gallywix"],
  ["TB_BaconShop_HERO_25", "TB_BaconShop_HP_049", "lichbaz"],
  ["TB_BaconShop_HERO_38", "TB_BaconShop_HP_038", "mukla"],
];
const heroes = heroSpecs.map(([id, powerId, key]) => {
  const h = source.find((c) => c.id === id),
    p = source.find((c) => c.id === powerId);
  return {
    id,
    key,
    name: h.name,
    health: key === "patchwerk" ? 60 : 40,
    power: p.name,
    cost: p.cost,
    text: clean(p.text),
    passive: clean(p.text).includes("Passive"),
    art: `art/${id}.${tiles ? 'png' : 'webp'}`,
  };
});
if (renders) for (const c of [...cards,...heroes]) {
  const original=source.find(s=>s.id===c.id && images.cards.en_US[s.dbfId]) || source.find(s=>s.name===c.name && images.cards.en_US[s.dbfId]);
  if(original){c.art=`art/render_${c.id}.png`;renderMap.set(c.id,original);}
}
await fs.mkdir("src/data", { recursive: true });
await fs.mkdir("public/art", { recursive: true });
const hash = createHash("sha256").update(JSON.stringify(cards)).digest("hex");
await fs.writeFile("src/data/cards.json", JSON.stringify(cards, null, 2));
await fs.writeFile("src/data/heroes.json", JSON.stringify(heroes, null, 2));
await fs.writeFile(
  "src/data/manifest.json",
  JSON.stringify(
    {
      build,
      patch: "15.6",
      title: "Origins",
      hash,
      source: `https://api.hearthstonejson.com/v1/${build}/enUS/cards.json`,
      cardCount: cards.length,
      heroCount: heroes.length,
      notes: [
        "Historical tiers are a reviewed local manifest, not supplied by this old JSON schema.",
        "Akazamzarak secrets and exact historical random-summon pools remain outside the supported ruleset.",
      ],
    },
    null,
    2,
  ),
);
console.log(
  `Normalized ${cards.length} minions and ${heroes.length} heroes from build ${build}`,
);
if (process.argv.includes("--art")) {
  const entries = [...cards, ...heroes];
  let count = 0;
  const resources = [];
  async function fetchArt(c) {
    const original=renderMap.get(c.id);
    const url = original ? `https://cdn.jsdelivr.net/gh/schmich/hearthstone-card-images@${images.config.version}/cards/en_US/${original.dbfId}.png` : tiles ? `https://cdn.jsdelivr.net/gh/HearthSim/hs-card-tiles@master/Tiles/${c.id}.png` : `https://art.hearthstonejson.com/v1/256x/${c.id}.webp`;
    try {
      const bytes = await fs.readFile("public/" + c.art);
      if (bytes.length < 100 || bytes.subarray(0,5).toString().includes('<')) throw Error('Invalid cached image');
      count++;
      resources.push({id:c.id,path:c.art,url,sourceId:original?.id||c.id,sha256:createHash('sha256').update(bytes).digest('hex')});
      return;
    } catch {}
    try {
      await run(
        "curl.exe",
        [
          "-L",
          "--fail",
          "--retry",
          "2",
          "--max-time",
          "15",
          url,
          "-o",
          "public/" + c.art,
        ],
        { windowsHide: true },
      );
      const bytes=await fs.readFile('public/'+c.art);
      if(bytes.length<100 || bytes.subarray(0,5).toString().includes('<'))throw Error('Not an image');
      resources.push({id:c.id,path:c.art,url,sourceId:original?.id||c.id,sha256:createHash('sha256').update(bytes).digest('hex')});
      count++;
    } catch {
      console.error("Art unavailable:", c.id);
    }
  }
  for(let i=0;i<entries.length;i+=5)await Promise.all(entries.slice(i,i+5).map(fetchArt));
  await fs.writeFile('public/art/manifest.json',JSON.stringify({format:renders?'Archived card renders with HearthSim tile fallbacks':tiles?'HearthSim archived card tiles':'HearthstoneJSON portraits',resources},null,2));
  console.log(`Cached ${count}/${entries.length} portraits`);
}
