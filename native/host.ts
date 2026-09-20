import fs from 'node:fs';
import path from 'node:path';
import readline from 'node:readline';
import {BASE_CARDS,HEROES,newGame,clone,act,beginRound,makeUnit,returnUnit,emit,validateCards,contentHash} from '../src/game/engine';
import {combat,resolveRound,recruitPresentation} from '../src/game/combat';
import {recruitAI} from '../src/game/ai';
import {parseGame} from '../src/game/save';
import type {Game,Card} from '../src/game/types';

// This process has no renderer, browser, HTTP listener or network dependency.
// The raylib executable owns the window. JSON commands travel over private pipes.
const dataDir=process.env.BOBS_DATA_DIR||path.join(process.env.LOCALAPPDATA||process.cwd(),'BobsBattlegrounds');
fs.mkdirSync(dataDir,{recursive:true});
function read<T>(name:string,fallback:T):T {for(const suffix of ['.json','.json.bak'])try{return JSON.parse(fs.readFileSync(path.join(dataDir,name+suffix),'utf8'));}catch{}return fallback;}
function write(name:string,value:unknown){const filename=path.join(dataDir,name+'.json');fs.writeFileSync(filename+'.tmp',JSON.stringify(value));if(fs.existsSync(filename))fs.copyFileSync(filename,filename+'.bak');fs.renameSync(filename+'.tmp',filename);}
let cards:Card[]=BASE_CARDS;
try{cards=validateCards(read('cards',BASE_CARDS));}catch{}
let game:Game|null=null;
try {const saved=read('save',null);if(saved)game=parseGame(saved);}catch{}
let history:Game[]=read('history',[]);if(!Array.isArray(history))history=[];
let lab=newGame(42,'patchwerk',cards);lab.players[0].board=[];lab.players[1].board=[];lab.modified=true;
let settings=read('settings',{volume:.4,speed:1,fullscreen:false});
const undo:Game[]=[];
function persist(next:Game){if(game){undo.push(game);if(undo.length>30)undo.shift();}game=next;write('save',game);if(game.phase==='finished'){history=[game,...history.filter(h=>h.createdAt!==game!.createdAt)].slice(0,12);write('history',history);}}
function state(){return game?{...game,players:game.players.map(recruitPresentation),history:undefined}:null;}
function metadata(){return history.map(h=>({id:h.createdAt,seed:h.seed,rounds:h.round,hero:h.players[0].hero,placement:h.players[0].placement,modified:h.modified}));}
function required(){if(!game)throw Error('Start a match first.');return clone(game);}
function dispatch(m:any){let extra:Record<string,unknown>={};
 switch(m.type){
  case 'boot':extra={heroes:HEROES,cards,settings,history:metadata(),lab,dataDir};break;
  case 'new':{const seed=Number(m.seed);if(!Number.isInteger(seed)||seed<1||seed>0xffffffff)throw Error('Seed must be between 1 and 4294967295.');persist(newGame(seed,m.hero,cards,m.difficulty));undo.length=0;break;}
  case 'action':{const next=required();const presentation:any[]=[];let prior='';
   act(next,0,m.command,(kind,player,source)=>{const signature=JSON.stringify(player);if(signature!==prior){presentation.push({kind,player:recruitPresentation(player),source});prior=signature;}});
   if(presentation.length)presentation.push({kind:'settle',player:recruitPresentation(next.players[0])});
   persist(next);extra.presentation=presentation;break;}
  case 'end':{const next=required();if(next.players[0].discovers.length)throw Error('Choose a discovery first.');for(const p of next.players.slice(1))recruitAI(next,p);resolveRound(next);persist(next);extra.history=metadata();break;}
  case 'advance':{const next=required();if(next.phase!=='combat')throw Error('The round cannot advance now.');beginRound(next);persist(next);break;}
  case 'autoplay':{const next=required();if(next.phase==='combat')beginRound(next);if(next.phase==='recruit'){for(const p of next.players)recruitAI(next,p);resolveRound(next);}persist(next);extra.history=metadata();break;}
  case 'rewind':{const prev=undo.pop();if(!prev)throw Error('No earlier snapshot in this session.');prev.modified=true;game=prev;write('save',game);break;}
  case 'debug':{const next=required(),p=next.players[Number(m.player)||0];if(!p)throw Error('Invalid player');const [cmd,arg]=String(m.command).trim().split(/\s+/),n=Number(arg);
   if(cmd==='gold'||cmd==='health'||cmd==='tier'){const min=cmd==='gold'?0:1,max=cmd==='tier'?6:cmd==='health'?999:100;if(!Number.isInteger(n)||n<min||n>max)throw Error(`Use a value from ${min} to ${max}`);if(p.placement)throw Error('Cannot edit an eliminated player');p[cmd]=n;}
   else if(cmd==='spawn'){if(p.board.length>=7)throw Error('The warband is full');p.board.push(makeUnit(next,arg,false,0));}
   else if(cmd==='give'){if(p.hand.length>=10)throw Error('Your hand is full');p.hand.push(makeUnit(next,arg,false,0));}
   else if(cmd==='clear'){p.board.forEach(u=>returnUnit(next,u));p.board=[];}
   else if(cmd==='seed'){if(!Number.isInteger(n)||n<1||n>0xffffffff)throw Error('Invalid seed');next.rng=n;}
   else throw Error('Commands: gold N, health N, tier N, spawn CARD_ID, give CARD_ID, clear, seed N');
   next.modified=true;emit(next,'debug',m.command);persist(next);break;}
  case 'card.save':{const next=clone(cards);const index=next.findIndex(c=>c.id===m.card.id);if(index<0)next.push(m.card);else next[index]=m.card;cards=validateCards(next);write('cards',cards);extra.cards=cards;break;}
  case 'card.reset':{const base=BASE_CARDS.find(c=>c.id===m.cardId);if(!base)throw Error('No base definition');cards=cards.map(c=>c.id===m.cardId?clone(base):c);write('cards',cards);extra.cards=cards;break;}
  case 'pack.import':{cards=validateCards(JSON.parse(fs.readFileSync(m.path,'utf8')));write('cards',cards);extra.cards=cards;break;}
  case 'pack.export':{const target=path.join(dataDir,'card-pack-export.json');fs.writeFileSync(target,JSON.stringify(cards,null,2));extra.message='Exported card-pack-export.json';break;}
  case 'save.export':{if(!game)throw Error('No match to export');const target=path.join(dataDir,`match-${game.seed}.json`);fs.writeFileSync(target,JSON.stringify(game));extra.message=`Exported match-${game.seed}.json`;break;}
  case 'save.import':persist(parseGame(JSON.parse(fs.readFileSync(m.path,'utf8'))));break;
  case 'history':extra.history=metadata();break;
  case 'replay':{const saved=history.find(h=>h.createdAt===m.matchId);if(!saved)throw Error('Match not found');const round=saved.history[Number(m.round)||0];if(!round)throw Error('Round not found');extra.replay={result:round.combats.find(c=>c.leftId===0||c.rightId===0)||round.combats[0],players:round.players,round:round.round};break;}
  case 'settings':settings={...settings,...m.settings};write('settings',settings);extra.settings=settings;break;
  case 'lab.new':{lab=newGame(Number(m.seed)||42,'patchwerk',cards);lab.players[0].board=[];lab.players[1].board=[];lab.modified=true;extra.lab=lab;break;}
  case 'lab.add':{const next=clone(lab),p=next.players[m.side===1?1:0];if(p.board.length>=7)throw Error('A warband holds seven minions');p.board.push(makeUnit(next,m.cardId,!!m.golden,0));lab=next;extra.lab=lab;break;}
  case 'lab.remove':{lab.players[m.side===1?1:0].board.splice(m.index,1);extra.lab=lab;break;}
  case 'lab.stat':{const u=lab.players[m.side===1?1:0].board[m.index];if(!u)throw Error('No minion selected');const v=Number(m.value);if(!Number.isInteger(v)||v<(m.stat==='attack'?0:1)||v>9999)throw Error('Invalid stat');if(m.stat==='attack')u.attack=v;else u.health=u.maxHealth=v;extra.lab=lab;break;}
  case 'lab.run':{const next=clone(lab);next.rng=Number(m.seed)>>>0||42;extra.replay={result:combat(next,next.players[0],next.players[1]),players:lab.players,round:0};break;}
  case 'lab.odds':{const next=clone(lab);next.rng=Number(m.seed)>>>0||42;let wins=0,ties=0,capped=0;const samples=Math.min(1000,Math.max(1,Number(m.samples)||100));for(let i=0;i<samples;i++){const c=combat(next,next.players[0],next.players[1],false);if(c.winner===0)wins++;if(c.winner===null)ties++;if(c.capped)capped++;}extra.odds={wins,ties,losses:samples-wins-ties,samples,capped};break;}
  case 'lab.export':fs.writeFileSync(path.join(dataDir,'scenario.json'),JSON.stringify(lab));extra.message='Exported scenario.json';break;
  case 'lab.import':lab=parseGame(JSON.parse(fs.readFileSync(m.path,'utf8')));extra.lab=lab;break;
  default:throw Error('Unknown request');
 }
 if(extra.lab)extra.lab={...lab,players:lab.players.map(recruitPresentation)};
 return {id:m.id,ok:true,type:m.type,game:state(),...extra};
}
const rl=readline.createInterface({input:process.stdin,crlfDelay:Infinity});
rl.on('line',line=>{let message:any;try{if(line.length>40_000_000)throw Error('Request too large');message=JSON.parse(line);process.stdout.write(JSON.stringify(dispatch(message))+'\n');}catch(e){process.stdout.write(JSON.stringify({id:message?.id,ok:false,error:e instanceof Error?e.message:String(e)})+'\n');}});
rl.on('close',()=>process.exit(0));
