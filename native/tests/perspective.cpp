#include "../perspective.hpp"
#include "../motion.hpp"
#include <iostream>
#include <stdexcept>

static void check(bool valid,const char* message){
    if(!valid)throw std::runtime_error(message);
}
int main(){
    try{
        for(int i=0;i<4;i++){
            auto start=motion::cornerFlip(0,i,true);
            auto end=motion::cornerFlip(motion::phaseDuration,i,true);
            check(!start.incoming&&start.angle==0,"Flip must start on the outgoing face");
            check(end.incoming&&std::abs(end.angle)<.001f,"Flip must finish upright on the incoming face");
            float middle=.45f+(i%2)*.045f+(i/2)*.065f;
            check(std::abs(std::abs(motion::cornerFlip(middle,i,true).angle)-PI/2)<.001f,
                  "Face swap must occur edge-on");
            for(float t:{.2f,.4f,.7f,.9f}){
                auto forward=motion::cornerFlip(t,i,true),reverse=motion::cornerFlip(t,i,false);
                check(std::abs(forward.angle+reverse.angle)<.001f&&forward.incoming==reverse.incoming,
                      "Return to recruitment must reverse the flip");
            }
        }
        auto table=perspective::table();
        float farWidth=table.project({1500,100}).x-table.project({100,100}).x;
        float nearWidth=table.project({1500,800}).x-table.project({100,800}).x;
        check(nearWidth>farWidth,"Near board edge must be wider than far edge");
        for(float pitch:{-.4f,0.f,.4f})for(float yaw:{-.38f,0.f,.38f})
        for(float scale:{.9f,1.f,1.1f})for(float lift:{0.f,35.f}){
            perspective::Plane p{{800,525},pitch,yaw,.12f,scale,lift,1000};
            Rectangle local{740,445,120,170};
            for(Vector2 point:std::array<Vector2,5>{{{800,525},{741,446},{859,614},{700,525},{900,625}}}){
                Vector2 projected=p.project(point),restored=p.unproject(projected);
                check(Vector2Distance(point,restored)<.02f,"Projection/picking round trip failed");
                check(p.contains(projected,local)==CheckCollisionPointRec(point,local),
                      "Tilted card picking disagrees with visible plane");
            }
            auto center=p.project({800,525});
            check(std::abs(center.x-800)<.01f&&std::abs(center.y-(525-lift))<.01f,
                  "Lifting must preserve the card anchor");
        }
        std::cout<<"Perspective depth, lift, picking and corner-flip checks passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
