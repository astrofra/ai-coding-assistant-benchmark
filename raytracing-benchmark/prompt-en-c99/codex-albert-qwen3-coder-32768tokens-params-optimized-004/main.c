#define _CRT_SECURE_NO_WARNINGS
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

static inline vec3 vec3_scale(float s, vec3 a) {
    return (vec3){s * a.x, s * a.y, s * a.z};
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

static inline float vec3_length(vec3 a) {
    return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
}

static inline float vec3_length_squared(vec3 a) {
    return a.x * a.x + a.y * a.y + a.z * a.z;
}

static inline vec3 vec3_normalize(vec3 a) {
    float len = vec3_length(a);
    if (len > 1e-8f) {
        return vec3_scale(1.0f / len, a);
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
    int material; // 0 = glass, 1 = chrome
} sphere;

// Plane structure
typedef struct {
    vec3 point;
    vec3 normal;
    float albedo; // 0.0 = black, 1.0 = white
} plane;

// Light structure
typedef struct {
    vec3 position;
    float intensity;
} light;

// Constants
#define WIDTH 1280
#define HEIGHT 720
#define SAMPLES_PER_PIXEL 4
#define MAX_BOUNCES 8
#define EPSILON 1e-4f
#define FOG_DENSITY 0.035f

// Material properties
#define GLASS_IOR 1.5f
#define CHROME_REFLECTION_WEIGHT 0.95f

// Sky colors
static const vec3 SKY_DARK_RED = {0.18f, 0.01f, 0.015f};
static const vec3 SKY_ORANGE = {1.0f, 0.25f, 0.03f};

// Scene objects
sphere spheres[] = {
    {{-1.25f, 1.0f, 0.0f}, 1.0f, 0}, // Glass sphere
    {{1.25f, 1.0f, 0.0f}, 1.0f, 1}   // Chrome sphere
};

plane ground = {
    {0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    0.0f // Will be set dynamically
};

light lights[] = {
    {{-3.5f, 6.0f, 4.5f}, 1.0f},     // Key light
    {{4.0f, 3.0f, 3.0f}, 0.35f},     // Fill light
    {{0.5f, 5.0f, -4.0f}, 0.7f}      // Back light
};

// Sky color function
vec3 sky_color(vec3 ray_dir) {
    // Clamp ray direction Y to [0, 1] for numerical robustness
    float y = fmaxf(0.0f, fminf(1.0f, fabsf(ray_dir.y)));
    float t = powf(y, 0.65f);
    return vec3_add(vec3_scale(1.0f - t, SKY_ORANGE),
                    vec3_scale(t, SKY_DARK_RED));
}

// Checkerboard pattern
float checkerboard_pattern(vec3 point) {
    int x = (int)floorf(point.x);
    int z = (int)floorf(point.z);
    return ((x + z) & 1) ? 1.0f : 0.0f;
}

// Ray-sphere intersection
int intersect_sphere(ray r, sphere s, float *t) {
    vec3 oc = vec3_sub(r.origin, s.center);
    float a = vec3_dot(r.direction, r.direction);
    float b = 2.0f * vec3_dot(oc, r.direction);
    float c = vec3_dot(oc, oc) - s.radius * s.radius;
    float discriminant = b * b - 4 * a * c;

    if (discriminant < 0) {
        return 0;
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

    return 0;
}

// Ray-plane intersection
int intersect_plane(ray r, plane p, float *t) {
    float denom = vec3_dot(p.normal, r.direction);
    if (fabsf(denom) < EPSILON) {
        return 0; // Ray is parallel to the plane
    }

    vec3 op = vec3_sub(p.point, r.origin);
    *t = vec3_dot(op, p.normal) / denom;

    return (*t > EPSILON) ? 1 : 0;
}

// Fresnel reflectance for dielectrics (Schlick's approximation)
float fresnel_dielectric(float cos_theta, float eta) {
    float r0 = (1.0f - eta) / (1.0f + eta);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * powf(1.0f - cos_theta, 5.0f);
}

// Reflect vector
vec3 reflect(vec3 I, vec3 N) {
    return vec3_sub(I, vec3_scale(2.0f * vec3_dot(I, N), N));
}

// Refract vector
vec3 refract(vec3 I, vec3 N, float eta) {
    float k = 1.0f - eta * eta * (1.0f - vec3_dot(I, N) * vec3_dot(I, N));
    if (k < 0.0f) {
        return (vec3){0.0f, 0.0f, 0.0f}; // Total internal reflection
    }
    return vec3_sub(vec3_scale(eta, I), vec3_scale(eta * vec3_dot(I, N) + sqrtf(k), N));
}

// Shade a point with lighting
vec3 shade_point(vec3 point, vec3 normal, int material, float albedo) {
    vec3 color = {0.0f, 0.0f, 0.0f};

    // Ambient light
    color = vec3_add(color, vec3_scale(0.1f, (vec3){albedo, albedo, albedo}));

    for (int i = 0; i < 3; i++) {
        light l = lights[i];
        vec3 light_dir = vec3_normalize(vec3_sub(l.position, point));
        float light_distance = vec3_length(vec3_sub(l.position, point));

        // Shadow check
        ray shadow_ray = {point, light_dir};
        float shadow_t;
        int in_shadow = 0;

        // Check if we hit something before reaching the light
        for (int s = 0; s < 2; s++) {
            if (intersect_sphere(shadow_ray, spheres[s], &shadow_t) && shadow_t < light_distance - EPSILON) {
                in_shadow = 1;
                break;
            }
        }

        if (!in_shadow) {
            // Diffuse lighting
            float NdotL = vec3_dot(normal, light_dir);
            if (NdotL > 0) {
                color = vec3_add(color, vec3_scale(NdotL * l.intensity, (vec3){albedo, albedo, albedo}));
            }

            // Specular lighting (Phong)
            if (material == 1) { // Chrome sphere
                vec3 view_dir = vec3_normalize(vec3_scale(-1.0f, point)); // Simplified view direction
                vec3 reflect_dir = reflect(vec3_scale(-1.0f, light_dir), normal);
                float RdotV = vec3_dot(reflect_dir, view_dir);
                if (RdotV > 0) {
                    float spec = powf(RdotV, 32.0f);
                    color = vec3_add(color, vec3_scale(spec * l.intensity, (vec3){1.0f, 1.0f, 1.0f}));
                }
            }
        }
    }

    return color;
}

// Trace a ray recursively
vec3 trace_ray(ray r, int depth) {
    if (depth >= MAX_BOUNCES) {
        return sky_color(r.direction);
    }

    float closest_t = 1e20f;
    int hit_sphere = -1;
    int hit_plane = 0;
    float plane_t = 0.0f;
    vec3 hit_point = {0.0f, 0.0f, 0.0f};
    vec3 hit_normal = {0.0f, 0.0f, 0.0f};
    int hit_material = -1;
    float hit_albedo = 0.0f;

    // Test intersection with spheres
    for (int i = 0; i < 2; i++) {
        float t;
        if (intersect_sphere(r, spheres[i], &t) && t < closest_t) {
            closest_t = t;
            hit_sphere = i;
            hit_material = spheres[i].material;
        }
    }

    // Test intersection with ground plane
    if (intersect_plane(r, (plane){ground.point, ground.normal, 0.0f}, &plane_t)) {
        if (plane_t < closest_t) {
            closest_t = plane_t;
            hit_plane = 1;
            hit_material = -1; // Ground plane
            hit_point = vec3_add(r.origin, vec3_scale(plane_t, r.direction));
            hit_normal = ground.normal;
            hit_albedo = checkerboard_pattern(hit_point);
        }
    }

    // If we hit something
    if (hit_sphere >= 0 || hit_plane) {
        vec3 point = vec3_add(r.origin, vec3_scale(closest_t, r.direction));

        // Calculate normal
        vec3 normal;
        if (hit_sphere >= 0) {
            normal = vec3_normalize(vec3_sub(point, spheres[hit_sphere].center));
            // Ensure normal points outward
            if (vec3_dot(normal, r.direction) > 0) {
                normal = vec3_scale(-1.0f, normal);
            }
        } else {
            normal = hit_normal;
        }

        // For ground plane, we need to compute the proper normal
        if (hit_plane) {
            normal = ground.normal;
        }

        // Handle material properties
        if (hit_material == 0) { // Glass sphere
            vec3 reflect_dir = reflect(r.direction, normal);
            vec3 refract_dir;
            
            // Determine if ray is entering or exiting the sphere
            float cos_i = vec3_dot(normal, r.direction);
            float eta = (cos_i > 0) ? GLASS_IOR : 1.0f / GLASS_IOR;
            float cos_t2 = 1.0f - eta * eta * (1.0f - cos_i * cos_i);
            
            if (cos_t2 >= 0) {
                // Refraction occurs
                refract_dir = refract(r.direction, normal, eta);
                
                // Reflect and refract rays
                float fresnel = fresnel_dielectric(fabsf(cos_i), eta);
                
                // Trace reflection ray
                ray reflect_ray = {vec3_add(point, vec3_scale(EPSILON, normal)), reflect_dir};
                vec3 reflect_color = trace_ray(reflect_ray, depth + 1);
                
                // Trace refraction ray
                ray refract_ray = {vec3_add(point, vec3_scale(-EPSILON, normal)), refract_dir};
                vec3 refract_color = trace_ray(refract_ray, depth + 1);
                
                // Combine reflection and refraction
                vec3 result = vec3_add(vec3_scale(fresnel, reflect_color),
                                       vec3_scale(1.0f - fresnel, refract_color));
                
                // Apply fog effect
                float transmittance = expf(-FOG_DENSITY * closest_t);
                vec3 fog_color = sky_color(r.direction);
                return vec3_add(vec3_scale(transmittance, result),
                                vec3_scale(1.0f - transmittance, fog_color));
            } else {
                // Total internal reflection
                ray reflect_ray = {vec3_add(point, vec3_scale(EPSILON, normal)), reflect_dir};
                return trace_ray(reflect_ray, depth + 1);
            }
        } else if (hit_material == 1) { // Chrome sphere
            vec3 reflect_dir = reflect(r.direction, normal);
            ray reflect_ray = {vec3_add(point, vec3_scale(EPSILON, normal)), reflect_dir};
            vec3 reflect_color = trace_ray(reflect_ray, depth + 1);
            
            // Apply chrome material properties
            vec3 base_color = shade_point(point, normal, hit_material, 0.8f); // Approximate chrome color
            vec3 result = vec3_add(vec3_scale(1.0f - CHROME_REFLECTION_WEIGHT, base_color),
                                   vec3_scale(CHROME_REFLECTION_WEIGHT, reflect_color));
            
            // Apply fog effect
            float transmittance = expf(-FOG_DENSITY * closest_t);
            vec3 fog_color = sky_color(r.direction);
            return vec3_add(vec3_scale(transmittance, result),
                            vec3_scale(1.0f - transmittance, fog_color));
        } else { // Ground plane
            vec3 base_color = shade_point(point, normal, hit_material, hit_albedo);
            
            // Apply fog effect
            float transmittance = expf(-FOG_DENSITY * closest_t);
            vec3 fog_color = sky_color(r.direction);
            return vec3_add(vec3_scale(transmittance, base_color),
                            vec3_scale(1.0f - transmittance, fog_color));
        }
    }

    // No hit - return sky color
    return sky_color(r.direction);
}

// Convert linear RGB to sRGB
uint8_t linear_to_srgb(float c) {
    if (c <= 0.0031308f) {
        return (uint8_t)(c * 12.92f * 255.0f + 0.5f);
    } else {
        return (uint8_t)((1.055f * powf(c, 1.0f / 2.4f) - 0.055f) * 255.0f + 0.5f);
    }
}

// Save TGA file
int save_tga(const char* filename, uint8_t* pixels) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return 0;
    }

    // TGA Header (18 bytes)
    uint8_t header[18] = {0};
    header[2] = 2; // Image type: uncompressed true-color
    header[12] = WIDTH & 0xFF; // Width low byte
    header[13] = (WIDTH >> 8) & 0xFF; // Width high byte
    header[14] = HEIGHT & 0xFF; // Height low byte
    header[15] = (HEIGHT >> 8) & 0xFF; // Height high byte
    header[16] = 24; // Pixel depth
    header[17] = 0x20; // Descriptor: top-left origin, left-to-right pixel order

    if (fwrite(header, 1, 18, file) != 18) {
        fclose(file);
        fprintf(stderr, "Error: Failed to write TGA header\n");
        return 0;
    }

    // Write pixel data (BGR order)
    for (int y = HEIGHT - 1; y >= 0; y--) {
        for (int x = 0; x < WIDTH; x++) {
            int idx = (y * WIDTH + x) * 3;
            uint8_t b = pixels[idx];
            uint8_t g = pixels[idx + 1];
            uint8_t r = pixels[idx + 2];
            if (fwrite(&b, 1, 1, file) != 1 ||
                fwrite(&g, 1, 1, file) != 1 ||
                fwrite(&r, 1, 1, file) != 1) {
                fclose(file);
                fprintf(stderr, "Error: Failed to write pixel data\n");
                return 0;
            }
        }
    }

    fclose(file);
    return 1;
}

int main(void) {
    // Camera setup
    vec3 camera_pos = {0.0f, 2.6f, 6.5f};
    vec3 camera_target = {0.0f, 1.0f, 0.0f};
    vec3 camera_up = {0.0f, 1.0f, 0.0f};

    // Compute camera coordinate system
    vec3 forward = vec3_normalize(vec3_sub(camera_target, camera_pos));
    vec3 right = vec3_normalize(vec3_cross(forward, camera_up));
    vec3 up = vec3_cross(right, forward);

    // Field of view and aspect ratio
    float fov = 45.0f * M_PI / 180.0f;
    float aspect_ratio = (float)WIDTH / (float)HEIGHT;
    float half_height = tanf(fov / 2.0f);
    float half_width = half_height * aspect_ratio;

    // Allocate pixel buffer
    uint8_t* pixels = (uint8_t*)malloc(WIDTH * HEIGHT * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }

    // Render the image
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            vec3 pixel_color = {0.0f, 0.0f, 0.0f};

            // Sub-pixel sampling
            for (int s = 0; s < SAMPLES_PER_PIXEL; s++) {
                // Compute sub-pixel position
                float u = (x + (s % 2) * 0.5f) / WIDTH;
                float v = (y + (s / 2) * 0.5f) / HEIGHT;

                // Compute ray direction
                vec3 ray_dir = vec3_add(vec3_scale(half_width * (2.0f * u - 1.0f), right),
                                        vec3_add(vec3_scale(half_height * (1.0f - 2.0f * v), up),
                                                 forward));
                ray_dir = vec3_normalize(ray_dir);

                // Create ray
                ray r = {camera_pos, ray_dir};

                // Trace ray
                vec3 color = trace_ray(r, 0);

                // Accumulate color (linear RGB)
                pixel_color = vec3_add(pixel_color, color);
            }

            // Average samples and convert to sRGB
            pixel_color = vec3_scale(1.0f / SAMPLES_PER_PIXEL, pixel_color);

            // Convert to sRGB and store in pixel buffer
            int idx = (y * WIDTH + x) * 3;
            pixels[idx] = linear_to_srgb(pixel_color.z);     // Blue
            pixels[idx + 1] = linear_to_srgb(pixel_color.y); // Green
            pixels[idx + 2] = linear_to_srgb(pixel_color.x); // Red
        }
    }

    // Save the image
    if (!save_tga("out.tga", pixels)) {
        free(pixels);
        return 1;
    }

    free(pixels);
    printf("Raytracer completed successfully. Image saved as out.tga\n");
    return 0;
}
