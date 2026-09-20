#pragma once
#include "raylib.h"
#include "raymath.h"
#include <string>
#include <map>
#include <vector>
#include <cmath>
#include <sstream>
#include <algorithm>

namespace ui {
inline constexpr int W=1600,H=900;
inline Color Ink{48,26,18,255},Cream{250,224,161,255},Gold{221,169,66,255},Pale{255,241,194,255},Wood{66,35,21,255};
inline Font title{},body{};inline Shader ovalShader{},panelShader{},boardShader{};inline int uvLocation=-1,panelUv=-1;inline Vector2 mouse{};inline bool clicked=false,released=false,down=false,blocked=false;inline std::string focus;
inline std::map<std::string,Texture2D> textures;
inline std::string root;
inline float time=0;
inline float delta=1.f/60;inline bool wantsPointer=false;
inline std::map<std::string,float> buttonGlow;
struct EditState{size_t cursor=0,anchor=0;float scroll=0;};
inline std::map<std::string,EditState> edits;
inline void initialize(const std::string& path){root=path;
#ifdef __APPLE__
title=LoadFontEx("/System/Library/Fonts/Supplemental/Georgia Bold.ttf",64,nullptr,250);body=LoadFontEx("/System/Library/Fonts/Supplemental/Arial.ttf",40,nullptr,250);
#else
title=LoadFontEx("C:/Windows/Fonts/georgiab.ttf",64,nullptr,250);body=LoadFontEx("C:/Windows/Fonts/segoeui.ttf",40,nullptr,250);
#endif
if(!title.texture.id)title=GetFontDefault();if(!body.texture.id)body=GetFontDefault();
 SetTextureFilter(title.texture,TEXTURE_FILTER_BILINEAR);SetTextureFilter(body.texture,TEXTURE_FILTER_BILINEAR);
 const char* fragment="#version 330\nin vec2 fragTexCoord;in vec4 fragColor;out vec4 finalColor;uniform sampler2D texture0;uniform vec4 colDiffuse;uniform vec4 uvBounds;void main(){vec2 q=(fragTexCoord-uvBounds.xy)/uvBounds.zw*2.0-1.0;float d=dot(q,q);if(d>1.0)discard;finalColor=texture(texture0,fragTexCoord)*fragColor*colDiffuse;finalColor.a*=1.0-smoothstep(0.95,1.0,d);}";
 ovalShader=LoadShaderFromMemory(nullptr,fragment);uvLocation=GetShaderLocation(ovalShader,"uvBounds");
 const char* panelFragment="#version 330\nin vec2 fragTexCoord;in vec4 fragColor;out vec4 finalColor;uniform sampler2D texture0;uniform vec4 colDiffuse;uniform vec4 uvBounds;void main(){vec2 q=(fragTexCoord-uvBounds.xy)/uvBounds.zw;float edge=min(min(q.x,1.0-q.x),min(q.y,1.0-q.y));finalColor=texture(texture0,fragTexCoord)*fragColor*colDiffuse;finalColor.a*=smoothstep(0.0,0.20,edge);}";
 panelShader=LoadShaderFromMemory(nullptr,panelFragment);panelUv=GetShaderLocation(panelShader,"uvBounds");
 const char* boardFragment="#version 330\nin vec2 fragTexCoord;in vec4 fragColor;out vec4 finalColor;uniform sampler2D texture0;uniform vec4 colDiffuse;void main(){vec4 c=texture(texture0,fragTexCoord)*fragColor*colDiffuse;float l=dot(c.rgb,vec3(0.299,0.587,0.114));c.rgb=mix(c.rgb,vec3(l),0.28)*vec3(0.94,0.97,1.08);finalColor=c;}";
 boardShader=LoadShaderFromMemory(nullptr,boardFragment);
}
inline Texture2D texture(const std::string& name){if(name.empty())return {};auto it=textures.find(name);if(it!=textures.end())return it->second;auto full=root+"/"+name;Texture2D t{};if(FileExists(full.c_str())){t=LoadTexture(full.c_str());if(t.id){GenTextureMipmaps(&t);SetTextureFilter(t,TEXTURE_FILTER_TRILINEAR);}}textures[name]=t;return t;}
inline void auraGlow(Vector2 center,float width,float height,float phase,float opacity){
 const std::string key="@soft-aura";if(!textures.contains(key)){Image img=GenImageColor(128,128,BLANK);auto pixels=(Color*)img.data;for(int y=0;y<128;y++)for(int x=0;x<128;x++){float dx=(x-63.5f)/63.5f,dy=(y-63.5f)/63.5f;float falloff=std::max(0.f,1-dx*dx-dy*dy);pixels[y*128+x]={255,244,177,(unsigned char)(255*falloff*falloff)};}textures[key]=LoadTextureFromImage(img);UnloadImage(img);SetTextureFilter(textures[key],TEXTURE_FILTER_BILINEAR);}
 for(int i=0;i<2;i++){float t=fmodf(phase+i*.5f,1.f);float radius=.12f+1.30f*t;float fade=sinf(t*PI);fade*=fade;Rectangle r={center.x-width*radius,center.y-height*radius,width*radius*2,height*radius*2};DrawTexturePro(textures[key],{0,0,128,128},r,{0,0},0,Fade(WHITE,opacity*fade));}
}
inline void text(const std::string& s,float x,float y,float size,Color color,bool fancy=false,bool center=false){Font f=fancy?title:body;float width=MeasureTextEx(f,s.c_str(),size,0).x;if(center)x-=width*.5f;if(fancy&&size>=18&&color.r>150)DrawTextEx(f,s.c_str(),{x+1,y+1},size,0,Color{0,0,0,(unsigned char)(color.a*.5f)});DrawTextEx(f,s.c_str(),{x,y},size,0,color);}
inline float wrapped(const std::string& s,float x,float y,float maxWidth,float size,Color color,bool center=false){std::istringstream words(s);std::string word,line;float yy=y;while(words>>word){std::string next=line.empty()?word:line+" "+word;if(MeasureTextEx(body,next.c_str(),size,0).x>maxWidth&&!line.empty()){text(line,x,yy,size,color,false,center);yy+=size*1.22f;line=word;}else line=next;}if(!line.empty()){text(line,x,yy,size,color,false,center);yy+=size*1.22f;}return yy;}
inline bool hover(Rectangle r){return !blocked&&CheckCollisionPointRec(mouse,r);}
inline void panel(Rectangle r,Color fill=Wood,bool parchment=false){DrawRectangleRounded({r.x+5,r.y+9,r.width,r.height},.08f,10,{0,0,0,120});DrawRectangleRounded(r,.08f,10,{31,16,13,255});DrawRectangleRounded({r.x+3,r.y+3,r.width-6,r.height-6},.075f,10,{124,76,37,255});DrawRectangleRounded({r.x+7,r.y+7,r.width-14,r.height-14},.065f,10,parchment?Color{217,174,106,255}:fill);DrawRectangleRoundedLinesEx({r.x+4,r.y+4,r.width-8,r.height-8},.07f,10,2,{198,144,64,255});
 for(int i=0;i<4;i++){float x=r.x+(i%2?r.width-15:15),y=r.y+(i/2?r.height-15:15);DrawCircle((int)x+1,(int)y+2,4,{28,19,13,255});DrawCircle((int)x,(int)y,3,{168,118,49,255});DrawCircle((int)x-1,(int)y-1,1,{237,193,108,255});}
}
inline bool button(const std::string& label,Rectangle r,bool enabled=true,bool blue=false,float size=23){
 bool hot=hover(r)&&enabled;wantsPointer|=hot;auto key=std::to_string((int)r.x)+":"+std::to_string((int)r.y);float& glow=buttonGlow[key];glow+=(float(hot)-glow)*std::min(1.f,delta*14);
 Color base=!enabled?Color{67,57,47,255}:blue?Color{37,91,114,255}:Color{132,78,33,255};
 DrawRectangleRounded({r.x-2,r.y+5,r.width+4,r.height+3},.23f,10,{28,17,13,255});if(hot&&down)r.y+=3;
 DrawRectangleRounded(r,.2f,10,{60,37,24,255});DrawRectangleRounded({r.x+3,r.y+3,r.width-6,r.height-6},.17f,10,base);
 for(int i=0;i<4;i++){float y=r.y+8+i*(r.height-15)/4;DrawLineEx({r.x+11,y},{r.x+r.width-11,y+1},1,Fade(blue?SKYBLUE:Gold,enabled?.08f:.025f));}
 DrawRectangleRounded({r.x+4,r.y+4,r.width-8,r.height-8},.15f,10,Fade(blue?SKYBLUE:Gold,glow*.19f));
 DrawRectangleRoundedLinesEx({r.x+2,r.y+2,r.width-4,r.height-4},.18f,10,2,enabled?(hot?Pale:Gold):Color{99,83,60,255});
 DrawLineEx({r.x+12,r.y+6},{r.x+r.width-12,r.y+6},2,Fade(Pale,enabled?.33f:.1f));DrawLineEx({r.x+12,r.y+r.height-5},{r.x+r.width-12,r.y+r.height-5},2,{48,29,18,180});
 while(MeasureTextEx(title,label.c_str(),size,0).x>r.width-20&&size>12)size--;
 text(label,r.x+r.width*.5f,r.y+(r.height-size)*.5f-1,size,enabled?Pale:Color{149,138,117,255},true,true);return hot&&clicked;
}
inline void gem(float x,float y,float radius,Color color,const std::string& value,float fontSize=23,int sides=6){DrawPoly({x+2,y+4},sides,radius+3,-90,{36,20,17,255});DrawPoly({x,y},sides,radius+2,-90,Gold);DrawPoly({x,y},sides,radius-1,-90,color);DrawPolyLinesEx({x,y},sides,radius-3,-90,2,Fade(Pale,.4f));text(value,x,y-fontSize*.56f,fontSize,WHITE,true,true);}
inline void star(float x,float y,float radius,Color c){Vector2 p[10];for(int i=0;i<10;i++){float a=-PI/2+i*PI/5;float rr=i%2?radius*.45f:radius;p[i]={x+cosf(a)*rr,y+sinf(a)*rr};}for(int i=0;i<10;i++)DrawTriangle({x,y},p[(i+1)%10],p[i],c);}
inline void portrait(const std::string& art,Rectangle r,Color tint=WHITE){
 std::string resolved=art;
 if(art.starts_with("art/")){std::string id=art.substr(4);if(id.starts_with("render_"))id.erase(0,7);auto full="art/full_"+id;if(texture(full).id)resolved=full;}
 auto t=texture(resolved);if(!t.id){DrawEllipse((int)(r.x+r.width/2),(int)(r.y+r.height/2),r.width/2,r.height/2,{51,60,76,255});star(r.x+r.width/2,r.y+r.height/2,r.width*.22f,{142,167,193,255});return;}
 Rectangle source={0,0,(float)t.width,(float)t.height};if(resolved.find("render_")!=std::string::npos){source={t.width*.255f,t.height*.072f,t.width*.50f,t.height*.395f};}else{float aspect=r.width/r.height,ts=(float)t.width/t.height;if(ts>aspect){source.width=t.height*aspect;source.x=(t.width-source.width)*.5f;}else{source.height=t.width/aspect;source.y=(t.height-source.height)*.5f;}}
 float uv[4]={source.x/t.width,source.y/t.height,source.width/t.width,source.height/t.height};BeginShaderMode(ovalShader);SetShaderValue(ovalShader,uvLocation,uv,SHADER_UNIFORM_VEC4);DrawTexturePro(t,source,r,{0,0},0,tint);EndShaderMode();
}
inline void layer(const std::string& file,Rectangle r,Color tint=WHITE){auto t=texture("assets/frames/"+file);if(t.id)DrawTexturePro(t,{0,0,(float)t.width,(float)t.height},r,{0,0},0,tint);}
inline void minionRim(Vector2 center,float w,float h,bool golden,Color tint=WHITE){auto t=texture(golden?"assets/frames/base-minion-premium.png":"assets/frames/frame-minion-neutral.png");if(!t.id)return;Rectangle src=golden?Rectangle{57,0,410,535}:Rectangle{87,0,356,490};float uv[]={src.x/t.width,src.y/t.height,src.width/t.width,src.height/t.height};BeginShaderMode(ovalShader);SetShaderValue(ovalShader,uvLocation,uv,SHADER_UNIFORM_VEC4);DrawTexturePro(t,src,{center.x-w/2,center.y-h/2,w,h},{0,0},0,tint);EndShaderMode();}
inline void statBadge(Vector2 p,float scale,int value,bool health,bool golden=false,Color tint=WHITE,Color numberColor=WHITE){std::string suffix=golden?"-premium.png":".png";layer((health?"health":"attack-minion")+suffix,{p.x-(health?18:30)*scale,p.y-30*scale,(health?39:57)*scale,62*scale},tint);float size=30*scale;std::string label=std::to_string(value);if(label.size()>2)size*=.75f;for(auto offset:std::vector<Vector2>{{-1.8f,0},{1.8f,0},{0,-1.8f},{0,1.8f}})text(label,p.x+offset.x*scale,p.y-size*.5f+offset.y*scale,size,Fade(BLACK,tint.a/255.f),true,true);text(label,p.x,p.y-size*.5f,size,Fade(numberColor,tint.a/255.f),true,true);}
inline bool input(const std::string& id,std::string& value,Rectangle r,bool numeric=false){
 bool changed=false;auto& edit=edits[id];edit.cursor=std::min(edit.cursor,value.size());edit.anchor=std::min(edit.anchor,value.size());
 auto width=[&](size_t n){return MeasureTextEx(body,value.substr(0,n).c_str(),20,0).x;};
 if(hover(r)&&clicked){bool fresh=focus!=id;focus=id;edit.cursor=value.size();if(!fresh){float x=mouse.x-r.x-10+edit.scroll;for(size_t i=0;i<=value.size();i++)if(width(i)>=x){edit.cursor=i;break;}}edit.anchor=edit.cursor;}
 DrawRectangleRounded(r,.08f,6,{33,24,23,255});DrawRectangleRoundedLinesEx(r,.08f,6,focus==id?2:1,focus==id?Gold:Color{109,79,47,255});
 auto eraseSelection=[&](){size_t first=std::min(edit.cursor,edit.anchor),last=std::max(edit.cursor,edit.anchor);if(first!=last){value.erase(first,last-first);edit.cursor=edit.anchor=first;changed=true;return true;}return false;};
 if(focus==id&&!blocked){bool ctrl=IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL),shift=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT);
  if(ctrl&&IsKeyPressed(KEY_A)){edit.anchor=0;edit.cursor=value.size();}
  if(ctrl&&(IsKeyPressed(KEY_C)||IsKeyPressed(KEY_X))&&edit.cursor!=edit.anchor){SetClipboardText(value.substr(std::min(edit.cursor,edit.anchor),std::max(edit.cursor,edit.anchor)-std::min(edit.cursor,edit.anchor)).c_str());if(IsKeyPressed(KEY_X))eraseSelection();}
  std::string inserted;if(ctrl&&IsKeyPressed(KEY_V)){const char* paste=GetClipboardText();if(paste)for(unsigned char c:std::string(paste))if(c>=32&&c<127&&(!numeric||(c>='0'&&c<='9')))inserted+=(char)c;}
  int ch;while((ch=GetCharPressed())>0)if(!ctrl&&ch>=32&&ch<127&&(!numeric||(ch>='0'&&ch<='9')))inserted+=(char)ch;
  if(!inserted.empty()){eraseSelection();inserted.resize(std::min(inserted.size(),size_t(1000)-std::min(value.size(),size_t(1000))));value.insert(edit.cursor,inserted);edit.cursor+=inserted.size();edit.anchor=edit.cursor;changed=true;}
  if(IsKeyPressed(KEY_BACKSPACE)||IsKeyPressedRepeat(KEY_BACKSPACE)){if(!eraseSelection()&&edit.cursor){value.erase(--edit.cursor,1);edit.anchor=edit.cursor;changed=true;}}
  if(IsKeyPressed(KEY_DELETE)||IsKeyPressedRepeat(KEY_DELETE)){if(!eraseSelection()&&edit.cursor<value.size()){value.erase(edit.cursor,1);changed=true;}}
  int move=0;if(IsKeyPressed(KEY_LEFT)||IsKeyPressedRepeat(KEY_LEFT))move=-1;if(IsKeyPressed(KEY_RIGHT)||IsKeyPressedRepeat(KEY_RIGHT))move=1;
  if(move){if(!shift&&edit.cursor!=edit.anchor)edit.cursor=move<0?std::min(edit.cursor,edit.anchor):std::max(edit.cursor,edit.anchor);else edit.cursor=std::clamp((int)edit.cursor+move,0,(int)value.size());if(!shift)edit.anchor=edit.cursor;}
  if(IsKeyPressed(KEY_HOME)){edit.cursor=0;if(!shift)edit.anchor=0;}if(IsKeyPressed(KEY_END)){edit.cursor=value.size();if(!shift)edit.anchor=edit.cursor;}if(IsKeyPressed(KEY_ENTER))focus.clear();
 }
 float caret=width(edit.cursor),available=r.width-22;if(caret-edit.scroll>available)edit.scroll=caret-available;if(caret<edit.scroll)edit.scroll=caret;edit.scroll=std::max(0.f,edit.scroll);
 BeginScissorMode((int)r.x+7,(int)r.y+3,(int)r.width-14,(int)r.height-6);float left=r.x+10-edit.scroll;
 if(focus==id&&edit.cursor!=edit.anchor)DrawRectangleRec({left+width(std::min(edit.cursor,edit.anchor)),r.y+7,std::abs(width(edit.cursor)-width(edit.anchor)),25},{58,104,124,190});
 text(value,left,r.y+8,20,Cream);if(focus==id&&((int)(time*2)%2==0))DrawLineEx({left+caret,r.y+8},{left+caret,r.y+31},2,Gold);EndScissorMode();return changed;
}
inline void cleanup(){for(auto&[name,t]:textures)if(t.id)UnloadTexture(t);UnloadShader(ovalShader);UnloadShader(panelShader);UnloadShader(boardShader);if(title.texture.id!=GetFontDefault().texture.id)UnloadFont(title);if(body.texture.id!=GetFontDefault().texture.id)UnloadFont(body);}
}
