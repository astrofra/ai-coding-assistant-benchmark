#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// Vector operations
typedef struct {
    float x, y, z;
} vec3;

vec3 vec3_add(vec3 a, vec3 b) {
    return (vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

vec3 vec3_sub(vec3 a, vec3 b) {
    return (vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

vec3 vec3_mul(float s, vec3 v) {
    return (vec3){s * v.x, s * v.y, s * v.z};
}

vec3 vec3_div(float s, vec3 v) {
    return (vec3){v.x / s, v.y / s, v.z / s};
}

float vec3_dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

vec3 vec3_cross(vec3 a, vec3 b) {
    return (vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float vec3_length(vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

float vec3_length_squared(vec3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

vec3 vec3_normalize(vec3 v) {
    float len = vec3_length(v);
    if (len > 1e-8f) {
        return vec3_div(len, v);
    }
    return (vec3){0.0f, 0.0f, 0.0f};
}

vec3 vec3_negate(vec3 v) {
    return (vec3){-v.x, -v.y, -v.z};
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
    float transparency;
    float ior; // Index of refraction
} material;

// Light structure
typedef struct {
    vec3 position;
    vec3 intensity;
} light;

// Scene structure
typedef struct {
    sphere glass_sphere;
    sphere chrome_sphere;
    plane ground_plane;
    light lights[3];
    vec3 sky_horizon;
    vec3 sky_dark_red;
} scene;

// Global scene
scene g_scene;

// Sky color computation
vec3 sky_color(vec3 ray_dir) {
    // Clamp ray_dir.y to [0, 1] for numerical robustness
    float y = fmaxf(0.0f, fminf(1.0f, fabsf(ray_dir.y)));
    float t = powf(y, 0.65f);
    return vec3_add(
        vec3_mul(1.0f - t, g_scene.sky_horizon),
        vec3_mul(t, g_scene.sky_dark_red)
    );
}

// Checkerboard pattern
vec3 checkerboard_color(vec3 point) {
    int x = (int)floorf(point.x);
    int z = (int)floorf(point.z);
    // Alternate colors based on sum of coordinates
    if ((x + z) % 2 == 0) {
        return (vec3){1.0f, 1.0f, 1.0f}; // White
    } else {
        return (vec3){0.0f, 0.0f, 0.0f}; // Black
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
int intersect_plane(ray r, plane p, float *t) {
    float denom = vec3_dot(p.normal, r.direction);
    if (fabsf(denom) < 1e-6f) {
        return 0; // Ray parallel to plane
    }

    float t_val = vec3_dot(vec3_sub(p.point, r.origin), p.normal) / denom;
    if (t_val > 1e-4f) {
        *t = t_val;
        return 1;
    }

    return 0; // Intersection behind ray origin
}

// Fresnel reflection coefficient using Schlick's approximation
float fresnel(float cos_theta, float eta_ratio) {
    float r0 = (1.0f - eta_ratio) / (1.0f + eta_ratio);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * powf(1.0f - cos_theta, 5.0f);
}

// Reflect vector
vec3 reflect(vec3 incident, vec3 normal) {
    return vec3_sub(incident, vec3_mul(2.0f * vec3_dot(incident, normal), normal));
}

// Refract vector
vec3 refract(vec3 incident, vec3 normal, float eta_ratio) {
    float cos_i = vec3_dot(normal, incident);
    if (cos_i > 0.0f) {
        // Ray entering the material
        normal = vec3_negate(normal);
        cos_i = -cos_i;
    }

    float k = 1.0f - eta_ratio * eta_ratio * (1.0f - cos_i * cos_i);
    if (k < 0.0f) {
        // Total internal reflection
        return reflect(incident, normal);
    }

    return vec3_sub(
        vec3_mul(eta_ratio, incident),
        vec3_mul(eta_ratio * cos_i + sqrtf(k), normal)
    );
}

// Trace a ray through the scene
vec3 trace_ray(ray r, int depth) {
    if (depth > 8) {
        return sky_color(r.direction);
    }

    float closest_t = 1e30f;
    int hit_sphere = 0;
    int hit_ground = 0;
    float sphere_t = 0.0f;
    float ground_t = 0.0f;
    vec3 hit_point, hit_normal;
    material hit_material;

    // Check sphere intersections
    if (intersect_sphere(r, g_scene.glass_sphere, &sphere_t)) {
        if (sphere_t < closest_t) {
            closest_t = sphere_t;
            hit_sphere = 1;
            hit_point = vec3_add(r.origin, vec3_mul(closest_t, r.direction));
            hit_normal = vec3_normalize(vec3_sub(hit_point, g_scene.glass_sphere.center));
            hit_material = (material){.albedo = (vec3){1.0f, 1.0f, 1.0f}, 
                                     .reflectivity = 0.0f, 
                                     .transparency = 1.0f, 
                                     .ior = 1.5f};
        }
    }

    if (intersect_sphere(r, g_scene.chrome_sphere, &sphere_t)) {
        if (sphere_t < closest_t) {
            closest_t = sphere_t;
            hit_sphere = 1;
            hit_point = vec3_add(r.origin, vec3_mul(closest_t, r.direction));
            hit_normal = vec3_normalize(vec3_sub(hit_point, g_scene.chrome_sphere.center));
            hit_material = (material){.albedo = (vec3){0.8f, 0.8f, 0.8f}, 
                                     .reflectivity = 0.95f, 
                                     .transparency = 0.0f, 
                                     .ior = 1.0f};
        }
    }

    // Check ground plane intersection
    if (intersect_plane(r, g_scene.ground_plane, &ground_t)) {
        if (ground_t < closest_t) {
            closest_t = ground_t;
            hit_ground = 1;
            hit_point = vec3_add(r.origin, vec3_mul(closest_t, r.direction));
            hit_normal = (vec3){0.0f, 1.0f, 0.0f};
            hit_material.albedo = checkerboard_color(hit_point);
            hit_material.reflectivity = 0.0f;
            hit_material.transparency = 0.0f;
            hit_material.ior = 1.0f;
        }
    }

    // If we hit nothing, return sky color
    if (!hit_sphere && !hit_ground) {
        return sky_color(r.direction);
    }

    // Calculate lighting for the hit point
    vec3 color = (vec3){0.0f, 0.0f, 0.0f};
    vec3 view_dir = vec3_negate(r.direction);

    // Ambient lighting
    color = vec3_add(color, vec3_mul(0.1f, hit_material.albedo));

    // Shadow rays and lighting calculations
    for (int i = 0; i < 3; i++) {
        light l = g_scene.lights[i];
        vec3 light_dir = vec3_normalize(vec3_sub(l.position, hit_point));
        float light_distance = vec3_length(vec3_sub(l.position, hit_point));

        // Shadow ray
        ray shadow_ray = {hit_point, light_dir};
        float shadow_t = 0.0f;
        int in_shadow = 0;

        // Check if the point is in shadow
        if (intersect_sphere(shadow_ray, g_scene.glass_sphere, &shadow_t) && shadow_t < light_distance) {
            in_shadow = 1;
        }
        if (!in_shadow && intersect_sphere(shadow_ray, g_scene.chrome_sphere, &shadow_t) && shadow_t < light_distance) {
            in_shadow = 1;
        }
        if (!in_shadow && intersect_plane(shadow_ray, g_scene.ground_plane, &shadow_t) && shadow_t < light_distance) {
            in_shadow = 1;
        }

        if (!in_shadow) {
            // Diffuse lighting
            float diffuse = fmaxf(0.0f, vec3_dot(hit_normal, light_dir));
            
            // Specular lighting (Phong)
            vec3 reflect_dir = reflect(vec3_negate(light_dir), hit_normal);
            float spec = powf(fmaxf(0.0f, vec3_dot(view_dir, reflect_dir)), 32.0f);

            // Combine lighting
            vec3 light_contribution = vec3_mul(diffuse, hit_material.albedo);
            light_contribution = vec3_add(light_contribution, vec3_mul(spec, (vec3){1.0f, 1.0f, 1.0f}));
            color = vec3_add(color, vec3_mul(l.intensity.x, light_contribution));
        }
    }

    // Handle reflections and refractions
    if (hit_material.reflectivity > 0.0f) {
        vec3 reflect_dir = reflect(r.direction, hit_normal);
        ray reflect_ray = {vec3_add(hit_point, vec3_mul(1e-4f, hit_normal)), reflect_dir};
        vec3 reflect_color = trace_ray(reflect_ray, depth + 1);
        color = vec3_add(color, vec3_mul(hit_material.reflectivity, reflect_color));
    }

    if (hit_material.transparency > 0.0f && hit_material.ior != 1.0f) {
        // Handle refraction for glass sphere
        vec3 refract_dir = refract(r.direction, hit_normal, 1.0f / hit_material.ior);
        ray refract_ray = {vec3_add(hit_point, vec3_mul(-1e-4f, hit_normal)), refract_dir};
        vec3 refract_color = trace_ray(refract_ray, depth + 1);
        color = vec3_add(color, vec3_mul(hit_material.transparency, refract_color));
    }

    // Apply fog effect
    float transmittance = expf(-0.035f * closest_t);
    vec3 fog_color = sky_color(r.direction);
    color = vec3_add(vec3_mul(transmittance, color), vec3_mul(1.0f - transmittance, fog_color));

    return color;
}

// Convert linear RGB to sRGB
vec3 linear_to_srgb(vec3 linear) {
    vec3 srgb;
    srgb.x = (linear.x <= 0.0031308f) ? 12.92f * linear.x : 1.055f * powf(linear.x, 1.0f / 2.4f) - 0.055f;
    srgb.y = (linear.y <= 0.0031308f) ? 12.92f * linear.y : 1.055f * powf(linear.y, 1.0f / 2.4f) - 0.055f;
    srgb.z = (linear.z <= 0.0031308f) ? 12.92f * linear.z : 1.055f * powf(linear.z, 1.0f / 2.4f) - 0.055f;
    return srgb;
}

// Write TGA file
int write_tga(const char* filename, unsigned char* pixels, int width, int height) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open %s for writing\n", filename);
        return 0;
    }

    // TGA Header (18 bytes)
    unsigned char header[18] = {0};
    header[2] = 2; // Image type: uncompressed truecolor
    header[12] = width & 0xFF;           // Width (little-endian)
    header[13] = (width >> 8) & 0xFF;
    header[14] = height & 0xFF;          // Height (little-endian)
    header[15] = (height >> 8) & 0xFF;
    header[16] = 24;                     // Pixel depth
    header[17] = 0x20;                   // Descriptor: top-left origin

    if (fwrite(header, 1, 18, file) != 18) {
        fclose(file);
        fprintf(stderr, "Error: Failed to write TGA header\n");
        return 0;
    }

    // Write pixel data (BGR order, bottom to top)
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int pixel_idx = (y * width + x) * 3;
            unsigned char b = pixels[pixel_idx];
            unsigned char g = pixels[pixel_idx + 1];
            unsigned char r = pixels[pixel_idx + 2];
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
    // Initialize scene
    g_scene.glass_sphere.center = (vec3){-1.25f, 1.0f, 0.0f};
    g_scene.glass_sphere.radius = 1.0f;
    
    g_scene.chrome_sphere.center = (vec3){1.25f, 1.0f, 0.0f};
    g_scene.chrome_sphere.radius = 1.0f;
    
    g_scene.ground_plane.point = (vec3){0.0f, 0.0f, 0.0f};
    g_scene.ground_plane.normal = (vec3){0.0f, 1.0f, 0.0f};

    g_scene.lights[0].position = (vec3){-3.5f, 6.0f, 4.5f};
    g_scene.lights[0].intensity = (vec3){1.0f, 1.0f, 1.0f};

    g_scene.lights[1].position = (vec3){4.0f, 3.0f, 3.0f};
    g_scene.lights[1].intensity = (vec3){0.35f, 0.35f, 0.35f};

    g_scene.lights[2].position = (vec3){0.5f, 5.0f, -4.0f};
    g_scene.lights[2].intensity = (vec3){0.7f, 0.7f, 0.7f};

    g_scene.sky_horizon = (vec3){1.0f, 0.25f, 0.03f};
    g_scene.sky_dark_red = (vec3){0.18f, 0.01f, 0.015f};

    // Camera setup
    vec3 camera_pos = (vec3){0.0f, 2.6f, 6.5f};
    vec3 camera_target = (vec3){0.0f, 1.0f, 0.0f};
    vec3 camera_up = (vec3){0.0f, 1.0f, 0.0f};

    // Compute camera coordinate system
    vec3 camera_forward = vec3_normalize(vec3_sub(camera_target, camera_pos));
    vec3 camera_right = vec3_normalize(vec3_cross(camera_forward, camera_up));
    camera_up = vec3_cross(camera_right, camera_forward);

    // Image parameters
    const int width = 1280;
    const int height = 720;
    const int samples_per_pixel = 4; // 2x2 subpixel sampling

    // Allocate pixel buffer
    unsigned char* pixels = (unsigned char*)malloc(width * height * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Failed to allocate pixel buffer\n");
        return 1;
    }

    // Render image
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            vec3 pixel_color = (vec3){0.0f, 0.0f, 0.0f};

            // Subpixel sampling
            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    // Compute subpixel position with jitter
                    float u = (x + (sx + 0.5f) / 2.0f) / width;
                    float v = (y + (sy + 0.5f) / 2.0f) / height;

                    // Convert to normalized device coordinates
                    float aspect_ratio = (float)width / (float)height;
                    float fov_rad = 45.0f * M_PI / 180.0f;
                    float half_height = tanf(fov_rad / 2.0f);
                    float half_width = aspect_ratio * half_height;

                    vec3 ray_dir = vec3_add(
                        vec3_mul(half_width * (2.0f * u - 1.0f), camera_right),
                        vec3_add(
                            vec3_mul(half_height * (1.0f - 2.0f * v), camera_up),
                            camera_forward
                        )
                    );
                    ray_dir = vec3_normalize(ray_dir);

                    ray r = {camera_pos, ray_dir};
                    vec3 color = trace_ray(r, 0);

                    // Accumulate in linear RGB space
                    pixel_color = vec3_add(pixel_color, color);
                }
            }

            // Average samples
            pixel_color = vec3_div(samples_per_pixel, pixel_color);

            // Convert to sRGB and clamp to [0, 1]
            vec3 srgb_color = linear_to_srgb(pixel_color);
            srgb_color.x = fmaxf(0.0f, fminf(1.0f, srgb_color.x));
            srgb_color.y = fmaxf(0.0f, fminf(1.0f, srgb_color.y));
            srgb_color.z = fmaxf(0.0f, fminf(1.0f, srgb_color.z));

            // Convert to 8-bit values
            int idx = (y * width + x) * 3;
            pixels[idx] = (unsigned char)(srgb_color.z * 255.0f); // B
            pixels[idx + 1] = (unsigned char)(srgb_color.y * 255.0f); // G
            pixels[idx + 2] = (unsigned char)(srgb_color.x * 255.0f); // R
        }
    }

    // Write output image
    if (!write_tga("out.tga", pixels, width, height)) {
        free(pixels);
        return 1;
    }

    free(pixels);
    printf("Raytracer completed successfully. Output saved to out.tga\n");
    return 0;
}
