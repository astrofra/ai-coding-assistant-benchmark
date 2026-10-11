/*
 * C99 Mini Ray Tracer
 *
 * Renders two spheres (glass and chrome) resting on a checkerboard plane,
 * with three-point lighting, a red-to-orange gradient sky, and distance fog.
 * Output: out.tga (uncompressed 24-bit true-color, 1280x720).
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Small vector math                                                   */
/* ------------------------------------------------------------------ */

typedef struct { double x, y, z; } Vec3;

static Vec3 v3(double x, double y, double z) {
    Vec3 v; v.x = x; v.y = y; v.z = z; return v;
}

static Vec3 add(Vec3 a, Vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static Vec3 sub(Vec3 a, Vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static Vec3 mul(Vec3 a, double s) { return v3(a.x * s, a.y * s, a.z * s); }
static Vec3 vmul(Vec3 a, Vec3 b) { return v3(a.x * b.x, a.y * b.y, a.z * b.z); }

static double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

static Vec3 normalize(Vec3 v) {
    double len = sqrt(dot(v, v));
    if (len > 1e-12) return mul(v, 1.0 / len);
    return v3(0.0, 1.0, 0.0);
}

/* Project v onto the plane defined by unit normal n. */
static Vec3 project(Vec3 v, Vec3 n) {
    return sub(v, mul(n, dot(v, n)));
}

/* ------------------------------------------------------------------ */
/* Scene configuration                                                 */
/* ------------------------------------------------------------------ */

#define IMG_W 1280
#define IMG_H 720

typedef struct { Vec3 center; double radius; int glass; } Sphere;

/* Glass has IOR 1.5; chrome is strongly reflective with no diffuse. */
static const Sphere spheres[2] = {
    { { -1.25, 1.0, 0.0 }, 1.0, 1 }, /* glass   */
    { {  1.25, 1.0, 0.0 }, 1.0, 0 }  /* chrome  */
};

/* Three white point lights. Strengths used without distance attenuation. */
typedef struct { Vec3 pos; double strength; } Light;
static const Light lights[3] = {
    { { -3.5, 6.0, 4.5 }, 1.0  }, /* key  */
    { {  4.0, 3.0, 3.0 }, 0.35 }, /* fill */
    { {  0.5, 5.0, -4.0 }, 0.7 }  /* rim  */
};

static Vec3 ray_dir(int px, int py, double sx, double sy) {
    double fx = (px + sx) / IMG_W;
    double fy = (py + sy) / IMG_H;
    double aspect = (double)IMG_W / (double)IMG_H;
    double tan_half_fov = tan(45.0 * 0.5 * (M_PI / 180.0));

    /* Camera: position (0,2.6,6.5), look-at (0,1,0), up +Y. */
    Vec3 campos = v3(0.0, 2.6, 6.5);
    Vec3 target = v3(0.0, 1.0, 0.0);
    Vec3 forward = normalize(sub(target, campos));
    Vec3 up = v3(0.0, 1.0, 0.0);
    Vec3 right = normalize(project(up, forward));
    Vec3 top = normalize(project(forward, right)); /* up, orthogonalized */

    double ndc_x = (2.0 * fx - 1.0) * aspect * tan_half_fov;
    double ndc_y = (1.0 - 2.0 * fy) * tan_half_fov;
    Vec3 dir = add(add(mul(right, ndc_x), mul(top, ndc_y)), forward);
    return normalize(dir);
}

/* ------------------------------------------------------------------ */
/* Procedural sky                                                      */
/* ------------------------------------------------------------------ */

static double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

static Vec3 sky(Vec3 d) {
    Vec3 horizon = v3(1.0, 0.25, 0.03);
    Vec3 nadir = v3(0.18, 0.01, 0.015);
    double t = pow(clamp01(fabs(d.y)), 0.65);
    /* horizon + t * (nadir - horizon) */
    return v3(horizon.x + t * (nadir.x - horizon.x),
              horizon.y + t * (nadir.y - horizon.y),
              horizon.z + t * (nadir.z - horizon.z));
}

/* ------------------------------------------------------------------ */
/* Intersections                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    int hit;
    double dist;
    Vec3 normal;
    int is_sphere;
    int glass;
} Hit;

/* Ray-sphere: returns nearest positive root, considering both roots so
 * rays starting inside the sphere still exit through the second one. */
static int ray_sphere(Vec3 o, Vec3 d, Sphere s, double *t_out, Vec3 *n_out) {
    Vec3 oc = sub(o, s.center);
    double a = dot(d, d);
    double b = 2.0 * dot(oc, d);
    double c = dot(oc, oc) - s.radius * s.radius;
    double disc = b * b - 4.0 * a * c;
    if (disc < 0.0) return 0;

    double sq = sqrt(disc);
    double t0 = (-b - sq) / (2.0 * a);
    double t1 = (-b + sq) / (2.0 * a);
    double t = t0;
    if (t0 < 1e-6) {
        if (t1 < 1e-6) return 0;
        t = t1; /* ray starts inside the sphere */
    }
    *t_out = t;
    Vec3 p = add(o, mul(d, t));
    Vec3 n = normalize(sub(p, s.center));
    /* if entering from outside, surface normal points away from origin */
    if (dot(oc, d) > 0.0) n = mul(n, -1.0);
    *n_out = n;
    return 1;
}

/* Infinite plane at y = 0, normal +Y. */
static int ray_plane(Vec3 o, Vec3 d, double *t_out) {
    if (fabs(d.y) < 1e-9) return 0; /* parallel: no hit */
    double t = -o.y / d.y;
    if (t < 1e-6) return 0;
    *t_out = t;
    return 1;
}

static Hit intersect_scene(Vec3 o, Vec3 d) {
    Hit best;
    best.hit = 0;
    best.dist = 1e18;

    for (int i = 0; i < 2; i++) {
        double t;
        Vec3 n;
        if (ray_sphere(o, d, spheres[i], &t, &n) && t > 1e-6 && t < best.dist) {
            best.dist = t;
            best.normal = n;
            best.is_sphere = 1;
            best.glass = spheres[i].glass;
        }
    }

    double tp;
    if (ray_plane(o, d, &tp) && tp > 1e-6 && tp < best.dist) {
        best.dist = tp;
        best.normal = v3(0.0, 1.0, 0.0);
        best.is_sphere = 0;
        best.glass = 0;
    }

    if (best.dist < 1e17) best.hit = 1;
    return best;
}

/* ------------------------------------------------------------------ */
/* Shading and ray tracing                                             */
/* ------------------------------------------------------------------ */

static Vec3 checker(Vec3 p) {
    int cx = (int)floor(p.x);
    int cz = (int)floor(p.z);
    return ((cx + cz) & 1) ? v3(1.0, 1.0, 1.0) : v3(0.0, 0.0, 0.0);
}

/* Schlick's approximation for Fresnel reflectance. */
static double fresnel(Vec3 n, Vec3 d, double n1, double n2) {
    double r0 = (n1 - n2) / (n1 + n2);
    r0 *= r0;
    double cosT = -dot(n, d);
    if (cosT > 1.0) cosT = 1.0;
    if (cosT < 0.0) cosT = 0.0;
    return r0 + (1.0 - r0) * pow(1.0 - cosT, 5.0);
}

static Vec3 reflect(Vec3 d, Vec3 n) {
    return sub(d, mul(n, 2.0 * dot(d, n)));
}

/* Check whether a shadow ray toward the light is blocked. Glass blocks
 * partially (simulated neutral attenuation); chrome blocks fully. */
static int shadowed(Vec3 o, Vec3 l, double light_dist) {
    Vec3 d = normalize(l);
    for (int i = 0; i < 2; i++) {
        double t;
        Vec3 n;
        if (ray_sphere(o, d, spheres[i], &t, &n) && t > 1e-4 && t < light_dist) {
            if (!spheres[i].glass) return 1; /* opaque chrome blocks light */
            /* glass blocks most light, transmitting a small neutral amount */
            return dot(spheres[i].center, d) <= 0.0;
        }
    }
    return 0;
}

#define MAX_DEPTH 8

static Vec3 shade(Vec3 p, Vec3 d, int depth, Hit h);

static Vec3 trace(Vec3 o, Vec3 d, int depth) {
    Hit h = intersect_scene(o, d);
    if (!h.hit) return sky(d); /* miss: sky gradient */
    Vec3 surf = shade(add(o, mul(d, h.dist)), d, depth, h);
    /* Distance fog: blend the shaded surface toward the sky color of this
     * ray direction, once per finite segment. */
    double trans = exp(-0.035 * h.dist);
    Vec3 fog = sky(d);
    return v3(trans * surf.x + (1.0 - trans) * fog.x,
              trans * surf.y + (1.0 - trans) * fog.y,
              trans * surf.z + (1.0 - trans) * fog.z);
}

/* Direct diffuse+specular lighting for the spheres and plane. */
static Vec3 direct_light(Vec3 p, Vec3 n, Vec3 vdir) {
    Vec3 acc = v3(0.0, 0.0, 0.0);
    for (int i = 0; i < 3; i++) {
        Vec3 l = sub(lights[i].pos, p);
        double linv = sqrt(dot(l, l));
        Vec3 ld = mul(l, 1.0 / linv);
        if (shadowed(p, l, linv - 1e-4)) continue;

        double diff = dot(n, ld);
        if (diff > 0.0) {
            Vec3 lambert = mul(v3(lights[i].strength,
                                  lights[i].strength,
                                  lights[i].strength), diff);
            acc = add(acc, lambert);
        }

        /* Blinn-Phong specular highlight, analytic for point lights. */
        Vec3 h = normalize(add(ld, vdir));
        double ndoth = dot(n, h);
        if (ndoth > 0.0) {
            double spec = pow(ndoth, 64.0) * lights[i].strength;
            acc = add(acc, v3(spec, spec, spec));
        }
    }
    return acc;
}

static Vec3 shade(Vec3 p, Vec3 d, int depth, Hit h) {
    Vec3 n = h.normal;
    Vec3 color = v3(0.0, 0.0, 0.0);
    Vec3 vdir = mul(d, -1.0); /* toward viewer (normalized since d is) */

    if (h.is_sphere && h.glass) {
        /* Dielectric glass: fresnel-weighted reflection + refraction.
         * N is the outward surface normal. cosI = -dot(N, d) is positive
         * when the ray enters the sphere, negative when it exits. */
        double n1, n2, cosI;
        Vec3 N = h.normal;
        double cI = -dot(N, d);
        int entering = cI > 0.0;
        if (entering) { n1 = 1.0; n2 = 1.5; cosI = cI; }
        else          { n1 = 1.5; n2 = 1.0; N = mul(N, -1.0); cosI = -cI; }

        double eta = n1 / n2;
        double sin2T = eta * eta * (1.0 - cosI * cosI);

        Vec3 rdir = reflect(d, N);
        Vec3 ro = add(p, mul(rdir, 1e-4));
        Vec3 refl_col = (depth < MAX_DEPTH) ? trace(ro, rdir, depth + 1) : sky(rdir);

        if (sin2T > 1.0) {
            /* total internal reflection: reflect all energy */
            color = refl_col;
        } else {
            double cosT = sqrt(1.0 - sin2T);
            Vec3 tdir = add(mul(N, eta * cosI - cosT), mul(vdir, eta));
            tdir = normalize(tdir);
            Vec3 to = add(p, mul(tdir, 1e-4));
            Vec3 trans_col = (depth < MAX_DEPTH)
                               ? trace(to, tdir, depth + 1)
                               : sky(tdir);

            double fr = fresnel(N, d, n1, n2);
            color = add(mul(refl_col, fr), mul(trans_col, 1.0 - fr));
        }
    } else if (h.is_sphere) {
        /* Chrome: strong mirror reflection, ~0.95 weight, slight ambient. */
        Vec3 r = reflect(d, n);
        Vec3 ro = add(p, mul(r, 1e-4));
        Vec3 refl_col = (depth < MAX_DEPTH) ? trace(ro, r, depth + 1) : sky(r);
        double kr = 0.95;
        color = mul(refl_col, kr);
        color = add(color, mul(v3(0.05, 0.05, 0.05), 0.2));
    } else {
        /* Checkerboard ground. */
        Vec3 albedo = checker(p);
        Vec3 light = direct_light(p, n, vdir);
        color = vmul(albedo, light);
        color = add(color, mul(albedo, 0.08)); /* small ambient */
    }

    return color;
}

/* ------------------------------------------------------------------ */
/* Tone mapping and TGA output                                         */
/* ------------------------------------------------------------------ */

static unsigned char encode(double c) {
    if (c < 0.0) c = 0.0;
    if (c > 1.0) c = 1.0;
    /* gamma 2.2 */
    return (unsigned char)(pow(c, 1.0 / 2.2) * 255.0 + 0.5);
}

static int write_tga(const unsigned char *pixels) {
    FILE *f = fopen("out.tga", "wb");
    if (!f) {
        fprintf(stderr, "error: cannot open out.tga for writing\n");
        return 0;
    }

    unsigned char header[18] = {0};
    header[2] = 2;                       /* uncompressed true-color */
    header[12] = IMG_W & 0xff;           /* width little-endian */
    header[13] = (IMG_W >> 8) & 0xff;
    header[14] = IMG_H & 0xff;           /* height little-endian */
    header[15] = (IMG_H >> 8) & 0xff;
    header[16] = 24;                     /* 24-bit color, no alpha */
    header[17] = 0x20;                   /* top-left origin */

    if (fwrite(header, 1, 18, f) != 18 ||
        fwrite(pixels, 1, (size_t)IMG_W * IMG_H * 3, f) !=
            (size_t)IMG_W * IMG_H * 3) {
        fprintf(stderr, "error: failed writing out.tga\n");
        fclose(f);
        return 0;
    }
    if (fclose(f) != 0) {
        fprintf(stderr, "error: failed closing out.tga\n");
        return 0;
    }
    return 1;
}

int main(void) {
    unsigned char *pixels =
        (unsigned char*)malloc((size_t)IMG_W * IMG_H * 3);
    if (!pixels) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }

    /* Deterministic 2x2 subpixel grid averaged in linear space. */
    const double ss[4][2] = { {0.25, 0.25}, {0.75, 0.25},
                              {0.25, 0.75}, {0.75, 0.75} };

    for (int y = 0; y < IMG_H; y++) {
        for (int x = 0; x < IMG_W; x++) {
            Vec3 lin = v3(0.0, 0.0, 0.0);
            for (int s = 0; s < 4; s++) {
                Vec3 d = ray_dir(x, y, ss[s][0], ss[s][1]);
                Vec3 c = trace(v3(0.0, 2.6, 6.5), d, 0);
                lin = add(lin, c);
            }
            lin = mul(lin, 0.25);

            size_t idx = ((size_t)y * IMG_W + x) * 3;
            pixels[idx + 0] = encode(lin.z); /* B */
            pixels[idx + 1] = encode(lin.y); /* G */
            pixels[idx + 2] = encode(lin.x); /* R */
        }
    }

    if (!write_tga(pixels)) {
        free(pixels);
        return 1;
    }
    free(pixels);
    printf("wrote out.tga\n");
    return 0;
}
