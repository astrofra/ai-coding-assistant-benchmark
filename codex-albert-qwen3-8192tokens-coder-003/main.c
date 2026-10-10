#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Vector operations */
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

vec3 vec3_mul(vec3 a, double s) {
    vec3 result;
    result.x = a.x * s;
    result.y = a.y * s;
    result.z = a.z * s;
    return result;
}

vec3 vec3_div(vec3 a, double s) {
    vec3 result;
    result.x = a.x / s;
    result.y = a.y / s;
    result.z = a.z / s;
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
        result = vec3_div(v, len);
    } else {
        result.x = 0.0;
        result.y = 0.0;
        result.z = 0.0;
    }
    return result;
}

/* Ray structure */
typedef struct {
    vec3 origin;
    vec3 direction;
} ray;

/* Material types */
typedef enum {
    MATERIAL_LAMBERTIAN,
    MATERIAL_METAL,
    MATERIAL_DIELECTRIC
} material_type;

/* Material structure */
typedef struct {
    material_type type;
    vec3 color;
    double roughness;
    double reflectivity;
    double ior; /* index of refraction */
} material;

/* Sphere structure */
typedef struct {
    vec3 center;
    double radius;
    material mat;
} sphere;

/* Plane structure */
typedef struct {
    vec3 point;
    vec3 normal;
    material mat;
} plane;

/* Light structure */
typedef struct {
    vec3 position;
    vec3 intensity;
} light;

/* Scene structure */
typedef struct {
    sphere spheres[2];
    plane ground;
    light lights[3];
    int num_lights;
} scene;

/* Global scene */
scene g_scene;

/* Sky color computation */
vec3 sky_color(vec3 ray_dir) {
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

/* Checkerboard pattern */
vec3 checkerboard_color(vec3 point) {
    int x, z;
    vec3 result;
    
    x = (int)floor(point.x);
    z = (int)floor(point.z);
    
    /* Alternate black and white squares */
    if ((x + z) % 2 == 0) {
        result.x = 1.0; /* White */
        result.y = 1.0;
        result.z = 1.0;
    } else {
        result.x = 0.0; /* Black */
        result.y = 0.0;
        result.z = 0.0;
    }
    
    return result;
}

/* Ray-sphere intersection */
int intersect_sphere(ray r, sphere s, double *t) {
    vec3 oc;
    double a, b, c, discriminant, sqrt_discriminant, t1, t2;
    
    oc = vec3_sub(r.origin, s.center);
    a = vec3_dot(r.direction, r.direction);
    b = 2.0 * vec3_dot(oc, r.direction);
    c = vec3_dot(oc, oc) - s.radius * s.radius;
    discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) {
        return 0; /* No intersection */
    }
    
    /* Find the nearest positive root */
    sqrt_discriminant = sqrt(discriminant);
    t1 = (-b - sqrt_discriminant) / (2.0 * a);
    t2 = (-b + sqrt_discriminant) / (2.0 * a);
    
    if (t1 > 1e-4) {
        *t = t1;
        return 1; /* Hit at t1 */
    } else if (t2 > 1e-4) {
        *t = t2;
        return 1; /* Hit at t2 */
    }
    
    return 0; /* No valid intersection */
}

/* Ray-plane intersection */
int intersect_plane(ray r, plane p, double *t) {
    double denom;
    vec3 op;
    
    denom = vec3_dot(p.normal, r.direction);
    
    if (fabs(denom) < 1e-6) {
        return 0; /* Ray is parallel to plane */
    }
    
    op = vec3_sub(p.point, r.origin);
    *t = vec3_dot(op, p.normal) / denom;
    
    if (*t > 1e-4) {
        return 1; /* Valid intersection */
    }
    
    return 0; /* Intersection behind ray origin */
}

/* Fresnel reflection coefficient (Schlick's approximation) */
double fresnel(double cos_theta, double eta_ratio) {
    double r0;
    r0 = (1.0 - eta_ratio) / (1.0 + eta_ratio);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow((1.0 - cos_theta), 5.0);
}

/* Reflect vector */
vec3 reflect(vec3 incident, vec3 normal) {
    vec3 result;
    double dot;
    dot = vec3_dot(incident, normal);
    result = vec3_sub(incident, vec3_mul(normal, 2.0 * dot));
    return result;
}

/* Refract vector */
vec3 refract(vec3 incident, vec3 normal, double eta_ratio) {
    double cos_i, k;
    vec3 result;
    
    cos_i = vec3_dot(incident, normal);
    k = 1.0 - eta_ratio * eta_ratio * (1.0 - cos_i * cos_i);
    
    if (k < 0.0) {
        result.x = 0.0; /* Total internal reflection */
        result.y = 0.0;
        result.z = 0.0;
        return result;
    }
    
    result.x = eta_ratio * (incident.x - normal.x * cos_i) - normal.x * sqrt(k);
    result.y = eta_ratio * (incident.y - normal.y * cos_i) - normal.y * sqrt(k);
    result.z = eta_ratio * (incident.z - normal.z * cos_i) - normal.z * sqrt(k);
    
    return result;
}

/* Shadow ray test */
int is_shadowed(ray r, double max_dist) {
    double t;
    int i;
    
    /* Test intersection with spheres */
    for (i = 0; i < 2; i++) {
        if (intersect_sphere(r, g_scene.spheres[i], &t)) {
            if (t < max_dist) {
                return 1; /* Shadowed */
            }
        }
    }
    
    /* Test intersection with ground plane */
    if (intersect_plane(r, g_scene.ground, &t)) {
        if (t < max_dist) {
            return 1; /* Shadowed */
        }
    }
    
    return 0; /* Not shadowed */
}

/* Compute lighting for a point */
vec3 compute_lighting(vec3 point, vec3 normal, vec3 view_dir, material mat) {
    vec3 color;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    vec3 light_dir;
    double light_distance, diffuse_factor, spec_factor;
    int i;
    double temp1, temp2, temp3;
    
    color.x = 0.0;
    color.y = 0.0;
    color.z = 0.0;
    
    /* Ambient component */
    ambient.x = 0.1;
    ambient.y = 0.1;
    ambient.z = 0.1;
    
    /* We need to do component-wise multiplication */
    temp1 = ambient.x * mat.color.x;
    temp2 = ambient.y * mat.color.y;
    temp3 = ambient.z * mat.color.z;
    color.x = temp1;
    color.y = temp2;
    color.z = temp3;
    
    /* Direct lighting from each light */
    for (i = 0; i < g_scene.num_lights; i++) {
        light_dir = vec3_sub(g_scene.lights[i].position, point);
        light_dir = vec3_normalize(light_dir);
        light_distance = vec3_length(vec3_sub(g_scene.lights[i].position, point));
        
        /* Shadow test */
        ray shadow_ray;
        shadow_ray.origin = vec3_add(point, vec3_mul(normal, 1e-4)); /* Offset from surface */
        shadow_ray.direction = light_dir;
        
        if (!is_shadowed(shadow_ray, light_distance)) {
            /* Diffuse lighting (Lambert) */
            diffuse_factor = fmax(0.0, vec3_dot(normal, light_dir));
            
            /* Component-wise multiplication */
            temp1 = mat.color.x * g_scene.lights[i].intensity.x;
            temp2 = mat.color.y * g_scene.lights[i].intensity.y;
            temp3 = mat.color.z * g_scene.lights[i].intensity.z;
            diffuse.x = temp1 * diffuse_factor;
            diffuse.y = temp2 * diffuse_factor;
            diffuse.z = temp3 * diffuse_factor;
            
            /* Specular lighting (Phong) */
            vec3 reflect_dir;
            reflect_dir = vec3_mul(light_dir, -1.0);
            reflect_dir = reflect(reflect_dir, normal);
            spec_factor = pow(fmax(0.0, vec3_dot(view_dir, reflect_dir)), 32.0);
            
            /* Component-wise multiplication for specular */
            temp1 = 1.0 * g_scene.lights[i].intensity.x;
            temp2 = 1.0 * g_scene.lights[i].intensity.y;
            temp3 = 1.0 * g_scene.lights[i].intensity.z;
            specular.x = temp1 * spec_factor;
            specular.y = temp2 * spec_factor;
            specular.z = temp3 * spec_factor;
            
            color = vec3_add(color, diffuse);
            color = vec3_add(color, specular);
        }
    }
    
    return color;
}

/* Trace a ray recursively */
vec3 trace_ray(ray r, int depth) {
    double closest_t, t;
    int hit_sphere, hit_ground;
    vec3 hit_point, hit_normal;
    material hit_mat;
    vec3 view_dir;
    vec3 color;
    vec3 reflect_color;
    vec3 refract_color;
    vec3 ray_dir;
    double cos_i, eta_ratio, reflect_prob;
    vec3 outward_normal;
    vec3 refracted;
    vec3 reflected;
    ray reflect_ray;
    ray refract_ray;
    vec3 lit_color;
    vec3 checker_color;
    double transmittance;
    vec3 fog_color;
    vec3 final_color;
    
    if (depth <= 0) {
        return sky_color(r.direction);
    }
    
    closest_t = 1e20;
    hit_sphere = -1;
    hit_ground = 0;
    
    /* Test intersection with spheres */
    {
        int i;
        for (i = 0; i < 2; i++) {
            if (intersect_sphere(r, g_scene.spheres[i], &t) && t < closest_t) {
                closest_t = t;
                hit_sphere = i;
                hit_point = vec3_add(r.origin, vec3_mul(r.direction, t));
                hit_normal = vec3_normalize(vec3_sub(hit_point, g_scene.spheres[i].center));
                hit_mat = g_scene.spheres[i].mat;
            }
        }
    }
    
    /* Test intersection with ground plane */
    if (intersect_plane(r, g_scene.ground, &t) && t < closest_t) {
        closest_t = t;
        hit_ground = 1;
        hit_point = vec3_add(r.origin, vec3_mul(r.direction, t));
        hit_normal.x = 0.0;
        hit_normal.y = 1.0;
        hit_normal.z = 0.0;
        hit_mat = g_scene.ground.mat;
    }
    
    /* No intersection */
    if (closest_t >= 1e20) {
        return sky_color(r.direction);
    }
    
    /* Compute lighting for the hit point */
    view_dir = vec3_mul(r.direction, -1.0);
    view_dir = vec3_normalize(view_dir);
    color.x = 0.0;
    color.y = 0.0;
    color.z = 0.0;
    
    if (hit_sphere != -1) {
        /* Handle sphere materials */
        if (hit_mat.type == MATERIAL_DIELECTRIC) {
            /* Dielectric (glass) material */
            ray_dir = vec3_normalize(r.direction);
            cos_i = vec3_dot(ray_dir, hit_normal);
            eta_ratio = (cos_i > 0) ? hit_mat.ior : 1.0 / hit_mat.ior;
            
            /* Determine if we're entering or exiting the sphere */
            if (cos_i > 0) {
                outward_normal = vec3_mul(hit_normal, -1.0);
            } else {
                outward_normal = hit_normal;
            }
            refracted = refract(ray_dir, outward_normal, eta_ratio);
            
            reflect_prob = fresnel(fabs(cos_i), eta_ratio);
            
            /* Reflection */
            reflected = reflect(ray_dir, hit_normal);
            reflect_ray.origin = vec3_add(hit_point, vec3_mul(hit_normal, 1e-4));
            reflect_ray.direction = reflected;
            
            reflect_color = trace_ray(reflect_ray, depth - 1);
            
            if (vec3_length_squared(refracted) > 1e-10) {
                /* Refraction */
                refract_ray.origin = vec3_add(hit_point, vec3_mul(outward_normal, -1e-4));
                refract_ray.direction = refracted;
                
                refract_color = trace_ray(refract_ray, depth - 1);
                
                /* Combine reflection and refraction */
                color = vec3_add(vec3_mul(reflect_color, reflect_prob),
                                vec3_mul(refract_color, 1.0 - reflect_prob));
            } else {
                /* Total internal reflection */
                color = reflect_color;
            }
        } else if (hit_mat.type == MATERIAL_METAL) {
            /* Metal (chrome) material */
            ray_dir = vec3_normalize(r.direction);
            reflected = reflect(ray_dir, hit_normal);
            reflect_ray.origin = vec3_add(hit_point, vec3_mul(hit_normal, 1e-4));
            reflect_ray.direction = reflected;
            
            color = trace_ray(reflect_ray, depth - 1);
            
            /* Apply lighting to metal */
            lit_color = compute_lighting(hit_point, hit_normal, view_dir, hit_mat);
            color.x = color.x * hit_mat.color.x + lit_color.x * 0.1;
            color.y = color.y * hit_mat.color.y + lit_color.y * 0.1;
            color.z = color.z * hit_mat.color.z + lit_color.z * 0.1;
        } else {
            /* Lambertian material (ground) */
            lit_color = compute_lighting(hit_point, hit_normal, view_dir, hit_mat);
            color.x = lit_color.x * hit_mat.color.x;
            color.y = lit_color.y * hit_mat.color.y;
            color.z = lit_color.z * hit_mat.color.z;
        }
    } else if (hit_ground) {
        /* Ground plane - check if it's the checkerboard */
        checker_color = checkerboard_color(hit_point);
        lit_color = compute_lighting(hit_point, hit_normal, view_dir, hit_mat);
        color.x = lit_color.x * checker_color.x;
        color.y = lit_color.y * checker_color.y;
        color.z = lit_color.z * checker_color.z;
    }
    
    /* Apply fog effect */
    transmittance = exp(-0.035 * closest_t);
    fog_color = sky_color(r.direction);
    final_color = vec3_add(vec3_mul(color, transmittance),
                          vec3_mul(fog_color, 1.0 - transmittance));
    
    return final_color;
}

/* Convert RGB to sRGB gamma corrected */
double gamma_correct(double c) {
    if (c <= 0.0031308) {
        return 12.92 * c;
    } else {
        return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
    }
}

/* Save TGA image */
void save_tga(const char* filename, unsigned char* pixels, int width, int height) {
    FILE* file;
    unsigned char header[18];
    int y, x, idx;
    
    file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s for writing\n", filename);
        exit(1);
    }
    
    /* TGA Header (18 bytes) */
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
    header[12] = width & 0xFF; /* Width (little-endian) */
    header[13] = (width >> 8) & 0xFF;
    header[14] = height & 0xFF; /* Height (little-endian) */
    header[15] = (height >> 8) & 0xFF;
    header[16] = 24; /* Pixel depth */
    header[17] = 0x20; /* Image descriptor (top-left origin, left-to-right) */
    
    fwrite(header, 1, 18, file);
    
    /* Write pixel data (BGR order, bottom to top) */
    for (y = height - 1; y >= 0; y--) {
        for (x = 0; x < width; x++) {
            idx = (y * width + x) * 3;
            /* Write BGR (TGA uses BGR order) */
            fputc(pixels[idx + 2], file); /* Blue */
            fputc(pixels[idx + 1], file); /* Green */
            fputc(pixels[idx + 0], file); /* Red */
        }
    }
    
    fclose(file);
}

/* Initialize the scene */
void init_scene(void) {
    /* Initialize spheres */
    /* Glass sphere */
    g_scene.spheres[0].center.x = -1.25;
    g_scene.spheres[0].center.y = 1.0;
    g_scene.spheres[0].center.z = 0.0;
    g_scene.spheres[0].radius = 1.0;
    g_scene.spheres[0].mat.type = MATERIAL_DIELECTRIC;
    g_scene.spheres[0].mat.color.x = 1.0;
    g_scene.spheres[0].mat.color.y = 1.0;
    g_scene.spheres[0].mat.color.z = 1.0;
    g_scene.spheres[0].mat.ior = 1.5;
    
    /* Chrome sphere */
    g_scene.spheres[1].center.x = 1.25;
    g_scene.spheres[1].center.y = 1.0;
    g_scene.spheres[1].center.z = 0.0;
    g_scene.spheres[1].radius = 1.0;
    g_scene.spheres[1].mat.type = MATERIAL_METAL;
    g_scene.spheres[1].mat.color.x = 0.8;
    g_scene.spheres[1].mat.color.y = 0.8;
    g_scene.spheres[1].mat.color.z = 0.8;
    g_scene.spheres[1].mat.reflectivity = 0.95;
    
    /* Initialize ground plane */
    g_scene.ground.point.x = 0.0;
    g_scene.ground.point.y = 0.0;
    g_scene.ground.point.z = 0.0;
    g_scene.ground.normal.x = 0.0;
    g_scene.ground.normal.y = 1.0;
    g_scene.ground.normal.z = 0.0;
    g_scene.ground.mat.type = MATERIAL_LAMBERTIAN;
    g_scene.ground.mat.color.x = 0.5;
    g_scene.ground.mat.color.y = 0.5;
    g_scene.ground.mat.color.z = 0.5;
    
    /* Initialize lights */
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
    
    g_scene.num_lights = 3;
}

int main(void) {
    /* Initialize the scene */
    init_scene();
    
    const int width = 1280;
    const int height = 720;
    const int samples_per_pixel = 4; /* 2x2 subpixel sampling */
    
    /* Allocate pixel buffer */
    unsigned char* pixels;
    int y, x, sy, sx, idx, r, g, b;
    double u, v, s, t;
    vec3 pixel_color;
    vec3 ray_dir;
    ray r_ray;
    vec3 color;
    double transmittance;
    vec3 fog_color;
    vec3 final_color;
    double gamma_result;
    vec3 temp_vec3;
    int temp_int;
    double temp_double;
    
    pixels = (unsigned char*)malloc(width * height * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Cannot allocate memory for pixels\n");
        return 1;
    }
    
    /* Camera setup */
    vec3 camera_pos;
    vec3 camera_target;
    vec3 camera_up;
    vec3 forward;
    vec3 right;
    vec3 up;
    double fov, aspect_ratio, half_height, half_width;
    
    camera_pos.x = 0.0;
    camera_pos.y = 2.6;
    camera_pos.z = 6.5;
    
    camera_target.x = 0.0;
    camera_target.y = 1.0;
    camera_target.z = 0.0;
    
    camera_up.x = 0.0;
    camera_up.y = 1.0;
    camera_up.z = 0.0;
    
    /* Compute camera coordinate system */
    forward = vec3_sub(camera_target, camera_pos);
    forward = vec3_normalize(forward);
    right = vec3_cross(forward, camera_up);
    right = vec3_normalize(right);
    up = vec3_cross(right, forward);
    
    /* Field of view and aspect ratio */
    fov = 45.0 * M_PI / 180.0;
    aspect_ratio = (double)width / (double)height;
    half_height = tan(fov / 2.0);
    half_width = half_height * aspect_ratio;
    
    /* Render the image */
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pixel_color.x = 0.0;
            pixel_color.y = 0.0;
            pixel_color.z = 0.0;
            
            /* Subpixel sampling */
            for (sy = 0; sy < 2; sy++) {
                for (sx = 0; sx < 2; sx++) {
                    /* Compute subpixel position */
                    u = (x + (sx + 0.5) / 2.0) / width;
                    v = (y + (sy + 0.5) / 2.0) / height;
                    
                    /* Scale to [-1, 1] range */
                    s = 2.0 * u - 1.0;
                    t = 1.0 - 2.0 * v; /* Flip Y axis */
                    
                    /* Compute ray direction */
                    ray_dir = vec3_add(
                        vec3_add(vec3_mul(right, s * half_width),
                                vec3_mul(up, t * half_height)),
                        forward);
                    ray_dir = vec3_normalize(ray_dir);
                    
                    /* Create ray */
                    r_ray.origin = camera_pos;
                    r_ray.direction = ray_dir;
                    
                    /* Trace ray */
                    color = trace_ray(r_ray, 8);
                    
                    /* Gamma correction */
                    gamma_result = gamma_correct(color.x);
                    if (gamma_result < 0.0) gamma_result = 0.0;
                    if (gamma_result > 1.0) gamma_result = 1.0;
                    color.x = gamma_result;
                    
                    gamma_result = gamma_correct(color.y);
                    if (gamma_result < 0.0) gamma_result = 0.0;
                    if (gamma_result > 1.0) gamma_result = 1.0;
                    color.y = gamma_result;
                    
                    gamma_result = gamma_correct(color.z);
                    if (gamma_result < 0.0) gamma_result = 0.0;
                    if (gamma_result > 1.0) gamma_result = 1.0;
                    color.z = gamma_result;
                    
                    /* Clamp to [0, 1] */
                    if (color.x < 0.0) color.x = 0.0;
                    if (color.x > 1.0) color.x = 1.0;
                    if (color.y < 0.0) color.y = 0.0;
                    if (color.y > 1.0) color.y = 1.0;
                    if (color.z < 0.0) color.z = 0.0;
                    if (color.z > 1.0) color.z = 1.0;
                    
                    /* Convert to 8-bit and accumulate */
                    pixel_color.x += color.x * 255.0;
                    pixel_color.y += color.y * 255.0;
                    pixel_color.z += color.z * 255.0;
                }
            }
            
            /* Average the samples */
            pixel_color.x /= samples_per_pixel;
            pixel_color.y /= samples_per_pixel;
            pixel_color.z /= samples_per_pixel;
            
            /* Clamp final pixel values */
            r = (int)(pixel_color.x < 0.0 ? 0 : (pixel_color.x > 255.0 ? 255 : pixel_color.x));
            g = (int)(pixel_color.y < 0.0 ? 0 : (pixel_color.y > 255.0 ? 255 : pixel_color.y));
            b = (int)(pixel_color.z < 0.0 ? 0 : (pixel_color.z > 255.0 ? 255 : pixel_color.z));
            
            /* Store pixel in buffer (BGR order) */
            idx = (y * width + x) * 3;
            pixels[idx + 2] = (unsigned char)r; /* Blue */
            pixels[idx + 1] = (unsigned char)g; /* Green */
            pixels[idx + 0] = (unsigned char)b; /* Red */
        }
    }
    
    /* Save the image */
    save_tga("out.tga", pixels, width, height);
    
    /* Free memory */
    free(pixels);
    
    printf("Raytracer completed successfully!\n");
    printf("Generated image: out.tga\n");
    
    return 0;
}
