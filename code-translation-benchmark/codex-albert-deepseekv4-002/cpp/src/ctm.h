/*
 * Port of the Java Ctm: affine transform + fixed-point Bezier path builder.
 * Preserves the transform-stack quirks (push copies twice, pop decrements
 * before the bounds check) and the 16.16 BCurve subdivision literally.
 */
#ifndef FLORA_CTM_H
#define FLORA_CTM_H

#include <cstddef>

namespace flora {

class Surface;
struct Color;

class Ctm {
public:
    Ctm();

    void push();
    void pop();
    void rotate(float theta);
    void skew(float theta, float theta2);
    void translate(float dx, float dy);
    void scale(float sx, float sy);
    void moveto(float x, float y);
    void lineto(float x, float y);
    void curveto(float p2x, float p2y, float p3x, float p3y, float p4x, float p4y);
    void fill(Surface* surf, const Color& col);

    float gctm[9];

private:
    static const int NINETY = 16;
    float ctms_[16][9];
    int cp_ = 0;
    int penx_ = 0;
    int peny_ = 0;
    static const int ONEHALF = 32768;
    static const int GROWCHUNK = 256;
    int xpts_[256];
    int ypts_[256];
    int cap_ = 256;
    int npts_ = 0;

    static const int FLAT = 10;

    void multMat3x3(const float* a, const float* b, float* dest);
    void identityctm(float* a);
    void copyMat3x3(const float* a, float* b);
    void makeTranslationMatrix(float* a, float dx, float dy);
    void makeScaleMatrix(float* a, float sx, float sy);
    void makeRotationMatrix(float* a, float rtheta);
    void makeSkewMatrix(float* a, float rtheta, float rtheta2);
    void rotctm(float* a, float theta);
    void skewctm(float* a, float theta, float theta2);
    void transctm(float* a, float dx, float dy);
    void scalectm(float* a, float sx, float sy);
    void addpt(int x, int y);
    void bcurve(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4);
    int x(float x, float y);
    int y(float x, float y);
};

}  // namespace flora

#endif  // FLORA_CTM_H
