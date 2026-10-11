#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Vector operations */
typedef struct {
    double x, y, z;
} Vec3;

static Vec3 vec3_add(Vec3 a, Vec3 b) {
    Vec3 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

static Vec3 vec3_sub(Vec3 a, Vec3 b) {
    Vec3 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

static Vec3 vec3_mul(Vec3 a, double s) {
    Vec3 result;
    result.x = a.x * s;
    result.y = a.y * s;
    result.z = a.z * s;
    return result;
}

static Vec3 vec3_div(Vec3 a, double s) {
    Vec3 result;
    result.x = a.x / s;
    result.y = a.y / s;
    result.z = a.z / s;
    return result;
}

static double vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 vec3_cross(Vec3 a, Vec3 b) {
    Vec3 result;
    result.x = a.y * b.z - a.z * b.y;
    result.y = a.z * b.x - a.x * b.z;
    result.z = a.x * b.y - a.y * b.x;
    return result;
}

static double vec3_length(Vec3 v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static double vec3_length_squared(Vec3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static Vec3 vec3_normalize(Vec3 v) {
    double len = vec3_length(v);
    Vec3 result;
    if (len > 1e-10) {
        result.x = v.x / len;
        result.y = v.y / len;
        result.z = v.z / len;
    } else {
        result.x = 0.0;
        result.y = 0.0;
        result.z = 0.0;
    }
    return result;
}

/* Ray structure */
typedef struct {
    Vec3 origin;
    Vec3 direction;
} Ray;

/* Material structure */
typedef struct {
    Vec3 color;
    double reflectivity;
    double transparency;
    double ior; /* Index of refraction */
} Material;

/* Sphere structure */
typedef struct {
    Vec3 center;
    double radius;
    Material material;
} Sphere;

/* Plane structure */
typedef struct {
    Vec3 point;
    Vec3 normal;
    Material material;
} Plane;

/* Light structure */
typedef struct {
    Vec3 position;
    Vec3 intensity;
} Light;

/* Global scene objects */
Sphere spheres[2];
Plane ground_plane;
Light lights[3];

/* Sky color computation */
static Vec3 sky_color(Vec3 ray_dir) {
    Vec3 dark_red;
    Vec3 orange;
    Vec3 result;
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
static Vec3 checkerboard_color(Vec3 point) {
    int x, z;
    int is_black;
    Vec3 result;
    
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

/* Ray-sphere intersection */
static int ray_sphere_intersect(Ray ray, Sphere sphere, double *t) {
    Vec3 oc;
    double a, b, c, discriminant, sqrt_discriminant, t1, t2;
    
    oc = vec3_sub(ray.origin, sphere.center);
    a = vec3_dot(ray.direction, ray.direction);
    b = 2.0 * vec3_dot(oc, ray.direction);
    c = vec3_dot(oc, oc) - sphere.radius * sphere.radius;
    discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) {
        return 0; /* No intersection */
    }
    
    sqrt_discriminant = sqrt(discriminant);
    t1 = (-b - sqrt_discriminant) / (2.0 * a);
    t2 = (-b + sqrt_discriminant) / (2.0 * a);
    
    /* Return the closest positive intersection */
    if (t1 > 1e-4) {
        *t = t1;
        return 1;
    } else if (t2 > 1e-4) {
        *t = t2;
        return 1;
    }
    
    return 0; /* Intersection behind ray origin */
}

/* Ray-plane intersection */
static int ray_plane_intersect(Ray ray, Plane plane, double *t) {
    double denom;
    Vec3 oc;
    
    denom = vec3_dot(plane.normal, ray.direction);
    if (fabs(denom) < 1e-6) {
        return 0; /* Ray is parallel to plane */
    }
    
    oc = vec3_sub(plane.point, ray.origin);
    *t = vec3_dot(oc, plane.normal) / denom;
    
    if (*t > 1e-4) {
        return 1; /* Valid intersection */
    }
    
    return 0; /* Intersection behind ray origin */
}

/* Fresnel reflection coefficient using Schlick's approximation */
static double fresnel(double cos_theta, double eta_ratio) {
    double r0;
    r0 = (1.0 - eta_ratio) / (1.0 + eta_ratio);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow(1.0 - cos_theta, 5.0);
}

/* Reflect a vector around a normal */
static Vec3 reflect(Vec3 incident, Vec3 normal) {
    Vec3 result;
    double dot_product;
    
    dot_product = vec3_dot(incident, normal);
    result = vec3_sub(incident, vec3_mul(normal, 2.0 * dot_product));
    return result;
}

/* Refract a vector using Snell's law */
static Vec3 refract(Vec3 incident, Vec3 normal, double eta_ratio) {
    double cos_i, k;
    Vec3 result;
    
    cos_i = vec3_dot(incident, normal);
    k = 1.0 - eta_ratio * eta_ratio * (1.0 - cos_i * cos_i);
    
    if (k < 0.0) {
        /* Total internal reflection */
        return reflect(incident, normal);
    }
    
    result = vec3_sub(vec3_mul(incident, eta_ratio), 
                      vec3_mul(normal, eta_ratio * cos_i + sqrt(k)));
    return result;
}

/* Shade a point with lighting */
static Vec3 shade_point(Vec3 point, Vec3 normal, Material material, int is_ground) {
    Vec3 color;
    Vec3 ambient;
    Vec3 checker_color;
    Vec3 light_intensity;
    Vec3 spec_intensity;
    Vec3 light_dir;
    Vec3 view_dir;
    Vec3 reflect_dir;
    double light_distance;
    double diffuse;
    double spec;
    int i, j;
    double shadow_t;
    int in_shadow;
    Ray shadow_ray;
    
    /* Declare all variables at the beginning of the function (C89 requirement) */
    color.x = 0.0;
    color.y = 0.0;
    color.z = 0.0;
    
    ambient.x = 0.1;
    ambient.y = 0.1;
    ambient.z = 0.1;
    
    /* For ground, use checkerboard pattern */
    if (is_ground) {
        checker_color = checkerboard_color(point);
        color.x = checker_color.x * 1.0; /* Diffuse contribution */
        color.y = checker_color.y * 1.0; /* Diffuse contribution */
        color.z = checker_color.z * 1.0; /* Diffuse contribution */
    } else {
        color.x = material.color.x * 1.0; /* Diffuse contribution */
        color.y = material.color.y * 1.0; /* Diffuse contribution */
        color.z = material.color.z * 1.0; /* Diffuse contribution */
    }
    
    /* Add ambient lighting */
    color.x += ambient.x * 0.1;
    color.y += ambient.y * 0.1;
    color.z += ambient.z * 0.1;
    
    /* Calculate lighting from each light */
    for (i = 0; i < 3; i++) {
        light_dir = vec3_normalize(vec3_sub(lights[i].position, point));
        light_distance = vec3_length(vec3_sub(lights[i].position, point));
        
        /* Shadow ray test */
        shadow_ray.origin = point;
        shadow_ray.direction = light_dir;
        in_shadow = 0;
        
        /* Test if we hit something before the light */
        for (j = 0; j < 2; j++) {
            if (j == 0) {
                /* Test sphere intersection */
                if (ray_sphere_intersect(shadow_ray, spheres[0], &shadow_t)) {
                    if (shadow_t < light_distance - 1e-4) {
                        in_shadow = 1;
                        break;
                    }
                }
                if (ray_sphere_intersect(shadow_ray, spheres[1], &shadow_t)) {
                    if (shadow_t < light_distance - 1e-4) {
                        in_shadow = 1;
                        break;
                    }
                }
            } else {
                /* Test ground intersection */
                if (ray_plane_intersect(shadow_ray, ground_plane, &shadow_t)) {
                    if (shadow_t < light_distance - 1e-4) {
                        in_shadow = 1;
                        break;
                    }
                }
            }
        }
        
        if (!in_shadow) {
            /* Diffuse lighting (Lambert) */
            diffuse = fmax(0.0, vec3_dot(normal, light_dir));
            
            /* Specular lighting (Phong) */
            /* Simplified view direction calculation */
            view_dir.x = 0.0; /* Camera position x */
            view_dir.y = 2.6; /* Camera position y */  
            view_dir.z = 6.5; /* Camera position z */
            view_dir = vec3_normalize(vec3_sub(view_dir, point));
            view_dir.x = -view_dir.x; /* Ray direction inverted */
            view_dir.y = -view_dir.y; /* Ray direction inverted */
            view_dir.z = -view_dir.z; /* Ray direction inverted */
            reflect_dir = reflect(light_dir, normal);
            spec = pow(fmax(0.0, vec3_dot(view_dir, reflect_dir)), 32.0);
            
            light_intensity.x = lights[i].intensity.x * diffuse;
            light_intensity.y = lights[i].intensity.y * diffuse;
            light_intensity.z = lights[i].intensity.z * diffuse;
            color.x += light_intensity.x;
            color.y += light_intensity.y;
            color.z += light_intensity.z;
            
            /* Add specular component for spheres */
            if (!is_ground) {
                spec_intensity.x = lights[i].intensity.x * spec * 0.5;
                spec_intensity.y = lights[i].intensity.y * spec * 0.5;
                spec_intensity.z = lights[i].intensity.z * spec * 0.5;
                color.x += spec_intensity.x;
                color.y += spec_intensity.y;
                color.z += spec_intensity.z;
            }
        }
    }
    
    return color;
}

/* Recursive ray tracing function */
static Vec3 trace_ray(Ray ray, int depth) {
    double closest_t;
    Vec3 hit_point;
    Vec3 hit_normal;
    Material hit_material;
    Vec3 shaded_color;
    Vec3 reflect_color;
    Vec3 refract_color;
    Vec3 reflect_dir;
    Vec3 refract_dir;
    double eta_ratio;
    double cos_i;
    int inside;
    Ray reflect_ray;
    Ray refract_ray;
    Vec3 offset;
    Vec3 result;
    double fog_factor;
    Vec3 fog_color;
    
    /* Declare all variables at the beginning of the function (C89 requirement) */
    closest_t = 1e20;
    
    if (depth > 8) {
        return sky_color(ray.direction);
    }
    
    /* Test sphere intersections */
    {
        int i;
        double t;
        for (i = 0; i < 2; i++) {
            if (ray_sphere_intersect(ray, spheres[i], &t) && t < closest_t) {
                closest_t = t;
                hit_point = vec3_add(ray.origin, vec3_mul(ray.direction, t));
                
                /* Calculate normal */
                hit_normal = vec3_normalize(vec3_sub(hit_point, spheres[i].center));
                hit_material = spheres[i].material;
            }
        }
    }
    
    /* Test ground intersection */
    {
        double t;
        if (ray_plane_intersect(ray, ground_plane, &t) && t < closest_t) {
            closest_t = t;
            hit_point = vec3_add(ray.origin, vec3_mul(ray.direction, t));
            
            /* Ground normal is always up */
            hit_normal.x = 0.0;
            hit_normal.y = 1.0;
            hit_normal.z = 0.0;
            hit_material = ground_plane.material;
        }
    }
    
    /* If nothing hit, return sky color */
    if (closest_t == 1e20) {
        return sky_color(ray.direction);
    }
    
    /* Shading */
    shaded_color = shade_point(hit_point, hit_normal, hit_material, 0);
    
    /* Reflection and refraction */
    reflect_color.x = 0.0;
    reflect_color.y = 0.0;
    reflect_color.z = 0.0;
    refract_color.x = 0.0;
    refract_color.y = 0.0;
    refract_color.z = 0.0;
    
    if (hit_material.reflectivity > 0.0 || hit_material.transparency > 0.0) {
        reflect_dir = reflect(ray.direction, hit_normal);
        eta_ratio = 1.0; /* Default to air */
        cos_i = vec3_dot(ray.direction, hit_normal);
        inside = 0;
        
        /* Determine if ray is entering or exiting */
        if (cos_i < 0.0) {
            /* Entering */
            eta_ratio = hit_material.ior;
        } else {
            /* Exiting */
            eta_ratio = 1.0 / hit_material.ior;
            inside = 1;
            /* Reverse normal for exiting */
            hit_normal.x = -hit_normal.x;
            hit_normal.y = -hit_normal.y;
            hit_normal.z = -hit_normal.z;
        }
        
        /* Calculate Fresnel coefficient */
        /* Note: We don't use cos_t in this implementation but it's declared for consistency */
        
        /* Reflection */
        if (hit_material.reflectivity > 0.0) {
            offset = vec3_mul(hit_normal, 1e-4);
            reflect_ray.origin = vec3_add(hit_point, offset);
            reflect_ray.direction = reflect_dir;
            reflect_color = trace_ray(reflect_ray, depth + 1);
            reflect_color = vec3_mul(reflect_color, hit_material.reflectivity);
        }
        
        /* Refraction */
        if (hit_material.transparency > 0.0 && eta_ratio != 1.0) {
            refract_dir = refract(ray.direction, hit_normal, eta_ratio);
            
            /* Offset the ray origin to avoid self-intersection */
            offset = vec3_mul(hit_normal, 1e-4);
            if (inside) {
                offset.x = -offset.x;
                offset.y = -offset.y;
                offset.z = -offset.z;
            }
            refract_ray.origin = vec3_add(hit_point, offset);
            refract_ray.direction = refract_dir;
            
            refract_color = trace_ray(refract_ray, depth + 1);
            refract_color = vec3_mul(refract_color, hit_material.transparency);
        }
    }
    
    /* Combine colors */
    result = vec3_add(shaded_color, reflect_color);
    result = vec3_add(result, refract_color);
    
    /* Apply fog */
    fog_factor = exp(-0.035 * closest_t);
    fog_color = sky_color(ray.direction);
    result.x = fog_factor * result.x + (1.0 - fog_factor) * fog_color.x;
    result.y = fog_factor * result.y + (1.0 - fog_factor) * fog_color.y;
    result.z = fog_factor * result.z + (1.0 - fog_factor) * fog_color.z;
    
    return result;
}

/* Convert linear RGB to sRGB gamma corrected */
static unsigned char gamma_correct(double c) {
    if (c <= 0.0031308) {
        return (unsigned char)(c * 12.92 * 255.0 + 0.5);
    } else {
        return (unsigned char)((1.055 * pow(c, 1.0 / 2.4) - 0.055) * 255.0 + 0.5);
    }
}

/* Save TGA file */
static int save_tga(const char* filename, unsigned char* buffer, int width, int height) {
    FILE* file;
    unsigned char header[18];
    int y, x, idx;
    unsigned char b, g, r;
    
    file = fopen(filename, "wb");
    if (!file) {
        return 0;
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
    header[17] = 0x20;
    
    if (fwrite(header, 1, 18, file) != 18) {
        fclose(file);
        return 0;
    }
    
    /* Write pixel data (BGR format) */
    for (y = height - 1; y >= 0; y--) {
        for (x = 0; x < width; x++) {
            idx = (y * width + x) * 3;
            b = buffer[idx];
            g = buffer[idx + 1];
            r = buffer[idx + 2];
            
            if (fwrite(&b, 1, 1, file) != 1 ||
                fwrite(&g, 1, 1, file) != 1 ||
                fwrite(&r, 1, 1, file) != 1) {
                fclose(file);
                return 0;
            }
        }
    }
    
    fclose(file);
    return 1;
}

int main(void) {
    /* All variables declared at the beginning of the function (C89 requirement) */
    /* Scene initialization */
    /* Glass sphere */
    spheres[0].center.x = -1.25;
    spheres[0].center.y = 1.0;
    spheres[0].center.z = 0.0;
    spheres[0].radius = 1.0;
    spheres[0].material.color.x = 1.0; /* Clear */
    spheres[0].material.color.y = 1.0; /* Clear */
    spheres[0].material.color.z = 1.0; /* Clear */
    spheres[0].material.reflectivity = 0.0;
    spheres[0].material.transparency = 1.0;
    spheres[0].material.ior = 1.5;
    
    /* Chrome sphere */
    spheres[1].center.x = 1.25;
    spheres[1].center.y = 1.0;
    spheres[1].center.z = 0.0;
    spheres[1].radius = 1.0;
    spheres[1].material.color.x = 0.8; /* Silver */
    spheres[1].material.color.y = 0.8; /* Silver */
    spheres[1].material.color.z = 0.8; /* Silver */
    spheres[1].material.reflectivity = 0.95;
    spheres[1].material.transparency = 0.0;
    spheres[1].material.ior = 1.0;
    
    /* Ground plane */
    ground_plane.point.x = 0.0;
    ground_plane.point.y = 0.0;
    ground_plane.point.z = 0.0;
    ground_plane.normal.x = 0.0;
    ground_plane.normal.y = 1.0;
    ground_plane.normal.z = 0.0;
    ground_plane.material.color.x = 0.0; /* Will be replaced by checkerboard */
    ground_plane.material.color.y = 0.0; /* Will be replaced by checkerboard */
    ground_plane.material.color.z = 0.0; /* Will be replaced by checkerboard */
    ground_plane.material.reflectivity = 0.0;
    ground_plane.material.transparency = 0.0;
    ground_plane.material.ior = 1.0;
    
    /* Lights */
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
    
    /* Rendering variables */
    Vec3 camera_pos;
    Vec3 camera_target;
    Vec3 camera_up;
    Vec3 camera_forward;
    Vec3 camera_right;
    Vec3 camera_up_vec;
    double fov;
    double aspect_ratio;
    double viewport_height;
    double viewport_width;
    int width, height;
    int x, y, sx, sy;
    double u, v, screen_x, screen_y;
    Vec3 pixel_color;
    Vec3 ray_dir;
    Ray ray;
    Vec3 color;
    unsigned char* buffer;
    int idx;
    
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
    
    /* Compute camera vectors */
    camera_forward = vec3_normalize(vec3_sub(camera_target, camera_pos));
    camera_right = vec3_normalize(vec3_cross(camera_forward, camera_up));
    camera_up_vec = vec3_cross(camera_right, camera_forward);
    
    /* Image dimensions */
    width = 1280;
    height = 720;
    
    /* Allocate pixel buffer */
    buffer = (unsigned char*)malloc(width * height * 3);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    /* Viewport dimensions (aspect ratio 1280:720) */
    fov = 45.0 * M_PI / 180.0;
    aspect_ratio = (double)width / (double)height;
    viewport_height = 2.0 * tan(fov / 2.0);
    viewport_width = viewport_height * aspect_ratio;
    
    /* Render image with supersampling */
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pixel_color.x = 0.0;
            pixel_color.y = 0.0;
            pixel_color.z = 0.0;
            
            /* 2x2 subpixel sampling */
            for (sy = 0; sy < 2; sy++) {
                for (sx = 0; sx < 2; sx++) {
                    /* Calculate subpixel position */
                    u = (x + (sx + 0.5) / 2.0) / width;
                    v = (y + (sy + 0.5) / 2.0) / height;
                    
                    /* Scale to viewport */
                    screen_x = (u - 0.5) * viewport_width;
                    screen_y = (0.5 - v) * viewport_height;
                    
                    /* Ray direction */
                    ray_dir = vec3_normalize(
                        vec3_add(vec3_mul(camera_right, screen_x),
                                vec3_add(vec3_mul(camera_up_vec, screen_y),
                                        camera_forward)));
                    
                    ray.origin = camera_pos;
                    ray.direction = ray_dir;
                    color = trace_ray(ray, 0);
                    
                    /* Accumulate color */
                    pixel_color.x += color.x;
                    pixel_color.y += color.y;
                    pixel_color.z += color.z;
                }
            }
            
            /* Average the subpixels */
            pixel_color = vec3_div(pixel_color, 4.0);
            
            /* Convert to 8-bit and store in buffer (BGR order) */
            idx = (y * width + x) * 3;
            buffer[idx] = gamma_correct(pixel_color.z);     /* Blue */
            buffer[idx + 1] = gamma_correct(pixel_color.y); /* Green */
            buffer[idx + 2] = gamma_correct(pixel_color.x); /* Red */
        }
    }
    
    /* Save the image */
    if (!save_tga("out.tga", buffer, width, height)) {
        fprintf(stderr, "Failed to save TGA file\n");
        free(buffer);
        return 1;
    }
    
    free(buffer);
    printf("Raytracer completed successfully. Image saved as out.tga\n");
    return 0;
}
