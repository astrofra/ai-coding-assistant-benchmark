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
    int inside;
} hit_record;

// Sky color calculation
static vec3 sky_color(vec3 ray_dir) {
    // Clamp ray direction Y component to [0, 1]
    float t = powf(fmaxf(0.0f, fminf(1.0f, fabsf(ray_dir.y))), 0.65f);
    
    // Dark red at zenith/nadir
    vec3 dark_red = (vec3){0.18f, 0.01f, 0.015f};
    
    // Orange at horizon
    vec3 orange = (vec3){1.0f, 0.25f, 0.03f};
    
    // Interpolate between colors
    return vec3_add(vec3_scale(1.0f - t, orange), vec3_scale(t, dark_red));
}

// Checkerboard pattern
static vec3 checkerboard_color(vec3 point) {
    // Use floor to handle negative coordinates properly
    int x = (int)floorf(point.x);
    int z = (int)floorf(point.z);
    
    // Alternate between black and white
    if ((x + z) % 2 == 0) {
        return (vec3){1.0f, 1.0f, 1.0f}; // White
    } else {
        return (vec3){0.0f, 0.0f, 0.0f}; // Black
    }
}

// Ray-sphere intersection
static int intersect_sphere(ray r, sphere s, hit_record *hit) {
    vec3 oc = vec3_sub(r.origin, s.center);
    
    // Quadratic equation coefficients: at^2 + bt + c = 0
    float a = vec3_dot(r.direction, r.direction);
    float b = 2.0f * vec3_dot(oc, r.direction);
    float c = vec3_dot(oc, oc) - s.radius * s.radius;
    
    float discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) {
        return 0; // No intersection
    }
    
    // Find the nearest positive root
    float sqrt_discriminant = sqrtf(discriminant);
    float t1 = (-b - sqrt_discriminant) / (2.0f * a);
    float t2 = (-b + sqrt_discriminant) / (2.0f * a);
    
    float t = t1 > 1e-4f ? t1 : t2;
    
    if (t < 1e-4f) {
        return 0; // Intersection too close
    }
    
    // Fill hit record
    hit->t = t;
    hit->point = vec3_add(r.origin, vec3_scale(t, r.direction));
    
    // Normal points outward
    hit->normal = vec3_normalize(vec3_sub(hit->point, s.center));
    
    // Check if ray is inside sphere
    hit->inside = vec3_dot(r.direction, hit->normal) > 0;
    if (hit->inside) {
        hit->normal = vec3_scale(-1.0f, hit->normal);
    }
    
    return 1; // Hit found
}

// Ray-plane intersection
static int intersect_plane(ray r, plane p, hit_record *hit) {
    float denom = vec3_dot(p.normal, r.direction);
    
    if (fabsf(denom) < 1e-6f) {
        return 0; // Ray is parallel to plane
    }
    
    float t = vec3_dot(vec3_sub(p.point, r.origin), p.normal) / denom;
    
    if (t < 1e-4f) {
        return 0; // Intersection too close or behind
    }
    
    // Fill hit record
    hit->t = t;
    hit->point = vec3_add(r.origin, vec3_scale(t, r.direction));
    hit->normal = p.normal;
    
    // Check if ray is inside plane (we assume the plane is at y=0, so normal is (0,1,0))
    hit->inside = vec3_dot(r.direction, hit->normal) > 0;
    if (hit->inside) {
        hit->normal = vec3_scale(-1.0f, hit->normal);
    }
    
    return 1; // Hit found
}

// Fresnel reflection coefficient using Schlick's approximation
static float fresnel_reflectance(float cos_theta, float eta) {
    float r0 = (1.0f - eta) / (1.0f + eta);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * powf(1.0f - cos_theta, 5.0f);
}

// Refract ray using Snell's law
static vec3 refract(vec3 incident, vec3 normal, float eta) {
    float cos_i = vec3_dot(incident, normal);
    float k = 1.0f - eta * eta * (1.0f - cos_i * cos_i);
    
    if (k < 0.0f) {
        return (vec3){0.0f, 0.0f, 0.0f}; // Total internal reflection
    }
    
    return vec3_sub(vec3_scale(eta, incident), 
                    vec3_scale(eta * cos_i + sqrtf(k), normal));
}

// Sample lights for shading
static vec3 shade_lighting(hit_record hit, vec3 light_pos, float light_strength, vec3 view_dir) {
    vec3 light_dir = vec3_normalize(vec3_sub(light_pos, hit.point));
    
    // Shadow ray
    vec3 shadow_origin = vec3_add(hit.point, vec3_scale(1e-4f, hit.normal));
    
    // Simple shadow check - for now we'll just assume no shadows for spheres
    // In a more complex implementation, we'd trace a shadow ray here
    
    // Diffuse lighting (Lambert)
    float NdotL = fmaxf(0.0f, vec3_dot(hit.normal, light_dir));
    
    // Specular lighting (Phong)
    vec3 reflect_dir = vec3_normalize(vec3_sub(vec3_scale(2.0f * NdotL, hit.normal), light_dir));
    float RdotV = fmaxf(0.0f, vec3_dot(reflect_dir, view_dir));
    float specular = powf(RdotV, 32.0f);
    
    // Combine lighting
    vec3 diffuse = vec3_scale(NdotL, hit.mat.albedo);
    vec3 specular_component = vec3_scale(specular, (vec3){1.0f, 1.0f, 1.0f});
    
    return vec3_scale(light_strength, vec3_add(diffuse, specular_component));
}

// Trace a ray through the scene
static vec3 trace_ray(ray r, int depth) {
    // Maximum recursion depth
    if (depth >= 8) {
        return sky_color(r.direction);
    }
    
    hit_record closest_hit = {1e30f, {0,0,0}, {0,0,0}, {{0,0,0}, 0.0f, 0.0f, 1.0f, 0.0f}, 0};
    int hit_anything = 0;
    
    // Define spheres
    sphere glass_sphere = {{-1.25f, 1.0f, 0.0f}, 1.0f};
    sphere chrome_sphere = {{1.25f, 1.0f, 0.0f}, 1.0f};
    
    // Define plane (ground)
    plane ground = {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    
    // Check sphere intersections
    hit_record temp_hit;
    if (intersect_sphere(r, glass_sphere, &temp_hit)) {
        if (temp_hit.t < closest_hit.t) {
            closest_hit = temp_hit;
            closest_hit.mat = (material){
                {1.0f, 1.0f, 1.0f}, // Albedo (clear)
                0.0f,               // Roughness
                0.0f,               // Metallic
                1.5f,               // IOR
                0.9f                // Transmission
            };
            hit_anything = 1;
        }
    }
    
    if (intersect_sphere(r, chrome_sphere, &temp_hit)) {
        if (temp_hit.t < closest_hit.t) {
            closest_hit = temp_hit;
            closest_hit.mat = (material){
                {0.8f, 0.8f, 0.8f}, // Albedo (silver)
                0.0f,               // Roughness
                0.95f,              // Metallic
                1.0f,               // IOR
                0.0f                // Transmission
            };
            hit_anything = 1;
        }
    }
    
    // Check plane intersection
    if (intersect_plane(r, ground, &temp_hit)) {
        if (temp_hit.t < closest_hit.t) {
            closest_hit = temp_hit;
            // Get checkerboard color at this point
            vec3 checker_color = checkerboard_color(closest_hit.point);
            closest_hit.mat = (material){
                checker_color,      // Albedo (checkerboard)
                0.0f,               // Roughness
                0.0f,               // Metallic
                1.0f,               // IOR
                0.0f                // Transmission
            };
            hit_anything = 1;
        }
    }
    
    if (!hit_anything) {
        // No hit - return sky color
        return sky_color(r.direction);
    }
    
    // Handle material properties and lighting
    vec3 color = {0.0f, 0.0f, 0.0f};
    vec3 view_dir = vec3_normalize(vec3_scale(-1.0f, r.direction));
    
    // Ambient lighting
    color = vec3_add(color, vec3_scale(0.1f, closest_hit.mat.albedo));
    
    // Light positions (three-point lighting)
    vec3 lights[3] = {
        {-3.5f, 6.0f, 4.5f},   // Key light
        {4.0f, 3.0f, 3.0f},    // Fill light
        {0.5f, 5.0f, -4.0f}    // Back light
    };
    
    float light_strengths[3] = {1.0f, 0.35f, 0.7f};
    vec3 light_colors[3] = {
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f}
    };
    
    // Apply lighting from each light
    for (int i = 0; i < 3; i++) {
        color = vec3_add(color, shade_lighting(closest_hit, lights[i], light_strengths[i], view_dir));
    }
    
    // Handle reflections and refractions
    if (closest_hit.mat.metallic > 0.9f) {
        // Chrome sphere - mirror reflection
        vec3 reflect_dir = vec3_normalize(vec3_sub(r.direction, 
                                                   vec3_scale(2.0f * vec3_dot(r.direction, closest_hit.normal), 
                                                             closest_hit.normal)));
        ray reflect_ray = {vec3_add(closest_hit.point, vec3_scale(1e-4f, closest_hit.normal)), reflect_dir};
        vec3 reflect_color = trace_ray(reflect_ray, depth + 1);
        color = vec3_add(vec3_scale(closest_hit.mat.metallic, reflect_color),
                         vec3_scale(1.0f - closest_hit.mat.metallic, color));
    } else if (closest_hit.mat.transmission > 0.0f) {
        // Glass sphere - refraction
        float eta = closest_hit.mat.ior; // IOR of glass
        vec3 refract_dir = refract(r.direction, closest_hit.normal, eta);
        
        if (vec3_length(refract_dir) > 1e-6f) {
            // Refraction ray
            ray refract_ray = {vec3_add(closest_hit.point, vec3_scale(1e-4f, closest_hit.normal)), refract_dir};
            vec3 refract_color = trace_ray(refract_ray, depth + 1);
            
            // Reflection ray
            vec3 reflect_dir = vec3_normalize(vec3_sub(r.direction, 
                                                       vec3_scale(2.0f * vec3_dot(r.direction, closest_hit.normal), 
                                                                 closest_hit.normal)));
            ray reflect_ray = {vec3_add(closest_hit.point, vec3_scale(-1e-4f, closest_hit.normal)), reflect_dir};
            vec3 reflect_color = trace_ray(reflect_ray, depth + 1);
            
            // Fresnel mixing
            float cos_theta = vec3_dot(r.direction, closest_hit.normal);
            if (closest_hit.inside) {
                cos_theta = -cos_theta;
            }
            float reflectance = fresnel_reflectance(cos_theta, eta);
            
            color = vec3_add(vec3_scale(1.0f - closest_hit.mat.transmission, reflect_color),
                             vec3_scale(closest_hit.mat.transmission, refract_color));
            color = vec3_add(vec3_scale(reflectance, reflect_color),
                             vec3_scale(1.0f - reflectance, color));
        } else {
            // Total internal reflection
            vec3 reflect_dir = vec3_normalize(vec3_sub(r.direction, 
                                                       vec3_scale(2.0f * vec3_dot(r.direction, closest_hit.normal), 
                                                                 closest_hit.normal)));
            ray reflect_ray = {vec3_add(closest_hit.point, vec3_scale(1e-4f, closest_hit.normal)), reflect_dir};
            vec3 reflect_color = trace_ray(reflect_ray, depth + 1);
            color = reflect_color;
        }
    }
    
    // Apply fog effect
    float transmittance = expf(-0.035f * closest_hit.t);
    vec3 fog_color = sky_color(r.direction);
    color = vec3_add(vec3_scale(transmittance, color),
                     vec3_scale(1.0f - transmittance, fog_color));
    
    return color;
}

// Convert linear RGB to sRGB with gamma correction
static uint8_t linear_to_srgb(float c) {
    if (c <= 0.0031308f) {
        return (uint8_t)(c * 12.92f * 255.0f + 0.5f);
    } else {
        return (uint8_t)((1.055f * powf(c, 1.0f / 2.4f) - 0.055f) * 255.0f + 0.5f);
    }
}

// Save image as TGA file
static int save_tga(const char* filename, uint8_t* pixels, int width, int height) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        return 0;
    }
    
    // TGA Header (18 bytes)
    uint8_t header[18] = {0};
    header[2] = 2; // Image type: uncompressed truecolor
    header[12] = width & 0xFF;          // Width (low byte)
    header[13] = (width >> 8) & 0xFF;   // Width (high byte)
    header[14] = height & 0xFF;         // Height (low byte)
    header[15] = (height >> 8) & 0xFF;  // Height (high byte)
    header[16] = 24; // Pixel depth
    header[17] = 0x20; // Descriptor: top-left origin, left-to-right order
    
    fwrite(header, 1, 18, fp);
    
    // Write pixel data (BGR format, top to bottom)
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            // Write BGR (not RGB)
            fputc(pixels[idx + 2], fp); // Blue
            fputc(pixels[idx + 1], fp); // Green
            fputc(pixels[idx + 0], fp); // Red
        }
    }
    
    fclose(fp);
    return 1;
}

int main(void) {
    // Image dimensions
    const int width = 1280;
    const int height = 720;
    
    // Allocate pixel buffer
    uint8_t* pixels = (uint8_t*)malloc(width * height * 3);
    if (!pixels) {
        fprintf(stderr, "Failed to allocate pixel buffer\n");
        return 1;
    }
    
    // Camera setup
    vec3 camera_pos = {0.0f, 2.6f, 6.5f};
    vec3 camera_target = {0.0f, 1.0f, 0.0f};
    vec3 camera_up = {0.0f, 1.0f, 0.0f};
    
    // Calculate camera coordinate system
    vec3 forward = vec3_normalize(vec3_sub(camera_target, camera_pos));
    vec3 right = vec3_normalize(vec3_cross(forward, camera_up));
    vec3 up = vec3_cross(right, forward);
    
    // Field of view and aspect ratio
    float fov = 45.0f * M_PI / 180.0f;
    float aspect_ratio = (float)width / (float)height;
    
    // Calculate the half-height of the view plane at distance 1
    float half_height = tanf(fov / 2.0f);
    float half_width = half_height * aspect_ratio;
    
    // Render the image with supersampling
    const int samples_per_pixel = 4; // 2x2 sub-pixel sampling
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            vec3 pixel_color = {0.0f, 0.0f, 0.0f};
            
            // Sub-pixel sampling
            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    // Calculate sub-pixel position
                    float u = (x + (sx + 0.5f) / 2.0f) / width;
                    float v = (y + (sy + 0.5f) / 2.0f) / height;
                    
                    // Map to [-1, 1] range
                    float s = (2.0f * u - 1.0f) * half_width;
                    float t = (1.0f - 2.0f * v) * half_height;
                    
                    // Ray direction
                    vec3 ray_dir = vec3_normalize(vec3_add(
                        vec3_scale(s, right),
                        vec3_add(vec3_scale(t, up), forward)
                    ));
                    
                    ray r = {camera_pos, ray_dir};
                    vec3 color = trace_ray(r, 0);
                    
                    // Accumulate color
                    pixel_color = vec3_add(pixel_color, color);
                }
            }
            
            // Average the samples
            pixel_color = vec3_scale(1.0f / samples_per_pixel, pixel_color);
            
            // Convert to 8-bit and store
            int idx = (y * width + x) * 3;
            pixels[idx + 0] = linear_to_srgb(pixel_color.x); // Red
            pixels[idx + 1] = linear_to_srgb(pixel_color.y); // Green
            pixels[idx + 2] = linear_to_srgb(pixel_color.z); // Blue
        }
    }
    
    // Save the image
    if (!save_tga("out.tga", pixels, width, height)) {
        fprintf(stderr, "Failed to save TGA file\n");
        free(pixels);
        return 1;
    }
    
    free(pixels);
    printf("Raytracer completed successfully.\n");
    printf("Generated image: out.tga\n");
    
    return 0;
}
