#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Vector math functions
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

static inline vec3 vec3_scale(double s, vec3 v) {
    return (vec3){s * v.x, s * v.y, s * v.z};
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

static inline double vec3_length(vec3 v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline double vec3_length_squared(vec3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static inline vec3 vec3_normalize(vec3 v) {
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
const double EPSILON = 1e-4;
const int MAX_BOUNCES = 8;

// Sky gradient parameters
const vec3 SKY_DARK_RED = {0.18, 0.01, 0.015};
const vec3 SKY_ORANGE = {1.0, 0.25, 0.03};

// Scene objects
const sphere GLASS_SPHERE = {{-1.25, 1.0, 0.0}, 1.0};
const sphere CHROME_SPHERE = {{1.25, 1.0, 0.0}, 1.0};
const plane GROUND_PLANE = {{0.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};

// Lights
const light LIGHTS[] = {
    {{-3.5, 6.0, 4.5}, {1.0, 1.0, 1.0}},   // Key light
    {{4.0, 3.0, 3.0}, {0.35, 0.35, 0.35}}, // Fill light
    {{0.5, 5.0, -4.0}, {0.7, 0.7, 0.7}}    // Back light
};
const int NUM_LIGHTS = 3;

// Checkerboard pattern
static inline int checkerboard_pattern(double x, double z) {
    // Use floor to handle negative coordinates correctly
    int ix = (int)floor(x);
    int iz = (int)floor(z);
    return (ix + iz) % 2;
}

// Sky color calculation
static vec3 sky_color(vec3 ray_dir) {
    // Clamp y to [0, 1] for numerical robustness
    double y = fmin(fmax(ray_dir.y, 0.0), 1.0);
    double t = pow(y, 0.65);
    return vec3_add(
        vec3_scale(1.0 - t, SKY_ORANGE),
        vec3_scale(t, SKY_DARK_RED)
    );
}

// Ray-sphere intersection
static int intersect_sphere(ray r, sphere s, double *t) {
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
static int intersect_plane(ray r, plane p, double *t) {
    double denom = vec3_dot(p.normal, r.direction);
    if (fabs(denom) < EPSILON) {
        return 0; // Ray is parallel to plane
    }

    vec3 op = vec3_sub(p.point, r.origin);
    *t = vec3_dot(op, p.normal) / denom;

    if (*t > EPSILON) {
        return 1; // Valid intersection
    }

    return 0; // Intersection behind ray origin
}

// Fresnel coefficient calculation using Schlick's approximation
static double fresnel(double cos_theta, double eta_ratio) {
    double r0 = (1.0 - eta_ratio) / (1.0 + eta_ratio);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow(1.0 - cos_theta, 5.0);
}

// Reflect vector
static vec3 reflect(vec3 incident, vec3 normal) {
    return vec3_sub(incident, vec3_scale(2.0 * vec3_dot(incident, normal), normal));
}

// Refract vector
static int refract(vec3 incident, vec3 normal, double eta_ratio, vec3 *refracted) {
    double cos_i = vec3_dot(normal, incident);
    double k = 1.0 - eta_ratio * eta_ratio * (1.0 - cos_i * cos_i);

    if (k < 0.0) {
        return 0; // Total internal reflection
    }

    *refracted = vec3_sub(
        vec3_scale(eta_ratio, incident),
        vec3_scale(eta_ratio * cos_i + sqrt(k), normal)
    );
    return 1;
}

// Shade a point with lighting
static vec3 shade_point(vec3 point, vec3 normal, material mat, int is_glass) {
    vec3 color = {0.0, 0.0, 0.0};

    // Ambient component
    vec3 ambient = {0.1, 0.1, 0.1};
    color = vec3_add(color, vec3_mul(mat.albedo, ambient));

    // Lighting from all lights
    for (int i = 0; i < NUM_LIGHTS; i++) {
        light l = LIGHTS[i];
        vec3 light_dir = vec3_normalize(vec3_sub(l.position, point));
        double light_distance = vec3_length(vec3_sub(l.position, point));

        // Shadow ray
        ray shadow_ray = {point, light_dir};
        double t;

        // Check if we're in shadow
        int in_shadow = 0;
        if (is_glass) {
            // For glass, allow some light through
            // Simple approximation: attenuate by distance
            vec3 attenuated_intensity = vec3_scale(1.0 / (light_distance * light_distance), l.intensity);
            color = vec3_add(color, vec3_mul(mat.albedo, attenuated_intensity));
        } else {
            // For other materials, check for shadows
            if (intersect_sphere(shadow_ray, GLASS_SPHERE, &t) || 
                intersect_sphere(shadow_ray, CHROME_SPHERE, &t) ||
                intersect_plane(shadow_ray, GROUND_PLANE, &t)) {
                // Shadowed
                in_shadow = 1;
            }

            if (!in_shadow) {
                // Diffuse lighting (Lambert)
                double diff = fmax(0.0, vec3_dot(normal, light_dir));
                vec3 diffuse = vec3_scale(diff, l.intensity);
                color = vec3_add(color, vec3_mul(mat.albedo, diffuse));

                // Specular lighting (Phong)
                vec3 view_dir = vec3_normalize(vec3_scale(-1.0, light_dir)); // Assuming camera at origin
                vec3 reflect_dir = reflect(light_dir, normal);
                double spec = pow(fmax(0.0, vec3_dot(view_dir, reflect_dir)), 32.0);
                vec3 specular = vec3_scale(spec, l.intensity);
                color = vec3_add(color, specular);
            }
        }
    }

    return color;
}

// Trace a ray recursively
static vec3 trace_ray(ray r, int depth) {
    if (depth >= MAX_BOUNCES) {
        return sky_color(r.direction);
    }

    double t = 1e20;
    vec3 hit_point, normal;
    material mat;
    int hit_sphere = 0;
    int is_glass = 0;

    // Ray-sphere intersection tests
    double t1, t2;
    vec3 temp_hit_point, temp_normal;

    // Test glass sphere
    if (intersect_sphere(r, GLASS_SPHERE, &t1)) {
        temp_hit_point = vec3_add(r.origin, vec3_scale(t1, r.direction));
        temp_normal = vec3_normalize(vec3_sub(temp_hit_point, GLASS_SPHERE.center));
        if (t1 < t) {
            t = t1;
            hit_point = temp_hit_point;
            normal = temp_normal;
            mat = (material){{1.0, 1.0, 1.0}, 0.0, 1.0, 1.5}; // Glass material
            hit_sphere = 1;
            is_glass = 1;
        }
    }

    // Test chrome sphere
    if (intersect_sphere(r, CHROME_SPHERE, &t2)) {
        temp_hit_point = vec3_add(r.origin, vec3_scale(t2, r.direction));
        temp_normal = vec3_normalize(vec3_sub(temp_hit_point, CHROME_SPHERE.center));
        if (t2 < t) {
            t = t2;
            hit_point = temp_hit_point;
            normal = temp_normal;
            mat = (material){{0.8, 0.8, 0.8}, 0.95, 0.0, 1.0}; // Chrome material
            hit_sphere = 1;
            is_glass = 0;
        }
    }

    // Test ground plane
    if (intersect_plane(r, GROUND_PLANE, &t)) {
        temp_hit_point = vec3_add(r.origin, vec3_scale(t, r.direction));
        temp_normal = GROUND_PLANE.normal;
        // Ensure normal points upward
        if (temp_normal.y < 0) {
            temp_normal = vec3_scale(-1.0, temp_normal);
        }
        if (t < t1 && t < t2) {
            hit_point = temp_hit_point;
            normal = temp_normal;
            // Checkerboard pattern
            int pattern = checkerboard_pattern(hit_point.x, hit_point.z);
            vec3 albedo = pattern ? (vec3){1.0, 1.0, 1.0} : (vec3){0.0, 0.0, 0.0};
            mat = (material){albedo, 0.0, 0.0, 1.0}; // Diffuse ground
            hit_sphere = 1;
            is_glass = 0;
        }
    }

    // If we hit something
    if (hit_sphere) {
        // Offset ray origin to avoid self-intersection
        vec3 ray_origin = vec3_add(hit_point, vec3_scale(EPSILON, normal));

        // For glass, handle refraction
        if (is_glass) {
            // Calculate reflection and refraction
            vec3 reflect_dir = reflect(r.direction, normal);
            vec3 refract_dir;
            double eta_ratio = 1.0 / mat.ior; // From air to glass
            
            // Determine if we're entering or exiting the sphere
            double cos_i = vec3_dot(normal, r.direction);
            if (cos_i > 0) {
                // Exiting sphere
                eta_ratio = mat.ior / 1.0;
                normal = vec3_scale(-1.0, normal); // Flip normal
            }

            // Calculate reflection coefficient
            double cos_theta = fabs(cos_i);
            double reflect_coeff = fresnel(cos_theta, eta_ratio);

            // Trace reflection ray
            ray reflect_ray = {ray_origin, reflect_dir};
            vec3 reflect_color = trace_ray(reflect_ray, depth + 1);

            // Trace refraction ray if possible
            vec3 refract_color = {0.0, 0.0, 0.0};
            if (refract(r.direction, normal, eta_ratio, &refract_dir)) {
                ray refract_ray = {ray_origin, refract_dir};
                refract_color = trace_ray(refract_ray, depth + 1);
            }

            // Combine reflection and refraction
            vec3 result = vec3_add(
                vec3_scale(reflect_coeff, reflect_color),
                vec3_scale(1.0 - reflect_coeff, refract_color)
            );

            // Apply fog
            double distance = vec3_length(vec3_sub(hit_point, r.origin));
            double transmittance = exp(-0.035 * distance);
            vec3 fog_color = sky_color(r.direction);
            return vec3_add(vec3_scale(transmittance, result), vec3_scale(1.0 - transmittance, fog_color));
        } else {
            // Handle chrome or ground
            vec3 base_color = shade_point(hit_point, normal, mat, is_glass);

            // Handle reflection for chrome
            if (mat.reflectivity > 0.0) {
                vec3 reflect_dir = reflect(r.direction, normal);
                ray reflect_ray = {ray_origin, reflect_dir};
                vec3 reflect_color = trace_ray(reflect_ray, depth + 1);
                base_color = vec3_add(vec3_scale(mat.reflectivity, reflect_color),
                                      vec3_scale(1.0 - mat.reflectivity, base_color));
            }

            // Apply fog
            double distance = vec3_length(vec3_sub(hit_point, r.origin));
            double transmittance = exp(-0.035 * distance);
            vec3 fog_color = sky_color(r.direction);
            return vec3_add(vec3_scale(transmittance, base_color), vec3_scale(1.0 - transmittance, fog_color));
        }
    } else {
        // No hit - return sky color
        return sky_color(r.direction);
    }
}

// Convert RGB to sRGB gamma corrected
static unsigned char gamma_correct(double c) {
    // Apply sRGB gamma curve
    if (c <= 0.0031308) {
        return (unsigned char)(c * 12.92 * 255.0 + 0.5);
    } else {
        return (unsigned char)((1.055 * pow(c, 1.0 / 2.4) - 0.055) * 255.0 + 0.5);
    }
}

// Save image as TGA file
static int save_tga(const char* filename, unsigned char* pixels) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Could not open file %s for writing\n", filename);
        return 0;
    }

    // TGA Header (18 bytes)
    unsigned char header[18] = {
        0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        WIDTH & 0xFF, (WIDTH >> 8) & 0xFF,
        HEIGHT & 0xFF, (HEIGHT >> 8) & 0xFF,
        24, 0x20
    };

    if (fwrite(header, 1, 18, fp) != 18) {
        fclose(fp);
        fprintf(stderr, "Error: Failed to write TGA header\n");
        return 0;
    }

    // Write pixel data (BGR order, top to bottom)
    for (int y = HEIGHT - 1; y >= 0; y--) {
        for (int x = 0; x < WIDTH; x++) {
            int idx = (y * WIDTH + x) * 3;
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

int main(void) {
    // Allocate pixel buffer
    unsigned char* pixels = (unsigned char*)malloc(WIDTH * HEIGHT * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Failed to allocate pixel buffer\n");
        return 1;
    }

    // Camera setup
    vec3 camera_pos = {0.0, 2.6, 6.5};
    vec3 camera_target = {0.0, 1.0, 0.0};
    vec3 camera_up = {0.0, 1.0, 0.0};

    // Calculate camera vectors
    vec3 forward = vec3_normalize(vec3_sub(camera_target, camera_pos));
    vec3 right = vec3_normalize(vec3_cross(forward, camera_up));
    vec3 up = vec3_cross(right, forward);

    // Calculate field of view
    double fov_rad = FOV * M_PI / 180.0;
    double aspect_ratio = (double)WIDTH / (double)HEIGHT;
    double half_height = tan(fov_rad / 2.0);
    double half_width = half_height * aspect_ratio;

    // Render the image
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            vec3 pixel_color = {0.0, 0.0, 0.0};

            // Sample multiple subpixels per pixel (2x2 grid)
            int num_samples = 4;
            for (int sy = 0; sy < 2; sy++) {
                for (int sx = 0; sx < 2; sx++) {
                    // Calculate subpixel position
                    double u = (x + (sx + 0.5) / 2.0) / WIDTH;
                    double v = (y + (sy + 0.5) / 2.0) / HEIGHT;

                    // Scale and offset
                    double scaled_u = (u - 0.5) * 2.0 * half_width;
                    double scaled_v = (v - 0.5) * 2.0 * half_height;

                    // Calculate ray direction
                    vec3 ray_dir = vec3_normalize(vec3_add(
                        vec3_scale(scaled_u, right),
                        vec3_scale(scaled_v, up)
                    ));
                    ray_dir = vec3_add(ray_dir, vec3_scale(-1.0, forward));

                    // Create ray
                    ray r = {camera_pos, ray_dir};

                    // Trace ray
                    vec3 color = trace_ray(r, 0);

                    // Accumulate in linear RGB
                    pixel_color = vec3_add(pixel_color, color);
                }
            }

            // Average the samples
            pixel_color = vec3_scale(1.0 / num_samples, pixel_color);

            // Convert to 8-bit with gamma correction
            int idx = (y * WIDTH + x) * 3;
            pixels[idx] = gamma_correct(pixel_color.z);     // B
            pixels[idx + 1] = gamma_correct(pixel_color.y); // G
            pixels[idx + 2] = gamma_correct(pixel_color.x); // R
        }
    }

    // Save the image
    if (!save_tga("out.tga", pixels)) {
        free(pixels);
        return 1;
    }

    free(pixels);
    return 0;
}
