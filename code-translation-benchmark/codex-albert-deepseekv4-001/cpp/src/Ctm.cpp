// Translation of source/cfr/Ctm.java.
#include "Ctm.hpp"
#include "java_semantics.hpp"
#include <cmath>
#include <iostream>

void Ctm::MultMat3x3(Matrix& a, Matrix& b, Matrix& dest) {
    dest[0] = a[0] * b[0] + a[1] * b[3] + a[2] * b[6];
    dest[1] = a[0] * b[1] + a[1] * b[4] + a[2] * b[7];
    dest[2] = 0.0f;
    dest[3] = a[3] * b[0] + a[4] * b[3] + a[5] * b[6];
    dest[4] = a[3] * b[1] + a[4] * b[4] + a[5] * b[7];
    dest[5] = 0.0f;
    dest[6] = a[6] * b[0] + a[7] * b[3] + a[8] * b[6];
    dest[7] = a[6] * b[1] + a[7] * b[4] + a[8] * b[7];
    dest[8] = 1.0f;
}

void Ctm::identityctm(Matrix& a) {
    a[0] = 1.0f; a[1] = 0.0f; a[2] = 0.0f;
    a[3] = 0.0f; a[4] = 1.0f; a[5] = 0.0f;
    a[6] = 0.0f; a[7] = 0.0f; a[8] = 1.0f;
}

void Ctm::CopyMat3x3(Matrix& a, Matrix& b) {
    b[0]=a[0]; b[1]=a[1]; b[2]=a[2]; b[3]=a[3]; b[4]=a[4];
    b[5]=a[5]; b[6]=a[6]; b[7]=a[7]; b[8]=a[8];
}

void Ctm::MakeTranslationMatrix(Matrix& a, float dx, float dy) {
    a[0]=1.0f; a[1]=0.0f; a[2]=0.0f; a[3]=0.0f; a[4]=1.0f;
    a[5]=0.0f; a[6]=dx; a[7]=dy; a[8]=1.0f;
}

void Ctm::MakeScaleMatrix(Matrix& a, float sx, float sy) {
    a[0]=sx; a[1]=0.0f; a[2]=0.0f; a[3]=0.0f; a[4]=sy;
    a[5]=0.0f; a[6]=0.0f; a[7]=0.0f; a[8]=1.0f;
}

void Ctm::MakeRotationMatrix(Matrix& a, float rtheta) {
    double theta = (double)rtheta * host::PI / 180.0;
    a[0] = (float)std::cos(theta);
    a[1] = (float)std::sin(theta);
    a[2] = 0.0f;
    a[3] = -((float)std::sin(theta));
    a[4] = (float)std::cos(theta);
    a[5] = 0.0f;
    a[6] = 0.0f;
    a[7] = 0.0f;
    a[8] = 1.0f;
}

void Ctm::MakeSkewMatrix(Matrix& a, float rtheta, float rtheta2) {
    double theta = (double)rtheta * host::PI / 180.0;
    double theta2 = (double)rtheta2 * host::PI / 180.0;
    a[0] = 1.0f;
    a[1] = (float)std::tan(theta);
    a[2] = 0.0f;
    a[3] = -((float)std::tan(theta2));
    a[4] = 1.0f;
    a[5] = 0.0f;
    a[6] = 0.0f;
    a[7] = 0.0f;
    a[8] = 1.0f;
}

void Ctm::rotctm(Matrix& a, float theta) {
    Matrix scratch = {};
    Matrix rotmat = {};
    this->CopyMat3x3(a, scratch);
    this->MakeRotationMatrix(rotmat, theta);
    this->MultMat3x3(rotmat, scratch, a);
}
void Ctm::skewctm(Matrix& a, float theta, float theta2) {
    Matrix scratch = {};
    Matrix rotmat = {};
    this->CopyMat3x3(a, scratch);
    this->MakeSkewMatrix(rotmat, theta, theta2);
    this->MultMat3x3(rotmat, scratch, a);
}
void Ctm::transctm(Matrix& a, float dx, float dy) {
    Matrix scratch = {};
    Matrix xlatemat = {};
    this->CopyMat3x3(a, scratch);
    this->MakeTranslationMatrix(xlatemat, dx, dy);
    this->MultMat3x3(xlatemat, scratch, a);
}
void Ctm::scalectm(Matrix& a, float sx, float sy) {
    Matrix scratch = {};
    Matrix scalemat = {};
    this->CopyMat3x3(a, scratch);
    this->MakeScaleMatrix(scalemat, sx, sy);
    this->MultMat3x3(scalemat, scratch, a);
}
void Ctm::init() {
    this->cp = 0;
    this->identityctm(this->gctm);
    this->CopyMat3x3(this->gctm, this->ctms[0]);
}
Ctm::Ctm() { this->init(); }

// Bounds-checked at() retains Java array exceptions, including the recovered
// push boundary at cp == 15. No silent correction of the original stack logic.
void Ctm::push() {
    if (this->cp >= 16) return;
    this->CopyMat3x3(this->gctm, this->ctms.at(this->cp));
    ++this->cp;
    this->CopyMat3x3(this->gctm, this->ctms.at(this->cp));
}
void Ctm::pop() {
    --this->cp;
    if (this->cp < 0) return;
    this->CopyMat3x3(this->ctms.at(this->cp), this->gctm);
}
void Ctm::rotate(float theta) { this->rotctm(this->gctm, theta); }
void Ctm::skew(float theta, float theta2) { this->skewctm(this->gctm, theta, theta2); }
void Ctm::translate(float dx, float dy) { this->transctm(this->gctm, dx, dy); }
void Ctm::scale(float sx, float sy) { this->scalectm(this->gctm, sx, sy); }
void Ctm::moveto(float x, float y) {
    this->penx = this->x(x, y);
    this->peny = this->y(x, y);
    this->npts = 0;
    this->addpt(this->penx, this->peny);
}
void Ctm::addpt(int x, int y) {
    this->penx = x;
    this->peny = y;
    if (this->npts > static_cast<int>(this->xpts.size()) - 16) {
        int len = static_cast<int>(this->xpts.size());
        std::cout << "*** Growing internal bezier cache larger to total " << (len + this->growchunk) << "\n";
        this->xpts.resize(len + this->growchunk);
        this->ypts.resize(len + this->growchunk);
        std::cout << "**> Successful.\n";
    }
    this->xpts[this->npts] = this->penx;
    this->ypts[this->npts++] = this->peny;
}
void Ctm::curveto(float p2x, float p2y, float p3x, float p3y, float p4x, float p4y) {
    int a = this->x(p2x, p2y);
    int b = this->y(p2x, p2y);
    int c = this->x(p3x, p3y);
    int d = this->y(p3x, p3y);
    int e = this->x(p4x, p4y);
    int f = this->y(p4x, p4y);
    this->BCurve(java::shl(this->penx, 16), java::shl(this->peny, 16),
                java::shl(a, 16), java::shl(b, 16),
                java::shl(c, 16), java::shl(d, 16),
                java::shl(e, 16), java::shl(f, 16));
    this->addpt(e, f);
}
void Ctm::lineto(float x, float y) { this->addpt(this->x(x, y), this->y(x, y)); }
void Ctm::strokeandfill(host::Graphics& g, host::Color cstroke, host::Color cfill) {
    g.setColor(cfill);
    this->gr = &g;
    this->gr->fillPolygon(this->xpts, this->ypts, this->npts);
    g.setColor(cstroke);
    this->stroke(g);
}
void Ctm::fill(host::Graphics& g) {
    this->gr = &g;
    this->gr->fillPolygon(this->xpts, this->ypts, this->npts);
    this->npts = 0;
}
void Ctm::stroke(host::Graphics& g) {
    this->gr = &g;
    this->gr->drawPolygon(this->xpts, this->ypts, this->npts);
    this->npts = 0;
}

// Same subdivision as CFR/Procyon and javap; compound decompiler assignments
// are expanded for readability, with Java int wrap and arithmetic shifts.
void Ctm::BCurve(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4) {
    int x12 = java::shr(java::add(x1, x2), 1);
    int x23 = java::shr(java::add(x2, x3), 1);
    int x34 = java::shr(java::add(x3, x4), 1);
    int x123 = java::shr(java::add(x12, x23), 1);
    int x234 = java::shr(java::add(x23, x34), 1);
    int x1234 = java::shr(java::add(x123, x234), 1);
    int y12 = java::shr(java::add(y1, y2), 1);
    int y23 = java::shr(java::add(y2, y3), 1);
    int y34 = java::shr(java::add(y3, y4), 1);
    int y123 = java::shr(java::add(y12, y23), 1);
    int y234 = java::shr(java::add(y23, y34), 1);
    int y1234 = java::shr(java::add(y123, y234), 1);
    int dx = java::shr(x1 < x1234 ? java::sub(x1234, x1) : java::sub(x1, x1234), 16);
    int dy = java::shr(y1 < y1234 ? java::sub(y1234, y1) : java::sub(y1, y1234), 16);
    if (java::add(dx, dy) > 10) {
        this->BCurve(x1, y1, x12, y12, x123, y123, x1234, y1234);
    }
    int newx = java::shr(java::add(x1234, this->ONEHALF), 16);
    int newy = java::shr(java::add(y1234, this->ONEHALF), 16);
    this->penx = newx;
    this->peny = newy;
    this->addpt(this->penx, this->peny);
    dx = java::shr(x1234 < x4 ? java::sub(x4, x1234) : java::sub(x1234, x4), 16);
    dy = java::shr(y1234 < y4 ? java::sub(y4, y1234) : java::sub(y1234, y4), 16);
    if (java::add(dx, dy) > 10) {
        this->BCurve(x1234, y1234, x234, y234, x34, y34, x4, y4);
    }
}
int Ctm::x(float x, float y) { return java::toInt(x * gctm[0] + y * gctm[3] + gctm[6] + 0.5f); }
int Ctm::xsc(float x) { return java::toInt(x * gctm[0] + 0.5f); }
int Ctm::ysc(float x) { return java::toInt(x * gctm[4] + 0.5f); }
int Ctm::y(float x, float y) { return java::toInt(x * gctm[1] + y * gctm[4] + gctm[7] + 0.5f); }
