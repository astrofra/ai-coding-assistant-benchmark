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

static double vec3_length(vec3 a) {
    return sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}

static double vec3_length_squared(vec3 a) {
    return a.x * a.x + a.y * a.y + a.z * a.z;
}

static vec3 vec3_normalize(vec3 a) {
    double len = vec3_length(a);
    vec3 result;
    if (len > 1e-10) {
        result.x = a.x / len;
        result.y = a.y / len;
        result.z = a.z / len;
    } else {
        result.x = 0.0;
        result.y = 0.0;
        result.z = 0.0;
    }
    return result;
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
    double reflectivity;
    double transparency;
    double roughness;
    vec3 color;
    double ior; // Index of refraction
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
    vec3 camera_pos;
    vec3 camera_target;
    vec3 camera_up;
    double fov_y;
    double aspect_ratio;
} scene;

// Sky color computation
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
    
    // Ensure alternating pattern for negative coordinates
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
static int intersect_ray_sphere(ray r, sphere s, double *t) {
    vec3 oc;
    double a, b, c, discriminant, sqrt_discriminant, t1, t2;
    
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
static int intersect_ray_plane(ray r, plane p, double *t) {
    double denom;
    vec3 op;
    
    denom = vec3_dot(p.normal, r.direction);
    
    if (fabs(denom) < 1e-6) {
        return 0; // Ray is parallel to plane
    }

    op = vec3_sub(p.point, r.origin);
    *t = vec3_dot(op, p.normal) / denom;

    if (*t > 1e-4) {
        return 1;
    }

    return 0;
}

// Fresnel reflection coefficient using Schlick's approximation
static double fresnel(double cos_theta, double eta) {
    double r0;
    r0 = (1.0 - eta) / (1.0 + eta);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow((1.0 - cos_theta), 5.0);
}

// Reflection direction
static vec3 reflect(vec3 incident, vec3 normal) {
    vec3 result;
    double dot_product;
    
    dot_product = vec3_dot(incident, normal);
    result = vec3_sub(incident, vec3_mul(normal, 2.0 * dot_product));
    return result;
}

// Refraction direction
static vec3 refract(vec3 incident, vec3 normal, double eta) {
    double cos_i, k;
    vec3 result;
    
    cos_i = vec3_dot(incident, normal);
    k = 1.0 - eta * eta * (1.0 - cos_i * cos_i);

    if (k < 0.0) {
        // Total internal reflection
        return reflect(incident, normal);
    }

    result = vec3_sub(vec3_mul(incident, eta), 
                      vec3_mul(normal, eta * cos_i + sqrt(k)));
    return result;
}

// Trace a ray through the scene
static vec3 trace_ray(ray r, scene s, int depth) {
    // Declare all variables at the beginning of the function (C89 compliance)
    double t;
    vec3 hit_point, hit_normal, color, local_color, temp_color;
    material hit_material;
    int hit_sphere = 0;
    int hit_ground = 0;
    vec3 light_dir, reflect_dir, view_dir;
    double light_distance, diff, spec, cos_theta;
    int i, in_shadow;
    double shadow_t;
    ray shadow_ray;
    ray reflect_ray, refract_ray;
    vec3 reflect_color, refract_color;
    double transmittance, fog_color_x, fog_color_y, fog_color_z;
    vec3 refract_dir;
    
    // Limit recursion depth
    if (depth >= 8) {
        return sky_color(r.direction);
    }

    // Test intersection with glass sphere
    if (intersect_ray_sphere(r, s.glass_sphere, &t)) {
        hit_sphere = 1;
        hit_point = vec3_add(r.origin, vec3_mul(r.direction, t));
        hit_normal = vec3_normalize(vec3_sub(hit_point, s.glass_sphere.center));
        hit_material.reflectivity = 0.0;
        hit_material.transparency = 1.0;
        hit_material.roughness = 0.0;
        hit_material.color.x = 1.0;
        hit_material.color.y = 1.0;
        hit_material.color.z = 1.0;
        hit_material.ior = 1.5;
    }

    // Test intersection with chrome sphere
    if (intersect_ray_sphere(r, s.chrome_sphere, &t)) {
        if (!hit_sphere || t < 1e-4) {
            hit_sphere = 2;
            hit_point = vec3_add(r.origin, vec3_mul(r.direction, t));
            hit_normal = vec3_normalize(vec3_sub(hit_point, s.chrome_sphere.center));
            hit_material.reflectivity = 0.95;
            hit_material.transparency = 0.0;
            hit_material.roughness = 0.0;
            hit_material.color.x = 0.8;
            hit_material.color.y = 0.8;
            hit_material.color.z = 0.8;
            hit_material.ior = 1.0;
        }
    }

    // Test intersection with ground plane
    if (intersect_ray_plane(r, s.ground_plane, &t)) {
        if (!hit_sphere || t < 1e-4) {
            hit_ground = 1;
            hit_point = vec3_add(r.origin, vec3_mul(r.direction, t));
            hit_normal.x = 0.0;
            hit_normal.y = 1.0;
            hit_normal.z = 0.0;
            // Get checkerboard color
            local_color = checkerboard_color(hit_point);
            hit_material.reflectivity = 0.0;
            hit_material.transparency = 0.0;
            hit_material.roughness = 0.0;
            hit_material.color = local_color;
            hit_material.ior = 1.0;
        }
    }

    // If we hit nothing, return sky color
    if (!hit_sphere && !hit_ground) {
        return sky_color(r.direction);
    }

    // Calculate lighting for the hit point
    color.x = 0.0;
    color.y = 0.0;
    color.z = 0.0;
    local_color = hit_material.color;

    // Shadow rays and lighting calculations
    i = 0;
    while (i < 3) {
        light_dir = vec3_normalize(vec3_sub(s.lights[i].position, hit_point));
        light_distance = vec3_length(vec3_sub(s.lights[i].position, hit_point));

        // Shadow ray
        shadow_ray.origin = vec3_add(hit_point, vec3_mul(hit_normal, 1e-4));
        shadow_ray.direction = light_dir;

        // Check if we're in shadow
        in_shadow = 0;
        if (hit_sphere == 1) { // Glass sphere
            if (intersect_ray_sphere(shadow_ray, s.glass_sphere, &shadow_t) && shadow_t < light_distance) {
                in_shadow = 1;
            }
        } else if (hit_sphere == 2) { // Chrome sphere
            if (intersect_ray_sphere(shadow_ray, s.chrome_sphere, &shadow_t) && shadow_t < light_distance) {
                in_shadow = 1;
            }
        } else { // Ground plane
            if (intersect_ray_plane(shadow_ray, s.ground_plane, &shadow_t) && shadow_t < light_distance) {
                in_shadow = 1;
            }
        }

        if (!in_shadow) {
            // Diffuse lighting (Lambert)
            diff = fmax(0.0, vec3_dot(hit_normal, light_dir));

            // Specular lighting (Phong)
            view_dir = vec3_normalize(vec3_sub(s.camera_pos, hit_point));
            reflect_dir = reflect(vec3_mul(light_dir, -1.0), hit_normal);
            spec = pow(fmax(0.0, vec3_dot(view_dir, reflect_dir)), 32.0);

            // Combine lighting
            temp_color = vec3_mul(local_color, diff * s.lights[i].intensity.x);
            color = vec3_add(color, temp_color);
            temp_color.x = spec;
            temp_color.y = spec;
            temp_color.z = spec;
            temp_color = vec3_mul(temp_color, s.lights[i].intensity.x);
            color = vec3_add(color, temp_color);
        }
        i++;
    }

    // Ambient lighting
    temp_color = vec3_mul(local_color, 0.1);
    color = vec3_add(color, temp_color);

    // Handle reflections and refractions
    if (hit_material.reflectivity > 0.0) {
        reflect_dir = reflect(r.direction, hit_normal);
        reflect_ray.origin = vec3_add(hit_point, vec3_mul(hit_normal, 1e-4));
        reflect_ray.direction = reflect_dir;

        reflect_color = trace_ray(reflect_ray, s, depth + 1);
        temp_color = vec3_mul(color, 1.0 - hit_material.reflectivity);
        temp_color = vec3_add(temp_color, vec3_mul(reflect_color, hit_material.reflectivity));
        color = temp_color;
    }

    // Handle refractions
    if (hit_material.transparency > 0.0 && hit_material.ior != 1.0) {
        // Determine if ray is entering or exiting sphere
        cos_theta = vec3_dot(r.direction, hit_normal);
        if (cos_theta > 0.0) {
            // Entering sphere
            double eta = hit_material.ior;
            refract_dir = refract(r.direction, hit_normal, eta);
        } else {
            // Exiting sphere
            double eta = 1.0 / hit_material.ior;
            refract_dir = refract(r.direction, hit_normal, eta);
        }
        
        // If total internal reflection occurs, we use reflection instead
        if (vec3_length(refract_dir) < 1e-6) {
            reflect_dir = reflect(r.direction, hit_normal);
            reflect_ray.origin = vec3_add(hit_point, vec3_mul(hit_normal, 1e-4));
            reflect_ray.direction = reflect_dir;

            reflect_color = trace_ray(reflect_ray, s, depth + 1);
            temp_color = vec3_mul(color, 1.0 - hit_material.transparency);
            temp_color = vec3_add(temp_color, vec3_mul(reflect_color, hit_material.transparency));
            color = temp_color;
        } else {
            // Refraction
            refract_ray.origin = vec3_add(hit_point, vec3_mul(hit_normal, -1e-4)); // Inside sphere
            refract_ray.direction = refract_dir;

            refract_color = trace_ray(refract_ray, s, depth + 1);
            temp_color = vec3_mul(color, 1.0 - hit_material.transparency);
            temp_color = vec3_add(temp_color, vec3_mul(refract_color, hit_material.transparency));
            color = temp_color;
        }
    }

    // Apply fog effect
    transmittance = exp(-0.035 * t);
    temp_color = sky_color(r.direction);
    fog_color_x = temp_color.x;
    fog_color_y = temp_color.y;
    fog_color_z = temp_color.z;
    temp_color = vec3_mul(color, transmittance);
    color.x = temp_color.x;
    color.y = temp_color.y;
    color.z = temp_color.z;
    temp_color.x = fog_color_x * (1.0 - transmittance);
    temp_color.y = fog_color_y * (1.0 - transmittance);
    temp_color.z = fog_color_z * (1.0 - transmittance);
    color = vec3_add(color, temp_color);

    return color;
}

// Generate TGA file header
static void write_tga_header(FILE *fp) {
    unsigned char header[18];
    int i;
    
    for (i = 0; i < 18; i++) {
        header[i] = 0;
    }

    header[2] = 2; // Image type: uncompressed true-color
    header[12] = 1280 & 0xFF; // Width (low byte)
    header[13] = (1280 >> 8) & 0xFF; // Width (high byte)
    header[14] = 720 & 0xFF; // Height (low byte)
    header[15] = (720 >> 8) & 0xFF; // Height (high byte)
    header[16] = 24; // Pixel depth
    header[17] = 0x20; // Descriptor: top-left origin, left-to-right pixel order

    fwrite(header, 1, 18, fp);
}

// Convert RGB to 8-bit values with gamma correction
static unsigned char gamma_correct(double c) {
    double gamma;
    gamma = 2.2;
    if (c <= 0.0) return 0;
    if (c >= 1.0) return 255;
    return (unsigned char)(pow(c, 1.0 / gamma) * 255.0 + 0.5);
}

// Main function
int main(void) {
    // Declare all variables at the beginning of the function (C89 compliance)
    scene s;
    int i, j;
    double u, v, ndc_x, ndc_y;
    vec3 forward, right, up, ray_dir;
    ray r;
    vec3 color, ray_color;
    unsigned char *image;
    int idx;
    int sy, sx;
    FILE *fp;
    
    // Camera setup
    s.camera_pos.x = 0.0;
    s.camera_pos.y = 2.6;
    s.camera_pos.z = 6.5;
    s.camera_target.x = 0.0;
    s.camera_target.y = 1.0;
    s.camera_target.z = 0.0;
    s.camera_up.x = 0.0;
    s.camera_up.y = 1.0;
    s.camera_up.z = 0.0;
    s.fov_y = 45.0 * M_PI / 180.0;
    s.aspect_ratio = 1280.0 / 720.0;

    // Glass sphere
    s.glass_sphere.center.x = -1.25;
    s.glass_sphere.center.y = 1.0;
    s.glass_sphere.center.z = 0.0;
    s.glass_sphere.radius = 1.0;

    // Chrome sphere
    s.chrome_sphere.center.x = 1.25;
    s.chrome_sphere.center.y = 1.0;
    s.chrome_sphere.center.z = 0.0;
    s.chrome_sphere.radius = 1.0;

    // Ground plane
    s.ground_plane.point.x = 0.0;
    s.ground_plane.point.y = 0.0;
    s.ground_plane.point.z = 0.0;
    s.ground_plane.normal.x = 0.0;
    s.ground_plane.normal.y = 1.0;
    s.ground_plane.normal.z = 0.0;

    // Lights
    s.lights[0].position.x = -3.5;
    s.lights[0].position.y = 6.0;
    s.lights[0].position.z = 4.5;
    s.lights[0].intensity.x = 1.0;
    s.lights[0].intensity.y = 1.0;
    s.lights[0].intensity.z = 1.0;
    
    s.lights[1].position.x = 4.0;
    s.lights[1].position.y = 3.0;
    s.lights[1].position.z = 3.0;
    s.lights[1].intensity.x = 0.35;
    s.lights[1].intensity.y = 0.35;
    s.lights[1].intensity.z = 0.35;
    
    s.lights[2].position.x = 0.5;
    s.lights[2].position.y = 5.0;
    s.lights[2].position.z = -4.0;
    s.lights[2].intensity.x = 0.7;
    s.lights[2].intensity.y = 0.7;
    s.lights[2].intensity.z = 0.7;

    // Create image buffer
    image = (unsigned char *)malloc(1280 * 720 * 3);
    if (!image) {
        fprintf(stderr, "Failed to allocate memory for image\n");
        return 1;
    }

    // Generate image
    for (j = 0; j < 720; j++) {
        for (i = 0; i < 1280; i++) {
            // Subpixel sampling
            color.x = 0.0;
            color.y = 0.0;
            color.z = 0.0;
            
            // 2x2 subpixel sampling
            for (sy = 0; sy < 2; sy++) {
                for (sx = 0; sx < 2; sx++) {
                    u = (i + (sx + 0.5) / 2.0) / 1280.0;
                    v = (j + (sy + 0.5) / 2.0) / 720.0;

                    // Convert to normalized device coordinates
                    ndc_x = (2.0 * u - 1.0) * tan(s.fov_y / 2.0) * s.aspect_ratio;
                    ndc_y = (1.0 - 2.0 * v) * tan(s.fov_y / 2.0);

                    // Camera coordinate system
                    forward = vec3_normalize(vec3_sub(s.camera_target, s.camera_pos));
                    right = vec3_normalize(vec3_cross(forward, s.camera_up));
                    up = vec3_cross(right, forward);

                    // Ray direction
                    ray_dir = vec3_normalize(vec3_add(vec3_add(vec3_mul(right, ndc_x),
                                                               vec3_mul(up, ndc_y)),
                                                      forward));

                    // Ray origin
                    r.origin = s.camera_pos;
                    r.direction = ray_dir;

                    // Trace ray
                    ray_color = trace_ray(r, s, 0);

                    // Accumulate color
                    color = vec3_add(color, ray_color);
                }
            }

            // Average the samples
            color = vec3_div(color, 4.0);

            // Store pixel in BGR format
            idx = (j * 1280 + i) * 3;
            image[idx] = gamma_correct(color.z);     // Blue
            image[idx + 1] = gamma_correct(color.y); // Green
            image[idx + 2] = gamma_correct(color.x); // Red
        }
    }

    // Write TGA file
    fp = fopen("out.tga", "wb");
    if (!fp) {
        fprintf(stderr, "Failed to create output file\n");
        free(image);
        return 1;
    }

    write_tga_header(fp);
    fwrite(image, 1, 1280 * 720 * 3, fp);
    fclose(fp);

    free(image);

    return 0;
}
