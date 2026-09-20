import fs from 'node:fs/promises';
import {createRequire} from 'node:module';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import {createHash} from 'node:crypto';
const run=promisify(execFile),images=createRequire(import.meta.url)('hearthstone-card-images');
const source=JSON.parse(await fs.readFile('cards-source.json','utf8'));
const names=['Amalgam','Tabbycat','Murloc Scout','Imp','Guard Bot','Jo-E Bot','Damaged Golem','Big Bad Wolf','Spider','Rat','Microbot','Hyena','Primalfin','Robosaur','Voidwalker','Finkle Einhorn','Ironhide Runt','Plant'];
const map={},resources=[];
await Promise.all(names.map(async name=>{
 const c=source.find(c=>c.name===name&&images.cards.en_US[c.dbfId]);
 const fallback=source.find(c=>c.name===name);if(!c&&!fallback)return;
 const original=c||fallback,art=`art/${c?'render_':''}token_${original.id}.png`;
 const url=c?`https://cdn.jsdelivr.net/gh/schmich/hearthstone-card-images@${images.config.version}/cards/en_US/${c.dbfId}.png`:`https://cdn.jsdelivr.net/gh/HearthSim/hs-card-tiles@master/Tiles/${original.id}.png`;
 try{try{await fs.access('public/'+art);}catch{await run('curl.exe',['-sSL','--fail','--max-time','25','--retry','1',url,'-o','public/'+art],{windowsHide:true});}const bytes=await fs.readFile('public/'+art);map[name]=art;resources.push({name,path:art,url,sourceId:original.id,sha256:createHash('sha256').update(bytes).digest('hex')});}catch{console.log('Unavailable token: '+name);}
}));
await fs.writeFile('src/data/token-art.json',JSON.stringify(map,null,2));
await fs.writeFile('public/art/token-manifest.json',JSON.stringify({resources},null,2));console.log('Token portraits: '+resources.length);
