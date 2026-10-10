#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

// Vector math utilities
typedef struct {
    double x, y, z;
} vec3;

static inline vec3 vec3_add(vec3 a, vec3 b) {
    return (vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline vec3 vec3_sub(vec3 a, vec3 b) {
    return (vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline vec3 vec3_mul(vec3 a, vec3 b) {
    return (vec3){a.x * b.x, a.y * b.y, a.z * b.z};
}

static inline vec3 vec3_scale(double s, vec3 v) {
    return (vec3){s * v.x, s * v.y, s * v.z};
}

static inline double vec3_dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline vec3 vec3_cross(vec3 a, vec3 b) {
    return (vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static inline double vec3_length(vec3 v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline double vec3_length_squared(vec3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static inline vec3 vec3_normalize(vec3 v) {
    double len = vec3_length(v);
    if (len > 1e-10) {
        return vec3_scale(1.0 / len, v);
    }
    return (vec3){0, 0, 0};
}

// Ray structure
typedef struct {
    vec3 origin;
    vec3 direction;
} ray;

// Sphere structure
typedef struct {
    vec3 center;
    double radius;
    int material; // 0 = glass, 1 = chrome
} sphere;

// Material properties
typedef struct {
    double ior; // Index of refraction
    vec3 color;
    double reflectivity;
    double transparency;
} material;

// Scene constants
#define WIDTH 1280
#define HEIGHT 720
#define SAMPLES_PER_PIXEL 4
#define MAX_BOUNCES 8
#define EPSILON 1e-4
#define FOG_DENSITY 0.035

// Sky colors
static const vec3 SKY_DARK_RED = {0.18, 0.01, 0.015};
static const vec3 SKY_ORANGE = {1.0, 0.25, 0.03};

// Lights
static const vec3 LIGHT_KEY_POS = {-3.5, 6.0, 4.5};
static const double LIGHT_KEY_STRENGTH = 1.0;

static const vec3 LIGHT_FILL_POS = {4.0, 3.0, 3.0};
static const double LIGHT_FILL_STRENGTH = 0.35;

static const vec3 LIGHT_BACK_POS = {0.5, 5.0, -4.0};
static const double LIGHT_BACK_STRENGTH = 0.7;

// Camera setup
static const vec3 CAMERA_POS = {0.0, 2.6, 6.5};
static const vec3 CAMERA_TARGET = {0.0, 1.0, 0.0};
static const vec3 CAMERA_UP = {0.0, 1.0, 0.0};
static const double FOV = 45.0; // degrees

// Spheres
static const sphere SPHERE_GLASS = {{-1.25, 1.0, 0.0}, 1.0, 0};
static const sphere SPHERE_CHROME = {{1.25, 1.0, 0.0}, 1.0, 1};

// Materials
static const material MATERIAL_GLASS = {1.5, {1.0, 1.0, 1.0}, 0.0, 1.0};
static const material MATERIAL_CHROME = {1.0, {0.8, 0.8, 0.8}, 0.95, 0.0};

// Ray-sphere intersection
int intersect_sphere(ray r, sphere s, double *t) {
    vec3 oc = vec3_sub(r.origin, s.center);
    double a = vec3_dot(r.direction, r.direction);
    double b = 2.0 * vec3_dot(oc, r.direction);
    double c = vec3_dot(oc, oc) - s.radius * s.radius;
    double discriminant = b * b - 4 * a * c;

    if (discriminant < 0) {
        return 0;
    }

    double sqrt_discriminant = sqrt(discriminant);
    double t1 = (-b - sqrt_discriminant) / (2.0 * a);
    double t2 = (-b + sqrt_discriminant) / (2.0 * a);

    // Return the closest positive intersection
    if (t1 > EPSILON) {
        *t = t1;
        return 1;
    } else if (t2 > EPSILON) {
        *t = t2;
        return 1;
    }

    return 0;
}

// Ray-plane intersection
int intersect_plane(ray r, vec3 point, vec3 normal, double *t) {
    double denom = vec3_dot(normal, r.direction);
    if (fabs(denom) < 1e-6) {
        return 0; // Ray is parallel to plane
    }

    vec3 p0l0 = vec3_sub(point, r.origin);
    *t = vec3_dot(p0l0, normal) / denom;

    return *t > EPSILON ? 1 : 0;
}

// Compute checkerboard pattern
vec3 get_checkerboard_color(vec3 point) {
    int x = (int)floor(point.x);
    int z = (int)floor(point.z);
    if ((x + z) % 2 == 0) {
        return (vec3){1.0, 1.0, 1.0}; // White
    } else {
        return (vec3){0.0, 0.0, 0.0}; // Black
    }
}

// Sky color evaluation
vec3 sky_color(vec3 direction) {
    double t = pow(fabs(direction.y), 0.65);
    t = fmax(0.0, fmin(1.0, t)); // Clamp to [0,1]
    return vec3_add(
        vec3_scale(1.0 - t, SKY_ORANGE),
        vec3_scale(t, SKY_DARK_RED)
    );
}

// Compute Fresnel coefficient using Schlick's approximation
double fresnel(double cos_theta, double eta_ratio) {
    double r0 = (1.0 - eta_ratio) / (1.0 + eta_ratio);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow(1.0 - cos_theta, 5.0);
}

// Reflect a vector around a normal
vec3 reflect(vec3 incident, vec3 normal) {
    return vec3_sub(incident, vec3_scale(2.0 * vec3_dot(incident, normal), normal));
}

// Refract a vector through a surface
vec3 refract(vec3 incident, vec3 normal, double eta_ratio) {
    double cos_i = vec3_dot(incident, normal);
    double k = 1.0 - eta_ratio * eta_ratio * (1.0 - cos_i * cos_i);

    if (k < 0.0) {
        // Total internal reflection
        return reflect(incident, normal);
    }

    vec3 refracted = vec3_sub(
        vec3_scale(eta_ratio, incident),
        vec3_scale(eta_ratio * cos_i + sqrt(k), normal)
    );

    return vec3_normalize(refracted);
}

// Trace a ray through the scene
vec3 trace_ray(ray r, int depth) {
    if (depth >= MAX_BOUNCES) {
        return sky_color(r.direction);
    }

    double closest_t = 1e20;
    int hit_sphere = -1; // -1 = no hit, 0 = glass, 1 = chrome
    vec3 hit_point, hit_normal;
    material hit_material;
    
    // Check intersection with spheres
    double t;
    if (intersect_sphere(r, SPHERE_GLASS, &t) && t < closest_t) {
        closest_t = t;
        hit_sphere = 0;
        hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
        hit_normal = vec3_normalize(vec3_sub(hit_point, SPHERE_GLASS.center));
        hit_material = MATERIAL_GLASS;
    }
    
    if (intersect_sphere(r, SPHERE_CHROME, &t) && t < closest_t) {
        closest_t = t;
        hit_sphere = 1;
        hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
        hit_normal = vec3_normalize(vec3_sub(hit_point, SPHERE_CHROME.center));
        hit_material = MATERIAL_CHROME;
    }

    // Check intersection with ground plane
    if (intersect_plane(r, (vec3){0.0, 0.0, 0.0}, (vec3){0.0, 1.0, 0.0}, &t) && t < closest_t) {
        closest_t = t;
        hit_sphere = -1; // Ground plane
        hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
        hit_normal = (vec3){0.0, 1.0, 0.0};
        hit_material.color = get_checkerboard_color(hit_point);
        hit_material.reflectivity = 0.0;
        hit_material.transparency = 0.0;
    }

    // No intersection
    if (hit_sphere == -1) {
        return sky_color(r.direction);
    }

    // Shadow ray to lights
    vec3 color = {0.0, 0.0, 0.0};
    vec3 light_dir_key, light_dir_fill, light_dir_back;
    double light_attenuation_key, light_attenuation_fill, light_attenuation_back;

    // Key light
    light_dir_key = vec3_sub(LIGHT_KEY_POS, hit_point);
    light_dir_key = vec3_normalize(light_dir_key);
    light_attenuation_key = 1.0; // No attenuation for simplicity

    // Fill light
    light_dir_fill = vec3_sub(LIGHT_FILL_POS, hit_point);
    light_dir_fill = vec3_normalize(light_dir_fill);
    light_attenuation_fill = 1.0; // No attenuation for simplicity

    // Back light
    light_dir_back = vec3_sub(LIGHT_BACK_POS, hit_point);
    light_dir_back = vec3_normalize(light_dir_back);
    light_attenuation_back = 1.0; // No attenuation for simplicity

    // Diffuse lighting calculation
    double diffuse_key = fmax(0.0, vec3_dot(hit_normal, light_dir_key));
    double diffuse_fill = fmax(0.0, vec3_dot(hit_normal, light_dir_fill));
    double diffuse_back = fmax(0.0, vec3_dot(hit_normal, light_dir_back));

    // Specular lighting calculation
    vec3 view_dir = vec3_normalize(vec3_sub(CAMERA_POS, hit_point));
    vec3 reflect_key = reflect(vec3_scale(-1.0, light_dir_key), hit_normal);
    vec3 reflect_fill = reflect(vec3_scale(-1.0, light_dir_fill), hit_normal);
    vec3 reflect_back = reflect(vec3_scale(-1.0, light_dir_back), hit_normal);

    double specular_key = pow(fmax(0.0, vec3_dot(view_dir, reflect_key)), 32.0);
    double specular_fill = pow(fmax(0.0, vec3_dot(view_dir, reflect_fill)), 32.0);
    double specular_back = pow(fmax(0.0, vec3_dot(view_dir, reflect_back)), 32.0);

    // Combine lighting
    vec3 light_color_key = vec3_scale(LIGHT_KEY_STRENGTH * light_attenuation_key, 
                                      (vec3){1.0, 1.0, 1.0});
    vec3 light_color_fill = vec3_scale(LIGHT_FILL_STRENGTH * light_attenuation_fill, 
                                       (vec3){1.0, 1.0, 1.0});
    vec3 light_color_back = vec3_scale(LIGHT_BACK_STRENGTH * light_attenuation_back, 
                                       (vec3){1.0, 1.0, 1.0});

    vec3 diffuse = vec3_add(
        vec3_add(
            vec3_scale(diffuse_key, light_color_key),
            vec3_scale(diffuse_fill, light_color_fill)
        ),
        vec3_scale(diffuse_back, light_color_back)
    );

    vec3 specular = vec3_add(
        vec3_add(
            vec3_scale(specular_key, light_color_key),
            vec3_scale(specular_fill, light_color_fill)
        ),
        vec3_scale(specular_back, light_color_back)
    );

    // Base color
    vec3 base_color = vec3_mul(hit_material.color, diffuse);

    // Add specular highlights
    base_color = vec3_add(base_color, specular);

    // Handle material-specific effects
    if (hit_sphere == 0) { // Glass sphere
        // For glass, we need to consider reflection and refraction
        vec3 reflected = trace_ray(
            (ray){vec3_add(hit_point, vec3_scale(EPSILON, hit_normal)), 
                  reflect(r.direction, hit_normal)}, 
            depth + 1
        );

        vec3 refracted = trace_ray(
            (ray){vec3_sub(hit_point, vec3_scale(EPSILON, hit_normal)), 
                  refract(r.direction, hit_normal, 1.0 / hit_material.ior)}, 
            depth + 1
        );

        // Calculate Fresnel coefficient
        double cos_theta = vec3_dot(r.direction, hit_normal);
        double eta_ratio = 1.0 / hit_material.ior; // From air to glass
        double kr = fresnel(cos_theta, eta_ratio);

        // Mix reflection and refraction
        color = vec3_add(
            vec3_scale(1.0 - kr, refracted),
            vec3_scale(kr, reflected)
        );
    } else if (hit_sphere == 1) { // Chrome sphere
        // For chrome, we use reflection
        vec3 reflected = trace_ray(
            (ray){vec3_add(hit_point, vec3_scale(EPSILON, hit_normal)), 
                  reflect(r.direction, hit_normal)}, 
            depth + 1
        );
        color = vec3_scale(hit_material.reflectivity, reflected);
    } else { // Ground plane
        color = base_color;
    }

    // Apply fog effect
    double transmittance = exp(-FOG_DENSITY * closest_t);
    vec3 fog_color = sky_color(r.direction);
    color = vec3_add(
        vec3_scale(transmittance, color),
        vec3_scale(1.0 - transmittance, fog_color)
    );

    return color;
}

// Convert RGB to sRGB gamma corrected
uint8_t gamma_correct(double c) {
    if (c <= 0.0031308) {
        return (uint8_t)(c * 12.92 * 255.0 + 0.5);
    } else {
        return (uint8_t)((1.055 * pow(c, 1.0 / 2.4) - 0.055) * 255.0 + 0.5);
    }
}

// Save image as TGA file
int save_tga(const char *filename, uint8_t *pixels, int width, int height) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return 0;
    }

    // TGA Header (18 bytes)
    uint8_t header[18] = {0};
    header[2] = 2; // Image type: uncompressed true-color
    header[12] = width & 0xFF;
    header[13] = (width >> 8) & 0xFF;
    header[14] = height & 0xFF;
    header[15] = (height >> 8) & 0xFF;
    header[16] = 24; // Bits per pixel
    header[17] = 0x20; // Origin: top-left

    if (fwrite(header, 1, 18, fp) != 18) {
        fclose(fp);
        fprintf(stderr, "Error: Failed to write TGA header\n");
        return 0;
    }

    // Write pixel data (BGR format)
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            uint8_t b = pixels[idx];
            uint8_t g = pixels[idx + 1];
            uint8_t r = pixels[idx + 2];
            
            if (fwrite(&b, 1, 1, fp) != 1 || 
                fwrite(&g, 1, 1, fp) != 1 || 
                fwrite(&r, 1, 1, fp) != 1) {
                fclose(fp);
                fprintf(stderr, "Error: Failed to write pixel data\n");
                return 0;
            }
        }
    }

    fclose(fp);
    return 1;
}

int main(void) {
    // Allocate pixel buffer
    uint8_t *pixels = malloc(WIDTH * HEIGHT * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }

    // Setup camera
    vec3 forward = vec3_normalize(vec3_sub(CAMERA_TARGET, CAMERA_POS));
    vec3 right = vec3_normalize(vec3_cross(forward, CAMERA_UP));
    vec3 up = vec3_cross(right, forward);

    // Calculate aspect ratio and fov
    double aspect_ratio = (double)WIDTH / (double)HEIGHT;
    double fov_rad = FOV * M_PI / 180.0;
    double half_height = tan(fov_rad / 2.0);
    double half_width = aspect_ratio * half_height;

    // Render the image
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            vec3 pixel_color = {0.0, 0.0, 0.0};

            // Sample multiple times per pixel for anti-aliasing
            for (int s = 0; s < SAMPLES_PER_PIXEL; s++) {
                // Sub-pixel jittering for anti-aliasing
                double u = (x + (s % 2) * 0.5) / WIDTH;
                double v = (y + (s / 2) * 0.5) / HEIGHT;

                // Calculate ray direction
                vec3 ray_dir = vec3_add(
                    vec3_scale(half_width * (2.0 * u - 1.0), right),
                    vec3_scale(half_height * (1.0 - 2.0 * v), up)
                );
                ray_dir = vec3_add(ray_dir, forward);
                ray_dir = vec3_normalize(ray_dir);

                // Trace ray
                ray r = {CAMERA_POS, ray_dir};
                vec3 color = trace_ray(r, 0);

                // Accumulate color
                pixel_color = vec3_add(pixel_color, color);
            }

            // Average the samples
            pixel_color = vec3_scale(1.0 / SAMPLES_PER_PIXEL, pixel_color);

            // Convert to 8-bit and store
            int idx = (y * WIDTH + x) * 3;
            pixels[idx] = gamma_correct(pixel_color.z);     // B
            pixels[idx + 1] = gamma_correct(pixel_color.y); // G  
            pixels[idx + 2] = gamma_correct(pixel_color.x); // R
        }
    }

    // Save the image
    if (!save_tga("out.tga", pixels, WIDTH, HEIGHT)) {
        free(pixels);
        return 1;
    }

    free(pixels);
    printf("Ray tracer completed successfully. Image saved as out.tga\n");
    return 0;
}
