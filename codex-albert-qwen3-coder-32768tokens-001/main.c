#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Vector operations
typedef struct {
    double x, y, z;
} vec3;

vec3 vec3_add(vec3 a, vec3 b) {
    vec3 r;
    r.x = a.x + b.x;
    r.y = a.y + b.y;
    r.z = a.z + b.z;
    return r;
}

vec3 vec3_sub(vec3 a, vec3 b) {
    vec3 r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    return r;
}

vec3 vec3_mul(vec3 a, double s) {
    vec3 r;
    r.x = a.x * s;
    r.y = a.y * s;
    r.z = a.z * s;
    return r;
}

vec3 vec3_div(vec3 a, double s) {
    vec3 r;
    r.x = a.x / s;
    r.y = a.y / s;
    r.z = a.z / s;
    return r;
}

double vec3_dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

vec3 vec3_cross(vec3 a, vec3 b) {
    vec3 r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return r;
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
        return vec3_div(v, len);
    }
    vec3 r;
    r.x = 0.0;
    r.y = 0.0;
    r.z = 0.0;
    return r;
}

vec3 vec3_negate(vec3 v) {
    vec3 r;
    r.x = -v.x;
    r.y = -v.y;
    r.z = -v.z;
    return r;
}

// Ray structure
typedef struct {
    vec3 origin;
    vec3 direction;
} ray;

// Material structure
typedef enum {
    MATERIAL_LAMBERTIAN,
    MATERIAL_METAL,
    MATERIAL_DIELECTRIC
} material_type;

typedef struct {
    material_type type;
    vec3 albedo;
    double fuzz;
    double ref_idx;
} material;

// Sphere structure
typedef struct {
    vec3 center;
    double radius;
    material mat;
} sphere;

// Plane structure
typedef struct {
    vec3 point;
    vec3 normal;
    material mat;
} plane;

// Scene objects
sphere spheres[2];
plane ground_plane;

// Light structure
typedef struct {
    vec3 position;
    vec3 intensity;
} light;

light lights[3];

// Sky parameters
vec3 sky_zenith;
vec3 sky_horizon;

// Constants
const double PI = 3.14159265358979323846;
const double EPSILON = 1e-4;
const int MAX_DEPTH = 8;
const int SAMPLES_PER_PIXEL = 4;

// Forward declarations
vec3 ray_color(ray r, int depth);
int intersect_sphere(ray r, sphere s, double *t);
int intersect_plane(ray r, plane p, double *t);
vec3 reflect(vec3 v, vec3 n);
double schlick(double cosine, double ref_idx);
vec3 sky_color(vec3 direction);
vec3 gamma22(vec3 color);
void write_tga(const char *filename, unsigned char *pixels, int width, int height);

// Initialize scene
void init_scene() {
    // Initialize sky parameters
    sky_zenith.x = 0.18;
    sky_zenith.y = 0.01;
    sky_zenith.z = 0.015;
    
    sky_horizon.x = 1.0;
    sky_horizon.y = 0.25;
    sky_horizon.z = 0.03;

    // Initialize spheres
    spheres[0].center.x = -1.25;
    spheres[0].center.y = 1.0;
    spheres[0].center.z = 0.0;  // Glass sphere
    spheres[0].radius = 1.0;
    spheres[0].mat.type = MATERIAL_DIELECTRIC;
    spheres[0].mat.albedo.x = 1.0;
    spheres[0].mat.albedo.y = 1.0;
    spheres[0].mat.albedo.z = 1.0;
    spheres[0].mat.ref_idx = 1.5;

    spheres[1].center.x = 1.25;
    spheres[1].center.y = 1.0;
    spheres[1].center.z = 0.0;   // Chrome sphere
    spheres[1].radius = 1.0;
    spheres[1].mat.type = MATERIAL_METAL;
    spheres[1].mat.albedo.x = 0.8;
    spheres[1].mat.albedo.y = 0.8;
    spheres[1].mat.albedo.z = 0.8;
    spheres[1].mat.fuzz = 0.05;

    // Initialize ground plane
    ground_plane.point.x = 0.0;
    ground_plane.point.y = 0.0;
    ground_plane.point.z = 0.0;
    ground_plane.normal.x = 0.0;
    ground_plane.normal.y = 1.0;
    ground_plane.normal.z = 0.0;
    ground_plane.mat.type = MATERIAL_LAMBERTIAN;
    ground_plane.mat.albedo.x = 0.0;  // Will be overridden by checkerboard
    ground_plane.mat.albedo.y = 0.0;
    ground_plane.mat.albedo.z = 0.0;

    // Initialize lights
    lights[0].position.x = -3.5;
    lights[0].position.y = 6.0;
    lights[0].position.z = 4.5;
    lights[0].intensity.x = 1.0;
    lights[0].intensity.y = 1.0;
    lights[0].intensity.z = 1.0;

    lights[1].position.x = 4.0;
    lights[1].position.y = 3.0;
    lights[1].position.z = 3.0;
    lights[1].intensity.x = 0.35;
    lights[1].intensity.y = 0.35;
    lights[1].intensity.z = 0.35;

    lights[2].position.x = 0.5;
    lights[2].position.y = 5.0;
    lights[2].position.z = -4.0;
    lights[2].intensity.x = 0.7;
    lights[2].intensity.y = 0.7;
    lights[2].intensity.z = 0.7;
}

// Check if a point is on a checkerboard tile
int is_black_tile(double x, double z) {
    int ix = (int)floor(x);
    int iz = (int)floor(z);
    return (ix + iz) % 2 == 0;
}

// Get checkerboard color at a point
vec3 checkerboard_color(double x, double z) {
    vec3 result;
    if (is_black_tile(x, z)) {
        result.x = 0.0;
        result.y = 0.0;
        result.z = 0.0;
    } else {
        result.x = 1.0;
        result.y = 1.0;
        result.z = 1.0;
    }
    return result;
}

// Ray-sphere intersection
int intersect_sphere(ray r, sphere s, double *t) {
    vec3 oc;
    oc.x = r.origin.x - s.center.x;
    oc.y = r.origin.y - s.center.y;
    oc.z = r.origin.z - s.center.z;
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
    double denom = vec3_dot(p.normal, r.direction);
    if (fabs(denom) > 1e-6) {
        vec3 op;
        op.x = p.point.x - r.origin.x;
        op.y = p.point.y - r.origin.y;
        op.z = p.point.z - r.origin.z;
        *t = vec3_dot(op, p.normal) / denom;
        if (*t > EPSILON) {
            return 1;
        }
    }
    return 0;
}

// Reflect vector
vec3 reflect(vec3 v, vec3 n) {
    vec3 result;
    double temp = 2.0 * vec3_dot(v, n);
    result.x = v.x - temp * n.x;
    result.y = v.y - temp * n.y;
    result.z = v.z - temp * n.z;
    return result;
}

// Schlick's approximation for Fresnel effect
double schlick(double cosine, double ref_idx) {
    double r0 = (1.0 - ref_idx) / (1.0 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow((1.0 - cosine), 5.0);
}

// Sky color calculation
vec3 sky_color(vec3 direction) {
    double t = pow(fabs(direction.y), 0.65);
    t = fmax(0.0, fmin(1.0, t));  // Clamp t to [0, 1]
    vec3 result;
    result.x = (1.0 - t) * sky_horizon.x + t * sky_zenith.x;
    result.y = (1.0 - t) * sky_horizon.y + t * sky_zenith.y;
    result.z = (1.0 - t) * sky_horizon.z + t * sky_zenith.z;
    return result;
}

// Compute lighting for a surface point
vec3 compute_lighting(vec3 point, vec3 normal, vec3 view_dir, material mat) {
    vec3 color;
    color.x = 0.0;
    color.y = 0.0;
    color.z = 0.0;

    // Ambient component
    vec3 ambient;
    ambient.x = 0.1;
    ambient.y = 0.1;
    ambient.z = 0.1;
    color = vec3_add(color, ambient);

    // Light contributions
    int i;
    for (i = 0; i < 3; i++) {
        vec3 light_dir;
        light_dir.x = lights[i].position.x - point.x;
        light_dir.y = lights[i].position.y - point.y;
        light_dir.z = lights[i].position.z - point.z;
        double light_distance = vec3_length(light_dir);
        light_dir = vec3_normalize(light_dir);

        // Shadow ray
        ray shadow_ray;
        shadow_ray.origin = vec3_add(point, vec3_mul(normal, EPSILON));
        shadow_ray.direction = light_dir;

        // Check if point is in shadow
        int in_shadow = 0;
        double t;

        // Test against spheres
        int j;
        for (j = 0; j < 2; j++) {
            if (intersect_sphere(shadow_ray, spheres[j], &t)) {
                if (t < light_distance) {
                    in_shadow = 1;
                    break;
                }
            }
        }

        // Test against ground plane
        if (!in_shadow) {
            if (intersect_plane(shadow_ray, ground_plane, &t)) {
                if (t < light_distance) {
                    in_shadow = 1;
                }
            }
        }

        if (!in_shadow) {
            // Diffuse lighting
            double diffuse = fmax(0.0, vec3_dot(normal, light_dir));

            // Specular lighting (only for metals and dielectrics)
            vec3 reflect_dir = reflect(vec3_negate(light_dir), normal);
            double specular = 0.0;
            if (mat.type != MATERIAL_LAMBERTIAN) {
                specular = pow(fmax(0.0, vec3_dot(view_dir, reflect_dir)), 16.0);
            }

            vec3 light_contribution;
            if (mat.type == MATERIAL_DIELECTRIC) {
                // Dielectric material - use light intensity directly
                light_contribution.x = lights[i].intensity.x * diffuse;
                light_contribution.y = lights[i].intensity.y * diffuse;
                light_contribution.z = lights[i].intensity.z * diffuse;
            } else {
                // Lambertian or metal - use albedo
                light_contribution.x = mat.albedo.x * diffuse;
                light_contribution.y = mat.albedo.y * diffuse;
                light_contribution.z = mat.albedo.z * diffuse;
            }

            light_contribution.x *= lights[i].intensity.x;
            light_contribution.y *= lights[i].intensity.y;
            light_contribution.z *= lights[i].intensity.z;

            // Add specular component for non-lambertian materials
            if (mat.type != MATERIAL_LAMBERTIAN) {
                vec3 spec_contribution;
                spec_contribution.x = lights[i].intensity.x * specular;
                spec_contribution.y = lights[i].intensity.y * specular;
                spec_contribution.z = lights[i].intensity.z * specular;
                light_contribution = vec3_add(light_contribution, spec_contribution);
            }

            color = vec3_add(color, light_contribution);
        }
    }

    return color;
}

// Ray color calculation with recursive ray tracing
vec3 ray_color(ray r, int depth) {
    if (depth <= 0) {
        return sky_color(r.direction);
    }

    double t;
    int hit_sphere = 0;
    int hit_plane = 0;
    int closest_hit = -1;
    double closest_t = 1e20;
    vec3 hit_point, hit_normal;
    material hit_mat;

    // Test intersection with spheres
    int i;
    for (i = 0; i < 2; i++) {
        if (intersect_sphere(r, spheres[i], &t)) {
            if (t < closest_t) {
                closest_t = t;
                closest_hit = i;
                hit_sphere = 1;
            }
        }
    }

    // Test intersection with ground plane
    if (intersect_plane(r, ground_plane, &t)) {
        if (t < closest_t) {
            closest_t = t;
            closest_hit = 2;  // Special marker for plane
            hit_plane = 1;
        }
    }

    if (closest_hit >= 0) {
        // Calculate hit point and normal
        hit_point.x = r.origin.x + r.direction.x * closest_t;
        hit_point.y = r.origin.y + r.direction.y * closest_t;
        hit_point.z = r.origin.z + r.direction.z * closest_t;

        if (hit_sphere) {
            // Sphere hit
            sphere s = spheres[closest_hit];
            hit_normal.x = hit_point.x - s.center.x;
            hit_normal.y = hit_point.y - s.center.y;
            hit_normal.z = hit_point.z - s.center.z;
            hit_normal = vec3_normalize(hit_normal);

            // Flip normal if ray is inside sphere
            if (vec3_dot(r.direction, hit_normal) > 0) {
                hit_normal = vec3_negate(hit_normal);
            }

            hit_mat = s.mat;
        } else if (hit_plane) {
            // Plane hit
            hit_normal = ground_plane.normal;
            hit_mat = ground_plane.mat;
        }

        // Compute lighting
        vec3 view_dir = vec3_negate(r.direction);
        vec3 base_color = compute_lighting(hit_point, hit_normal, view_dir, hit_mat);

        // Handle different material types
        vec3 final_color;

        if (hit_mat.type == MATERIAL_DIELECTRIC) {
            // Dielectric material (glass)
            vec3 reflected;
            ray temp_ray;
            temp_ray.origin = vec3_add(hit_point, vec3_mul(hit_normal, EPSILON));
            temp_ray.direction = reflect(r.direction, hit_normal);
            reflected = ray_color(temp_ray, depth - 1);
            
            vec3 refracted;
            refracted.x = 0.0;
            refracted.y = 0.0;
            refracted.z = 0.0;

            // Calculate refraction
            double cos_theta = fmin(-vec3_dot(r.direction, hit_normal), 1.0);
            double sin_theta = sqrt(1.0 - cos_theta * cos_theta);
            double refraction_ratio = hit_mat.ref_idx;

            if (sin_theta * refraction_ratio <= 1.0) {
                // Refraction occurs
                vec3 refract_dir;
                refract_dir.x = r.direction.x * refraction_ratio - hit_normal.x * (refraction_ratio * cos_theta + sqrt(1.0 - refraction_ratio * refraction_ratio * sin_theta * sin_theta));
                refract_dir.y = r.direction.y * refraction_ratio - hit_normal.y * (refraction_ratio * cos_theta + sqrt(1.0 - refraction_ratio * refraction_ratio * sin_theta * sin_theta));
                refract_dir.z = r.direction.z * refraction_ratio - hit_normal.z * (refraction_ratio * cos_theta + sqrt(1.0 - refraction_ratio * refraction_ratio * sin_theta * sin_theta));
                refract_dir = vec3_normalize(refract_dir);
                
                vec3 refracted_point = vec3_add(hit_point, vec3_mul(hit_normal, -EPSILON));
                ray refract_ray;
                refract_ray.origin = refracted_point;
                refract_ray.direction = refract_dir;
                refracted = ray_color(refract_ray, depth - 1);
            } else {
                // Total internal reflection
                temp_ray.origin = vec3_add(hit_point, vec3_mul(hit_normal, EPSILON));
                temp_ray.direction = reflect(r.direction, hit_normal);
                reflected = ray_color(temp_ray, depth - 1);
            }

            // Fresnel effect
            double reflect_prob = schlick(cos_theta, hit_mat.ref_idx);
            final_color.x = reflected.x * reflect_prob + refracted.x * (1.0 - reflect_prob);
            final_color.y = reflected.y * reflect_prob + refracted.y * (1.0 - reflect_prob);
            final_color.z = reflected.z * reflect_prob + refracted.z * (1.0 - reflect_prob);
        } else if (hit_mat.type == MATERIAL_METAL) {
            // Metal material (chrome)
            vec3 reflect_dir = reflect(r.direction, hit_normal);
            // Add some fuzziness to the reflection
            vec3 fuzzed_reflect_dir;
            fuzzed_reflect_dir.x = reflect_dir.x + ((rand() / (double)RAND_MAX) * 2.0 - 1.0) * hit_mat.fuzz;
            fuzzed_reflect_dir.y = reflect_dir.y + ((rand() / (double)RAND_MAX) * 2.0 - 1.0) * hit_mat.fuzz;
            fuzzed_reflect_dir.z = reflect_dir.z + ((rand() / (double)RAND_MAX) * 2.0 - 1.0) * hit_mat.fuzz;
            fuzzed_reflect_dir = vec3_normalize(fuzzed_reflect_dir);
            ray reflect_ray;
            reflect_ray.origin = vec3_add(hit_point, vec3_mul(hit_normal, EPSILON));
            reflect_ray.direction = fuzzed_reflect_dir;
            final_color = ray_color(reflect_ray, depth - 1);
        } else {
            // Lambertian material (ground)
            // Override with checkerboard color
            vec3 checker_color = checkerboard_color(hit_point.x, hit_point.z);
            final_color.x = checker_color.x * base_color.x;
            final_color.y = checker_color.y * base_color.y;
            final_color.z = checker_color.z * base_color.z;
        }

        // Apply fog
        double transmittance = exp(-0.035 * closest_t);
        vec3 fog_color = sky_color(r.direction);
        final_color.x = transmittance * final_color.x + (1.0 - transmittance) * fog_color.x;
        final_color.y = transmittance * final_color.y + (1.0 - transmittance) * fog_color.y;
        final_color.z = transmittance * final_color.z + (1.0 - transmittance) * fog_color.z;

        return final_color;
    } else {
        // No intersection - return sky color
        return sky_color(r.direction);
    }
}

// Gamma correction (sRGB)
vec3 gamma22(vec3 color) {
    vec3 result;
    result.x = pow(fmax(0.0, fmin(1.0, color.x)), 1.0/2.2);
    result.y = pow(fmax(0.0, fmin(1.0, color.y)), 1.0/2.2);
    result.z = pow(fmax(0.0, fmin(1.0, color.z)), 1.0/2.2);
    return result;
}

// Write TGA file
void write_tga(const char *filename, unsigned char *pixels, int width, int height) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Could not open file %s for writing\n", filename);
        exit(1);
    }

    // TGA Header (18 bytes)
    unsigned char header[18];
    header[0] = 0;
    header[1] = 0;
    header[2] = 2;  // Image type: uncompressed truecolor
    header[3] = 0;
    header[4] = 0;
    header[5] = 0;
    header[6] = 0;
    header[7] = 0;
    header[8] = 0;
    header[9] = 0;
    header[10] = 0;
    header[11] = 0;
    header[12] = width & 0xFF;          // Width (low byte)
    header[13] = (width >> 8) & 0xFF;   // Width (high byte)
    header[14] = height & 0xFF;         // Height (low byte)
    header[15] = (height >> 8) & 0xFF;  // Height (high byte)
    header[16] = 24;  // Pixel depth
    header[17] = 0x20; // Descriptor: top-left origin, left-to-right pixel order

    fwrite(header, 1, 18, fp);

    // Write pixels (BGR order, bottom to top)
    int y, x;
    for (y = height - 1; y >= 0; y--) {
        for (x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            // BGR order
            fputc(pixels[idx + 2], fp);  // Blue
            fputc(pixels[idx + 1], fp);  // Green
            fputc(pixels[idx + 0], fp);  // Red
        }
    }

    fclose(fp);
}

// Main function
int main() {
    // Initialize scene
    init_scene();

    const int width = 1280;
    const int height = 720;
    const double aspect_ratio = (double)width / (double)height;
    const double fov = 45.0 * PI / 180.0;  // Convert to radians
    const double half_height = tan(fov / 2.0);
    const double half_width = aspect_ratio * half_height;

    // Camera setup
    vec3 camera_pos;
    camera_pos.x = 0.0;
    camera_pos.y = 2.6;
    camera_pos.z = 6.5;
    vec3 camera_target;
    camera_target.x = 0.0;
    camera_target.y = 1.0;
    camera_target.z = 0.0;
    vec3 camera_up;
    camera_up.x = 0.0;
    camera_up.y = 1.0;
    camera_up.z = 0.0;

    // Camera coordinate system
    vec3 w;
    w.x = camera_pos.x - camera_target.x;
    w.y = camera_pos.y - camera_target.y;
    w.z = camera_pos.z - camera_target.z;
    w = vec3_normalize(w);
    vec3 u;
    u = vec3_cross(camera_up, w);
    u = vec3_normalize(u);
    vec3 v;
    v = vec3_cross(w, u);

    // Allocate pixel buffer
    unsigned char *pixels = malloc(width * height * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Could not allocate memory for pixels\n");
        return 1;
    }

    // Render image
    int y, x, s;
    // Déclarations au début des fonctions
    vec3 pixel_color;
    vec3 ray_dir;
    ray r;
    vec3 color;
    
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pixel_color.x = 0.0;
            pixel_color.y = 0.0;
            pixel_color.z = 0.0;

            // Multi-sampling
            for (s = 0; s < SAMPLES_PER_PIXEL; s++) {
                // Jittered sampling
                double u_sample = (x + (rand() / (double)RAND_MAX)) / width;
                double v_sample = (y + (rand() / (double)RAND_MAX)) / height;

                // Calculate ray direction
                ray_dir.x = u.x * (u_sample - 0.5) * 2.0 * half_width + v.x * (v_sample - 0.5) * 2.0 * half_height + w.x * -1.0;
                ray_dir.y = u.y * (u_sample - 0.5) * 2.0 * half_width + v.y * (v_sample - 0.5) * 2.0 * half_height + w.y * -1.0;
                ray_dir.z = u.z * (u_sample - 0.5) * 2.0 * half_width + v.z * (v_sample - 0.5) * 2.0 * half_height + w.z * -1.0;
                ray_dir = vec3_normalize(ray_dir);

                r.origin = camera_pos;
                r.direction = ray_dir;
                color = ray_color(r, MAX_DEPTH);

                // Accumulate color
                pixel_color = vec3_add(pixel_color, color);
            }

            // Average the samples
            pixel_color = vec3_div(pixel_color, (double)SAMPLES_PER_PIXEL);

            // Apply gamma correction
            pixel_color = gamma22(pixel_color);

            // Convert to 8-bit and store
            int idx = (y * width + x) * 3;
            pixels[idx + 0] = (unsigned char)(fmax(0.0, fmin(255.0, pixel_color.x * 255.0)));  // Red
            pixels[idx + 1] = (unsigned char)(fmax(0.0, fmin(255.0, pixel_color.y * 255.0)));  // Green
            pixels[idx + 2] = (unsigned char)(fmax(0.0, fmin(255.0, pixel_color.z * 255.0)));  // Blue
        }
    }

    // Write TGA file
    write_tga("out.tga", pixels, width, height);

    // Clean up
    free(pixels);

    printf("Raytracer completed successfully.\n");
    printf("Generated image: out.tga\n");

    return 0;
}
