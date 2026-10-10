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
    vec3 normal;
    float distance; // distance from origin along normal
} plane;

// Material structure
typedef struct {
    vec3 albedo;
    float reflectivity;
    float refractivity;
    float ior; // index of refraction
} material;

// Light structure
typedef struct {
    vec3 position;
    vec3 intensity;
} light;

// Scene parameters
static const vec3 camera_pos = {0.0f, 2.6f, 6.5f};
static const vec3 camera_target = {0.0f, 1.0f, 0.0f};
static const vec3 camera_up = {0.0f, 1.0f, 0.0f};
static const float fov_y = 45.0f;
static const int width = 1280;
static const int height = 720;

// Sphere definitions
static const sphere glass_sphere = {{-1.25f, 1.0f, 0.0f}, 1.0f};
static const sphere chrome_sphere = {{1.25f, 1.0f, 0.0f}, 1.0f};

// Light definitions
static const light lights[] = {
    {{-3.5f, 6.0f, 4.5f}, {1.0f, 1.0f, 1.0f}},   // Key light
    {{ 4.0f, 3.0f, 3.0f}, {0.35f, 0.35f, 0.35f}}, // Fill light
    {{ 0.5f, 5.0f, -4.0f}, {0.7f, 0.7f, 0.7f}}    // Back light
};
static const int num_lights = 3;

// Sky colors
static const vec3 sky_dark_red = {0.18f, 0.01f, 0.015f};
static const vec3 sky_orange = {1.0f, 0.25f, 0.03f};

// Fog parameters
static const float fog_density = 0.035f;

// Maximum recursion depth
static const int max_depth = 8;

// Ray-sphere intersection
int intersect_sphere(const ray* r, const sphere* s, float* t) {
    vec3 oc = vec3_sub(r->origin, s->center);
    float a = vec3_dot(r->direction, r->direction);
    float b = 2.0f * vec3_dot(oc, r->direction);
    float c = vec3_dot(oc, oc) - s->radius * s->radius;
    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f) {
        return 0; // No intersection
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

    return 0; // Intersection behind ray origin
}

// Ray-plane intersection
int intersect_plane(const ray* r, const plane* p, float* t) {
    float denom = vec3_dot(p->normal, r->direction);
    if (fabsf(denom) < 1e-6f) {
        return 0; // Ray is parallel to plane
    }

    *t = (p->distance - vec3_dot(p->normal, r->origin)) / denom;

    if (*t > 1e-4f) {
        return 1; // Valid intersection
    }

    return 0; // Intersection behind ray origin
}

// Compute checkerboard pattern
vec3 checkerboard_pattern(vec3 point) {
    int x = (int)floorf(point.x);
    int z = (int)floorf(point.z);
    
    // Alternate black and white squares
    if ((x + z) % 2 == 0) {
        return (vec3){1.0f, 1.0f, 1.0f}; // White
    } else {
        return (vec3){0.0f, 0.0f, 0.0f}; // Black
    }
}

// Compute sky color based on ray direction
vec3 sky_color(vec3 ray_dir) {
    // Clamp y to [0, 1] for numerical robustness
    float y = fmaxf(0.0f, fminf(1.0f, fabsf(ray_dir.y)));
    
    // Apply power to create smoother transition
    float t = powf(y, 0.65f);
    
    // Linear interpolation between dark red and orange
    return vec3_add(
        vec3_scale(1.0f - t, sky_dark_red),
        vec3_scale(t, sky_orange)
    );
}

// Compute fog effect
vec3 apply_fog(vec3 color, vec3 ray_dir, float distance) {
    float transmittance = expf(-fog_density * distance);
    vec3 fog_color = sky_color(ray_dir);
    
    return vec3_add(
        vec3_scale(transmittance, color),
        vec3_scale(1.0f - transmittance, fog_color)
    );
}

// Fresnel reflectance approximation (Schlick's approximation)
float fresnel_reflectance(float cos_theta, float eta) {
    float r0 = (1.0f - eta) / (1.0f + eta);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * powf(1.0f - cos_theta, 5.0f);
}

// Ray tracing function
vec3 trace_ray(const ray* r, int depth) {
    // If we've exceeded our recursion depth, return black
    if (depth >= max_depth) {
        return (vec3){0.0f, 0.0f, 0.0f};
    }

    // Initialize ray color
    vec3 color = {0.0f, 0.0f, 0.0f};

    // Define materials
    material glass_material = {{1.0f, 1.0f, 1.0f}, 0.0f, 1.0f, 1.5f}; // Clear glass
    material chrome_material = {{0.8f, 0.8f, 0.8f}, 0.95f, 0.0f, 1.0f}; // Chrome

    // Define the ground plane (y = 0)
    plane ground = {{0.0f, 1.0f, 0.0f}, 0.0f};

    // Initialize closest hit distance and object
    float closest_t = 1e30f;
    int hit_ground = 0;
    int hit_glass = 0;
    int hit_chrome = 0;
    float ground_t, glass_t, chrome_t;

    // Check for intersections with all objects
    if (intersect_plane(r, &ground, &ground_t)) {
        if (ground_t < closest_t) {
            closest_t = ground_t;
            hit_ground = 1;
        }
    }

    if (intersect_sphere(r, &glass_sphere, &glass_t)) {
        if (glass_t < closest_t) {
            closest_t = glass_t;
            hit_glass = 1;
        }
    }

    if (intersect_sphere(r, &chrome_sphere, &chrome_t)) {
        if (chrome_t < closest_t) {
            closest_t = chrome_t;
            hit_chrome = 1;
        }
    }

    // If no intersection, return sky color
    if (closest_t > 1e20f) {
        return sky_color(r->direction);
    }

    // Calculate intersection point
    vec3 hit_point = vec3_add(r->origin, vec3_scale(closest_t, r->direction));

    // Determine which object was hit
    if (hit_ground) {
        // Ground intersection
        vec3 ground_normal = {0.0f, 1.0f, 0.0f};
        
        // Compute checkerboard pattern
        vec3 albedo = checkerboard_pattern(hit_point);
        
        // Simple diffuse lighting
        vec3 total_light = {0.0f, 0.0f, 0.0f};
        
        for (int i = 0; i < num_lights; i++) {
            light l = lights[i];
            
            // Shadow ray
            vec3 light_dir = vec3_normalize(vec3_sub(l.position, hit_point));
            
            // Offset ray origin to avoid self-intersection
            vec3 shadow_origin = vec3_add(hit_point, vec3_scale(1e-4f, ground_normal));
            
            // Check if point is in shadow
            ray shadow_ray = {shadow_origin, light_dir};
            float shadow_t;
            
            // Check if shadow ray hits any object before reaching the light
            int in_shadow = 0;
            if (intersect_sphere(&shadow_ray, &glass_sphere, &shadow_t) && shadow_t < vec3_length(vec3_sub(l.position, hit_point))) {
                in_shadow = 1;
            } else if (intersect_sphere(&shadow_ray, &chrome_sphere, &shadow_t) && shadow_t < vec3_length(vec3_sub(l.position, hit_point))) {
                in_shadow = 1;
            }
            
            if (!in_shadow) {
                // Diffuse lighting
                float NdotL = fmaxf(0.0f, vec3_dot(ground_normal, light_dir));
                vec3 light_contribution = vec3_scale(NdotL, l.intensity);
                total_light = vec3_add(total_light, light_contribution);
            }
        }
        
        // Apply lighting to albedo
        color = vec3_mul(albedo, total_light);
        
        // Apply fog
        color = apply_fog(color, r->direction, closest_t);
    } else if (hit_glass || hit_chrome) {
        // Sphere intersection
        vec3 sphere_center = hit_glass ? glass_sphere.center : chrome_sphere.center;
        material* mat = hit_glass ? &glass_material : &chrome_material;
        
        // Compute normal at intersection point
        vec3 normal = vec3_normalize(vec3_sub(hit_point, sphere_center));
        
        // Ensure normal points towards ray origin (for inside/outside detection)
        if (vec3_dot(normal, r->direction) > 0.0f) {
            normal = vec3_scale(-1.0f, normal);
        }
        
        // Offset ray origin to avoid self-intersection
        vec3 ray_origin = vec3_add(hit_point, vec3_scale(1e-4f, normal));
        
        // Handle reflection and refraction
        if (mat->reflectivity > 0.0f || mat->refractivity > 0.0f) {
            // Reflection
            vec3 reflect_dir = vec3_sub(r->direction, 
                vec3_scale(2.0f * vec3_dot(r->direction, normal), normal));
            ray reflect_ray = {ray_origin, reflect_dir};
            
            vec3 reflect_color = trace_ray(&reflect_ray, depth + 1);
            
            // For glass, also handle refraction
            vec3 refract_color = {0.0f, 0.0f, 0.0f};
            if (mat->refractivity > 0.0f) {
                // Snell's law
                float eta = mat->ior; // IOR of glass
                float cos_i = -vec3_dot(normal, r->direction);
                
                // Check for total internal reflection
                float sin_t2 = eta * eta * (1.0f - cos_i * cos_i);
                if (sin_t2 <= 1.0f) {
                    // Refraction occurs
                    float cos_t = sqrtf(1.0f - sin_t2);
                    vec3 refract_dir = vec3_add(
                        vec3_scale(eta, r->direction),
                        vec3_scale(eta * cos_i - cos_t, normal)
                    );
                    
                    // Adjust direction for correct sign
                    refract_dir = vec3_normalize(refract_dir);
                    
                    // Trace the refracted ray
                    ray refract_ray = {ray_origin, refract_dir};
                    refract_color = trace_ray(&refract_ray, depth + 1);
                } else {
                    // Total internal reflection - reflect instead
                    // Already computed above
                }
            }
            
            // Combine reflection and refraction using weights
            float reflect_weight = mat->reflectivity;
            float refract_weight = mat->refractivity;
            
            // For glass, we want to combine both reflection and refraction
            if (hit_glass) {
                // Use Fresnel to determine mix of reflection and refraction
                float cos_theta = -vec3_dot(normal, r->direction);
                float reflectance = fresnel_reflectance(cos_theta, mat->ior);
                reflect_weight = reflectance;
                refract_weight = 1.0f - reflectance;
            }
            
            color = vec3_add(
                vec3_scale(reflect_weight, reflect_color),
                vec3_scale(refract_weight, refract_color)
            );
        } else {
            // Simple material (should not happen for our materials)
            color = mat->albedo;
        }
        
        // Add lighting for spheres
        vec3 total_light = {0.0f, 0.0f, 0.0f};
        
        for (int i = 0; i < num_lights; i++) {
            light l = lights[i];
            
            // Shadow ray
            vec3 light_dir = vec3_normalize(vec3_sub(l.position, hit_point));
            
            // Offset ray origin to avoid self-intersection
            vec3 shadow_origin = vec3_add(hit_point, vec3_scale(1e-4f, normal));
            
            // Check if point is in shadow
            ray shadow_ray = {shadow_origin, light_dir};
            float shadow_t;
            
            // Check if shadow ray hits any object before reaching the light
            int in_shadow = 0;
            if (hit_glass && intersect_sphere(&shadow_ray, &glass_sphere, &shadow_t) && shadow_t < vec3_length(vec3_sub(l.position, hit_point))) {
                in_shadow = 1;
            } else if (hit_chrome && intersect_sphere(&shadow_ray, &chrome_sphere, &shadow_t) && shadow_t < vec3_length(vec3_sub(l.position, hit_point))) {
                in_shadow = 1;
            }
            
            if (!in_shadow) {
                // Diffuse lighting
                float NdotL = fmaxf(0.0f, vec3_dot(normal, light_dir));
                vec3 light_contribution = vec3_scale(NdotL, l.intensity);
                total_light = vec3_add(total_light, light_contribution);
            }
        }
        
        // Apply lighting to color
        color = vec3_mul(color, total_light);
        
        // Apply fog
        color = apply_fog(color, r->direction, closest_t);
    }

    return color;
}

// Convert linear RGB to sRGB with gamma correction
uint8_t linear_to_srgb(float c) {
    if (c <= 0.0031308f) {
        return (uint8_t)(c * 12.92f * 255.0f + 0.5f);
    } else {
        return (uint8_t)((1.055f * powf(c, 1.0f / 2.4f) - 0.055f) * 255.0f + 0.5f);
    }
}

// Save TGA image
int save_tga(const char* filename, uint8_t* pixels, int width, int height) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s for writing\n", filename);
        return 0;
    }

    // TGA Header (18 bytes)
    uint8_t header[18] = {0};
    header[2] = 2; // Image type: uncompressed truecolor
    header[12] = width & 0xFF;           // Width (low byte)
    header[13] = (width >> 8) & 0xFF;    // Width (high byte)
    header[14] = height & 0xFF;          // Height (low byte)
    header[15] = (height >> 8) & 0xFF;   // Height (high byte)
    header[16] = 24;                     // Pixel depth
    header[17] = 0x20;                   // Descriptor: top-left origin, left-to-right order

    // Write header
    if (fwrite(header, 1, 18, file) != 18) {
        fclose(file);
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
    // Allocate memory for image pixels
    uint8_t* pixels = (uint8_t*)malloc(width * height * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }

    // Set up camera
    vec3 camera_forward = vec3_normalize(vec3_sub(camera_target, camera_pos));
    vec3 camera_right = vec3_normalize(vec3_cross(camera_forward, camera_up));
    vec3 camera_up_vec = vec3_cross(camera_right, camera_forward);

    // Compute image plane dimensions
    float aspect_ratio = (float)width / (float)height;
    float fov_rad = fov_y * M_PI / 180.0f;
    float half_height = tanf(fov_rad / 2.0f);
    float half_width = half_height * aspect_ratio;

    // Render image
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            vec3 pixel_color = {0.0f, 0.0f, 0.0f};
            
            // Use 2x2 sub-pixel sampling for anti-aliasing
            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    // Compute sub-pixel position
                    float u = (x + (sx + 0.5f) / 2.0f) / width;
                    float v = (y + (sy + 0.5f) / 2.0f) / height;
                    
                    // Scale and offset to [-1, 1] range
                    float screen_x = (2.0f * u - 1.0f) * half_width;
                    float screen_y = (1.0f - 2.0f * v) * half_height;
                    
                    // Compute ray direction
                    vec3 ray_dir = vec3_normalize(vec3_add(
                        vec3_add(vec3_scale(screen_x, camera_right),
                                vec3_scale(screen_y, camera_up_vec)),
                        camera_forward
                    ));

                    ray r = {camera_pos, ray_dir};
                    
                    // Trace ray
                    vec3 color = trace_ray(&r, 0);
                    
                    // Accumulate color
                    pixel_color = vec3_add(pixel_color, color);
                }
            }
            
            // Average the samples
            pixel_color = vec3_scale(0.25f, pixel_color);
            
            // Convert to 8-bit sRGB and store
            int idx = (y * width + x) * 3;
            pixels[idx] = linear_to_srgb(pixel_color.z);     // Blue
            pixels[idx + 1] = linear_to_srgb(pixel_color.y); // Green  
            pixels[idx + 2] = linear_to_srgb(pixel_color.x); // Red
        }
    }

    // Save the image
    if (!save_tga("out.tga", pixels, width, height)) {
        free(pixels);
        return 1;
    }

    free(pixels);
    return 0;
}
