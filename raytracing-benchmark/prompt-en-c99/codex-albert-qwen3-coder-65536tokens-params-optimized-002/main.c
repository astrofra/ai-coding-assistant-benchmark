/*
 * C99 Mini Ray Tracer — Coding Benchmark
 * Implementation for the raytracer task
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

// Vector math utilities
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

// Material properties
typedef enum {
    MATERIAL_LAMBERTIAN,
    MATERIAL_METAL,
    MATERIAL_DIELECTRIC
} material_type;

typedef struct {
    material_type type;
    vec3 albedo;
    float roughness;
    float ior; // index of refraction
} material;

// Sphere structure
typedef struct {
    vec3 center;
    float radius;
    material mat;
} sphere;

// Plane structure
typedef struct {
    vec3 point;
    vec3 normal;
    material mat;
} plane;

// Light structure
typedef struct {
    vec3 position;
    vec3 intensity;
} light;

// Scene definition
static const int WIDTH = 1280;
static const int HEIGHT = 720;
static const int MAX_BOUNCES = 8;
static const int SAMPLES_PER_PIXEL = 4;

// Sky colors
static const vec3 SKY_DARK_RED = {0.18f, 0.01f, 0.015f};
static const vec3 SKY_ORANGE = {1.0f, 0.25f, 0.03f};

// Camera parameters
static const vec3 CAMERA_POS = {0.0f, 2.6f, 6.5f};
static const vec3 CAMERA_TARGET = {0.0f, 1.0f, 0.0f};
static const vec3 CAMERA_UP = {0.0f, 1.0f, 0.0f};
static const float FOV_Y = 45.0f;

// Lights
static const light LIGHTS[] = {
    {{-3.5f, 6.0f, 4.5f}, {1.0f, 1.0f, 1.0f}},   // Key light
    {{4.0f, 3.0f, 3.0f}, {0.35f, 0.35f, 0.35f}},  // Fill light
    {{0.5f, 5.0f, -4.0f}, {0.7f, 0.7f, 0.7f}}     // Back light
};
static const int NUM_LIGHTS = 3;

// Spheres
static const sphere SPHERES[] = {
    // Glass sphere
    {{-1.25f, 1.0f, 0.0f}, 1.0f, 
     {MATERIAL_DIELECTRIC, {1.0f, 1.0f, 1.0f}, 0.0f, 1.5f}},
    // Chrome sphere
    {{1.25f, 1.0f, 0.0f}, 1.0f,
     {MATERIAL_METAL, {0.8f, 0.8f, 0.8f}, 0.02f, 0.0f}}
};
static const int NUM_SPHERES = 2;

// Ground plane
static const plane GROUND_PLANE = {
    {0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {MATERIAL_LAMBERTIAN, {1.0f, 1.0f, 1.0f}, 0.0f, 0.0f}
};

// Ray-sphere intersection
static int intersect_sphere(ray r, sphere s, float *t) {
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
    if (t1 > 1e-4f) {
        *t = t1;
        return 1;
    } else if (t2 > 1e-4f) {
        *t = t2;
        return 1;
    }
    
    return 0;
}

// Ray-plane intersection
static int intersect_plane(ray r, plane p, float *t) {
    float denom = vec3_dot(p.normal, r.direction);
    
    if (fabsf(denom) < 1e-6f) {
        return 0; // Ray is parallel to plane
    }
    
    vec3 op = vec3_sub(p.point, r.origin);
    float t_val = vec3_dot(op, p.normal) / denom;
    
    if (t_val > 1e-4f) {
        *t = t_val;
        return 1;
    }
    
    return 0;
}

// Compute checkerboard pattern
static vec3 checkerboard_pattern(float x, float z) {
    int ix = (int)floorf(x);
    int iz = (int)floorf(z);
    
    // Ensure correct alternating pattern for negative coordinates
    if ((ix + iz) % 2 == 0) {
        return (vec3){1.0f, 1.0f, 1.0f}; // White
    } else {
        return (vec3){0.0f, 0.0f, 0.0f}; // Black
    }
}

// Sky color computation
static vec3 sky_color(vec3 direction) {
    // Clamp y component to [0, 1] for numerical robustness
    float y = fmaxf(0.0f, fminf(1.0f, direction.y));
    float t = powf(fabsf(y), 0.65f);
    
    return vec3_add(
        vec3_scale(1.0f - t, SKY_ORANGE),
        vec3_scale(t, SKY_DARK_RED)
    );
}

// Fresnel reflectance using Schlick's approximation
static float fresnel_reflectance(float cos_theta, float eta_ratio) {
    float r0 = (1.0f - eta_ratio) / (1.0f + eta_ratio);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * powf(1.0f - cos_theta, 5.0f);
}

// Sample a light for shadow calculation
static vec3 sample_light(light l, vec3 hit_point, vec3 normal) {
    vec3 light_dir = vec3_sub(l.position, hit_point);
    float light_distance = vec3_length(light_dir);
    light_dir = vec3_normalize(light_dir);
    
    // Shadow ray
    vec3 shadow_origin = vec3_add(hit_point, vec3_scale(1e-4f, normal));
    ray shadow_ray = {shadow_origin, light_dir};
    
    // Check if point is in shadow
    float t;
    int hit_ground = intersect_plane(shadow_ray, GROUND_PLANE, &t);
    int hit_sphere = 0;
    
    // Check sphere intersections
    for (int i = 0; i < NUM_SPHERES; i++) {
        if (intersect_sphere(shadow_ray, SPHERES[i], &t)) {
            if (t < light_distance) {
                hit_sphere = 1;
                break;
            }
        }
    }
    
    // If in shadow, return black
    if (hit_ground || hit_sphere) {
        return (vec3){0.0f, 0.0f, 0.0f};
    }
    
    // Compute Lambertian diffuse lighting
    float NdotL = fmaxf(0.0f, vec3_dot(normal, light_dir));
    
    return vec3_scale(NdotL, l.intensity);
}

// Trace a ray through the scene
static vec3 trace_ray(ray r, int depth) {
    if (depth >= MAX_BOUNCES) {
        return sky_color(r.direction);
    }
    
    float closest_t = 1e20f;
    int hit_sphere = -1;
    int hit_plane = 0;
    float plane_t;
    vec3 hit_point, hit_normal;
    material hit_material;
    
    // Check sphere intersections
    for (int i = 0; i < NUM_SPHERES; i++) {
        float t;
        if (intersect_sphere(r, SPHERES[i], &t) && t < closest_t) {
            closest_t = t;
            hit_sphere = i;
        }
    }
    
    // Check plane intersection
    if (intersect_plane(r, GROUND_PLANE, &plane_t) && plane_t < closest_t) {
        closest_t = plane_t;
        hit_plane = 1;
    }
    
    // No intersection
    if (closest_t > 1e19f) {
        return sky_color(r.direction);
    }
    
    // Get hit information
    if (hit_plane) {
        hit_point = vec3_add(r.origin, vec3_scale(closest_t, r.direction));
        hit_normal = GROUND_PLANE.normal;
        hit_material = GROUND_PLANE.mat;
        
        // Compute checkerboard pattern at intersection point
        vec3 albedo = checkerboard_pattern(hit_point.x, hit_point.z);
        hit_material.albedo = albedo;
    } else {
        sphere s = SPHERES[hit_sphere];
        hit_point = vec3_add(r.origin, vec3_scale(closest_t, r.direction));
        vec3 to_center = vec3_sub(s.center, hit_point);
        hit_normal = vec3_normalize(to_center);
        hit_material = s.mat;
        
        // For spheres, we need to ensure the normal points outward
        if (vec3_dot(hit_normal, r.direction) > 0) {
            hit_normal = vec3_scale(-1.0f, hit_normal);
        }
    }
    
    // Shading and lighting
    vec3 color = {0.0f, 0.0f, 0.0f};
    
    // Ambient lighting
    color = vec3_add(color, vec3_scale(0.1f, hit_material.albedo));
    
    // Direct lighting from all lights
    for (int i = 0; i < NUM_LIGHTS; i++) {
        vec3 light_contribution = sample_light(LIGHTS[i], hit_point, hit_normal);
        color = vec3_add(color, light_contribution);
    }
    
    // Handle material types
    switch (hit_material.type) {
        case MATERIAL_LAMBERTIAN: {
            // Diffuse material - already computed above
            break;
        }
        case MATERIAL_METAL: {
            // Metal (mirror-like) material
            vec3 reflect_dir = vec3_sub(r.direction, 
                                       vec3_scale(2.0f * vec3_dot(r.direction, hit_normal), hit_normal));
            reflect_dir = vec3_normalize(reflect_dir);
            
            // Offset ray origin to avoid self-intersection
            vec3 reflect_origin = vec3_add(hit_point, vec3_scale(1e-4f, hit_normal));
            ray reflect_ray = {reflect_origin, reflect_dir};
            
            vec3 reflect_color = trace_ray(reflect_ray, depth + 1);
            
            // Mix with base color (metallic)
            color = vec3_add(vec3_scale(0.9f, reflect_color), 
                            vec3_scale(0.1f, hit_material.albedo));
            break;
        }
        case MATERIAL_DIELECTRIC: {
            // Dielectric (glass) material
            float cos_theta = fmaxf(-1.0f, fminf(1.0f, vec3_dot(r.direction, hit_normal)));
            float eta_ratio = 1.0f / hit_material.ior; // Air to glass
            vec3 refract_dir;
            int into = cos_theta < 0; // Ray entering or exiting the sphere

            if (into) {
                cos_theta = -cos_theta;
                eta_ratio = 1.0f / eta_ratio;
                hit_normal = vec3_scale(-1.0f, hit_normal);
            }
            
            // Compute refraction direction
            float k = 1.0f - eta_ratio * eta_ratio * (1.0f - cos_theta * cos_theta);
            
            if (k < 0.0f) {
                // Total internal reflection
                vec3 reflect_dir = vec3_sub(r.direction, 
                                          vec3_scale(2.0f * vec3_dot(r.direction, hit_normal), hit_normal));
                reflect_dir = vec3_normalize(reflect_dir);
                
                vec3 reflect_origin = vec3_add(hit_point, vec3_scale(1e-4f, hit_normal));
                ray reflect_ray = {reflect_origin, reflect_dir};
                vec3 reflect_color = trace_ray(reflect_ray, depth + 1);
                color = reflect_color;
            } else {
                // Refraction
                refract_dir = vec3_sub(vec3_scale(eta_ratio, r.direction),
                                     vec3_scale(eta_ratio * cos_theta - sqrtf(k), hit_normal));
                refract_dir = vec3_normalize(refract_dir);
                
                // Compute Fresnel reflectance
                float reflect_prob = fresnel_reflectance(cos_theta, eta_ratio);
                
                // Trace reflected ray
                vec3 reflect_dir = vec3_sub(r.direction, 
                                          vec3_scale(2.0f * vec3_dot(r.direction, hit_normal), hit_normal));
                reflect_dir = vec3_normalize(reflect_dir);
                
                vec3 reflect_origin = vec3_add(hit_point, vec3_scale(1e-4f, hit_normal));
                ray reflect_ray = {reflect_origin, reflect_dir};
                vec3 reflect_color = trace_ray(reflect_ray, depth + 1);
                
                // Trace refracted ray
                vec3 refract_origin = vec3_add(hit_point, vec3_scale(-1e-4f, hit_normal)); // Inside sphere
                ray refract_ray = {refract_origin, refract_dir};
                vec3 refract_color = trace_ray(refract_ray, depth + 1);
                
                // Combine reflections and refractions
                color = vec3_add(vec3_scale(reflect_prob, reflect_color),
                                vec3_scale(1.0f - reflect_prob, refract_color));
            }
            break;
        }
    }
    
    // Apply fog effect
    float transmittance = expf(-0.035f * closest_t);
    vec3 fog_color = sky_color(r.direction);
    color = vec3_add(vec3_scale(transmittance, color),
                    vec3_scale(1.0f - transmittance, fog_color));
    
    return color;
}

// Convert linear RGB to sRGB gamma corrected
static uint8_t linear_to_gamma(float c) {
    if (c <= 0.0031308f) {
        return (uint8_t)(c * 12.92f * 255.0f + 0.5f);
    } else {
        return (uint8_t)((1.055f * powf(c, 1.0f/2.4f) - 0.055f) * 255.0f + 0.5f);
    }
}

// Save TGA file
static int save_tga(const char* filename, uint8_t* pixels, int width, int height) {
    FILE* fp = fopen(filename, "wb");
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
    // Allocate pixel buffer
    uint8_t* pixels = malloc(WIDTH * HEIGHT * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Failed to allocate pixel buffer\n");
        return 1;
    }
    
    // Setup camera
    vec3 forward = vec3_normalize(vec3_sub(CAMERA_TARGET, CAMERA_POS));
    vec3 right = vec3_normalize(vec3_cross(forward, CAMERA_UP));
    vec3 up = vec3_cross(right, forward);
    
    // Calculate aspect ratio and fov
    float aspect_ratio = (float)WIDTH / (float)HEIGHT;
    float fov_y_rad = FOV_Y * M_PI / 180.0f;
    float half_height = tanf(fov_y_rad / 2.0f);
    float half_width = half_height * aspect_ratio;
    
    // Render the image
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            vec3 pixel_color = {0.0f, 0.0f, 0.0f};
            
            // Sub-pixel sampling
            for (int s = 0; s < SAMPLES_PER_PIXEL; s++) {
                float u = (x + (s % 2) * 0.5f) / (WIDTH - 1);
                float v = (y + (s / 2) * 0.5f) / (HEIGHT - 1);
                
                // Adjust for sub-pixel sampling
                float sample_u = (u - 0.5f) * 2.0f * half_width;
                float sample_v = (v - 0.5f) * 2.0f * half_height;
                
                vec3 direction = vec3_normalize(vec3_add(
                    vec3_add(vec3_scale(sample_u, right),
                             vec3_scale(sample_v, up)),
                    forward
                ));
                
                ray r = {CAMERA_POS, direction};
                vec3 color = trace_ray(r, 0);
                
                pixel_color = vec3_add(pixel_color, color);
            }
            
            // Average the samples
            pixel_color = vec3_scale(1.0f / SAMPLES_PER_PIXEL, pixel_color);
            
            // Convert to 8-bit and store
            int idx = (y * WIDTH + x) * 3;
            pixels[idx] = linear_to_gamma(pixel_color.z);     // Blue
            pixels[idx + 1] = linear_to_gamma(pixel_color.y); // Green  
            pixels[idx + 2] = linear_to_gamma(pixel_color.x); // Red
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
