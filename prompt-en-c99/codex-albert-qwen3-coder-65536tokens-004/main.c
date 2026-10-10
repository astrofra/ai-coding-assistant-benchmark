#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

// Vector operations
typedef struct {
    float x, y, z;
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

static inline vec3 vec3_scale(float s, vec3 v) {
    return (vec3){s * v.x, s * v.y, s * v.z};
}

static inline float vec3_dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline vec3 vec3_cross(vec3 a, vec3 b) {
    return (vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static inline float vec3_length(vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline float vec3_length_squared(vec3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static inline vec3 vec3_normalize(vec3 v) {
    float len = vec3_length(v);
    if (len > 1e-8f) {
        return vec3_scale(1.0f / len, v);
    }
    return (vec3){0.0f, 0.0f, 0.0f};
}

// Ray structure
typedef struct {
    vec3 origin;
    vec3 direction;
} ray;

// Sphere structure
typedef struct {
    vec3 center;
    float radius;
} sphere;

// Plane structure
typedef struct {
    vec3 point;
    vec3 normal;
} plane;

// Material structure
typedef struct {
    vec3 albedo;
    float reflectivity;
    float transparency;
    float ior; // Index of refraction
} material;

// Light structure
typedef struct {
    vec3 position;
    vec3 intensity;
} light;

// Scene constants
#define WIDTH 1280
#define HEIGHT 720
#define SAMPLES 4
#define MAX_BOUNCES 8
#define EPSILON 1e-4f
#define FOG_DENSITY 0.035f

// Sky colors
static const vec3 SKY_DARK_RED = {0.18f, 0.01f, 0.015f};
static const vec3 SKY_ORANGE = {1.0f, 0.25f, 0.03f};

// Materials
static const material GLASS_MATERIAL = {{1.0f, 1.0f, 1.0f}, 0.0f, 1.0f, 1.5f};
static const material CHROME_MATERIAL = {{0.8f, 0.8f, 0.8f}, 0.95f, 0.0f, 1.0f};
static const material CHECKERBOARD_MATERIAL = {{0.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 1.0f};

// Lights
static const light LIGHTS[] = {
    {{-3.5f, 6.0f, 4.5f}, {1.0f, 1.0f, 1.0f}},   // Key light
    {{ 4.0f, 3.0f, 3.0f}, {0.35f, 0.35f, 0.35f}}, // Fill light
    {{ 0.5f, 5.0f, -4.0f}, {0.7f, 0.7f, 0.7f}}    // Back light
};

// Sphere definitions
static const sphere SPHERE_GLASS = {{-1.25f, 1.0f, 0.0f}, 1.0f};
static const sphere SPHERE_CHROME = {{1.25f, 1.0f, 0.0f}, 1.0f};

// Plane definition (ground plane at y = 0)
static const plane GROUND_PLANE = {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};

// Camera setup
static const vec3 CAMERA_POS = {0.0f, 2.6f, 6.5f};
static const vec3 CAMERA_TARGET = {0.0f, 1.0f, 0.0f};
static const vec3 CAMERA_UP = {0.0f, 1.0f, 0.0f};
static const float FOV = 45.0f;

// Sky color calculation
static vec3 sky_color(vec3 ray_dir) {
    // Clamp ray direction Y to [0, 1] for numerical robustness
    float t = powf(fabsf(ray_dir.y), 0.65f);
    t = fmaxf(0.0f, fminf(1.0f, t));
    
    return vec3_add(
        vec3_scale(1.0f - t, SKY_ORANGE),
        vec3_scale(t, SKY_DARK_RED)
    );
}

// Checkerboard pattern
static vec3 checkerboard_pattern(vec3 point) {
    int x = (int)floorf(point.x);
    int z = (int)floorf(point.z);
    
    // Alternate black and white squares
    if ((x + z) % 2 == 0) {
        return (vec3){1.0f, 1.0f, 1.0f}; // White
    } else {
        return (vec3){0.0f, 0.0f, 0.0f}; // Black
    }
}

// Ray-sphere intersection
static int intersect_sphere(ray r, sphere s, float *t) {
    vec3 oc = vec3_sub(r.origin, s.center);
    float a = vec3_dot(r.direction, r.direction);
    float b = 2.0f * vec3_dot(oc, r.direction);
    float c = vec3_dot(oc, oc) - s.radius * s.radius;
    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f) {
        return 0; // No intersection
    }

    float sqrt_discriminant = sqrtf(discriminant);
    float t1 = (-b - sqrt_discriminant) / (2.0f * a);
    float t2 = (-b + sqrt_discriminant) / (2.0f * a);

    // Return the closest positive intersection
    if (t1 > EPSILON) {
        *t = t1;
        return 1;
    } else if (t2 > EPSILON) {
        *t = t2;
        return 1;
    }

    return 0; // Intersection behind ray origin
}

// Ray-plane intersection
static int intersect_plane(ray r, plane p, float *t) {
    float denom = vec3_dot(p.normal, r.direction);
    
    if (fabsf(denom) < 1e-6f) {
        return 0; // Ray is parallel to plane
    }

    vec3 op = vec3_sub(p.point, r.origin);
    *t = vec3_dot(op, p.normal) / denom;

    if (*t > EPSILON) {
        return 1;
    }

    return 0; // Intersection behind ray origin
}

// Fresnel reflection coefficient using Schlick's approximation
static float fresnel(float cos_theta, float eta_ratio) {
    float r0 = (1.0f - eta_ratio) / (1.0f + eta_ratio);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * powf(1.0f - cos_theta, 5.0f);
}

// Refract ray using Snell's law
static vec3 refract(vec3 incident, vec3 normal, float eta_ratio) {
    float cos_i = vec3_dot(normal, incident);
    if (cos_i > 0.0f) {
        // Ray entering the material
        normal = vec3_scale(-1.0f, normal);
        cos_i = -cos_i;
    }

    float k = 1.0f - eta_ratio * eta_ratio * (1.0f - cos_i * cos_i);

    if (k < 0.0f) {
        // Total internal reflection
        return vec3_scale(-1.0f, incident);
    }

    return vec3_add(
        vec3_scale(eta_ratio, incident),
        vec3_scale(eta_ratio * cos_i - sqrtf(k), normal)
    );
}

// Trace a ray through the scene
static vec3 trace_ray(ray r, int depth) {
    if (depth >= MAX_BOUNCES) {
        return sky_color(r.direction);
    }

    float closest_t = 1e30f;
    vec3 hit_point = {0.0f, 0.0f, 0.0f};
    vec3 normal = {0.0f, 0.0f, 0.0f};
    material mat = {{0.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 0.0f};
    int hit_ground = 0;
    int hit_glass = 0;
    int hit_chrome = 0;

    // Intersect with spheres
    float t;
    if (intersect_sphere(r, SPHERE_GLASS, &t) && t < closest_t) {
        closest_t = t;
        hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
        normal = vec3_normalize(vec3_sub(hit_point, SPHERE_GLASS.center));
        mat = GLASS_MATERIAL;
        hit_glass = 1;
    }

    if (intersect_sphere(r, SPHERE_CHROME, &t) && t < closest_t) {
        closest_t = t;
        hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
        normal = vec3_normalize(vec3_sub(hit_point, SPHERE_CHROME.center));
        mat = CHROME_MATERIAL;
        hit_chrome = 1;
    }

    // Intersect with ground plane
    if (intersect_plane(r, GROUND_PLANE, &t) && t < closest_t) {
        closest_t = t;
        hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
        normal = GROUND_PLANE.normal;
        hit_ground = 1;

        // Apply checkerboard pattern
        vec3 albedo = checkerboard_pattern(hit_point);
        mat.albedo = albedo;
        mat.reflectivity = 0.0f;
        mat.transparency = 0.0f;
        mat.ior = 1.0f;
    }

    // No intersection
    if (closest_t > 1e20f) {
        return sky_color(r.direction);
    }

    // Calculate lighting
    vec3 color = {0.0f, 0.0f, 0.0f};

    if (hit_ground) {
        // Ground plane lighting
        color = mat.albedo;
    } else if (hit_glass || hit_chrome) {
        // For spheres, we'll compute lighting based on material
        // We'll use a simple Lambertian diffuse + Phong specular model
        vec3 light_color = {0.0f, 0.0f, 0.0f};

        // Ambient light
        light_color = vec3_add(light_color, vec3_scale(0.1f, mat.albedo));

        // Direct lighting from each light
        for (int i = 0; i < 3; i++) {
            vec3 light_dir = vec3_normalize(vec3_sub(LIGHTS[i].position, hit_point));
            float light_distance = vec3_length(vec3_sub(LIGHTS[i].position, hit_point));
            
            // Shadow ray
            ray shadow_ray = {vec3_add(hit_point, vec3_scale(EPSILON, normal)), light_dir};
            float shadow_t;
            int in_shadow = 0;
            
            // Check if shadow ray intersects any object
            if (intersect_sphere(shadow_ray, SPHERE_GLASS, &shadow_t) && shadow_t < light_distance) {
                in_shadow = 1;
            }
            if (!in_shadow && intersect_sphere(shadow_ray, SPHERE_CHROME, &shadow_t) && shadow_t < light_distance) {
                in_shadow = 1;
            }
            if (!in_shadow && intersect_plane(shadow_ray, GROUND_PLANE, &shadow_t) && shadow_t < light_distance) {
                in_shadow = 1;
            }

            if (!in_shadow) {
                // Diffuse lighting
                float NdotL = vec3_dot(normal, light_dir);
                if (NdotL > 0.0f) {
                    light_color = vec3_add(light_color, vec3_scale(NdotL, mat.albedo));
                }

                // Specular lighting (Phong)
                vec3 view_dir = vec3_normalize(vec3_scale(-1.0f, r.direction));
                vec3 reflect_dir = vec3_normalize(vec3_sub(vec3_scale(2.0f * NdotL, normal), light_dir));
                float RdotV = vec3_dot(reflect_dir, view_dir);
                if (RdotV > 0.0f) {
                    float spec = powf(RdotV, 32.0f);
                    if (hit_chrome) {
                        // Chrome has stronger specular highlights
                        light_color = vec3_add(light_color, vec3_scale(spec * 0.8f, LIGHTS[i].intensity));
                    } else {
                        // Glass has weaker specular highlights
                        light_color = vec3_add(light_color, vec3_scale(spec * 0.2f, LIGHTS[i].intensity));
                    }
                }
            }
        }

        // Apply light color
        color = light_color;
    }

    // Handle reflection/refraction
    if (mat.reflectivity > 0.0f || mat.transparency > 0.0f) {
        vec3 reflect_color = {0.0f, 0.0f, 0.0f};
        vec3 refract_color = {0.0f, 0.0f, 0.0f};

        // Reflection
        if (mat.reflectivity > 0.0f) {
            vec3 reflect_dir = vec3_normalize(vec3_sub(r.direction, vec3_scale(2.0f * vec3_dot(normal, r.direction), normal)));
            ray reflect_ray = {vec3_add(hit_point, vec3_scale(EPSILON, normal)), reflect_dir};
            reflect_color = trace_ray(reflect_ray, depth + 1);
        }

        // Refraction (only for glass)
        if (mat.transparency > 0.0f && mat.ior != 1.0f) {
            float eta_ratio = 1.0f / mat.ior; // Air to glass
            vec3 refract_dir = refract(r.direction, normal, eta_ratio);
            
            // Check if we're entering or exiting the sphere
            if (vec3_dot(normal, r.direction) > 0.0f) {
                // Exiting the sphere - flip normal
                normal = vec3_scale(-1.0f, normal);
                eta_ratio = mat.ior; // Glass to air
            }
            
            // Calculate refraction direction
            refract_dir = refract(r.direction, normal, eta_ratio);
            
            // Trace through the glass sphere
            if (vec3_length_squared(refract_dir) > 1e-6f) {
                ray refract_ray = {vec3_add(hit_point, vec3_scale(EPSILON, normal)), refract_dir};
                refract_color = trace_ray(refract_ray, depth + 1);
            }
        }

        // Combine reflection and refraction
        if (hit_glass) {
            // For glass, blend reflection and refraction
            float fresnel_factor = fresnel(vec3_dot(normal, r.direction), mat.ior);
            color = vec3_add(
                vec3_scale(1.0f - fresnel_factor, refract_color),
                vec3_scale(fresnel_factor, reflect_color)
            );
        } else if (hit_chrome) {
            // For chrome, mostly reflect
            color = vec3_add(
                vec3_scale(mat.reflectivity, reflect_color),
                vec3_scale(1.0f - mat.reflectivity, color)
            );
        }
    }

    // Apply fog
    float transmittance = expf(-FOG_DENSITY * closest_t);
    vec3 fog_color = sky_color(r.direction);
    color = vec3_add(
        vec3_scale(transmittance, color),
        vec3_scale(1.0f - transmittance, fog_color)
    );

    return color;
}

// Convert RGB to sRGB gamma corrected
static uint8_t gamma_correct(float c) {
    if (c <= 0.0031308f) {
        return (uint8_t)(c * 12.92f * 255.0f + 0.5f);
    } else {
        return (uint8_t)((1.055f * powf(c, 1.0f / 2.4f) - 0.055f) * 255.0f + 0.5f);
    }
}

// Save image as TGA file
static int save_tga(const char *filename, uint8_t *pixels, int width, int height) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open %s for writing\n", filename);
        return 0;
    }

    // TGA Header (18 bytes)
    uint8_t header[18] = {
        0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        width & 0xFF, (width >> 8) & 0xFF,
        height & 0xFF, (height >> 8) & 0xFF,
        24, 0x20  // 24-bit, top-left origin
    };

    if (fwrite(header, 1, 18, fp) != 18) {
        fclose(fp);
        fprintf(stderr, "Error: Failed to write TGA header\n");
        return 0;
    }

    // Write pixel data (BGR format, top to bottom)
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
    // Allocate memory for image
    uint8_t *pixels = malloc(WIDTH * HEIGHT * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Cannot allocate memory for image\n");
        return 1;
    }

    // Set up camera
    vec3 forward = vec3_normalize(vec3_sub(CAMERA_TARGET, CAMERA_POS));
    vec3 right = vec3_normalize(vec3_cross(forward, CAMERA_UP));
    vec3 up = vec3_cross(right, forward);

    // Calculate aspect ratio and field of view
    float aspect_ratio = (float)WIDTH / (float)HEIGHT;
    float fov_rad = FOV * M_PI / 180.0f;
    float half_height = tanf(fov_rad / 2.0f);
    float half_width = half_height * aspect_ratio;

    // Render the image
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            vec3 pixel_color = {0.0f, 0.0f, 0.0f};

            // Sample multiple times per pixel for anti-aliasing
            for (int s = 0; s < SAMPLES; s++) {
                // Calculate sub-pixel position
                float u = (x + (s % 2) * 0.5f) / WIDTH;
                float v = (y + (s / 2) * 0.5f) / HEIGHT;

                // Adjust for aspect ratio and field of view
                vec3 ray_dir = vec3_add(
                    vec3_scale(half_width * (2.0f * u - 1.0f), right),
                    vec3_add(
                        vec3_scale(half_height * (1.0f - 2.0f * v), up),
                        forward
                    )
                );
                ray_dir = vec3_normalize(ray_dir);

                ray r = {CAMERA_POS, ray_dir};
                vec3 color = trace_ray(r, 0);

                // Accumulate color
                pixel_color = vec3_add(pixel_color, color);
            }

            // Average the samples
            pixel_color = vec3_scale(1.0f / SAMPLES, pixel_color);

            // Gamma correct and store pixel
            int idx = (y * WIDTH + x) * 3;
            pixels[idx] = gamma_correct(pixel_color.z);     // Blue
            pixels[idx + 1] = gamma_correct(pixel_color.y); // Green
            pixels[idx + 2] = gamma_correct(pixel_color.x); // Red
        }
    }

    // Save the image
    if (!save_tga("out.tga", pixels, WIDTH, HEIGHT)) {
        free(pixels);
        return 1;
    }

    free(pixels);
    printf("Raytracer completed successfully. Image saved as out.tga\n");
    return 0;
}
