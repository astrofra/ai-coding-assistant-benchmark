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
    vec3 normal;
    float distance; // distance from origin along normal
} plane;

// Material structure
typedef struct {
    vec3 albedo;
    float reflectivity;
    float transparency;
    float ior; // index of refraction
} material;

// Light structure
typedef struct {
    vec3 position;
    vec3 intensity;
} light;

// Scene configuration
static const vec3 camera_pos = {0.0f, 2.6f, 6.5f};
static const vec3 camera_target = {0.0f, 1.0f, 0.0f};
static const vec3 camera_up = {0.0f, 1.0f, 0.0f};
static const float fov_y = 45.0f;
static const int width = 1280;
static const int height = 720;

// Glass sphere
static const sphere glass_sphere = {{-1.25f, 1.0f, 0.0f}, 1.0f};

// Chrome sphere
static const sphere chrome_sphere = {{1.25f, 1.0f, 0.0f}, 1.0f};

// Ground plane
static const plane ground_plane = {{0.0f, 1.0f, 0.0f}, 0.0f}; // y = 0

// Lights
static const light lights[] = {
    {{-3.5f, 6.0f, 4.5f}, {1.0f, 1.0f, 1.0f}},   // Key light
    {{4.0f, 3.0f, 3.0f}, {0.35f, 0.35f, 0.35f}},  // Fill light
    {{0.5f, 5.0f, -4.0f}, {0.7f, 0.7f, 0.7f}}     // Back light
};

// Sky colors
static const vec3 dark_red = {0.18f, 0.01f, 0.015f};
static const vec3 orange = {1.0f, 0.25f, 0.03f};

// Constants
static const float kEpsilon = 1e-4f;
static const float kMaxDepth = 8;
static const float kFogDensity = 0.035f;

// Sky color computation
static vec3 sky_color(vec3 ray_dir) {
    // Clamp y component to [0, 1] for numerical robustness
    float y = fmaxf(0.0f, fminf(1.0f, fabsf(ray_dir.y)));
    
    // Power curve for smooth gradient
    float t = powf(y, 0.65f);
    
    // Linear interpolation between dark red and orange
    return vec3_add(
        vec3_scale(1.0f - t, dark_red),
        vec3_scale(t, orange)
    );
}

// Ray-sphere intersection
static int intersect_sphere(ray r, sphere s, float *t) {
    vec3 oc = vec3_sub(r.origin, s.center);
    float a = vec3_dot(r.direction, r.direction);
    float b = 2.0f * vec3_dot(oc, r.direction);
    float c = vec3_dot(oc, oc) - s.radius * s.radius;
    float discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) {
        return 0; // No intersection
    }
    
    float sqrt_discriminant = sqrtf(discriminant);
    float t1 = (-b - sqrt_discriminant) / (2.0f * a);
    float t2 = (-b + sqrt_discriminant) / (2.0f * a);
    
    // Return the closest positive intersection
    if (t1 > kEpsilon) {
        *t = t1;
        return 1;
    } else if (t2 > kEpsilon) {
        *t = t2;
        return 1;
    }
    
    return 0; // Intersection behind ray origin
}

// Ray-plane intersection
static int intersect_plane(ray r, plane p, float *t) {
    float denom = vec3_dot(p.normal, r.direction);
    
    if (fabsf(denom) < kEpsilon) {
        return 0; // Ray is parallel to plane
    }
    
    *t = (p.distance - vec3_dot(p.normal, r.origin)) / denom;
    
    if (*t > kEpsilon) {
        return 1; // Valid intersection
    }
    
    return 0; // Intersection behind ray origin
}

// Compute checkerboard pattern
static vec3 checkerboard_pattern(float x, float z) {
    int ix = (int)floorf(x);
    int iz = (int)floorf(z);
    
    // Ensure alternating pattern even for negative coordinates
    if ((ix + iz) % 2 == 0) {
        return (vec3){1.0f, 1.0f, 1.0f}; // White
    } else {
        return (vec3){0.0f, 0.0f, 0.0f}; // Black
    }
}

// Compute diffuse lighting with Lambertian shading
static vec3 compute_diffuse_lighting(vec3 point, vec3 normal, vec3 light_pos, vec3 light_intensity) {
    vec3 light_dir = vec3_normalize(vec3_sub(light_pos, point));
    float diffuse = fmaxf(0.0f, vec3_dot(normal, light_dir));
    
    return vec3_scale(diffuse, light_intensity);
}

// Compute Fresnel reflectance using Schlick's approximation
static float fresnel_reflectance(float cos_theta, float eta_ratio) {
    float r0 = (1.0f - eta_ratio) / (1.0f + eta_ratio);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * powf(1.0f - cos_theta, 5.0f);
}

// Trace a ray through the scene
static vec3 trace_ray(ray r, int depth) {
    if (depth >= kMaxDepth) {
        return sky_color(r.direction);
    }
    
    float closest_t = 1e30f;
    int hit_sphere = 0;
    int hit_ground = 0;
    vec3 hit_point, hit_normal;
    material mat;
    
    // Check for sphere intersections
    float t;
    if (intersect_sphere(r, glass_sphere, &t) && t < closest_t) {
        closest_t = t;
        hit_sphere = 1;
        hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
        hit_normal = vec3_normalize(vec3_sub(hit_point, glass_sphere.center));
        mat = (material){{1.0f, 1.0f, 1.0f}, 0.0f, 1.0f, 1.5f}; // Glass
    }
    
    if (intersect_sphere(r, chrome_sphere, &t) && t < closest_t) {
        closest_t = t;
        hit_sphere = 1;
        hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
        hit_normal = vec3_normalize(vec3_sub(hit_point, chrome_sphere.center));
        mat = (material){{0.8f, 0.8f, 0.8f}, 0.95f, 0.0f, 1.0f}; // Chrome
    }
    
    // Check for ground plane intersection
    if (intersect_plane(r, ground_plane, &t) && t < closest_t) {
        closest_t = t;
        hit_ground = 1;
        hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
        hit_normal = (vec3){0.0f, 1.0f, 0.0f}; // Upward facing normal
        
        // Get checkerboard pattern
        vec3 pattern_color = checkerboard_pattern(hit_point.x, hit_point.z);
        mat = (material){pattern_color, 0.0f, 0.0f, 1.0f}; // Diffuse ground
    }
    
    // No intersection - return sky color
    if (!hit_sphere && !hit_ground) {
        return sky_color(r.direction);
    }
    
    // Shadow calculation and lighting
    vec3 color = (vec3){0.0f, 0.0f, 0.0f};
    
    if (hit_sphere) {
        // For spheres, we apply lighting and reflections/refractions
        // Compute lighting from all lights
        for (int i = 0; i < 3; i++) {
            vec3 light_dir = vec3_normalize(vec3_sub(lights[i].position, hit_point));
            
            // Shadow ray test
            ray shadow_ray = {vec3_add(hit_point, vec3_scale(kEpsilon, hit_normal)), light_dir};
            float shadow_t;
            
            // Check if there's an intersection with any object
            int in_shadow = 0;
            
            // Check glass sphere
            if (intersect_sphere(shadow_ray, glass_sphere, &shadow_t) && shadow_t < vec3_length(vec3_sub(lights[i].position, hit_point))) {
                in_shadow = 1;
            }
            
            // Check chrome sphere
            if (!in_shadow && intersect_sphere(shadow_ray, chrome_sphere, &shadow_t) && shadow_t < vec3_length(vec3_sub(lights[i].position, hit_point))) {
                in_shadow = 1;
            }
            
            // Check ground plane
            if (!in_shadow && intersect_plane(shadow_ray, ground_plane, &shadow_t) && shadow_t < vec3_length(vec3_sub(lights[i].position, hit_point))) {
                in_shadow = 1;
            }
            
            if (!in_shadow) {
                // Compute diffuse lighting
                float diffuse = fmaxf(0.0f, vec3_dot(hit_normal, light_dir));
                
                // Specular highlight for chrome
                vec3 view_dir = vec3_normalize(vec3_sub(camera_pos, hit_point));
                vec3 reflect_dir = vec3_normalize(vec3_sub(vec3_scale(2.0f * diffuse, hit_normal), light_dir));
                float spec = powf(fmaxf(0.0f, vec3_dot(view_dir, reflect_dir)), 32.0f);
                
                if (mat.reflectivity > 0.0f) {
                    // Chrome has strong specular highlights
                    color = vec3_add(color, vec3_scale(lights[i].intensity.x * diffuse, mat.albedo));
                    color = vec3_add(color, vec3_scale(lights[i].intensity.x * spec, (vec3){1.0f, 1.0f, 1.0f}));
                } else {
                    // Glass has diffuse lighting
                    color = vec3_add(color, vec3_scale(lights[i].intensity.x * diffuse, mat.albedo));
                }
            }
        }
        
        // Apply material properties
        if (mat.reflectivity > 0.0f) {
            // Chrome reflection
            vec3 reflect_dir = vec3_normalize(vec3_sub(r.direction, vec3_scale(2.0f * vec3_dot(r.direction, hit_normal), hit_normal)));
            ray reflect_ray = {vec3_add(hit_point, vec3_scale(kEpsilon, hit_normal)), reflect_dir};
            vec3 reflect_color = trace_ray(reflect_ray, depth + 1);
            color = vec3_add(vec3_scale(mat.reflectivity, reflect_color), vec3_scale(1.0f - mat.reflectivity, color));
        } else if (mat.transparency > 0.0f) {
            // Glass refraction
            float cos_i = vec3_dot(hit_normal, r.direction);
            float eta = 1.0f / mat.ior; // Air to glass
            
            if (cos_i < 0) {
                // Ray entering the sphere
                cos_i = -cos_i;
            } else {
                // Ray exiting the sphere
                eta = mat.ior;
                hit_normal = vec3_scale(-1.0f, hit_normal);
            }
            
            float sin_t2 = eta * eta * (1.0f - cos_i * cos_i);
            
            if (sin_t2 <= 1.0f) {
                // Refraction occurs
                float cos_t = sqrtf(1.0f - sin_t2);
                vec3 refract_dir = vec3_add(vec3_scale(eta, r.direction), vec3_scale(eta * cos_i - cos_t, hit_normal));
                refract_dir = vec3_normalize(refract_dir);
                
                ray refract_ray = {vec3_add(hit_point, vec3_scale(kEpsilon, hit_normal)), refract_dir};
                vec3 refract_color = trace_ray(refract_ray, depth + 1);
                
                // Fresnel effect
                float reflectance = fresnel_reflectance(cos_i, eta);
                color = vec3_add(vec3_scale(1.0f - reflectance, refract_color), vec3_scale(reflectance, color));
            } else {
                // Total internal reflection
                vec3 reflect_dir = vec3_normalize(vec3_sub(r.direction, vec3_scale(2.0f * vec3_dot(r.direction, hit_normal), hit_normal)));
                ray reflect_ray = {vec3_add(hit_point, vec3_scale(kEpsilon, hit_normal)), reflect_dir};
                vec3 reflect_color = trace_ray(reflect_ray, depth + 1);
                color = reflect_color;
            }
        }
    } else if (hit_ground) {
        // Ground plane - only diffuse lighting
        for (int i = 0; i < 3; i++) {
            vec3 light_dir = vec3_normalize(vec3_sub(lights[i].position, hit_point));
            
            // Shadow ray test
            ray shadow_ray = {vec3_add(hit_point, vec3_scale(kEpsilon, hit_normal)), light_dir};
            float shadow_t;
            
            // Check if there's an intersection with any object
            int in_shadow = 0;
            
            // Check glass sphere
            if (intersect_sphere(shadow_ray, glass_sphere, &shadow_t) && shadow_t < vec3_length(vec3_sub(lights[i].position, hit_point))) {
                in_shadow = 1;
            }
            
            // Check chrome sphere
            if (!in_shadow && intersect_sphere(shadow_ray, chrome_sphere, &shadow_t) && shadow_t < vec3_length(vec3_sub(lights[i].position, hit_point))) {
                in_shadow = 1;
            }
            
            if (!in_shadow) {
                // Compute diffuse lighting
                float diffuse = fmaxf(0.0f, vec3_dot(hit_normal, light_dir));
                color = vec3_add(color, vec3_scale(lights[i].intensity.x * diffuse, mat.albedo));
            }
        }
    }
    
    // Apply fog effect
    float distance = vec3_length(vec3_sub(hit_point, camera_pos));
    float transmittance = expf(-kFogDensity * distance);
    vec3 fog_color = sky_color(r.direction);
    
    if (hit_sphere) {
        color = vec3_add(vec3_scale(transmittance, color), vec3_scale(1.0f - transmittance, fog_color));
    } else {
        // For ground, apply fog differently
        color = vec3_add(vec3_scale(transmittance, color), vec3_scale(1.0f - transmittance, fog_color));
    }
    
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

// Write TGA file
static int write_tga(const char* filename, uint8_t* pixels, int width, int height) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open %s for writing\n", filename);
        return 0;
    }
    
    // TGA Header (18 bytes)
    uint8_t header[18] = {0};
    header[2] = 2; // Image type: Uncompressed True Color
    header[12] = width & 0xFF;         // Width (low byte)
    header[13] = (width >> 8) & 0xFF;  // Width (high byte)
    header[14] = height & 0xFF;        // Height (low byte)
    header[15] = (height >> 8) & 0xFF; // Height (high byte)
    header[16] = 24; // Pixel depth
    header[17] = 0x20; // Image descriptor: Top-left origin, left-to-right pixel order
    
    if (fwrite(header, 1, 18, fp) != 18) {
        fclose(fp);
        fprintf(stderr, "Error: Failed to write TGA header\n");
        return 0;
    }
    
    // Write pixel data (BGR order, top to bottom)
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
    // Allocate memory for pixel buffer
    uint8_t* pixels = (uint8_t*)malloc(width * height * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Cannot allocate pixel buffer\n");
        return 1;
    }
    
    // Calculate camera setup
    vec3 forward = vec3_normalize(vec3_sub(camera_target, camera_pos));
    vec3 right = vec3_normalize(vec3_cross(forward, camera_up));
    vec3 up = vec3_cross(right, forward);
    
    // Calculate aspect ratio and FOV
    float aspect_ratio = (float)width / (float)height;
    float fov_rad = fov_y * M_PI / 180.0f;
    float half_height = tanf(fov_rad / 2.0f);
    float half_width = half_height * aspect_ratio;
    
    // Render the image with supersampling
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            vec3 pixel_color = (vec3){0.0f, 0.0f, 0.0f};
            
            // 2x2 supersampling
            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    // Calculate subpixel position
                    float u = (x + (sx + 0.5f) / 2.0f) / width;
                    float v = (y + (sy + 0.5f) / 2.0f) / height;
                    
                    // Scale to [-1, 1] range
                    float scaled_u = (u - 0.5f) * 2.0f;
                    float scaled_v = (v - 0.5f) * 2.0f;
                    
                    // Calculate ray direction
                    vec3 ray_dir = vec3_normalize(vec3_add(
                        vec3_add(vec3_scale(half_width * scaled_u, right),
                                vec3_scale(half_height * scaled_v, up)),
                        forward
                    ));
                    
                    ray r = {camera_pos, ray_dir};
                    vec3 color = trace_ray(r, 0);
                    
                    // Accumulate color
                    pixel_color = vec3_add(pixel_color, color);
                }
            }
            
            // Average the supersampled colors
            pixel_color = vec3_scale(0.25f, pixel_color);
            
            // Convert to 8-bit and store
            int idx = (y * width + x) * 3;
            pixels[idx] = gamma_correct(pixel_color.z);     // Blue
            pixels[idx + 1] = gamma_correct(pixel_color.y); // Green  
            pixels[idx + 2] = gamma_correct(pixel_color.x); // Red
        }
    }
    
    // Write the TGA file
    if (!write_tga("out.tga", pixels, width, height)) {
        free(pixels);
        return 1;
    }
    
    free(pixels);
    return 0;
}
