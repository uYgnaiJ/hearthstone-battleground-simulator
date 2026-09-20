import fs from 'node:fs/promises';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import {createHash} from 'node:crypto';
const run=promisify(execFile), read=async p=>JSON.parse(await fs.readFile(p,'utf8'));
const heroes=await read('src/data/heroes.json'), cards=await read('src/data/cards.json'), source=await read('cards-source.json');
const jobs=new Map();
for(const c of [...heroes,...cards])jobs.set(`full_${c.id}`,c.id);
// The original Battlegrounds Pyramad art is unavailable; the adventure uses the same hero.
jobs.set('full_TB_BaconShop_HERO_39','ULDA_BOSS_12h');
jobs.set('bob','DALA_BOSS_99h');
for(const h of heroes){const c=source.find(c=>c.name===h.power&&c.id.startsWith('TB_BaconShop'));
 if(c)jobs.set(`power_${h.key}`,c.id);
}
const resources=[],failed=[];
const queue=[...jobs];
await Promise.all(Array.from({length:4},async()=>{while(queue.length){const [name,id]=queue.shift(),path=`art/${name}.png`,url=`https://art.hearthstonejson.com/v1/orig/${id}.png`;
 try{try{await fs.access('public/'+path);}catch{await run(process.platform==='win32'?'curl.exe':'curl',['-fsSL','--retry','1','--max-time','30',url,'-o','public/'+path]);}
 const data=await fs.readFile('public/'+path);if(data.readUInt32BE(0)!==0x89504e47)throw Error('Not PNG');
 resources.push({path,url,sourceId:id,sha256:createHash('sha256').update(data).digest('hex')});
 }catch{failed.push(id);await fs.rm('public/'+path,{force:true});}
}}));
resources.sort((a,b)=>a.path.localeCompare(b.path));
await fs.writeFile('public/art/presentation-manifest.json',JSON.stringify({copyright:'Blizzard Entertainment',resources,failed},null,2)+'\n');
console.log(`Cached ${resources.length}/${jobs.size} full artwork images; missing: ${failed.join(', ')}`);
