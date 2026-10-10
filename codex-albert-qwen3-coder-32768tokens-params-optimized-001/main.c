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
    double len;
    vec3 result;
    len = vec3_length(v);
    if (len > 1e-10) {
        result = vec3_div(v, len);
    } else {
        result.x = 0.0;
        result.y = 0.0;
        result.z = 0.0;
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

/* Ray structure */
typedef struct {
    vec3 origin;
    vec3 direction;
} ray;

/* Material structure */
typedef struct {
    vec3 albedo;
    double roughness;
    double metallic;
    double ior; /* Index of refraction */
    double transmission;
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

/* Global scene objects */
sphere spheres[2];
plane ground_plane;
light lights[3];

/* Sky color calculation */
vec3 sky_color(vec3 ray_dir) {
    vec3 dark_red;
    vec3 orange;
    double t;
    vec3 result;
    double y;
    
    dark_red.x = 0.18;
    dark_red.y = 0.01;
    dark_red.z = 0.015;
    orange.x = 1.0;
    orange.y = 0.25;
    orange.z = 0.03;
    
    /* Clamp ray direction y component to [0, 1] */
    y = ray_dir.y;
    if (y < 0.0) y = -y;
    if (y > 1.0) y = 1.0;
    
    t = pow(y, 0.65);
    result = vec3_add(vec3_mul(orange, 1.0 - t), vec3_mul(dark_red, t));
    return result;
}

/* Fog effect */
vec3 apply_fog(vec3 color, vec3 ray_dir, double distance) {
    double transmittance;
    vec3 fog_color;
    vec3 result;
    
    transmittance = exp(-0.035 * distance);
    fog_color = sky_color(ray_dir);
    result = vec3_add(vec3_mul(color, transmittance), vec3_mul(fog_color, 1.0 - transmittance));
    return result;
}

/* Fresnel reflection coefficient using Schlick's approximation */
double fresnel(double cos_theta, double eta_ratio) {
    double r0;
    double result;
    
    r0 = (1.0 - eta_ratio) / (1.0 + eta_ratio);
    r0 = r0 * r0;
    result = r0 + (1.0 - r0) * pow(1.0 - cos_theta, 5.0);
    return result;
}

/* Ray-sphere intersection */
int intersect_sphere(ray r, sphere s, double *t) {
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
        return 0; /* No intersection */
    }
    
    /* Find the nearest positive root */
    sqrt_discriminant = sqrt(discriminant);
    t1 = (-b - sqrt_discriminant) / (2.0 * a);
    t2 = (-b + sqrt_discriminant) / (2.0 * a);
    
    if (t1 > 1e-4) {
        *t = t1;
        return 1; /* Hit front face */
    } else if (t2 > 1e-4) {
        *t = t2;
        return 2; /* Hit back face (inside sphere) */
    }
    
    return 0; /* No valid intersection */
}

/* Ray-plane intersection */
int intersect_plane(ray r, plane p, double *t) {
    double denom;
    double t_val;
    
    denom = vec3_dot(p.normal, r.direction);
    if (fabs(denom) < 1e-6) {
        return 0; /* Ray is parallel to plane */
    }
    
    t_val = vec3_dot(vec3_sub(p.point, r.origin), p.normal) / denom;
    if (t_val > 1e-4) {
        *t = t_val;
        return 1;
    }
    
    return 0; /* No valid intersection */
}

/* Compute checkerboard pattern */
vec3 checkerboard_pattern(vec3 point) {
    int x, z;
    int is_black;
    vec3 result;
    
    x = (int)floor(point.x);
    z = (int)floor(point.z);
    is_black = (x + z) % 2;
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

/* Shade a point on a surface */
vec3 shade(vec3 point, vec3 normal, material mat, vec3 view_dir) {
    vec3 color;
    vec3 ambient;
    vec3 total_light;
    vec3 light_dir;
    double light_distance;
    ray shadow_ray;
    double t_shadow;
    int in_shadow;
    double diff;
    vec3 diffuse;
    vec3 reflect_dir;
    double spec;
    vec3 specular;
    int i, j;
    
    color = mat.albedo;
    
    /* Add ambient lighting */
    ambient = vec3_mul(color, 0.1);
    
    /* Direct lighting from each light */
    total_light = ambient;
    
    for (i = 0; i < 3; i++) {
        light_dir = vec3_normalize(vec3_sub(lights[i].position, point));
        light_distance = vec3_length(vec3_sub(lights[i].position, point));
        
        /* Shadow ray */
        shadow_ray.origin = vec3_add(point, vec3_mul(normal, 1e-4)); /* Offset to avoid self-intersection */
        shadow_ray.direction = light_dir;
        
        /* Check if point is in shadow */
        in_shadow = 0;
        
        /* Check intersection with spheres */
        for (j = 0; j < 2; j++) {
            if (intersect_sphere(shadow_ray, spheres[j], &t_shadow)) {
                if (t_shadow < light_distance) {
                    in_shadow = 1;
                    break;
                }
            }
        }
        
        /* Check intersection with ground plane */
        if (!in_shadow) {
            if (intersect_plane(shadow_ray, ground_plane, &t_shadow)) {
                if (t_shadow < light_distance) {
                    in_shadow = 1;
                }
            }
        }
        
        if (!in_shadow) {
            /* Diffuse lighting */
            diff = fmax(0.0, vec3_dot(normal, light_dir));
            diffuse = vec3_mul(color, diff * lights[i].intensity.x);
            
            /* Specular lighting */
            reflect_dir = vec3_normalize(vec3_sub(vec3_mul(light_dir, 2.0 * diff), light_dir));
            spec = pow(fmax(0.0, vec3_dot(view_dir, reflect_dir)), 32.0);
            specular.x = 1.0;
            specular.y = 1.0;
            specular.z = 1.0;
            specular = vec3_mul(specular, spec * lights[i].intensity.x);
            
            total_light = vec3_add(total_light, vec3_mul(vec3_add(diffuse, specular), lights[i].intensity.y));
        }
    }
    
    return total_light;
}

/* Reflect a vector */
vec3 reflect(vec3 incident, vec3 normal) {
    vec3 result;
    double dot_product;
    
    dot_product = vec3_dot(incident, normal);
    result = vec3_sub(incident, vec3_mul(normal, 2.0 * dot_product));
    return result;
}

/* Refract a vector */
vec3 refract(vec3 incident, vec3 normal, double eta_ratio) {
    double cos_i;
    double k;
    vec3 result;
    
    cos_i = vec3_dot(incident, normal);
    k = 1.0 - eta_ratio * eta_ratio * (1.0 - cos_i * cos_i);
    
    if (k < 0.0) {
        result.x = 0.0;
        result.y = 0.0;
        result.z = 0.0; /* Total internal reflection */
        return result;
    }
    
    result = vec3_sub(vec3_mul(incident, eta_ratio), vec3_mul(normal, eta_ratio * cos_i + sqrt(k)));
    return result;
}

/* Trace a ray through the scene */
vec3 trace_ray(ray r, int depth) {
    double closest_t;
    int hit_sphere;
    int hit_front_face;
    vec3 hit_point;
    vec3 hit_normal;
    material hit_material;
    vec3 view_dir;
    vec3 color;
    double reflect_weight;
    double refract_weight;
    vec3 reflected_dir;
    ray reflected_ray;
    vec3 reflected_color;
    vec3 refracted_dir;
    ray refracted_ray;
    vec3 refracted_color;
    double eta_ratio;
    int i;
    double t;
    
    if (depth <= 0) {
        return sky_color(r.direction);
    }
    
    closest_t = 1e20;
    hit_sphere = -1;
    hit_front_face = 0;
    
    /* Check intersection with spheres */
    for (i = 0; i < 2; i++) {
        if (intersect_sphere(r, spheres[i], &t) && t < closest_t) {
            closest_t = t;
            hit_sphere = i;
            hit_front_face = (t > 1e-4) ? 1 : 0;
            hit_point = vec3_add(r.origin, vec3_mul(r.direction, t));
            hit_normal = vec3_div(vec3_sub(hit_point, spheres[i].center), spheres[i].radius);
            if (!hit_front_face) {
                hit_normal = vec3_negate(hit_normal);
            }
            hit_material = spheres[i].mat;
        }
    }
    
    /* Check intersection with ground plane */
    if (intersect_plane(r, ground_plane, &t) && t < closest_t) {
        closest_t = t;
        hit_sphere = -1; /* Ground plane */
        hit_front_face = 1;
        hit_point = vec3_add(r.origin, vec3_mul(r.direction, t));
        hit_normal.x = 0.0;
        hit_normal.y = 1.0;
        hit_normal.z = 0.0;
        hit_material = ground_plane.mat;
    }
    
    /* If we hit something */
    if (hit_sphere >= 0 || hit_sphere == -1) {
        view_dir = vec3_negate(r.direction);
        if (hit_sphere == -1) {
            /* Ground plane - use checkerboard pattern */
            vec3 base_color = checkerboard_pattern(hit_point);
            color = shade(hit_point, hit_normal, hit_material, view_dir);
            color = vec3_mul(color, base_color.x);
        } else {
            /* Sphere */
            color = shade(hit_point, hit_normal, hit_material, view_dir);
        }
        
        /* Handle reflection and refraction */
        reflect_weight = 0.0;
        refract_weight = 0.0;
        
        if (hit_material.metallic > 0.0) {
            /* Metallic sphere - reflection only */
            reflect_weight = hit_material.metallic;
        } else if (hit_material.ior > 1.0) {
            /* Dielectric sphere - refraction and reflection */
            double cos_theta;
            double eta_ratio;
            double reflect_prob;
            
            cos_theta = fmax(-1.0, fmin(1.0, vec3_dot(vec3_negate(r.direction), hit_normal)));
            eta_ratio = hit_material.ior; /* Assuming air has IOR = 1.0 */
            reflect_prob = fresnel(cos_theta, eta_ratio);
            reflect_weight = reflect_prob;
            refract_weight = 1.0 - reflect_prob;
        }
        
        /* Reflection */
        if (reflect_weight > 1e-4) {
            reflected_dir = reflect(r.direction, hit_normal);
            reflected_ray.origin = vec3_add(hit_point, vec3_mul(hit_normal, 1e-4));
            reflected_ray.direction = reflected_dir;
            reflected_color = trace_ray(reflected_ray, depth - 1);
            color = vec3_add(vec3_mul(color, 1.0 - reflect_weight), vec3_mul(reflected_color, reflect_weight));
        }
        
        /* Refraction */
        if (refract_weight > 1e-4 && hit_material.ior > 1.0) {
            if (hit_front_face) {
                eta_ratio = 1.0 / hit_material.ior; /* From air to glass */
            } else {
                eta_ratio = hit_material.ior; /* From glass to air */
            }
            
            refracted_dir = refract(r.direction, hit_normal, eta_ratio);
            
            if (vec3_length(refracted_dir) > 1e-4) {
                refracted_ray.origin = vec3_add(hit_point, vec3_mul(hit_normal, -1e-4)); /* Inside sphere */
                refracted_ray.direction = refracted_dir;
                
                refracted_color = trace_ray(refracted_ray, depth - 1);
                color = vec3_add(vec3_mul(color, 1.0 - refract_weight), vec3_mul(refracted_color, refract_weight));
            }
        }
        
        /* Apply fog */
        return apply_fog(color, r.direction, closest_t);
    } else {
        /* No hit - return sky color */
        return sky_color(r.direction);
    }
}

/* Save image as TGA file */
void save_tga(const char* filename, unsigned char* pixels, int width, int height) {
    FILE* file;
    unsigned char header[18];
    int y, x;
    int idx;
    
    file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
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
    header[12] = width & 0xFF;
    header[13] = (width >> 8) & 0xFF;
    header[14] = height & 0xFF;
    header[15] = (height >> 8) & 0xFF;
    header[16] = 24;
    header[17] = 0x20;  /* 24-bit, top-left origin */
    
    fwrite(header, 1, 18, file);
    
    /* Write pixel data (BGR format) */
    for (y = height - 1; y >= 0; y--) {
        for (x = 0; x < width; x++) {
            idx = (y * width + x) * 3;
            /* Write BGR (not RGB) */
            fputc(pixels[idx + 2], file); /* Blue */
            fputc(pixels[idx + 1], file); /* Green */
            fputc(pixels[idx + 0], file); /* Red */
        }
    }
    
    fclose(file);
}

/* Gamma correction */
double gamma(double value) {
    if (value <= 0.0031308) {
        return 12.92 * value;
    } else {
        return 1.055 * pow(value, 1.0 / 2.4) - 0.055;
    }
}

int main(void) {
    /* Variables declarations at the beginning of function */
    vec3 camera_pos;
    vec3 camera_target;
    vec3 camera_up;
    vec3 forward;
    vec3 right;
    vec3 up;
    double fov;
    double aspect_ratio;
    double half_height;
    double half_width;
    unsigned char* pixels;
    int y, x, sy, sx, idx;
    vec3 color;
    double u, v;
    vec3 ray_dir;
    ray r;
    vec3 ray_color;
    int width;
    int height;
    
    /* Initialize scene */
    /* Glass sphere (left) */
    spheres[0].center.x = -1.25;
    spheres[0].center.y = 1.0;
    spheres[0].center.z = 0.0;
    spheres[0].radius = 1.0;
    spheres[0].mat.albedo.x = 1.0;
    spheres[0].mat.albedo.y = 1.0;
    spheres[0].mat.albedo.z = 1.0;
    spheres[0].mat.roughness = 0.0;
    spheres[0].mat.metallic = 0.0;
    spheres[0].mat.ior = 1.5;
    spheres[0].mat.transmission = 1.0;
    
    /* Chrome sphere (right) */
    spheres[1].center.x = 1.25;
    spheres[1].center.y = 1.0;
    spheres[1].center.z = 0.0;
    spheres[1].radius = 1.0;
    spheres[1].mat.albedo.x = 0.8;
    spheres[1].mat.albedo.y = 0.8;
    spheres[1].mat.albedo.z = 0.8;
    spheres[1].mat.roughness = 0.05;
    spheres[1].mat.metallic = 0.95;
    spheres[1].mat.ior = 1.0; /* Not used for metal */
    spheres[1].mat.transmission = 0.0;
    
    /* Ground plane */
    ground_plane.point.x = 0.0;
    ground_plane.point.y = 0.0;
    ground_plane.point.z = 0.0;
    ground_plane.normal.x = 0.0;
    ground_plane.normal.y = 1.0;
    ground_plane.normal.z = 0.0;
    ground_plane.mat.albedo.x = 0.0;
    ground_plane.mat.albedo.y = 0.0;
    ground_plane.mat.albedo.z = 0.0; /* Will be replaced by checkerboard */
    ground_plane.mat.roughness = 0.0;
    ground_plane.mat.metallic = 0.0;
    ground_plane.mat.ior = 1.0;
    ground_plane.mat.transmission = 0.0;
    
    /* Lights */
    lights[0].position.x = -3.5;
    lights[0].position.y = 6.0;
    lights[0].position.z = 4.5;
    lights[0].intensity.x = 1.0;
    lights[0].intensity.y = 1.0;
    lights[0].intensity.z = 1.0; /* Strength 1.0 */

    lights[1].position.x = 4.0;
    lights[1].position.y = 3.0;
    lights[1].position.z = 3.0;
    lights[1].intensity.x = 0.35;
    lights[1].intensity.y = 1.0;
    lights[1].intensity.z = 1.0; /* Strength 0.35 */

    lights[2].position.x = 0.5;
    lights[2].position.y = 5.0;
    lights[2].position.z = -4.0;
    lights[2].intensity.x = 0.7;
    lights[2].intensity.y = 1.0;
    lights[2].intensity.z = 1.0; /* Strength 0.7 */
    
    /* Camera setup */
    camera_pos.x = 0.0;
    camera_pos.y = 2.6;
    camera_pos.z = 6.5;
    camera_target.x = 0.0;
    camera_target.y = 1.0;
    camera_target.z = 0.0;
    camera_up.x = 0.0;
    camera_up.y = 1.0;
    camera_up.z = 0.0;
    
    fov = 45.0;
    aspect_ratio = 1280.0 / 720.0;
    
    /* Calculate camera vectors */
    forward = vec3_normalize(vec3_sub(camera_target, camera_pos));
    right = vec3_normalize(vec3_cross(forward, camera_up));
    up = vec3_cross(right, forward);
    
    /* Viewport dimensions */
    half_height = tan(fov * M_PI / 180.0 / 2.0);
    half_width = half_height * aspect_ratio;
    
    /* Allocate pixel buffer */
    width = 1280;
    height = 720;
    
    pixels = (unsigned char*)malloc(width * height * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Could not allocate pixel buffer\n");
        exit(1);
    }
    
    /* Render image */
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            color.x = 0.0;
            color.y = 0.0;
            color.z = 0.0;
            
            /* 2x2 subpixel sampling */
            for (sy = 0; sy < 2; sy++) {
                for (sx = 0; sx < 2; sx++) {
                    /* Calculate subpixel position */
                    u = (x + (sx + 0.5) / 2.0) / width;
                    v = (y + (sy + 0.5) / 2.0) / height;
                    
                    /* Calculate ray direction */
                    ray_dir = vec3_add(vec3_mul(forward, 1.0),
                                       vec3_add(vec3_mul(right, (u - 0.5) * 2.0 * half_width),
                                               vec3_mul(up, (0.5 - v) * 2.0 * half_height)));
                    ray_dir = vec3_normalize(ray_dir);
                    
                    r.origin = camera_pos;
                    r.direction = ray_dir;
                    
                    /* Trace ray */
                    ray_color = trace_ray(r, 8);
                    
                    /* Apply gamma correction and accumulate */
                    color.x += gamma(ray_color.x);
                    color.y += gamma(ray_color.y);
                    color.z += gamma(ray_color.z);
                }
            }
            
            /* Average the 4 samples */
            color.x /= 4.0;
            color.y /= 4.0;
            color.z /= 4.0;
            
            /* Clamp and convert to 8-bit */
            idx = (y * width + x) * 3;
            pixels[idx + 0] = (unsigned char)(fmin(1.0, color.x) * 255.0); /* Red */
            pixels[idx + 1] = (unsigned char)(fmin(1.0, color.y) * 255.0); /* Green */
            pixels[idx + 2] = (unsigned char)(fmin(1.0, color.z) * 255.0); /* Blue */
        }
    }
    
    /* Save image */
    save_tga("out.tga", pixels, width, height);
    
    free(pixels);
    
    return 0;
}
