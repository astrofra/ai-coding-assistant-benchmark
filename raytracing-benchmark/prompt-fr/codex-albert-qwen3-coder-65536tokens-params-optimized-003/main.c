#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Vector operations
typedef struct {
    double x, y, z;
} vec3;

static vec3 vec3_add(vec3 a, vec3 b) {
    vec3 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

static vec3 vec3_sub(vec3 a, vec3 b) {
    vec3 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

static vec3 vec3_mul(vec3 a, double s) {
    vec3 result;
    result.x = a.x * s;
    result.y = a.y * s;
    result.z = a.z * s;
    return result;
}

static vec3 vec3_div(vec3 a, double s) {
    vec3 result;
    result.x = a.x / s;
    result.y = a.y / s;
    result.z = a.z / s;
    return result;
}

static double vec3_dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static vec3 vec3_cross(vec3 a, vec3 b) {
    vec3 result;
    result.x = a.y * b.z - a.z * b.y;
    result.y = a.z * b.x - a.x * b.z;
    result.z = a.x * b.y - a.y * b.x;
    return result;
}

static double vec3_length(vec3 v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static double vec3_length_squared(vec3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static vec3 vec3_normalize(vec3 v) {
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

// Ray structure
typedef struct {
    vec3 origin;
    vec3 direction;
} ray;

// Material types
typedef enum {
    MATERIAL_LAMBERTIAN,
    MATERIAL_METAL,
    MATERIAL_DIELECTRIC
} material_type;

// Material structure
typedef struct {
    material_type type;
    vec3 albedo;
    double fuzz;
    double ir; // Index of refraction
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

// Light structure
typedef struct {
    vec3 position;
    vec3 intensity;
} light;

// Scene structure
typedef struct {
    sphere spheres[2];
    plane ground;
    light lights[3];
    vec3 camera_pos;
    vec3 camera_target;
    vec3 camera_up;
    double fov;
    int width;
    int height;
} scene;

// Global scene
scene g_scene;

// Sky color calculation
static vec3 sky_color(vec3 ray_dir) {
    vec3 dark_red;
    vec3 orange;
    vec3 result;
    double t;
    
    dark_red.x = 0.18;
    dark_red.y = 0.01;
    dark_red.z = 0.015;
    orange.x = 1.0;
    orange.y = 0.25;
    orange.z = 0.03;
    
    t = pow(fabs(ray_dir.y), 0.65);
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    
    result.x = (1.0 - t) * orange.x + t * dark_red.x;
    result.y = (1.0 - t) * orange.y + t * dark_red.y;
    result.z = (1.0 - t) * orange.z + t * dark_red.z;
    
    return result;
}

// Checkerboard pattern
static vec3 checkerboard_color(vec3 point) {
    int x, z;
    vec3 result;
    
    x = (int)floor(point.x);
    z = (int)floor(point.z);
    if ((x + z) % 2 == 0) {
        result.x = 1.0;
        result.y = 1.0;
        result.z = 1.0; // White
    } else {
        result.x = 0.0;
        result.y = 0.0;
        result.z = 0.0; // Black
    }
    return result;
}

// Ray-sphere intersection
static int ray_sphere_intersect(ray r, sphere s, double *t) {
    vec3 oc;
    double a, b, c, discriminant, sqrt_discriminant;
    
    oc = vec3_sub(r.origin, s.center);
    a = vec3_dot(r.direction, r.direction);
    b = 2.0 * vec3_dot(oc, r.direction);
    c = vec3_dot(oc, oc) - s.radius * s.radius;
    discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) {
        return 0;
    }
    
    sqrt_discriminant = sqrt(discriminant);
    *t = (-b - sqrt_discriminant) / (2.0 * a);
    if (*t > 1e-4) {
        return 1;
    }
    
    *t = (-b + sqrt_discriminant) / (2.0 * a);
    if (*t > 1e-4) {
        return 1;
    }
    
    return 0;
}

// Ray-plane intersection
static int ray_plane_intersect(ray r, plane p, double *t) {
    double denom;
    vec3 op;
    
    denom = vec3_dot(p.normal, r.direction);
    if (fabs(denom) > 1e-6) {
        op = vec3_sub(p.point, r.origin);
        *t = vec3_dot(op, p.normal) / denom;
        if (*t > 1e-4) {
            return 1;
        }
    }
    return 0;
}

// Fresnel reflectance for dielectrics
static double fresnel_dielectric(double cos_theta, double ri) {
    // Schlick's approximation
    double r0;
    r0 = (1.0 - ri) / (1.0 + ri);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow((1.0 - cos_theta), 5.0);
}

// Reflect vector
static vec3 reflect(vec3 incident, vec3 normal) {
    vec3 result;
    double temp;
    
    temp = 2.0 * vec3_dot(incident, normal);
    result = vec3_sub(incident, vec3_mul(normal, temp));
    return result;
}

// Refract vector
static int refract(vec3 incident, vec3 normal, double ni_over_nt, vec3 *refracted) {
    vec3 uv;
    double dt, discriminant;
    
    uv = vec3_normalize(incident);
    dt = vec3_dot(uv, normal);
    discriminant = 1.0 - ni_over_nt * ni_over_nt * (1.0 - dt * dt);
    
    if (discriminant > 0) {
        refracted->x = uv.x * ni_over_nt - normal.x * (ni_over_nt * dt + sqrt(discriminant));
        refracted->y = uv.y * ni_over_nt - normal.y * (ni_over_nt * dt + sqrt(discriminant));
        refracted->z = uv.z * ni_over_nt - normal.z * (ni_over_nt * dt + sqrt(discriminant));
        return 1;
    }
    
    return 0;
}

// Sample a random direction on a hemisphere
static vec3 sample_hemisphere(vec3 normal) {
    // Generate uniform random numbers
    double r1, r2;
    double theta, phi;
    double sin_theta, x, y, z;
    vec3 tangent, bitangent, result;
    
    r1 = rand() / (double)RAND_MAX;
    r2 = rand() / (double)RAND_MAX;
    
    // Cosine-weighted hemisphere sampling
    theta = acos(sqrt(r1));
    phi = 2.0 * M_PI * r2;
    
    // Convert to Cartesian coordinates
    sin_theta = sin(theta);
    x = sin_theta * cos(phi);
    y = cos(theta);
    z = sin_theta * sin(phi);
    
    // Transform to world space
    tangent.x = 0;
    tangent.y = 1;
    tangent.z = 0;
    tangent = vec3_cross(normal, tangent);
    if (vec3_length_squared(tangent) < 1e-6) {
        tangent.x = 1;
        tangent.y = 0;
        tangent.z = 0;
    }
    tangent = vec3_normalize(tangent);
    bitangent = vec3_cross(normal, tangent);
    
    result.x = tangent.x * x + bitangent.x * z + normal.x * y;
    result.y = tangent.y * x + bitangent.y * z + normal.y * y;
    result.z = tangent.z * x + bitangent.z * z + normal.z * y;
    
    return result;
}

// Trace a ray through the scene
static vec3 trace_ray(ray r, int depth) {
    double closest_t, t;
    int hit_sphere;
    double plane_t;
    vec3 hit_point, hit_normal, hit_mat_albedo;
    material_type hit_mat_type;
    double hit_mat_ir;
    vec3 color, temp_vec3, temp_vec3_2;
    double temp_double, temp_double_2;
    int i, j, in_shadow;
    double shadow_t, light_distance;
    vec3 light_dir, view_dir, reflect_dir, shadow_ray_origin, shadow_ray_direction;
    vec3 reflected, refracted, reflected_color, refracted_color;
    double ni_over_nt, cos_theta, reflect_prob, transmittance;
    vec3 fog_color;
    ray reflected_ray, refracted_ray;
    
    if (depth <= 0) {
        return sky_color(r.direction);
    }
    
    closest_t = 1e20;
    hit_sphere = -1;
    plane_t = 0;
    
    // Check sphere intersections
    for (i = 0; i < 2; i++) {
        if (ray_sphere_intersect(r, g_scene.spheres[i], &t) && t < closest_t) {
            closest_t = t;
            hit_sphere = i;
            hit_point = vec3_add(r.origin, vec3_mul(r.direction, t));
            hit_normal = vec3_normalize(vec3_sub(hit_point, g_scene.spheres[i].center));
            hit_mat_type = g_scene.spheres[i].mat.type;
            hit_mat_albedo = g_scene.spheres[i].mat.albedo;
            hit_mat_ir = g_scene.spheres[i].mat.ir;
        }
    }
    
    // Check plane intersection
    if (ray_plane_intersect(r, g_scene.ground, &plane_t)) {
        if (plane_t < closest_t) {
            closest_t = plane_t;
            hit_point = vec3_add(r.origin, vec3_mul(r.direction, plane_t));
            hit_normal = g_scene.ground.normal;
            hit_mat_type = g_scene.ground.mat.type;
            hit_mat_albedo = g_scene.ground.mat.albedo;
            hit_mat_ir = g_scene.ground.mat.ir;
        }
    }
    
    // No hit - return sky color
    if (closest_t == 1e20) {
        return sky_color(r.direction);
    }
    
    // Calculate lighting
    color.x = 0;
    color.y = 0;
    color.z = 0;
    
    // Get the material at the hit point
    if (hit_sphere >= 0) {
        // For spheres, we might need to adjust normal depending on ray direction
        if (vec3_dot(r.direction, hit_normal) > 0) {
            hit_normal = vec3_mul(hit_normal, -1.0);
        }
    }
    
    // Add ambient lighting
    temp_vec3 = vec3_mul(hit_mat_albedo, 0.1);
    color = vec3_add(color, temp_vec3);
    
    // Calculate lighting from each light
    for (i = 0; i < 3; i++) {
        light_dir = vec3_normalize(vec3_sub(g_scene.lights[i].position, hit_point));
        light_distance = vec3_length(vec3_sub(g_scene.lights[i].position, hit_point));
        
        // Shadow ray test
        shadow_ray_origin = vec3_add(hit_point, vec3_mul(hit_normal, 1e-4)); // Offset to avoid self-intersection
        shadow_ray_direction = light_dir;
        
        // Check if there's an intersection with any object
        in_shadow = 0;
        shadow_t = 1e20;
        
        // Check sphere intersections
        for (j = 0; j < 2; j++) {
            // Create ray manually to avoid compound literals
            ray shadow_ray;
            shadow_ray.origin = shadow_ray_origin;
            shadow_ray.direction = shadow_ray_direction;
            if (ray_sphere_intersect(shadow_ray, g_scene.spheres[j], &t) && t < shadow_t) {
                shadow_t = t;
                if (t < light_distance) {
                    in_shadow = 1;
                }
            }
        }
        
        // Check plane intersection
        if (!in_shadow) {
            // Create ray manually to avoid compound literals
            ray shadow_ray;
            shadow_ray.origin = shadow_ray_origin;
            shadow_ray.direction = shadow_ray_direction;
            if (ray_plane_intersect(shadow_ray, g_scene.ground, &shadow_t) && shadow_t < light_distance) {
                in_shadow = 1;
            }
        }
        
        if (!in_shadow) {
            // Diffuse lighting (Lambert)
            temp_double = fmax(0.0, vec3_dot(hit_normal, light_dir));
            
            // Specular lighting (Phong)
            view_dir = vec3_normalize(vec3_mul(r.direction, -1.0));
            reflect_dir = reflect(vec3_mul(light_dir, -1.0), hit_normal);
            temp_double_2 = pow(fmax(0.0, vec3_dot(view_dir, reflect_dir)), 32.0);
            
            if (hit_mat_type == MATERIAL_METAL) {
                // Metal has no diffuse component
                temp_vec3 = vec3_mul(g_scene.lights[i].intensity, temp_double_2 * 0.8);
                color = vec3_add(color, temp_vec3);
            } else if (hit_mat_type == MATERIAL_DIELECTRIC) {
                // Dielectric material (glass) has no diffuse component
                temp_vec3 = vec3_mul(g_scene.lights[i].intensity, temp_double_2 * 0.8);
                color = vec3_add(color, temp_vec3);
            } else {
                // Lambertian material
                temp_vec3 = vec3_mul(hit_mat_albedo, temp_double * g_scene.lights[i].intensity.x);
                color = vec3_add(color, temp_vec3);
                temp_vec3 = vec3_mul(g_scene.lights[i].intensity, temp_double_2 * 0.2);
                color = vec3_add(color, temp_vec3);
            }
        }
    }
    
    // Handle material-specific behavior
    if (hit_mat_type == MATERIAL_METAL) {
        // Metal reflection
        reflected = reflect(r.direction, hit_normal);
        temp_vec3 = vec3_add(hit_point, vec3_mul(hit_normal, 1e-4));
        // Create ray manually to avoid compound literals
        reflected_ray.origin = temp_vec3;
        reflected_ray.direction = reflected;
        reflected_color = trace_ray(reflected_ray, depth - 1);
        temp_vec3 = vec3_mul(color, 0.1);
        temp_vec3_2 = vec3_mul(reflected_color, 0.9);
        color = vec3_add(temp_vec3, temp_vec3_2);
    } else if (hit_mat_type == MATERIAL_DIELECTRIC) {
        // Dielectric (glass) refraction
        double cannot_refract;
        vec3 outward_normal;
        
        // Initialize variables for clarity
        ni_over_nt = hit_mat_ir;
        outward_normal = hit_normal;
        cos_theta = vec3_dot(vec3_mul(r.direction, -1.0), hit_normal);
        
        if (cos_theta > 0) {
            outward_normal = vec3_mul(outward_normal, -1.0);
            ni_over_nt = 1.0 / hit_mat_ir;
        }
        
        cannot_refract = !refract(r.direction, outward_normal, ni_over_nt, &refracted);
        if (cannot_refract) {
            reflect_prob = 1.0;
        } else {
            reflect_prob = fresnel_dielectric(cos_theta, ni_over_nt);
        }
        
        if (rand() / (double)RAND_MAX < reflect_prob) {
            // Reflection
            reflected = reflect(r.direction, hit_normal);
            temp_vec3 = vec3_add(hit_point, vec3_mul(hit_normal, 1e-4));
            // Create ray manually to avoid compound literals
            reflected_ray.origin = temp_vec3;
            reflected_ray.direction = reflected;
            reflected_color = trace_ray(reflected_ray, depth - 1);
            color = reflected_color;
        } else {
            // Refraction
            temp_vec3 = vec3_add(hit_point, vec3_mul(outward_normal, 1e-4));
            // Create ray manually to avoid compound literals
            refracted_ray.origin = temp_vec3;
            refracted_ray.direction = refracted;
            refracted_color = trace_ray(refracted_ray, depth - 1);
            color = refracted_color;
        }
    }
    
    // Apply fog effect
    transmittance = exp(-0.035 * closest_t);
    fog_color = sky_color(r.direction);
    temp_vec3 = vec3_mul(color, transmittance);
    temp_vec3_2 = vec3_mul(fog_color, 1.0 - transmittance);
    color = vec3_add(temp_vec3, temp_vec3_2);
    
    return color;
}

// Convert linear RGB to sRGB
static unsigned char linear_to_srgb(double c) {
    if (c <= 0.0031308) {
        return (unsigned char)(c * 12.92 * 255.0 + 0.5);
    } else {
        return (unsigned char)((1.055 * pow(c, 1.0 / 2.4) - 0.055) * 255.0 + 0.5);
    }
}

// Save image as TGA
static int save_tga(const char *filename, unsigned char *pixels, int width, int height) {
    FILE *fp;
    unsigned char header[18];
    int y, x, idx;
    unsigned char b, g, r;
    
    fp = fopen(filename, "wb");
    if (!fp) {
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
        return 0;
    }
    
    // Write pixel data (BGR order)
    for (y = height - 1; y >= 0; y--) {
        for (x = 0; x < width; x++) {
            idx = (y * width + x) * 3;
            b = pixels[idx];
            g = pixels[idx + 1];
            r = pixels[idx + 2];
            if (fwrite(&b, 1, 1, fp) != 1 || 
                fwrite(&g, 1, 1, fp) != 1 || 
                fwrite(&r, 1, 1, fp) != 1) {
                fclose(fp);
                return 0;
            }
        }
    }
    
    fclose(fp);
    return 1;
}

// Initialize the scene
static void init_scene(void) {
    // Camera setup
    g_scene.camera_pos.x = 0.0;
    g_scene.camera_pos.y = 2.6;
    g_scene.camera_pos.z = 6.5;
    g_scene.camera_target.x = 0.0;
    g_scene.camera_target.y = 1.0;
    g_scene.camera_target.z = 0.0;
    g_scene.camera_up.x = 0.0;
    g_scene.camera_up.y = 1.0;
    g_scene.camera_up.z = 0.0;
    g_scene.fov = 45.0;
    g_scene.width = 1280;
    g_scene.height = 720;
    
    // Create spheres
    // Glass sphere
    g_scene.spheres[0].center.x = -1.25;
    g_scene.spheres[0].center.y = 1.0;
    g_scene.spheres[0].center.z = 0.0;
    g_scene.spheres[0].radius = 1.0;
    g_scene.spheres[0].mat.type = MATERIAL_DIELECTRIC;
    g_scene.spheres[0].mat.albedo.x = 1.0;
    g_scene.spheres[0].mat.albedo.y = 1.0;
    g_scene.spheres[0].mat.albedo.z = 1.0;
    g_scene.spheres[0].mat.ir = 1.5;
    g_scene.spheres[0].mat.fuzz = 0.0;
    
    // Chrome sphere
    g_scene.spheres[1].center.x = 1.25;
    g_scene.spheres[1].center.y = 1.0;
    g_scene.spheres[1].center.z = 0.0;
    g_scene.spheres[1].radius = 1.0;
    g_scene.spheres[1].mat.type = MATERIAL_METAL;
    g_scene.spheres[1].mat.albedo.x = 0.8;
    g_scene.spheres[1].mat.albedo.y = 0.8;
    g_scene.spheres[1].mat.albedo.z = 0.8;
    g_scene.spheres[1].mat.ir = 0.0;
    g_scene.spheres[1].mat.fuzz = 0.05;
    
    // Create ground plane
    g_scene.ground.point.x = 0.0;
    g_scene.ground.point.y = 0.0;
    g_scene.ground.point.z = 0.0;
    g_scene.ground.normal.x = 0.0;
    g_scene.ground.normal.y = 1.0;
    g_scene.ground.normal.z = 0.0;
    g_scene.ground.mat.type = MATERIAL_LAMBERTIAN;
    g_scene.ground.mat.albedo.x = 0.5;
    g_scene.ground.mat.albedo.y = 0.5;
    g_scene.ground.mat.albedo.z = 0.5;
    g_scene.ground.mat.ir = 0.0;
    g_scene.ground.mat.fuzz = 0.0;
    
    // Create lights
    g_scene.lights[0].position.x = -3.5;
    g_scene.lights[0].position.y = 6.0;
    g_scene.lights[0].position.z = 4.5;
    g_scene.lights[0].intensity.x = 1.0;
    g_scene.lights[0].intensity.y = 1.0;
    g_scene.lights[0].intensity.z = 1.0;
    
    g_scene.lights[1].position.x = 4.0;
    g_scene.lights[1].position.y = 3.0;
    g_scene.lights[1].position.z = 3.0;
    g_scene.lights[1].intensity.x = 0.35;
    g_scene.lights[1].intensity.y = 0.35;
    g_scene.lights[1].intensity.z = 0.35;
    
    g_scene.lights[2].position.x = 0.5;
    g_scene.lights[2].position.y = 5.0;
    g_scene.lights[2].position.z = -4.0;
    g_scene.lights[2].intensity.x = 0.7;
    g_scene.lights[2].intensity.y = 0.7;
    g_scene.lights[2].intensity.z = 0.7;
}

// Generate the ray direction for a pixel
static ray get_ray(int x, int y, double subx, double suby) {
    vec3 forward, right, up;
    double aspect_ratio, fov_rad, half_height, half_width;
    double u, v, adjusted_u, adjusted_v;
    vec3 direction;
    ray r;
    
    // Calculate camera vectors
    forward = vec3_sub(g_scene.camera_target, g_scene.camera_pos);
    forward = vec3_normalize(forward);
    right = vec3_cross(forward, g_scene.camera_up);
    right = vec3_normalize(right);
    up = vec3_cross(right, forward);
    
    // Calculate aspect ratio and FOV
    aspect_ratio = (double)g_scene.width / (double)g_scene.height;
    fov_rad = g_scene.fov * M_PI / 180.0;
    half_height = tan(fov_rad / 2.0);
    half_width = half_height * aspect_ratio;
    
    // Calculate pixel position
    u = (x + subx) / (double)g_scene.width;
    v = (y + suby) / (double)g_scene.height;
    
    // Adjust for aspect ratio
    adjusted_u = (u - 0.5) * 2.0 * half_width;
    adjusted_v = (0.5 - v) * 2.0 * half_height;
    
    // Calculate ray direction
    direction = vec3_add(
        vec3_add(
            vec3_mul(forward, -1.0),
            vec3_mul(right, adjusted_u)
        ),
        vec3_mul(up, adjusted_v)
    );
    
    // Normalize direction
    direction = vec3_normalize(direction);
    
    r.origin = g_scene.camera_pos;
    r.direction = direction;
    
    return r;
}

int main(void) {
    // Declare variables at the beginning to comply with C89
    unsigned char *pixels;
    int y, x, sy, sx;
    double subx, suby;
    vec3 color, temp_vec3;
    ray r;
    int idx;
    
    // Initialize random seed
    srand(12345);
    
    // Initialize scene
    init_scene();
    
    // Allocate pixel buffer
    pixels = (unsigned char *)malloc(g_scene.width * g_scene.height * 3);
    if (!pixels) {
        fprintf(stderr, "Failed to allocate pixel buffer\n");
        return 1;
    }
    
    // Render image
    printf("Rendering image...\n");
    
    // 2x2 subpixel sampling
    for (y = 0; y < g_scene.height; y++) {
        for (x = 0; x < g_scene.width; x++) {
            color.x = 0;
            color.y = 0;
            color.z = 0;
            
            // 2x2 subpixel sampling
            for (sy = 0; sy < 2; sy++) {
                for (sx = 0; sx < 2; sx++) {
                    subx = (sx + 0.25) / 2.0;
                    suby = (sy + 0.25) / 2.0;
                    
                    r = get_ray(x, y, subx, suby);
                    temp_vec3 = trace_ray(r, 8);
                    
                    // Accumulate samples
                    color = vec3_add(color, temp_vec3);
                }
            }
            
            // Average samples
            color = vec3_div(color, 4.0);
            
            // Clamp and convert to 8-bit
            idx = (y * g_scene.width + x) * 3;
            pixels[idx] = linear_to_srgb(fmin(1.0, color.z));     // B
            pixels[idx + 1] = linear_to_srgb(fmin(1.0, color.y)); // G
            pixels[idx + 2] = linear_to_srgb(fmin(1.0, color.x)); // R
        }
    }
    
    // Save image
    printf("Saving image to out.tga...\n");
    if (!save_tga("out.tga", pixels, g_scene.width, g_scene.height)) {
        fprintf(stderr, "Failed to save image\n");
        free(pixels);
        return 1;
    }
    
    printf("Image saved successfully!\n");
    
    // Free memory
    free(pixels);
    
    return 0;
}
