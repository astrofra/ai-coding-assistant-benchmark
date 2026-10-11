/*
 * C99 Mini Ray Tracer
 *
 * Renders two spheres (glass left, chrome right) on an infinite
 * checkerboard plane, with three-point lighting, a red-to-orange
 * gradient sky, and distance fog. Outputs out.tga (1280x720, 24-bit).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#define WIDTH  1280
#define HEIGHT 720
#define SAMPLE_SIDE 2                    /* 2x2 subpixel grid = 4 samples/pixel */
#define MAX_DEPTH 8                      /* secondary-ray bounce count */
#define EPS 1e-4                         /* surface offset to avoid acne */

static const double INF = 1e30;

/* ------------------------------------------------------------------ */
/* 3D vector math                                                     */
/* ------------------------------------------------------------------ */

typedef struct { double x, y, z; } V3;

static V3 v3(double x, double y, double z) {
    V3 v; v.x = x; v.y = y; v.z = z; return v;
}

static V3 vadd(V3 a, V3 b) { return v3(a.x+b.x, a.y+b.y, a.z+b.z); }
static V3 vsub(V3 a, V3 b) { return v3(a.x-b.x, a.y-b.y, a.z-b.z); }
static V3 vmul(V3 a, double s) { return v3(a.x*s, a.y*s, a.z*s); }
static V3 vneg(V3 a) { return v3(-a.x, -a.y, -a.z); }
static double vdot(V3 a, V3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static double vlen(V3 a) { return sqrt(vdot(a, a)); }
static V3 vnorm(V3 a) {
    double l = vlen(a);
    if (l < 1e-12) return v3(0.0, 1.0, 0.0);
    return vmul(a, 1.0 / l);
}
static V3 vlerp(V3 a, V3 b, double t) {
    return v3(a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t);
}

/* ------------------------------------------------------------------ */
/* Materials and scene objects                                        */
/* ------------------------------------------------------------------ */

enum MatKind { MAT_GLASS, MAT_CHROME, MAT_CHECKER };

typedef struct {
    enum MatKind kind;
    double ior;          /* index of refraction for glass */
} Material;

typedef struct {
    V3 center;
    double radius;
    Material mat;
} Sphere;

typedef struct {
    V3 pos;
    double strength;
} Light;

static Sphere glass_sphere = { { -1.25, 1.0, 0.0 }, 1.0, { MAT_GLASS, 1.5 } };
static Sphere chrome_sphere = { { 1.25, 1.0, 0.0 }, 1.0, { MAT_CHROME, 1.0 } };

static Light lights[3] = {
    { { -3.5, 6.0, 4.5 }, 1.0  },   /* key   */
    { {  4.0, 3.0, 3.0 }, 0.35 },   /* fill  */
    { {  0.5, 5.0, -4.0 }, 0.7  }   /* rim   */
};

static const double AMBIENT = 0.08;

/* ------------------------------------------------------------------ */
/* Intersection helpers                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    int hit;
    double t;
    int is_sphere;
    int sphere_index;      /* 0 = glass, 1 = chrome */
    V3 point;
    V3 normal;             /* geometric normal, assumes outward */
    Material mat;
} Hit;

/* Ray-sphere analytic intersection: returns nearest t > 0 or INF. */
static double sphere_intersect(V3 o, V3 d, V3 c, double r,
                               double *n1, double *n2) {
    V3 oc = vsub(o, c);
    double b = vdot(oc, d);
    double cc = vdot(oc, oc) - r * r;
    double disc = b * b - cc;
    if (disc < 0.0) {
        *n1 = INF; *n2 = INF;
        return INF;
    }
    double s = sqrt(disc);
    double t0 = -b - s;
    double t1 = -b + s;
    *n1 = t0;
    *n2 = t1;
    if (t0 > 1e-4) return t0;
    if (t1 > 1e-4) return t1;   /* start inside sphere */
    return INF;
}

/* Infinite plane y = plane_y. Returns t > 0 or INF if parallel/behind. */
static double plane_intersect(V3 o, V3 d, double plane_y,
                              double *out_not_parallel) {
    (void)out_not_parallel;
    if (fabs(d.y) < 1e-12) return INF;   /* parallel */
    double t = (plane_y - o.y) / d.y;
    if (t <= 1e-4) return INF;
    return t;
}

/* Find nearest hit for a ray, given an optional current object to ignore
 * (handles rays leaving a dielectric). Returns 1 if hit, else 0. */
static Hit trace_scene(V3 o, V3 d, int ignore_sphere) {
    Hit h;
    h.hit = 0;
    h.t = INF;

    /* Glass sphere */
    if (ignore_sphere != 0) {
        double n1, n2;
        double t = sphere_intersect(o, d, glass_sphere.center,
                                    glass_sphere.radius, &n1, &n2);
        if (t < h.t) {
            h.hit = 1;
            h.t = t;
            h.is_sphere = 1;
            h.sphere_index = 0;
            h.point = vadd(o, vmul(d, t));
            V3 n = vnorm(vsub(h.point, glass_sphere.center));
            h.normal = n; /* orient later based on direction */
            h.mat = glass_sphere.mat;
        }
    }

    /* Chrome sphere */
    if (ignore_sphere != 1) {
        double n1, n2;
        double t = sphere_intersect(o, d, chrome_sphere.center,
                                    chrome_sphere.radius, &n1, &n2);
        if (t < h.t) {
            h.hit = 1;
            h.t = t;
            h.is_sphere = 1;
            h.sphere_index = 1;
            h.point = vadd(o, vmul(d, t));
            V3 n = vnorm(vsub(h.point, chrome_sphere.center));
            h.normal = n;
            h.mat = chrome_sphere.mat;
        }
    }

    /* Ground plane y = 0 */
    {
        double t = plane_intersect(o, d, 0.0, NULL);
        if (t < h.t) {
            h.hit = 1;
            h.t = t;
            h.is_sphere = 0;
            h.sphere_index = -1;
            h.point = vadd(o, vmul(d, t));
            h.normal = v3(0.0, 1.0, 0.0);
            h.mat.kind = MAT_CHECKER;
            h.mat.ior = 1.0;
        }
    }

    return h;
}

/* ------------------------------------------------------------------ */
/* Environment (sky) and fog                                          */
/* ------------------------------------------------------------------ */

static V3 sky_color(V3 d) {
    d = vnorm(d);
    double t = pow(fabs(d.y), 0.65);
    if (t > 1.0) t = 1.0;
    if (t < 0.0) t = 0.0;
    V3 horizon = v3(1.0, 0.25, 0.03);
    V3 darkred = v3(0.18, 0.01, 0.015);
    return vlerp(horizon, darkred, t);
}

static V3 apply_fog(V3 color, V3 ray_dir, double dist) {
    double trans = exp(-0.035 * dist);
    V3 fog = sky_color(ray_dir);
    return vlerp(fog, color, trans);
}

/* ------------------------------------------------------------------ */
/* Checkerboard albedo on infinite y=0 plane                          */
/* ------------------------------------------------------------------ */

static V3 checker_albedo(V3 p) {
    int sx = (int)floor(p.x);
    int sz = (int)floor(p.z);
    int c = (sx + sz) & 1;
    return c ? v3(1.0, 1.0, 1.0) : v3(0.0, 0.0, 0.0);
}

/* ------------------------------------------------------------------ */
/* Shadows                                                            */
/* ------------------------------------------------------------------ */

/* Checks whether a shadow ray toward a light is blocked. Glass transmits
 * with a simple neutral attenuation; chrome blocks. */
static double shadow_transmit(V3 o, V3 light_dir, double light_dist) {
    double throughput = 1.0;
    /* check glass sphere */
    {
        double n1, n2;
        double t = sphere_intersect(o, light_dir, glass_sphere.center,
                                    glass_sphere.radius, &n1, &n2);
        if (t < light_dist) throughput *= 0.85;  /* light passes through with attenuation */
    }
    /* chrome blocks fully */
    {
        double n1, n2;
        double t = sphere_intersect(o, light_dir, chrome_sphere.center,
                                    chrome_sphere.radius, &n1, &n2);
        if (t < light_dist) return 0.0;
    }
    return throughput;
}

/* ------------------------------------------------------------------ */
/* Fresnel / Schlick                                                  */
/* ------------------------------------------------------------------ */

static double fresnel_schlick(double cos_i, double n1, double n2) {
    double r0 = (n1 - n2) / (n1 + n2);
    r0 = r0 * r0;
    double c = 1.0 - cos_i;
    return r0 + (1.0 - r0) * c * c * c * c * c;
}

/* Reflect vector d about normal n. d is incident direction (pointing away
 * from the surface toward the viewer). */
static V3 reflect(V3 d, V3 n) {
    return vsub(d, vmul(n, 2.0 * vdot(d, n)));
}

/* Refract using Snell's law. Returns 1 on success, 0 on TIR. */
static int refract(V3 d, V3 n, double eta, V3 *out) {
    double cos_i = vdot(n, vneg(d));  /* n and d point to opposite sides */
    double sin2_t = eta * eta * (1.0 - cos_i * cos_i);
    if (sin2_t > 1.0) return 0;  /* total internal reflection */
    double cos_t = sqrt(1.0 - sin2_t);
    V3 t = vadd(vmul(d, eta), vmul(n, eta * cos_i - cos_t));
    *out = vnorm(t);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Shading                                                            */
/* ------------------------------------------------------------------ */

static V3 shade(V3 o, V3 d, int depth);

static V3 shade_sphere(const Hit *h, V3 d, int depth) {
    /* Orient normal toward incident direction d (d points toward origin
     * of the ray, i.e. away from the surface toward the viewer). */
    V3 n = h->normal;
    int entering = vdot(n, d) < 0.0 ? 1 : 0;  /* d points into surface? */
    if (!entering) n = vneg(n); /* we are inside; flip normal outward? */

    if (h->mat.kind == MAT_CHROME) {
        V3 rd = reflect(d, n);
        V3 reflected = shade(vadd(h->point, vmul(n, EPS)), rd, depth + 1);
        /* specular highlights from lights */
        V3 spec = v3(0.0, 0.0, 0.0);
        for (int i = 0; i < 3; i++) {
            V3 l = vnorm(vsub(lights[i].pos, h->point));
            V3 hv = vnorm(vadd(l, vneg(d)));
            double s = pow(fmax(vdot(n, hv), 0.0), 128.0);
            double shadow = shadow_transmit(vadd(h->point, vmul(n, EPS)),
                                            l, vlen(vsub(lights[i].pos, h->point)));
            spec = vadd(spec, vmul(v3(1,1,1), lights[i].strength * s * shadow));
        }
        V3 refl_color = reflected;
        return vadd(vmul(refl_color, 0.95), vmul(spec, 0.5));
    }

    /* Glass dielectric */
    double ior_out = 1.0;    /* air */
    double ior_in  = 1.5;
    double eta;
    V3 interface_n;
    if (entering) {
        interface_n = n;
        eta = ior_out / ior_in;
    } else {
        interface_n = vneg(n);
        eta = ior_in / ior_out;
    }

    double cos_theta = fmax(vdot(interface_n, vneg(d)), 0.0);
    double fr = fresnel_schlick(cos_theta,
                                entering ? ior_out : ior_in,
                                entering ? ior_in  : ior_out);

    V3 reflected_dir = reflect(d, interface_n);
    V3 reflected = shade(vadd(h->point, vmul(interface_n, EPS)),
                         reflected_dir, depth + 1);

    V3 refracted_dir;
    V3 refracted;
    if (refract(d, interface_n, eta, &refracted_dir)) {
        V3 trans_o = vadd(h->point, vmul(vneg(interface_n), EPS));
        refracted = shade(trans_o, refracted_dir, depth + 1);
    } else {
        fr = 1.0;   /* total internal reflection */
        refracted = v3(0.0, 0.0, 0.0);
    }

    V3 base = vlerp(refracted, reflected, fr);

    /* Light specular highlights */
    V3 spec = v3(0.0, 0.0, 0.0);
    for (int i = 0; i < 3; i++) {
        V3 l = vnorm(vsub(lights[i].pos, h->point));
        V3 hv = vnorm(vadd(l, vneg(d)));
        double s = pow(fmax(vdot(n, hv), 0.0), 256.0);
        double shadow = shadow_transmit(vadd(h->point, vmul(interface_n, EPS)),
                                        l, vlen(vsub(lights[i].pos, h->point)));
        spec = vadd(spec, vmul(v3(1,1,1), lights[i].strength * s * shadow));
    }
    return vadd(base, vmul(spec, 0.4));
}

static V3 shade(V3 o, V3 d, int depth) {
    if (depth > MAX_DEPTH) {
        return sky_color(d); /* deliberate fallback at recursion limit */
    }

    Hit h = trace_scene(o, d, -1);
    if (!h.hit) return sky_color(d);

    V3 color;
    if (h.is_sphere) {
        color = shade_sphere(&h, d, depth);
    } else {
        /* Checkerboard ground */
        V3 albedo = checker_albedo(h.point);
        V3 n = h.normal;
        V3 diff = v3(0.0, 0.0, 0.0);
        for (int i = 0; i < 3; i++) {
            V3 l = vnorm(vsub(lights[i].pos, h.point));
            double ndl = fmax(vdot(n, l), 0.0);
            double shadow = shadow_transmit(vadd(h.point, vmul(n, EPS)),
                                            l, vlen(vsub(lights[i].pos, h.point)));
            diff = vadd(diff, vmul(albedo, ndl * lights[i].strength * shadow));
        }
        V3 ambient = vmul(albedo, AMBIENT);
        color = vadd(diff, ambient);
    }

    color = apply_fog(color, d, h.t);
    return color;
}

/* ------------------------------------------------------------------ */
/* Camera                                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    V3 origin;
    V3 lower_left;
    V3 horizontal;
    V3 vertical;
} Camera;

static Camera make_camera(void) {
    Camera cam;
    cam.origin = v3(0.0, 2.6, 6.5);
    V3 lookat = v3(0.0, 1.0, 0.0);
    V3 up = v3(0.0, 1.0, 0.0);
    double fov_y = 45.0 * M_PI / 180.0;
    double aspect = (double)WIDTH / (double)HEIGHT;

    V3 w = vnorm(vsub(cam.origin, lookat));  /* camera backward */
    V3 u = vnorm(v3(w.y*up.z - w.z*up.y, w.z*up.x - w.x*up.z, w.x*up.y - w.y*up.x));
    V3 v = v3(u.y*w.z - u.z*w.y, u.z*w.x - u.x*w.z, u.x*w.y - u.y*w.x);

    double theta = tan(fov_y * 0.5);
    double half_h = theta;
    double half_w = aspect * half_h;

    cam.lower_left = vsub(vsub(vsub(cam.origin, vmul(u, half_w)),
                               vmul(v, half_h)), w);
    cam.horizontal = vmul(u, 2.0 * half_w);
    cam.vertical = vmul(v, 2.0 * half_h);
    return cam;
}

static V3 ray_direction(const Camera *cam, double u, double v) {
    V3 dir = vsub(vadd(cam->lower_left, vadd(vmul(cam->horizontal, u),
                                             vmul(cam->vertical, v))),
                  cam->origin);
    return vnorm(dir);
}

/* ------------------------------------------------------------------ */
/* Linear color / tone mapping / sRGB encoding                        */
/* ------------------------------------------------------------------ */

static double srgb_encode(double c) {
    if (c <= 0.0) return 0.0;
    if (c >= 1.0) return 1.0;
    double a = 0.055;
    return c <= 0.0031308 ? 12.92 * c : (1.0 + a) * pow(c, 1.0/2.4) - a;
}

/* Simple Reinhard exposure applied per channel in linear space. */
static double tonemap(double c) {
    return c / (c + 1.0);
}

/* ------------------------------------------------------------------ */
/* TGA output                                                         */
/* ------------------------------------------------------------------ */

static int write_tga(const unsigned char *pixels, size_t nbytes,
                     const char *path) {
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "error: cannot open %s: %s\n", path, strerror(errno));
        return 0;
    }

    /* 18-byte header */
    unsigned char hdr[18];
    memset(hdr, 0, sizeof(hdr));
    hdr[2] = 2;                          /* uncompressed true-color */
    hdr[12] = WIDTH & 0xFF;              /* width little-endian */
    hdr[13] = (WIDTH >> 8) & 0xFF;
    hdr[14] = HEIGHT & 0xFF;
    hdr[15] = (HEIGHT >> 8) & 0xFF;
    hdr[16] = 24;                        /* 24-bit, no alpha */
    hdr[17] = 0x20;                      /* top-left origin */

    if (fwrite(hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) {
        fprintf(stderr, "error: failed writing TGA header\n");
        fclose(fp);
        return 0;
    }
    if (fwrite(pixels, 1, nbytes, fp) != nbytes) {
        fprintf(stderr, "error: failed writing TGA pixel data\n");
        fclose(fp);
        return 0;
    }
    if (fclose(fp) != 0) {
        fprintf(stderr, "error: failed closing %s\n", path);
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(void) {
    const size_t nbytes = (size_t)WIDTH * (size_t)HEIGHT * 3;
    unsigned char *pixels = malloc(nbytes);
    if (!pixels) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    Camera cam = make_camera();

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            double acc_r = 0.0, acc_g = 0.0, acc_b = 0.0;
            for (int sy = 0; sy < SAMPLE_SIDE; sy++) {
                for (int sx = 0; sx < SAMPLE_SIDE; sx++) {
                    /* deterministic 2x2 subpixel grid; mirror horizontally so
                     * increasing screen x maps to increasing world x (the camera
                     * basis u points toward negative x, glass sits on the left). */
                    double u = 1.0 - ((double)x + (sx + 0.5) / SAMPLE_SIDE) / WIDTH;
                    /* TGA rows are top-left origin. The camera vertical parameter
                     * maps from 1 (top of frame) to 0 (bottom), so invert y here
                     * to keep the sky at the top of the image. */
                    double v = ((double)(HEIGHT - 1 - y) +
                                (sy + 0.5) / SAMPLE_SIDE) / HEIGHT;
                    V3 d = ray_direction(&cam, u, v);
                    V3 c = shade(cam.origin, d, 0);
                    acc_r += c.x;
                    acc_g += c.y;
                    acc_b += c.z;
                }
            }
            int ns = SAMPLE_SIDE * SAMPLE_SIDE;
            double r = acc_r / ns;
            double g = acc_g / ns;
            double b = acc_b / ns;

            /* exposure + Reinhard tone map (linear space) */
            r = tonemap(r);
            g = tonemap(g);
            b = tonemap(b);

            /* gamma / sRGB encode */
            r = srgb_encode(r);
            g = srgb_encode(g);
            b = srgb_encode(b);

            size_t idx = ((size_t)y * WIDTH + (size_t)x) * 3;
            /* TGA stores B, G, R */
            pixels[idx + 0] = (unsigned char)(b * 255.0 + 0.5);
            pixels[idx + 1] = (unsigned char)(g * 255.0 + 0.5);
            pixels[idx + 2] = (unsigned char)(r * 255.0 + 0.5);
        }
    }

    if (!write_tga(pixels, nbytes, "out.tga")) {
        free(pixels);
        return 1;
    }
    free(pixels);

    /* verify exact expected file size */
    FILE *fp = fopen("out.tga", "rb");
    if (!fp) {
        fprintf(stderr, "error: cannot reopen out.tga\n");
        return 1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "error: fseek failed\n");
        fclose(fp);
        return 1;
    }
    long sz = ftell(fp);
    fclose(fp);
    if (sz != 18L + (long)nbytes) {
        fprintf(stderr, "error: unexpected file size %ld\n", sz);
        return 1;
    }

    return 0;
}
