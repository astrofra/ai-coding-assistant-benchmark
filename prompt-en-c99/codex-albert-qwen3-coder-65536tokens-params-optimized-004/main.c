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

static inline float vec3_length_sq(vec3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static inline float vec3_length(vec3 v) {
    return sqrtf(vec3_length_sq(v));
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
    float ior; // Index of refraction
    int is_glass;
} material;

// Scene objects
sphere spheres[] = {
    {{-1.25f, 1.0f, 0.0f}, 1.0f},  // Glass sphere
    {{1.25f, 1.0f, 0.0f}, 1.0f}   // Chrome sphere
};

material materials[] = {
    {{1.0f, 1.0f, 1.0f}, 0.0f, 1.5f, 1},  // Glass
    {{0.8f, 0.8f, 0.8f}, 0.95f, 0.0f, 0}, // Chrome
};

// Light structure
typedef struct {
    vec3 position;
    vec3 intensity;
} light;

light lights[] = {
    {{-3.5f, 6.0f, 4.5f}, {1.0f, 1.0f, 1.0f}},   // Key light
    {{4.0f, 3.0f, 3.0f}, {0.35f, 0.35f, 0.35f}},  // Fill light
    {{0.5f, 5.0f, -4.0f}, {0.7f, 0.7f, 0.7f}}     // Back light
};

// Sky color calculation
vec3 sky_color(vec3 direction) {
    // Clamp direction.y to [0, 1] for numerical robustness
    float t = powf(fmaxf(0.0f, fminf(1.0f, fabsf(direction.y))), 0.65f);
    
    vec3 dark_red = {0.18f, 0.01f, 0.015f};
    vec3 orange = {1.0f, 0.25f, 0.03f};
    
    return vec3_add(vec3_scale(1.0f - t, orange), vec3_scale(t, dark_red));
}

// Checkerboard pattern
vec3 checkerboard_color(vec3 point) {
    int x = (int)floorf(point.x);
    int z = (int)floorf(point.z);
    
    // Alternate black and white squares
    if ((x + z) % 2 == 0) {
        return (vec3){1.0f, 1.0f, 1.0f};  // White
    } else {
        return (vec3){0.0f, 0.0f, 0.0f};  // Black
    }
}

// Ray-sphere intersection
int intersect_sphere(ray r, sphere s, float *t) {
    vec3 oc = vec3_sub(r.origin, s.center);
    float a = vec3_dot(r.direction, r.direction);
    float b = 2.0f * vec3_dot(oc, r.direction);
    float c = vec3_dot(oc, oc) - s.radius * s.radius;
    float discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) {
        return 0;  // No intersection
    }
    
    // Find the nearest positive root
    float sqrt_discriminant = sqrtf(discriminant);
    float t1 = (-b - sqrt_discriminant) / (2.0f * a);
    float t2 = (-b + sqrt_discriminant) / (2.0f * a);
    
    if (t1 > 1e-4f) {
        *t = t1;
        return 1;
    } else if (t2 > 1e-4f) {
        *t = t2;
        return 1;
    }
    
    return 0;  // Intersection behind ray origin
}

// Ray-plane intersection
int intersect_plane(ray r, plane p, float *t) {
    float denom = vec3_dot(p.normal, r.direction);
    if (fabsf(denom) < 1e-6f) {
        return 0;  // Ray is parallel to plane
    }
    
    vec3 op = vec3_sub(p.point, r.origin);
    *t = vec3_dot(op, p.normal) / denom;
    
    if (*t > 1e-4f) {
        return 1;
    }
    
    return 0;  // Intersection behind ray origin
}

// Fresnel reflection coefficient (Schlick's approximation)
float fresnel(float cos_theta, float eta_ratio) {
    float r0 = (1.0f - eta_ratio) / (1.0f + eta_ratio);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * powf(1.0f - cos_theta, 5.0f);
}

// Reflect a vector around a normal
vec3 reflect(vec3 incident, vec3 normal) {
    return vec3_sub(incident, vec3_scale(2.0f * vec3_dot(incident, normal), normal));
}

// Refract a vector using Snell's law
vec3 refract(vec3 incident, vec3 normal, float eta_ratio) {
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
vec3 trace_ray(ray r, int depth) {
    if (depth > 8) {
        return sky_color(r.direction);
    }
    
    float closest_t = 1e20f;
    int hit_sphere = -1;
    int hit_plane = 0;
    float plane_t = 0.0f;
    
    // Test sphere intersections
    for (int i = 0; i < 2; i++) {
        float t;
        if (intersect_sphere(r, spheres[i], &t) && t < closest_t) {
            closest_t = t;
            hit_sphere = i;
        }
    }
    
    // Test plane intersection
    plane ground = {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    if (intersect_plane(r, ground, &plane_t) && plane_t < closest_t) {
        closest_t = plane_t;
        hit_plane = 1;
    }
    
    // No intersection
    if (closest_t > 1e19f) {
        return sky_color(r.direction);
    }
    
    // Calculate intersection point
    vec3 hit_point = vec3_add(r.origin, vec3_scale(closest_t, r.direction));
    
    // Calculate normal
    vec3 normal;
    if (hit_plane) {
        normal = ground.normal;
        // Ensure normal points upward
        if (vec3_dot(normal, r.direction) > 0) {
            normal = vec3_scale(-1.0f, normal);
        }
    } else {
        normal = vec3_normalize(vec3_sub(hit_point, spheres[hit_sphere].center));
        // Ensure normal points outward
        if (vec3_dot(normal, r.direction) > 0) {
            normal = vec3_scale(-1.0f, normal);
        }
    }
    
    // Get material properties
    material mat = materials[hit_sphere >= 0 ? hit_sphere : 1];
    
    // Shadow calculation
    vec3 lighting = {0.0f, 0.0f, 0.0f};
    
    // Ambient lighting
    lighting = vec3_add(lighting, vec3_scale(0.1f, mat.albedo));
    
    // For each light
    for (int i = 0; i < 3; i++) {
        vec3 light_dir = vec3_normalize(vec3_sub(lights[i].position, hit_point));
        float light_distance = vec3_length(vec3_sub(lights[i].position, hit_point));
        
        // Shadow ray
        ray shadow_ray = {hit_point, light_dir};
        float shadow_t = 1e20f;
        int in_shadow = 0;
        
        // Check if we're hitting something before the light
        if (hit_sphere >= 0) {
            // Check if the shadow ray hits the same sphere
            if (intersect_sphere(shadow_ray, spheres[hit_sphere], &shadow_t)) {
                if (shadow_t < light_distance - 1e-4f) {
                    in_shadow = 1;
                }
            }
            
            // Check if the shadow ray hits the ground
            if (!in_shadow && intersect_plane(shadow_ray, ground, &shadow_t)) {
                if (shadow_t < light_distance - 1e-4f) {
                    in_shadow = 1;
                }
            }
        } else {
            // Check if the shadow ray hits the ground
            if (intersect_plane(shadow_ray, ground, &shadow_t)) {
                if (shadow_t < light_distance - 1e-4f) {
                    in_shadow = 1;
                }
            }
        }
        
        if (!in_shadow) {
            // Diffuse lighting
            float diff = fmaxf(0.0f, vec3_dot(light_dir, normal));
            vec3 diffuse = vec3_scale(diff, lights[i].intensity);
            
            // Specular lighting (Phong)
            vec3 view_dir = vec3_normalize(vec3_scale(-1.0f, r.direction));
            vec3 reflect_dir = reflect(vec3_scale(-1.0f, light_dir), normal);
            float spec = powf(fmaxf(0.0f, vec3_dot(view_dir, reflect_dir)), 32.0f);
            vec3 specular = vec3_scale(spec, lights[i].intensity);
            
            // Combine diffuse and specular
            vec3 light_contribution = vec3_add(diffuse, specular);
            
            // Apply material color
            if (hit_sphere >= 0) {
                if (mat.is_glass) {
                    // For glass, we want to apply lighting to the transmitted light
                    lighting = vec3_add(lighting, vec3_mul(mat.albedo, light_contribution));
                } else {
                    lighting = vec3_add(lighting, vec3_mul(mat.albedo, light_contribution));
                }
            } else {
                // Ground lighting
                vec3 ground_color = checkerboard_color(hit_point);
                lighting = vec3_add(lighting, vec3_mul(ground_color, light_contribution));
            }
        }
    }
    
    // Handle reflection/refraction
    if (mat.reflectivity > 0.0f) {
        vec3 reflect_color = {0.0f, 0.0f, 0.0f};
        
        if (hit_sphere >= 0 && mat.is_glass) {
            // Glass sphere - handle refraction
            vec3 view_dir = vec3_normalize(vec3_scale(-1.0f, r.direction));
            float cos_i = vec3_dot(normal, view_dir);
            float eta_ratio = 1.0f / mat.ior;  // Air to glass
            
            if (cos_i < 0) {
                // Entering the sphere
                cos_i = -cos_i;
            } else {
                // Exiting the sphere
                eta_ratio = 1.0f / eta_ratio;
                normal = vec3_scale(-1.0f, normal);
            }
            
            // Calculate reflection and refraction
            vec3 reflect_dir = reflect(view_dir, normal);
            vec3 refract_dir = refract(view_dir, normal, eta_ratio);
            
            // Fresnel coefficients
            float kr = fresnel(cos_i, eta_ratio);
            
            // Trace reflection and refraction rays
            ray reflect_ray = {hit_point, reflect_dir};
            ray refract_ray = {hit_point, refract_dir};
            
            // Offset the rays to avoid self-intersection
            reflect_ray.origin = vec3_add(reflect_ray.origin, vec3_scale(1e-4f, normal));
            refract_ray.origin = vec3_add(refract_ray.origin, vec3_scale(-1e-4f, normal));
            
            vec3 reflect_color = trace_ray(reflect_ray, depth + 1);
            vec3 refract_color = trace_ray(refract_ray, depth + 1);
            
            // Combine reflection and refraction with Fresnel weights
            reflect_color = vec3_scale(kr, reflect_color);
            refract_color = vec3_scale(1.0f - kr, refract_color);
            
            lighting = vec3_add(reflect_color, refract_color);
        } else {
            // Mirror sphere - handle reflection
            vec3 reflect_dir = reflect(r.direction, normal);
            ray reflect_ray = {hit_point, reflect_dir};
            
            // Offset the ray to avoid self-intersection
            reflect_ray.origin = vec3_add(reflect_ray.origin, vec3_scale(1e-4f, normal));
            
            vec3 reflect_color = trace_ray(reflect_ray, depth + 1);
            lighting = vec3_add(vec3_scale(mat.reflectivity, reflect_color),
                               vec3_scale(1.0f - mat.reflectivity, lighting));
        }
    }
    
    // Apply fog effect
    vec3 fog_color = sky_color(r.direction);
    float transmittance = expf(-0.035f * closest_t);
    
    // For ground hit, use the checkerboard color
    if (hit_plane) {
        vec3 ground_color = checkerboard_color(hit_point);
        lighting = vec3_mul(lighting, ground_color);
    }
    
    vec3 result = vec3_add(vec3_scale(transmittance, lighting),
                          vec3_scale(1.0f - transmittance, fog_color));
    
    return result;
}

// Convert RGB to sRGB gamma corrected
uint8_t to_gamma_corrected(float c) {
    if (c <= 0.0031308f) {
        return (uint8_t)(c * 12.92f * 255.0f + 0.5f);
    } else {
        return (uint8_t)((1.055f * powf(c, 1.0f / 2.4f) - 0.055f) * 255.0f + 0.5f);
    }
}

// Save TGA file
int save_tga(const char* filename, uint8_t* pixels, int width, int height) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Could not open file %s for writing\n", filename);
        return 0;
    }
    
    // TGA Header (18 bytes)
    uint8_t header[18] = {0};
    header[2] = 2;  // Image type: uncompressed truecolor
    header[12] = width & 0xFF;          // Width LSB
    header[13] = (width >> 8) & 0xFF;   // Width MSB
    header[14] = height & 0xFF;         // Height LSB
    header[15] = (height >> 8) & 0xFF;  // Height MSB
    header[16] = 24;  // Pixel depth
    header[17] = 0x20;  // Descriptor: top-left origin, left-to-right pixel order
    
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
    const int width = 1280;
    const int height = 720;
    const int num_samples = 4;  // 2x2 subpixel sampling

    // Allocate pixel buffer
    uint8_t* pixels = malloc(width * height * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Failed to allocate pixel buffer\n");
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
    
    // For each pixel
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            vec3 pixel_color = {0.0f, 0.0f, 0.0f};
            
            // Subpixel sampling
            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    // Calculate subpixel position
                    float u = (x + (sx + 0.5f) / 2.0f) / width;
                    float v = (y + (sy + 0.5f) / 2.0f) / height;
                    
                    // Convert to normalized device coordinates
                    float ndc_x = (2.0f * u - 1.0f) * tanf(fov / 2.0f) * aspect_ratio;
                    float ndc_y = (1.0f - 2.0f * v) * tanf(fov / 2.0f);
                    
                    // Ray direction in world space
                    vec3 ray_dir = vec3_normalize(vec3_add(
                        vec3_scale(ndc_x, right),
                        vec3_add(vec3_scale(ndc_y, up), forward)
                    ));
                    
                    // Create ray
                    ray r = {camera_pos, ray_dir};
                    
                    // Trace ray
                    vec3 color = trace_ray(r, 0);
                    
                    // Accumulate color (linear RGB)
                    pixel_color = vec3_add(pixel_color, color);
                }
            }
            
            // Average the samples
            pixel_color = vec3_scale(1.0f / 4.0f, pixel_color);
            
            // Convert to 8-bit sRGB
            int idx = (y * width + x) * 3;
            pixels[idx] = to_gamma_corrected(pixel_color.z);     // Blue
            pixels[idx + 1] = to_gamma_corrected(pixel_color.y); // Green  
            pixels[idx + 2] = to_gamma_corrected(pixel_color.x); // Red
        }
    }
    
    // Save the image
    if (!save_tga("out.tga", pixels, width, height)) {
        free(pixels);
        return 1;
    }
    
    free(pixels);
    printf("Raytracer completed successfully. Image saved as out.tga\n");
    return 0;
}
