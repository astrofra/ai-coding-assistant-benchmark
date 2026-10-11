#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Vector math structures and functions
typedef struct {
    float x, y, z;
} vec3;

// Vector operations
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

static inline float vec3_length_sq(vec3 v) {
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
    float roughness;
    float metallic;
    float ior; // Index of refraction
    float transmission; // Transmission coefficient
} material;

// Hit record structure
typedef struct {
    float t;
    vec3 point;
    vec3 normal;
    material mat;
    int front_face;
} hit_record;

// Scene objects
static const sphere spheres[] = {
    {{-1.25f, 1.0f, 0.0f}, 1.0f},   // Glass sphere
    {{1.25f, 1.0f, 0.0f}, 1.0f}     // Chrome sphere
};

static const material materials[] = {
    {{1.0f, 1.0f, 1.0f}, 0.0f, 0.0f, 1.5f, 1.0f},  // Glass
    {{0.8f, 0.8f, 0.8f}, 0.0f, 0.95f, 1.0f, 0.0f}   // Chrome
};

// Light structure
typedef struct {
    vec3 position;
    vec3 intensity;
} light;

static const light lights[] = {
    {{-3.5f, 6.0f, 4.5f}, {1.0f, 1.0f, 1.0f}},   // Key light
    {{4.0f, 3.0f, 3.0f}, {0.35f, 0.35f, 0.35f}},  // Fill light
    {{0.5f, 5.0f, -4.0f}, {0.7f, 0.7f, 0.7f}}    // Back light
};

// Sky color computation
static vec3 sky_color(vec3 ray_dir) {
    // Clamp ray direction Y component to [0, 1] for numerical robustness
    float t = powf(fmaxf(0.0f, fminf(1.0f, fabsf(ray_dir.y))), 0.65f);
    
    // Dark red color at zenith/nadir
    vec3 dark_red = {0.18f, 0.01f, 0.015f};
    // Orange color at horizon
    vec3 orange = {1.0f, 0.25f, 0.03f};
    
    return vec3_add(vec3_scale(1.0f - t, orange), vec3_scale(t, dark_red));
}

// Checkerboard pattern
static vec3 checkerboard_color(vec3 point) {
    // Use floor to handle negative coordinates properly
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
    
    // Calculate both roots
    float sqrt_discriminant = sqrtf(discriminant);
    float t1 = (-b - sqrt_discriminant) / (2.0f * a);
    float t2 = (-b + sqrt_discriminant) / (2.0f * a);
    
    // Return the closest positive root
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
    float denominator = vec3_dot(p.normal, r.direction);
    
    if (fabsf(denominator) < 1e-6f) {
        return 0; // Ray is parallel to plane
    }
    
    vec3 op = vec3_sub(p.point, r.origin);
    *t = vec3_dot(op, p.normal) / denominator;
    
    if (*t > 1e-4f) {
        return 1;
    }
    
    return 0;
}

// Fresnel reflection coefficient using Schlick's approximation
static float fresnel(float cos_theta, float eta_ratio) {
    float r0 = (1.0f - eta_ratio) / (1.0f + eta_ratio);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * powf(1.0f - cos_theta, 5.0f);
}

// Reflect a vector around a normal
static vec3 reflect(vec3 incident, vec3 normal) {
    return vec3_sub(incident, vec3_scale(2.0f * vec3_dot(incident, normal), normal));
}

// Refract a vector through a surface using Snell's law
static vec3 refract(vec3 incident, vec3 normal, float eta_ratio) {
    float cos_i = vec3_dot(normal, incident);
    float k = 1.0f - eta_ratio * eta_ratio * (1.0f - cos_i * cos_i);
    
    if (k < 0.0f) {
        // Total internal reflection
        return reflect(incident, normal);
    }
    
    return vec3_sub(vec3_scale(eta_ratio, incident), 
                    vec3_scale(eta_ratio * cos_i + sqrtf(k), normal));
}

// Trace a ray through the scene
static vec3 trace_ray(ray r, int depth, int max_depth) {
    if (depth >= max_depth) {
        return sky_color(r.direction);
    }
    
    float closest_t = 1e30f;
    hit_record hit;
    int hit_anything = 0;
    
    // Intersect with spheres
    for (int i = 0; i < 2; i++) {
        float t;
        if (intersect_sphere(r, spheres[i], &t) && t < closest_t) {
            closest_t = t;
            hit_anything = 1;
            
            // Fill hit record
            hit.t = t;
            hit.point = vec3_add(r.origin, vec3_scale(t, r.direction));
            hit.normal = vec3_normalize(vec3_sub(hit.point, spheres[i].center));
            hit.mat = materials[i];
            hit.front_face = vec3_dot(r.direction, hit.normal) < 0.0f;
            
            // Flip normal if we're inside the sphere
            if (!hit.front_face) {
                hit.normal = vec3_scale(-1.0f, hit.normal);
            }
        }
    }
    
    // Intersect with ground plane
    plane ground = {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    float t;
    if (intersect_plane(r, ground, &t) && t < closest_t) {
        closest_t = t;
        hit_anything = 1;
        
        // Fill hit record
        hit.t = t;
        hit.point = vec3_add(r.origin, vec3_scale(t, r.direction));
        hit.normal = (vec3){0.0f, 1.0f, 0.0f};
        hit.mat = (material){{0.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 1.0f, 0.0f};
        hit.front_face = vec3_dot(r.direction, hit.normal) < 0.0f;
        
        // Set checkerboard color
        hit.mat.albedo = checkerboard_color(hit.point);
    }
    
    if (!hit_anything) {
        return sky_color(r.direction);
    }
    
    // Shading calculations
    vec3 color = {0.0f, 0.0f, 0.0f};
    
    // Ambient lighting
    color = vec3_add(color, vec3_scale(0.1f, hit.mat.albedo));
    
    // Direct lighting from all lights
    for (int i = 0; i < 3; i++) {
        vec3 light_dir = vec3_normalize(vec3_sub(lights[i].position, hit.point));
        float light_distance = vec3_length(vec3_sub(lights[i].position, hit.point));
        
        // Shadow ray
        vec3 shadow_origin = vec3_add(hit.point, vec3_scale(1e-4f, hit.normal));
        ray shadow_ray = {shadow_origin, light_dir};
        float shadow_t;
        
        // Check if we're in shadow
        int in_shadow = 0;
        for (int j = 0; j < 2; j++) {
            if (intersect_sphere(shadow_ray, spheres[j], &shadow_t) && shadow_t < light_distance) {
                in_shadow = 1;
                break;
            }
        }
        
        // If not in shadow, compute lighting
        if (!in_shadow) {
            // Diffuse lighting
            float diffuse = fmaxf(0.0f, vec3_dot(hit.normal, light_dir));
            vec3 light_color = vec3_mul(lights[i].intensity, hit.mat.albedo);
            color = vec3_add(color, vec3_scale(diffuse, light_color));
            
            // Specular lighting (for metals, this is mostly reflection)
            if (hit.mat.metallic > 0.0f) {
                vec3 view_dir = vec3_normalize(vec3_scale(-1.0f, r.direction));
                vec3 reflect_dir = reflect(vec3_scale(-1.0f, light_dir), hit.normal);
                float spec = powf(fmaxf(0.0f, vec3_dot(view_dir, reflect_dir)), 32.0f);
                color = vec3_add(color, vec3_scale(spec * 0.5f, lights[i].intensity));
            }
        }
    }
    
    // Reflection and refraction
    if (depth < max_depth - 1) {
        vec3 reflect_color = {0.0f, 0.0f, 0.0f};
        vec3 refract_color = {0.0f, 0.0f, 0.0f};
        
        // Reflection
        if (hit.mat.metallic > 0.0f) {
            vec3 reflect_dir = reflect(r.direction, hit.normal);
            vec3 reflect_origin = vec3_add(hit.point, vec3_scale(1e-4f, hit.normal));
            ray reflect_ray = {reflect_origin, reflect_dir};
            reflect_color = trace_ray(reflect_ray, depth + 1, max_depth);
        }
        
        // Refraction (for glass)
        if (hit.mat.ior > 1.0f) {
            vec3 refract_dir;
            float eta_ratio = hit.front_face ? 1.0f / hit.mat.ior : hit.mat.ior / 1.0f;
            
            if (hit.front_face) {
                // Exiting the sphere
                refract_dir = refract(r.direction, vec3_scale(-1.0f, hit.normal), eta_ratio);
            } else {
                // Entering the sphere
                refract_dir = refract(r.direction, hit.normal, eta_ratio);
            }
            
            // Check if total internal reflection occurs
            if (vec3_length_sq(refract_dir) > 1e-6f) {
                vec3 refract_origin = vec3_add(hit.point, vec3_scale(1e-4f, hit.normal));
                ray refract_ray = {refract_origin, refract_dir};
                refract_color = trace_ray(refract_ray, depth + 1, max_depth);
            }
        }
        
        // Mix reflections and refractions
        if (hit.mat.metallic > 0.0f) {
            // Metal reflection
            color = vec3_add(vec3_scale(hit.mat.metallic, reflect_color),
                             vec3_scale(1.0f - hit.mat.metallic, color));
        } else if (hit.mat.ior > 1.0f) {
            // Glass refraction
            float reflect_coeff = fresnel(vec3_dot(r.direction, hit.normal), hit.mat.ior);
            color = vec3_add(vec3_scale(reflect_coeff, reflect_color),
                             vec3_scale(1.0f - reflect_coeff, refract_color));
        }
    }
    
    // Apply fog effect
    float transmittance = expf(-0.035f * hit.t);
    vec3 fog_color = sky_color(r.direction);
    color = vec3_add(vec3_scale(transmittance, color),
                     vec3_scale(1.0f - transmittance, fog_color));
    
    return color;
}

// Convert linear RGB to sRGB gamma corrected
static unsigned char gamma_correct(float c) {
    if (c <= 0.0031308f) {
        return (unsigned char)(c * 12.92f * 255.0f + 0.5f);
    } else {
        return (unsigned char)((1.055f * powf(c, 1.0f / 2.4f) - 0.055f) * 255.0f + 0.5f);
    }
}

// Write TGA file
static int write_tga(const char* filename, int width, int height, unsigned char* pixels) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open %s for writing\n", filename);
        return 0;
    }
    
    // TGA Header (18 bytes)
    unsigned char header[18] = {0};
    header[2] = 2; // Image type: uncompressed truecolor
    header[12] = width & 0xFF;         // Width (low byte)
    header[13] = (width >> 8) & 0xFF;  // Width (high byte)
    header[14] = height & 0xFF;        // Height (low byte)
    header[15] = (height >> 8) & 0xFF; // Height (high byte)
    header[16] = 24;                   // Pixel depth
    header[17] = 0x20;                 // Descriptor: top-left origin, left-to-right pixel order
    
    fwrite(header, 1, 18, fp);
    
    // Write pixel data (BGR format)
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            unsigned char b = pixels[idx];
            unsigned char g = pixels[idx + 1];
            unsigned char r = pixels[idx + 2];
            fwrite(&b, 1, 1, fp);
            fwrite(&g, 1, 1, fp);
            fwrite(&r, 1, 1, fp);
        }
    }
    
    fclose(fp);
    return 1;
}

int main(void) {
    // Image dimensions
    const int width = 1280;
    const int height = 720;
    const int max_depth = 8;
    
    // Allocate pixel buffer
    unsigned char* pixels = malloc(width * height * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Cannot allocate pixel buffer\n");
        return 1;
    }
    
    // Camera setup
    vec3 camera_pos = {0.0f, 2.6f, 6.5f};
    vec3 look_at = {0.0f, 1.0f, 0.0f};
    vec3 camera_up = {0.0f, 1.0f, 0.0f};
    
    // Compute camera coordinate system
    vec3 forward = vec3_normalize(vec3_sub(look_at, camera_pos));
    vec3 right = vec3_normalize(vec3_cross(forward, camera_up));
    vec3 up = vec3_cross(right, forward);
    
    // Field of view and aspect ratio
    const float fov = 45.0f;
    const float aspect_ratio = (float)width / (float)height;
    const float fov_rad = fov * M_PI / 180.0f;
    const float half_height = tanf(fov_rad / 2.0f);
    const float half_width = half_height * aspect_ratio;
    
    // Render the image
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Subpixel sampling
            vec3 color = {0.0f, 0.0f, 0.0f};
            const int samples_per_pixel = 4;
            
            for (int s = 0; s < samples_per_pixel; s++) {
                // Generate subpixel jitter
                float u = (x + (s % 2) * 0.5f) / (float)(width - 1);
                float v = (y + (s / 2) * 0.5f) / (float)(height - 1);
                
                // Adjust for aspect ratio and FOV
                float ray_u = (u - 0.5f) * 2.0f * half_width;
                float ray_v = (v - 0.5f) * 2.0f * half_height;
                
                vec3 ray_dir = vec3_normalize(
                    vec3_add(vec3_add(vec3_scale(ray_u, right),
                                      vec3_scale(ray_v, up)),
                             forward)
                );
                
                ray r = {camera_pos, ray_dir};
                vec3 sample_color = trace_ray(r, 0, max_depth);
                
                color = vec3_add(color, sample_color);
            }
            
            // Average samples
            color = vec3_scale(1.0f / (float)samples_per_pixel, color);
            
            // Convert to 8-bit and store
            int idx = (y * width + x) * 3;
            pixels[idx] = gamma_correct(color.z);     // Blue
            pixels[idx + 1] = gamma_correct(color.y); // Green  
            pixels[idx + 2] = gamma_correct(color.x); // Red
        }
    }
    
    // Write the TGA file
    if (!write_tga("out.tga", width, height, pixels)) {
        free(pixels);
        return 1;
    }
    
    free(pixels);
    printf("Raytracer completed successfully.\n");
    printf("Output image saved as out.tga\n");
    return 0;
}
