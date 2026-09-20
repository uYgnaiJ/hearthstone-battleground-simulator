import fs from 'node:fs/promises';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import {createHash} from 'node:crypto';
const run=promisify(execFile),source=JSON.parse(await fs.readFile('cards-source.json','utf8')),heroes=JSON.parse(await fs.readFile('src/data/heroes.json','utf8'));
const resources=[];
for(let start=0;start<heroes.length;start+=4)await Promise.all(heroes.slice(start,start+4).map(async hero=>{
 const card=source.find(c=>c.name===hero.power&&c.id.startsWith('TB_BaconShop'))||source.find(c=>c.name===hero.power);if(!card)return;
 const path=`art/power_${hero.key}.png`,url=`https://cdn.jsdelivr.net/gh/HearthSim/hs-card-tiles@master/Tiles/${card.id}.png`;
 try{await fs.access('public/'+path);}catch{try{await run('curl.exe',['-sSL','--fail','--max-time','15',url,'-o','public/'+path],{windowsHide:true});}catch{return;}}
 const data=await fs.readFile('public/'+path);resources.push({key:hero.key,path,url,sourceId:card.id,sha256:createHash('sha256').update(data).digest('hex')});
}));
await fs.writeFile('public/art/power-manifest.json',JSON.stringify({copyright:'Blizzard Entertainment',resources},null,2));console.log(`Cached ${resources.length}/${heroes.length} hero-power icons`);
