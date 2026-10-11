/*
 * Standalone ANSI C89 ray tracer.
 * Build: cc -std=c89 -pedantic-errors -Wall -Wextra -O2 main.c -lm -o raytracer
 * Run:   ./raytracer
 * Colors remain linear until the final gamma-2.2 conversion.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#define WIDTH 1280
#define HEIGHT 720
#define SAMPLES_PER_AXIS 4
#define MAX_BOUNCES 12
#define EPSILON 0.0001
#define PI 3.14159265358979323846
#define GLASS 0
#define CHROME 1
#define GROUND 2

typedef struct {
    double x, y, z;
} Vec;

typedef struct {
    Vec origin;
    Vec direction;
    double cone_radius;
    double cone_spread;
} Ray;

typedef struct {
    Vec center;
    double radius;
    int material;
} Sphere;

typedef struct {
    Vec position;
    double strength;
} Light;

typedef struct {
    double distance;
    Vec position;
    Vec outward;
    Vec normal;
    int entering;
    int material;
} Hit;

static const Sphere spheres[2] = {
    {{-1.25, 1.0, 0.0}, 1.0, GLASS},
    {{ 1.25, 1.0, 0.0}, 1.0, CHROME}
};

static const Light lights[3] = {
    {{-3.5, 6.0,  4.5}, 1.0},
    {{ 4.0, 3.0,  3.0}, 0.35},
    {{ 0.5, 5.0, -4.0}, 0.7}
};

static double clamp(double value, double low, double high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static Vec vec(double x, double y, double z)
{
    Vec result;
    result.x = x;
    result.y = y;
    result.z = z;
    return result;
}

static Vec add(Vec a, Vec b)
{
    return vec(a.x + b.x, a.y + b.y, a.z + b.z);
}

static Vec subtract(Vec a, Vec b)
{
    return vec(a.x - b.x, a.y - b.y, a.z - b.z);
}

static Vec scale(Vec a, double s)
{
    return vec(a.x * s, a.y * s, a.z * s);
}

static double dot(Vec a, Vec b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec cross(Vec a, Vec b)
{
    return vec(a.y * b.z - a.z * b.y,
               a.z * b.x - a.x * b.z,
               a.x * b.y - a.y * b.x);
}

static Vec normalize(Vec a)
{
    double length_squared;
    length_squared = dot(a, a);
    if (length_squared < 1.0e-30) return vec(0.0, 1.0, 0.0);
    return scale(a, 1.0 / sqrt(length_squared));
}

static Vec mix(Vec a, Vec b, double amount)
{
    return add(scale(a, 1.0 - amount), scale(b, amount));
}

static Vec reflect(Vec direction, Vec normal)
{
    return normalize(subtract(direction,
                              scale(normal, 2.0 * dot(direction, normal))));
}

static Vec sky(Vec direction)
{
    double t;
    t = pow(clamp(fabs(direction.y), 0.0, 1.0), 0.65);
    return mix(vec(1.0, 0.25, 0.03), vec(0.18, 0.01, 0.015), t);
}

static Vec fog(Vec color, Vec direction, double distance)
{
    double transmittance;
    transmittance = exp(-0.035 * distance);
    return mix(sky(direction), color, transmittance);
}

/* Directions have unit length, so the quadratic's leading term is one.
 * Testing both roots also finds the exit when the origin is inside glass. */
static int intersect_sphere(Ray ray, const Sphere *sphere,
                            double maximum, double *distance)
{
    Vec offset;
    double b, c, discriminant, root, t;
    offset = subtract(ray.origin, sphere->center);
    b = dot(offset, ray.direction);
    c = dot(offset, offset) - sphere->radius * sphere->radius;
    discriminant = b * b - c;
    if (discriminant < 0.0) return 0;
    root = sqrt(discriminant);
    t = -b - root;
    if (t <= EPSILON) t = -b + root;
    if (t <= EPSILON || t >= maximum) return 0;
    *distance = t;
    return 1;
}

static int intersect_plane(Ray ray, double maximum, double *distance)
{
    double t;
    if (fabs(ray.direction.y) < 1.0e-12) return 0;
    t = -ray.origin.y / ray.direction.y;
    if (t <= EPSILON || t >= maximum) return 0;
    *distance = t;
    return 1;
}

static int intersect_scene(Ray ray, Hit *hit)
{
    double nearest, distance;
    int i, material;
    nearest = DBL_MAX;
    material = -1;
    for (i = 0; i < 2; ++i) {
        if (intersect_sphere(ray, &spheres[i], nearest, &distance)) {
            nearest = distance;
            material = spheres[i].material;
        }
    }
    if (intersect_plane(ray, nearest, &distance)) {
        nearest = distance;
        material = GROUND;
    }
    if (material < 0) return 0;
    hit->distance = nearest;
    hit->material = material;
    hit->position = add(ray.origin, scale(ray.direction, nearest));
    if (material == GROUND) {
        hit->outward = vec(0.0, 1.0, 0.0);
    } else {
        hit->outward = normalize(subtract(hit->position,
                                          spheres[material].center));
    }
    hit->entering = dot(ray.direction, hit->outward) < 0.0;
    hit->normal = scale(hit->outward, hit->entering ? 1.0 : -1.0);
    return 1;
}

/* Offset along the geometric normal towards the outgoing ray's medium. */
static Ray surface_ray(const Hit *hit, Vec direction,
                       double cone_radius, double cone_spread)
{
    Ray result;
    double side;
    result.direction = normalize(direction);
    side = dot(result.direction, hit->outward) >= 0.0 ? 1.0 : -1.0;
    result.origin = add(hit->position, scale(hit->outward, side * EPSILON));
    result.cone_radius = cone_radius;
    result.cone_spread = cone_spread;
    return result;
}

static double shadow_transmission(const Hit *hit, Vec light_position)
{
    Ray shadow;
    Vec to_light;
    double light_distance, distance, transmission;
    int i;
    to_light = subtract(light_position, hit->position);
    shadow = surface_ray(hit, to_light, 0.0, 0.0);
    to_light = subtract(light_position, shadow.origin);
    light_distance = sqrt(dot(to_light, to_light));
    if (light_distance <= EPSILON) return 1.0;
    shadow.direction = scale(to_light, 1.0 / light_distance);
    transmission = 1.0;
    for (i = 0; i < 2; ++i) {
        if (intersect_sphere(shadow, &spheres[i],
                             light_distance - EPSILON, &distance)) {
            if (spheres[i].material == CHROME) return 0.0;
            /* Neutral approximation for direct light through the glass.
             * Camera and reflection rays use full dielectric transport. */
            transmission *= 0.72;
        }
    }
    if (intersect_plane(shadow, light_distance - EPSILON, &distance)) {
        return 0.0;
    }
    return transmission;
}

/* Integral of the alternating +1/-1 unit-width square wave. The period
 * has zero integral; floor preserves the same phase at negative positions. */
static double checker_integral(double position)
{
    double phase;
    phase = position - 2.0 * floor(position * 0.5);
    return phase <= 1.0 ? phase : 2.0 - phase;
}

static double checker_average(double position, double radius)
{
    double parity;
    if (radius < 1.0e-7) {
        parity = floor(position);
        return parity - 2.0 * floor(parity * 0.5) < 1.0 ? 1.0 : -1.0;
    }
    if (radius > 1.0e6) return 0.0;
    return clamp((checker_integral(position + radius) -
                  checker_integral(position - radius)) / (2.0 * radius),
                 -1.0, 1.0);
}

static double checker_albedo(Ray ray, const Hit *hit)
{
    double radius, x_radius, z_radius, x_slope, z_slope;
    radius = ray.cone_radius + hit->distance * ray.cone_spread;
    x_slope = ray.direction.x / ray.direction.y;
    z_slope = ray.direction.z / ray.direction.y;
    /* Project the sample's narrow ray cone onto the plane, then integrate
     * the black/white checker over a box. This suppresses distant moire
     * without replacing or clipping the infinite plane. */
    x_radius = radius * sqrt(1.0 + x_slope * x_slope);
    z_radius = radius * sqrt(1.0 + z_slope * z_slope);
    return 0.5 + 0.5 * checker_average(hit->position.x, x_radius) *
                       checker_average(hit->position.z, z_radius);
}

static Vec shade_ground(Ray ray, const Hit *hit)
{
    Vec direction;
    double illumination, cosine, albedo;
    int i;
    illumination = 0.035;
    for (i = 0; i < 3; ++i) {
        direction = normalize(subtract(lights[i].position, hit->position));
        cosine = dot(hit->normal, direction);
        if (cosine > 0.0) {
            illumination += 0.65 * lights[i].strength * cosine *
                            shadow_transmission(hit, lights[i].position);
        }
    }
    albedo = checker_albedo(ray, hit);
    return scale(vec(albedo, albedo, albedo), illumination);
}

static Vec specular_highlights(Ray ray, const Hit *hit,
                               double exponent, double strength)
{
    Vec direction, halfway;
    double cosine, highlight, total;
    int i;
    total = 0.0;
    for (i = 0; i < 3; ++i) {
        direction = normalize(subtract(lights[i].position, hit->position));
        if (dot(hit->normal, direction) <= 0.0) continue;
        halfway = normalize(subtract(direction, ray.direction));
        cosine = clamp(dot(hit->normal, halfway), 0.0, 1.0);
        highlight = strength * lights[i].strength * pow(cosine, exponent);
        if (highlight > 1.0e-10) {
            total += highlight * shadow_transmission(hit, lights[i].position);
        }
    }
    return vec(total, total, total);
}

/* The normal points into the incident medium. At an exit use the
 * transmitted cosine in Schlick's approximation, which tends to one
 * at the critical angle. Total internal reflection takes all energy. */
static int dielectric(Vec incoming, Vec normal, int entering,
                      Vec *transmitted, double *fresnel)
{
    double incident_index, transmitted_index, ratio, cosine;
    double sine_squared, transmitted_cosine, r0, schlick_cosine, m;
    incident_index = entering ? 1.0 : 1.5;
    transmitted_index = entering ? 1.5 : 1.0;
    ratio = incident_index / transmitted_index;
    cosine = clamp(-dot(incoming, normal), 0.0, 1.0);
    sine_squared = ratio * ratio * (1.0 - cosine * cosine);
    if (sine_squared >= 1.0) {
        *fresnel = 1.0;
        return 0;
    }
    transmitted_cosine = sqrt(1.0 - sine_squared);
    *transmitted = normalize(add(scale(incoming, ratio),
                                 scale(normal, ratio * cosine -
                                               transmitted_cosine)));
    r0 = (incident_index - transmitted_index) /
         (incident_index + transmitted_index);
    r0 *= r0;
    schlick_cosine = entering ? cosine : transmitted_cosine;
    m = 1.0 - schlick_cosine;
    *fresnel = r0 + (1.0 - r0) * m * m * m * m * m;
    return 1;
}

static Vec trace(Ray ray, int depth);

static Vec secondary_radiance(Ray ray, int depth)
{
    /* After twelve secondary bounces use environment radiance for the
     * unresolved continuation, rather than introducing black holes. */
    if (depth >= MAX_BOUNCES) return sky(ray.direction);
    return trace(ray, depth + 1);
}

static Vec shade_chrome(Ray ray, const Hit *hit, int depth)
{
    Ray reflected;
    Vec color;
    double radius;
    radius = ray.cone_radius + hit->distance * ray.cone_spread;
    reflected = surface_ray(hit, reflect(ray.direction, hit->normal),
                            radius, ray.cone_spread);
    color = scale(secondary_radiance(reflected, depth), 0.95);
    return add(color, specular_highlights(ray, hit, 280.0, 1.35));
}

static Vec shade_glass(Ray ray, const Hit *hit, int depth)
{
    Ray reflected, refracted;
    Vec reflection, transmission, transmitted, color;
    double radius, fresnel;
    int has_transmission;
    radius = ray.cone_radius + hit->distance * ray.cone_spread;
    reflected = surface_ray(hit, reflect(ray.direction, hit->normal),
                            radius, ray.cone_spread);
    reflection = secondary_radiance(reflected, depth);
    has_transmission = dielectric(ray.direction, hit->normal, hit->entering,
                                  &transmitted, &fresnel);
    color = reflection;
    if (has_transmission) {
        refracted = surface_ray(hit, transmitted, radius, ray.cone_spread);
        transmission = secondary_radiance(refracted, depth);
        color = mix(transmission, reflection, fresnel);
    }
    if (hit->entering) {
        color = add(color, specular_highlights(ray, hit, 320.0, 0.9));
    }
    return color;
}

static Vec trace(Ray ray, int depth)
{
    Hit hit;
    Vec color;
    if (!intersect_scene(ray, &hit)) return sky(ray.direction);
    if (hit.material == GROUND) {
        color = shade_ground(ray, &hit);
    } else if (hit.material == CHROME) {
        color = shade_chrome(ray, &hit, depth);
    } else {
        color = shade_glass(ray, &hit, depth);
    }
    /* Exactly one fog application for this finite segment, including
     * reflected paths and both the inside and outside of the glass. */
    return fog(color, ray.direction, hit.distance);
}

static unsigned char encode_channel(double linear)
{
    double encoded;
    encoded = pow(clamp(linear, 0.0, 1.0), 1.0 / 2.2);
    return (unsigned char)(255.0 * encoded + 0.5);
}

static int write_tga_header(FILE *file)
{
    unsigned char header[18];
    int i;
    for (i = 0; i < 18; ++i) header[i] = 0;
    header[2] = 2;
    header[12] = (unsigned char)(WIDTH & 255);
    header[13] = (unsigned char)((WIDTH >> 8) & 255);
    header[14] = (unsigned char)(HEIGHT & 255);
    header[15] = (unsigned char)((HEIGHT >> 8) & 255);
    header[16] = 24;
    header[17] = 0x20;
    return fwrite(header, 1, sizeof(header), file) == sizeof(header);
}

static int render_tga(FILE *file)
{
    unsigned char row[WIDTH * 3];
    Vec camera, forward, right, up, world_up, color, direction;
    Ray ray;
    double half_height, aspect, screen_x, screen_y, sample_weight;
    int x, y, sx, sy, index;
    camera = vec(0.0, 2.6, 6.5);
    forward = normalize(subtract(vec(0.0, 1.0, 0.0), camera));
    world_up = vec(0.0, 1.0, 0.0);
    right = normalize(cross(forward, world_up));
    up = normalize(cross(right, forward));
    half_height = tan(45.0 * PI / 360.0);
    aspect = (double)WIDTH / (double)HEIGHT;
    sample_weight = 1.0 / (SAMPLES_PER_AXIS * SAMPLES_PER_AXIS);
    ray.origin = camera;
    ray.cone_radius = 0.0;
    ray.cone_spread = half_height / (HEIGHT * SAMPLES_PER_AXIS);
    if (!write_tga_header(file)) return 0;
    for (y = 0; y < HEIGHT; ++y) {
        for (x = 0; x < WIDTH; ++x) {
            color = vec(0.0, 0.0, 0.0);
            for (sy = 0; sy < SAMPLES_PER_AXIS; ++sy) {
                for (sx = 0; sx < SAMPLES_PER_AXIS; ++sx) {
                    screen_x = (2.0 * (x + (sx + 0.5) / SAMPLES_PER_AXIS) /
                                WIDTH - 1.0) * aspect * half_height;
                    screen_y = (1.0 - 2.0 *
                                (y + (sy + 0.5) / SAMPLES_PER_AXIS) /
                                HEIGHT) * half_height;
                    direction = add(forward, add(scale(right, screen_x),
                                                 scale(up, screen_y)));
                    ray.direction = normalize(direction);
                    color = add(color, trace(ray, 0));
                }
            }
            color = scale(color, sample_weight);
            index = x * 3;
            row[index] = encode_channel(color.z);
            row[index + 1] = encode_channel(color.y);
            row[index + 2] = encode_channel(color.x);
        }
        if (fwrite(row, 1, sizeof(row), file) != sizeof(row)) return 0;
        if ((y + 1) % 120 == 0) {
            fprintf(stderr, "Rendered %d / %d rows\n", y + 1, HEIGHT);
        }
    }
    return 1;
}

int main(void)
{
    FILE *file;
    int rendered, closed;
    file = fopen("out.tga", "wb");
    if (file == NULL) {
        perror("Cannot create out.tga");
        return EXIT_FAILURE;
    }
    rendered = render_tga(file);
    if (!rendered) fprintf(stderr, "Failed while writing out.tga\n");
    closed = fclose(file);
    if (closed != 0) perror("Cannot finish writing out.tga");
    if (!rendered || closed != 0) return EXIT_FAILURE;
    fprintf(stderr, "Wrote out.tga (%d x %d, 24-bit BGR, %d samples/pixel)\n",
            WIDTH, HEIGHT, SAMPLES_PER_AXIS * SAMPLES_PER_AXIS);
    return EXIT_SUCCESS;
}
