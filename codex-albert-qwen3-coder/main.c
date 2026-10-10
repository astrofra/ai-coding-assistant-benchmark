#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Vector operations
typedef struct {
    double x, y, z;
} vec3;

static vec3 vec3_add(vec3 a, vec3 b) {
    vec3 r;
    r.x = a.x + b.x;
    r.y = a.y + b.y;
    r.z = a.z + b.z;
    return r;
}

static vec3 vec3_sub(vec3 a, vec3 b) {
    vec3 r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    return r;
}

static vec3 vec3_mul(vec3 a, double s) {
    vec3 r;
    r.x = a.x * s;
    r.y = a.y * s;
    r.z = a.z * s;
    return r;
}

static vec3 vec3_div(vec3 a, double s) {
    vec3 r;
    r.x = a.x / s;
    r.y = a.y / s;
    r.z = a.z / s;
    return r;
}

static double vec3_dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static vec3 vec3_cross(vec3 a, vec3 b) {
    vec3 r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return r;
}

static double vec3_length(vec3 a) {
    return sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}

static double vec3_length_squared(vec3 a) {
    return a.x * a.x + a.y * a.y + a.z * a.z;
}

static vec3 vec3_normalize(vec3 a) {
    double len = vec3_length(a);
    vec3 r;
    if (len > 1e-10) {
        r = vec3_div(a, len);
    } else {
        r.x = 0.0;
        r.y = 0.0;
        r.z = 0.0;
    }
    return r;
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
    double roughness;
    double ior; // Index of refraction
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
    double aspect_ratio;
} scene;

// Global scene
scene g_scene;

// Sky color calculation
static vec3 sky_color(vec3 ray_dir) {
    vec3 dark_red;
    vec3 orange;
    vec3 result;
    double t;
    double y_abs;
    
    dark_red.x = 0.18;
    dark_red.y = 0.01;
    dark_red.z = 0.015;
    orange.x = 1.0;
    orange.y = 0.25;
    orange.z = 0.03;
    
    // Clamp y to [0, 1] for numerical robustness
    y_abs = fabs(ray_dir.y);
    if (y_abs < 0.0) y_abs = 0.0;
    if (y_abs > 1.0) y_abs = 1.0;
    
    t = pow(y_abs, 0.65);
    result = vec3_add(vec3_mul(dark_red, 1.0 - t), vec3_mul(orange, t));
    return result;
}

// Checkerboard pattern
static vec3 checkerboard_color(vec3 point) {
    int x, z;
    int is_black;
    vec3 result;
    
    x = (int)floor(point.x);
    z = (int)floor(point.z);
    is_black = (x + z) % 2 == 0;
    if (is_black) {
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
static int ray_sphere_intersect(ray r, sphere s, double *t) {
    vec3 oc;
    double a, b, c;
    double discriminant;
    double sqrt_discriminant;
    double t1, t2;
    
    oc = vec3_sub(r.origin, s.center);
    a = vec3_dot(r.direction, r.direction);
    b = 2.0 * vec3_dot(oc, r.direction);
    c = vec3_dot(oc, oc) - s.radius * s.radius;
    discriminant = b * b - 4 * a * c;

    if (discriminant < 0) {
        return 0;
    }

    sqrt_discriminant = sqrt(discriminant);
    t1 = (-b - sqrt_discriminant) / (2.0 * a);
    t2 = (-b + sqrt_discriminant) / (2.0 * a);

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
static int ray_plane_intersect(ray r, plane p, double *t) {
    double denom;
    double t_val;
    
    denom = vec3_dot(p.normal, r.direction);
    if (fabs(denom) < 1e-6) {
        return 0; // Ray is parallel to plane
    }

    t_val = vec3_dot(vec3_sub(p.point, r.origin), p.normal) / denom;
    if (t_val > 1e-4) {
        *t = t_val;
        return 1;
    }

    return 0;
}

// Fresnel reflectance for dielectrics (Schlick's approximation)
static double fresnel_dielectric(double cos_theta, double eta) {
    double r0;
    double result;
    
    r0 = (1.0 - eta) / (1.0 + eta);
    r0 = r0 * r0;
    result = r0 + (1.0 - r0) * pow(1.0 - cos_theta, 5.0);
    return result;
}

// Reflect vector
static vec3 reflect(vec3 I, vec3 N) {
    vec3 result;
    result = vec3_sub(I, vec3_mul(N, 2.0 * vec3_dot(I, N)));
    return result;
}

// Refract vector
static vec3 refract(vec3 I, vec3 N, double eta) {
    double k;
    vec3 result;
    
    k = 1.0 - eta * eta * (1.0 - vec3_dot(I, N) * vec3_dot(I, N));
    if (k < 0.0) {
        result.x = 0.0;
        result.y = 0.0;
        result.z = 0.0; // Total internal reflection
        return result;
    }
    result = vec3_sub(vec3_mul(I, eta), vec3_mul(N, eta * vec3_dot(I, N) + sqrt(k)));
    return result;
}

// Trace ray recursively
static vec3 trace_ray(ray r, int depth) {
    double closest_t;
    int hit_sphere;
    int hit_plane;
    double t;
    vec3 hit_point;
    vec3 normal;
    material mat;
    vec3 shadow_intensity;
    vec3 color;
    vec3 diffuse;
    vec3 specular;
    vec3 light_contribution;
    double cos_theta;
    double eta;
    double reflect_prob;
    vec3 reflect_color;
    vec3 refract_color;
    vec3 reflect_dir;
    vec3 refract_dir;
    vec3 reflect_origin;
    vec3 refract_origin;
    ray reflect_ray;
    ray refract_ray;
    double transmittance;
    vec3 fog_color;
    vec3 temp_vec;
    double diff;
    double spec;
    vec3 view_dir;
    vec3 reflect_dir_temp;
    double light_distance;
    vec3 light_dir;
    vec3 shadow_origin;
    ray shadow_ray;
    double shadow_t;
    int in_shadow;
    int j;
    int i;
    vec3 white;
    
    if (depth <= 0) {
        return sky_color(r.direction);
    }

    closest_t = 1e20;
    hit_sphere = -1;
    hit_plane = 0;

    // Test sphere intersections
    i = 0;
    while (i < 2) {
        if (ray_sphere_intersect(r, g_scene.spheres[i], &t)) {
            if (t < closest_t) {
                closest_t = t;
                hit_sphere = i;
            }
        }
        i++;
    }

    // Test plane intersection
    if (ray_plane_intersect(r, g_scene.ground, &t)) {
        if (t < closest_t) {
            closest_t = t;
            hit_plane = 1;
        }
    }

    // No hit
    if (closest_t >= 1e19) {
        return sky_color(r.direction);
    }

    // Hit something
    hit_point = vec3_add(r.origin, vec3_mul(r.direction, closest_t));
    
    if (hit_sphere != -1) {
        // Sphere hit
        sphere s = g_scene.spheres[hit_sphere];
        normal = vec3_normalize(vec3_sub(hit_point, s.center));
        mat = s.mat;
        
        // Correct normal orientation for inside/outside
        if (vec3_dot(r.direction, normal) > 0) {
            normal = vec3_mul(normal, -1.0);
        }
    } else {
        // Plane hit
        normal = g_scene.ground.normal;
        mat = g_scene.ground.mat;
    }

    // Shadow ray testing
    shadow_intensity.x = 1.0;
    shadow_intensity.y = 1.0;
    shadow_intensity.z = 1.0;
    
    i = 0;
    while (i < 3) {
        light l = g_scene.lights[i];
        light_dir = vec3_normalize(vec3_sub(l.position, hit_point));
        light_distance = vec3_length(vec3_sub(l.position, hit_point));

        // Offset shadow ray origin to avoid self-intersection
        shadow_origin = vec3_add(hit_point, vec3_mul(normal, 1e-4));
        shadow_ray.origin = shadow_origin;
        shadow_ray.direction = light_dir;

        // Check if we're in shadow
        in_shadow = 0;
        
        // Test sphere intersections for shadow ray
        j = 0;
        while (j < 2) {
            if (j == hit_sphere) {
                j++;
                continue; // Skip the hit sphere
            }
            if (ray_sphere_intersect(shadow_ray, g_scene.spheres[j], &shadow_t)) {
                if (shadow_t < light_distance) {
                    in_shadow = 1;
                    break;
                }
            }
            j++;
        }

        // Test plane intersection for shadow ray
        if (!in_shadow && ray_plane_intersect(shadow_ray, g_scene.ground, &shadow_t)) {
            if (shadow_t < light_distance) {
                in_shadow = 1;
            }
        }

        if (in_shadow) {
            // Simple attenuation for glass
            if (mat.type == MATERIAL_DIELECTRIC) {
                shadow_intensity.x *= 0.3; // Tinted attenuation
                shadow_intensity.y *= 0.3;
                shadow_intensity.z *= 0.3;
            } else {
                shadow_intensity.x = 0.0; // Full shadow
                shadow_intensity.y = 0.0;
                shadow_intensity.z = 0.0;
            }
        }
        i++;
    }

    // Lighting calculation
    color.x = 0.0;
    color.y = 0.0;
    color.z = 0.0;

    // Ambient lighting
    temp_vec = vec3_mul(mat.albedo, 0.1);
    color = vec3_add(color, temp_vec);

    // Diffuse and specular lighting
    i = 0;
    while (i < 3) {
        light l = g_scene.lights[i];
        light_dir = vec3_normalize(vec3_sub(l.position, hit_point));
        light_distance = vec3_length(vec3_sub(l.position, hit_point));

        // Diffuse component
        diff = fmax(0.0, vec3_dot(normal, light_dir));
        diffuse = vec3_mul(mat.albedo, diff * l.intensity.x);

        // Specular component (Phong model)
        view_dir = vec3_normalize(vec3_mul(r.direction, -1.0));
        reflect_dir_temp = reflect(vec3_mul(light_dir, -1.0), normal);
        spec = pow(fmax(0.0, vec3_dot(view_dir, reflect_dir_temp)), 32.0);
        // Define white vector before using it
        white.x = 1.0;
        white.y = 1.0;
        white.z = 1.0;
        specular = vec3_mul(white, spec * l.intensity.x);

        // Combine lighting components
        light_contribution = vec3_add(diffuse, specular);
        temp_vec = vec3_mul(light_contribution, shadow_intensity.x);
        color = vec3_add(color, temp_vec);
        i++;
    }

    // Handle material-specific rendering
    if (mat.type == MATERIAL_DIELECTRIC) {
        // Dielectric (glass) material
        cos_theta = vec3_dot(vec3_mul(r.direction, -1.0), normal);
        eta = mat.ior; // 1.5 for glass

        // Calculate reflection coefficient
        reflect_prob = fresnel_dielectric(fabs(cos_theta), eta);

        // Sample reflection and refraction
        reflect_color.x = 0.0;
        reflect_color.y = 0.0;
        reflect_color.z = 0.0;
        refract_color.x = 0.0;
        refract_color.y = 0.0;
        refract_color.z = 0.0;

        // Reflection
        if (reflect_prob > 0.0) {
            reflect_dir = reflect(r.direction, normal);
            reflect_origin = vec3_add(hit_point, vec3_mul(normal, 1e-4));
            reflect_ray.origin = reflect_origin;
            reflect_ray.direction = reflect_dir;
            reflect_color = trace_ray(reflect_ray, depth - 1);
        }

        // Refraction
        if (reflect_prob < 1.0) {
            double eta_i = 1.0; // Air
            double eta_t = eta; // Glass
            double eta_ratio = eta_i / eta_t;

            refract_dir = refract(r.direction, normal, eta_ratio);
            if (vec3_length(refract_dir) > 1e-6) { // Check if refraction occurred
                refract_origin = vec3_add(hit_point, vec3_mul(normal, -1e-4)); // Inside sphere
                refract_ray.origin = refract_origin;
                refract_ray.direction = refract_dir;
                refract_color = trace_ray(refract_ray, depth - 1);
            }
        }

        // Combine reflection and refraction
        temp_vec = vec3_mul(reflect_color, reflect_prob);
        color = vec3_add(temp_vec, vec3_mul(refract_color, 1.0 - reflect_prob));
    } else if (mat.type == MATERIAL_METAL) {
        // Metal (chrome) material
        reflect_prob = 0.95; // Strong reflection

        reflect_dir = reflect(r.direction, normal);
        reflect_origin = vec3_add(hit_point, vec3_mul(normal, 1e-4));
        reflect_ray.origin = reflect_origin;
        reflect_ray.direction = reflect_dir;
        reflect_color = trace_ray(reflect_ray, depth - 1);

        color = vec3_mul(reflect_color, reflect_prob);
    }

    // Fog effect
    transmittance = exp(-0.035 * closest_t);
    fog_color = sky_color(r.direction);
    temp_vec = vec3_mul(color, transmittance);
    color = vec3_add(temp_vec, vec3_mul(fog_color, 1.0 - transmittance));

    return color;
}

// Gamma correction
static double gamma(double x) {
    double result;
    if (x <= 0.0031308) {
        result = 12.92 * x;
    } else {
        result = 1.055 * pow(x, 1.0 / 2.4) - 0.055;
    }
    return result;
}

// Save TGA file
static int save_tga(const char *filename, unsigned char *pixels, int width, int height) {
    FILE *fp;
    int i;
    unsigned char header[18];
    int idx;
    unsigned char b, g, r;
    
    fp = fopen(filename, "wb");
    if (!fp) {
        return 0;
    }

    // TGA Header (18 bytes)
    for (i = 0; i < 18; i++) {
        header[i] = 0;
    }
    header[2] = 2; // Image type: uncompressed true-color
    header[12] = width & 0xFF;          // Width (low byte)
    header[13] = (width >> 8) & 0xFF;   // Width (high byte)
    header[14] = height & 0xFF;         // Height (low byte)
    header[15] = (height >> 8) & 0xFF;  // Height (high byte)
    header[16] = 24; // Pixel depth (24 bits)
    header[17] = 0x20; // Image descriptor (top-left origin)

    if (fwrite(header, 1, 18, fp) != 18) {
        fclose(fp);
        return 0;
    }

    // Write pixel data (BGR format)
    i = height - 1;
    while (i >= 0) {
        int x = 0;
        while (x < width) {
            idx = (i * width + x) * 3;
            b = pixels[idx];
            g = pixels[idx + 1];
            r = pixels[idx + 2];
            if (fwrite(&b, 1, 1, fp) != 1 || fwrite(&g, 1, 1, fp) != 1 || fwrite(&r, 1, 1, fp) != 1) {
                fclose(fp);
                return 0;
            }
            x++;
        }
        i--;
    }

    fclose(fp);
    return 1;
}

// Initialize scene
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
    g_scene.aspect_ratio = 1280.0 / 720.0;

    // Spheres
    // Glass sphere (left)
    g_scene.spheres[0].center.x = -1.25;
    g_scene.spheres[0].center.y = 1.0;
    g_scene.spheres[0].center.z = 0.0;
    g_scene.spheres[0].radius = 1.0;
    g_scene.spheres[0].mat.type = MATERIAL_DIELECTRIC;
    g_scene.spheres[0].mat.albedo.x = 1.0;
    g_scene.spheres[0].mat.albedo.y = 1.0;
    g_scene.spheres[0].mat.albedo.z = 1.0;
    g_scene.spheres[0].mat.ior = 1.5;

    // Chrome sphere (right)
    g_scene.spheres[1].center.x = 1.25;
    g_scene.spheres[1].center.y = 1.0;
    g_scene.spheres[1].center.z = 0.0;
    g_scene.spheres[1].radius = 1.0;
    g_scene.spheres[1].mat.type = MATERIAL_METAL;
    g_scene.spheres[1].mat.albedo.x = 0.8;
    g_scene.spheres[1].mat.albedo.y = 0.8;
    g_scene.spheres[1].mat.albedo.z = 0.8;
    g_scene.spheres[1].mat.roughness = 0.05;

    // Ground plane
    g_scene.ground.point.x = 0.0;
    g_scene.ground.point.y = 0.0;
    g_scene.ground.point.z = 0.0;
    g_scene.ground.normal.x = 0.0;
    g_scene.ground.normal.y = 1.0;
    g_scene.ground.normal.z = 0.0;
    g_scene.ground.mat.type = MATERIAL_LAMBERTIAN;
    g_scene.ground.mat.albedo.x = 0.0;
    g_scene.ground.mat.albedo.y = 0.0;
    g_scene.ground.mat.albedo.z = 0.0; // Will be overridden by checkerboard

    // Lights
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

// Main function
int main(void) {
    vec3 camera_forward;
    vec3 camera_right;
    vec3 camera_up;
    double fov_rad;
    double half_height;
    double half_width;
    int width;
    int height;
    int samples_per_pixel;
    int max_depth;
    unsigned char *pixels;
    int y, x, sy, sx;
    double u, v;
    double ndc_x, ndc_y;
    vec3 ray_dir;
    ray r;
    vec3 pixel_color;
    vec3 color;
    int idx;
    double gamma_result;
    
    init_scene();

    width = 1280;
    height = 720;
    samples_per_pixel = 4;
    max_depth = 8;

    // Allocate pixel buffer
    pixels = malloc(width * height * 3);
    if (!pixels) {
        fprintf(stderr, "Failed to allocate pixel buffer\n");
        return 1;
    }

    // Calculate camera setup
    camera_forward = vec3_normalize(vec3_sub(g_scene.camera_target, g_scene.camera_pos));
    camera_right = vec3_normalize(vec3_cross(camera_forward, g_scene.camera_up));
    camera_up = vec3_cross(camera_right, camera_forward);

    // Calculate field of view in radians
    fov_rad = g_scene.fov * M_PI / 180.0;
    half_height = tan(fov_rad / 2.0);
    half_width = half_height * g_scene.aspect_ratio;

    // Render the image
    y = 0;
    while (y < height) {
        x = 0;
        while (x < width) {
            pixel_color.x = 0.0;
            pixel_color.y = 0.0;
            pixel_color.z = 0.0;

            // Sub-pixel sampling
            sy = 0;
            while (sy < 2) {
                sx = 0;
                while (sx < 2) {
                    u = (x + (sx + 0.5) / 2.0) / width;
                    v = (y + (sy + 0.5) / 2.0) / height;

                    // Convert to normalized device coordinates
                    ndc_x = (2.0 * u - 1.0) * half_width;
                    ndc_y = (1.0 - 2.0 * v) * half_height;

                    // Calculate ray direction
                    ray_dir = vec3_normalize(vec3_add(
                        vec3_add(vec3_mul(camera_right, ndc_x),
                                 vec3_mul(camera_up, ndc_y)),
                        camera_forward));

                    r.origin = g_scene.camera_pos;
                    r.direction = ray_dir;

                    // Trace ray
                    color = trace_ray(r, max_depth);

                    // Accumulate color
                    pixel_color = vec3_add(pixel_color, color);
                    sx++;
                }
                sy++;
            }

            // Average samples
            pixel_color = vec3_div(pixel_color, (double)samples_per_pixel);

            // Gamma correction
            idx = (y * width + x) * 3;
            gamma_result = gamma(pixel_color.z);
            pixels[idx] = (unsigned char)(fmin(1.0, gamma_result) * 255.0); // B
            gamma_result = gamma(pixel_color.y);
            pixels[idx + 1] = (unsigned char)(fmin(1.0, gamma_result) * 255.0); // G
            gamma_result = gamma(pixel_color.x);
            pixels[idx + 2] = (unsigned char)(fmin(1.0, gamma_result) * 255.0); // R
            x++;
        }
        y++;
    }

    // Save image
    if (!save_tga("out.tga", pixels, width, height)) {
        fprintf(stderr, "Failed to save TGA file\n");
        free(pixels);
        return 1;
    }

    free(pixels);
    return 0;
}
