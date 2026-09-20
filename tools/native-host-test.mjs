import {spawn} from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import readline from 'node:readline';
import assert from 'node:assert/strict';
const dir=path.resolve('test-results/native-host-'+Date.now());
fs.mkdirSync(dir,{recursive:true});
let child,sequence=0,pending=new Map();
function launch(){child=spawn(process.execPath,['release/native/rules.cjs'],{env:{...process.env,BOBS_DATA_DIR:dir},windowsHide:true,stdio:['pipe','pipe','inherit']});readline.createInterface({input:child.stdout}).on('line',line=>{const m=JSON.parse(line),p=pending.get(m.id);if(p){clearTimeout(p.timer);pending.delete(m.id);p.resolve(m);}});}
function call(type,args={}){return new Promise((resolve,reject)=>{const id=++sequence;pending.set(id,{resolve,timer:setTimeout(()=>reject(Error('Timeout: '+type)),15000)});child.stdin.write(JSON.stringify({id,type,...args})+'\n');});}
async function ok(type,args){const m=await call(type,args);assert.equal(m.ok,true,m.error);return m;}
async function close(){const done=new Promise(r=>child.on('exit',r));child.stdin.end();await done;}
try{
 launch();let m=await ok('boot');assert.equal(m.cards.length,80);assert.equal(m.heroes.length,23);
 const warleader=m.cards.find(c=>c.name==='Murloc Warleader').id;
 await ok('new',{seed:35747,hero:'curator'});
 await ok('debug',{player:0,command:'give '+warleader});
 m=await ok('action',{command:{type:'play',index:0}});
 assert.equal(m.game.players[0].board[0].attack,3,'Amalgam must display Warleader aura');
 assert.equal(m.presentation[0].player.board[0].attack,3,'Entry presentation includes aura');
 assert.equal(JSON.parse(fs.readFileSync(path.join(dir,'save.json'),'utf8')).players[0].board[0].attack,1,'Aura must not enter saved stats');
 await close();launch();m=await ok('boot');assert.equal(m.game.players[0].board[0].attack,3,'Resume reapplies aura once');
 m=await ok('action',{command:{type:'sell',index:1}});assert.equal(m.game.players[0].board[0].attack,1,'Selling source removes aura');
 m=await ok('new',{seed:35747,hero:'curator',difficulty:'standard'});const hash=m.game.contentHash;
 m=await ok('action',{command:{type:'buy',index:0}});assert.equal(m.game.players[0].hand.length,1);
 m=await ok('action',{command:{type:'play',index:0}});assert(m.game.players[0].board.length>=2);
 m=await ok('end');assert(m.game.lastCombat.frames.length>0);
 m=await ok('advance');assert.equal(m.game.round,2);
 m=await ok('boot');const card={...m.cards[0],attack:m.cards[0].attack+10};
 m=await ok('card.save',{card});assert.equal(m.game.contentHash,hash);assert.equal(m.cards[0].attack,card.attack);
 await ok('pack.export');assert(fs.existsSync(path.join(dir,'card-pack-export.json')));
 await ok('debug',{player:0,command:'gold 10'});m=await ok('rewind');assert.notEqual(m.game.players[0].gold,10);
 const invalid=await call('save.import',{path:path.join(dir,'cards.json')});assert.equal(invalid.ok,false);
 await ok('lab.new');await ok('lab.add',{side:0,cardId:card.id});await ok('lab.add',{side:1,cardId:card.id,golden:true});
 m=await ok('lab.odds',{samples:100,seed:42});assert.equal(m.odds.wins+m.odds.ties+m.odds.losses,100);
 m=await ok('lab.run',{seed:42});assert(m.replay.result.frames.length>0);
 await ok('save.export');await close();launch();m=await ok('boot');assert.equal(m.game.round,2);assert.equal(m.cards[0].attack,card.attack);
 let turns=0;while(m.game.phase!=='finished'&&turns++<100)m=await ok('autoplay');assert.equal(m.game.phase,'finished');
 m=await ok('history');assert.equal(m.history.length,1);const match=m.history[0];
 m=await ok('replay',{matchId:match.id,round:match.rounds-1});assert.equal(m.replay.round,match.rounds);
 m=await ok('card.reset',{cardId:card.id});assert.equal(m.cards[0].attack,card.attack-10);
 await close();console.log('Native host passed: gameplay, edits, undo, imports, lab, restart, full match and replay.');
}catch(e){child?.kill();throw e;}
