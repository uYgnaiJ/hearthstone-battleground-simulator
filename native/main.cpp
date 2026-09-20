#include <iostream>
#include "bridge.hpp"
#include "render.hpp"
#include "audio.hpp"
#include "motion.hpp"
#include <fstream>
#include <chrono>
#include <iomanip>

using namespace ui;
static json emptyArray=json::array();
static std::string s(const json& j,const char* key,const std::string& fallback=""){return j.is_object()&&j.contains(key)&&j[key].is_string()?j[key].get<std::string>():fallback;}
static int num(const json& j,const char* key,int fallback=0){return j.is_object()&&j.contains(key)&&j[key].is_number()?j[key].get<int>():fallback;}
static bool flag(const json& j,const char* key){return j.is_object()&&j.contains(key)&&j[key].is_boolean()&&j[key].get<bool>();}
static const json& array(const json& j,const char* key){return j.is_object()&&j.contains(key)&&j[key].is_array()?j[key]:emptyArray;}
static bool keyword(const json& unit,const char* key){for(const auto& k:array(unit,"keywords"))if(k==key)return true;return false;}
static Rectangle rect(float x,float y,float w,float h){return {x,y,w,h};}
struct Particle{Vector2 p,v;float age,life,size;Color color;int kind;};
struct Floater{Vector2 p;std::string text;float age;Color color;};
struct Hit{std::string zone;int index;Rectangle bounds;json unit;};
struct Flight{json unit;Vector2 from,to;float age=0,life=.38f;int style=0;bool started=false;};
struct ShieldWave{int uid;Vector2 position;float age=0;bool gain=true;};

class NativeGame {
 RulesBridge bridge;GameAudio audio;
 json game=nullptr,cards=emptyArray,heroes=emptyArray,lab=nullptr,history=emptyArray,settings=json::object(),replay=nullptr,odds=nullptr;
 std::filesystem::path directory;std::string scene="menu",overlay,returnScene="menu",dataDirectory;
 bool busy=true,ready=false,exitGame=false,debug=false,paused=false,replayOnly=false,labReplay=false,showHelp=false;
 std::string notice;float noticeTime=0,shake=0;
 std::vector<Particle> particles;std::vector<Floater> floaters;std::vector<Hit> hits;
 std::vector<Flight> flights;bool drawingFlight=false;
 std::vector<ShieldWave> shieldWaves;int buffScroll=0;
 std::deque<json> recruitSteps;json recruitView=nullptr;float recruitClock=0,recruitDuration=0;std::string recruitKind;int recruitSource=-1;
 json pendingDrag=nullptr;Vector2 pendingDragPosition{};int handHover=-1;std::map<int,float> handLift;
 json hoverUnit=nullptr,editing=nullptr;
 std::string selectedZone;int selectedIndex=-1;json selectedUnit=nullptr;
 bool dragging=false;Vector2 dragStart{};json dragged=nullptr;std::string dragZone;int dragIndex=-1;
 int heroPage=0,heroChoice=0,cardPage=0,historyChoice=0,historyRound=0,labSide=0,labSelected=-1,debugPlayer=0;
 bool labGolden=false,collectionForLab=false;
 std::string seed="35747",search,consoleLine,editorName,editorText,editorEffect;
 int difficulty=1,tierFilter=0;
 int animationFrame=0;float animationTime=0;bool frameImpact=false;float speed=1;
 bool resultCelebrated=false;float finishClock=0;bool finishHit=false;
 RenderTexture2D transitionFrame{};float transitionClock=2;std::string transitionLabel;
 bool smokeEntryCaptured=false,smokeSummonCaptured=false;json statusReplay=nullptr;
 int smokeStage=0,smokeWait=0;bool smoke=false;float smokeTime=0;std::string smokeError;
 Texture2D board{};RenderTexture2D canvas{};float hoverStarted=0;int lastHoverUid=-1;std::string lastHoverId;

 void tell(const std::string& message){notice=message;noticeTime=5.5f;}
 bool presenting()const{return !recruitView.is_null();}
 void send(json message){if(busy)return;auto type=s(message,"type");if(presenting()&&(type=="action"||type=="end"||type=="autoplay"||type=="rewind"||type=="debug"))return;if(!bridge.running()){tell("The rules engine is unavailable. Restart to restore your save.");return;}busy=true;if(bridge.send(message)<0){busy=false;tell("Could not send the game action.");}}
 void action(json command){if(!busy&&dragging&&!dragged.is_null()){pendingDrag=dragged;pendingDragPosition=mouse;}send({{"type","action"},{"command",command}});selectedZone.clear();selectedIndex=-1;}
 bool moving(const json& unit)const{if(!unit.contains("uid"))return false;int uid=num(unit,"uid");if(dragging&&num(dragged,"uid",-1)==uid)return true;if(num(pendingDrag,"uid",-1)==uid)return true;for(const auto& f:flights)if(num(f.unit,"uid",-1)==uid)return true;return false;}
 Vector2 handPosition(int index,int count)const{float spacing=std::min(112.f,650.f/std::max(1,count));return {815-(count-1)*spacing/2+index*spacing,833};}
 Vector2 originalPosition(const json& unit)const{for(const char* zone:{"shop","board","hand"}){const auto& list=array(player(),zone);for(int i=0;i<(int)list.size();i++)if(num(list[i],"uid")==num(unit,"uid"))return std::string(zone)=="hand"?handPosition(i,list.size()):position(i,list.size(),std::string(zone)=="shop");}return {815,805};}
 void returnDragged(){if(dragging&&!dragged.is_null())flights.push_back({dragged,mouse,originalPosition(dragged),0,.22f});dragged=nullptr;dragging=false;}
 bool btn(const std::string& label,Rectangle r,bool enabled=true,bool blue=false,float size=23){bool pressed=ui::button(label,r,enabled&&!busy&&(!presenting()||!overlay.empty()||debug||label=="Menu"),blue,size);if(pressed)audio.play(3);return pressed;}
 const json& player()const{return presenting()?recruitView:game.is_object()&&!array(game,"players").empty()?game["players"][0]:emptyArray;}
 const json* definition(const std::string& id)const{const auto& pool=game.is_object()?array(game,"cards"):cards;for(const auto& c:pool)if(s(c,"id")==id)return &c;for(const auto& c:cards)if(s(c,"id")==id)return &c;return nullptr;}
 void spark(Vector2 p,Color color,int count=20,int kind=0){for(int i=0;i<count;i++){float a=GetRandomValue(0,628)/100.f,v=GetRandomValue(45,210);particles.push_back({p,{cosf(a)*v,sinf(a)*v},0,GetRandomValue(35,90)/100.f,GetRandomValue(2,6)*1.f,color,kind});}}
 bool transitioning()const{return transitionClock<1.05f;}
 void beginTransition(const std::string& label){if(!transitionFrame.id||!canvas.id)return;BeginTextureMode(transitionFrame);ClearBackground(BLANK);DrawTexturePro(canvas.texture,{0,0,W,-H},{0,0,W,H},{0,0},0,WHITE);EndTextureMode();transitionClock=0;transitionLabel=label;audio.play(2);}
 void drawTransition(){if(!transitioning()||!board.id)return;float t=motion::smooth(transitionClock/1.05f),roll=sinf(t*PI);for(int i=0;i<4;i++){bool right=i%2,bottom=i/2;float x=right?1210:0,y=bottom?650:0;Vector2 center={x+195,y+125};Rectangle src={x/W*board.width,y/H*board.height,390.f/W*board.width,250.f/H*board.height};float uv[]={src.x/board.width,src.y/board.height,src.width/board.width,src.height/board.height};BeginShaderMode(panelShader);SetShaderValue(panelShader,panelUv,uv,SHADER_UNIFORM_VEC4);DrawTexturePro(board,src,{center.x+(right?1:-1)*35*roll,center.y+(bottom?1:-1)*25*roll,390,250},{195,125},((right==bottom)?1:-1)*70*roll,Fade(WHITE,.9f));EndShaderMode();}float a=sinf(transitionClock/1.05f*PI);DrawRectangle(0,0,W,H,Fade({35,19,15,255},a*.22f));if(a>.35f){panel({577,374,446,105},{60,35,24,245});text(transitionLabel,800,408,36,Fade(Pale,a),true,true);}}
 bool powerOrb(const json& p,Vector2 center,bool usable=false){if(!p.is_object())return false;const auto& hero=p["hero"];bool passive=flag(hero,"passive"),used=flag(p,"powerUsed"),enabled=usable&&!passive&&!used&&!busy&&!presenting()&&!transitioning()&&num(p,"gold")>=num(hero,"cost");bool hot=!blocked&&Vector2Distance(mouse,center)<43;wantsPointer|=hot&&enabled;DrawCircleV({center.x+3,center.y+5},46,{26,15,13,230});DrawCircleV(center,44,Gold);DrawCircleV(center,39,{49,30,22,255});DrawCircleGradient(center,35,enabled?Color{78,160,210,255}:Color{63,102,128,255},{25,42,63,255});for(int i=0;i<6;i++){float a=i*PI/3-.3f;Vector2 outer={center.x+cosf(a)*27,center.y+sinf(a)*27};DrawLineEx(center,outer,2,Fade({173,225,248,255},.45f));}star(center.x,center.y,21,{161,224,245,255});std::string powerArt="art/power_"+s(hero,"key")+".png";if(texture(powerArt).id)portrait(powerArt,{center.x-34,center.y-34,68,68});DrawCircleLines(center.x,center.y,32,Fade(Pale,.7f));if(hot)DrawCircleLines(center.x,center.y,46,Pale);gem(center.x-30,center.y+31,17,{35,91,193,255},std::to_string(num(hero,"cost")),21,6);if(passive||used)text(passive?"Passive":"Used",center.x,center.y+48,14,Gold,true,true);else text(s(hero,"power"),center.x,center.y+48,15,Pale,true,true);if(hot){float x=center.x+55,y=std::clamp(center.y-52,12.f,730.f);panel({x,y,318,120});text(s(hero,"power"),x+15,y+12,20,Pale,true);wrapped(s(hero,"text"),x+15,y+43,288,16,Cream);}if(hot&&enabled&&clicked){audio.play(3);return true;}return false;}
 void setReplay(json value,bool only,bool fromLab=false){if(scene=="tavern")beginTransition("COMBAT");replay=std::move(value);if(replay.contains("result")&&num(replay["result"],"rightId",-1)==0){auto& r=replay["result"];std::swap(r["leftId"],r["rightId"]);for(auto& f:r["frames"])std::swap(f["left"],f["right"]);}replayOnly=only;labReplay=fromLab;animationFrame=0;animationTime=0;paused=false;frameImpact=false;resultCelebrated=false;finishClock=0;finishHit=false;flights.clear();shieldWaves.clear();scene="combat";selectedZone.clear();audio.play(2);}
 void receive(){json response;while(bridge.poll(response)){busy=false;if(!response.value("ok",false)){if(!pendingDrag.is_null()){flights.push_back({pendingDrag,pendingDragPosition,originalPosition(pendingDrag),0,.22f});pendingDrag=nullptr;}tell(s(response,"error","Unknown rules error"));if(smoke)smokeError=s(response,"error");continue;}
  const std::string type=s(response,"type");if(response.contains("game")){if(type=="action"&&game.is_object()&&response["game"].is_object()){if(!array(response,"presentation").empty()){recruitView=game["players"][0];recruitSteps.clear();for(const auto& step:response["presentation"])recruitSteps.push_back(step);nextRecruitStep();}else animateRecruit(game["players"][0],response["game"]["players"][0]);}game=response["game"];}
  if(response.contains("cards"))cards=response["cards"];if(response.contains("heroes"))heroes=response["heroes"];if(response.contains("lab"))lab=response["lab"];if(response.contains("history"))history=response["history"];if(response.contains("settings")){settings=response["settings"];speed=settings.value("speed",1.f);audio.volume=settings.value("volume",.4f);}if(response.contains("dataDir"))dataDirectory=s(response,"dataDir");
  if(type=="boot"){ready=true;busy=false;if(!smoke&&settings.value("fullscreen",false)!=IsWindowFullscreen())ToggleFullscreen();}
  if(type=="new"){focus.clear();flights.clear();scene="tavern";overlay.clear();selectedZone.clear();}
  if(type=="end"||type=="autoplay"){if(game.contains("lastCombat"))setReplay({{"result",game["lastCombat"]},{"players",game["players"]},{"round",game["round"]}},false);else scene="tavern";}
  if(type=="advance"||type=="save.import"){if(scene=="combat")beginTransition("RECRUIT");scene="tavern";overlay.clear();}
  if(type=="action")pendingDrag=nullptr;
  if(type=="card.save"||type=="card.reset"){focus.clear();editing=nullptr;tell("Card saved. New matches and new lab scenarios use this version.");}
  if(type=="rewind")tell("Restored the previous state. This match is marked modified.");
  if(response.contains("replay"))setReplay(response["replay"],true,type=="lab.run");
  if(response.contains("odds")){odds=response["odds"];tell("Simulation complete.");}
  if(response.contains("message"))tell(s(response,"message"));
 }}
 void drawBackground(){if(board.id)DrawTexturePro(board,{0,0,(float)board.width,(float)board.height},{0,0,1600,900},{0,0},0,WHITE);else ClearBackground({92,53,29,255});
  // Gently animated hearth light on the physical board corners.
  for(int i=0;i<3;i++){float a=.055f+.014f*sinf(ui::time*2.1f+i);DrawCircleGradient({35,410},110,Fade(ORANGE,a),Fade(ORANGE,0));DrawCircleGradient({1540,520},110,Fade(ORANGE,a),Fade(ORANGE,0));}
 }
 Color statColor(const json& u,bool health)const{const auto* def=definition(s(u,"cardId"));if(!def)return num(u,health?"auraHealth":"auraAttack")>0?Color{106,240,84,255}:WHITE;const char* key=health?"health":"attack";int base=num(*def,key)*(flag(u,"golden")?2:1);if(health&&num(u,"health")<num(u,"maxHealth",num(u,"health")))return {255,91,78,255};return num(u,key)>base?Color{106,240,84,255}:WHITE;}
 void cardVisual(const json& u,Rectangle r,bool details=true){
  float sx=r.width/600,sy=r.height/830;bool golden=flag(u,"golden");auto box=[&](float x,float y,float w,float h){return Rectangle{r.x+x*sx,r.y+y*sy,w*sx,h*sy};};
  portrait(s(u,"art"),box(125,17,350,472));layer(golden?"base-minion-premium.png":"frame-minion-neutral.png",box(34,9,528,793));
  layer(golden?"name-banner-minion-premium.png":"name-banner-minion.png",box(60,389,485,113));
  std::string name=s(u,"name");float titleSize=std::max(10.f,47*sy);while(MeasureTextEx(title,name.c_str(),titleSize,0).x>r.width*.76f&&titleSize>8)titleSize--;
  text(name,r.x+302*sx,r.y+436*sy,titleSize,Pale,true,true);
  auto def=u.contains("id")?&u:definition(s(u,"cardId"));std::string desc=def?(golden?s(*def,"goldenText",s(*def,"text")):s(*def,"text")):s(u,"text");
  if(details){float bodySize=std::max(9.f,38*sy);if(desc.size()>130)bodySize*=.88f;wrapped(desc,r.x+300*sx,r.y+553*sy,403*sx,bodySize,golden?Cream:Ink,true);}
  else for(int i=0;i<num(u,"tier",1);i++)star(r.x+(300+(i-(num(u,"tier",1)-1)*.5f)*34)*sx,r.y+621*sy,12*sx,Gold);
  std::string tribe=s(u,"tribe");if(tribe=="MECHANICAL")tribe="MECH";if(tribe=="NONE"||tribe.empty())tribe="MINION";
  layer(golden?"race-banner-premium.png":"race-banner.png",box(137,740,326,57));text(tribe,r.x+300*sx,r.y+756*sy,std::max(8.f,30*sy),Pale,true,true);
  statBadge({r.x+88*sx,r.y+750*sy},r.width/280,num(u,"attack"),false,golden,WHITE,statColor(u,false));statBadge({r.x+520*sx,r.y+750*sy},r.width/280,num(u,"health"),true,golden,WHITE,statColor(u,true));
  if(details){gem(r.x+71*sx,r.y+80*sy,23*r.width/252,{104,61,134,255},std::to_string(num(u,"tier",1)),23*r.width/252,6);}
 }
 void cardTooltip(const json& u,float x,float y){float w=252,h=354;x=std::clamp(x,12.f,1600-w-12);y=std::clamp(y,12.f,900-h-12);DrawRectangleRounded({x+14,y+22,w-25,h-14},.11f,8,{0,0,0,80});cardVisual(u,{x,y,w,h});}
 void shieldChanges(const json& before,const json& after){for(const char* zone:{"board","hand","shop"})for(int i=0;i<(int)array(after,zone).size();i++){const auto& u=after[zone][i];const json* old=nullptr;for(const char* z:{"board","hand","shop"})for(const auto& v:array(before,z))if(num(v,"uid")==num(u,"uid"))old=&v;bool entering=std::string(zone)=="board"&&std::none_of(array(before,"board").begin(),array(before,"board").end(),[&](const json& v){return num(v,"uid")==num(u,"uid");});if((old&&keyword(*old,"DIVINE_SHIELD")!=keyword(u,"DIVINE_SHIELD"))||(entering&&keyword(u,"DIVINE_SHIELD"))){Vector2 pos=std::string(zone)=="hand"?handPosition(i,after[zone].size()):position(i,after[zone].size(),std::string(zone)=="shop");shieldWaves.push_back({num(u,"uid"),pos,0,keyword(u,"DIVINE_SHIELD")});}}}
 void auraLinks(const json& unit){if(array(unit,"auraEffects").empty())return;json boards=json::array();if(scene=="tavern")boards.push_back({{"units",array(player(),"board")},{"upper",false}});else if(scene=="combat"&&!replay.is_null()){const auto& frame=replay["result"]["frames"][animationFrame];boards.push_back({{"units",array(frame,"left")},{"upper",false}});boards.push_back({{"units",array(frame,"right")},{"upper",true}});}else if(scene=="lab"){for(int i=0;i<2;i++)boards.push_back({{"units",lab["players"][i]["board"]},{"upper",i==1}});}
  for(const auto& row:boards){const auto& units=row["units"];for(int i=0;i<(int)units.size();i++)if(num(units[i],"uid")==num(unit,"uid")){Vector2 to=position(i,units.size(),flag(row,"upper"));for(const auto& e:array(unit,"auraEffects"))for(int j=0;j<(int)units.size();j++)if(num(units[j],"uid")==num(e,"sourceUid")&&i!=j){Vector2 from=position(j,units.size(),flag(row,"upper"));DrawEllipseLines(from.x,from.y,77,96,{255,241,177,255});for(int n=0;n<24;n++){float a=n/24.f,b=(n+1)/24.f;Vector2 p=Vector2Lerp(from,to,a),q=Vector2Lerp(from,to,b);p.y-=sinf(a*PI)*57;q.y-=sinf(b*PI)*57;DrawLineEx(p,q,2,Fade({255,241,177,255},.6f));}float k=fmodf(ui::time*.7f,1.f);Vector2 p=Vector2Lerp(from,to,k);p.y-=sinf(k*PI)*57;DrawCircleGradient(p,11,{255,244,188,180},BLANK);}}}
 }
 void buffTooltip(const json& u,float x,float y){std::vector<std::pair<std::string,std::string>> rows;auto delta=[](int n){return std::string(n>=0?"+":"")+std::to_string(n);};int accountedA=0,accountedH=0;
  for(const auto& e:array(u,"auraEffects"))rows.push_back({s(e,"source")+" - Aura",delta(num(e,"attack"))+" Attack / "+delta(num(e,"health"))+" Health"});
  for(const auto& e:array(u,"enchantments")){accountedA+=num(e,"attack");accountedH+=num(e,"health");rows.push_back({s(e,"source"),delta(num(e,"attack"))+" Attack / "+delta(num(e,"health"))+" Health"});}
  const auto* def=definition(s(u,"cardId"));if(def){int f=flag(u,"golden")?2:1;int da=num(u,"attack")-num(*def,"attack")*f-num(u,"auraAttack")-accountedA,dh=num(u,"maxHealth",num(u,"health"))-num(*def,"health")*f-num(u,"auraHealth")-accountedH;if(da||dh)rows.push_back({"Other stat changes",delta(da)+" Attack / "+delta(dh)+" Health"});}
  for(const auto& k:array(u,"keywords")){std::string key=k.get<std::string>();if(key=="DIVINE_SHIELD")rows.push_back({"Divine Shield","Absorbs the next damage taken"});else if(key=="REBORN")rows.push_back({"Reborn","Returns once with 1 Health"});else if(key=="TAUNT")rows.push_back({"Taunt","Enemies must attack Taunt first"});else if(key=="POISONOUS")rows.push_back({"Poisonous","Destroys minions damaged"});else if(key=="WINDFURY")rows.push_back({"Windfury","Attacks twice per turn"});}
  if(flag(u,"auraSource"))rows.push_back({s(u,"name")+" - Aura source","Continuously empowers eligible minions"});
  if(rows.empty())rows.push_back({"No active buffs","Base stats and card abilities"});
  int visible=std::min(6,(int)rows.size());buffScroll=std::clamp(buffScroll-(int)GetMouseWheelMove(),0,std::max(0,(int)rows.size()-visible));float listH=35+visible*38+((int)rows.size()>visible?16:0),totalH=354+listH;x=std::clamp(x,12.f,1288.f);y=std::clamp(y,12.f,888-totalH);cardTooltip(u,x+24,y);panel({x,y+354,300,listH},{43,34,28,250});text("ENCHANTMENTS",x+150,y+364,15,Gold,true,true);for(int i=0;i<visible;i++){const auto& row=rows[buffScroll+i];float yy=y+390+i*38;text(row.first,x+15,yy,14,{168,236,135,255},true);text(row.second,x+15,yy+17,13,Cream);}if((int)rows.size()>visible)text("Scroll: "+std::to_string(buffScroll+1)+"-"+std::to_string(buffScroll+visible)+" / "+std::to_string(rows.size()),x+150,y+listH+338,11,Gold,false,true);
 }
 void drawMinion(const json& u,Vector2 center,float scale=1,float alpha=1,bool selected=false,bool shop=false){if(!drawingFlight&&moving(u))return;float w=108*scale,h=141*scale;Color gold=flag(u,"golden")?Color{255,199,61,255}:Color{165,128,80,255};float pulse=.5f+.5f*sinf(ui::time*5);int cx=(int)center.x,cy=(int)center.y;
  DrawEllipse(cx+5,cy+15,w*.62f,h*.53f,Fade(BLACK,.35f*alpha));
  bool auraEmitter=flag(u,"auraSource")||s(u,"effect")=="Murloc Warleader"||s(u,"effect")=="Dire Wolf Alpha"||s(u,"effect")=="Mal'Ganis"||s(u,"effect")=="Siegebreaker"||s(u,"effect")=="Phalanx Commander"||s(u,"effect")=="Old Murk-Eye";
  if(!shop&&!drawingFlight&&(scene=="tavern"||scene=="combat"||scene=="lab")){if(auraEmitter)auraGlow(center,w*1.2f,h*1.1f,ui::time*.43f,.45f*alpha);else if(num(u,"auraAttack")||num(u,"auraHealth"))auraGlow(center,w,h,ui::time*.43f,.18f*alpha);}
  if(selected){DrawEllipse(cx,cy,w*.64f,h*.60f,Fade({52,207,104,255},(.22f+.15f*pulse)*alpha));DrawEllipseLines(cx,cy,w*.62f,h*.59f,Fade({72,242,108,255},alpha));}
  if(keyword(u,"TAUNT")){Vector2 pts[7]={{center.x-w*.48f,center.y-h*.57f},{center.x+w*.48f,center.y-h*.57f},{center.x+w*.63f,center.y-h*.40f},{center.x+w*.58f,center.y+h*.28f},{center.x,center.y+h*.66f},{center.x-w*.58f,center.y+h*.28f},{center.x-w*.63f,center.y-h*.40f}};for(int i=0;i<7;i++){DrawTriangle(center,pts[(i+1)%7],pts[i],Fade(i<3?Color{79,89,98,255}:Color{54,64,75,255},alpha));DrawLineEx(pts[i],pts[(i+1)%7],5*scale,Fade({169,175,173,255},alpha));}}
  DrawEllipse(cx,cy,w*.55f,h*.54f,Fade({39,29,22,255},alpha));portrait(s(u,"art"),{center.x-w*.47f,center.y-h*.48f,w*.94f,h*.96f},Fade(WHITE,alpha));minionRim(center,w*1.13f,h*1.14f,flag(u,"golden"),Fade(WHITE,alpha));

  if(keyword(u,"DIVINE_SHIELD")){DrawEllipse(cx,cy,w*.62f,h*.60f,Fade({255,209,45,255},.27f*alpha));for(int k=0;k<96;k++){float a=k*2*PI/96,b=(k+1)*2*PI/96;DrawLineEx({center.x+cosf(a)*w*.61f,center.y+sinf(a)*h*.59f},{center.x+cosf(b)*w*.61f,center.y+sinf(b)*h*.59f},(4.5f+pulse)*scale,Fade({255,225,98,255},alpha));}DrawEllipseLines(cx-3*scale,cy-4*scale,w*.56f,h*.54f,Fade(WHITE,.8f*alpha));DrawRectangleRounded({center.x-51*scale,center.y-h*.67f,102*scale,20*scale},.5f,8,Fade({110,70,8,255},alpha));text("DIVINE SHIELD",center.x,center.y-h*.67f+3*scale,12*scale,Fade({255,244,162,255},alpha),true,true);}
  if(keyword(u,"POISONOUS")){DrawCircle(cx,cy-(int)(h*.47f),12*scale,Fade({32,111,30,255},alpha));text("!",cx,cy-h*.47f-12*scale,22*scale,Fade({204,250,72,255},alpha),true,true);}
  if(keyword(u,"REBORN")){for(int k=0;k<48;k++){float a=k*2*PI/48+ui::time*.35f,b=a+PI/65;DrawLineEx({center.x+cosf(a)*w*.69f,center.y+sinf(a)*h*.66f},{center.x+cosf(b)*w*.69f,center.y+sinf(b)*h*.66f},4*scale,Fade({76,231,255,255},alpha));}DrawRectangleRounded({center.x-46*scale,center.y+h*.57f,92*scale,24*scale},.45f,8,Fade({12,66,83,255},alpha));text("REBORN",center.x,center.y+h*.57f+4*scale,15*scale,Fade({131,246,255,255},alpha),true,true);}
  statBadge({center.x-w*.43f,center.y+h*.43f},scale*.78f,num(u,"attack"),false,flag(u,"golden"),Fade(WHITE,alpha),statColor(u,false));
  statBadge({center.x+w*.43f,center.y+h*.43f},scale*.78f,num(u,"health"),true,flag(u,"golden"),Fade(WHITE,alpha),statColor(u,true));
  if(shop){DrawCircle(cx-(int)(w*.40f),cy-(int)(h*.42f),15*scale,Fade({92,53,124,255},alpha));text(std::to_string(num(u,"tier",1)),center.x-w*.4f,center.y-h*.42f-10*scale,18*scale,Fade(Pale,alpha),true,true);}
  if(keyword(u,"WINDFURY"))text(">>",center.x,center.y+h*.43f,18*scale,Fade(Cream,alpha),true,true);
 }
 Vector2 position(int index,int count,bool upper)const {float spacing=142,start=817-(count-1)*spacing/2;return {start+index*spacing,upper?286.f:525.f};}
 void nextRecruitStep(){if(recruitSteps.empty()){recruitView=nullptr;recruitKind.clear();return;}json before=recruitView,step=recruitSteps.front();recruitSteps.pop_front();recruitView=step["player"];recruitKind=s(step,"kind");recruitSource=num(step,"source",-1);recruitClock=0;int summons=0;for(const auto& u:array(recruitView,"board")){bool known=false;for(const auto& old:array(before,"board"))if(num(old,"uid")==num(u,"uid"))known=true;if(!known)summons++;}recruitDuration=recruitKind=="enter"?.44f:recruitKind=="battlecry"?.58f+summons*.1f:.16f;animateRecruit(before,recruitView,recruitKind);
  if(recruitKind=="battlecry"){for(int i=0;i<(int)array(recruitView,"board").size();i++){const auto& u=recruitView["board"][i];Vector2 pos=position(i,recruitView["board"].size(),false);if(num(u,"uid")==recruitSource){spark(pos,{183,141,246,255},20);audio.play(2);}for(const auto& old:array(before,"board"))if(num(old,"uid")==num(u,"uid")){int da=num(u,"attack")-num(old,"attack"),dh=num(u,"health")-num(old,"health");if(da>0||dh>0)floaters.push_back({pos,"+"+std::to_string(da)+" / +"+std::to_string(dh),0,{143,244,128,255}});}}}
 }
 void tickRecruit(float dt){if(!presenting()||!overlay.empty()||debug)return;recruitClock+=dt*speed;if(recruitClock>=recruitDuration)nextRecruitStep();}
 void recruitPulse(){if(recruitKind!="battlecry"||!presenting())return;for(int i=0;i<(int)array(player(),"board").size();i++)if(num(player()["board"][i],"uid")==recruitSource){Vector2 p=position(i,player()["board"].size(),false);float t=std::min(1.f,recruitClock/.5f);DrawEllipseLines(p.x,p.y,65+t*60,82+t*32,Fade({195,139,255,255},1-t));text("Battlecry",p.x,p.y-109,20,Fade({123,61,160,255},1-t),true,true);}}
 void animateRecruit(const json& before,const json& after,const std::string& kind=""){shieldChanges(before,after);int popIndex=0;
  for(const char* zone:{"board","hand"}){const auto& units=array(after,zone);for(int i=0;i<(int)units.size();i++){const auto& u=units[i];bool boardZone=std::string(zone)=="board";Vector2 to=boardZone?position(i,units.size(),false):handPosition(i,units.size());bool wasHere=false;Vector2 from{815,760};const auto& previous=array(before,zone);for(int j=0;j<(int)previous.size();j++)if(num(previous[j],"uid")==num(u,"uid")){wasHere=true;from=boardZone?position(j,previous.size(),false):handPosition(j,previous.size());}if(wasHere){if(boardZone&&(Vector2Distance(from,to)>1||num(pendingDrag,"uid",-1)==num(u,"uid"))){if(num(pendingDrag,"uid",-1)==num(u,"uid"))from=pendingDragPosition;flights.push_back({u,from,to,0,.2f});}continue;}
   for(const char* source:{"shop","hand"}){const auto& old=array(before,source);for(int j=0;j<(int)old.size();j++)if(num(old[j],"uid")==num(u,"uid"))from=std::string(source)=="shop"?position(j,old.size(),true):handPosition(j,old.size());}if(num(pendingDrag,"uid",-1)==num(u,"uid"))from=pendingDragPosition;if(boardZone&&kind=="battlecry")flights.push_back({u,to,to,-.12f*popIndex++,.36f,1});else flights.push_back({u,from,to,0,.30f});
  }}
 }
 void drawFlights(float dt){drawingFlight=true;for(auto& f:flights){if(overlay.empty()&&!debug)f.age+=dt*(presenting()?speed:1);if(f.age<0)continue;if(!f.started){f.started=true;if(f.style==1){spark(f.to,{187,132,250,255},19);audio.play(2);}}
  float t=std::min(1.f,f.age/f.life),ease=1-powf(1-t,3);Vector2 p=Vector2Lerp(f.from,f.to,ease);float scale=1,alpha=1;if(f.style==1){float k=t-1;scale=std::max(.08f,1+2.7f*k*k*k+1.7f*k*k);alpha=std::min(1.f,t*5);}else{p.y-=sinf(t*PI)*28;scale=.96f+.09f*sinf(t*PI);if(t>.88f&&!f.unit.is_null())scale=1-.07f*sinf((t-.88f)/.12f*PI);}drawMinion(f.unit,p,scale,alpha);
 }drawingFlight=false;flights.erase(std::remove_if(flights.begin(),flights.end(),[](const Flight& f){return f.age>=f.life;}),flights.end());}
 void minionRow(const json& units,const std::string& zone,bool upper){int count=(int)units.size();for(int i=0;i<count;i++){Vector2 pos=position(i,count,upper);bool sel=selectedZone==zone&&selectedIndex==i;Rectangle r{pos.x-60,pos.y-80,120,170};bool hot=hover(r);drawMinion(units[i],pos,hot?1.055f:1,1,sel||hot,zone=="shop");if(zone!="preview")hits.push_back({zone,i,r,units[i]});if(hot&&!moving(units[i])){wantsPointer=true;hoverUnit=units[i];}}}
 void heroPortrait(const json& hero,Vector2 center,float scale=1,int health=-999){float w=91*scale,h=113*scale;DrawEllipse((int)center.x+3,(int)center.y+6,w*.61f,h*.58f,{22,14,18,190});DrawEllipse((int)center.x,(int)center.y,w*.59f,h*.57f,{128,77,35,255});DrawEllipse((int)center.x,(int)center.y,w*.54f,h*.53f,Gold);portrait(s(hero,"art"),{center.x-w*.49f,center.y-h*.49f,w*.98f,h*.98f});DrawEllipseLines((int)center.x,(int)center.y,w*.55f,h*.53f,{246,205,116,255});if(health!=-999)gem(center.x+w*.46f,center.y+h*.4f,20*scale,{155,29,36,255},std::to_string(health),23*scale,5);}
 void drawLobby(const json& players){std::vector<json> sorted;for(const auto& p:players)sorted.push_back(p);std::stable_sort(sorted.begin(),sorted.end(),[](const json&a,const json&b){return num(a,"health")>num(b,"health");});for(int i=0;i<(int)sorted.size();i++){const auto& p=sorted[i];float y=170+i*66.f;float x=128;Color tint=num(p,"health")>0?WHITE:GRAY;DrawRectangleRounded({87,y-25,79,59},.16f,8,num(p,"id")==0?Color{93,94,43,200}:Color{38,24,22,140});heroPortrait(p["hero"],{x-1,y+3},.40f);gem(x+27,y+20,12,{150,38,37,255},num(p,"health")>0?std::to_string(num(p,"health")):"X",15,5);text(std::to_string(p.value("placement",i+1)),95,y-20,13,Pale,true);for(int t=0;t<num(p,"tier",1);t++)star(99+t*9,y+31,3.5f,Gold);
  if(hover({82,y-26,91,63})){panel({180,y-28,260,73});text(s(p["hero"],"name"),310,y-16,20,Pale,true,true);text(s(p,"name")+"  |  Tavern "+std::to_string(num(p,"tier")),310,y+11,15,Cream,false,true);}}
 }
 void menu(){DrawRectangle(0,0,W,H,{17,10,20,95});panel({475,142,650,632},{56,29,23,245});for(int i=0;i<3;i++)DrawCircleGradient({800,227},150-i*25,Fade({47,147,211,255},.035f),BLANK);star(800,211,45,{67,165,213,255});DrawCircleLines(800,212,53,Gold);text("BATTLEGROUNDS",800,282,45,Pale,true,true);text("THE LOCAL TAVERN",800,336,17,Gold,true,true);float y=391;
  if(game.is_object()){if(btn("Resume game",{610,y,380,56},true,true)){scene=s(game,"phase")=="recruit"?"tavern":"combat";if(scene=="combat"&&game.contains("lastCombat"))setReplay({{"result",game["lastCombat"]},{"players",game["players"]},{"round",game["round"]}},false);}y+=69;}
  if(btn("Play Battlegrounds",{610,y,380,56})){scene="heroes";heroChoice=0;}y+=70;
  if(btn("Card Workshop",{550,y,240,49},true,false,20)){scene="collection";collectionForLab=false;cardPage=0;}
  if(btn("Combat Lab",{810,y,240,49},true,false,20)){scene="lab";if(lab.is_null())send({{"type","lab.new"}});}y+=63;
  if(btn("Matchbook",{550,y,240,49},true,false,20)){scene="history";send({{"type","history"}});}
  if(btn("Settings",{810,y,240,49},true,false,20))overlay="settings";
  text("Origins 15.6  |  A seat by the fire",800,746,18,{201,171,125,255},false,true);
  if(btn("Quit",{1408,821,136,43},true,false,20))exitGame=true;
 }
 void heroSelection(){DrawRectangle(190,143,1215,555,{35,19,14,80});text("Choose your hero",805,66,35,Pale,true,true);text("A new warband. A new story.",810,187,23,Ink,true,true);int start=heroPage*4;for(int i=0;i<4&&start+i<(int)heroes.size();i++){const auto& h=heroes[start+i];float x=377+i*289;Rectangle r{x-115,253,230,326};bool selected=heroChoice==start+i;panel(r,selected?Color{92,68,35,255}:Color{60,34,24,255});if(selected)DrawRectangleRoundedLinesEx({r.x-3,r.y-3,r.width+6,r.height+6},.08f,8,3,Gold);heroPortrait(h,{x,338},1.03f,num(h,"health"));wrapped(s(h,"name"),x,420,204,22,Pale,true);wrapped(s(h,"text"),x,478,193,16,Cream,true);if(hover(r)&&clicked){heroChoice=start+i;audio.play(2);}}
  if(btn("<",{209,392,53,60},heroPage>0))heroPage--;if(btn(">",{1380,392,53,60},start+4<(int)heroes.size()))heroPage++;
  text("Match seed",413,635,17,Ink,true);input("seed",seed,{410,663,182,42},true);text("Opponents",663,635,17,Ink,true);if(btn(difficulty==0?"Casual":difficulty==1?"Standard":"Expert",{660,661,191,43},true,false,19))difficulty=(difficulty+1)%3;
  if(btn("Enter the tavern",{934,648,310,65},!heroes.empty(),true,25)){unsigned long long v=0;try{v=std::stoull(seed);}catch(...){}if(v<1||v>0xffffffff)tell("Enter a seed from 1 to 4294967295.");else send({{"type","new"},{"seed",v},{"hero",s(heroes[heroChoice],"key")},{"difficulty",difficulty==0?"casual":difficulty==1?"standard":"expert"}});}
  if(btn("Back",{51,814,133,47},true,false,20))scene="menu";
 }
 void drawHand(){const auto& hand=array(player(),"hand");int n=hand.size(),hotIndex=-1;
  for(int i=n-1;i>=0;i--){auto p=handPosition(i,n);if(hover({p.x-51,762,102,138})){hotIndex=i;break;}}
  if(hotIndex<0&&handHover>=0&&handHover<n){auto p=handPosition(handHover,n);if(hover({p.x-55,716,110,184}))hotIndex=handHover;}
  handHover=dragging?-1:hotIndex;
  auto draw=[&](int i){auto p=handPosition(i,n);int uid=num(hand[i],"uid");float& lift=handLift[uid];lift+=((i==handHover?1.f:0.f)-lift)*std::min(1.f,ui::delta*18);float top=762-46*lift;
   if(!moving(hand[i])){if(i==handHover){wantsPointer=true;DrawRectangleRounded({p.x-54,top-3,108,151},.1f,8,{81,167,93,180});hoverUnit=hand[i];}
    cardVisual(hand[i],{p.x-57,top-2,114,153},false);
   }hits.push_back({"hand",i,{p.x-51,top,102,145},hand[i]});
  };for(int i=0;i<n;i++)if(i!=handHover)draw(i);if(handHover>=0)draw(handHover);
  if(n==0)text("Your next recruit awaits",812,838,20,{201,161,102,255},true,true);
 }
 void dragGuide(){if(!dragging||dragged.is_null()||scene!="tavern")return;std::string hint;Color color={102,207,113,255};Rectangle area{285,427,1040,177};bool valid=false;
  if(dragZone=="shop"){valid=num(player(),"gold")>=3&&array(player(),"hand").size()<10;area={465,741,705,145};hint=valid?"Release below the shop to recruit  -  3 gold":num(player(),"gold")<3?"Not enough gold":"Your hand is full";}
  else if(dragZone=="board"){if(mouse.y<350){area={285,214,1040,153};valid=true;hint="Release to sell  +1 gold";color=Gold;}else{valid=true;hint="Release to set attack order";}}
  else{valid=array(player(),"board").size()<7;hint=valid?"Release to play  -  drop over a minion to target":"Your warband is full";if(keyword(dragged,"MAGNETIC")&&(IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT))){hint="Drop on a friendly Mech to magnetize";for(const auto& h:hits)if(h.zone=="board"&&(s(h.unit,"tribe")=="MECHANICAL"||s(h.unit,"tribe")=="ALL")){DrawEllipseLines(h.bounds.x+60,h.bounds.y+80,65,85,SKYBLUE);if(CheckCollisionPointRec(mouse,h.bounds))valid=true;}}}
  if(!valid)color={217,85,62,255};DrawRectangleRounded(area,.13f,12,Fade(color,.045f));DrawRectangleRoundedLinesEx(area,.13f,12,2,Fade(color,.65f));panel({461,367,709,49});text(hint,815,382,18,color,true,true);
 }
 void selectionControls(){if(dragging||!pendingDrag.is_null()||selectedZone.empty()||selectedUnit.is_null())return;panel({1039,673,348,122});text(s(selectedUnit,"name"),1213,686,20,Pale,true,true);
  if(selectedZone=="shop"){if(btn("Recruit  (3)",{1076,723,274,48},num(player(),"gold")>=3,false,21))action({{"type","buy"},{"index",selectedIndex}});}
  if(selectedZone=="hand"){if(btn("Play",{1057,723,145,48},array(player(),"board").size()<7,false,21))action({{"type","play"},{"index",selectedIndex}});if(keyword(selectedUnit,"MAGNETIC")){if(btn("Magnetize",{1215,723,153,48},true,false,18))tell("Drag this Magnetic minion onto a friendly Mech while holding Shift.");}else text("Drag onto a minion\nfor a target",1284,726,14,Cream,false,true);}
  if(selectedZone=="board"){if(btn("Sell +1",{1055,725,135,45},true,false,19))action({{"type","sell"},{"index",selectedIndex}});if(btn("<",{1203,725,69,45},selectedIndex>0,false,22))action({{"type","move"},{"from",selectedIndex},{"to",selectedIndex-1}});if(btn(">",{1287,725,69,45},selectedIndex+1<(int)array(player(),"board").size(),false,22))action({{"type","move"},{"from",selectedIndex},{"to",selectedIndex+1}});}
 }
 void tavern(){if(game.is_null()){scene="menu";return;}const auto& p=player();text("Bob's Tavern",810,52,31,Pale,true,true);text("Round "+std::to_string(num(game,"round")),809,96,16,Gold,true,true);drawLobby(array(game,"players"));
  // Shop and board occupy the two halves of the real game table.
  text("RECRUIT",815,174,19,{112,69,34,255},true,true);minionRow(array(p,"shop"),"shop",true);
  for(int i=0;i<7;i++){auto pos=position(i,7,false);DrawEllipse((int)pos.x,(int)pos.y,59,80,{94,58,30,12});DrawEllipseLines((int)pos.x,(int)pos.y,59,80,{128,87,40,40});}
  minionRow(array(p,"board"),"board",false);
  if(flag(p,"frozen")){for(const auto& h:hits)if(h.zone=="shop"){DrawEllipse((int)(h.bounds.x+60),(int)(h.bounds.y+80),64,88,{98,199,235,35});DrawEllipseLines((int)(h.bounds.x+60),(int)(h.bounds.y+80),64,88,{174,237,255,210});}text("FROZEN",812,392,19,{223,250,255,255},true,true);}
  if(btn("Refresh  1",{1179,135,179,48},num(p,"gold")>=1,false,20))action({{"type","refresh"}});
  if(btn(flag(p,"frozen")?"Unfreeze":"Freeze",{1179,195,179,46},true,true,20))action({{"type","freeze"}});
  if(btn(num(p,"tier")>=6?"Tavern 6":"Upgrade  "+std::to_string(num(p,"upgrade")),{262,135,186,48},num(p,"tier")<6&&num(p,"gold")>=num(p,"upgrade"),false,20))action({{"type","upgrade"}});
  for(int i=0;i<num(p,"tier");i++)star(279+i*25,211,10,{169,97,153,255});
  if(btn(busy?"Thinking":"End turn",{1390,386,151,67},array(p,"discovers").empty(),false,23))send({{"type","end"}});
  text("Buy 3  /  Sell 1",1454,469,15,Cream,false,true);
  heroPortrait(p["hero"],{808,715},.95f,num(p,"health"));
  text(s(p["hero"],"name"),808,782,17,Pale,true,true);
  if(powerOrb(p,{952,711},true)){json cmd={{"type","power"}};if(selectedZone=="board")cmd["target"]=num(selectedUnit,"uid");action(cmd);}
  gem(1397,789,34,{184,125,21,255},std::to_string(num(p,"gold")),36,10);text("GOLD",1456,784,18,Gold,true);
  if(num(p,"coins")>0&&btn("Coin +1",{1210,813,136,41},num(p,"gold")<10,false,18))action({{"type","coin"}});
  if(num(p,"bananas")>0&&btn("Banana",{1210,760,136,41},selectedZone=="board",false,18))action({{"type","banana"},{"target",num(selectedUnit,"uid")}});
  if(btn("Menu",{40,805,139,47},true,false,20))overlay="pause";
  if(btn("F2  Tools",{204,805,146,47},true,false,19))debug=!debug;
  drawHand();selectionControls();
  if(array(p,"board").empty())text("Drag a minion here to build your warband",810,520,23,{134,90,46,150},true,true);
  text("Drag to recruit, play or reposition. Drop a warband minion on Bob to sell.",809,618,17,{102,61,30,255},false,true);
  if(num(p,"health")<=0){text("You have been eliminated. Continue to spectate the lobby.",810,440,23,{124,30,24,255},true,true);}
 }
 void dragInput(){if(transitioning()||presenting()||busy||debug||!overlay.empty()||!editing.is_null()||!array(player(),"discovers").empty()||scene!="tavern")return;
  if(clicked){focus.clear();bool found=false;for(auto it=hits.rbegin();it!=hits.rend();++it)if(CheckCollisionPointRec(mouse,it->bounds)&&!moving(it->unit)){dragged=it->unit;dragZone=it->zone;dragIndex=it->index;dragStart=mouse;dragging=false;found=true;selectedUnit=it->unit;selectedZone=it->zone;selectedIndex=it->index;break;}if(!found&&mouse.y<635){selectedZone.clear();selectedIndex=-1;}}
  if(down&&!dragged.is_null()&&Vector2Distance(mouse,dragStart)>9)dragging=true;
  if(released&&!dragged.is_null()){if(dragging){if(dragZone=="shop"&&mouse.y>415)action({{"type","buy"},{"index",dragIndex}});
   else if(dragZone=="hand"&&mouse.y>400&&mouse.y<650){int pos=0;const auto& board=array(player(),"board");for(int i=0;i<(int)board.size();i++)if(mouse.x>position(i,board.size(),false).x)pos=i+1;json cmd={{"type","play"},{"index",dragIndex},{"position",pos}};for(const auto& h:hits)if(h.zone=="board"&&CheckCollisionPointRec(mouse,h.bounds)){cmd["target"]=num(h.unit,"uid");if((IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT))&&keyword(dragged,"MAGNETIC"))cmd["magnetic"]=true;}action(cmd);}
   else if(dragZone=="board"&&mouse.y<350)action({{"type","sell"},{"index",dragIndex}});
   else if(dragZone=="board"&&mouse.y>415&&mouse.y<630){int n=array(player(),"board").size();int target=0;float best=9999;for(int i=0;i<n;i++){float dist=std::abs(mouse.x-position(i,n,false).x);if(dist<best){best=dist;target=i;}}if(target!=dragIndex)action({{"type","move"},{"from",dragIndex},{"to",target}});}
  }if(pendingDrag.is_null())returnDragged();else{dragged=nullptr;dragging=false;}}
  if(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)){selectedZone.clear();returnDragged();}
 }
 void discoverOverlay(){const auto& options=array(player(),"discovers");if(scene!="tavern"||options.empty())return;DrawRectangle(0,0,W,H,{15,10,18,180});panel({311,136,982,625});text("A little something extra",802,175,34,Pale,true,true);text("Choose a minion",802,222,22,Gold,true,true);int n=options[0].size();for(int i=0;i<n;i++){auto d=definition(options[0][i].get<std::string>());if(!d)continue;float x=803+(i-(n-1)*.5f)*281;cardTooltip(*d,x-126,292);if(btn("Choose",{x-105,667,210,48},true,true,22))action({{"type","discover"},{"index",i}});}}
 void impact(const json& prev,const json& current){bool hadDamage=false;for(const char* key:{"left","right"}){bool upper=std::string(key)=="right";const auto& old=array(prev,key);const auto& now=array(current,key);for(int i=0;i<(int)old.size();i++){const auto& u=old[i];auto it=std::find_if(now.begin(),now.end(),[&](const json& v){return num(v,"uid")==num(u,"uid");});Vector2 pos=position(i,old.size(),upper);if(num(u,"uid")==num(current,"attacker",-1)){for(const char* other:{"left","right"}){const auto& targets=array(prev,other);for(int j=0;j<(int)targets.size();j++)if(num(targets[j],"uid")==num(current,"target",-1))pos=Vector2Lerp(pos,position(j,targets.size(),std::string(other)=="right"),.805f);}}if(it==now.end()){spark(pos,{72,45,55,170},19,2);continue;}int delta=num(*it,"health")-num(u,"health");if(delta<0){floaters.push_back({pos,std::to_string(delta),0,{255,222,134,255}});spark(pos,{245,166,47,255},16);hadDamage=true;}if(keyword(u,"DIVINE_SHIELD")&&!keyword(*it,"DIVINE_SHIELD")){shieldWaves.push_back({num(u,"uid"),pos,0,false});spark(pos,{246,234,130,255},38,1);floaters.push_back({{pos.x,pos.y-62},"Shield broken",0,{255,236,119,255}});audio.play(4);}}for(int i=0;i<(int)now.size();i++){const auto& u=now[i];bool fresh=std::none_of(old.begin(),old.end(),[&](const json& v){return num(v,"uid")==num(u,"uid");});if(fresh){Vector2 pos=position(i,now.size(),upper);if(keyword(u,"DIVINE_SHIELD"))shieldWaves.push_back({num(u,"uid"),pos,0,true});spark(pos,flag(u,"rebornUsed")?Color{91,237,255,255}:Color{160,102,245,255},32);audio.play(2);}else{auto prior=std::find_if(old.begin(),old.end(),[&](const json& v){return num(v,"uid")==num(u,"uid");});if(prior!=old.end()&&!keyword(*prior,"DIVINE_SHIELD")&&keyword(u,"DIVINE_SHIELD"))shieldWaves.push_back({num(u,"uid"),position(i,now.size(),upper),0,true});if(prior!=old.end()&&!keyword(*prior,"REBORN")&&keyword(u,"REBORN")){Vector2 pos=position(i,now.size(),upper);spark(pos,{91,237,255,255},30);floaters.push_back({{pos.x,pos.y-60},"Reborn granted",0,{122,242,255,255}});audio.play(2);}}}}if(hadDamage){audio.play(1);shake=5;}}
 std::string rebornName(const json& prev,const json& current)const{for(const char* key:{"left","right"})for(const auto& u:array(current,key))if(flag(u,"rebornUsed")){bool existed=false;for(const auto& v:array(prev,key))if(num(u,"uid")==num(v,"uid"))existed=true;if(!existed)return s(u,"name");}return "";}
 float combatDuration(const json& frames,int index)const{const auto& f=frames[index];if(f.contains("attacker"))return .86f;if(index>0&&!rebornName(frames[index-1],f).empty())return 1.35f;if(s(f,"text").find(":")!=std::string::npos)return 1.2f;return .46f;}
 void combatScene(float dt){if(replay.is_null()||!replay.contains("result")){scene="tavern";return;}const auto& result=replay["result"];const auto& frames=array(result,"frames");if(frames.empty()){scene="tavern";return;}int total=frames.size();animationFrame=std::clamp(animationFrame,0,total-1);float duration=combatDuration(frames,animationFrame)/std::max(.25f,speed);
  if(!paused&&!blocked&&animationFrame<total-1){animationTime+=dt;if(animationTime>=duration){animationTime-=duration;animationFrame++;frameImpact=false;duration=combatDuration(frames,animationFrame)/std::max(.25f,speed);}}float phase=animationTime/duration;
  const auto& current=frames[animationFrame];const auto& prev=frames[std::max(0,animationFrame-1)];bool attack=current.contains("attacker");if(!frameImpact&&phase>motion::impact&&animationFrame>0){impact(prev,current);frameImpact=true;}
  const auto& shown=attack&&phase<motion::impact?prev:current;int attacker=num(current,"attacker",-1),target=num(current,"target",-1);Vector2 from{},to{};bool foundA=false,foundT=false;
  for(const char* key:{"left","right"}){bool upper=std::string(key)=="right";const auto& units=array(prev,key);for(int i=0;i<(int)units.size();i++){int uid=num(units[i],"uid");if(uid==attacker){from=position(i,units.size(),upper);foundA=true;}if(uid==target){to=position(i,units.size(),upper);foundT=true;}}}
  text("COMBAT",489,86,26,Pale,true,true);text("Round "+std::to_string(num(replay,"round",num(game,"round"))),489,121,17,Gold,true,true);drawLobby(array(replay,"players"));
  bool done=animationFrame==total-1;int winner=result["winner"].is_null()?-999:num(result,"winner"),leftId=num(result,"leftId"),rightId=num(result,"rightId");
  json left=nullptr,right=nullptr;for(const auto& p:array(replay,"players")){if(num(p,"id")==leftId)left=p;if(num(p,"id")==rightId)right=p;}
  if(right.is_object()){text(s(right["hero"],"name"),808,51,23,Pale,true,true);text(s(right,"name")+" | Tavern "+std::to_string(num(right,"tier")),489,152,16,Gold,false,true);powerOrb(right,{952,135});text(s(right["hero"],"power"),1052,93,21,Pale,true);std::string ability=s(right["hero"],"text");if(ability.rfind("Hero Power ",0)==0)ability=ability.substr(11);wrapped(ability,1052,122,275,16,Cream);}else text("The Ghost",808,53,23,Pale,true,true);
  const auto& survivors=array(current,winner==leftId?"left":"right");float gatherEnd=.45f+survivors.size()*.24f+.75f,impactAt=gatherEnd+.46f,finishEnd=gatherEnd+1.15f;
  if(!done){finishClock=0;finishHit=false;resultCelebrated=false;}else if(!paused&&!blocked)finishClock+=dt*speed;
  bool finishComplete=done&&(winner==-999?finishClock>.55f:finishClock>=finishEnd);
  Vector2 ownHero={808,715},enemyHero={808,136};bool ownWins=winner==leftId;Vector2 winnerHome=ownWins?ownHero:enemyHero,loserHome=ownWins?enemyHero:ownHero;
  Vector2 attackingHero=winnerHome;if(done&&winner!=-999&&finishClock>gatherEnd){float t=std::clamp((finishClock-gatherEnd)/1.05f,0.f,1.f);attackingHero=Vector2Lerp(winnerHome,loserHome,motion::attackTravel(t)*1.09f);attackingHero.x+=sinf(t*PI)*24;}
  if(done&&winner!=-999&&finishClock>=impactAt&&!finishHit){finishHit=true;resultCelebrated=true;floaters.push_back({loserHome,"-"+std::to_string(num(result,"damage")),0,{255,215,115,255}});spark(loserHome,{255,216,106,255},36);shake=10;audio.play(1);}
  json attackVisual=nullptr;Vector2 attackPosition{};float attackScale=1;
  for(const char* key:{"right","left"}){bool upper=std::string(key)=="right";const auto& units=array(shown,key);const auto& old=array(prev,key);
   for(int i=0;i<(int)units.size();i++){const auto& u=units[i];Vector2 pos=position(i,units.size(),upper);float scale=1,alpha=1;
    auto prior=std::find_if(old.begin(),old.end(),[&](const json& v){return num(v,"uid")==num(u,"uid");});
    if(!attack&&animationFrame>0){if(prior==old.end()){float k=std::clamp((phase-.40f)/.45f,0.f,1.f);if(k<=0)continue;scale=.15f+.85f*(1-powf(1-k,3))+.14f*sinf(k*PI);alpha=std::min(1.f,k*4);}else{int j=std::distance(old.begin(),prior);pos=Vector2Lerp(position(j,old.size(),upper),pos,motion::smooth(phase/.7f));}}
    if(num(u,"uid")==attacker&&foundA&&foundT&&phase<.90f){float travel=motion::attackTravel(phase);pos=Vector2Add(from,Vector2Scale(Vector2Subtract(to,from),travel));scale=1+.045f*std::max(0.f,travel);}
    if(num(u,"uid")==target&&foundA&&foundT)pos=Vector2Add(pos,Vector2Scale(Vector2Normalize(Vector2Subtract(to,from)),motion::recoil(phase)*36));
    if(num(u,"uid")==attacker&&attack){attackVisual=u;attackPosition=pos;attackScale=scale;}else drawMinion(u,pos,scale,alpha);
    if(hover({pos.x-60,pos.y-80,120,160}))hoverUnit=u;
   }
   if(!attack&&animationFrame>0&&phase<.48f)for(int i=0;i<(int)old.size();i++){bool gone=std::none_of(units.begin(),units.end(),[&](const json& u){return num(u,"uid")==num(old[i],"uid");});if(gone){auto pos=position(i,old.size(),upper);float k=phase/.48f;pos.y+=18*k;drawMinion(old[i],pos,1-.30f*k,1-k);}}
  }
  if(!attackVisual.is_null()){
   if(phase>.22f&&phase<motion::impact&&foundT){Vector2 dir=Vector2Normalize(Vector2Subtract(to,from));for(int i=0;i<3;i++){Vector2 off={dir.y*(i-1)*14,-dir.x*(i-1)*14};Vector2 head=Vector2Add(attackPosition,off);DrawLineEx(Vector2Subtract(head,Vector2Scale(dir,65-i*12)),head,3-i*.6f,Fade(Cream,.45f));}}
   drawMinion(attackVisual,attackPosition,attackScale);
  }
  if(attack&&foundT&&phase>=motion::impact&&phase<motion::impact+.14f){float k=(phase-motion::impact)/.14f;Vector2 contact=Vector2Lerp(from,to,.88f);for(int i=0;i<10;i++){float angle=i*PI/5+.15f;Vector2 dir={cosf(angle),sinf(angle)};DrawLineEx(Vector2Add(contact,Vector2Scale(dir,14+k*20)),Vector2Add(contact,Vector2Scale(dir,38+k*55)),3*(1-k),Fade({255,235,162,255},1-k));}}
  std::string resurrected=rebornName(prev,current);if(!resurrected.empty()){panel({557,397,508,46},{16,62,76,245});text("REBORN",811,401,18,{131,246,255,255},true,true);text(resurrected+" returns with 1 Health",811,422,16,Pale,false,true);for(const char* key:{"left","right"}){const auto& units=array(current,key);for(int i=0;i<(int)units.size();i++)if(flag(units[i],"rebornUsed")&&s(units[i],"name")==resurrected){Vector2 p=position(i,units.size(),std::string(key)=="right");float k=std::clamp(phase,0.f,1.f);DrawEllipseLines(p.x,p.y,75+35*k,91+28*k,Fade({102,240,255,255},1-k));}}}
  else if(!attack&&s(current,"text").find(":")!=std::string::npos){panel({493,397,636,46},{26,57,76,245});text("HERO POWER",811,401,13,{137,226,255,255},true,true);text(s(current,"text"),811,420,18,Pale,true,true);}
  panel({398,607,811,40},{69,38,24,230});std::string event=resurrected.empty()?s(current,"text"):resurrected+" is Reborn with 1 Health";if(done){if(winner==-999)event="A TIE  -  No damage taken";else event=(winner==leftId?"VICTORY":"DEFEAT")+std::string("   |   ")+std::to_string(num(result,"damage"))+" damage";}text(event,804,618,event.size()>70?15:19,done?(winner==leftId?Gold:winner==-999?Cream:Color{245,155,122,255}):Cream,true,true);
  auto drawCombatHero=[&](const json& p,bool own){if(!p.is_object())return;int health=num(p,"health");bool loser=winner!=-999&&num(p,"id")!=winner;if(loser&&!finishHit&&!labReplay)health+=num(result,"damage");Vector2 pos=own?ownHero:enemyHero;if(done&&winner!=-999&&num(p,"id")==winner)pos=attackingHero;heroPortrait(p["hero"],pos,.95f,health);};
  if(done&&ownWins){drawCombatHero(right,false);drawCombatHero(left,true);}else{drawCombatHero(left,true);drawCombatHero(right,false);}if(left.is_object())powerOrb(left,{952,711});
  if(done&&winner!=-999){int tierStars=0,gathered=0;for(const auto& u:survivors)tierStars+=std::max(1,num(u,"tier",1));int base=std::max(0,num(result,"damage")-tierStars);for(int i=0;i<(int)survivors.size();i++){Vector2 from=position(i,survivors.size(),!ownWins);int count=std::max(1,num(survivors[i],"tier",1));for(int j=0;j<count;j++){float start=.25f+i*.24f+j*.035f,t=(finishClock-start)/.65f;if(t>=1){gathered++;continue;}if(t<0)continue;Vector2 src={from.x+(j-(count-1)*.5f)*15,from.y-42};Vector2 p=Vector2Lerp(src,winnerHome,motion::smooth(t));p.x+=sinf(t*PI)*32;p.y-=sinf(t*PI)*35;DrawCircleGradient(p,23,Fade(Gold,.5f),BLANK);star(p.x,p.y,12+5*sinf(t*PI),Pale);}}if(finishClock<finishEnd){gem(attackingHero.x-48,attackingHero.y+43,23,{143,88,13,255},std::to_string(std::min(num(result,"damage"),base+gathered)),26,8);}}
  if(btn(paused?"Play":"Pause",{354,788,141,45},true,false,19))paused=!paused;
  if(btn("<",{509,788,59,45},animationFrame>0,false,22)){animationFrame--;animationTime=duration*.8f;paused=true;}
  if(btn(">",{580,788,59,45},animationFrame<total-1,false,22)){animationFrame++;animationTime=duration*.8f;paused=true;frameImpact=false;}
  if(btn("Skip",{653,788,100,45},true,false,19)){animationFrame=total-1;animationTime=duration;finishClock=100;paused=false;}
  if(btn(speed<1?"0.5x":speed<2?"1x":"2x",{1190,788,96,45},true,false,19))speed=speed<1?1:speed<2?2:.5f;
  if(btn(finishComplete?(replayOnly?"Return":s(game,"phase")=="finished"?"Results":"Recruit"):"Fighting",{1388,386,151,67},finishComplete,false,23)){if(replayOnly){scene=labReplay?"lab":"history";replay=nullptr;}else if(s(game,"phase")=="finished")overlay="results";else send({{"type","advance"}});}
  text(std::to_string(animationFrame+1)+" / "+std::to_string(total),1090,802,16,Cream,false,true);
  if(replayOnly&&!labReplay&&historyChoice<(int)history.size()){
   if(btn("Previous round",{345,841,218,38},historyRound>0,false,17)){historyRound--;send({{"type","replay"},{"matchId",s(history[historyChoice],"id")},{"round",historyRound}});}
   if(btn("Next round",{1080,841,218,38},historyRound+1<num(history[historyChoice],"rounds"),false,17)){historyRound++;send({{"type","replay"},{"matchId",s(history[historyChoice],"id")},{"round",historyRound}});}
  }
  if(btn("Menu",{40,805,139,47},true,false,20))overlay="pause";
 }
 void collection(){text(collectionForLab?"Choose a minion":"Card Workshop",807,51,31,Pale,true,true);input("search",search,{238,145,395,42});if(btn(tierFilter==0?"All tiers":"Tier "+std::to_string(tierFilter),{659,144,161,43},true,false,19)){tierFilter=(tierFilter+1)%7;cardPage=0;}
  text("Changes apply to new games",1172,161,17,Ink,true,true);std::vector<json> list;std::string query=search;std::transform(query.begin(),query.end(),query.begin(),::tolower);for(const auto& c:cards){auto name=s(c,"name");std::transform(name.begin(),name.end(),name.begin(),::tolower);if(name.find(query)!=std::string::npos&&(!tierFilter||num(c,"tier")==tierFilter))list.push_back(c);}int pages=std::max(1,((int)list.size()+9)/10);cardPage=std::clamp(cardPage,0,pages-1);
  for(int i=0;i<10&&cardPage*10+i<(int)list.size();i++){const auto& c=list[cardPage*10+i];float x=337+(i%5)*232,y=275+(i/5)*231;Rectangle r{x-99,y-70,198,218};panel(r,{104,67,34,255},true);drawMinion(c,{x,y+12},.77f,flag(c,"enabled")?1:.35f,false,true);wrapped(s(c,"name"),x,y+90,177,17,Ink,true);text("Click to "+std::string(collectionForLab?"add":"edit"),x,y+126,12,{108,74,40,255},false,true);if(hover(r)){hoverUnit=c;if(clicked&&!busy){if(collectionForLab){send({{"type","lab.add"},{"side",labSide},{"cardId",s(c,"id")},{"golden",labGolden}});scene="lab";}else{editing=c;editorName=s(c,"name");editorText=s(c,"text");editorEffect=s(c,"effect");focus.clear();}}}}
  if(!blocked&&GetMouseWheelMove()!=0)cardPage=std::clamp(cardPage-(int)GetMouseWheelMove(),0,pages-1);if(list.empty())text("No matching minions",810,414,30,Ink,true,true);if(btn("<",{653,758,77,47},cardPage>0))cardPage--;text(std::to_string(cardPage+1)+" / "+std::to_string(pages),811,773,21,Pale,true,true);if(btn(">",{888,758,77,47},cardPage+1<pages))cardPage++;
  if(btn("Back",{43,815,133,45},true,false,20)){scene=collectionForLab?"lab":"menu";collectionForLab=false;}
  if(btn("Export pack",{1241,813,180,44},true,false,19))send({{"type","pack.export"}});
 }
 void editor(){if(editing.is_null())return;DrawRectangle(0,0,W,H,{20,12,16,195});panel({277,90,1050,736});text("Card Workshop",803,123,34,Pale,true,true);cardTooltip(editing,316,233);text(s(editing,"id"),442,608,14,Gold,false,true);
  text("Name",610,184,16,Gold,true);input("cardname",editorName,{610,211,667,40});editing["name"]=editorName;
  const char* keys[]={"attack","health","tier","pool","buffAttack","buffHealth"};const char* labels[]={"Attack","Health","Tavern tier","Pool copies","Buff attack x","Buff health x"};for(int i=0;i<6;i++){float x=610+(i%3)*228,y=281+(i/3)*87;text(labels[i],x,y,17,Cream,true);int v=num(editing,keys[i]);if(btn("-",{x,y+29,49,36},v>(i==0||i>=4?0:1),false,21))editing[keys[i]]=v-1;text(std::to_string(num(editing,keys[i])),x+99,y+35,24,Pale,true,true);if(btn("+",{x+145,y+29,49,36},v<(i==2?6:i>=4?100:999),false,21))editing[keys[i]]=v+1;}
  const char* kws[]={"TAUNT","DIVINE_SHIELD","POISONOUS","WINDFURY","REBORN","MAGNETIC"};const char* names[]={"Taunt","Divine Shield","Poisonous","Windfury","Reborn","Magnetic"};for(int i=0;i<6;i++){bool on=keyword(editing,kws[i]);float x=610+(i%3)*227,y=474+(i/3)*46;if(btn(std::string(on?"[x] ":"[ ] ")+names[i],{x,y,200,37},true,on,16)){auto& ks=editing["keywords"];if(on)ks.erase(std::remove(ks.begin(),ks.end(),kws[i]),ks.end());else ks.push_back(kws[i]);}}
  text("Effect handler",610,568,16,Gold,true);input("effect",editorEffect,{764,557,513,40});editing["effect"]=editorEffect;
  text("Display text",610,617,16,Gold,true);input("cardtext",editorText,{610,645,667,41});editing["text"]=editorText;
  text("Description text does not create new mechanics. Existing handlers are reusable.",610,696,14,{173,145,106,255});
  if(btn(flag(editing,"enabled")?"Enabled":"Disabled",{316,677,252,43},true,flag(editing,"enabled"),19))editing["enabled"]=!flag(editing,"enabled");
  if(btn("Cancel",{611,741,143,47},true,false,20)){editing=nullptr;focus.clear();}
  if(btn("Reset",{770,741,143,47},true,false,20))send({{"type","card.reset"},{"cardId",s(editing,"id")}});
  if(btn("Duplicate",{927,741,156,47},true,false,20)){json c=editing;c["id"]="custom_"+std::to_string((unsigned long long)(GetTime()*1000000));c["name"]=editorName+" (custom)";send({{"type","card.save"},{"card",c}});}
  if(btn("Save",{1101,741,173,47},true,true,23))send({{"type","card.save"},{"card",editing}});
 }
 void labScene(){if(lab.is_null())return;text("Combat Lab",810,54,32,Pale,true,true);text("WARBAND B",810,165,19,Ink,true,true);text("WARBAND A",810,627,19,Ink,true,true);minionRow(lab["players"][1]["board"],"lab1",true);minionRow(lab["players"][0]["board"],"lab0",false);
  if(btn("Add to A",{308,702,173,47},true,labSide==0,20)){labSide=0;collectionForLab=true;cardPage=0;scene="collection";}
  if(btn("Add to B",{495,702,173,47},true,labSide==1,20)){labSide=1;collectionForLab=true;cardPage=0;scene="collection";}
  if(btn(labGolden?"Golden: ON":"Golden: OFF",{308,766,360,42},true,labGolden,18))labGolden=!labGolden;
  if(btn("Run",{1388,386,151,67},true,false,24))send({{"type","lab.run"},{"seed",std::atoi(seed.c_str())}});
  if(btn("100 combats",{1142,697,218,47},true,true,20))send({{"type","lab.odds"},{"samples",100},{"seed",std::atoi(seed.c_str())}});
  if(btn("Save scenario",{1142,759,218,43},true,false,19))send({{"type","lab.export"}});
  if(btn("Clear boards",{1142,814,218,42},true,false,18))send({{"type","lab.new"},{"seed",std::atoi(seed.c_str())}});
  if(btn("Back",{43,815,133,45},true,false,20))scene="menu";
  text("Seed",734,745,16,Gold,true);input("labseed",seed,{790,733,211,41},true);
  if(!odds.is_null()){std::ostringstream line;line<<"A wins "<<num(odds,"wins")<<"%    Tie "<<num(odds,"ties")<<"%    B wins "<<num(odds,"losses")<<"%";panel({477,391,665,66});text(line.str(),810,412,22,Pale,true,true);}
  for(const auto& h:hits)if((h.zone=="lab0"||h.zone=="lab1")&&hover(h.bounds)&&clicked){labSide=h.zone=="lab1"?1:0;labSelected=h.index;}
  if(labSelected>=0&&labSelected<(int)array(lab["players"][labSide],"board").size()){const auto& u=lab["players"][labSide]["board"][labSelected];panel({1050,62,506,73});text(s(u,"name"),1290,71,17,Cream,true,true);if(btn("Atk -",{1064,98,86,28},num(u,"attack")-num(u,"auraAttack")>0,false,14))send({{"type","lab.stat"},{"side",labSide},{"index",labSelected},{"stat","attack"},{"value",num(u,"attack")-num(u,"auraAttack")-1}});if(btn("Atk +",{1160,98,86,28},true,false,14))send({{"type","lab.stat"},{"side",labSide},{"index",labSelected},{"stat","attack"},{"value",num(u,"attack")-num(u,"auraAttack")+1}});if(btn("HP +",{1256,98,86,28},true,false,14))send({{"type","lab.stat"},{"side",labSide},{"index",labSelected},{"stat","health"},{"value",num(u,"health")-num(u,"auraHealth")+1}});if(btn("Remove",{1352,98,181,28},true,false,14)){send({{"type","lab.remove"},{"side",labSide},{"index",labSelected}});labSelected=-1;}}
 }
 void historyScene(){text("The Matchbook",810,54,31,Pale,true,true);panel({309,166,1011,566});if(history.empty())text("Your first story is waiting to be written.",810,379,28,Cream,true,true);
  for(int i=0;i<std::min(6,(int)history.size());i++){const auto& h=history[i];float y=208+i*74;heroPortrait(h["hero"],{367,y+19},.37f);text("#"+std::to_string(num(h,"placement"))+"   "+s(h["hero"],"name"),417,y,23,Pale,true);text(std::to_string(num(h,"rounds"))+" rounds   Seed "+std::to_string(num(h,"seed")),418,y+32,15,Gold);if(btn("Replay",{1100,y,168,48},true,false,21)){historyChoice=i;historyRound=0;send({{"type","replay"},{"matchId",s(h,"id")},{"round",0}});}}
  if(btn("Back",{43,815,133,45},true,false,20))scene="menu";
 }
 void debugPanel(){if(!debug)return;panel({953,48,612,795},{32,25,29,247});text("DEVELOPER CONSOLE",1255,74,25,Gold,true,true);if(btn("X",{1493,65,47,35},true,false,20)){debug=false;return;}
  if(btn("Player "+std::to_string(debugPlayer+1),{978,123,173,38},game.is_object(),false,17))debugPlayer=(debugPlayer+1)%8;
  if(btn("Rewind",{1175,123,166,38},game.is_object(),false,17))send({{"type","rewind"}});
  if(btn("AI turn",{1366,123,166,38},game.is_object()&&s(game,"phase")!="finished",false,17))send({{"type","autoplay"}});
  if(game.is_object()){const auto& p=game["players"][debugPlayer];text("Round "+std::to_string(num(game,"round"))+"  RNG "+std::to_string(num(game,"rng")),978,183,16,Cream);text(s(p["hero"],"name")+"   Gold "+std::to_string(num(p,"gold"))+"   Health "+std::to_string(num(p,"health")),978,211,17,Pale);text("Pack "+s(game,"contentHash")+"    "+(flag(game,"modified")?"MODIFIED":"BASE RULES"),978,240,15,Gold);
   if(btn("10 gold",{978,277,171,39},true,false,18))send({{"type","debug"},{"player",debugPlayer},{"command","gold 10"}});
   if(btn("40 health",{1174,277,169,39},true,false,18))send({{"type","debug"},{"player",debugPlayer},{"command","health 40"}});
   if(btn("Tier 6",{1367,277,165,39},true,false,18))send({{"type","debug"},{"player",debugPlayer},{"command","tier 6"}});
   wrapped(s(p,"aiReason"),978,333,550,15,{164,183,162,255});
   const auto& log=array(game,"log");int y=411;for(int i=std::max(0,(int)log.size()-10);i<(int)log.size();i++){std::string line="R"+std::to_string(num(log[i],"round"))+"  "+s(log[i],"text");text(line.substr(0,75),979,(float)y,14,{191,169,132,255});y+=25;}
  }
  text("gold N | health N | tier N | spawn/give CARD_ID | clear | seed N",978,696,14,Gold);
  bool submit=focus=="console"&&IsKeyPressed(KEY_ENTER);input("console",consoleLine,{978,725,451,42});if(btn("Run",{1444,725,88,42},game.is_object(),true,18)||submit){if(consoleLine=="help")tell("Commands: gold 10, health 40, tier 6, spawn CFM_315, give CFM_315, clear, seed 42");else send({{"type","debug"},{"player",debugPlayer},{"command",consoleLine}});consoleLine.clear();focus="console";}
  text("Drop a JSON save, pack, or lab scenario into the game window to import.",978,797,13,{151,126,94,255});
 }
 void modal(){if(overlay.empty())return;DrawRectangle(0,0,W,H,{12,8,14,185});panel({486,145,625,610});text(overlay=="settings"?"Settings":overlay=="results"?"Well played!":"The Tavern",799,180,35,Pale,true,true);
  if(overlay=="results"){text("You finished #"+std::to_string(num(player(),"placement",8)),800,278,38,Gold,true,true);heroPortrait(player()["hero"],{800,395},1.2f);if(btn("Play again",{596,529,407,57},true,true,26)){overlay.clear();scene="heroes";}if(btn("Main menu",{596,603,407,52},true,false,23)){overlay.clear();scene="menu";}return;}
  if(overlay=="pause"){if(btn("Return to game",{596,265,407,55},true,true)){overlay.clear();}if(btn("Card Workshop",{596,336,407,51})){overlay.clear();scene="collection";collectionForLab=false;}if(btn("Export save",{596,401,407,51},game.is_object()))send({{"type","save.export"}});if(btn("Settings",{596,467,407,51}))overlay="settings";if(btn("How to play",{596,533,407,51}))overlay="help";if(btn("Main menu",{596,600,407,51})){scene="menu";overlay.clear();}return;}
  if(overlay=="help"){wrapped("Recruit a minion for 3 gold, then drag it from your hand onto the lower row. Sell a minion for 1 gold by dragging it to the shop. Three matching minions combine into a golden minion. Playing it discovers a minion from the next tavern tier.\n\nDrag your warband to set attack order. Hold Shift while dropping a Magnetic card onto a Mech to merge it. For targeted powers, click a warband minion first.\n\nR refreshes. F freezes. Space ends your turn. F2 opens the debug console. F11 toggles fullscreen. Escape opens the menu.",533,257,529,22,Cream);if(btn("Got it",{623,667,354,51},true,true))overlay.clear();return;}
  text("Sound volume",538,272,23,Cream,true);if(btn("-",{841,267,57,43},audio.volume>0))audio.volume=std::max(0.f,audio.volume-.1f);text(std::to_string((int)std::round(audio.volume*100))+"%",938,279,22,Pale,true,true);if(btn("+",{980,267,57,43},audio.volume<1))audio.volume=std::min(1.f,audio.volume+.1f);
  text("Animation speed",538,340,23,Cream,true);if(btn(speed<1?"Relaxed":speed>1?"Fast":"Normal",{841,333,196,43},true,false,19))speed=speed<1?1:speed<2?2:.5f;
  text("Display",538,408,23,Cream,true);if(btn(IsWindowFullscreen()?"Windowed":"Fullscreen",{841,400,196,43},true,false,19))ToggleFullscreen();
  wrapped("Data stays on this computer. Autosaves include the original card pack. Drag JSON files into the window to restore or import.",538,480,500,19,Cream);
  wrapped(dataDirectory,538,571,500,14,Gold);
  if(btn("Save settings",{622,665,355,51},true,true,22)){send({{"type","settings"},{"settings",{{"speed",speed},{"volume",audio.volume},{"fullscreen",IsWindowFullscreen()}}}});overlay.clear();}
 }
 void droppedFiles(){if(!IsFileDropped())return;auto files=LoadDroppedFiles();if(files.count){std::string filename=files.paths[0];std::string type=scene=="collection"?"pack.import":scene=="lab"?"lab.import":"save.import";send({{"type",type},{"path",filename}});}UnloadDroppedFiles(files);}
 void keyboard(){
  if(dragging&&!IsWindowFocused())returnDragged();
  if(IsKeyPressed(KEY_F2)){returnDragged();debug=!debug;focus.clear();}
  if(IsKeyPressed(KEY_F11))ToggleFullscreen();
  if(IsKeyPressed(KEY_ESCAPE)){focus.clear();if(dragging){returnDragged();selectedZone.clear();return;}if(debug)debug=false;else if(!editing.is_null())editing=nullptr;else if(!overlay.empty())overlay.clear();else if(scene=="tavern"||scene=="combat")overlay="pause";else scene="menu";}
  if(transitioning()||presenting()||dragging||!focus.empty()||debug||!overlay.empty()||!editing.is_null())return;
  if(scene=="tavern"&&!busy&&array(player(),"discovers").empty()){if(IsKeyPressed(KEY_R))action({{"type","refresh"}});if(IsKeyPressed(KEY_F))action({{"type","freeze"}});if(IsKeyPressed(KEY_SPACE))send({{"type","end"}});}
  if(scene=="combat"&&IsKeyPressed(KEY_SPACE))paused=!paused;
 }
 void tickEffects(float dt){for(auto& p:particles){p.age+=dt;p.p=Vector2Add(p.p,Vector2Scale(p.v,dt));p.v.y+=p.kind==2?-30:120;}particles.erase(std::remove_if(particles.begin(),particles.end(),[](const Particle&p){return p.age>p.life;}),particles.end());for(auto& f:floaters){f.age+=dt;f.p.y-=40*dt;}floaters.erase(std::remove_if(floaters.begin(),floaters.end(),[](const Floater&f){return f.age>1.1f;}),floaters.end());noticeTime-=dt;shake=std::max(0.f,shake-dt*20);}
 void drawEffects(){for(const auto& wave:shieldWaves){float t=std::clamp(wave.age/.48f,0.f,1.f);Vector2 pos=wave.position;for(const auto& f:flights)if(num(f.unit,"uid")==wave.uid)pos=Vector2Lerp(f.from,f.to,1-powf(1-std::min(1.f,f.age/f.life),3));float radius=wave.gain?1.45f-.45f*motion::smooth(t):1+.65f*t;Color gold={255,230,117,255};if(wave.gain)DrawEllipse(pos.x,pos.y,66*radius,84*radius,Fade(gold,sinf(t*PI)*.3f));for(int j=0;j<48;j++){float a=j*PI/24,b=a+(wave.gain?PI/24:PI/42);Vector2 from={pos.x+cosf(a)*66*radius,pos.y+sinf(a)*84*radius},to={pos.x+cosf(b)*66*radius,pos.y+sinf(b)*84*radius};DrawLineEx(from,to,(wave.gain?5:7)*(1-t)+1,Fade(gold,1-t));}}
for(const auto& p:particles){float a=1-p.age/p.life;if(p.kind==1)DrawPoly(p.p,3,p.size*1.6f,p.age*200,Fade(p.color,a));else if(p.kind==2)DrawCircleV(p.p,p.size*(1+p.age*4),Fade(p.color,a*.5f));else{DrawCircleGradient(p.p,p.size*3,Fade(p.color,a*.4f),BLANK);DrawCircleV(p.p,p.size*a,Fade(p.color,a));}}for(const auto& f:floaters){float alpha=std::min(1.f,(1.1f-f.age)*2);bool damage=!f.text.empty()&&f.text[0]=='-';if(damage&&f.age<.65f){float a=alpha*std::min(1.f,(.65f-f.age)*5);star(f.p.x+2,f.p.y+3,34,Fade({108,40,14,255},a));star(f.p.x,f.p.y,31,Fade({238,176,43,255},a));}text(f.text,f.p.x,f.p.y-17,damage?33:27,Fade(f.color,alpha),true,true);}}
 void take(const std::string& name){auto image=LoadImageFromTexture(canvas.texture);ImageFlipVertical(&image);ExportImage(image,(directory/"screenshots"/(name+".png")).string().c_str());UnloadImage(image);}
 void smokeDrag(const std::string& zone,Vector2 destination,bool accepted=true){auto it=std::find_if(hits.begin(),hits.end(),[&](const Hit& h){return h.zone==zone&&h.index==0;});if(it==hits.end()){smokeError="Missing drag hit area: "+zone;return;}mouse={it->bounds.x+it->bounds.width/2,it->bounds.y+it->bounds.height/2};clicked=true;released=false;down=true;dragInput();mouse=destination;clicked=false;dragging=true;
  if(!moving(dragged))smokeError="Drag source was not hidden";
  BeginTextureMode(canvas);drawBackground();hits.clear();tavern();dragGuide();drawingFlight=true;drawMinion(dragged,mouse,1.08f,1,true);drawingFlight=false;EndTextureMode();take(zone=="shop"?"08-drag-shop":zone=="hand"?"09-drag-hand":!accepted?"11-cancel-drag":destination.y<350?"12-sell-drag":"10-drag-board");
  released=true;dragInput();released=false;down=false;if(accepted&&pendingDrag.is_null())smokeError="Drag disappeared before command response";if(!accepted&&(!pendingDrag.is_null()||dragging||flights.empty()))smokeError="Canceled drag did not return to its slot";
 }
 void smokeTick(float dt){if(!smoke)return;smokeTime+=dt;if(smokeTime>65||!smokeError.empty()){if(smokeError.empty())smokeError="Timeout";std::ofstream(directory/"smoke-result.json")<<json({{"ok",false},{"stage",smokeStage},{"error",smokeError}}).dump(2);exitGame=true;return;}
  if(!statusReplay.is_null()){if(busy||!ready)return;if(smokeStage++==0){setReplay(statusReplay,true);paused=true;return;}const auto& frames=replay["result"]["frames"];int captures=0;bool sawReborn=false;for(int i=0;i<(int)frames.size();i++){bool reborn=i>0&&!rebornName(frames[i-1],frames[i]).empty();bool power=s(frames[i],"text").find(":")!=std::string::npos;if(i>2&&!reborn&&!power)continue;animationFrame=i;animationTime=combatDuration(frames,i)*.70f/std::max(.25f,speed);frameImpact=false;particles.clear();floaters.clear();BeginTextureMode(canvas);drawBackground();combatScene(0);drawEffects();EndTextureMode();take("status-"+std::to_string(i));captures++;sawReborn|=reborn;}std::ofstream(directory/"status-smoke-result.json")<<json({{"ok",sawReborn},{"captures",captures},{"rebornCue",sawReborn}}).dump(2);exitGame=true;return;}
  if(smokeStage==19&&presenting()){if(recruitKind=="enter"&&recruitClock>.33f&&!smokeEntryCaptured){take("13-battlecry-entry");smokeEntryCaptured=true;if(array(player(),"board").size()!=2)smokeError="Battlecry token appeared before minion landed";}if(recruitKind=="battlecry"&&recruitClock>.25f&&!smokeSummonCaptured){take("14-battlecry-summon");smokeSummonCaptured=true;if(array(player(),"board").size()!=3)smokeError="Battlecry token did not appear after entry";}}
  if(busy||!ready||presenting()||transitioning())return;if(++smokeWait<30)return;smokeWait=0;
  switch(smokeStage++){
   case 0:take("01-menu");send({{"type","new"},{"seed",35747},{"hero","curator"},{"difficulty","standard"}});break;
   case 1:scene="tavern";smokeDrag("shop",{800,730});break;
   case 2:smokeDrag("hand",{930,525});break;
   case 3:take("02-tavern");send({{"type","end"}});break;
   case 4:animationFrame=std::min(2,(int)array(replay["result"],"frames").size()-1);animationTime=.86f*.12f;paused=true;frameImpact=false;break;
   case 5:take("03-windup");animationTime=.86f*.40f;frameImpact=false;BeginTextureMode(canvas);drawBackground();combatScene(0);drawEffects();EndTextureMode();take("03-combat");{json savedReplay=replay;if(replay["result"]["winner"].is_null()){auto& r=replay["result"];r["winner"]=r["leftId"];r["damage"]=4;auto& last=r["frames"].back();last["left"]=r["frames"][0]["left"];last["right"]=json::array();if(last["left"].size()>1)last["left"][1]["tier"]=2;}animationFrame=replay["result"]["frames"].size()-1;const auto& end=replay["result"]["frames"][animationFrame];bool own=num(replay["result"],"winner",-1)==num(replay["result"],"leftId");float gather=.45f+array(end,own?"left":"right").size()*.24f+.75f;for(int i=0;i<3;i++){finishClock=i==0?.55f:i==1?gather+.40f:gather+.56f;finishHit=false;floaters.clear();particles.clear();BeginTextureMode(canvas);drawBackground();combatScene(0);drawEffects();EndTextureMode();take(i==0?"22-stars-gather":i==1?"23-hero-strike":"24-hero-damage");if(i<2&&finishHit)smokeError="Hero damage appeared before impact";if(i==2&&!finishHit)smokeError="Hero strike did not apply visual damage";}beginTransition("RECRUIT");transitionClock=.44f;BeginTextureMode(canvas);drawBackground();drawTransition();EndTextureMode();take("25-phase-roll");transitionClock=2;replay=savedReplay;}send({{"type","advance"}});break;
   case 6:if(num(game,"round")!=2)smokeError="Round did not advance";scene="collection";break;
   case 7:take("04-collection");editing=cards[0];editorName=s(editing,"name");editorText=s(editing,"text");editorEffect=s(editing,"effect");break;
   case 8:take("05-editor");editing=nullptr;scene="lab";send({{"type","lab.new"}});break;
   case 9:send({{"type","lab.add"},{"side",0},{"cardId",s(cards[0],"id")},{"golden",false}});break;
   case 10:send({{"type","lab.add"},{"side",1},{"cardId",s(cards[5],"id")},{"golden",true}});break;
   case 11:take("06-lab");send({{"type","lab.odds"},{"samples",100}});break;
   case 12:scene="tavern";debug=true;break;
   case 13:take("07-debug");debug=false;send({{"type","debug"},{"player",0},{"command","gold 10"}});break;
   case 14:if(num(player(),"gold")!=10)smokeError="Debug mutation failed";lastHoverUid=num(player()["board"][0],"uid");smokeDrag("board",{937,525});break;
   case 15:if(num(player()["board"][1],"uid")!=lastHoverUid)smokeError="Reorder drag failed";smokeDrag("board",{250,690},false);break;
   case 16:if(!flights.empty()||dragging||!pendingDrag.is_null())smokeError="Canceled drag left a visual copy";smokeDrag("board",{800,280});break;
   case 17:if(array(player(),"board").size()!=1)smokeError="Sell drag failed";send({{"type","debug"},{"player",0},{"command","give CFM_315"}});break;
   case 18:action({{"type","play"},{"index",0}});break;
   case 19:if(!smokeEntryCaptured||!smokeSummonCaptured||array(player(),"board").size()!=3)smokeError="Battlecry presentation did not complete in order";take("15-battlecry-complete");scene="collection";editing=cards[0];editing["golden"]=true;editorName=s(editing,"name");editorText=s(editing,"text");editorEffect=s(editing,"effect");break;
   case 20:take("16-golden-frame");editing=nullptr;scene="tavern";send({{"type","debug"},{"player",0},{"command","spawn EX1_507"}});break;
   case 21:take("17-warleader-aura");{json target=player()["board"][0];BeginTextureMode(canvas);drawBackground();tavern();auraLinks(target);buffTooltip(target,1030,120);EndTextureMode();take("18-aura-hover");ui::time+=.55f;BeginTextureMode(canvas);drawBackground();tavern();auraLinks(target);buffTooltip(target,1030,120);EndTextureMode();take("21-aura-pulse");json before=player(),after=before;after["board"][0]["keywords"].push_back("DIVINE_SHIELD");shieldWaves.clear();shieldChanges(before,after);if(shieldWaves.empty()||!shieldWaves.back().gain)smokeError="Missing shield gain animation";else shieldWaves.back().age=.16f;BeginTextureMode(canvas);drawBackground();tavern();drawEffects();EndTextureMode();take("19-shield-gain");shieldWaves.clear();shieldChanges(after,before);if(shieldWaves.empty()||shieldWaves.back().gain)smokeError="Missing shield break animation";else shieldWaves.back().age=.16f;BeginTextureMode(canvas);drawBackground();tavern();drawEffects();EndTextureMode();take("20-shield-break");}if(num(player()["board"][0],"attack")!=3||statColor(player()["board"][0],false).g!=240)smokeError="Warleader aura not shown on target";if(!(motion::attackTravel(.1f)<0&&motion::attackTravel(.36f)>.7f&&std::abs(motion::attackTravel(.36f)-motion::attackTravel(.42f))<.001f&&motion::attackTravel(.95f)==0))smokeError="Attack timing regression";std::ofstream(directory/"smoke-result.json")<<json({{"ok",smokeError.empty()},{"round",num(game,"round")},{"gold",num(player(),"gold")},{"samples",num(odds,"samples")},{"screenshots",26},{"battlecryOrder",smokeEntryCaptured&&smokeSummonCaptured},{"dragChecks",{"buy","play","reorder","cancel","sell","source hidden","pending response"}},{"renderer","raylib 6.0 / OpenGL"}}).dump(2);exitGame=true;break;
  }
 }
public:
 int run(int argc,char**argv){for(int i=1;i<argc;i++)if(std::string(argv[i])=="--smoke")smoke=true;else if(std::string(argv[i])=="--status-smoke"&&i+1<argc){smoke=true;std::ifstream fixture(argv[++i]);fixture>>statusReplay;}
  directory=std::filesystem::absolute(std::filesystem::path(argv[0])).parent_path();std::filesystem::create_directories(directory/"screenshots");SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT|(smoke?FLAG_WINDOW_HIDDEN:0));InitWindow(1600,900,"Battlegrounds - The Local Tavern");SetWindowMinSize(1050,650);SetTargetFPS(60);SetExitKey(KEY_NULL);ui::initialize(directory.string());audio.init();board=texture("assets/tavern-board.png");canvas=LoadRenderTexture(W,H);transitionFrame=LoadRenderTexture(W,H);SetTextureFilter(canvas.texture,TEXTURE_FILTER_BILINEAR);
  if(!bridge.start(directory)){busy=false;tell("Could not start the rules engine. Check the data folder beside the executable.");}else bridge.send({{"type","boot"}});
  while(!WindowShouldClose()&&!exitGame){float dt=std::min(GetFrameTime(),.1f);ui::time+=dt;receive();if(overlay.empty()&&!debug)transitionClock+=dt;float scale=std::min((float)GetScreenWidth()/W,(float)GetScreenHeight()/H);Vector2 offset={(GetScreenWidth()-W*scale)*.5f,(GetScreenHeight()-H*scale)*.5f};mouse=Vector2Scale(Vector2Subtract(GetMousePosition(),offset),1/scale);clicked=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);released=IsMouseButtonReleased(MOUSE_BUTTON_LEFT);down=IsMouseButtonDown(MOUSE_BUTTON_LEFT);keyboard();droppedFiles();tickEffects(dt);tickRecruit(dt);if(overlay.empty()&&!debug&&!(scene=="combat"&&paused)){for(auto& wave:shieldWaves)wave.age+=dt*speed;shieldWaves.erase(std::remove_if(shieldWaves.begin(),shieldWaves.end(),[](const ShieldWave& wave){return wave.age>=.48f;}),shieldWaves.end());}hits.clear();hoverUnit=nullptr;
   ui::delta=dt;wantsPointer=false;if(down&&!dragged.is_null()&&Vector2Distance(mouse,dragStart)>9)dragging=true;
   BeginTextureMode(canvas);ClearBackground({29,17,21,255});drawBackground();blocked=transitioning()||!overlay.empty()||debug||!editing.is_null()||(scene=="tavern"&&!array(player(),"discovers").empty());
   if(!ready){text("Opening the tavern...",800,425,35,Pale,true,true);}else if(scene=="menu")menu();else if(scene=="heroes")heroSelection();else if(scene=="tavern")tavern();else if(scene=="combat")combatScene(dt);else if(scene=="collection")collection();else if(scene=="lab")labScene();else if(scene=="history")historyScene();
   dragInput();dragGuide();recruitPulse();drawFlights(dt);drawEffects();blocked=transitioning();
   if(dragging&&!dragged.is_null()){drawingFlight=true;drawMinion(dragged,mouse,1.08f,1,true);drawingFlight=false;}
   else if(!pendingDrag.is_null()){drawingFlight=true;drawMinion(pendingDrag,pendingDragPosition,1.08f,1,true);drawingFlight=false;}
   else if(!hoverUnit.is_null()&&overlay.empty()&&!debug&&editing.is_null()){std::string id=s(hoverUnit,"cardId",s(hoverUnit,"id"))+std::to_string(num(hoverUnit,"uid",-1));if(id!=lastHoverId){lastHoverId=id;hoverStarted=ui::time;buffScroll=0;}auraLinks(hoverUnit);if(ui::time-hoverStarted>.6f)buffTooltip(hoverUnit,mouse.x+35,mouse.y-190);}
   else lastHoverId.clear();
   discoverOverlay();editor();drawTransition();modal();debugPanel();if(busy&&ready){panel({678,12,245,37});text("Thinking...",800,19,18,Cream,true,true);}
   if(noticeTime>0){panel({389,849,822,44});text(notice,800,862,notice.size()>85?14:18,Pale,false,true);}EndTextureMode();SetMouseCursor(dragging?MOUSE_CURSOR_RESIZE_ALL:wantsPointer?MOUSE_CURSOR_POINTING_HAND:MOUSE_CURSOR_DEFAULT);
   BeginDrawing();ClearBackground(BLACK);float sx=shake>0?sinf(ui::time*71)*shake:0;DrawTexturePro(canvas.texture,{0,0,(float)W,(float)-H},{offset.x+sx,offset.y,W*scale,H*scale},{0,0},0,WHITE);EndDrawing();smokeTick(dt);
  }
  audio.close();UnloadRenderTexture(transitionFrame);UnloadRenderTexture(canvas);ui::cleanup();CloseWindow();return smoke&&!smokeError.empty()?1:0;
 }
};
int main(int argc,char**argv){try{
 if(argc>1&&std::string(argv[1])=="--rules"){
  bg::Host host(std::filesystem::absolute(argv[0]).parent_path()/"data");
  std::string line;while(std::getline(std::cin,line)){try{bg::require(line.size()<=40000000,"Request too large");std::cout<<host.handle(bg::J::parse(line)).dump()<<std::endl;}catch(const std::exception& e){std::cout<<bg::J({{"ok",false},{"error",e.what()}}).dump()<<std::endl;}}return 0;
 }
 NativeGame game;return game.run(argc,argv);}catch(const std::exception& e){std::ofstream("native-crash.log")<<e.what();return 1;}}
