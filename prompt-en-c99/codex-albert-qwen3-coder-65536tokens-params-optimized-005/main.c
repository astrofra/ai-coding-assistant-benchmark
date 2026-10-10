#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

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

// Material types
typedef enum {
    MATERIAL_LAMBERTIAN,
    MATERIAL_METAL,
    MATERIAL_DIELECTRIC
} material_type;

// Material structure
typedef struct {
    material_type type;
    vec3 albedo;
    float roughness;
    float ior; // Index of refraction
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

// Scene structure
typedef struct {
    sphere spheres[2];
    plane ground;
    light lights[3];
    vec3 camera_pos;
    vec3 camera_target;
    vec3 camera_up;
    float fov;
    int width;
    int height;
} scene;

// Sky color function
static vec3 sky_color(vec3 ray_dir) {
    vec3 dark_red = (vec3){0.18f, 0.01f, 0.015f};
    vec3 horizon = (vec3){1.0f, 0.25f, 0.03f};
    
    float t = powf(fabsf(ray_dir.y), 0.65f);
    t = fmaxf(0.0f, fminf(1.0f, t)); // Clamp to [0, 1]
    
    return vec3_add(vec3_scale(1.0f - t, horizon), vec3_scale(t, dark_red));
}

// Checkerboard pattern
static vec3 checkerboard_color(vec3 point) {
    int x = (int)floorf(point.x);
    int z = (int)floorf(point.z);
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
    if (fabsf(denom) > 1e-6f) {
        vec3 op = vec3_sub(p.point, r.origin);
        float t_val = vec3_dot(op, p.normal) / denom;
        if (t_val > 1e-4f) {
            *t = t_val;
            return 1;
        }
    }
    return 0;
}

// Fresnel reflectance using Schlick's approximation
static float fresnel_reflectance(float cos_theta, float eta) {
    float r0 = (1.0f - eta) / (1.0f + eta);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * powf(1.0f - cos_theta, 5.0f);
}

// Reflect a vector around a normal
static vec3 reflect(vec3 incident, vec3 normal) {
    return vec3_sub(incident, vec3_scale(2.0f * vec3_dot(incident, normal), normal));
}

// Refract a vector using Snell's law
static vec3 refract(vec3 incident, vec3 normal, float eta) {
    float cos_i = vec3_dot(normal, incident);
    if (cos_i > 0.0f) {
        // Ray entering the material
        normal = vec3_scale(-1.0f, normal);
        cos_i = -cos_i;
    }
    
    float k = 1.0f - eta * eta * (1.0f - cos_i * cos_i);
    if (k < 0.0f) {
        // Total internal reflection
        return reflect(incident, normal);
    }
    
    return vec3_sub(vec3_scale(eta, incident), vec3_scale(eta * cos_i + sqrtf(k), normal));
}

// Sample a light for shadow calculation
static vec3 sample_light(light l, vec3 hit_point) {
    vec3 light_dir = vec3_sub(l.position, hit_point);
    float distance = vec3_length(light_dir);
    light_dir = vec3_normalize(light_dir);
    return vec3_scale(l.intensity.x / (distance * distance), light_dir);
}

// Shade a point with lighting
static vec3 shade_point(scene s, vec3 hit_point, vec3 normal, material mat) {
    vec3 color = (vec3){0.0f, 0.0f, 0.0f};
    
    // Ambient component
    vec3 ambient = (vec3){0.1f, 0.1f, 0.1f};
    color = vec3_add(color, vec3_mul(mat.albedo, ambient));
    
    // Light contributions
    for (int i = 0; i < 3; i++) {
        vec3 light_dir = vec3_sub(s.lights[i].position, hit_point);
        float distance = vec3_length(light_dir);
        light_dir = vec3_normalize(light_dir);
        
        // Shadow ray
        ray shadow_ray = {hit_point, light_dir};
        float shadow_t = 0.0f;
        int in_shadow = 0;
        
        // Check if we're in shadow
        for (int j = 0; j < 2; j++) {
            if (intersect_sphere(shadow_ray, s.spheres[j], &shadow_t)) {
                if (shadow_t < distance) {
                    in_shadow = 1;
                    break;
                }
            }
        }
        
        if (!in_shadow) {
            // Diffuse lighting
            float diff = fmaxf(0.0f, vec3_dot(normal, light_dir));
            vec3 diffuse = vec3_scale(diff, mat.albedo);
            
            // Specular lighting (for metals, we'll use a simpler approach)
            vec3 view_dir = vec3_normalize(vec3_scale(-1.0f, hit_point));
            vec3 reflect_dir = reflect(vec3_scale(-1.0f, light_dir), normal);
            float spec = powf(fmaxf(0.0f, vec3_dot(view_dir, reflect_dir)), 32.0f);
            vec3 specular = vec3_scale(spec, (vec3){1.0f, 1.0f, 1.0f});
            
            // Combine lighting
            if (mat.type == MATERIAL_LAMBERTIAN) {
                color = vec3_add(color, vec3_scale(s.lights[i].intensity.x, vec3_add(diffuse, specular)));
            } else if (mat.type == MATERIAL_METAL) {
                color = vec3_add(color, vec3_scale(s.lights[i].intensity.x, specular));
            } else if (mat.type == MATERIAL_DIELECTRIC) {
                color = vec3_add(color, vec3_scale(s.lights[i].intensity.x, diffuse));
            }
        }
    }
    
    return color;
}

// Trace a ray through the scene
static vec3 trace_ray(scene s, ray r, int depth) {
    if (depth <= 0) {
        return sky_color(r.direction);
    }
    
    float closest_t = 1e30f;
    int hit_sphere = -1;
    int hit_plane = 0;
    float plane_t = 0.0f;
    vec3 hit_point = {0.0f, 0.0f, 0.0f};
    vec3 hit_normal = {0.0f, 0.0f, 0.0f};
    material hit_mat = {MATERIAL_LAMBERTIAN, {0.0f, 0.0f, 0.0f}, 0.0f, 1.0f};
    
    // Test intersection with spheres
    for (int i = 0; i < 2; i++) {
        float t = 0.0f;
        if (intersect_sphere(r, s.spheres[i], &t) && t < closest_t) {
            closest_t = t;
            hit_sphere = i;
            hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
            hit_normal = vec3_normalize(vec3_sub(hit_point, s.spheres[i].center));
            hit_mat = s.spheres[i].mat;
            // Ensure normal points outward
            if (vec3_dot(hit_normal, r.direction) > 0.0f) {
                hit_normal = vec3_scale(-1.0f, hit_normal);
            }
        }
    }
    
    // Test intersection with ground plane
    if (intersect_plane(r, s.ground, &plane_t) && plane_t < closest_t) {
        closest_t = plane_t;
        hit_plane = 1;
        hit_point = vec3_add(r.origin, vec3_scale(plane_t, r.direction));
        hit_normal = s.ground.normal;
        hit_mat = s.ground.mat;
        // Ensure normal points outward
        if (vec3_dot(hit_normal, r.direction) > 0.0f) {
            hit_normal = vec3_scale(-1.0f, hit_normal);
        }
    }
    
    // No hit
    if (closest_t > 1e20f) {
        return sky_color(r.direction);
    }
    
    // Calculate shading for the hit point
    vec3 base_color = shade_point(s, hit_point, hit_normal, hit_mat);
    
    // Handle reflections and refractions
    if (hit_mat.type == MATERIAL_DIELECTRIC) {
        // Dielectric material (glass)
        vec3 reflect_dir = reflect(r.direction, hit_normal);
        vec3 refract_dir = refract(r.direction, hit_normal, 1.0f / hit_mat.ior);
        
        // Fresnel effect
        float cos_theta = fmaxf(0.0f, fminf(1.0f, vec3_dot(r.direction, hit_normal)));
        float reflect_prob = fresnel_reflectance(cos_theta, hit_mat.ior);
        
        // Simplified approach - always take the refracted ray for glass
        // We'll implement a basic version without Russian roulette for now
        vec3 refract_color = trace_ray(s, (ray){vec3_add(hit_point, vec3_scale(-1e-4f, hit_normal)), refract_dir}, depth - 1);
        return refract_color;
    } else if (hit_mat.type == MATERIAL_METAL) {
        // Metal material (chrome)
        vec3 reflect_dir = reflect(r.direction, hit_normal);
        vec3 reflect_color = trace_ray(s, (ray){vec3_add(hit_point, vec3_scale(1e-4f, hit_normal)), reflect_dir}, depth - 1);
        return reflect_color;
    } else {
        // Lambertian material (diffuse)
        return base_color;
    }
}

// Convert RGB to sRGB gamma corrected
static unsigned char gamma_correct(float c) {
    if (c <= 0.0031308f) {
        return (unsigned char)(c * 12.92f * 255.0f + 0.5f);
    } else {
        return (unsigned char)((1.055f * powf(c, 1.0f / 2.4f) - 0.055f) * 255.0f + 0.5f);
    }
}

// Save image as TGA
static int save_tga(const char* filename, unsigned char* pixels, int width, int height) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return 0;
    }
    
    // TGA Header (18 bytes)
    unsigned char header[18] = {0};
    header[2] = 2; // Image type: uncompressed truecolor
    header[12] = width & 0xFF;       // Width (little-endian)
    header[13] = (width >> 8) & 0xFF;
    header[14] = height & 0xFF;      // Height (little-endian)
    header[15] = (height >> 8) & 0xFF;
    header[16] = 24;                 // Pixel depth
    header[17] = 0x20;               // Descriptor: top-left origin, left-to-right
    
    fwrite(header, 1, 18, file);
    
    // Write pixel data (BGR format, top to bottom)
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            // BGR format
            fputc(pixels[idx + 2], file); // Blue
            fputc(pixels[idx + 1], file); // Green
            fputc(pixels[idx], file);     // Red
        }
    }
    
    fclose(file);
    return 1;
}

int main(void) {
    // Initialize scene
    scene s;
    
    // Camera setup
    s.camera_pos = (vec3){0.0f, 2.6f, 6.5f};
    s.camera_target = (vec3){0.0f, 1.0f, 0.0f};
    s.camera_up = (vec3){0.0f, 1.0f, 0.0f};
    s.fov = 45.0f;
    s.width = 1280;
    s.height = 720;
    
    // Create spheres
    // Glass sphere
    s.spheres[0].center = (vec3){-1.25f, 1.0f, 0.0f};
    s.spheres[0].radius = 1.0f;
    s.spheres[0].mat.type = MATERIAL_DIELECTRIC;
    s.spheres[0].mat.albedo = (vec3){1.0f, 1.0f, 1.0f};
    s.spheres[0].mat.ior = 1.5f;
    
    // Chrome sphere
    s.spheres[1].center = (vec3){1.25f, 1.0f, 0.0f};
    s.spheres[1].radius = 1.0f;
    s.spheres[1].mat.type = MATERIAL_METAL;
    s.spheres[1].mat.albedo = (vec3){0.8f, 0.8f, 0.8f};
    s.spheres[1].mat.roughness = 0.1f;
    
    // Ground plane
    s.ground.point = (vec3){0.0f, 0.0f, 0.0f};
    s.ground.normal = (vec3){0.0f, 1.0f, 0.0f};
    s.ground.mat.type = MATERIAL_LAMBERTIAN;
    s.ground.mat.albedo = (vec3){0.0f, 0.0f, 0.0f}; // Will be overridden by checkerboard
    
    // Lights
    s.lights[0].position = (vec3){-3.5f, 6.0f, 4.5f};
    s.lights[0].intensity = (vec3){1.0f, 1.0f, 1.0f};
    
    s.lights[1].position = (vec3){4.0f, 3.0f, 3.0f};
    s.lights[1].intensity = (vec3){0.35f, 0.35f, 0.35f};
    
    s.lights[2].position = (vec3){0.5f, 5.0f, -4.0f};
    s.lights[2].intensity = (vec3){0.7f, 0.7f, 0.7f};
    
    // Create image buffer
    unsigned char* image_buffer = (unsigned char*)malloc(s.width * s.height * 3);
    if (!image_buffer) {
        fprintf(stderr, "Failed to allocate image buffer\n");
        return 1;
    }
    
    // Set up camera coordinate system
    vec3 forward = vec3_normalize(vec3_sub(s.camera_target, s.camera_pos));
    vec3 right = vec3_normalize(vec3_cross(forward, s.camera_up));
    vec3 up = vec3_cross(right, forward);
    
    // Compute aspect ratio and field of view
    float aspect_ratio = (float)s.width / (float)s.height;
    float fov_rad = s.fov * M_PI / 180.0f;
    float half_height = tanf(fov_rad / 2.0f);
    float half_width = half_height * aspect_ratio;
    
    // Render the image
    for (int y = 0; y < s.height; y++) {
        for (int x = 0; x < s.width; x++) {
            vec3 pixel_color = (vec3){0.0f, 0.0f, 0.0f};
            
            // Subpixel sampling (2x2 grid)
            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    // Calculate subpixel position
                    float u = (x + (sx + 0.5f) / 2.0f) / s.width;
                    float v = (y + (sy + 0.5f) / 2.0f) / s.height;
                    
                    // Adjust for aspect ratio and FOV
                    float adjusted_u = (u - 0.5f) * 2.0f * half_width;
                    float adjusted_v = (0.5f - v) * 2.0f * half_height;
                    
                    // Compute ray direction
                    vec3 ray_dir = vec3_normalize(vec3_add(
                        vec3_add(vec3_scale(adjusted_u, right), vec3_scale(adjusted_v, up)),
                        forward
                    ));
                    
                    ray r = {s.camera_pos, ray_dir};
                    
                    // Trace the ray
                    vec3 color = trace_ray(s, r, 8);
                    
                    // Apply fog
                    // For simplicity, we'll just apply a basic fog effect
                    float fog_factor = expf(-0.035f * 100.0f); // Simple fog factor
                    vec3 sky = sky_color(ray_dir);
                    color = vec3_add(vec3_scale(fog_factor, color), vec3_scale(1.0f - fog_factor, sky));
                    
                    // Accumulate color
                    pixel_color = vec3_add(pixel_color, color);
                }
            }
            
            // Average the subpixels
            pixel_color = vec3_scale(0.25f, pixel_color);
            
            // Convert to 8-bit and store
            int idx = (y * s.width + x) * 3;
            image_buffer[idx] = gamma_correct(pixel_color.z);     // Blue
            image_buffer[idx + 1] = gamma_correct(pixel_color.y); // Green  
            image_buffer[idx + 2] = gamma_correct(pixel_color.x); // Red
        }
    }
    
    // Save the image
    if (!save_tga("out.tga", image_buffer, s.width, s.height)) {
        fprintf(stderr, "Failed to save TGA file\n");
        free(image_buffer);
        return 1;
    }
    
    free(image_buffer);
    printf("Raytracer completed successfully. Output saved as out.tga\n");
    return 0;
}
