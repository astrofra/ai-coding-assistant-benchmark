/*
 * C99 Mini Ray Tracer
 *
 * Renders two spheres (glass left, chrome right) resting on an infinite
 * black-and-white checkerboard plane, with three-point lighting, a red-to-
 * orange gradient sky, and distance fog. Output is a 1280x720 24-bit TGA.
 *
 * Strict ISO C99, standard library + libm only. No external image libraries.
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

#define IMG_W       1280
#define IMG_H       720
#define SAMPLES     4              /* 2x2 deterministic subpixel grid    */
#define MAX_BOUNCE  8              /* secondary-ray recursion limit      */
#define EPSILON     1e-4f          /* surface offset to avoid acne       */

typedef struct { double x, y, z; } Vec3;

/* ------------------------------------------------------------------ */
/* Vector math                                                        */
/* ------------------------------------------------------------------ */

static Vec3 vadd(Vec3 a, Vec3 b) { Vec3 r = {a.x+b.x, a.y+b.y, a.z+b.z}; return r; }
static Vec3 vsub(Vec3 a, Vec3 b) { Vec3 r = {a.x-b.x, a.y-b.y, a.z-b.z}; return r; }
static Vec3 vmul(Vec3 a, double s) { Vec3 r = {a.x*s, a.y*s, a.z*s}; return r; }
static double vdot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static Vec3 vcross(Vec3 a, Vec3 b) {
    Vec3 r = {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
    return r;
}
static double vlen(Vec3 a) { return sqrt(vdot(a, a)); }
static Vec3 vnorm(Vec3 a) {
    double l = vlen(a);
    /* Guard against a near-zero length to avoid dividing by zero. */
    if (l < 1e-15) { Vec3 z = {0, 0, 0}; return z; }
    return vmul(a, 1.0 / l);
}

/* ------------------------------------------------------------------ */
/* Scene objects                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    Vec3 center;
    double radius;
} Sphere;

enum MaterialKind { MAT_GLASS, MAT_CHROME };

typedef struct {
    enum MaterialKind kind;
    Vec3 albedo;
    double ior;
} Material;

static const Sphere g_glass = { { -1.25, 1.0, 0.0 }, 1.0 };
static const Sphere g_chrome = { { 1.25, 1.0, 0.0 }, 1.0 };

static const Material mat_glass  = { MAT_GLASS,  { 0.0, 0.0, 0.0 }, 1.5 };
static const Material mat_chrome = { MAT_CHROME, { 0.9, 0.9, 0.9 }, 1.0 };

/* ------------------------------------------------------------------ */
/* Lighting                                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    Vec3 pos;
    double strength;
} Light;

static const Light g_lights[3] = {
    { { -3.5, 6.0, 4.5 }, 1.0  },   /* key light, camera's left   */
    { {  4.0, 3.0, 3.0 }, 0.35 },   /* fill light, camera's right */
    { {  0.5, 5.0, -4.0 }, 0.7  }    /* back / rim light           */
};

/* Small ambient so shadowed regions are not pure black. */
static const double g_ambient = 0.06;

/* ------------------------------------------------------------------ */
/* Intersection primitives                                            */
/* ------------------------------------------------------------------ */

/* Ray defined by origin o and (unit) direction d. Returns t of nearest
 * positive hit or INFINITY when there is none. */

typedef struct {
    double t;
    Vec3 normal;
    const Material *mat;
    int is_ground;
} Hit;

/* Analytic ray-sphere intersection. Supports origins inside the sphere by
 * considering both quadratic roots. */
static double ray_sphere(Vec3 o, Vec3 d, const Sphere *s, double *t_out_p,
                         Vec3 *normal_out, int entering) {
    Vec3 oc = vsub(o, s->center);
    double b = vdot(oc, d);
    double c = vdot(oc, oc) - s->radius * s->radius;
    double disc = b * b - c;
    if (disc < 0.0)
        return 0.0;
    double sq = sqrt(disc);
    double t = -b - sq;   /* near root */
    if (t < 1e-5)
        t = -b + sq;      /* far root (used when starting inside) */
    if (t < 1e-5)
        return 0.0;
    if (t_out_p) {
        *t_out_p = t;
        Vec3 p = vadd(o, vmul(d, t));
        Vec3 n = vnorm(vsub(p, s->center));
        *normal_out = n;
        (void)entering;
    }
    return 1.0;
}

/* Analytic ray-plane intersection for the ground at y = 0. */
static double ray_plane(Vec3 o, Vec3 d) {
    if (fabs(d.y) < 1e-12)
        return 0.0;              /* ray parallel to the ground   */
    double t = -o.y / d.y;
    if (t < 1e-5)
        return 0.0;
    return t;
}

/* ------------------------------------------------------------------ */
/* Checkerboard albedo                                                */
/* ------------------------------------------------------------------ */

/* Procedural infinite checkerboard: strips alternate on integer cells of
 * world X and Z, using floor so negative coordinates stay continuous. */
static Vec3 checker_albedo(double x, double z) {
    int cx = (int)floor(x);
    int cz = (int)floor(z);
    if (((cx + cz) & 1) == 0) {
        Vec3 w = { 1.0, 1.0, 1.0 };
        return w;
    } else {
        Vec3 k = { 0.0, 0.0, 0.0 };
        return k;
    }
}

/* ------------------------------------------------------------------ */
/* Sky                                                                */
/* ------------------------------------------------------------------ */

/* Procedural gradient sky evaluated from the normalized ray direction.
 * Red at zenith/nadir, orange at the horizon, symetrically above/below. */
static Vec3 sky_rgb(Vec3 d) {
    double t = pow(fabs(d.y), 0.65);
    if (t > 1.0) t = 1.0;
    if (t < 0.0) t = 0.0;
    Vec3 horizon = { 1.0, 0.25, 0.03 };
    Vec3 red     = { 0.18, 0.01, 0.015 };
    Vec3 r;
    r.x = (1.0 - t) * horizon.x + t * red.x;
    r.y = (1.0 - t) * horizon.y + t * red.y;
    r.z = (1.0 - t) * horizon.z + t * red.z;
    return r;
}

/* ------------------------------------------------------------------ */
/* Shading helpers                                                    */
/* ------------------------------------------------------------------ */

/* Fresnel reflectance for a dielectric using Schlick's approximation,
 * given the cosine of the incident angle and the two indices of refraction.
 * c is cos(theta) with theta measured from the (outward-facing) normal. */
static double fresnel_schlick(double cos_i, double ni, double nt) {
    double r0 = (ni - nt) / (ni + nt);
    r0 *= r0;
    /* cos_i near grazing -> high reflectivity. */
    double c = 1.0 - cos_i;
    return r0 + (1.0 - r0) * c * c * c * c * c;
}

/* ------------------------------------------------------------------ */
/* Main trace                                                         */
/* ------------------------------------------------------------------ */

/* Cast a ray against the scene, returning the nearest hit (or infinity). */
static int closest_hit(Vec3 o, Vec3 d, Hit *hit) {
    double best = INFINITY;
    int found = 0;

    /* Ground plane. */
    double tp = ray_plane(o, d);
    if (tp > 0.0 && tp < best) {
        best = tp;
        hit->t = tp;
        hit->normal = (Vec3){0.0, 1.0, 0.0};
        hit->mat = NULL;
        hit->is_ground = 1;
        found = 1;
    }

    /* Spheres. */
    double ts;
    Vec3 n;
    if (ray_sphere(o, d, &g_glass, &ts, &n, 1) && ts < best) {
        best = ts;
        hit->t = ts;
        hit->normal = n;
        hit->mat = &mat_glass;
        hit->is_ground = 0;
        found = 1;
    }
    if (ray_sphere(o, d, &g_chrome, &ts, &n, 1) && ts < best) {
        best = ts;
        hit->t = ts;
        hit->normal = n;
        hit->mat = &mat_chrome;
        hit->is_ground = 0;
        found = 1;
    }
    return found;
}

/* Cast a shadow ray toward a light, checking for occluders. The ray is
 * capped at the light distance. Glass lets most light through (tinted). */
static int shadowed(Vec3 o, Vec3 d, double max_t) {
    Hit h;
    if (!closest_hit(o, d, &h))
        return 0;
    if (h.t > max_t)
        return 0;
    /* A glass intersection is mostly transparent to shadow rays, so treat
     * grazing glass occluders as largely unblocked (neutral approximation). */
    if (h.mat == &mat_glass)
        return (h.t > max_t);   /* effectively unblocked */
    (void)h;
    return 1;
}

/* Lambertian + specular shading for a finite hit, plus shadow rays. */
static Vec3 shade_point(Vec3 p, Vec3 n, Vec3 view, const Hit *hit) {
    Vec3 albedo;
    if (hit->is_ground) {
        albedo = checker_albedo(p.x, p.z);
    } else {
        albedo = hit->mat->albedo;
    }

    Vec3 color = vmul(albedo, g_ambient);

    int i;
    for (i = 0; i < 3; ++i) {
        Vec3 l = vsub(g_lights[i].pos, p);
        double dist = vlen(l);
        l = vmul(l, 1.0 / dist);

        Vec3 sh_origin = vadd(p, vmul(n, EPSILON));
        if (shadowed(sh_origin, l, dist - 1e-4))
            continue;

        double ndl = vdot(n, l);
        if (ndl > 0.0)
            color = vadd(color, vmul(vmul(albedo, ndl), g_lights[i].strength));

        /* Blinn-Phong specular highlight for the polished spheres. */
        Vec3 h = vnorm(vadd(l, view));
        double ndh = vdot(n, h);
        if (ndh > 0.0) {
            double spec = pow(ndh, 64.0);
            Vec3 white = { 1.0, 1.0, 1.0 };
            color = vadd(color, vmul(vmul(white, spec), g_lights[i].strength * 0.6));
        }
    }
    /* The white checker tiles should be slightly brighter than pure diffuse
     * so the pattern reads clearly. */
    return color;
}

/* Recursive path tracer with fog. depth is the number of bounces taken. */
static Vec3 trace(Vec3 o, Vec3 d, int depth) {
    Hit hit;
    if (!closest_hit(o, d, &hit)) {
        /* Ray escaped the scene: return the sky directly. */
        return sky_rgb(d);
    }

    Vec3 p = vadd(o, vmul(d, hit.t));

    if (hit.is_ground) {
        Vec3 view = vmul(d, -1.0);
        Vec3 color = shade_point(p, hit.normal, view, &hit);
        /* Distance fog blending ground into sky. */
        double trans = exp(-0.035 * hit.t);
        Vec3 fog = sky_rgb(d);
        Vec3 r;
        r.x = trans * color.x + (1.0 - trans) * fog.x;
        r.y = trans * color.y + (1.0 - trans) * fog.y;
        r.z = trans * color.z + (1.0 - trans) * fog.z;
        return r;
    }

    if (hit.mat->kind == MAT_GLASS) {
        /* Determine whether we are entering or exiting the sphere by
         * comparing the ray direction to the surface normal. */
        double cos_i = vdot(d, hit.normal);
        int entering = cos_i < 0.0;
        Vec3 n = entering ? hit.normal : vmul(hit.normal, -1.0);
        double ni = entering ? 1.0 : hit.mat->ior;   /* index on incident side */
        double nt = entering ? hit.mat->ior : 1.0;   /* index on far side      */
        double eta = ni / nt;

        double cos_theta = -vdot(d, n);
        cos_theta = cos_theta < -1.0 ? -1.0 : (cos_theta > 1.0 ? 1.0 : cos_theta);

        double sin2 = 1.0 - cos_theta * cos_theta;
        double kr = fresnel_schlick(-cos_i < 0 ? -cos_i : cos_i, ni, nt);
        double kt = 1.0 - kr;

        Vec3 refl = vsub(d, vmul(n, 2.0 * vdot(d, n)));
        Vec3 trans_dir;
        int total_internal = 0;

        if (sin2 * eta * eta > 1.0) {
            /* Total internal reflection: reflect all energy. */
            total_internal = 1;
            kr = 1.0;
            kt = 0.0;
        } else {
            double k = eta * cos_theta - sqrt(1.0 - eta * eta * sin2);
            trans_dir = vadd(vmul(d, eta), vmul(n, k));
            trans_dir = vnorm(trans_dir);
        }

        if (depth >= MAX_BOUNCE) {
            /* Fallback at recursion limit: rely on sky for missed rays. */
            Vec3 col = { 0.0, 0.0, 0.0 };
            return col;
        }

        Vec3 refl_origin = vadd(p, vmul(n, EPSILON));
        Vec3 refl_color;
        if (kr > 1e-4)
            refl_color = trace(refl_origin, refl, depth + 1);
        else {
            refl_color.x = refl_color.y = refl_color.z = 0.0;
        }

        {
            static int dbg_n = 0;
            if (getenv("RAY_DEBUG") && dbg_n < 24) {
                double rm = vlen(refl_color);
                fprintf(stderr, "  glass d=%d kr=%.3f rm=%.6g\n", depth, kr, rm);
                dbg_n++;
            }
        }

        Vec3 color;
        if (total_internal) {
            color = refl_color;
        } else {
            /* Offset the refracted origin along its own direction so it does
             * not re-hit the interface it is leaving. */
            Vec3 refr_origin = vadd(p, vmul(trans_dir, EPSILON));
            Vec3 refr_color = trace(refr_origin, trans_dir, depth + 1);
            color.x = kr * refl_color.x + kt * refr_color.x;
            color.y = kr * refl_color.y + kt * refr_color.y;
            color.z = kr * refl_color.z + kt * refr_color.z;
        }

        /* Fog applied to the finite glass segment. */
        double trans = exp(-0.035 * hit.t);
        Vec3 fog = sky_rgb(d);
        Vec3 r;
        r.x = trans * color.x + (1.0 - trans) * fog.x;
        r.y = trans * color.y + (1.0 - trans) * fog.y;
        r.z = trans * color.z + (1.0 - trans) * fog.z;
        return r;
    }

    /* Chrome: heavy mirror reflection with slight diffuse tint. */
    if (depth >= MAX_BOUNCE) {
        Vec3 col = { 0.0, 0.0, 0.0 };
        return col;
    }
    Vec3 refl = vsub(d, vmul(hit.normal, 2.0 * vdot(d, hit.normal)));
    Vec3 refl_origin = vadd(p, vmul(hit.normal, EPSILON));
    Vec3 refl_color = trace(refl_origin, refl, depth + 1);

    Vec3 view = vmul(d, -1.0);
    Vec3 local = shade_point(p, hit.normal, view, &hit);
    double rw = 0.95;   /* reflection weight */
    double dw = 0.05;   /* small diffuse contribution   */
    Vec3 color;
    color.x = rw * refl_color.x + dw * local.x;
    color.y = rw * refl_color.y + dw * local.y;
    color.z = rw * refl_color.z + dw * local.z;

    double trans = exp(-0.035 * hit.t);
    Vec3 fog = sky_rgb(d);
    Vec3 r;
    r.x = trans * color.x + (1.0 - trans) * fog.x;
    r.y = trans * color.y + (1.0 - trans) * fog.y;
    r.z = trans * color.z + (1.0 - trans) * fog.z;
    return r;
}

/* ------------------------------------------------------------------ */
/* Tone mapping and TGA output                                        */
/* ------------------------------------------------------------------ */

static unsigned char to8(double v) {
    if (v < 0.0) v = 0.0;
    /* Reinhard tone mapping: compress the linear HDR range into [0,1) so
     * that reflect/refract highlights (which can exceed 2.0) keep contrast
     * instead of clipping to pure white. */
    v = v / (1.0 + v);
    /* Gamma encode (2.2). */
    double g = pow(v, 1.0 / 2.2);
    return (unsigned char)(g * 255.0 + 0.5);
}


/* ------------------------------------------------------------------ */
/* Camera                                                             */
/* ------------------------------------------------------------------ */

static Vec3 camera_ray(double px, double py, Vec3 *basis) {
    /* px, py in [-1, 1] of the sensor plane. */
    double fov = 45.0 * (3.14159265358979323846 / 180.0);
    double tan_half = tan(fov * 0.5);
    double aspect = (double)IMG_W / (double)IMG_H;
    Vec3 right = basis[0];
    Vec3 up    = basis[1];
    Vec3 fwd   = basis[2];
    double x = px * tan_half * aspect;
    double y = py * tan_half;
    Vec3 d;
    d.x = fwd.x + right.x * x + up.x * y;
    d.y = fwd.y + right.y * x + up.y * y;
    d.z = fwd.z + right.z * x + up.z * y;
    return vnorm(d);
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    /* Camera basis. */
    Vec3 eye = { 0.0, 2.6, 6.5 };
    Vec3 target = { 0.0, 1.0, 0.0 };
    Vec3 world_up = { 0.0, 1.0, 0.0 };
    Vec3 fwd = vnorm(vsub(target, eye));
    Vec3 right = vnorm(vcross(fwd, world_up));
    Vec3 up = vcross(right, fwd);
    Vec3 basis[3] = { right, up, fwd };

    /* Temporary diagnostics: dump raw linear values for a row of glass
     * pixels when RAY_DEBUG is set. */
    int dbg = getenv("RAY_DEBUG") ? 1 : 0;

    /* Scene accumulator in linear light. */
    double *buf = malloc((size_t)IMG_W * IMG_H * 3 * sizeof(double));
    if (!buf) {
        fprintf(stderr, "raytracer: out of memory\n");
        return 1;
    }

    /* Deterministic 2x2 subpixel grid; offsets fixed for reproducibility. */
    const double offs[2] = { 0.25, 0.75 };

    for (int y = 0; y < IMG_H; ++y) {
        for (int x = 0; x < IMG_W; ++x) {
            double acc[3] = { 0.0, 0.0, 0.0 };
            for (int sy = 0; sy < 2; ++sy) {
                for (int sx = 0; sx < 2; ++sx) {
                    double ndc_x = ((double)x + offs[sx]) / (double)IMG_W;
                    double ndc_y = ((double)y + offs[sy]) / (double)IMG_H;
                    /* Map to sensor NDC in [-1, 1] with Y flipped (image top). */
                    double px = 2.0 * ndc_x - 1.0;
                    double py = 1.0 - 2.0 * ndc_y;
                    Vec3 d = camera_ray(px, py, basis);
                    Vec3 c = trace(eye, d, 0);
                    acc[0] += c.x;
                    acc[1] += c.y;
                    acc[2] += c.z;
                }
            }
            int idx = (y * IMG_W + x) * 3;
            buf[idx + 0] = acc[0] / (double)SAMPLES;
            buf[idx + 1] = acc[1] / (double)SAMPLES;
            buf[idx + 2] = acc[2] / (double)SAMPLES;
            if (dbg && y == 360 && (x % 20 == 0))
                fprintf(stderr, "px %d: %.3f %.3f %.3f\n",
                        x, buf[idx+0], buf[idx+1], buf[idx+2]);
        }
    }

    /* Write uncompressed 24-bit TGA. */
    FILE *f = fopen("out.tga", "wb");
    if (!f) {
        fprintf(stderr, "raytracer: cannot open out.tga for writing\n");
        free(buf);
        return 1;
    }

    unsigned char header[18] = { 0 };
    header[2] = 2;                    /* uncompressed true-color */
    header[12] = (unsigned char)(IMG_W & 0xff);
    header[13] = (unsigned char)((IMG_W >> 8) & 0xff);
    header[14] = (unsigned char)(IMG_H & 0xff);
    header[15] = (unsigned char)((IMG_H >> 8) & 0xff);
    header[16] = 24;                  /* bits per pixel, no alpha */
    header[17] = 0x20;                /* top-left origin, RGB order L->R */

    if (fwrite(header, 1, 18, f) != 18) {
        fprintf(stderr, "raytracer: failed writing TGA header\n");
        fclose(f);
        free(buf);
        return 1;
    }

    /* Write pixels B,G,R, row by row from top to bottom. */
    unsigned char *row = malloc((size_t)IMG_W * 3);
    if (!row) {
        fprintf(stderr, "raytracer: out of memory\n");
        fclose(f);
        free(buf);
        return 1;
    }
    for (int y = 0; y < IMG_H; ++y) {
        for (int x = 0; x < IMG_W; ++x) {
            int idx = (y * IMG_W + x) * 3;
            unsigned char b = to8(buf[idx + 2]);
            unsigned char g = to8(buf[idx + 1]);
            unsigned char r = to8(buf[idx + 0]);
            row[x * 3 + 0] = b;
            row[x * 3 + 1] = g;
            row[x * 3 + 2] = r;
        }
        if (fwrite(row, 1, (size_t)IMG_W * 3, f) != (size_t)IMG_W * 3) {
            fprintf(stderr, "raytracer: failed writing pixel data\n");
            free(row);
            fclose(f);
            free(buf);
            return 1;
        }
    }

    if (fclose(f) != 0) {
        fprintf(stderr, "raytracer: error closing out.tga\n");
        free(row);
        free(buf);
        return 1;
    }

    free(row);
    free(buf);
    return 0;
}
