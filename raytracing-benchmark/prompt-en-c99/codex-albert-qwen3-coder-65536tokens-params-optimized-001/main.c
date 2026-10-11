#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// Vector operations
typedef struct {
    double x, y, z;
} vec3;

vec3 vec3_add(vec3 a, vec3 b) {
    return (vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

vec3 vec3_sub(vec3 a, vec3 b) {
    return (vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

vec3 vec3_mul(vec3 a, vec3 b) {
    return (vec3){a.x * b.x, a.y * b.y, a.z * b.z};
}

vec3 vec3_scale(double s, vec3 v) {
    return (vec3){s * v.x, s * v.y, s * v.z};
}

vec3 vec3_negate(vec3 v) {
    return (vec3){-v.x, -v.y, -v.z};
}

double vec3_dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

vec3 vec3_cross(vec3 a, vec3 b) {
    return (vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

double vec3_length(vec3 v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

double vec3_length_squared(vec3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

vec3 vec3_normalize(vec3 v) {
    double len = vec3_length(v);
    if (len > 1e-10) {
        return vec3_scale(1.0 / len, v);
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
    int material; // 0 = glass, 1 = chrome
} sphere;

// Plane structure
typedef struct {
    double y;
} plane;

// Light structure
typedef struct {
    vec3 position;
    double intensity;
} light;

// Material properties
typedef struct {
    double eta; // Index of refraction
    double reflectivity;
    vec3 albedo;
} material;

// Scene parameters
const double PI = 3.14159265358979323846;
const double EPSILON = 1e-4;
const int MAX_BOUNCES = 8;
const int SAMPLES_PER_PIXEL = 4; // 2x2 subpixel sampling

// Predefined materials
material materials[] = {
    // Glass (eta = 1.5)
    {.eta = 1.5, .reflectivity = 0.0, .albedo = {1.0, 1.0, 1.0}},
    // Chrome (reflectivity ~0.95)
    {.eta = 1.0, .reflectivity = 0.95, .albedo = {1.0, 1.0, 1.0}}
};

// Lights
light lights[] = {
    {{-3.5, 6.0, 4.5}, 1.0},   // Key light
    {{4.0, 3.0, 3.0}, 0.35},   // Fill light
    {{0.5, 5.0, -4.0}, 0.7}   // Back light
};

const int NUM_LIGHTS = 3;

// Sky color computation
vec3 sky_color(vec3 direction) {
    // Clamp direction.y to [0, 1] for numerical robustness
    double y = fmin(fmax(direction.y, 0.0), 1.0);
    
    // Calculate the interpolation factor
    double t = pow(y, 0.65);
    
    // Define colors
    vec3 dark_red = {0.18, 0.01, 0.015};
    vec3 orange = {1.0, 0.25, 0.03};
    
    // Interpolate between dark red and orange
    return vec3_add(vec3_scale(1.0 - t, dark_red), vec3_scale(t, orange));
}

// Checkerboard pattern
int checker_pattern(double x, double z) {
    // Use floor to handle negative coordinates properly
    int ix = (int)floor(x);
    int iz = (int)floor(z);
    return (ix + iz) % 2;
}

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
int intersect_plane(ray r, plane p, double *t) {
    if (fabs(r.direction.y) < EPSILON) {
        return 0; // Ray is parallel to plane
    }
    
    double t_val = (p.y - r.origin.y) / r.direction.y;
    if (t_val > EPSILON) {
        *t = t_val;
        return 1;
    }
    
    return 0;
}

// Fresnel reflection coefficient using Schlick's approximation
double fresnel(double cos_theta, double eta_i, double eta_t) {
    double r0 = (eta_i - eta_t) / (eta_i + eta_t);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow(1.0 - cos_theta, 5.0);
}

// Reflect a ray direction around a normal
vec3 reflect(vec3 incident, vec3 normal) {
    return vec3_sub(incident, vec3_scale(2.0 * vec3_dot(incident, normal), normal));
}

// Refract a ray direction through a surface
vec3 refract(vec3 incident, vec3 normal, double eta_ratio) {
    double cos_i = vec3_dot(normal, incident);
    double k = 1.0 - eta_ratio * eta_ratio * (1.0 - cos_i * cos_i);
    
    if (k < 0.0) {
        // Total internal reflection
        return reflect(incident, normal);
    }
    
    return vec3_sub(vec3_scale(eta_ratio, incident),
                    vec3_scale(eta_ratio * cos_i + sqrt(k), normal));
}

// Sample a hemisphere for diffuse lighting
vec3 sample_hemisphere(vec3 normal) {
    // Generate uniform random numbers
    double r1 = 0.5; // Would be random in real implementation
    double r2 = 0.5;
    
    // Cosine-weighted hemisphere sampling
    double theta = acos(sqrt(r1));
    double phi = 2.0 * PI * r2;
    
    // Convert to Cartesian coordinates
    double sin_theta = sin(theta);
    double x = sin_theta * cos(phi);
    double y = cos(theta);
    double z = sin_theta * sin(phi);
    
    // Construct orthonormal basis
    vec3 w = normal;
    vec3 u = vec3_normalize(vec3_cross(w, (vec3){0.0, 1.0, 0.0}));
    if (vec3_length(u) < 1e-6) {
        u = (vec3){1.0, 0.0, 0.0};
    }
    vec3 v = vec3_cross(w, u);
    
    // Return vector in world space
    return vec3_add(vec3_scale(x, u), vec3_add(vec3_scale(y, w), vec3_scale(z, v)));
}

// Shade a point with lighting
vec3 shade_point(vec3 point, vec3 normal, material mat, int is_glass) {
    vec3 color = {0.0, 0.0, 0.0};
    
    // Ambient component
    color = vec3_add(color, vec3_scale(0.1, mat.albedo));
    
    // Lighting from each light source
    for (int i = 0; i < NUM_LIGHTS; i++) {
        light l = lights[i];
        
        // Shadow ray direction
        vec3 light_dir = vec3_normalize(vec3_sub(l.position, point));
        
        // Check if point is in shadow
        // For this simplified implementation, we won't compute proper shadows
        // We'll assume all points are lit except when they're on the back side
        
        // Diffuse lighting
        double diffuse = fmax(0.0, vec3_dot(normal, light_dir));
        
        // Specular lighting (simplified)
        vec3 view_dir = vec3_normalize(vec3_negate((vec3){0.0, 2.6, 6.5}));
        vec3 reflect_dir = reflect(vec3_negate(light_dir), normal);
        double specular = 0.0;
        
        if (!is_glass) {
            // Chrome has specular highlights
            specular = pow(fmax(0.0, vec3_dot(view_dir, reflect_dir)), 64.0);
        }
        
        // Combine lighting components
        vec3 light_contribution = vec3_scale(l.intensity * diffuse, mat.albedo);
        if (!is_glass) {
            light_contribution = vec3_add(light_contribution, vec3_scale(l.intensity * specular, (vec3){1.0, 1.0, 1.0}));
        }
        
        color = vec3_add(color, light_contribution);
    }
    
    return color;
}

// Trace a ray through the scene
vec3 trace_ray(ray r, int depth) {
    if (depth >= MAX_BOUNCES) {
        return sky_color(r.direction);
    }
    
    // Scene objects
    sphere spheres[] = {
        {{-1.25, 1.0, 0.0}, 1.0, 0}, // Glass sphere
        {{1.25, 1.0, 0.0}, 1.0, 1}   // Chrome sphere
    };
    
    plane ground = {0.0};
    
    double closest_t = 1e20;
    int hit_sphere = -1;
    int hit_glass = 0;
    vec3 hit_point, hit_normal;
    material hit_material;
    
    // Test intersection with spheres
    for (int i = 0; i < 2; i++) {
        double t;
        if (intersect_sphere(r, spheres[i], &t) && t < closest_t) {
            closest_t = t;
            hit_sphere = i;
            hit_glass = (spheres[i].material == 0);
            
            // Calculate hit point and normal
            hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
            hit_normal = vec3_normalize(vec3_sub(hit_point, spheres[i].center));
            hit_material = materials[spheres[i].material];
        }
    }
    
    // Test intersection with ground plane
    double t;
    if (intersect_plane(r, ground, &t) && t < closest_t) {
        closest_t = t;
        hit_sphere = -1;
        
        // Calculate hit point and normal
        hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
        hit_normal = (vec3){0.0, 1.0, 0.0};
        hit_material = (material){1.0, 0.0, {0.0, 0.0, 0.0}}; // Will be replaced
        
        // Get checkerboard color
        int pattern = checker_pattern(hit_point.x, hit_point.z);
        vec3 checker_color = pattern ? (vec3){1.0, 1.0, 1.0} : (vec3){0.0, 0.0, 0.0};
        hit_material.albedo = checker_color;
    }
    
    // No intersection
    if (closest_t == 1e20) {
        return sky_color(r.direction);
    }
    
    // Shading
    vec3 color;
    if (hit_sphere >= 0) {
        // Sphere intersection
        color = shade_point(hit_point, hit_normal, hit_material, hit_glass);
        
        // Handle reflections/refractions for glass and chrome
        if (hit_glass) {
            // Glass material - refraction and reflection
            double cos_i = -vec3_dot(hit_normal, r.direction);
            double eta_ratio = r.direction.y > 0 ? hit_material.eta : 1.0 / hit_material.eta;
            
            // Fresnel coefficient
            double reflect_prob = fresnel(cos_i, 1.0, hit_material.eta);
            
            // Random decision for reflection/refraction
            double rand_val = 0.5; // Would be random in real implementation
            
            if (rand_val < reflect_prob) {
                // Reflection
                vec3 reflected_dir = reflect(r.direction, hit_normal);
                ray reflected_ray = {vec3_add(hit_point, vec3_scale(EPSILON, hit_normal)), reflected_dir};
                color = trace_ray(reflected_ray, depth + 1);
            } else {
                // Refraction
                vec3 refracted_dir = refract(r.direction, hit_normal, eta_ratio);
                ray refracted_ray = {vec3_add(hit_point, vec3_scale(-EPSILON, hit_normal)), refracted_dir};
                color = trace_ray(refracted_ray, depth + 1);
            }
        } else {
            // Chrome material - reflection only
            vec3 reflected_dir = reflect(r.direction, hit_normal);
            ray reflected_ray = {vec3_add(hit_point, vec3_scale(EPSILON, hit_normal)), reflected_dir};
            color = trace_ray(reflected_ray, depth + 1);
        }
    } else {
        // Ground plane intersection
        // Apply checkerboard pattern
        int pattern = checker_pattern(hit_point.x, hit_point.z);
        vec3 checker_color = pattern ? (vec3){1.0, 1.0, 1.0} : (vec3){0.0, 0.0, 0.0};
        
        // Shade the checkerboard
        color = shade_point(hit_point, hit_normal, hit_material, 0);
        color = vec3_mul(color, checker_color);
    }
    
    // Apply fog effect
    double fog_factor = exp(-0.035 * closest_t);
    vec3 fog_color = sky_color(r.direction);
    color = vec3_add(vec3_scale(fog_factor, color), vec3_scale(1.0 - fog_factor, fog_color));
    
    return color;
}

// Write TGA file
int write_tga(const char* filename, unsigned char* pixels, int width, int height) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s for writing\n", filename);
        return 0;
    }
    
    // TGA Header (18 bytes)
    unsigned char header[18] = {
        0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        width & 0xFF, (width >> 8) & 0xFF,  // Width (little-endian)
        height & 0xFF, (height >> 8) & 0xFF, // Height (little-endian)
        24,  // Pixel depth
        0x20 // Image descriptor (top-left origin, left-to-right pixel order)
    };
    
    if (fwrite(header, 1, 18, file) != 18) {
        fclose(file);
        fprintf(stderr, "Error: Failed to write TGA header\n");
        return 0;
    }
    
    // Write pixel data (BGR order, row by row from top to bottom)
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            // Write BGR order
            if (fwrite(&pixels[idx + 2], 1, 1, file) != 1 || // Blue
                fwrite(&pixels[idx + 1], 1, 1, file) != 1 || // Green
                fwrite(&pixels[idx + 0], 1, 1, file) != 1) { // Red
                fclose(file);
                fprintf(stderr, "Error: Failed to write pixel data\n");
                return 0;
            }
        }
    }
    
    fclose(file);
    return 1;
}

// Gamma correction
double gamma(double c) {
    if (c <= 0.0031308) {
        return 12.92 * c;
    } else {
        return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
    }
}

// Main function
int main(void) {
    const int WIDTH = 1280;
    const int HEIGHT = 720;
    
    // Allocate pixel buffer
    unsigned char* pixels = (unsigned char*)malloc(WIDTH * HEIGHT * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }
    
    // Camera setup
    vec3 camera_pos = {0.0, 2.6, 6.5};
    vec3 camera_target = {0.0, 1.0, 0.0};
    vec3 camera_up = {0.0, 1.0, 0.0};
    
    // Compute camera coordinate system
    vec3 forward = vec3_normalize(vec3_sub(camera_target, camera_pos));
    vec3 right = vec3_normalize(vec3_cross(forward, camera_up));
    vec3 up = vec3_cross(right, forward);
    
    // Field of view
    double fov = 45.0 * PI / 180.0;
    double aspect_ratio = (double)WIDTH / (double)HEIGHT;
    double half_height = tan(fov / 2.0);
    double half_width = half_height * aspect_ratio;
    
    // Render the image
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            vec3 pixel_color = {0.0, 0.0, 0.0};
            
            // Subpixel sampling
            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    // Compute subpixel position
                    double u = (x + (sx + 0.5) / 2.0) / WIDTH;
                    double v = (y + (sy + 0.5) / 2.0) / HEIGHT;
                    
                    // Scale to [-1, 1]
                    double s = (2.0 * u - 1.0) * half_width;
                    double t = (1.0 - 2.0 * v) * half_height;
                    
                    // Compute ray direction
                    vec3 direction = vec3_normalize(vec3_add(
                        vec3_add(vec3_scale(s, right), vec3_scale(t, up)),
                        forward
                    ));
                    
                    // Trace ray
                    ray r = {camera_pos, direction};
                    vec3 color = trace_ray(r, 0);
                    
                    // Accumulate color
                    pixel_color = vec3_add(pixel_color, color);
                }
            }
            
            // Average the samples
            pixel_color = vec3_scale(0.25, pixel_color);
            
            // Apply gamma correction and convert to 8-bit
            int idx = (y * WIDTH + x) * 3;
            pixels[idx + 0] = (unsigned char)(fmin(1.0, gamma(pixel_color.z)) * 255.0); // Blue
            pixels[idx + 1] = (unsigned char)(fmin(1.0, gamma(pixel_color.y)) * 255.0); // Green
            pixels[idx + 2] = (unsigned char)(fmin(1.0, gamma(pixel_color.x)) * 255.0); // Red
        }
    }
    
    // Write the TGA file
    if (!write_tga("out.tga", pixels, WIDTH, HEIGHT)) {
        free(pixels);
        return 1;
    }
    
    free(pixels);
    printf("Raytracer completed successfully. Output saved to out.tga\n");
    return 0;
}
