#include <stdio.h>
#include <stdlib.h>
#include <math.h>

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

static inline float vec3_len2(vec3 v) {
    return vec3_dot(v, v);
}

static inline float vec3_len(vec3 v) {
    return sqrtf(vec3_len2(v));
}

static inline vec3 vec3_norm(vec3 v) {
    float len = vec3_len(v);
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
    float eta; // Index of refraction
    int is_glass;
} material;

// Light structure
typedef struct {
    vec3 position;
    vec3 intensity;
} light;

// Constants
#define WIDTH 1280
#define HEIGHT 720
#define SAMPLES 4
#define MAX_BOUNCES 8
#define EPSILON 1e-4f
#define FOG_DENSITY 0.035f

// Sky color function
static vec3 sky_color(vec3 ray_dir) {
    // Clamp ray direction Y component to [0, 1] for numerical robustness
    float y = fmaxf(0.0f, fminf(1.0f, fabsf(ray_dir.y)));
    
    // Calculate the interpolation factor using power function
    float t = powf(y, 0.65f);
    
    // Define colors
    vec3 dark_red = (vec3){0.18f, 0.01f, 0.015f};
    vec3 orange = (vec3){1.0f, 0.25f, 0.03f};
    
    // Interpolate between dark red and orange
    return vec3_add(vec3_scale(1.0f - t, dark_red), vec3_scale(t, orange));
}

// Checkerboard pattern
static vec3 checkerboard_color(vec3 point) {
    // Compute checker pattern from world-space X and Z coordinates
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
    float discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) {
        return 0;
    }
    
    // Find the nearest positive root
    float sqrt_discriminant = sqrtf(discriminant);
    float t1 = (-b - sqrt_discriminant) / (2.0f * a);
    float t2 = (-b + sqrt_discriminant) / (2.0f * a);
    
    // Return the nearest positive intersection
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
    
    return 0;
}

// Fresnel reflection coefficient (Schlick's approximation)
static float fresnel(float cos_theta, float eta_ratio) {
    float r0 = (1.0f - eta_ratio) / (1.0f + eta_ratio);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * powf(1.0f - cos_theta, 5.0f);
}

// Reflect a vector around a normal
static vec3 reflect(vec3 incident, vec3 normal) {
    return vec3_sub(incident, vec3_scale(2.0f * vec3_dot(incident, normal), normal));
}

// Refract a vector using Snell's law
static vec3 refract(vec3 incident, vec3 normal, float eta_ratio) {
    float cos_i = vec3_dot(normal, incident);
    float k = 1.0f - eta_ratio * eta_ratio * (1.0f - cos_i * cos_i);
    
    if (k < 0.0f) {
        return (vec3){0.0f, 0.0f, 0.0f}; // Total internal reflection
    }
    
    return vec3_sub(vec3_scale(eta_ratio, incident),
                    vec3_scale(eta_ratio * cos_i + sqrtf(k), normal));
}

// Shade a point with lighting
static vec3 shade_point(vec3 point, vec3 normal, material mat, int is_glass) {
    vec3 color = (vec3){0.0f, 0.0f, 0.0f};
    
    // Define lights
    light lights[] = {
        {{-3.5f, 6.0f, 4.5f}, {1.0f, 1.0f, 1.0f}},   // Key light
        {{4.0f, 3.0f, 3.0f}, {0.35f, 0.35f, 0.35f}},  // Fill light
        {{0.5f, 5.0f, -4.0f}, {0.7f, 0.7f, 0.7f}}     // Back light
    };
    int num_lights = sizeof(lights) / sizeof(lights[0]);
    
    for (int i = 0; i < num_lights; i++) {
        vec3 light_dir = vec3_norm(vec3_sub(lights[i].position, point));
        float light_dist = vec3_len(vec3_sub(lights[i].position, point));
        
        // Shadow ray
        ray shadow_ray = {point, light_dir};
        float t_shadow;
        
        // Check if we're in shadow
        int in_shadow = 0;
        if (is_glass) {
            // For glass, allow some light transmission
            // We'll use a simple approximation that lets some light through
            in_shadow = 0; // Glass doesn't block light directly
        } else {
            // For chrome, check if it blocks light
            // We'll assume chrome blocks direct light (this is simplified)
            in_shadow = 1; // Simplified for now - chrome blocks light
        }
        
        if (!in_shadow) {
            // Diffuse lighting (Lambert)
            float NdotL = vec3_dot(normal, light_dir);
            if (NdotL > 0) {
                color = vec3_add(color, vec3_scale(NdotL, mat.albedo));
            }
            
            // Simple specular highlight (Phong model)
            vec3 view_dir = vec3_norm(vec3_scale(-1.0f, light_dir)); // View direction
            vec3 reflect_dir = reflect(light_dir, normal);
            float RdotV = vec3_dot(reflect_dir, view_dir);
            if (RdotV > 0) {
                float spec = powf(RdotV, 32.0f); // Shininess
                color = vec3_add(color, vec3_scale(spec, lights[i].intensity));
            }
        }
    }
    
    // Ambient lighting
    color = vec3_add(color, vec3_scale(0.1f, mat.albedo));
    
    return color;
}

// Recursive ray tracer
static vec3 trace_ray(ray r, int depth) {
    if (depth >= MAX_BOUNCES) {
        return sky_color(r.direction);
    }
    
    // Scene geometry
    sphere spheres[] = {
        {{-1.25f, 1.0f, 0.0f}, 1.0f}, // Glass sphere
        {{1.25f, 1.0f, 0.0f}, 1.0f}   // Chrome sphere
    };
    
    plane ground = {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}; // Ground plane
    material materials[] = {
        {{1.0f, 1.0f, 1.0f}, 0.0f, 1.5f, 1}, // Glass material
        {{0.8f, 0.8f, 0.8f}, 0.95f, 0.0f, 0}  // Chrome material
    };
    
    // Find closest intersection
    float closest_t = 1e30f;
    int hit_sphere = -1;
    vec3 hit_point, hit_normal;
    material hit_material;
    
    // Check sphere intersections
    for (int i = 0; i < 2; i++) {
        float t;
        if (intersect_sphere(r, spheres[i], &t) && t < closest_t) {
            closest_t = t;
            hit_sphere = i;
            hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
            hit_normal = vec3_norm(vec3_sub(hit_point, spheres[i].center));
            hit_material = materials[i];
        }
    }
    
    // Check ground plane intersection
    float t_plane;
    if (intersect_plane(r, ground, &t_plane) && t_plane < closest_t) {
        closest_t = t_plane;
        hit_sphere = -1; // Ground plane
        hit_point = vec3_add(r.origin, vec3_scale(t_plane, r.direction));
        hit_normal = (vec3){0.0f, 1.0f, 0.0f};
        hit_material = (material){{0.5f, 0.5f, 0.5f}, 0.0f, 0.0f, 0}; // Default material
    }
    
    // If no intersection, return sky color
    if (hit_sphere == -1 && closest_t > 1e20f) {
        return sky_color(r.direction);
    }
    
    // Handle intersection
    vec3 result_color;
    
    if (hit_sphere == -1) {
        // Hit ground plane
        vec3 base_color = checkerboard_color(hit_point);
        result_color = shade_point(hit_point, hit_normal, hit_material, 0);
        result_color = vec3_mul(result_color, base_color);
    } else {
        // Hit sphere
        if (hit_material.is_glass) {
            // Glass sphere - handle refraction and reflection
            vec3 view_dir = vec3_scale(-1.0f, r.direction);
            float cos_i = vec3_dot(hit_normal, view_dir);
            
            // Determine if ray is entering or exiting
            float eta_ratio = 1.0f / hit_material.eta; // Air to glass
            
            if (cos_i < 0) {
                // Entering sphere - flip normal for correct orientation
                hit_normal = vec3_scale(-1.0f, hit_normal);
                eta_ratio = hit_material.eta / 1.0f; // Glass to air
                cos_i = -cos_i;
            }
            
            // Calculate reflection and refraction directions
            vec3 reflect_dir = reflect(r.direction, hit_normal);
            vec3 refract_dir_temp = refract(r.direction, hit_normal, eta_ratio);
            
            // Handle total internal reflection
            if (vec3_len2(refract_dir_temp) < 1e-6f) {
                // Total internal reflection - use reflection
                result_color = trace_ray((ray){vec3_add(hit_point, vec3_scale(EPSILON, hit_normal)), reflect_dir}, depth + 1);
            } else {
                // Calculate Fresnel coefficient
                float kr = fresnel(cos_i, eta_ratio);
                
                // Trace reflection and refraction rays
                vec3 reflect_color = trace_ray((ray){vec3_add(hit_point, vec3_scale(EPSILON, hit_normal)), reflect_dir}, depth + 1);
                vec3 refract_color = trace_ray((ray){vec3_add(hit_point, vec3_scale(-EPSILON, hit_normal)), refract_dir_temp}, depth + 1);
                
                // Combine reflection and refraction
                result_color = vec3_add(vec3_scale(kr, reflect_color), vec3_scale(1.0f - kr, refract_color));
            }
        } else {
            // Chrome sphere - mirror-like reflection
            vec3 reflect_dir = reflect(r.direction, hit_normal);
            result_color = trace_ray((ray){vec3_add(hit_point, vec3_scale(EPSILON, hit_normal)), reflect_dir}, depth + 1);
        }
    }
    
    // Apply fog effect
    float transmittance = expf(-FOG_DENSITY * closest_t);
    vec3 fog_color = sky_color(r.direction);
    result_color = vec3_add(vec3_scale(transmittance, result_color), vec3_scale(1.0f - transmittance, fog_color));
    
    return result_color;
}

// Convert linear RGB to sRGB
static float linear_to_srgb(float c) {
    if (c <= 0.0031308f) {
        return 12.92f * c;
    } else {
        return 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
    }
}

// Save TGA image
static int save_tga(const char* filename, unsigned char* pixels, int width, int height) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return 0;
    }
    
    // TGA Header (18 bytes)
    unsigned char header[18] = {0};
    header[2] = 2; // Image type: uncompressed true-color
    header[12] = width & 0xFF; // Width (little-endian)
    header[13] = (width >> 8) & 0xFF;
    header[14] = height & 0xFF; // Height (little-endian)
    header[15] = (height >> 8) & 0xFF;
    header[16] = 24; // Pixel depth
    header[17] = 0x20; // Image descriptor: top-left origin, left-to-right pixel order
    
    if (fwrite(header, 1, 18, file) != 18) {
        fclose(file);
        return 0;
    }
    
    // Write pixels (BGR order)
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            unsigned char b = pixels[idx];
            unsigned char g = pixels[idx + 1];
            unsigned char r = pixels[idx + 2];
            if (fwrite(&b, 1, 1, file) != 1 || 
                fwrite(&g, 1, 1, file) != 1 || 
                fwrite(&r, 1, 1, file) != 1) {
                fclose(file);
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
    vec3 forward = vec3_norm(vec3_sub(camera_target, camera_pos));
    vec3 right = vec3_norm(vec3_cross(forward, camera_up));
    vec3 up = vec3_cross(right, forward);
    
    // Field of view and aspect ratio
    float fov = 45.0f * M_PI / 180.0f;
    float aspect_ratio = (float)WIDTH / (float)HEIGHT;
    
    // Image buffer
    unsigned char* pixels = (unsigned char*)malloc(WIDTH * HEIGHT * 3);
    if (!pixels) {
        fprintf(stderr, "Failed to allocate memory for image\n");
        return 1;
    }
    
    // Render image
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            vec3 pixel_color = (vec3){0.0f, 0.0f, 0.0f};
            
            // Sub-pixel sampling
            for (int s = 0; s < SAMPLES; s++) {
                // Compute sub-pixel position
                float u = (x + (s % 2) * 0.5f) / WIDTH;
                float v = (y + (s / 2) * 0.5f) / HEIGHT;
                
                // Scale and offset UV coordinates
                float scale = tanf(fov / 2.0f);
                float px = (2.0f * u - 1.0f) * scale * aspect_ratio;
                float py = (1.0f - 2.0f * v) * scale;
                
                // Construct ray direction
                vec3 ray_dir = vec3_norm(vec3_add(vec3_add(vec3_scale(px, right), vec3_scale(py, up)), forward));
                
                // Create ray
                ray r = {camera_pos, ray_dir};
                
                // Trace ray
                vec3 color = trace_ray(r, 0);
                
                // Accumulate color
                pixel_color = vec3_add(pixel_color, color);
            }
            
            // Average samples
            pixel_color = vec3_scale(1.0f / SAMPLES, pixel_color);
            
            // Gamma correction and convert to 8-bit
            int idx = (y * WIDTH + x) * 3;
            pixels[idx] = (unsigned char)(fminf(255.0f, 255.0f * linear_to_srgb(pixel_color.z))); // Blue
            pixels[idx + 1] = (unsigned char)(fminf(255.0f, 255.0f * linear_to_srgb(pixel_color.y))); // Green
            pixels[idx + 2] = (unsigned char)(fminf(255.0f, 255.0f * linear_to_srgb(pixel_color.x))); // Red
        }
    }
    
    // Save image
    if (!save_tga("out.tga", pixels, WIDTH, HEIGHT)) {
        fprintf(stderr, "Failed to save TGA image\n");
        free(pixels);
        return 1;
    }
    
    free(pixels);
    printf("Raytracer completed successfully. Output saved to out.tga\n");
    return 0;
}
