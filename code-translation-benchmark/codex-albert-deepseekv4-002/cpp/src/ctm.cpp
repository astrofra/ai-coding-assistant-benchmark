#include "ctm.h"

#include "clock.h"
#include <cstdio>
#include "surface.h"

#include <cmath>

namespace flora {

Ctm::Ctm() {
    cp_ = 0;
    identityctm(gctm);
    copyMat3x3(gctm, ctms_[0]);
}

void Ctm::multMat3x3(const float* a, const float* b, float* dest) {
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

void Ctm::identityctm(float* a) {
    a[0] = 1.0f; a[1] = 0.0f; a[2] = 0.0f;
    a[3] = 0.0f; a[4] = 1.0f; a[5] = 0.0f;
    a[6] = 0.0f; a[7] = 0.0f; a[8] = 1.0f;
}

void Ctm::copyMat3x3(const float* a, float* b) {
    for (int i = 0; i < 9; i++) b[i] = a[i];
}

void Ctm::makeTranslationMatrix(float* a, float dx, float dy) {
    a[0] = 1.0f; a[1] = 0.0f; a[2] = 0.0f;
    a[3] = 0.0f; a[4] = 1.0f; a[5] = 0.0f;
    a[6] = dx;   a[7] = dy;   a[8] = 1.0f;
}

void Ctm::makeScaleMatrix(float* a, float sx, float sy) {
    a[0] = sx;   a[1] = 0.0f; a[2] = 0.0f;
    a[3] = 0.0f; a[4] = sy;   a[5] = 0.0f;
    a[6] = 0.0f; a[7] = 0.0f; a[8] = 1.0f;
}

void Ctm::makeRotationMatrix(float* a, float rtheta) {
    double theta = (double)rtheta * M_PI / 180.0;
    a[0] = (float)std::cos(theta);
    a[1] = (float)std::sin(theta);
    a[2] = 0.0f;
    a[3] = -(float)std::sin(theta);
    a[4] = (float)std::cos(theta);
    a[5] = 0.0f;
    a[6] = 0.0f; a[7] = 0.0f; a[8] = 1.0f;
}

void Ctm::makeSkewMatrix(float* a, float rtheta, float rtheta2) {
    double theta = (double)rtheta * M_PI / 180.0;
    double theta2 = (double)rtheta2 * M_PI / 180.0;
    a[0] = 1.0f;
    a[1] = (float)std::tan(theta);
    a[2] = 0.0f;
    a[3] = -(float)std::tan(theta2);
    a[4] = 1.0f;
    a[5] = 0.0f;
    a[6] = 0.0f; a[7] = 0.0f; a[8] = 1.0f;
}

void Ctm::rotctm(float* a, float theta) {
    float scratch[9];
    float rotmat[9];
    copyMat3x3(a, scratch);
    makeRotationMatrix(rotmat, theta);
    multMat3x3(rotmat, scratch, a);
}

void Ctm::skewctm(float* a, float theta, float theta2) {
    float scratch[9];
    float rotmat[9];
    copyMat3x3(a, scratch);
    makeSkewMatrix(rotmat, theta, theta2);
    multMat3x3(rotmat, scratch, a);
}

void Ctm::transctm(float* a, float dx, float dy) {
    float scratch[9];
    float xlatemat[9];
    copyMat3x3(a, scratch);
    makeTranslationMatrix(xlatemat, dx, dy);
    multMat3x3(xlatemat, scratch, a);
}

void Ctm::scalectm(float* a, float sx, float sy) {
    float scratch[9];
    float scalemat[9];
    copyMat3x3(a, scratch);
    makeScaleMatrix(scalemat, sx, sy);
    multMat3x3(scalemat, scratch, a);
}

void Ctm::push() {
    if (cp_ >= 16) return;
    copyMat3x3(gctm, ctms_[cp_]);
    ++cp_;
    copyMat3x3(gctm, ctms_[cp_]);
}

void Ctm::pop() {
    --cp_;
    if (cp_ < 0) return;
    copyMat3x3(ctms_[cp_], gctm);
}

void Ctm::rotate(float theta) { rotctm(gctm, theta); }
void Ctm::skew(float theta, float theta2) { skewctm(gctm, theta, theta2); }
void Ctm::translate(float dx, float dy) { transctm(gctm, dx, dy); }
void Ctm::scale(float sx, float sy) { scalectm(gctm, sx, sy); }

void Ctm::moveto(float x, float y) {
    penx_ = this->x(x, y);
    peny_ = this->y(x, y);
    npts_ = 0;
    addpt(penx_, peny_);
}

void Ctm::addpt(int x, int y) {
    penx_ = x;
    peny_ = y;
    if (npts_ > cap_ - 16) {
        cap_ += GROWCHUNK;
        /* grow internal cache; not expected for Flora's petal shapes */
    }
    xpts_[npts_] = penx_;
    ypts_[npts_] = peny_;
    ++npts_;
}

void Ctm::curveto(float p2x, float p2y, float p3x, float p3y, float p4x, float p4y) {
    int a = this->x(p2x, p2y);
    int b = this->y(p2x, p2y);
    int c = this->x(p3x, p3y);
    int d = this->y(p3x, p3y);
    int e = this->x(p4x, p4y);
    int f = this->y(p4x, p4y);
    bcurve(penx_ << 16, peny_ << 16, a << 16, b << 16,
           c << 16, d << 16, e << 16, f << 16);
    addpt(e, f);
}

void Ctm::lineto(float x, float y) { addpt(this->x(x, y), this->y(x, y)); }

void Ctm::bcurve(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4) {
    int y34, y234, y23, y12, y123, y1234;
    int x12 = (x1 + x2) >> 1;
    int x23 = (x2 + x3) >> 1;
    int x123 = (x12 + x23) >> 1;
    int x34 = (x3 + x4) >> 1;
    int x234 = (x23 + x34) >> 1;
    int x1234 = (x123 + x234) >> 1;
    int dx = (x1 < x1234 ? x1234 - x1 : x1 - x1234) >> 16;
    y12 = (y1 + y2) >> 1;
    y23 = (y2 + y3) >> 1;
    y123 = (y12 + y23) >> 1;
    y34 = (y3 + y4) >> 1;
    y234 = (y23 + y34) >> 1;
    y1234 = (y123 + y234) >> 1;
    int dy = (y1 < y1234 ? y1234 - y1 : y1 - y1234) >> 16;
    if (dx + dy > FLAT) {
        bcurve(x1, y1, x12, y12, x123, y123, x1234, y1234);
    }
    int newx = (x1234 + ONEHALF) >> 16;
    int newy = (y1234 + ONEHALF) >> 16;
    penx_ = newx;
    peny_ = newy;
    addpt(penx_, peny_);
    dx = (x1234 < x4 ? x4 - x1234 : x1234 - x4) >> 16;
    dy = (y1234 < y4 ? y4 - y1234 : y1234 - y4) >> 16;
    if (dx + dy > FLAT) {
        bcurve(x1234, y1234, x234, y234, x34, y34, x4, y4);
    }
}

int Ctm::x(float x, float y) {
    return (int)(x * gctm[0] + y * gctm[3] + gctm[6] + 0.5f);
}

int Ctm::y(float x, float y) {
    return (int)(x * gctm[1] + y * gctm[4] + gctm[7] + 0.5f);
}

void Ctm::fill(Surface* surf, const Color& col) {
    // Geometry trace, if active, mirrors the Java .geometry format.
    if (Clock::geometry != nullptr) {
        std::fprintf(Clock::geometry, "%d %d", (col.r << 16) | (col.g << 8) | col.b, npts_);
        for (int i = 0; i < npts_; i++) {
            std::fprintf(Clock::geometry, " %d %d", xpts_[i], ypts_[i]);
        }
        std::fprintf(Clock::geometry, "\n");
    }
    surf->fillPolygon(xpts_, ypts_, npts_, col);
    npts_ = 0;
}

}  // namespace flora
