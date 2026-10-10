#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Vector operations
typedef struct {
    double x, y, z;
} vec3;

static vec3 vec3_add(vec3 a, vec3 b) {
    return (vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static vec3 vec3_sub(vec3 a, vec3 b) {
    return (vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static vec3 vec3_mul(vec3 a, double s) {
    return (vec3){a.x * s, a.y * s, a.z * s};
}

static vec3 vec3_div(vec3 a, double s) {
    return (vec3){a.x / s, a.y / s, a.z / s};
}

static double vec3_dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static vec3 vec3_cross(vec3 a, vec3 b) {
    return (vec3){a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

static double vec3_length(vec3 a) {
    return sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}

static double vec3_length_squared(vec3 a) {
    return a.x * a.x + a.y * a.y + a.z * a.z;
}

static vec3 vec3_normalize(vec3 a) {
    double len = vec3_length(a);
    if (len > 1e-10) {
        return vec3_div(a, len);
    }
    return (vec3){0.0, 0.0, 0.0};
}

// Component-wise vector multiplication
static vec3 vec3_mul_components(vec3 a, vec3 b) {
    return (vec3){a.x * b.x, a.y * b.y, a.z * b.z};
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
    vec3 normal;
    double distance; // distance from origin along normal
} plane;

// Material structure
typedef struct {
    vec3 albedo;
    double reflectivity;
    double refractivity;
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
const double EPSILON = 1e-4;
const int MAX_BOUNCES = 8;

// Sky colors
const vec3 SKY_DARK_RED = {0.18, 0.01, 0.015};
const vec3 SKY_ORANGE = {1.0, 0.25, 0.03};

// Materials
const material GLASS_MATERIAL = {{1.0, 1.0, 1.0}, 0.0, 1.0, 1.5};
const material CHROME_MATERIAL = {{0.8, 0.8, 0.8}, 0.95, 0.0, 1.0};

// Lights
const light LIGHTS[] = {
    {{-3.5, 6.0, 4.5}, {1.0, 1.0, 1.0}},   // Key light
    {{4.0, 3.0, 3.0}, {0.35, 0.35, 0.35}}, // Fill light
    {{0.5, 5.0, -4.0}, {0.7, 0.7, 0.7}}    // Back light
};
const int NUM_LIGHTS = 3;

// Spheres
const sphere SPHERES[] = {
    {{-1.25, 1.0, 0.0}, 1.0}, // Glass sphere
    {{1.25, 1.0, 0.0}, 1.0}   // Chrome sphere
};
const int NUM_SPHERES = 2;

// Ground plane
const plane GROUND_PLANE = {{0.0, 1.0, 0.0}, 0.0};

// Camera
const vec3 CAMERA_POS = {0.0, 2.6, 6.5};
const vec3 CAMERA_TARGET = {0.0, 1.0, 0.0};
const vec3 CAMERA_UP = {0.0, 1.0, 0.0};

// Sky color function
static vec3 sky_color(vec3 direction) {
    double t = pow(fabs(direction.y), 0.65);
    t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
    return (vec3){
        (1.0 - t) * SKY_ORANGE.x + t * SKY_DARK_RED.x,
        (1.0 - t) * SKY_ORANGE.y + t * SKY_DARK_RED.y,
        (1.0 - t) * SKY_ORANGE.z + t * SKY_DARK_RED.z
    };
}

// Checkerboard pattern
static vec3 checkerboard_pattern(double x, double z) {
    int ix = (int)floor(x);
    int iz = (int)floor(z);
    if ((ix + iz) % 2 == 0) {
        return (vec3){1.0, 1.0, 1.0}; // White
    } else {
        return (vec3){0.0, 0.0, 0.0}; // Black
    }
}

// Ray-sphere intersection
static int ray_sphere_intersect(ray r, sphere s, double *t) {
    vec3 oc = vec3_sub(r.origin, s.center);
    double a = vec3_dot(r.direction, r.direction);
    double b = 2.0 * vec3_dot(oc, r.direction);
    double c = vec3_dot(oc, oc) - s.radius * s.radius;
    double discriminant = b * b - 4 * a * c;

    if (discriminant < 0) {
        return 0; // No intersection
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

    return 0; // Intersection behind ray origin
}

// Ray-plane intersection
static int ray_plane_intersect(ray r, plane p, double *t) {
    double denom = vec3_dot(p.normal, r.direction);
    if (fabs(denom) < EPSILON) {
        return 0; // Ray is parallel to plane
    }

    *t = (p.distance - vec3_dot(p.normal, r.origin)) / denom;
    return *t > EPSILON; // Only valid if intersection is in front of ray origin
}

// Fresnel reflection coefficient (Schlick's approximation)
static double fresnel(double cos_theta, double eta_ratio) {
    double r0 = (1.0 - eta_ratio) / (1.0 + eta_ratio);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow(1.0 - cos_theta, 5.0);
}

// Reflect vector
static vec3 reflect(vec3 incident, vec3 normal) {
    return vec3_sub(incident, vec3_mul(normal, 2.0 * vec3_dot(incident, normal)));
}

// Refract vector
static vec3 refract(vec3 incident, vec3 normal, double eta_ratio) {
    double cos_i = vec3_dot(incident, normal);
    double k = 1.0 - eta_ratio * eta_ratio * (1.0 - cos_i * cos_i);
    
    if (k < 0.0) {
        return (vec3){0.0, 0.0, 0.0}; // Total internal reflection
    }

    vec3 refracted = vec3_sub(
        vec3_mul(incident, eta_ratio),
        vec3_mul(normal, eta_ratio * cos_i + sqrt(k))
    );
    return refracted;
}

// Trace ray
static vec3 trace_ray(ray r, int depth) {
    if (depth >= MAX_BOUNCES) {
        return sky_color(r.direction);
    }

    double closest_t = 1e20;
    int hit_object = -1;
    vec3 hit_point, hit_normal;
    material hit_material;

    // Check sphere intersections
    for (int i = 0; i < NUM_SPHERES; i++) {
        double t;
        if (ray_sphere_intersect(r, SPHERES[i], &t) && t < closest_t) {
            closest_t = t;
            hit_object = i;
            hit_point = vec3_add(r.origin, vec3_mul(r.direction, t));
            
            // Calculate normal
            hit_normal = vec3_normalize(vec3_sub(hit_point, SPHERES[i].center));
            
            // For glass sphere, we need to check if we're entering or exiting
            if (i == 0) { // Glass sphere
                double dot = vec3_dot(r.direction, hit_normal);
                if (dot > 0.0) {
                    // Exiting the sphere, flip normal
                    hit_normal = vec3_mul(hit_normal, -1.0);
                }
            }
            
            hit_material = (i == 0) ? GLASS_MATERIAL : CHROME_MATERIAL;
        }
    }

    // Check plane intersection
    double t_plane;
    if (ray_plane_intersect(r, GROUND_PLANE, &t_plane) && t_plane < closest_t) {
        closest_t = t_plane;
        hit_object = NUM_SPHERES; // Ground plane
        hit_point = vec3_add(r.origin, vec3_mul(r.direction, t_plane));
        
        // Calculate normal (always pointing up for ground plane)
        hit_normal = (vec3){0.0, 1.0, 0.0};
        
        // Get checkerboard color
        vec3 checker_color = checkerboard_pattern(hit_point.x, hit_point.z);
        hit_material.albedo = checker_color;
        hit_material.reflectivity = 0.0;
        hit_material.refractivity = 0.0;
        hit_material.ior = 1.0;
    }

    // If no intersection, return sky color
    if (hit_object == -1) {
        return sky_color(r.direction);
    }

    // Shadow ray test
    vec3 color = {0.0, 0.0, 0.0};
    
    // Ambient lighting
    color = vec3_add(color, vec3_mul(hit_material.albedo, 0.1));

    // Light calculations
    for (int i = 0; i < NUM_LIGHTS; i++) {
        vec3 light_dir = vec3_normalize(vec3_sub(LIGHTS[i].position, hit_point));
        double light_distance = vec3_length(vec3_sub(LIGHTS[i].position, hit_point));
        
        // Shadow ray
        ray shadow_ray = {vec3_add(hit_point, vec3_mul(hit_normal, EPSILON)), light_dir};
        int in_shadow = 0;
        
        // Check if shadow ray hits any object before reaching the light
        for (int j = 0; j < NUM_SPHERES; j++) {
            double t;
            if (j == 0 && hit_object == 0) continue; // Skip glass sphere for shadow check if hitting glass
            if (ray_sphere_intersect(shadow_ray, SPHERES[j], &t) && t < light_distance) {
                in_shadow = 1;
                break;
            }
        }
        
        // Check ground plane for shadow
        if (!in_shadow) {
            double t;
            if (ray_plane_intersect(shadow_ray, GROUND_PLANE, &t) && t < light_distance) {
                in_shadow = 1;
            }
        }

        if (!in_shadow) {
            // Diffuse lighting (Lambert)
            double diff = fmax(0.0, vec3_dot(hit_normal, light_dir));
            vec3 light_intensity = vec3_mul(LIGHTS[i].intensity, diff);
            
            // Specular lighting (Phong)
            vec3 view_dir = vec3_normalize(vec3_sub(CAMERA_POS, hit_point));
            vec3 reflect_dir = reflect(vec3_mul(light_dir, -1.0), hit_normal);
            double spec = pow(fmax(0.0, vec3_dot(view_dir, reflect_dir)), 32.0);
            
            if (hit_object < NUM_SPHERES) { // Sphere
                if (hit_object == 0) { // Glass sphere
                    // Glass doesn't have specular highlights, only diffuse
                    color = vec3_add(color, vec3_mul_components(hit_material.albedo, light_intensity));
                } else { // Chrome sphere
                    // Chrome has both diffuse and specular
                    color = vec3_add(color, vec3_mul_components(hit_material.albedo, light_intensity));
                    color = vec3_add(color, vec3_mul_components((vec3){1.0, 1.0, 1.0}, vec3_mul(light_intensity, spec * 0.5)));
                }
            } else { // Ground plane
                color = vec3_add(color, vec3_mul_components(hit_material.albedo, light_intensity));
            }
        }
    }

    // Reflection and refraction
    vec3 reflect_color = {0.0, 0.0, 0.0};
    vec3 refract_color = {0.0, 0.0, 0.0};

    if (hit_material.reflectivity > 0.0 || hit_material.refractivity > 0.0) {
        vec3 reflect_dir = reflect(r.direction, hit_normal);
        ray reflect_ray = {vec3_add(hit_point, vec3_mul(hit_normal, EPSILON)), reflect_dir};
        reflect_color = trace_ray(reflect_ray, depth + 1);

        if (hit_material.refractivity > 0.0) {
            double eta_ratio = (hit_object == 0) ? 1.0 / hit_material.ior : hit_material.ior / 1.0;
            vec3 refract_dir = refract(r.direction, hit_normal, eta_ratio);
            if (vec3_length(refract_dir) > 0.0) {
                ray refract_ray = {vec3_add(hit_point, vec3_mul(hit_normal, -EPSILON)), refract_dir};
                refract_color = trace_ray(refract_ray, depth + 1);
            }
        }
    }

    // Mix colors based on material properties
    if (hit_object < NUM_SPHERES) { // Sphere
        if (hit_object == 0) { // Glass sphere
            // Use Fresnel for glass
            double cos_theta = vec3_dot(r.direction, hit_normal);
            double reflect_coeff = fresnel(cos_theta, hit_material.ior);
            double refract_coeff = 1.0 - reflect_coeff;
            color = vec3_add(vec3_mul(reflect_color, reflect_coeff), vec3_mul(refract_color, refract_coeff));
        } else { // Chrome sphere
            // Use material reflectivity
            color = vec3_add(vec3_mul(color, 1.0 - hit_material.reflectivity), vec3_mul(reflect_color, hit_material.reflectivity));
        }
    } else { // Ground plane
        color = vec3_add(vec3_mul(color, 1.0 - hit_material.reflectivity), vec3_mul(reflect_color, hit_material.reflectivity));
    }

    // Apply fog
    double transmittance = exp(-0.035 * closest_t);
    vec3 fog_color = sky_color(r.direction);
    color = vec3_add(vec3_mul(color, transmittance), vec3_mul(fog_color, 1.0 - transmittance));

    return color;
}

// Convert linear RGB to sRGB
static double linear_to_srgb(double c) {
    if (c <= 0.0031308) {
        return 12.92 * c;
    } else {
        return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
    }
}

// Save TGA file
static int save_tga(const char* filename, unsigned char* pixels, int width, int height) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open %s for writing\n", filename);
        return 0;
    }

    // TGA Header (18 bytes)
    unsigned char header[18] = {0};
    header[2] = 2; // Image type: uncompressed true-color
    header[12] = width & 0xFF;
    header[13] = (width >> 8) & 0xFF;
    header[14] = height & 0xFF;
    header[15] = (height >> 8) & 0xFF;
    header[16] = 24; // Bits per pixel
    header[17] = 0x20; // Descriptor: top-left origin, left-to-right pixel order

    if (fwrite(header, 1, 18, fp) != 18) {
        fclose(fp);
        fprintf(stderr, "Error: Failed to write TGA header\n");
        return 0;
    }

    // Write pixel data (BGR format, top to bottom)
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            unsigned char b = pixels[idx];
            unsigned char g = pixels[idx + 1];
            unsigned char r = pixels[idx + 2];
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

int main() {
    // Allocate pixel buffer
    unsigned char* pixels = malloc(WIDTH * HEIGHT * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }

    // Set up camera
    vec3 camera_forward = vec3_normalize(vec3_sub(CAMERA_TARGET, CAMERA_POS));
    vec3 camera_right = vec3_normalize(vec3_cross(camera_forward, CAMERA_UP));
    vec3 camera_up = vec3_cross(camera_right, camera_forward);

    // Calculate fov in radians
    double fov_rad = FOV * M_PI / 180.0;
    double half_height = tan(fov_rad / 2.0);
    double half_width = half_height * ASPECT_RATIO;

    // Render image
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            vec3 pixel_color = {0.0, 0.0, 0.0};

            // Sub-pixel sampling (2x2 grid)
            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    // Calculate sub-pixel position
                    double u = (x + (sx + 0.5) / 2.0) / WIDTH;
                    double v = (y + (sy + 0.5) / 2.0) / HEIGHT;

                    // Convert to normalized device coordinates
                    double ndc_x = (2.0 * u - 1.0) * half_width;
                    double ndc_y = (1.0 - 2.0 * v) * half_height;

                    // Calculate ray direction
                    vec3 ray_dir = vec3_normalize(vec3_add(
                        vec3_add(vec3_mul(camera_right, ndc_x),
                                 vec3_mul(camera_up, ndc_y)),
                        camera_forward
                    ));

                    ray r = {CAMERA_POS, ray_dir};
                    vec3 color = trace_ray(r, 0);

                    // Accumulate color
                    pixel_color = vec3_add(pixel_color, color);
                }
            }

            // Average the samples
            pixel_color = vec3_div(pixel_color, 4.0);

            // Convert to sRGB and clamp
            int idx = (y * WIDTH + x) * 3;
            pixels[idx] = (unsigned char)(fmin(1.0, linear_to_srgb(pixel_color.z)) * 255.0); // B
            pixels[idx + 1] = (unsigned char)(fmin(1.0, linear_to_srgb(pixel_color.y)) * 255.0); // G
            pixels[idx + 2] = (unsigned char)(fmin(1.0, linear_to_srgb(pixel_color.x)) * 255.0); // R
        }
    }

    // Save image
    if (!save_tga("out.tga", pixels, WIDTH, HEIGHT)) {
        free(pixels);
        return 1;
    }

    free(pixels);
    return 0;
}
