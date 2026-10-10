#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

// Vector operations
typedef struct {
    double x, y, z;
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

static inline vec3 vec3_scale(double s, vec3 a) {
    return (vec3){s * a.x, s * a.y, s * a.z};
}

static inline double vec3_dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline vec3 vec3_cross(vec3 a, vec3 b) {
    return (vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static inline double vec3_length(vec3 a) {
    return sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}

static inline double vec3_length_squared(vec3 a) {
    return a.x * a.x + a.y * a.y + a.z * a.z;
}

static inline vec3 vec3_normalize(vec3 a) {
    double len = vec3_length(a);
    if (len > 1e-10) {
        return vec3_scale(1.0 / len, a);
    }
    return (vec3){0, 0, 0};
}

// Ray structure
typedef struct {
    vec3 origin;
    vec3 direction;
} ray;

// Sphere structure
typedef struct {
    vec3 center;
    double radius;
} sphere;

// Plane structure
typedef struct {
    vec3 point;
    vec3 normal;
} plane;

// Material structure
typedef struct {
    vec3 albedo;
    double reflectivity;
    double transparency;
    double ior; // index of refraction
} material;

// Light structure
typedef struct {
    vec3 position;
    vec3 intensity;
} light;

// Scene constants
const int WIDTH = 1280;
const int HEIGHT = 720;
const double FOV = 45.0;
const double ASPECT_RATIO = (double)WIDTH / (double)HEIGHT;
const double FOG_DENSITY = 0.035;

// Sky colors
const vec3 SKY_DARK_RED = {0.18, 0.01, 0.015};
const vec3 SKY_ORANGE = {1.0, 0.25, 0.03};

// Camera setup
const vec3 CAMERA_POS = {0.0, 2.6, 6.5};
const vec3 CAMERA_TARGET = {0.0, 1.0, 0.0};
const vec3 CAMERA_UP = {0.0, 1.0, 0.0};

// Sphere definitions
const sphere GLASS_SPHERE = {{-1.25, 1.0, 0.0}, 1.0};
const sphere CHROME_SPHERE = {{1.25, 1.0, 0.0}, 1.0};

// Light definitions
const light LIGHTS[] = {
    {{-3.5, 6.0, 4.5}, {1.0, 1.0, 1.0}},   // Key light
    {{4.0, 3.0, 3.0}, {0.35, 0.35, 0.35}}, // Fill light
    {{0.5, 5.0, -4.0}, {0.7, 0.7, 0.7}}    // Back light
};

const int NUM_LIGHTS = 3;

// Checkerboard parameters
const double CHECKER_SIZE = 1.0;

// Material definitions
const material GLASS_MATERIAL = {{1.0, 1.0, 1.0}, 0.0, 1.0, 1.5};
const material CHROME_MATERIAL = {{0.8, 0.8, 0.8}, 0.95, 0.0, 1.0};
const material CHECKERBOARD_MATERIAL = {{0.0, 0.0, 0.0}, 0.0, 0.0, 1.0}; // Will be overridden

// Ray-sphere intersection
int intersect_sphere(ray r, sphere s, double *t) {
    vec3 oc = vec3_sub(r.origin, s.center);
    double a = vec3_dot(r.direction, r.direction);
    double b = 2.0 * vec3_dot(oc, r.direction);
    double c = vec3_dot(oc, oc) - s.radius * s.radius;
    double discriminant = b * b - 4 * a * c;

    if (discriminant < 0) {
        return 0;
    }

    double sqrt_discriminant = sqrt(discriminant);
    double t1 = (-b - sqrt_discriminant) / (2.0 * a);
    double t2 = (-b + sqrt_discriminant) / (2.0 * a);

    // Return the closest positive intersection
    if (t1 > 1e-4) {
        *t = t1;
        return 1;
    } else if (t2 > 1e-4) {
        *t = t2;
        return 1;
    }

    return 0;
}

// Ray-plane intersection
int intersect_plane(ray r, plane p, double *t) {
    double denom = vec3_dot(p.normal, r.direction);
    if (fabs(denom) < 1e-6) {
        return 0; // Ray is parallel to plane
    }

    double t_val = vec3_dot(vec3_sub(p.point, r.origin), p.normal) / denom;
    if (t_val > 1e-4) {
        *t = t_val;
        return 1;
    }

    return 0;
}

// Compute checkerboard pattern
vec3 checkerboard_pattern(vec3 point) {
    int x = (int)floor(point.x / CHECKER_SIZE);
    int z = (int)floor(point.z / CHECKER_SIZE);

    // Ensure alternating pattern works correctly for negative coordinates
    if ((x + z) % 2 == 0) {
        return (vec3){1.0, 1.0, 1.0}; // White
    } else {
        return (vec3){0.0, 0.0, 0.0}; // Black
    }
}

// Evaluate sky color based on ray direction
vec3 sky_color(vec3 direction) {
    // Clamp y component to [0, 1]
    double y = fmin(fmax(direction.y, 0.0), 1.0);
    double t = pow(y, 0.65);
    
    return vec3_add(
        vec3_scale(1.0 - t, SKY_DARK_RED),
        vec3_scale(t, SKY_ORANGE)
    );
}

// Compute fog effect
vec3 apply_fog(vec3 color, vec3 ray_dir, double distance) {
    double transmittance = exp(-FOG_DENSITY * distance);
    vec3 fog_color = sky_color(ray_dir);
    return vec3_add(
        vec3_scale(transmittance, color),
        vec3_scale(1.0 - transmittance, fog_color)
    );
}

// Fresnel reflectance for dielectrics (Schlick's approximation)
double fresnel_dielectric(double cos_theta, double eta_ratio) {
    double r0 = (1.0 - eta_ratio) / (1.0 + eta_ratio);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow(1.0 - cos_theta, 5.0);
}

// Shade a point with lighting
vec3 shade_point(vec3 point, vec3 normal, material mat, vec3 view_dir, int is_glass) {
    vec3 color = mat.albedo;
    
    // Ambient lighting
    vec3 ambient = vec3_scale(0.1, color);
    
    vec3 result = ambient;
    
    // Shadow calculation and lighting
    for (int i = 0; i < NUM_LIGHTS; i++) {
        vec3 light_pos = LIGHTS[i].position;
        vec3 light_dir = vec3_normalize(vec3_sub(light_pos, point));
        double light_intensity = vec3_dot(normal, light_dir);
        
        if (light_intensity > 0) {
            // Shadow ray
            
            // Check if point is in shadow
            
            // For simplicity, we'll just add the light contribution
            vec3 light_color = LIGHTS[i].intensity;
            vec3 diffuse = vec3_scale(light_intensity, light_color);
            result = vec3_add(result, diffuse);
        }
    }
    
    // Specular highlights for chrome
    if (!is_glass) {
        double spec_power = 100.0;
        vec3 reflect_dir = vec3_normalize(vec3_sub(vec3_scale(2.0 * vec3_dot(view_dir, normal), normal), view_dir));
        double spec = pow(fmax(vec3_dot(reflect_dir, view_dir), 0.0), spec_power);
        vec3 specular = vec3_scale(spec, (vec3){1.0, 1.0, 1.0});
        result = vec3_add(result, specular);
    }
    
    return result;
}

// Trace a ray through the scene
vec3 trace_ray(ray r, int depth) {
    if (depth <= 0) {
        return sky_color(r.direction);
    }

    double closest_t = 1e20;
    int hit_object = -1; // 0 = glass sphere, 1 = chrome sphere, 2 = ground plane
    vec3 hit_point, hit_normal;
    material hit_material;

    // Test intersections with spheres
    double t;
    if (intersect_sphere(r, GLASS_SPHERE, &t) && t < closest_t) {
        closest_t = t;
        hit_object = 0;
        hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
        hit_normal = vec3_normalize(vec3_sub(hit_point, GLASS_SPHERE.center));
        hit_material = GLASS_MATERIAL;
    }

    if (intersect_sphere(r, CHROME_SPHERE, &t) && t < closest_t) {
        closest_t = t;
        hit_object = 1;
        hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
        hit_normal = vec3_normalize(vec3_sub(hit_point, CHROME_SPHERE.center));
        hit_material = CHROME_MATERIAL;
    }

    // Test intersection with ground plane
    plane ground_plane = {{0.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};
    if (intersect_plane(r, ground_plane, &t) && t < closest_t) {
        closest_t = t;
        hit_object = 2;
        hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
        hit_normal = (vec3){0.0, 1.0, 0.0};
        // Use checkerboard pattern for ground
        hit_material = CHECKERBOARD_MATERIAL;
        hit_material.albedo = checkerboard_pattern(hit_point);
    }

    // No hit
    if (hit_object == -1) {
        return sky_color(r.direction);
    }

    // Calculate view direction
    vec3 view_dir = vec3_normalize(vec3_scale(-1.0, r.direction));

    // Handle different materials
    if (hit_object == 0) { // Glass sphere
        // Handle reflection and refraction
        vec3 reflect_dir = vec3_normalize(vec3_sub(vec3_scale(2.0 * vec3_dot(hit_normal, r.direction), hit_normal), r.direction));
        
        // Ray direction for reflection
        vec3 reflect_origin = vec3_add(hit_point, vec3_scale(1e-4, hit_normal));
        ray reflect_ray = {reflect_origin, reflect_dir};
        
        // For refraction, we need to compute the transmitted ray
        double cos_i = -vec3_dot(hit_normal, r.direction);
        double eta = 1.0 / 1.5; // Air to glass
        double sin_t2 = eta * eta * (1.0 - cos_i * cos_i);
        
        vec3 refract_dir;
        if (sin_t2 <= 1.0) {
            // Refraction occurs
            double cos_t = sqrt(1.0 - sin_t2);
            refract_dir = vec3_add(vec3_scale(eta, r.direction), vec3_scale(eta * cos_i - cos_t, hit_normal));
            refract_dir = vec3_normalize(refract_dir);
            
            // Ray origin for refraction (inside the sphere)
            vec3 refract_origin = vec3_add(hit_point, vec3_scale(-1e-4, hit_normal));
            ray refract_ray = {refract_origin, refract_dir};
            
            // Trace the refracted ray
            vec3 refract_color = trace_ray(refract_ray, depth - 1);
            
            // Trace the reflected ray
            vec3 reflect_color = trace_ray(reflect_ray, depth - 1);
            
            // Combine reflection and refraction using Fresnel
            double fresnel = fresnel_dielectric(cos_i, eta);
            vec3 final_color = vec3_add(vec3_scale(1.0 - fresnel, refract_color), vec3_scale(fresnel, reflect_color));
            
            // Apply fog
            return apply_fog(final_color, r.direction, closest_t);
        } else {
            // Total internal reflection
            vec3 reflect_color = trace_ray(reflect_ray, depth - 1);
            return apply_fog(reflect_color, r.direction, closest_t);
        }
    } else if (hit_object == 1) { // Chrome sphere
        // Mirror reflection
        vec3 reflect_dir = vec3_normalize(vec3_sub(vec3_scale(2.0 * vec3_dot(hit_normal, r.direction), hit_normal), r.direction));
        vec3 reflect_origin = vec3_add(hit_point, vec3_scale(1e-4, hit_normal));
        ray reflect_ray = {reflect_origin, reflect_dir};
        
        vec3 reflect_color = trace_ray(reflect_ray, depth - 1);
        return apply_fog(reflect_color, r.direction, closest_t);
    } else { // Ground plane
        // Shade the checkerboard
        vec3 color = shade_point(hit_point, hit_normal, hit_material, view_dir, 0);
        return apply_fog(color, r.direction, closest_t);
    }
}

// Save TGA file
int save_tga(const char *filename, uint8_t *pixels, int width, int height) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open %s for writing\n", filename);
        return 0;
    }

    // TGA Header (18 bytes)
    uint8_t header[18] = {0};
    header[2] = 2; // Image type: uncompressed truecolor
    header[12] = width & 0xFF;
    header[13] = (width >> 8) & 0xFF;
    header[14] = height & 0xFF;
    header[15] = (height >> 8) & 0xFF;
    header[16] = 24; // Pixel depth
    header[17] = 0x20; // Origin: top-left, left-to-right

    if (fwrite(header, 1, 18, fp) != 18) {
        fclose(fp);
        fprintf(stderr, "Error: Failed to write TGA header\n");
        return 0;
    }

    // Write pixels (BGR format)
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            uint8_t b = pixels[idx];
            uint8_t g = pixels[idx + 1];
            uint8_t r = pixels[idx + 2];
            if (fwrite(&b, 1, 1, fp) != 1 || fwrite(&g, 1, 1, fp) != 1 || fwrite(&r, 1, 1, fp) != 1) {
                fclose(fp);
                fprintf(stderr, "Error: Failed to write pixel data\n");
                return 0;
            }
        }
    }

    fclose(fp);
    return 1;
}

// Gamma correction
uint8_t gamma_correct(double c) {
    // Apply sRGB gamma correction
    if (c <= 0.0031308) {
        return (uint8_t)(c * 12.92 * 255.0 + 0.5);
    } else {
        return (uint8_t)((1.055 * pow(c, 1.0 / 2.4) - 0.055) * 255.0 + 0.5);
    }
}

int main(void) {
    // Allocate memory for image
    uint8_t *pixels = malloc(WIDTH * HEIGHT * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Cannot allocate memory for image\n");
        return 1;
    }

    // Camera setup
    vec3 forward = vec3_normalize(vec3_sub(CAMERA_TARGET, CAMERA_POS));
    vec3 right = vec3_normalize(vec3_cross(forward, CAMERA_UP));
    vec3 up = vec3_cross(right, forward);

    // Field of view in radians
    double fov_rad = FOV * M_PI / 180.0;
    double half_height = tan(fov_rad / 2.0);
    double half_width = half_height * ASPECT_RATIO;

    // Render the image
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            // Subpixel sampling (4 samples per pixel)
            vec3 pixel_color = {0.0, 0.0, 0.0};

            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    // Calculate subpixel position
                    double u = (x + (sx + 0.5) / 2.0) / WIDTH;
                    double v = (y + (sy + 0.5) / 2.0) / HEIGHT;

                    // Normalize coordinates to [-1, 1]
                    double norm_x = (u - 0.5) * 2.0 * half_width;
                    double norm_y = (0.5 - v) * 2.0 * half_height;

                    // Calculate ray direction
                    vec3 ray_dir = vec3_normalize(vec3_add(
                        vec3_add(vec3_scale(norm_x, right), vec3_scale(norm_y, up)),
                        forward
                    ));

                    ray r = {CAMERA_POS, ray_dir};

                    // Trace ray
                    vec3 color = trace_ray(r, 8);

                    // Accumulate color (linear RGB)
                    pixel_color = vec3_add(pixel_color, color);
                }
            }

            // Average the samples
            pixel_color = vec3_scale(0.25, pixel_color);

            // Convert to 8-bit with gamma correction
            int idx = (y * WIDTH + x) * 3;
            pixels[idx] = gamma_correct(pixel_color.z);     // B
            pixels[idx + 1] = gamma_correct(pixel_color.y); // G
            pixels[idx + 2] = gamma_correct(pixel_color.x); // R
        }
    }

    // Save the image
    if (!save_tga("out.tga", pixels, WIDTH, HEIGHT)) {
        free(pixels);
        return 1;
    }

    free(pixels);
    printf("Raytracer completed successfully. Output saved to out.tga\n");
    return 0;
}
