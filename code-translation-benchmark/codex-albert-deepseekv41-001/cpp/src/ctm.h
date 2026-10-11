// Port of Ctm: affine transform (gctm[9] / 16-deep stack) plus a fixed-point
// Bezier path builder with integer fillPolygon semantics.
//
// The matrix layout, multiplication order, the double-copy push() and the
// decrement-before-bounds-check pop() are reproduced verbatim from the source.
#pragma once

#include <vector>

struct Graphics;

class Ctm {
public:
    float gctm[9];
    int cp = 0;
    static const int N = 16;
    float ctms[N + 1][9];  // one spare slot so the literal push() never indexes OOB

    int penx = 0, peny = 0;
    int ONEHALF = 32768;
    std::vector<int> xpts, ypts;
    int growchunk = 256;  // retained for fidelity; std::vector grows as needed
    int npts = 0;
    Graphics* gr = nullptr;

    Ctm() { init(); }

    void init();

    // Matrix primitives.
    static void MultMat3x3(const float* a, const float* b, float* dest);
    static void identityctm(float* a);
    static void CopyMat3x3(const float* a, float* b);
    static void MakeTranslationMatrix(float* a, float dx, float dy);
    static void MakeScaleMatrix(float* a, float sx, float sy);
    static void MakeRotationMatrix(float* a, float rtheta);
    static void MakeSkewMatrix(float* a, float rtheta, float rtheta2);

    void rotctm(float* a, float theta);
    void skewctm(float* a, float theta, float theta2);
    void transctm(float* a, float dx, float dy);
    void scalectm(float* a, float sx, float sy);

    void push();
    void pop();
    void rotate(float theta);
    void skew(float theta, float theta2);
    void translate(float dx, float dy);
    void scale(float sx, float sy);

    // Path builder.
    void moveto(float x, float y);
    void addpt(int x, int y);
    void curveto(float p2x, float p2y, float p3x, float p3y, float p4x, float p4y);
    void lineto(float x, float y);
    void strokeandfill(Graphics& g, const struct Color& cstroke, const struct Color& cfill);
    void fill(Graphics& g);
    void stroke(Graphics& g);
    void BCurve(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4);

    int x(float x, float y) const { return (int)(x * gctm[0] + y * gctm[3] + gctm[6] + 0.5f); }
    int xsc(float x) const { return (int)(x * gctm[0] + 0.5f); }
    int ysc(float x) const { return (int)(x * gctm[4] + 0.5f); }
    int y(float x, float y) const { return (int)(x * gctm[1] + y * gctm[4] + gctm[7] + 0.5f); }
};
