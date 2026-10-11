// Counterpart of source/cfr/Ctm.java. Unused recovered methods are retained.
#pragma once
#include "host.hpp"
#include <array>
#include <vector>
class Ctm {
public:
    typedef std::array<float, 9> Matrix;
    Matrix gctm = {};
    int cp = 0;
    const int N = 16;
    std::array<Matrix, 16> ctms = {};
    int penx = 0, peny = 0;
    int ONEHALF = 32768;
    std::vector<int> xpts = std::vector<int>(256), ypts = std::vector<int>(256);
    int growchunk = 256, npts = 0;
    host::Graphics* gr = nullptr;
    void MultMat3x3(Matrix& a, Matrix& b, Matrix& dest);
    void identityctm(Matrix& a);
    void CopyMat3x3(Matrix& a, Matrix& b);
    void MakeTranslationMatrix(Matrix& a, float dx, float dy);
    void MakeScaleMatrix(Matrix& a, float sx, float sy);
    void MakeRotationMatrix(Matrix& a, float rtheta);
    void MakeSkewMatrix(Matrix& a, float rtheta, float rtheta2);
    void rotctm(Matrix& a, float theta);
    void skewctm(Matrix& a, float theta, float theta2);
    void transctm(Matrix& a, float dx, float dy);
    void scalectm(Matrix& a, float sx, float sy);
    void init();
    Ctm();
    void push();
    void pop();
    void rotate(float theta);
    void skew(float theta, float theta2);
    void translate(float dx, float dy);
    void scale(float sx, float sy);
    void moveto(float x, float y);
    void addpt(int x, int y);
    void curveto(float p2x, float p2y, float p3x, float p3y, float p4x, float p4y);
    void lineto(float x, float y);
    void strokeandfill(host::Graphics& g, host::Color cstroke, host::Color cfill);
    void fill(host::Graphics& g);
    void stroke(host::Graphics& g);
    void BCurve(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4);
    int x(float x, float y);
    int xsc(float x);
    int ysc(float x);
    int y(float x, float y);
};
