#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Vector operations
typedef struct {
    double x, y, z;
} vec3;

vec3 vec3_add(vec3 a, vec3 b) {
    vec3 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

vec3 vec3_sub(vec3 a, vec3 b) {
    vec3 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

vec3 vec3_mul(vec3 a, double t) {
    vec3 result;
    result.x = a.x * t;
    result.y = a.y * t;
    result.z = a.z * t;
    return result;
}

vec3 vec3_div(vec3 a, double t) {
    vec3 result;
    result.x = a.x / t;
    result.y = a.y / t;
    result.z = a.z / t;
    return result;
}

double vec3_dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

vec3 vec3_cross(vec3 a, vec3 b) {
    vec3 result;
    result.x = a.y * b.z - a.z * b.y;
    result.y = a.z * b.x - a.x * b.z;
    result.z = a.x * b.y - a.y * b.x;
    return result;
}

double vec3_length(vec3 v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

double vec3_length_squared(vec3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

vec3 vec3_normalize(vec3 v) {
    double len = vec3_length(v);
    vec3 result;
    if (len > 1e-10) {
        result.x = v.x / len;
        result.y = v.y / len;
        result.z = v.z / len;
    } else {
        result.x = 0;
        result.y = 0;
        result.z = 0;
    }
    return result;
}

vec3 vec3_negate(vec3 v) {
    vec3 result;
    result.x = -v.x;
    result.y = -v.y;
    result.z = -v.z;
    return result;
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

// Scene structure
typedef struct {
    sphere *spheres;
    int num_spheres;
    plane *planes;
    int num_planes;
    vec3 lights[3];
    double light_strengths[3];
} scene;

// Constants
#define WIDTH 1280
#define HEIGHT 720
#define MAX_DEPTH 8
#define SAMPLES_PER_PIXEL 4
#define PI 3.14159265358979323846
#define EPSILON 1e-4
#define FOG_DENSITY 0.035

// Sky color function
vec3 sky_color(vec3 ray_dir) {
    // Declare all variables at the beginning
    double y, t;
    vec3 dark_red, orange, result;
    
    // Clamp ray direction y component
    y = ray_dir.y;
    if (y > 1.0) y = 1.0;
    if (y < -1.0) y = -1.0;
    
    // Calculate t using the specified formula
    t = pow(fabs(y), 0.65);
    
    // Define colors
    dark_red.x = 0.18;
    dark_red.y = 0.01;
    dark_red.z = 0.015;
    
    orange.x = 1.0;
    orange.y = 0.25;
    orange.z = 0.03;
    
    // Interpolate between colors
    result.x = (1.0 - t) * dark_red.x + t * orange.x;
    result.y = (1.0 - t) * dark_red.y + t * orange.y;
    result.z = (1.0 - t) * dark_red.z + t * orange.z;
    return result;
}

// Checkerboard pattern
vec3 checkerboard_color(vec3 point) {
    // Declare all variables at the beginning
    int x, z;
    vec3 result;
    
    // Compute checker pattern from world-space X and Z coordinates
    x = (int)floor(point.x);
    z = (int)floor(point.z);
    
    // Alternate black and white squares
    if ((x + z) % 2 == 0) {
        result.x = 1.0; // White
        result.y = 1.0;
        result.z = 1.0;
    } else {
        result.x = 0.0; // Black
        result.y = 0.0;
        result.z = 0.0;
    }
    return result;
}

// Ray-sphere intersection
int intersect_sphere(ray r, sphere s, double *t) {
    // Declare all variables at the beginning
    vec3 oc;
    double a, b, c, discriminant, sqrt_discriminant, t1, t2;
    
    oc.x = r.origin.x - s.center.x;
    oc.y = r.origin.y - s.center.y;
    oc.z = r.origin.z - s.center.z;
    
    a = vec3_dot(r.direction, r.direction);
    b = 2.0 * vec3_dot(oc, r.direction);
    c = vec3_dot(oc, oc) - s.radius * s.radius;
    discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) {
        return 0; // No intersection
    }
    
    // Find the nearest positive root
    sqrt_discriminant = sqrt(discriminant);
    t1 = (-b - sqrt_discriminant) / (2.0 * a);
    t2 = (-b + sqrt_discriminant) / (2.0 * a);
    
    if (t1 > EPSILON) {
        *t = t1;
        return 1;
    } else if (t2 > EPSILON) {
        *t = t2;
        return 1;
    }
    
    return 0; // No valid intersection
}

// Ray-plane intersection
int intersect_plane(ray r, plane p, double *t) {
    // Declare all variables at the beginning
    double denominator;
    vec3 op;
    
    denominator = vec3_dot(p.normal, r.direction);
    
    if (fabs(denominator) < EPSILON) {
        return 0; // Ray is parallel to plane
    }
    
    op.x = p.point.x - r.origin.x;
    op.y = p.point.y - r.origin.y;
    op.z = p.point.z - r.origin.z;
    *t = vec3_dot(op, p.normal) / denominator;
    
    if (*t > EPSILON) {
        return 1;
    }
    
    return 0; // Intersection behind ray origin
}

// Fresnel reflection coefficient (Schlick's approximation)
double fresnel(double cos_theta, double ref_idx) {
    // Declare all variables at the beginning
    double r0;
    
    r0 = (1.0 - ref_idx) / (1.0 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow((1.0 - cos_theta), 5.0);
}

// Reflect vector
vec3 reflect(vec3 incident, vec3 normal) {
    // Declare all variables at the beginning
    double dot;
    vec3 result;
    
    dot = vec3_dot(incident, normal);
    result.x = incident.x - 2.0 * dot * normal.x;
    result.y = incident.y - 2.0 * dot * normal.y;
    result.z = incident.z - 2.0 * dot * normal.z;
    return result;
}

// Refract vector
int refract(vec3 incident, vec3 normal, double n_ratio, vec3 *refracted) {
    // Declare all variables at the beginning
    double cos_i, k, sqrt_k, n_cos_i;
    
    cos_i = vec3_dot(vec3_negate(incident), normal);
    k = 1.0 - n_ratio * n_ratio * (1.0 - cos_i * cos_i);
    
    if (k < 0.0) {
        return 0; // Total internal reflection
    }
    
    sqrt_k = sqrt(k);
    n_cos_i = n_ratio * cos_i;
    refracted->x = n_ratio * incident.x - (n_cos_i + sqrt_k) * normal.x;
    refracted->y = n_ratio * incident.y - (n_cos_i + sqrt_k) * normal.y;
    refracted->z = n_ratio * incident.z - (n_cos_i + sqrt_k) * normal.z;
    return 1;
}

// Lambertian material (diffuse)
vec3 lambertian_color(material mat, vec3 point, vec3 normal) {
    return mat.albedo;
}

// Metal material (reflective)
vec3 metal_color(material mat, vec3 point, vec3 normal, vec3 view_dir) {
    // Declare all variables at the beginning
    vec3 reflected, scattered;
    
    reflected = reflect(view_dir, normal);
    // Add fuzz (scatter) to make it less perfect mirror
    scattered.x = reflected.x + mat.fuzz * (rand() / (double)RAND_MAX);
    scattered.y = reflected.y + mat.fuzz * (rand() / (double)RAND_MAX);
    scattered.z = reflected.z + mat.fuzz * (rand() / (double)RAND_MAX);
    return vec3_normalize(scattered);
}

// Dielectric material (glass)
vec3 dielectric_color(material mat, vec3 point, vec3 normal, vec3 view_dir, double cos_theta) {
    // Declare all variables at the beginning
    double reflect_prob;
    vec3 result;
    
    // For glass, we return a mix of reflection and refraction
    // We'll compute reflection and refraction weights and mix them
    reflect_prob = fresnel(cos_theta, mat.ref_idx);
    result.x = reflect_prob;
    result.y = reflect_prob;
    result.z = reflect_prob;
    return result;
}

// Compute lighting for a surface point
vec3 compute_lighting(scene *s, vec3 point, vec3 normal, vec3 view_dir, material mat, double distance) {
    // Declare all variables at the beginning
    vec3 color, ambient, light_contribution, spec_contribution, temp, temp2, ambient_contribution;
    vec3 light_dir;
    double light_distance, diffuse, spec;
    int i, j, in_shadow;
    
    color.x = 0.0;
    color.y = 0.0;
    color.z = 0.0;
    
    ambient.x = 0.1;
    ambient.y = 0.1;
    ambient.z = 0.1; // Small ambient component

    // For each light
    for (i = 0; i < 3; i++) {
        light_dir.x = s->lights[i].x - point.x;
        light_dir.y = s->lights[i].y - point.y;
        light_dir.z = s->lights[i].z - point.z;
        light_dir = vec3_normalize(light_dir);
        
        light_distance = vec3_length(vec3_sub(s->lights[i], point));
        
        // Shadow ray test
        ray shadow_ray;
        shadow_ray.origin = point;
        shadow_ray.direction = light_dir;
        double shadow_t = 0.0;
        in_shadow = 0;
        
        // Test intersection with all spheres
        for (j = 0; j < s->num_spheres; j++) {
            if (intersect_sphere(shadow_ray, s->spheres[j], &shadow_t)) {
                if (shadow_t < light_distance - EPSILON) {
                    in_shadow = 1;
                    break;
                }
            }
        }
        
        // Test intersection with plane
        if (!in_shadow) {
            plane ground;
            ground.point.x = 0;
            ground.point.y = 0;
            ground.point.z = 0;
            ground.normal.x = 0;
            ground.normal.y = 1;
            ground.normal.z = 0;
            ground.mat.type = MATERIAL_LAMBERTIAN;
            ground.mat.albedo.x = 0.5;
            ground.mat.albedo.y = 0.5;
            ground.mat.albedo.z = 0.5;
            if (intersect_plane(shadow_ray, ground, &shadow_t)) {
                if (shadow_t < light_distance - EPSILON) {
                    in_shadow = 1;
                }
            }
        }
        
        if (!in_shadow) {
            // Diffuse lighting
            diffuse = fmax(0.0, vec3_dot(normal, light_dir));
            
            // Specular lighting (Phong)
            vec3 reflect_dir;
            reflect_dir = reflect(vec3_negate(light_dir), normal);
            spec = pow(fmax(0.0, vec3_dot(view_dir, reflect_dir)), 32);
            
            // Combine lighting with light strength
            light_contribution.x = 0.5 * diffuse;
            light_contribution.y = 0.5 * diffuse;
            light_contribution.z = 0.5 * diffuse;
            
            spec_contribution.x = 0.5 * spec;
            spec_contribution.y = 0.5 * spec;
            spec_contribution.z = 0.5 * spec;
            
            temp.x = light_contribution.x + spec_contribution.x;
            temp.y = light_contribution.y + spec_contribution.y;
            temp.z = light_contribution.z + spec_contribution.z;
            temp = vec3_mul(temp, s->light_strengths[i]);
            
            // Apply to material color
            vec3 mat_color;
            switch (mat.type) {
                case MATERIAL_LAMBERTIAN:
                    mat_color = lambertian_color(mat, point, normal);
                    break;
                case MATERIAL_METAL:
                    mat_color = metal_color(mat, point, normal, view_dir);
                    break;
                case MATERIAL_DIELECTRIC:
                    mat_color = dielectric_color(mat, point, normal, view_dir, vec3_dot(normal, light_dir));
                    break;
                default:
                    mat_color.x = 1.0;
                    mat_color.y = 1.0;
                    mat_color.z = 1.0;
            }
            
            temp2.x = temp.x * mat_color.x;
            temp2.y = temp.y * mat_color.y;
            temp2.z = temp.z * mat_color.z;
            color.x = color.x + temp2.x;
            color.y = color.y + temp2.y;
            color.z = color.z + temp2.z;
        }
    }
    
    // Add ambient lighting
    ambient_contribution.x = ambient.x * lambertian_color(mat, point, normal).x;
    ambient_contribution.y = ambient.y * lambertian_color(mat, point, normal).y;
    ambient_contribution.z = ambient.z * lambertian_color(mat, point, normal).z;
    color.x = color.x + ambient_contribution.x;
    color.y = color.y + ambient_contribution.y;
    color.z = color.z + ambient_contribution.z;
    
    return color;
}

// Trace a ray through the scene
vec3 trace_ray(ray r, scene *s, int depth) {
    // Declare all variables at the beginning
    double closest_t, t, cos_theta, transmittance;
    int hit_sphere, hit_plane;
    vec3 hit_point, hit_normal, ray_origin, view_dir, base_color, final_color, reflected, refracted, fog_color, reflected_color, refracted_color;
    material hit_mat;
    int i;
    
    if (depth <= 0) {
        return sky_color(r.direction);
    }
    
    closest_t = 1e20;
    hit_sphere = -1;
    hit_plane = -1;
    
    // Test intersection with all spheres
    for (i = 0; i < s->num_spheres; i++) {
        if (intersect_sphere(r, s->spheres[i], &t) && t < closest_t) {
            closest_t = t;
            hit_sphere = i;
            hit_point.x = r.origin.x + r.direction.x * t;
            hit_point.y = r.origin.y + r.direction.y * t;
            hit_point.z = r.origin.z + r.direction.z * t;
            hit_normal.x = hit_point.x - s->spheres[i].center.x;
            hit_normal.y = hit_point.y - s->spheres[i].center.y;
            hit_normal.z = hit_point.z - s->spheres[i].center.z;
            hit_normal = vec3_normalize(hit_normal);
            hit_mat = s->spheres[i].mat;
        }
    }
    
    // Test intersection with ground plane
    plane ground;
    ground.point.x = 0;
    ground.point.y = 0;
    ground.point.z = 0;
    ground.normal.x = 0;
    ground.normal.y = 1;
    ground.normal.z = 0;
    ground.mat.type = MATERIAL_LAMBERTIAN;
    ground.mat.albedo.x = 0.5;
    ground.mat.albedo.y = 0.5;
    ground.mat.albedo.z = 0.5;
    if (intersect_plane(r, ground, &t) && t < closest_t) {
        closest_t = t;
        hit_plane = 0;
        hit_point.x = r.origin.x + r.direction.x * t;
        hit_point.y = r.origin.y + r.direction.y * t;
        hit_point.z = r.origin.z + r.direction.z * t;
        hit_normal.x = 0;
        hit_normal.y = 1;
        hit_normal.z = 0;
        hit_mat = ground.mat;
    }
    
    // If we hit something
    if (hit_sphere >= 0 || hit_plane >= 0) {
        // Offset ray origin to avoid self-intersection
        ray_origin.x = hit_point.x + hit_normal.x * EPSILON;
        ray_origin.y = hit_point.y + hit_normal.y * EPSILON;
        ray_origin.z = hit_point.z + hit_normal.z * EPSILON;
        
        // Compute view direction
        view_dir = vec3_negate(r.direction);
        
        // Get distance for fog calculation
        double distance = closest_t;
        
        // Compute lighting
        base_color = compute_lighting(s, hit_point, hit_normal, view_dir, hit_mat, distance);
        
        // Handle different material types
        switch (hit_mat.type) {
            case MATERIAL_LAMBERTIAN:
                // Diffuse material
                final_color = base_color;
                break;
                
            case MATERIAL_METAL:
                // Metal material - reflect
                reflected = reflect(r.direction, hit_normal);
                ray reflected_ray;
                reflected_ray.origin = ray_origin;
                reflected_ray.direction = reflected;
                reflected_color = trace_ray(reflected_ray, s, depth - 1);
                
                // Mix reflection with base color
                final_color.x = 0.1 * base_color.x + 0.9 * reflected_color.x;
                final_color.y = 0.1 * base_color.y + 0.9 * reflected_color.y;
                final_color.z = 0.1 * base_color.z + 0.9 * reflected_color.z;
                break;
                
            case MATERIAL_DIELECTRIC:
                // Dielectric material (glass)
                cos_theta = fmin(1.0, vec3_dot(vec3_negate(r.direction), hit_normal));
                double reflect_prob = fresnel(cos_theta, hit_mat.ref_idx);
                
                // Decide whether to reflect or refract
                if (rand() / (double)RAND_MAX < reflect_prob) {
                    // Reflect
                    reflected = reflect(r.direction, hit_normal);
                    ray reflected_ray;
                    reflected_ray.origin = ray_origin;
                    reflected_ray.direction = reflected;
                    reflected_color = trace_ray(reflected_ray, s, depth - 1);
                    final_color = reflected_color;
                } else {
                    // Refract
                    if (refract(r.direction, hit_normal, 1.0 / hit_mat.ref_idx, &refracted)) {
                        // Create refracted ray with proper offset
                        vec3 refracted_ray_origin;
                        refracted_ray_origin.x = hit_point.x - hit_normal.x * EPSILON;
                        refracted_ray_origin.y = hit_point.y - hit_normal.y * EPSILON;
                        refracted_ray_origin.z = hit_point.z - hit_normal.z * EPSILON;
                        ray refracted_ray;
                        refracted_ray.origin = refracted_ray_origin;
                        refracted_ray.direction = refracted;
                        refracted_color = trace_ray(refracted_ray, s, depth - 1);
                        final_color = refracted_color;
                    } else {
                        // Total internal reflection - reflect
                        reflected = reflect(r.direction, hit_normal);
                        ray reflected_ray;
                        reflected_ray.origin = ray_origin;
                        reflected_ray.direction = reflected;
                        reflected_color = trace_ray(reflected_ray, s, depth - 1);
                        final_color = reflected_color;
                    }
                }
                break;
                
            default:
                final_color = base_color;
        }
        
        // Apply fog effect
        transmittance = exp(-FOG_DENSITY * distance);
        fog_color = sky_color(r.direction);
        final_color.x = transmittance * final_color.x + (1.0 - transmittance) * fog_color.x;
        final_color.y = transmittance * final_color.y + (1.0 - transmittance) * fog_color.y;
        final_color.z = transmittance * final_color.z + (1.0 - transmittance) * fog_color.z;
        
        return final_color;
    } else {
        // No hit - return sky color
        return sky_color(r.direction);
    }
}

// Convert RGB color to 8-bit
unsigned char clamp_color(double c) {
    if (c < 0.0) return 0;
    if (c > 1.0) return 255;
    return (unsigned char)(c * 255.0);
}

// Save TGA file
int save_tga(const char *filename, unsigned char *pixels, int width, int height) {
    // Declare all variables at the beginning
    FILE *fp;
    unsigned char header[18];
    int y, x;
    
    fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Could not open file %s for writing\n", filename);
        return 0;
    }

    // TGA Header (18 bytes)
    header[0] = 0;
    header[1] = 0;
    header[2] = 2;
    header[3] = 0;
    header[4] = 0;
    header[5] = 0;
    header[6] = 0;
    header[7] = 0;
    header[8] = 0;
    header[9] = 0;
    header[10] = 0;
    header[11] = 0;
    header[12] = width & 0xFF;
    header[13] = (width >> 8) & 0xFF;
    header[14] = height & 0xFF;
    header[15] = (height >> 8) & 0xFF;
    header[16] = 24;
    header[17] = 0x20;

    if (fwrite(header, 1, 18, fp) != 18) {
        fclose(fp);
        fprintf(stderr, "Error: Could not write TGA header\n");
        return 0;
    }

    // Write pixel data (BGR order, bottom to top)
    for (y = height - 1; y >= 0; y--) {
        for (x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            unsigned char b = pixels[idx];
            unsigned char g = pixels[idx + 1];
            unsigned char r = pixels[idx + 2];
            
            if (fwrite(&b, 1, 1, fp) != 1 ||
                fwrite(&g, 1, 1, fp) != 1 ||
                fwrite(&r, 1, 1, fp) != 1) {
                fclose(fp);
                fprintf(stderr, "Error: Could not write pixel data\n");
                return 0;
            }
        }
    }

    fclose(fp);
    return 1;
}

int main(void) {
    // Declare all variables at the beginning
    scene s;
    unsigned char *pixels;
    vec3 camera_pos, camera_target, camera_up, forward, right, up;
    double fov, aspect_ratio, half_height, half_width;
    int y, x, sy, sx;
    vec3 pixel_color, ray_dir, color;
    ray r;
    double u, v, scaled_u, scaled_v;
    
    // Initialize random seed for consistent results
    srand(42);

    // Create scene with two spheres
    s.num_spheres = 2;
    s.spheres = malloc(sizeof(sphere) * s.num_spheres);
    
    // Glass sphere on the left
    s.spheres[0].center.x = -1.25;
    s.spheres[0].center.y = 1.0;
    s.spheres[0].center.z = 0.0;
    s.spheres[0].radius = 1.0;
    s.spheres[0].mat.type = MATERIAL_DIELECTRIC;
    s.spheres[0].mat.albedo.x = 1.0;
    s.spheres[0].mat.albedo.y = 1.0;
    s.spheres[0].mat.albedo.z = 1.0;
    s.spheres[0].mat.ref_idx = 1.5;
    
    // Chrome sphere on the right
    s.spheres[1].center.x = 1.25;
    s.spheres[1].center.y = 1.0;
    s.spheres[1].center.z = 0.0;
    s.spheres[1].radius = 1.0;
    s.spheres[1].mat.type = MATERIAL_METAL;
    s.spheres[1].mat.albedo.x = 0.8;
    s.spheres[1].mat.albedo.y = 0.8;
    s.spheres[1].mat.albedo.z = 0.8;
    s.spheres[1].mat.fuzz = 0.05;
    
    // Lights
    s.lights[0].x = -3.5;
    s.lights[0].y = 6.0;
    s.lights[0].z = 4.5;      // Key light
    s.lights[1].x = 4.0;
    s.lights[1].y = 3.0;
    s.lights[1].z = 3.0;       // Fill light
    s.lights[2].x = 0.5;
    s.lights[2].y = 5.0;
    s.lights[2].z = -4.0;      // Back light
    
    s.light_strengths[0] = 1.0;   // Key light strength
    s.light_strengths[1] = 0.35;  // Fill light strength
    s.light_strengths[2] = 0.7;   // Back light strength
    
    // Allocate pixel buffer
    pixels = malloc(WIDTH * HEIGHT * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Could not allocate pixel buffer\n");
        return 1;
    }

    // Camera setup
    camera_pos.x = 0.0;
    camera_pos.y = 2.6;
    camera_pos.z = 6.5;
    
    camera_target.x = 0.0;
    camera_target.y = 1.0;
    camera_target.z = 0.0;
    
    camera_up.x = 0.0;
    camera_up.y = 1.0;
    camera_up.z = 0.0;
    
    // Compute camera coordinate system
    forward.x = camera_target.x - camera_pos.x;
    forward.y = camera_target.y - camera_pos.y;
    forward.z = camera_target.z - camera_pos.z;
    forward = vec3_normalize(forward);
    
    right = vec3_cross(forward, camera_up);
    right = vec3_normalize(right);
    
    up = vec3_cross(right, forward);
    
    // Compute field of view
    fov = 45.0 * PI / 180.0;
    aspect_ratio = (double)WIDTH / (double)HEIGHT;
    half_height = tan(fov / 2.0);
    half_width = half_height * aspect_ratio;
    
    // Render image
    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            pixel_color.x = 0.0;
            pixel_color.y = 0.0;
            pixel_color.z = 0.0;
            
            // Multi-sampling
            for (sy = 0; sy < 2; sy++) {
                for (sx = 0; sx < 2; sx++) {
                    // Compute subpixel position
                    u = (x + (sx + 0.5) / 2.0) / WIDTH;
                    v = (y + (sy + 0.5) / 2.0) / HEIGHT;
                    
                    // Scale and offset for perspective projection
                    scaled_u = (2.0 * u - 1.0) * half_width;
                    scaled_v = (1.0 - 2.0 * v) * half_height;
                    
                    // Compute ray direction
                    ray_dir.x = scaled_u * right.x + scaled_v * up.x + forward.x;
                    ray_dir.y = scaled_u * right.y + scaled_v * up.y + forward.y;
                    ray_dir.z = scaled_u * right.z + scaled_v * up.z + forward.z;
                    ray_dir = vec3_normalize(ray_dir);
                    
                    // Create ray
                    r.origin = camera_pos;
                    r.direction = ray_dir;
                    
                    // Trace ray
                    color = trace_ray(r, &s, MAX_DEPTH);
                    
                    // Gamma correction
                    color.x = pow(color.x, 1.0/2.2);
                    color.y = pow(color.y, 1.0/2.2);
                    color.z = pow(color.z, 1.0/2.2);
                    
                    // Accumulate color
                    pixel_color.x = pixel_color.x + color.x;
                    pixel_color.y = pixel_color.y + color.y;
                    pixel_color.z = pixel_color.z + color.z;
                }
            }
            
            // Average the samples
            pixel_color.x = pixel_color.x / 4.0;
            pixel_color.y = pixel_color.y / 4.0;
            pixel_color.z = pixel_color.z / 4.0;
            
            // Convert to 8-bit and store
            int idx = (y * WIDTH + x) * 3;
            pixels[idx] = clamp_color(pixel_color.z);     // Blue
            pixels[idx + 1] = clamp_color(pixel_color.y); // Green  
            pixels[idx + 2] = clamp_color(pixel_color.x); // Red
        }
    }

    // Save the image
    if (!save_tga("out.tga", pixels, WIDTH, HEIGHT)) {
        free(pixels);
        free(s.spheres);
        return 1;
    }

    // Cleanup
    free(pixels);
    free(s.spheres);

    printf("Raytracer completed successfully!\n");
    printf("Image saved as out.tga\n");
    return 0;
}
