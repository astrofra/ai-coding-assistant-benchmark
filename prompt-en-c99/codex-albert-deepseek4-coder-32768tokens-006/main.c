#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct { double x, y, z; } Vec;

static double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

static Vec vadd(Vec a, Vec b) { Vec r = { a.x + b.x, a.y + b.y, a.z + b.z }; return r; }
static Vec vsub(Vec a, Vec b) { Vec r = { a.x - b.x, a.y - b.y, a.z - b.z }; return r; }
static Vec vmul(Vec v, double s) { Vec r = { v.x * s, v.y * s, v.z * s }; return r; }
static double vdot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vec vcross(Vec a, Vec b) {
    Vec r = { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    return r;
}
static Vec vnorm(Vec v) {
    double len = sqrt(vdot(v, v));
    if (len < 1e-12) { Vec z = { 0, 0, 0 }; return z; }
    return vmul(v, 1.0 / len);
}

typedef struct {
    Vec center;
    double radius;
    int is_glass;
    double refl;      /* reflection weight for chrome */
    Vec albedo;       /* diffuse albedo                  */
} Sphere;

/* Camera frame */
static Vec cam_pos, cam_fwd, cam_right, cam_up;
static double half_w, half_h;

static void build_camera(void) {
    cam_pos.x = 0.0; cam_pos.y = 2.6; cam_pos.z = 6.5;
    Vec look = { 0.0, 1.0, 0.0 };
    Vec up   = { 0.0, 1.0, 0.0 };
    cam_fwd = vnorm(vsub(look, cam_pos));
    cam_right = vnorm(vcross(cam_fwd, up));
    cam_up = vcross(cam_right, cam_fwd);

    double vfov = 45.0 * 3.14159265358979323846 / 180.0;
    double aspect = 1280.0 / 720.0;
    half_h = tan(vfov * 0.5);
    half_w = half_h * aspect;
}

/* Gradient sky evaluated from a normalized direction. */
static Vec sky(Vec d) {
    double t = pow(fabs(clamp01(d.y)), 0.65);
    Vec horizon = { 1.0, 0.25, 0.03 };
    Vec zenith  = { 0.18, 0.01, 0.015 };
    Vec r;
    r.x = horizon.x * (1.0 - t) + zenith.x * t;
    r.y = horizon.y * (1.0 - t) + zenith.y * t;
    r.z = horizon.z * (1.0 - t) + zenith.z * t;
    return r;
}

/* Returns true and sets t to the nearest positive hit if a ray intersects. */
static int hit_plane(Vec ro, Vec rd, double *t) {
    if (fabs(rd.y) < 1e-9) return 0;
    double tt = -ro.y / rd.y;
    if (tt <= 1e-4) return 0;
    *t = tt;
    return 1;
}

static int hit_sphere(Vec ro, Vec rd, Vec c, double r, double *t) {
    Vec oc = vsub(ro, c);
    double b = vdot(oc, rd);
    double cc = vdot(oc, oc) - r * r;
    double disc = b * b - cc;
    if (disc < 0.0) return 0;
    double sq = sqrt(disc);
    double t0 = -b - sq;
    double t1 = -b + sq;
    double best = -1.0;
    /* Consider both roots so rays starting inside a sphere still exit. */
    if (t0 > 1e-4 && (best < 0.0 || t0 < best)) best = t0;
    if (t1 > 1e-4 && (best < 0.0 || t1 < best)) best = t1;
    if (best < 0.0) return 0;
    *t = best;
    return 1;
}

#define MAX_BOUNCES 8u

typedef struct {
    double t;
    Vec pos, n;
    int is_plane;
    Sphere sphere;   /* filled when is_plane == 0        */
    int hit_inside;  /* ray started inside this sphere    */
} Hit;

/* Find nearest valid intersection among ground + both spheres. */
static int trace(Vec ro, Vec rd, Hit *h) {
    double t_plane = -1.0, t_glass = -1.0, t_chrome = -1.0;
    hit_plane(ro, rd, &t_plane);
    hit_sphere(ro, rd, (Vec){-1.25, 1.0, 0.0}, 1.0, &t_glass);
    hit_sphere(ro, rd, (Vec){ 1.25, 1.0, 0.0}, 1.0, &t_chrome);

    double best = -1.0;
    int kind = -1;
    if (t_plane > 0.0 && (best < 0.0 || t_plane < best)) { best = t_plane; kind = 0; }
    if (t_glass > 0.0 && (best < 0.0 || t_glass < best)) { best = t_glass; kind = 1; }
    if (t_chrome > 0.0 && (best < 0.0 || t_chrome < best)) { best = t_chrome; kind = 2; }

    if (kind < 0) return 0;

    h->t = best;
    h->pos = vadd(ro, vmul(rd, best));
    if (kind == 0) {
        h->is_plane = 1;
        h->n = (Vec){ 0.0, 1.0, 0.0 };
    } else {
        Sphere s;
        if (kind == 1) {
            s.center = (Vec){ -1.25, 1.0, 0.0 };
            s.radius = 1.0;
            s.is_glass = 1;
            s.refl = 0.0;
            s.albedo = (Vec){ 1.0, 1.0, 1.0 };
        } else {
            s.center = (Vec){ 1.25, 1.0, 0.0 };
            s.radius = 1.0;
            s.is_glass = 0;
            s.refl = 0.95;
            s.albedo = (Vec){ 0.02, 0.02, 0.02 };
        }
        h->is_plane = 0;
        h->sphere = s;
        h->n = vnorm(vsub(h->pos, s.center));

        /* If the surface normal faces away from the ray, we entered from the
           inside (e.g. refracted ray inside glass); flip the normal so shading
           is done on the correct side. */
        if (vdot(h->n, rd) > 0.0) {
            h->n = vmul(h->n, -1.0);
            h->hit_inside = 1;
        } else {
            h->hit_inside = 0;
        }
    }
    return 1;
}

static Vec checker(Vec pos) {
    int ix = (int)floor(pos.x);
    int iz = (int)floor(pos.z);
    int even = ((ix + iz) & 1) == 0;
    if (even) { Vec w = { 1.0, 1.0, 1.0 }; return w; }
    Vec b = { 0.0, 0.0, 0.0 };
    return b;
}

/* Three-point lighting */
static Vec key  = { -3.5, 6.0, 4.5 };
static Vec fill = {  4.0, 3.0, 3.0 };
static Vec rim  = {  0.5, 5.0, -4.0 };

static double fresnel_schlick(double cosi, double n1, double n2) {
    double r0 = (n1 - n2) / (n1 + n2);
    r0 *= r0;
    double c = 1.0 - fmax(0.0, cosi);
    return r0 + (1.0 - r0) * c * c * c * c * c;
}

/* Reflect incoming ray about the surface normal (both normalized). */
static Vec reflect_vec(Vec rd, Vec n) {
    return vnorm(vsub(rd, vmul(n, 2.0 * vdot(rd, n))));
}

/* Returns 1 with the direction in *t when refraction happens,
   returns 0 on total internal reflection. n is the surface normal
   pointing against the incoming ray direction. */
static int refract(Vec n, Vec rd, double n1, double n2, Vec *t) {
    double cosi = fmax(-1.0, fmin(1.0, vdot(n, rd)));
    double eta = n1 / n2;
    double k = 1.0 - eta * eta * (1.0 - cosi * cosi);
    if (k < 0.0) return 0;
    Vec r;
    r.x = eta * (rd.x - cosi * n.x) - n.x * sqrt(k);
    r.y = eta * (rd.y - cosi * n.y) - n.y * sqrt(k);
    r.z = eta * (rd.z - cosi * n.z) - n.z * sqrt(k);
    *t = vnorm(r);
    return 1;
}

static Vec shade(Hit h, Vec view_dir, unsigned depth);

static int light_hit(Vec pos, Vec ldir, double dist) {
    /* Non-trivial tracing helper: does anything opaque block between pos
       (offset toward light) and the light at distance dist? */
    Vec ro = vadd(pos, vmul(ldir, 1e-4));
    Hit h;
    if (trace(ro, ldir, &h) && h.t > 1e-4 && h.t < dist) {
        if (!h.is_plane && h.sphere.is_glass) return 0; /* glass is transmissive */
        return 1;
    }
    return 0;
}

static Vec shade_plane(Vec pos, Vec n, Vec view_dir, unsigned depth) {
    Vec albedo = checker(pos);
    Vec lit = { 0.0, 0.0, 0.0 };
    Vec acc = { 0.0, 0.0, 0.0 };
    Vec l, hh;
    double ndotl, spec;

    l = vnorm(vsub(key, pos));
    if (!light_hit(pos, l, sqrt(vdot(vsub(key, pos), vsub(key, pos))))) {
        ndotl = fmax(0.0, vdot(n, l));
        hh = vnorm(vadd(l, view_dir));
        spec = pow(fmax(0.0, vdot(n, hh)), 16.0) * 0.05;
        acc.x += albedo.x * ndotl * 1.0 + spec;
        acc.y += albedo.y * ndotl * 1.0 + spec;
        acc.z += albedo.z * ndotl * 1.0 + spec;
    }
    l = vnorm(vsub(fill, pos));
    if (!light_hit(pos, l, sqrt(vdot(vsub(fill, pos), vsub(fill, pos))))) {
        ndotl = fmax(0.0, vdot(n, l));
        hh = vnorm(vadd(l, view_dir));
        spec = pow(fmax(0.0, vdot(n, hh)), 16.0) * 0.05;
        acc.x += albedo.x * ndotl * 0.35 + spec * 0.35;
        acc.y += albedo.y * ndotl * 0.35 + spec * 0.35;
        acc.z += albedo.z * ndotl * 0.35 + spec * 0.35;
    }
    l = vnorm(vsub(rim, pos));
    if (!light_hit(pos, l, sqrt(vdot(vsub(rim, pos), vsub(rim, pos))))) {
        ndotl = fmax(0.0, vdot(n, l));
        hh = vnorm(vadd(l, view_dir));
        spec = pow(fmax(0.0, vdot(n, hh)), 16.0) * 0.05;
        acc.x += albedo.x * ndotl * 0.7 + spec * 0.7;
        acc.y += albedo.y * ndotl * 0.7 + spec * 0.7;
        acc.z += albedo.z * ndotl * 0.7 + spec * 0.7;
    }

    /* Small ambient keeps shadowed tiles from going pure black. */
    Vec amb = vmul(albedo, 0.03);
    lit = vadd(acc, amb);

    /* Weak mirror sheen on the ground, strongest at grazing angles. */
    Vec rvec = reflect_vec(view_dir, n);
    double fres = fresnel_schlick(fabs(vdot(n, view_dir)), 1.0, 1.5);
    fres = fmin(0.35, fres * 0.6);
    Vec refl;
    if (depth + 1 < MAX_BOUNCES) {
        Vec ro = vadd(pos, vmul(n, 1e-4));
        Hit rh;
        if (trace(ro, rvec, &rh)) refl = shade(rh, rvec, depth + 1);
        else refl = sky(rvec);
    } else {
        refl = sky(rvec);
    }
    lit.x = lit.x * (1.0 - fres) + refl.x * fres;
    lit.y = lit.y * (1.0 - fres) + refl.y * fres;
    lit.z = lit.z * (1.0 - fres) + refl.z * fres;
    return lit;
}

static Vec shade_chrome(Vec pos, Vec n, Vec view_dir, unsigned depth) {
    Vec rvec = reflect_vec(view_dir, n);
    Vec refl;
    if (depth + 1 < MAX_BOUNCES) {
        Vec ro = vadd(pos, vmul(n, 1e-4));
        Hit rh;
        if (trace(ro, rvec, &rh)) refl = shade(rh, rvec, depth + 1);
        else refl = sky(rvec);
    } else {
        refl = sky(rvec);
    }

    Vec hl = { 0.0, 0.0, 0.0 };
    Vec l, hh;
    l = vnorm(vsub(key, pos));
    if (!light_hit(pos, l, sqrt(vdot(vsub(key, pos), vsub(key, pos))))) {
        hh = vnorm(vadd(l, view_dir));
        double s = pow(fmax(0.0, vdot(n, hh)), 90.0);
        hl.x += s; hl.y += s; hl.z += s;
    }
    l = vnorm(vsub(fill, pos));
    if (!light_hit(pos, l, sqrt(vdot(vsub(fill, pos), vsub(fill, pos))))) {
        hh = vnorm(vadd(l, view_dir));
        double s = pow(fmax(0.0, vdot(n, hh)), 90.0);
        hl.x += s * 0.35; hl.y += s * 0.35; hl.z += s * 0.35;
    }
    l = vnorm(vsub(rim, pos));
    if (!light_hit(pos, l, sqrt(vdot(vsub(rim, pos), vsub(rim, pos))))) {
        hh = vnorm(vadd(l, view_dir));
        double s = pow(fmax(0.0, vdot(n, hh)), 90.0);
        hl.x += s * 0.7; hl.y += s * 0.7; hl.z += s * 0.7;
    }

    double w = 0.95;
    Vec r;
    r.x = refl.x * w + hl.x;
    r.y = refl.y * w + hl.y;
    r.z = refl.z * w + hl.z;
    return r;
}

static Vec shade_glass(Vec pos, Vec n, Vec view_dir, int hit_inside, unsigned depth) {
    double c_reflect = fresnel_schlick(fabs(vdot(n, view_dir)), 1.0, 1.5);

    Vec rr = reflect_vec(view_dir, n);
    Vec refl;
    Vec ro = vadd(pos, vmul(n, 1e-4));
    Hit rh;
    if (depth + 1 < MAX_BOUNCES) {
        if (trace(ro, rr, &rh)) refl = shade(rh, rr, depth + 1);
        else refl = sky(rr);
    } else {
        refl = sky(rr);
    }

    double n1 = hit_inside ? 1.5 : 1.0;
    double n2 = hit_inside ? 1.0 : 1.5;
    Vec refr;
    int ok = refract(n, view_dir, n1, n2, &refr);
    Vec refr_col;
    if (!ok) {
        /* Total internal reflection. */
        ro = vadd(pos, vmul(n, 1e-4));
        if (depth + 1 < MAX_BOUNCES) {
            if (trace(ro, rr, &rh)) refr_col = shade(rh, rr, depth + 1);
            else refr_col = sky(rr);
        } else {
            refr_col = sky(rr);
        }
    } else {
        Vec refr_ro = vadd(pos, vmul(refr, 1e-4));
        if (depth + 1 < MAX_BOUNCES) {
            if (trace(refr_ro, refr, &rh)) refr_col = shade(rh, refr, depth + 1);
            else refr_col = sky(refr);
        } else {
            refr_col = sky(refr);
        }
    }

    double w = c_reflect;
    Vec r;
    r.x = refl.x * w + refr_col.x * (1.0 - w);
    r.y = refl.y * w + refr_col.y * (1.0 - w);
    r.z = refl.z * w + refr_col.z * (1.0 - w);

    /* A little specular highlight on the glass surface. */
    Vec l = vnorm(vsub(key, pos));
    if (!light_hit(pos, l, sqrt(vdot(vsub(key, pos), vsub(key, pos))))) {
        Vec hh = vnorm(vadd(l, view_dir));
        double s = pow(fmax(0.0, vdot(n, hh)), 200.0);
        r.x += s; r.y += s; r.z += s;
    }
    return r;
}

static Vec shade(Hit h, Vec view_dir, unsigned depth) {
    if (h.is_plane) return shade_plane(h.pos, h.n, view_dir, depth);
    if (h.sphere.is_glass) return shade_glass(h.pos, h.n, view_dir, h.hit_inside, depth);
    return shade_chrome(h.pos, h.n, view_dir, depth);
}

static double to8(double linear) {
    double c = pow(clamp01(linear), 1.0 / 2.2);
    return c;
}

static Vec shade_ray(Vec ro, Vec rd) {
    Hit h;
    if (trace(ro, rd, &h)) {
        Vec surf = shade(h, rd, 0);
        double tt = exp(-0.035 * h.t);
        Vec fg = sky(rd);
        Vec r;
        r.x = tt * surf.x + (1.0 - tt) * fg.x;
        r.y = tt * surf.y + (1.0 - tt) * fg.y;
        r.z = tt * surf.z + (1.0 - tt) * fg.z;
        return r;
    }
    return sky(rd);
}

int main(void) {
    build_camera();

    int w = 1280, h = 720;
    unsigned char *img = (unsigned char *)malloc((size_t)w * (size_t)h * 3);
    if (!img) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    double offs[2] = { 0.25, 0.75 };

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            Vec acc = { 0, 0, 0 };
            for (int sy = 0; sy < 2; ++sy) {
                for (int sx = 0; sx < 2; ++sx) {
                    double u = ((double)x + offs[sx]) / (double)w;
                    double v = ((double)y + offs[sy]) / (double)h;
                    double ndcx = 2.0 * u - 1.0;
                    double ndcy = 2.0 * v - 1.0;
                    Vec dir = vnorm(vadd(vadd(vmul(cam_right, ndcx * half_w),
                                              vmul(cam_up, ndcy * half_h)),
                                         cam_fwd));
                    Vec c = shade_ray(cam_pos, dir);
                    acc.x += c.x; acc.y += c.y; acc.z += c.z;
                }
            }
            acc.x *= 0.25; acc.y *= 0.25; acc.z *= 0.25;
            size_t idx = ((size_t)y * (size_t)w + (size_t)x) * 3;
            img[idx + 0] = (unsigned char)(to8(acc.z) * 255.0 + 0.5);
            img[idx + 1] = (unsigned char)(to8(acc.y) * 255.0 + 0.5);
            img[idx + 2] = (unsigned char)(to8(acc.x) * 255.0 + 0.5);
        }
    }

    FILE *f = fopen("out.tga", "wb");
    if (!f) { fprintf(stderr, "cannot open out.tga\n"); free(img); return 1; }

    unsigned char header[18];
    for (int i = 0; i < 18; ++i) header[i] = 0;
    header[2] = 2;
    header[12] = (unsigned char)(w & 0xFF);
    header[13] = (unsigned char)((w >> 8) & 0xFF);
    header[14] = (unsigned char)(h & 0xFF);
    header[15] = (unsigned char)((h >> 8) & 0xFF);
    header[16] = 24;
    header[17] = 0x20;

    if (fwrite(header, 1, 18, f) != 18) {
        fprintf(stderr, "failed writing header\n");
        fclose(f); free(img); return 1;
    }
    if (fwrite(img, 1, (size_t)w * (size_t)h * 3, f) != (size_t)w * (size_t)h * 3) {
        fprintf(stderr, "failed writing pixels\n");
        fclose(f); free(img); return 1;
    }
    if (fclose(f) != 0) {
        fprintf(stderr, "failed closing out.tga\n");
        free(img); return 1;
    }
    free(img);
    return 0;
}
