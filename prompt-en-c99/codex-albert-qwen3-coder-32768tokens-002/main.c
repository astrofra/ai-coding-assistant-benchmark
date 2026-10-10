#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

// Vector math functions
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
#define MAX_DEPTH 8
#define NUM_SAMPLES 4
#define SKY_DARK_RED (vec3){0.18f, 0.01f, 0.015f}
#define SKY_ORANGE (vec3){1.0f, 0.25f, 0.03f}
#define CHECKER_SIZE 1.0f
#define GLASS_IOR 1.5f
#define CHROME_REFLECTIVITY 0.95f
#define FOG_DENSITY 0.035f

// Global scene objects
sphere spheres[] = {
    {{-1.25f, 1.0f, 0.0f}, 1.0f, 0}, // Glass sphere
    {{1.25f, 1.0f, 0.0f}, 1.0f, 1}   // Chrome sphere
};

plane ground = {
    {0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    0.0f // Will be computed dynamically
};

light lights[] = {
    {{-3.5f, 6.0f, 4.5f}, 1.0f},    // Key light
    {{4.0f, 3.0f, 3.0f}, 0.35f},    // Fill light
    {{0.5f, 5.0f, -4.0f}, 0.7f}     // Back light
};

// Sky color function
vec3 sky_color(vec3 ray_dir) {
    // Clamp y component to [0, 1] for numerical robustness
    float y = fmaxf(0.0f, fminf(1.0f, fabsf(ray_dir.y)));
    float t = powf(y, 0.65f);
    return vec3_add(
        vec3_scale(1.0f - t, SKY_ORANGE),
        vec3_scale(t, SKY_DARK_RED)
    );
}

// Checkerboard pattern
float checkerboard_pattern(vec3 point) {
    // Compute checkerboard pattern from world coordinates
    int x = (int)floorf(point.x / CHECKER_SIZE);
    int z = (int)floorf(point.z / CHECKER_SIZE);
    return ((x + z) % 2 == 0) ? 1.0f : 0.0f;
}

// Ray-sphere intersection
int intersect_sphere(ray r, sphere s, float *t) {
    vec3 oc = vec3_sub(r.origin, s.center);
    float a = vec3_dot(r.direction, r.direction);
    float b = 2.0f * vec3_dot(oc, r.direction);
    float c = vec3_dot(oc, oc) - s.radius * s.radius;
    float discriminant = b * b - 4 * a * c;

    if (discriminant < 0) {
        return 0; // No intersection
    }

    // Calculate both roots
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

    return 0; // Intersection behind ray origin or too close
}

// Ray-plane intersection
int intersect_plane(ray r, plane p, float *t) {
    float denom = vec3_dot(p.normal, r.direction);
    if (fabsf(denom) < 1e-6f) {
        return 0; // Ray is parallel to plane
    }

    vec3 op = vec3_sub(p.point, r.origin);
    *t = vec3_dot(op, p.normal) / denom;

    if (*t > 1e-4f) {
        return 1; // Valid intersection
    }

    return 0; // Intersection behind ray origin or too close
}

// Fresnel reflectance for dielectric (glass)
float fresnel_dielectric(float cos_theta, float eta_ratio) {
    // Schlick's approximation
    float r0 = (1.0f - eta_ratio) / (1.0f + eta_ratio);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * powf(1.0f - cos_theta, 5.0f);
}

// Reflect ray direction
vec3 reflect(vec3 incident, vec3 normal) {
    return vec3_sub(incident, vec3_scale(2.0f * vec3_dot(incident, normal), normal));
}

// Refract ray direction (Snell's law)
vec3 refract(vec3 incident, vec3 normal, float eta_ratio) {
    float cos_i = vec3_dot(normal, incident);
    float k = 1.0f - eta_ratio * eta_ratio * (1.0f - cos_i * cos_i);

    if (k < 0.0f) {
        // Total internal reflection
        return reflect(incident, normal);
    }

    return vec3_sub(
        vec3_scale(eta_ratio, incident),
        vec3_scale(eta_ratio * cos_i + sqrtf(k), normal)
    );
}

// Shade a point on a sphere
vec3 shade_sphere(vec3 point, vec3 normal, int sphere_idx) {
    
    // Get sphere material properties
    vec3 color = (sphere_idx == 0) ? 
        (vec3){1.0f, 1.0f, 1.0f} : // Glass is clear
        (vec3){0.8f, 0.8f, 0.8f};  // Chrome is silver

    // Compute lighting
    vec3 result = (vec3){0.0f, 0.0f, 0.0f};
    
    for (int i = 0; i < 3; i++) {
        light l = lights[i];
        vec3 light_dir = vec3_normalize(vec3_sub(l.position, point));
        float light_distance = vec3_length(vec3_sub(l.position, point));
        
        // Shadow ray test
        ray shadow_ray = {point, light_dir};
        float t_shadow;
        int shadow_hit = 0;
        
        // Test shadow ray against all spheres
        for (int j = 0; j < 2; j++) {
            if (intersect_sphere(shadow_ray, spheres[j], &t_shadow) && t_shadow < light_distance) {
                shadow_hit = 1;
                break;
            }
        }
        
        // Test shadow ray against ground plane
        if (!shadow_hit && intersect_plane(shadow_ray, ground, &t_shadow) && t_shadow < light_distance) {
            shadow_hit = 1;
        }
        
        if (!shadow_hit) {
            // Diffuse lighting
            float diffuse = fmaxf(0.0f, vec3_dot(normal, light_dir));
            
            // Specular lighting (Phong)
            vec3 view_dir = vec3_normalize(vec3_scale(-1.0f, point)); // View direction from origin
            vec3 reflect_dir = reflect(vec3_scale(-1.0f, light_dir), normal);
            float specular = powf(fmaxf(0.0f, vec3_dot(view_dir, reflect_dir)), 32.0f);
            
            // Combine lighting
            vec3 light_color = vec3_scale(l.intensity, color);
            result = vec3_add(result, vec3_scale(diffuse, light_color));
            result = vec3_add(result, vec3_scale(specular * 0.5f, light_color));
        }
    }
    
    // Ambient lighting
    result = vec3_add(result, vec3_scale(0.1f, color));
    
    return result;
}

// Shade a point on the ground plane
vec3 shade_ground(vec3 point) {
    // Compute checkerboard pattern
    float pattern = checkerboard_pattern(point);
    vec3 base_color = vec3_scale(pattern, (vec3){1.0f, 1.0f, 1.0f});
    base_color = vec3_add(base_color, vec3_scale(1.0f - pattern, (vec3){0.0f, 0.0f, 0.0f}));
    
    // Simple lighting - just diffuse from lights
    vec3 result = (vec3){0.0f, 0.0f, 0.0f};
    
    for (int i = 0; i < 3; i++) {
        light l = lights[i];
        vec3 light_dir = vec3_normalize(vec3_sub(l.position, point));
        
        // Shadow ray test
        ray shadow_ray = {point, light_dir};
        float t_shadow;
        int shadow_hit = 0;
        
        // Test shadow ray against all spheres
        for (int j = 0; j < 2; j++) {
            if (intersect_sphere(shadow_ray, spheres[j], &t_shadow)) {
                shadow_hit = 1;
                break;
            }
        }
        
        if (!shadow_hit) {
            // Diffuse lighting
            float diffuse = fmaxf(0.0f, vec3_dot(ground.normal, light_dir));
            vec3 light_color = vec3_scale(l.intensity, base_color);
            result = vec3_add(result, vec3_scale(diffuse, light_color));
        }
    }
    
    // Ambient lighting
    result = vec3_add(result, vec3_scale(0.1f, base_color));
    
    return result;
}

// Trace a ray recursively
vec3 trace_ray(ray r, int depth) {
    if (depth >= MAX_DEPTH) {
        return sky_color(r.direction);
    }

    float t_min = 1e30f;
    int hit_object = -1; // -1 = none, 0 = glass sphere, 1 = chrome sphere, 2 = ground
    vec3 hit_point, hit_normal;
    
    // Test intersection with spheres
    for (int i = 0; i < 2; i++) {
        float t;
        if (intersect_sphere(r, spheres[i], &t) && t < t_min) {
            t_min = t;
            hit_object = i;
            hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
            
            // Compute normal
            hit_normal = vec3_normalize(vec3_sub(hit_point, spheres[i].center));
            // Ensure normal points outward
            if (vec3_dot(r.direction, hit_normal) > 0) {
                hit_normal = vec3_scale(-1.0f, hit_normal);
            }
        }
    }
    
    // Test intersection with ground plane
    float t_plane;
    if (intersect_plane(r, ground, &t_plane) && t_plane < t_min) {
        t_min = t_plane;
        hit_object = 2;
        hit_point = vec3_add(r.origin, vec3_scale(t_plane, r.direction));
        hit_normal = ground.normal;
    }
    
    // Handle different types of hits
    if (hit_object >= 0) {
        if (hit_object < 2) { // Sphere hit
            sphere s = spheres[hit_object];
            
            if (s.material == 0) { // Glass sphere
                // Ray is entering or exiting the sphere
                float cos_theta = vec3_dot(hit_normal, r.direction);
                float eta_ratio = (cos_theta > 0) ? GLASS_IOR : 1.0f / GLASS_IOR;
                vec3 refracted_dir = refract(r.direction, hit_normal, eta_ratio);
                
                // Calculate Fresnel reflectance
                float reflectance = fresnel_dielectric(fabsf(cos_theta), eta_ratio);
                
                // Reflection ray
                vec3 reflected_dir = reflect(r.direction, hit_normal);
                ray reflected_ray = {
                    vec3_add(hit_point, vec3_scale(1e-4f, hit_normal)),
                    reflected_dir
                };
                
                // Refraction ray (if not total internal reflection)
                ray refracted_ray = {
                    vec3_add(hit_point, vec3_scale(-1e-4f, hit_normal)),
                    refracted_dir
                };
                
                // Trace both rays
                vec3 reflected_color = trace_ray(reflected_ray, depth + 1);
                vec3 refracted_color = trace_ray(refracted_ray, depth + 1);
                
                // Combine with Fresnel weights
                vec3 final_color = vec3_add(
                    vec3_scale(reflectance, reflected_color),
                    vec3_scale(1.0f - reflectance, refracted_color)
                );
                
                // Shade the surface for direct lighting
                vec3 direct_lighting = shade_sphere(hit_point, hit_normal, hit_object);
                
                // Combine direct lighting with indirect lighting
                return vec3_add(
                    vec3_mul(direct_lighting, final_color),
                    vec3_scale(0.05f, final_color) // Small ambient contribution
                );
            } else { // Chrome sphere
                // Mirror-like reflection
                vec3 reflected_dir = reflect(r.direction, hit_normal);
                ray reflected_ray = {
                    vec3_add(hit_point, vec3_scale(1e-4f, hit_normal)),
                    reflected_dir
                };
                
                vec3 reflected_color = trace_ray(reflected_ray, depth + 1);
                
                // Shade the surface for direct lighting
                vec3 direct_lighting = shade_sphere(hit_point, hit_normal, hit_object);
                
                // Combine direct lighting with reflection
                return vec3_add(
                    vec3_mul(direct_lighting, vec3_scale(1.0f - CHROME_REFLECTIVITY, (vec3){1.0f, 1.0f, 1.0f})),
                    vec3_scale(CHROME_REFLECTIVITY, reflected_color)
                );
            }
        } else { // Ground hit
            vec3 color = shade_ground(hit_point);
            
            // Apply fog effect
            float fog_factor = expf(-FOG_DENSITY * t_min);
            vec3 fog_color = sky_color(r.direction);
            return vec3_add(
                vec3_scale(fog_factor, color),
                vec3_scale(1.0f - fog_factor, fog_color)
            );
        }
    } else {
        // No intersection - return sky color
        return sky_color(r.direction);
    }
}

// Convert linear RGB to sRGB with gamma correction
uint8_t linear_to_gamma(float c) {
    if (c <= 0.0031308f) {
        return (uint8_t)(c * 12.92f * 255.0f + 0.5f);
    } else {
        return (uint8_t)((1.055f * powf(c, 1.0f / 2.4f) - 0.055f) * 255.0f + 0.5f);
    }
}

// Save TGA image
int save_tga(const char* filename, uint8_t* pixels, int width, int height) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        return 0;
    }

    // TGA Header (18 bytes)
    uint8_t header[18] = {0};
    header[2] = 2; // Image type: uncompressed true-color
    header[12] = width & 0xFF;         // Width (little-endian)
    header[13] = (width >> 8) & 0xFF;
    header[14] = height & 0xFF;        // Height (little-endian)
    header[15] = (height >> 8) & 0xFF;
    header[16] = 24;                   // Pixel depth
    header[17] = 0x20;                 // Image descriptor: top-left origin

    fwrite(header, 1, 18, fp);

    // Write pixel data (BGR order)
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            // Write in BGR order
            fputc(pixels[idx + 2], fp); // Blue
            fputc(pixels[idx + 1], fp); // Green
            fputc(pixels[idx + 0], fp); // Red
        }
    }

    fclose(fp);
    return 1;
}

int main(void) {
    // Camera setup
    vec3 camera_pos = {0.0f, 2.6f, 6.5f};
    vec3 camera_target = {0.0f, 1.0f, 0.0f};
    vec3 camera_up = {0.0f, 1.0f, 0.0f};

    // Compute camera coordinate system
    vec3 camera_forward = vec3_normalize(vec3_sub(camera_target, camera_pos));
    vec3 camera_right = vec3_normalize(vec3_cross(camera_forward, camera_up));
    camera_up = vec3_cross(camera_right, camera_forward);

    // Image dimensions
    const int width = 1280;
    const int height = 720;
    const int num_pixels = width * height;

    // Allocate pixel buffer
    uint8_t* pixels = malloc(num_pixels * 3);
    if (!pixels) {
        fprintf(stderr, "Failed to allocate pixel buffer\n");
        return 1;
    }

    // Compute field of view and aspect ratio
    const float fov = 45.0f * M_PI / 180.0f; // 45 degrees in radians
    const float aspect_ratio = (float)width / (float)height;

    // For each pixel in the image
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            vec3 pixel_color = (vec3){0.0f, 0.0f, 0.0f};

            // Sample multiple times per pixel for anti-aliasing
            for (int s = 0; s < NUM_SAMPLES; s++) {
                // Compute subpixel coordinates
                float u = (x + (s % 2) * 0.5f) / (float)width;
                float v = (y + (s / 2) * 0.5f) / (float)height;

                // Compute ray direction
                vec3 ray_dir = vec3_add(
                    vec3_scale((2.0f * u - 1.0f) * tanf(fov / 2.0f) * aspect_ratio, camera_right),
                    vec3_add(
                        vec3_scale(1.0f - 2.0f * v, camera_up),
                        camera_forward
                    )
                );
                ray_dir = vec3_normalize(ray_dir);

                // Create ray from camera
                ray r = {camera_pos, ray_dir};

                // Trace ray
                vec3 color = trace_ray(r, 0);

                // Accumulate color
                pixel_color = vec3_add(pixel_color, color);
            }

            // Average samples
            pixel_color = vec3_scale(1.0f / (float)NUM_SAMPLES, pixel_color);

            // Convert to 8-bit sRGB
            int idx = (y * width + x) * 3;
            pixels[idx + 0] = linear_to_gamma(pixel_color.x); // Red
            pixels[idx + 1] = linear_to_gamma(pixel_color.y); // Green
            pixels[idx + 2] = linear_to_gamma(pixel_color.z); // Blue
        }
    }

    // Save image
    if (!save_tga("out.tga", pixels, width, height)) {
        fprintf(stderr, "Failed to save TGA file\n");
        free(pixels);
        return 1;
    }

    free(pixels);
    return 0;
}
