#pragma once
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <array>
#include <cmath>

// Textured planes in a fixed perspective camera. Screen-space UI remains flat.
// The same homogeneous transform is used for drawing and pointer hit tests.
namespace perspective {
struct Plane {
    Vector2 center;
    float pitch = 0, yaw = 0, roll = 0, scale = 1, lift = 0, focal = 1100;

    Matrix matrix() const {
        Matrix rotation = MatrixRotateXYZ({pitch, yaw, roll});
        Vector3 x = Vector3Transform({scale, 0, 0}, rotation);
        Vector3 y = Vector3Transform({0, scale, 0}, rotation);
        float cy = center.y - lift;
        Matrix m = MatrixIdentity();
        m.m3 = -x.z/focal;
        m.m7 = -y.z/focal;
        m.m15 = 1 - m.m3*center.x - m.m7*center.y;
        m.m0 = x.x + center.x*m.m3;
        m.m4 = y.x + center.x*m.m7;
        m.m12 = center.x - m.m0*center.x - m.m4*center.y;
        m.m1 = x.y + cy*m.m3;
        m.m5 = y.y + cy*m.m7;
        m.m13 = cy - m.m1*center.x - m.m5*center.y;
        return m;
    }
    Vector2 project(Vector2 p) const {
        Matrix m = matrix();
        float w = m.m3*p.x + m.m7*p.y + m.m15;
        return {(m.m0*p.x + m.m4*p.y + m.m12)/w,
                (m.m1*p.x + m.m5*p.y + m.m13)/w};
    }
    Vector2 unproject(Vector2 p) const {
        Matrix m = MatrixInvert(matrix());
        float w = m.m3*p.x + m.m7*p.y + m.m15;
        return {(m.m0*p.x + m.m4*p.y + m.m12)/w,
                (m.m1*p.x + m.m5*p.y + m.m13)/w};
    }
    Rectangle bounds(Rectangle r) const {
        std::array<Vector2,4> corners = {project({r.x,r.y}),project({r.x+r.width,r.y}),
            project({r.x+r.width,r.y+r.height}),project({r.x,r.y+r.height})};
        float left=corners[0].x,right=left,top=corners[0].y,bottom=top;
        for(auto p:corners){left=std::min(left,p.x);right=std::max(right,p.x);
            top=std::min(top,p.y);bottom=std::max(bottom,p.y);}
        return {left,top,right-left,bottom-top};
    }
    bool contains(Vector2 point, Rectangle local) const {
        return CheckCollisionPointRec(unproject(point),local);
    }
};

// Projection, rather than rlgl's CPU model transform, preserves homogeneous W
// across all existing sprite, font and oval-mask shaders.
class Scope {
public:
    explicit Scope(const Plane& plane) {
        rlDrawRenderBatchActive();
        rlMatrixMode(RL_PROJECTION);
        rlPushMatrix();
        Matrix projection = MatrixMultiply(plane.matrix(), rlGetMatrixProjection());
        // 2D batches start on the near clip plane. Keep painter-ordered planes
        // inside the clip volume after changing W, including masked portraits.
        projection.m2 = projection.m6 = projection.m10 = projection.m14 = 0;
        rlSetMatrixProjection(projection);
        rlMatrixMode(RL_MODELVIEW);
    }
    ~Scope() {
        rlDrawRenderBatchActive();
        rlMatrixMode(RL_PROJECTION);
        rlPopMatrix();
        rlMatrixMode(RL_MODELVIEW);
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};
inline Plane table(){return {{800,450},.20f,0,0,1,0,2600};}
inline Vector2 boardPoint(Vector2 p){return table().project(p);}
inline float depthScale(float y){return std::clamp(1+(y-450)*.00023f,.90f,1.09f);}
inline void shadow(Vector2 center,float width,float height,float lift,float alpha=1) {
    // Broadening contact shadow as a card rises off the table.
    float spread=1+lift*.005f;
    for(int i=5;i>=0;i--)
        DrawEllipse(center.x+5+lift*.18f,center.y+10+lift*.22f,
                    width*spread+i*2,height*spread+i*1.4f,
                    Fade(BLACK,alpha*(.018f+(5-i)*.005f)));
}
}
