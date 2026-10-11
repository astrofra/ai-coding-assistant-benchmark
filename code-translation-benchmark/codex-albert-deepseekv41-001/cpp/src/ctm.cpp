#include "ctm.h"

#include <cmath>
#include <cstdlib>

#include "clock.h"
#include "render.h"

static const double kPi = 3.14159265358979323846;

void Ctm::MultMat3x3(const float* a, const float* b, float* dest) {
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

void Ctm::CopyMat3x3(const float* a, float* b) {
    b[0] = a[0]; b[1] = a[1]; b[2] = a[2];
    b[3] = a[3]; b[4] = a[4]; b[5] = a[5];
    b[6] = a[6]; b[7] = a[7]; b[8] = a[8];
}

void Ctm::MakeTranslationMatrix(float* a, float dx, float dy) {
    a[0] = 1.0f;	a[1] = 0.0f;	a[2] = 0.0f;
    a[3] = 0.0f;	a[4] = 1.0f;	a[5] = 0.0f;
    a[6] = dx;		a[7] = dy;		a[8] = 1.0f;
}

void Ctm::MakeScaleMatrix(float* a, float sx, float sy) {
    a[0] = sx;		a[1] = 0.0f;	a[2] = 0.0f;
    a[3] = 0.0f;	a[4] = sy;		a[5] = 0.0f;
    a[6] = 0.0f;	a[7] = 0.0f;	a[8] = 1.0f;
}

void Ctm::MakeRotationMatrix(float* a, float rtheta) {
    double theta = (double)rtheta * kPi / 180.0;
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

void Ctm::MakeSkewMatrix(float* a, float rtheta, float rtheta2) {
    double theta = (double)rtheta * kPi / 180.0;
    double theta2 = (double)rtheta2 * kPi / 180.0;
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

void Ctm::rotctm(float* a, float theta) {
    float scratch[9];
    float rotmat[9];
    CopyMat3x3(a, scratch);
    MakeRotationMatrix(rotmat, theta);
    MultMat3x3(rotmat, scratch, a);
}

void Ctm::skewctm(float* a, float theta, float theta2) {
    float scratch[9];
    float rotmat[9];
    CopyMat3x3(a, scratch);
    MakeSkewMatrix(rotmat, theta, theta2);
    MultMat3x3(rotmat, scratch, a);
}

void Ctm::transctm(float* a, float dx, float dy) {
    float scratch[9];
    float xlatemat[9];
    CopyMat3x3(a, scratch);
    MakeTranslationMatrix(xlatemat, dx, dy);
    MultMat3x3(xlatemat, scratch, a);
}

void Ctm::scalectm(float* a, float sx, float sy) {
    float scratch[9];
    float scalemat[9];
    CopyMat3x3(a, scratch);
    MakeScaleMatrix(scalemat, sx, sy);
    MultMat3x3(scalemat, scratch, a);
}

void Ctm::init() {
    cp = 0;
    identityctm(gctm);
    CopyMat3x3(gctm, ctms[0]);
    xpts.reserve(256);
    ypts.reserve(256);
}

void Ctm::push() {
    if (cp >= 16) return;
    CopyMat3x3(gctm, ctms[cp]);  // double copy preserved from the source
    ++cp;
    CopyMat3x3(gctm, ctms[cp]);
}

void Ctm::pop() {
    --cp;
    if (cp < 0) return;
    CopyMat3x3(ctms[cp], gctm);
}

void Ctm::rotate(float theta) { rotctm(gctm, theta); }
void Ctm::skew(float theta, float theta2) { skewctm(gctm, theta, theta2); }
void Ctm::translate(float dx, float dy) { transctm(gctm, dx, dy); }
void Ctm::scale(float sx, float sy) { scalectm(gctm, sx, sy); }

void Ctm::moveto(float x, float y) {
    penx = this->x(x, y);
    peny = this->y(x, y);
    npts = 0;
    addpt(penx, peny);
}

void Ctm::addpt(int x, int y) {
    penx = x;
    peny = y;
    // Java grows a fixed 256-slot cache here (printing to stdout); a vector is
    // equivalent for the rendered result.
    if ((int)xpts.size() <= npts) {
        xpts.push_back(x);
        ypts.push_back(y);
    } else {
        xpts[npts] = x;
        ypts[npts] = y;
    }
    ++npts;
}

void Ctm::curveto(float p2x, float p2y, float p3x, float p3y, float p4x, float p4y) {
    int a = x(p2x, p2y);
    int b = y(p2x, p2y);
    int c = x(p3x, p3y);
    int d = y(p3x, p3y);
    int e = x(p4x, p4y);
    int f = y(p4x, p4y);
    BCurve(penx << 16, peny << 16, a << 16, b << 16, c << 16, d << 16, e << 16, f << 16);
    addpt(e, f);
}

void Ctm::lineto(float x, float y) { addpt(this->x(x, y), this->y(x, y)); }

void Ctm::strokeandfill(Graphics& g, const Color& cstroke, const Color& cfill) {
    g.setColor(cfill);
    gr = &g;
    gr->fillPolygon(xpts.data(), ypts.data(), npts);
    g.setColor(cstroke);
    stroke(g);
}

void Ctm::fill(Graphics& g) {
    if (Clock::geometry != nullptr) {
        std::ostream& o = *Clock::geometry;
        o << (g.getColorRGB()) << " " << npts;
        for (int i = 0; i < npts; ++i) o << " " << xpts[i] << " " << ypts[i];
        o << "\n";
    }
    gr = &g;
    gr->fillPolygon(xpts.data(), ypts.data(), npts);
    npts = 0;
}

void Ctm::stroke(Graphics& g) {
    gr = &g;
    gr->drawPolygon(xpts.data(), ypts.data(), npts);
    npts = 0;
}

// Recursive fixed-point Bezier subdivision, 16.16 coordinates.
void Ctm::BCurve(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4) {
    int x12 = (x1 + x2) >> 1;
    int x23 = (x2 + x3) >> 1;
    int x123 = (x12 + x23) >> 1;
    int x34 = (x3 + x4) >> 1;
    int x234 = (x23 + x34) >> 1;
    int x1234 = (x123 + x234) >> 1;

    int y12 = (y1 + y2) >> 1;
    int y23 = (y2 + y3) >> 1;
    int y34 = (y3 + y4) >> 1;
    int y123 = (y12 + y23) >> 1;
    int y234 = (y23 + y34) >> 1;
    int y1234 = (y123 + y234) >> 1;

    int dx = (x1 < x1234 ? x1234 - x1 : x1 - x1234) >> 16;
    int dy = (y1 < y1234 ? y1234 - y1 : y1 - y1234) >> 16;
    if (dx + dy > 10) {
        BCurve(x1, y1, x12, y12, x123, y123, x1234, y1234);
    }

    int newx = (x1234 + ONEHALF) >> 16;
    int newy = (y1234 + ONEHALF) >> 16;
    penx = newx;
    peny = newy;
    addpt(penx, peny);

    dx = (x1234 < x4 ? x4 - x1234 : x1234 - x4) >> 16;
    dy = (y1234 < y4 ? y4 - y1234 : y1234 - y4) >> 16;
    if (dx + dy > 10) {
        BCurve(x1234, y1234, x234, y234, x34, y34, x4, y4);
    }
}
