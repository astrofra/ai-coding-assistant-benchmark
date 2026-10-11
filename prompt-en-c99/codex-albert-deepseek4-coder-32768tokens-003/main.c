/*
 * C99 mini ray tracer.
 *
 * Renders two spheres (glass and chrome) on an infinite checkerboard
 * plane with three-point lighting, a red-to-orange gradient sky, and
 * distance fog.  Output is a 1280x720 24-bit TGA: out.tga.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Small vector math (3D)                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    double x, y, z;
} Vec;

static Vec vadd(Vec a, Vec b) { Vec r = {a.x+b.x, a.y+b.y, a.z+b.z}; return r; }
static Vec vsub(Vec a, Vec b) { Vec r = {a.x-b.x, a.y-b.y, a.z-b.z}; return r; }
static Vec vmul(Vec a, double s) { Vec r = {a.x*s, a.y*s, a.z*s}; return r; }
static Vec vneg(Vec a) { Vec r = {-a.x, -a.y, -a.z}; return r; }
static double vdot(Vec a, Vec b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static Vec vcross(Vec a, Vec b) {
    Vec r = {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
    return r;
}
static double vlen(Vec a) { return sqrt(vdot(a, a)); }
static Vec vnorm(Vec a) {
    double l = vlen(a);
    if (l > 0.0) {
        double inv = 1.0 / l;
        Vec r = {a.x*inv, a.y*inv, a.z*inv};
        return r;
    }
    Vec r = {0.0, 0.0, 0.0}; return r;
}

/* ------------------------------------------------------------------ */
/* Scene / ray helpers                                                 */
/* ------------------------------------------------------------------ */

#define EPSILON 1e-4

typedef struct {
    Vec origin;
    Vec dir; /* always normalized */
} Ray;

typedef enum {
    MAT_GLASS,   /* clear dielectric, IOR 1.5            */
    MAT_CHROME,  /* polished metal, strong reflection    */
    MAT_PLANE    /* infinite checkerboard                */
} MatKind;

typedef struct {
    MatKind kind;
    Vec center;   /* spheres */
    double radius;
    double ior;   /* glass */
    double refl;  /* chrome reflection weight */
} Material;

typedef struct {
    double t;      /* closest positive hit distance       */
    Vec point;     /* world-space hit point               */
    Vec normal;    /* outward-facing surface normal       */
    int front;     /* 1 if ray hit the front (outer) face */
    Material mat;
} Hit;

static Material make_glass(void) {
    Material m;
    m.kind = MAT_GLASS;
    m.center.x = -1.25; m.center.y = 1.0; m.center.z = 0.0;
    m.radius = 1.0;
    m.ior = 1.5;
    m.refl = 0.0;
    return m;
}

static Material make_chrome(void) {
    Material m;
    m.kind = MAT_CHROME;
    m.center.x = 1.25; m.center.y = 1.0; m.center.z = 0.0;
    m.radius = 1.0;
    m.ior = 1.0;
    m.refl = 0.95;
    return m;
}

static Material make_plane(void) {
    Material m;
    m.kind = MAT_PLANE;
    m.center.x = 0.0; m.center.y = 0.0; m.center.z = 0.0;
    m.radius = 0.0;
    m.ior = 1.0;
    m.refl = 0.0;
    return m;
}

/* Ray-sphere intersection; both roots are considered so rays starting
 * inside the sphere (as refracted rays do) still hit the exit surface. */
static int intersect_sphere(const Ray *ray, const Material *m, Hit *hit) {
    Vec oc = vsub(ray->origin, m->center);
    double b = vdot(oc, ray->dir);
    double c = vdot(oc, oc) - m->radius * m->radius;
    double disc = b * b - c;
    if (disc < 0.0) return 0;

    double sq = sqrt(disc);
    double t = -b - sq;               /* near root */
    int front = 1;
    if (t < EPSILON) {
        t = -b + sq;                  /* far root (inside sphere) */
        front = 0;
        if (t < EPSILON) return 0;
    }

    hit->t = t;
    hit->point = vadd(ray->origin, vmul(ray->dir, t));
    Vec n = vnorm(vsub(hit->point, m->center));
    hit->normal = front ? n : vneg(n);
    hit->front = front;
    hit->mat = *m;
    return 1;
}

/* Infinite plane at y = 0. Returns 0 if the ray is parallel. */
static int intersect_plane(const Ray *ray, const Material *m, Hit *hit) {
    if (fabs(ray->dir.y) < 1e-12) return 0;
    double t = -ray->origin.y / ray->dir.y;
    if (t < EPSILON) return 0;
    hit->t = t;
    hit->point.x = ray->origin.x + ray->dir.x * t;
    hit->point.y = 0.0;
    hit->point.z = ray->origin.z + ray->dir.z * t;
    hit->normal.x = 0.0; hit->normal.y = (ray->dir.y > 0.0 ? -1.0 : 1.0);
    hit->normal.z = 0.0;
    hit->front = 1;
    hit->mat = *m;
    return 1;
}

/* Return the nearest intersection, or 0 if the ray misses everything. */
static int trace(const Ray *ray, Hit *hit) {
    Hit h;
    int found = 0;
    double best = INFINITY;

    Material glass = make_glass();
    Material chrome = make_chrome();
    Material plane = make_plane();

    if (intersect_sphere(ray, &glass, &h)) {
        if (h.t < best) { best = h.t; *hit = h; found = 1; }
    }
    if (intersect_sphere(ray, &chrome, &h)) {
        if (h.t < best) { best = h.t; *hit = h; found = 1; }
    }
    if (intersect_plane(ray, &plane, &h)) {
        if (h.t < best) { best = h.t; *hit = h; found = 1; }
    }
    return found;
}

/* ------------------------------------------------------------------ */
/* Sky, checkerboard, lighting, fog                                    */
/* ------------------------------------------------------------------ */

/* Procedural gradient sky from a normalized direction. */
static Vec sky(const Vec d) {
    Vec dark = {0.18, 0.01, 0.015};
    Vec hori = {1.0, 0.25, 0.03};
    double interp = fabs(d.y);
    if (interp > 1.0) interp = 1.0;
    double t = pow(interp, 0.65);
    return vadd(vmul(hori, 1.0 - t), vmul(dark, t));
}

/* Checkerboard diffuse albedo, continuous across the origin. */
static Vec checker(const Vec p) {
    int ix = (int)floor(p.x);
    int iz = (int)floor(p.z);
    int color = (ix + iz) & 1;
    if (color == 0) { Vec c = {0.0, 0.0, 0.0}; return c; }
    Vec c = {1.0, 1.0, 1.0}; return c;
}

/* Three white point lights.  Strengths are used without distance
 * attenuation, as the specification requests. */
typedef struct {
    Vec pos;
    double intensity;
} Light;

static Light lighting[3];
static int lighting_ready = 0;

static void init_lights(void) {
    if (lighting_ready) return;
    lighting[0].pos.x = -3.5; lighting[0].pos.y = 6.0; lighting[0].pos.z = 4.5;
    lighting[0].intensity = 1.0;
    lighting[1].pos.x = 4.0;  lighting[1].pos.y = 3.0; lighting[1].pos.z = 3.0;
    lighting[1].intensity = 0.35;
    lighting[2].pos.x = 0.5;  lighting[2].pos.y = 5.0; lighting[2].pos.z = -4.0;
    lighting[2].intensity = 0.7;
    lighting_ready = 1;
}

/* Cast a shadow ray from point toward light until the light's distance.
 * Returns how much light is transmitted on [0,1]. Chrome and the plane
 * block fully; glass attenuates partially to keep refracted light alive. */
static double shadow(const Vec point, const Vec lightpos) {
    Vec to_light = vsub(lightpos, point);
    double dist = vlen(to_light);
    if (dist < 1e-6) return 1.0;
    Vec dir = vmul(to_light, 1.0 / dist);

    Ray r;
    r.origin = vadd(point, vmul(dir, EPSILON));
    r.dir = dir;

    Hit h;
    if (intersect_sphere(&r, &(Material){.kind=MAT_GLASS, .center.x=-1.25,
                                         .center.y=1.0, .center.z=0.0,
                                         .radius=1.0, .ior=1.5, .refl=0.0}, &h)) {
        if (h.t < dist) return 0.8; /* glass transmits most shadow light */
    }
    if (intersect_sphere(&r, &(Material){.kind=MAT_CHROME, .center.x=1.25,
                                         .center.y=1.0, .center.z=0.0,
                                         .radius=1.0, .ior=1.0, .refl=0.95}, &h)) {
        if (h.t < dist) return 0.0;
    }
    /* The ground plane and all opaque matter simply block. */
    if (intersect_plane(&r, &(Material){.kind=MAT_PLANE}, &h)) {
        if (h.t < dist) return 0.0;
    }
    return 1.0;
}

/* ------------------------------------------------------------------ */
/* Recursive ray tracing                                               */
/* ------------------------------------------------------------------ */

static Vec shade(const Ray *ray, int depth);

static Vec background(const Ray *ray) {
    return sky(ray->dir);
}

/* Distance fog blending a shaded segment into the sky along the ray. */
static Vec fog_apply(double dist, Vec shaded, const Ray *ray) {
    double trans = exp(-0.035 * dist);
    Vec fogcolor = sky(ray->dir);
    Vec r = vadd(vmul(shaded, trans), vmul(fogcolor, 1.0 - trans));
    return r;
}

/* Schlick's approximation for dielectric Fresnel reflectance. */
static double fresnel(double cos_i, double ior_in, double ior_out) {
    double r0 = (ior_in - ior_out) / (ior_in + ior_out);
    r0 *= r0;
    double c = 1.0 - cos_i;
    return r0 + (1.0 - r0) * c * c * c * c * c;
}

static Vec shade(const Ray *ray, int depth) {
    Hit hit;
    if (!trace(ray, &hit)) {
        return background(ray);
    }

    Vec shaded = {0.0, 0.0, 0.0};

    if (hit.mat.kind == MAT_PLANE) {
        /* Diffuse checker with a weak specular sheen from lights. */
        Vec albedo = checker(hit.point);
        Vec diffuse = {0.0, 0.0, 0.0};
        init_lights();
        /* The three relative strengths sum to 2.05; normalize by that so a
         * white tile facing the lights reaches a natural peak instead of
         * clipping to pure white.  This keeps the relative light balance. */
        double light_total = lighting[0].intensity +
                             lighting[1].intensity +
                             lighting[2].intensity;
        for (int i = 0; i < 3; i++) {
            Vec L = vnorm(vsub(lighting[i].pos, hit.point));
            double ndl = vdot(hit.normal, L);
            if (ndl < 0.0) continue;
            double trans = shadow(hit.point, lighting[i].pos);
            double lam = ndl * (lighting[i].intensity / light_total) * trans;
            diffuse = vadd(diffuse, vmul(albedo, lam));
        }
        double ambient = 0.05;
        diffuse = vadd(diffuse, vmul(albedo, ambient));
        shaded = diffuse;

        /* Very small mirror component keeps reflections subtle on ground. */
        if (depth > 0) {
            Vec R = vsub(ray->dir, vmul(vmul(hit.normal, 2.0 * vdot(ray->dir, hit.normal)), 1.0));
            Ray rr;
            rr.origin = vadd(hit.point, vmul(hit.normal, EPSILON));
            rr.dir = R;
            Vec refl = shade(&rr, depth - 1);
            shaded = vadd(shaded, vmul(refl, 0.05));
        }
    } else if (hit.mat.kind == MAT_CHROME) {
        /* Strong mirror reflections + analytic spec highlights. */
        Vec total = {0.0, 0.0, 0.0};
        init_lights();
        for (int i = 0; i < 3; i++) {
            Vec L = vnorm(vsub(lighting[i].pos, hit.point));
            Vec V = vneg(ray->dir);
            Vec H = vnorm(vadd(L, V));
            double ndl = vdot(hit.normal, L);
            if (ndl < 0.0) continue;
            double trans = shadow(hit.point, lighting[i].pos);
            double spec = pow(fmax(vdot(hit.normal, H), 0.0), 64.0);
            Vec c = {1.0, 1.0, 1.0};
            double amount = spec * 1.0 * lighting[i].intensity * trans;
            total = vadd(total, vmul(c, amount));
        }
        /* Dim base so reflections dominate and it reads as metal. */
        Vec base = {0.02, 0.02, 0.02};
        total = vadd(total, base);

        Vec refl = {0.0, 0.0, 0.0};
        if (depth > 0) {
            Vec R = vsub(ray->dir, vmul(vmul(hit.normal, 2.0 * vdot(ray->dir, hit.normal)), 1.0));
            Ray rr;
            rr.origin = vadd(hit.point, vmul(hit.normal, EPSILON));
            rr.dir = R;
            refl = shade(&rr, depth - 1);
        }
        shaded = vadd(vmul(total, 1.0 - hit.mat.refl), vmul(refl, hit.mat.refl));
    } else { /* MAT_GLASS */
        Vec n = hit.normal;
        Vec I = vneg(ray->dir);
        double cos_i = fmax(vdot(I, n), 0.0);

        /* Index ratio depends on entering or exiting. */
        double ior_in, ior_out;
        if (hit.front) {
            ior_in = 1.0;   /* air */
            ior_out = hit.mat.ior;
        } else {
            ior_in = hit.mat.ior;
            ior_out = 1.0;
        }
        double eta = ior_in / ior_out;

        double kr = fresnel(cos_i, ior_in, ior_out);
        if (hit.front == 0) kr = 1.0 - kr; /* fresnel handled at entry */

        Vec refl = {0.0, 0.0, 0.0};
        Vec refr = {0.0, 0.0, 0.0};

        /* Reflection direction. */
        Vec R = vsub(ray->dir, vmul(vmul(n, 2.0 * vdot(ray->dir, n)), 1.0));
        if (depth > 0) {
            Ray rr;
            rr.origin = vadd(hit.point, vmul(n, EPSILON));
            rr.dir = R;
            refl = shade(&rr, depth - 1);
        }

        /* Snell's law; if it fails we have total internal reflection. */
        Vec lt = {0.0, 0.0, 0.0};
        Vec perp = vsub(I, vmul(n, vdot(I, n)));
        double k = 1.0 - eta * eta * (1.0 - vdot(I, n) * vdot(I, n));
        int tir = 0;
        if (k < 0.0) {
            tir = 1; /* total internal reflection */
        } else {
            lt = vadd(vmul(perp, eta), vmul(n, -(eta * vdot(I, n) - sqrt(k))));
            lt = vnorm(lt);
            if (depth > 0) {
                Ray rt;
                rt.origin = vadd(hit.point, vmul(vneg(lt), EPSILON));
                rt.dir = lt;
                refr = shade(&rt, depth - 1);
            }
        }

        if (tir) {
            shaded = refl; /* all energy reflected */
        } else {
            shaded = vadd(vmul(refl, kr), vmul(refr, 1.0 - kr));
        }

        /* Specular highlight from lights adds life to the glass. */
        init_lights();
        for (int i = 0; i < 3; i++) {
            Vec L = vnorm(vsub(lighting[i].pos, hit.point));
            Vec V = vneg(ray->dir);
            Vec H = vnorm(vadd(L, V));
            double ndl = vdot(hit.normal, L);
            if (ndl < 0.0) continue;
            double trans = shadow(hit.point, lighting[i].pos);
            double spec = pow(fmax(vdot(hit.normal, H), 0.0), 128.0);
            Vec c = {1.0, 1.0, 1.0};
            double amount = spec * 1.0 * lighting[i].intensity * trans * (1.0 - kr);
            shaded = vadd(shaded, vmul(c, amount));
        }
    }

    return fog_apply(hit.t, shaded, ray);
}

/* ------------------------------------------------------------------ */
/* Tone mapping + TGA output                                           */
/* ------------------------------------------------------------------ */

static unsigned char clamp8(double v) {
    if (v < 0.0) v = 0.0;
    else if (v > 1.0) v = 1.0;
    /* Gamma 2.2 encode. */
    v = pow(v, 1.0 / 2.2);
    return (unsigned char)(v * 255.0 + 0.5);
}

static int write_tga(const char *path, const unsigned char *rgb, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "error: could not open %s for writing\n", path);
        return 0;
    }

    /* 18-byte TGA header, written explicitly to avoid struct/endian issues. */
    unsigned char header[18];
    for (int i = 0; i < 18; i++) header[i] = 0;
    header[2] = 2;              /* uncompressed true-color */
    header[12] = (unsigned char)(w & 0xff);
    header[13] = (unsigned char)((w >> 8) & 0xff);
    header[14] = (unsigned char)(h & 0xff);
    header[15] = (unsigned char)((h >> 8) & 0xff);
    header[16] = 24;            /* pixel depth, no alpha */
    header[17] = 0x20;          /* top-left origin */

    if (fwrite(header, 1, 18, f) != 18) {
        fprintf(stderr, "error: failed writing TGA header\n");
        fclose(f);
        return 0;
    }

    /* Pixels are already in B,G,R order, top-to-bottom. */
    if (fwrite(rgb, 1, (size_t)w * (size_t)h * 3u, f) !=
            (size_t)w * (size_t)h * 3u) {
        fprintf(stderr, "error: failed writing TGA pixels\n");
        fclose(f);
        return 0;
    }

    if (fclose(f) != 0) {
        fprintf(stderr, "error: failed closing %s\n", path);
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Camera + main loop                                                  */
/* ------------------------------------------------------------------ */

int main(void) {
    const int width = 1280;
    const int height = 720;
    const int spp = 2; /* 2x2 = 4 samples per pixel */

    Vec cam_pos = {0.0, 2.6, 6.5};
    Vec cam_look = {0.0, 1.0, 0.0};
    Vec cam_up = {0.0, 1.0, 0.0};

    double fov_y = 45.0 * (M_PI / 180.0);
    double tan_half = tan(fov_y * 0.5);
    double aspect = (double)width / (double)height;

    Vec forward = vnorm(vsub(cam_look, cam_pos));
    Vec rightv = vnorm(vcross(forward, cam_up));
    Vec upv = vcross(rightv, forward);

    /* Build the horizontal image plane spanning [-1,1] across the frame. */
    size_t total = (size_t)width * (size_t)height;
    unsigned char *rgb = (unsigned char *)malloc(total * 3u);
    if (!rgb) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    init_lights();

    int ok = 1;
    for (int y = 0; y < height && ok; y++) {
        for (int x = 0; x < width; x++) {
            Vec acc = {0.0, 0.0, 0.0};

            /* Deterministic 2x2 supersampling grid. */
            for (int sy = 0; sy < spp; sy++) {
                for (int sx = 0; sx < spp; sx++) {
                    /* Subpixel offset within the pixel, in [-0.5, 0.5]. */
                    double u = ((double)sx + 0.5) / (double)spp - 0.5;
                    double v = ((double)sy + 0.5) / (double)spp - 0.5;

                    /* NDC position of the subpixel center. */
                    double ndc_x = (2.0 * ((double)x + 0.5 + u) /
                                    (double)width) - 1.0;
                    double ndc_y = 1.0 - (2.0 * ((double)y + 0.5 + v) /
                                    (double)height);

                    double px = ndc_x * tan_half * aspect;
                    double py = ndc_y * tan_half;

                    Vec dir = vadd(vmul(rightv, px),
                                   vadd(vmul(upv, py), forward));
                    dir = vnorm(dir);

                    Ray r;
                    r.origin = cam_pos;
                    r.dir = dir;
                    Vec col = shade(&r, 8);
                    acc = vadd(acc, col);
                }
            }
            acc = vmul(acc, 1.0 / (double)(spp * spp));

            /* Store B,G,R for the TGA. */
            size_t idx = ((size_t)y * (size_t)width + (size_t)x) * 3u;
            rgb[idx + 0] = clamp8(acc.z);
            rgb[idx + 1] = clamp8(acc.y);
            rgb[idx + 2] = clamp8(acc.x);
        }
        if ((y % 120) == 0) {
            fprintf(stderr, "rendering row %d / %d\n", y, height);
        }
    }

    if (ok) {
        ok = write_tga("out.tga", rgb, width, height);
    }

    free(rgb);

    if (!ok) {
        fprintf(stderr, "error: rendering or write failed\n");
        return 1;
    }

    fprintf(stderr, "done: wrote out.tga (%dx%d)\n", width, height);
    return 0;
}
