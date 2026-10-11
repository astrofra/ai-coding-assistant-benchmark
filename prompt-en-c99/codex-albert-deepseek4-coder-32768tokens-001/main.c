/*
 * C99 Mini Ray Tracer
 *
 * Renders two spheres (glass and chrome) resting on an infinite
 * black-and-white checkerboard plane, under classic three-point lighting,
 * with a red-to-orange gradient sky and distance fog.
 *
 * Output: out.tga (1280x720, uncompressed 24-bit true-color TGA).
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct Vec {
    double x, y, z;
} Vec;

static Vec vadd(Vec a, Vec b) { Vec r = {a.x + b.x, a.y + b.y, a.z + b.z}; return r; }
static Vec vsub(Vec a, Vec b) { Vec r = {a.x - b.x, a.y - b.y, a.z - b.z}; return r; }
static Vec vmul(double s, Vec a) { Vec r = {s * a.x, s * a.y, s * a.z}; return r; }
static double vdot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vec vcross(Vec a, Vec b) {
    Vec r = {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    return r;
}
static Vec vnorm(Vec a) {
    double l = sqrt(vdot(a, a));
    if (l < 1e-12) return a;
    return vmul(1.0 / l, a);
}

/* A color is just a vector of linear RGB components. */

typedef struct Ray {
    Vec o, d;   /* origin and normalized direction */
} Ray;

typedef enum MaterialType {
    MAT_CHECKER,
    MAT_GLASS,
    MAT_CHROME
} MaterialType;

typedef struct Hit {
    double t;
    Vec pos;
    Vec n;            /* outward-facing geometric normal */
    MaterialType mat;
    double inside;    /* nonzero when the ray origin was inside the sphere */
} Hit;

typedef struct Sphere {
    Vec c;
    double r;
} Sphere;

/* Scene geometry. */
static const Sphere glass = { { -1.25, 1.0, 0.0 }, 1.0 };
static const Sphere chrome = { { 1.25, 1.0, 0.0 }, 1.0 };

/* Three white point lights: key, fill, rim. */
typedef struct Light {
    Vec pos;
    double strength;
} Light;

static const Light lights[3] = {
    { { -3.5, 6.0, 4.5 }, 1.0 },
    { {  4.0, 3.0, 3.0 }, 0.35 },
    { {  0.5, 5.0, -4.0 }, 0.7 }
};

static const int MAX_DEPTH = 8;
static const double EPS = 1e-4;

/*
 * Intersect the ray with a sphere. Returns the smallest positive t, or a
 * huge value if there is no hit. Handles the ray starting inside the sphere
 * by taking the near root even if it is negative (behind the origin).
 */
static double sphere_intersect(const Sphere *s, Ray r, double *tnear_out,
                               double *tnear_sign_out)
{
    Vec oc = vsub(r.o, s->c);
    double b = vdot(oc, r.d);
    double c = vdot(oc, oc) - s->r * s->r;
    double disc = b * b - c;
    if (disc < 0.0) return 1e30;
    double sq = sqrt(disc);
    double t0 = -b - sq;
    double t1 = -b + sq;
    /* Prefer the near root; if it is positive use it, else use the far root. */
    if (t0 > EPS) {
        if (tnear_out) *tnear_out = t0;
        if (tnear_sign_out) *tnear_sign_out = 1.0;
        return t0;
    }
    if (t1 > EPS) {
        if (tnear_out) *tnear_out = t1;
        /* Origin is inside; normal is flipped later by the caller. */
        if (tnear_sign_out) *tnear_sign_out = -1.0;
        return t1;
    }
    return 1e30;
}

/*
 * Intersect the ray with the infinite ground plane y = 0.
 * Returns t, or 1e30 when parallel / behind.
 */
static double plane_intersect(Ray r, double *tnear_out, double *tnear_sign_out)
{
    if (fabs(r.d.y) < 1e-12) return 1e30;
    double t = -r.o.y / r.d.y;
    if (t > EPS) {
        if (tnear_out) *tnear_out = t;
        if (tnear_sign_out) *tnear_sign_out = 1.0;
        return t;
    }
    return 1e30;
}

/*
 * Find the nearest hit along the ray, filling *hit. Returns 1 on a hit,
 * 0 when the ray misses everything.
 */
static int trace(Ray r, Hit *hit)
{
    double best = 1e30;
    int found = 0;

    double tnear, tsgn;

    tnear = 0.0; tsgn = 1.0;
    double t = sphere_intersect(&glass, r, &tnear, &tsgn);
    if (t < best) {
        best = t;
        hit->t = t;
        hit->mat = MAT_GLASS;
        hit->inside = (tsgn < 0.0);
        found = 1;
    }

    tnear = 0.0; tsgn = 1.0;
    t = sphere_intersect(&chrome, r, &tnear, &tsgn);
    if (t < best) {
        best = t;
        hit->t = t;
        hit->mat = MAT_CHROME;
        hit->inside = (tsgn < 0.0);
        found = 1;
    }

    tnear = 0.0; tsgn = 1.0;
    t = plane_intersect(r, &tnear, &tsgn);
    if (t < best) {
        best = t;
        hit->t = t;
        hit->mat = MAT_CHECKER;
        hit->inside = 0.0;
        found = 1;
    }

    if (!found) return 0;

    hit->pos = vadd(r.o, vmul(hit->t, r.d));
    if (hit->mat == MAT_CHECKER) {
        hit->n = (Vec) { 0.0, 1.0, 0.0 };
    } else if (hit->mat == MAT_GLASS) {
        Vec cn = vnorm(vsub(hit->pos, glass.c));
        /* Invert the normal when the ray is leaving the sphere. */
        hit->n = hit->inside ? vmul(-1.0, cn) : cn;
    } else {
        Vec cn = vnorm(vsub(hit->pos, chrome.c));
        hit->n = hit->inside ? vmul(-1.0, cn) : cn;
    }
    return 1;
}

/*
 * Deterministic checkerboard albedo from world XZ coordinates.
 * Uses floor so the pattern is continuous across the origin.
 */
static Vec checker_albedo(Vec p)
{
    int ix = (int)floor(p.x);
    int iz = (int)floor(p.z);
    int even = ((ix + iz) & 1) == 0;
    if (even) return (Vec) { 1.0, 1.0, 1.0 };
    return (Vec) { 0.0, 0.0, 0.0 };
}

/*
 * Procedural gradient sky evaluated from a normalized direction.
 * Dark red at the zenith/nadir, orange at the horizon.
 */
static Vec sky(Vec d)
{
    /* horizon red-orange */
    Vec horizon = { 1.0, 0.25, 0.03 };
    /* zenith / nadir dark red */
    Vec deep = { 0.18, 0.01, 0.015 };
    double a = fabs(d.y);
    if (a < 0.0) a = 0.0;
    if (a > 1.0) a = 1.0;
    double t = pow(a, 0.65);
    return vadd(vmul(1.0 - t, horizon), vmul(t, deep));
}

/* Schlick's approximation of the Fresnel reflectance. */
static double fresnel(Vec i, Vec n, double n1, double n2)
{
    double cosi = fabs(vdot(i, n));
    double ni = n1, nt = n2;
    if (cosi > 0.0 && n1 > n2) {
        /* Leaving a denser medium: check for total internal reflection. */
        double sin_t2 = (n1 * n1 / (n2 * n2)) * (1.0 - cosi * cosi);
        if (sin_t2 >= 1.0) return 1.0;
        double cos_t = sqrt(1.0 - sin_t2);
        ni = n2; nt = n1;
        cosi = cos_t;
    } else if (n1 > n2) {
        double ratio = n1 / n2;
        double sin_t2 = ratio * ratio * (1.0 - cosi * cosi);
        if (sin_t2 >= 1.0) return 1.0;
        (void)sin_t2;
    }
    double r0 = ((ni - nt) / (ni + nt));
    r0 = r0 * r0;
    double fc = pow(1.0 - cosi, 5.0);
    return r0 + (1.0 - r0) * fc;
}

/* Reflect direction about the normal. */
static Vec reflect_dir(Vec i, Vec n)
{
    return vsub(i, vmul(2.0 * vdot(i, n), n));
}

/*
 * Refract direction. eta = n1 / n2. Returns 1 on success, 0 on total
 * internal reflection.
 */
static int refract_dir(Vec i, Vec n, double eta, Vec *out)
{
    double cosi = vdot(i, n);
    double k = 1.0 - eta * eta * (1.0 - cosi * cosi);
    if (k < 0.0) return 0;   /* total internal reflection */
    *out = vadd(vmul(eta, vsub(i, vmul(cosi, n))), vmul(-sqrt(k), n));
    return 1;
}

/*
 * Trace a shadow ray from pos toward pos + dir, within a max distance.
 * Glass transmits some light; other objects fully block.
 */
static double shadow(Vec pos, Vec lpos)
{
    Vec to = vsub(lpos, pos);
    double dist = sqrt(vdot(to, to));
    if (dist < 1e-6) return 1.0;
    Vec dir = vmul(1.0 / dist, to);
    Ray ray;
    ray.o = vadd(pos, vmul(EPS, dir));
    ray.d = dir;

    Hit h;
    if (!trace(ray, &h)) return 1.0;
    if (h.t < dist) {
        /* Object in the way. Glass only partially attenuates. */
        if (h.mat == MAT_GLASS) return 0.6;
        return 0.0;
    }
    return 1.0;
}

/* Diffuse + specular shading for a non-mirror surface. */
static Vec shade_basic(Ray r, Hit hit, Vec albedo)
{
    Vec color = vmul(0.05, albedo); /* small ambient */
    int i;
    for (i = 0; i < 3; i++) {
        Vec lv = vnorm(vsub(lights[i].pos, hit.pos));
        double ndl = vdot(hit.n, lv);
        if (ndl > 0.0) {
            double vis = shadow(hit.pos, lights[i].pos);
            if (vis > 0.0) {
                color = vadd(color, vmul(lights[i].strength * ndl * vis, albedo));
            }
        }
        /* Blinn-Phong specular highlight. */
        Vec hv = vnorm(vadd(lv, vmul(-1.0, r.d)));
        double ndh = vdot(hit.n, hv);
        if (ndh > 0.0) {
            double spec = pow(ndh, 64.0);
            color = vadd(color, vmul(lights[i].strength * spec * 1.2,
                                     (Vec) { 1.0, 1.0, 1.0 }));
        }
    }
    return color;
}

/* Recursive shading. depth is the remaining bounce budget. */
static Vec shade(Ray r, int depth)
{
    Hit hit;
    if (!trace(r, &hit)) {
        return sky(r.d);
    }

    Vec surface;
    if (hit.mat == MAT_CHECKER) {
        surface = shade_basic(r, hit, checker_albedo(hit.pos));
    } else if (hit.mat == MAT_CHROME) {
        double refl = 0.95;
        Vec base = shade_basic(r, hit, (Vec) { 0.6, 0.6, 0.6 });
        if (depth > 0) {
            Ray rr;
            rr.o = vadd(hit.pos, vmul(EPS, hit.n));
            rr.d = reflect_dir(r.d, hit.n);
            Vec reflected = shade(rr, depth - 1);
            surface = vadd(vmul(1.0 - refl, base), vmul(refl, reflected));
        } else {
            surface = base;
        }
    } else {
        /* Glass: clear dielectric with index 1.5. */
        double n1 = 1.0, n2 = 1.5;
        if (hit.inside) { double tmp = n1; n1 = n2; n2 = tmp; }
        double kr = fresnel(r.d, hit.n, n1, n2);

        /* Reflection ray. */
        Ray rr;
        rr.o = vadd(hit.pos, vmul(hit.inside ? -1.0 * EPS : EPS, hit.n));
        rr.d = reflect_dir(r.d, hit.n);
        Vec refl_col = (depth > 0) ? shade(rr, depth - 1) : sky(rr.d);

        /* Refraction ray. */
        double eta = n1 / n2;
        Vec tdir;
        Vec refr_col;
        if (refract_dir(r.d, hit.n, eta, &tdir)) {
            Ray fr;
            fr.o = vadd(hit.pos, vmul(hit.inside ? EPS : -1.0 * EPS, hit.n));
            fr.d = vnorm(tdir);
            refr_col = (depth > 0) ? shade(fr, depth - 1) : sky(fr.d);
        } else {
            /* Total internal reflection: all energy goes to reflection. */
            kr = 1.0;
            refr_col = (Vec) { 0.0, 0.0, 0.0 };
        }

        surface = vadd(vmul(kr, refl_col), vmul(1.0 - kr, refr_col));
    }

    /* Distance fog blends the shaded surface into the sky at this distance. */
    double trans = exp(-0.035 * hit.t);
    Vec fog = sky(r.d);
    return vadd(vmul(trans, surface), vmul(1.0 - trans, fog));
}

/*
 * Convert a linear RGB color to output bytes: tone map, gamma-encode and clamp.
 */
static void encode(Vec c, unsigned char rgb[3])
{
    double scale = 0.9; /* exposure */
    double lin[3] = { c.x * scale, c.y * scale, c.z * scale };
    unsigned char out[3];
    int i;
    for (i = 0; i < 3; i++) {
        double v = lin[i];
        if (v < 0.0) v = 0.0;
        if (v > 1.0) v = 1.0;
        /* Gamma 2.2 encode. */
        double g = pow(v, 1.0 / 2.2);
        out[i] = (unsigned char)(g * 255.0 + 0.5);
    }
    rgb[0] = out[0];
    rgb[1] = out[1];
    rgb[2] = out[2];
}

static int write_tga(FILE *f, const unsigned char *pixels, int w, int h)
{
    /* 18-byte uncompressed true-color TGA header. */
    unsigned char header[18] = { 0 };
    header[2] = 2;                       /* uncompressed true-color */
    header[12] = (unsigned char)(w & 0xff);
    header[13] = (unsigned char)((w >> 8) & 0xff);
    header[14] = (unsigned char)(h & 0xff);
    header[15] = (unsigned char)((h >> 8) & 0xff);
    header[16] = 24;                     /* 24 bits per pixel */
    header[17] = 0x20;                   /* top-left origin */
    if (fwrite(header, 1, 18, f) != 18) return 0;
    if (fwrite(pixels, 1, (size_t)w * h * 3, f) != (size_t)w * h * 3) return 0;
    return 1;
}

int main(void)
{
    const int W = 1280, H = 720;
    const int SS = 2;   /* 2 x 2 supersampling => 4 samples per pixel */
    const double fov_y = 45.0 * M_PI / 180.0;
    const double aspect = (double)W / (double)H;

    Vec cam = { 0.0, 2.6, 6.5 };
    Vec look = { 0.0, 1.0, 0.0 };
    Vec up = { 0.0, 1.0, 0.0 };

    Vec fwd = vnorm(vsub(look, cam));      /* direction the camera looks */
    Vec right = vnorm(vcross(fwd, up));
    Vec upv = vcross(right, fwd);          /* perpendicular, points up-image */

    double tan_h = tan(fov_y * 0.5);

    unsigned char *pixels = (unsigned char *)malloc((size_t)W * H * 3);
    if (!pixels) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    int y, x;
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            Vec acc = { 0.0, 0.0, 0.0 };
            int sy, sx;
            for (sy = 0; sy < SS; sy++) {
                for (sx = 0; sx < SS; sx++) {
                    /* Deterministic subpixel center in [0,1). */
                    double su = ((double)x + (sx + 0.5) / (double)SS) + 0.5;
                    double sv = ((double)y + (sy + 0.5) / (double)SS) + 0.5;
                    double px = (2.0 * su / (double)W - 1.0) * tan_h * aspect;
                    double py = (1.0 - 2.0 * sv / (double)H) * tan_h;
                    Vec dir = vnorm(vadd(vadd(fwd, vmul(px, right)), vmul(py, upv)));
                    Ray r;
                    r.o = cam;
                    r.d = dir;
                    Vec c = shade(r, MAX_DEPTH);
                    acc = vadd(acc, c);
                }
            }
            acc = vmul(1.0 / (double)(SS * SS), acc);
            unsigned char rgb[3];
            encode(acc, rgb);
            size_t idx = ((size_t)y * W + x) * 3;
            pixels[idx + 0] = rgb[2]; /* B */
            pixels[idx + 1] = rgb[1]; /* G */
            pixels[idx + 2] = rgb[0]; /* R */
        }
    }

    FILE *f = fopen("out.tga", "wb");
    if (!f) {
        fprintf(stderr, "error: cannot open out.tga for writing\n");
        free(pixels);
        return 1;
    }
    int ok = write_tga(f, pixels, W, H);
    if (fclose(f) != 0) ok = 0;
    free(pixels);

    if (!ok) {
        fprintf(stderr, "error: failed to write out.tga\n");
        return 1;
    }
    return 0;
}
