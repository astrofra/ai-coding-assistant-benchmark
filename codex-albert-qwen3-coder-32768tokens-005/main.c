#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Vector operations */
typedef struct {
    double x, y, z;
} Vec3;

/* Ray structure */
typedef struct {
    Vec3 origin;
    Vec3 direction;
} Ray;

/* Material structure */
typedef struct {
    Vec3 albedo;
    double roughness;
    double metallic;
    double ior; /* Index of refraction */
    double transmission; /* Transmission coefficient */
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

/* Scene structure */
typedef struct {
    Sphere spheres[2];
    Plane ground;
    Light lights[3];
    Vec3 camera_pos;
    Vec3 camera_target;
    Vec3 camera_up;
    double fov_y;
    double aspect_ratio;
} Scene;

/* Sky color calculation */
static Vec3 sky_color(Vec3 ray_dir) {
    Vec3 dark_red, orange;
    double t;
    
    dark_red.x = 0.18;
    dark_red.y = 0.01;
    dark_red.z = 0.015;
    
    orange.x = 1.0;
    orange.y = 0.25;
    orange.z = 0.03;
    
    /* Clamp y to [0, 1] for numerical robustness */
    double y = ray_dir.y;
    if (y < 0.0) y = -y;
    if (y > 1.0) y = 1.0;
    
    t = pow(y, 0.65);
    Vec3 result;
    result.x = (1.0 - t) * dark_red.x + t * orange.x;
    result.y = (1.0 - t) * dark_red.y + t * orange.y;
    result.z = (1.0 - t) * dark_red.z + t * orange.z;
    return result;
}

/* Checkerboard pattern */
static Vec3 checkerboard_pattern(double x, double z) {
    int ix, iz;
    Vec3 result;
    
    ix = (int)floor(x);
    iz = (int)floor(z);
    if ((ix + iz) % 2 == 0) {
        result.x = 1.0;
        result.y = 1.0;
        result.z = 1.0; /* White */
    } else {
        result.x = 0.0;
        result.y = 0.0;
        result.z = 0.0; /* Black */
    }
    return result;
}

/* Ray-sphere intersection */
static int ray_sphere_intersect(Ray ray, Sphere sphere, double *t) {
    Vec3 oc;
    double a, b, c, discriminant, sqrt_discriminant;
    
    oc.x = ray.origin.x - sphere.center.x;
    oc.y = ray.origin.y - sphere.center.y;
    oc.z = ray.origin.z - sphere.center.z;
    
    a = ray.direction.x * ray.direction.x + ray.direction.y * ray.direction.y + ray.direction.z * ray.direction.z;
    b = 2.0 * (oc.x * ray.direction.x + oc.y * ray.direction.y + oc.z * ray.direction.z);
    c = oc.x * oc.x + oc.y * oc.y + oc.z * oc.z - sphere.radius * sphere.radius;
    discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) {
        return 0;
    }
    
    sqrt_discriminant = sqrt(discriminant);
    double t1 = (-b - sqrt_discriminant) / (2.0 * a);
    double t2 = (-b + sqrt_discriminant) / (2.0 * a);
    
    /* Return the closest positive intersection */
    if (t1 > 1e-4) {
        *t = t1;
        return 1;
    } else if (t2 > 1e-4) {
        *t = t2;
        return 1;
    }
    
    return 0;
}

/* Ray-plane intersection */
static int ray_plane_intersect(Ray ray, Plane plane, double *t) {
    double denom;
    Vec3 op;
    
    denom = plane.normal.x * ray.direction.x + plane.normal.y * ray.direction.y + plane.normal.z * ray.direction.z;
    if (fabs(denom) < 1e-6) {
        return 0; /* Ray is parallel to the plane */
    }
    
    op.x = plane.point.x - ray.origin.x;
    op.y = plane.point.y - ray.origin.y;
    op.z = plane.point.z - ray.origin.z;
    
    *t = (op.x * plane.normal.x + op.y * plane.normal.y + op.z * plane.normal.z) / denom;
    
    if (*t > 1e-4) {
        return 1;
    }
    
    return 0;
}

/* Fresnel reflection coefficient (Schlick's approximation) */
static double fresnel(double cos_theta, double eta) {
    double r0;
    r0 = (1.0 - eta) / (1.0 + eta);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow(1.0 - cos_theta, 5.0);
}

/* Reflect a vector */
static Vec3 reflect(Vec3 I, Vec3 N) {
    Vec3 result;
    double temp;
    
    temp = 2.0 * (I.x * N.x + I.y * N.y + I.z * N.z);
    result.x = I.x - temp * N.x;
    result.y = I.y - temp * N.y;
    result.z = I.z - temp * N.z;
    return result;
}

/* Refract a vector */
static Vec3 refract(Vec3 I, Vec3 N, double eta) {
    double k;
    Vec3 result;
    
    k = 1.0 - eta * eta * (1.0 - (I.x * N.x + I.y * N.y + I.z * N.z) * (I.x * N.x + I.y * N.y + I.z * N.z));
    if (k < 0.0) {
        result.x = 0.0;
        result.y = 0.0;
        result.z = 0.0; /* Total internal reflection */
        return result;
    }
    double temp = eta * (I.x * N.x + I.y * N.y + I.z * N.z) + sqrt(k);
    result.x = eta * I.x - temp * N.x;
    result.y = eta * I.y - temp * N.y;
    result.z = eta * I.z - temp * N.z;
    return result;
}

/* Trace a ray through the scene */
static Vec3 trace_ray(Ray ray, Scene scene, int depth) {
    double closest_t;
    int hit_sphere;
    int hit_ground;
    double ground_t;
    Vec3 color;
    Vec3 normal;
    Vec3 hit_point;
    double cos_theta;
    int entering;
    Vec3 reflect_dir;
    Vec3 refract_dir;
    double reflect_weight;
    double refract_weight;
    Vec3 albedo;
    Vec3 light_dir;
    Ray shadow_ray;
    double shadow_t;
    int in_shadow;
    double diffuse;
    double specular;
    Vec3 light_contribution;
    Vec3 fog_color;
    double transmittance;
    Vec3 temp_vec1, temp_vec2;
    double temp_double1, temp_double2;
    
    if (depth > 8) {
        return sky_color(ray.direction);
    }
    
    closest_t = 1e20;
    hit_sphere = -1;
    hit_ground = 0;
    ground_t = 0.0;
    
    /* Test sphere intersections */
    {
        int i;
        for (i = 0; i < 2; i++) {
            double t;
            if (ray_sphere_intersect(ray, scene.spheres[i], &t) && t < closest_t) {
                closest_t = t;
                hit_sphere = i;
            }
        }
    }
    
    /* Test ground intersection */
    if (ray_plane_intersect(ray, scene.ground, &ground_t)) {
        if (ground_t < closest_t) {
            closest_t = ground_t;
            hit_ground = 1;
        }
    }
    
    /* No intersection */
    if (closest_t > 1e19) {
        return sky_color(ray.direction);
    }
    
    /* Hit something */
    hit_point.x = ray.origin.x + ray.direction.x * closest_t;
    hit_point.y = ray.origin.y + ray.direction.y * closest_t;
    hit_point.z = ray.origin.z + ray.direction.z * closest_t;
    
    if (hit_ground) {
        /* Ground intersection */
        albedo = checkerboard_pattern(hit_point.x, hit_point.z);
        normal = scene.ground.normal;
        
        /* Simple diffuse lighting */
        color.x = albedo.x;
        color.y = albedo.y;
        color.z = albedo.z;
        
        {
            int i;
            for (i = 0; i < 3; i++) {
                Light light = scene.lights[i];
                light_dir.x = light.position.x - hit_point.x;
                light_dir.y = light.position.y - hit_point.y;
                light_dir.z = light.position.z - hit_point.z;
                
                /* Normalize light direction */
                temp_double1 = sqrt(light_dir.x * light_dir.x + light_dir.y * light_dir.y + light_dir.z * light_dir.z);
                if (temp_double1 > 1e-10) {
                    light_dir.x /= temp_double1;
                    light_dir.y /= temp_double1;
                    light_dir.z /= temp_double1;
                }
                
                /* Shadow test */
                shadow_ray.origin = hit_point;
                shadow_ray.direction = light_dir;
                shadow_t = 0.0;
                in_shadow = 0;
                
                /* Check if we hit any sphere before reaching the light */
                {
                    int j;
                    for (j = 0; j < 2; j++) {
                        if (ray_sphere_intersect(shadow_ray, scene.spheres[j], &shadow_t) && 
                            shadow_t < sqrt((light.position.x - hit_point.x) * (light.position.x - hit_point.x) +
                                            (light.position.y - hit_point.y) * (light.position.y - hit_point.y) +
                                            (light.position.z - hit_point.z) * (light.position.z - hit_point.z))) {
                            in_shadow = 1;
                            break;
                        }
                    }
                }
                
                if (!in_shadow) {
                    diffuse = fmax(0.0, normal.x * light_dir.x + normal.y * light_dir.y + normal.z * light_dir.z);
                    color.x += albedo.x * diffuse * light.intensity.x;
                    color.y += albedo.y * diffuse * light.intensity.y;
                    color.z += albedo.z * diffuse * light.intensity.z;
                }
            }
        }
        
        /* Fog effect */
        fog_color = sky_color(ray.direction);
        transmittance = exp(-0.035 * closest_t);
        color.x = transmittance * color.x + (1.0 - transmittance) * fog_color.x;
        color.y = transmittance * color.y + (1.0 - transmittance) * fog_color.y;
        color.z = transmittance * color.z + (1.0 - transmittance) * fog_color.z;
        
        return color;
    } else {
        /* Sphere intersection */
        Material material = scene.spheres[hit_sphere].material;
        hit_point.x = ray.origin.x + ray.direction.x * closest_t;
        hit_point.y = ray.origin.y + ray.direction.y * closest_t;
        hit_point.z = ray.origin.z + ray.direction.z * closest_t;
        
        /* Calculate normal */
        if (hit_sphere == 0) {
            /* Glass sphere */
            normal.x = hit_point.x - scene.spheres[hit_sphere].center.x;
            normal.y = hit_point.y - scene.spheres[hit_sphere].center.y;
            normal.z = hit_point.z - scene.spheres[hit_sphere].center.z;
        } else {
            /* Chrome sphere */
            normal.x = hit_point.x - scene.spheres[hit_sphere].center.x;
            normal.y = hit_point.y - scene.spheres[hit_sphere].center.y;
            normal.z = hit_point.z - scene.spheres[hit_sphere].center.z;
        }
        
        /* Normalize normal */
        temp_double1 = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (temp_double1 > 1e-10) {
            normal.x /= temp_double1;
            normal.y /= temp_double1;
            normal.z /= temp_double1;
        }
        
        /* Determine if ray is entering or exiting */
        cos_theta = ray.direction.x * normal.x + ray.direction.y * normal.y + ray.direction.z * normal.z;
        entering = (cos_theta < 0);
        if (!entering) {
            normal.x = -normal.x;
            normal.y = -normal.y;
            normal.z = -normal.z;
            cos_theta = -cos_theta;
        }
        
        /* Reflection and refraction */
        reflect_dir.x = ray.direction.x - 2.0 * (ray.direction.x * normal.x + ray.direction.y * normal.y + ray.direction.z * normal.z) * normal.x;
        reflect_dir.y = ray.direction.y - 2.0 * (ray.direction.x * normal.x + ray.direction.y * normal.y + ray.direction.z * normal.z) * normal.y;
        reflect_dir.z = ray.direction.z - 2.0 * (ray.direction.x * normal.x + ray.direction.y * normal.y + ray.direction.z * normal.z) * normal.z;
        
        refract_dir.x = 0.0;
        refract_dir.y = 0.0;
        refract_dir.z = 0.0;
        reflect_weight = 0.0;
        refract_weight = 0.0;
        
        if (material.ior > 1.0) {
            /* Dielectric (glass) */
            double eta = entering ? 1.0 / material.ior : material.ior;
            refract_dir = refract(ray.direction, normal, eta);
            
            /* Fresnel coefficient */
            double f = fresnel(cos_theta, eta);
            reflect_weight = f;
            refract_weight = 1.0 - f;
        } else {
            /* Metallic (chrome) */
            reflect_weight = 0.95;
            refract_weight = 0.0;
        }
        
        color.x = 0.0;
        color.y = 0.0;
        color.z = 0.0;
        
        /* Reflection */
        if (reflect_weight > 0.0) {
            Ray reflect_ray;
            reflect_ray.origin = hit_point;
            reflect_ray.origin.x += normal.x * 1e-4;
            reflect_ray.origin.y += normal.y * 1e-4;
            reflect_ray.origin.z += normal.z * 1e-4;
            reflect_ray.direction = reflect_dir;
            Vec3 reflect_color = trace_ray(reflect_ray, scene, depth + 1);
            color.x += reflect_color.x * reflect_weight;
            color.y += reflect_color.y * reflect_weight;
            color.z += reflect_color.z * reflect_weight;
        }
        
        /* Refraction */
        if (refract_weight > 0.0 && (sqrt(refract_dir.x * refract_dir.x + refract_dir.y * refract_dir.y + refract_dir.z * refract_dir.z) > 1e-10)) {
            Ray refract_ray;
            refract_ray.origin = hit_point;
            refract_ray.origin.x -= normal.x * 1e-4;
            refract_ray.origin.y -= normal.y * 1e-4;
            refract_ray.origin.z -= normal.z * 1e-4;
            refract_ray.direction = refract_dir;
            Vec3 refract_color = trace_ray(refract_ray, scene, depth + 1);
            color.x += refract_color.x * refract_weight;
            color.y += refract_color.y * refract_weight;
            color.z += refract_color.z * refract_weight;
        }
        
        /* Direct lighting */
        if (material.ior <= 1.0) { /* Metallic */
            albedo = material.albedo;
            {
                int i;
                for (i = 0; i < 3; i++) {
                    Light light = scene.lights[i];
                    light_dir.x = light.position.x - hit_point.x;
                    light_dir.y = light.position.y - hit_point.y;
                    light_dir.z = light.position.z - hit_point.z;
                    
                    /* Normalize light direction */
                    temp_double1 = sqrt(light_dir.x * light_dir.x + light_dir.y * light_dir.y + light_dir.z * light_dir.z);
                    if (temp_double1 > 1e-10) {
                        light_dir.x /= temp_double1;
                        light_dir.y /= temp_double1;
                        light_dir.z /= temp_double1;
                    }
                    
                    /* Shadow test */
                    shadow_ray.origin = hit_point;
                    shadow_ray.direction = light_dir;
                    shadow_t = 0.0;
                    in_shadow = 0;
                    
                    /* Check if we hit any sphere before reaching the light */
                    {
                        int j;
                        for (j = 0; j < 2; j++) {
                            if (j != hit_sphere && ray_sphere_intersect(shadow_ray, scene.spheres[j], &shadow_t) && 
                                shadow_t < sqrt((light.position.x - hit_point.x) * (light.position.x - hit_point.x) +
                                                (light.position.y - hit_point.y) * (light.position.y - hit_point.y) +
                                                (light.position.z - hit_point.z) * (light.position.z - hit_point.z))) {
                                in_shadow = 1;
                                break;
                            }
                        }
                    }
                    
                    if (!in_shadow) {
                        /* Simple lighting model for chrome */
                        diffuse = fmax(0.0, normal.x * light_dir.x + normal.y * light_dir.y + normal.z * light_dir.z);
                        specular = pow(fmax(0.0, reflect_dir.x * light_dir.x + reflect_dir.y * light_dir.y + reflect_dir.z * light_dir.z), 32.0);
                        light_contribution.x = albedo.x * diffuse * light.intensity.x;
                        light_contribution.y = albedo.y * diffuse * light.intensity.y;
                        light_contribution.z = albedo.z * diffuse * light.intensity.z;
                        light_contribution.x += 1.0 * specular * light.intensity.x;
                        light_contribution.y += 1.0 * specular * light.intensity.y;
                        light_contribution.z += 1.0 * specular * light.intensity.z;
                        color.x += light_contribution.x;
                        color.y += light_contribution.y;
                        color.z += light_contribution.z;
                    }
                }
            }
        } else {
            /* Glass: simple lighting */
            albedo = material.albedo;
            {
                int i;
                for (i = 0; i < 3; i++) {
                    Light light = scene.lights[i];
                    light_dir.x = light.position.x - hit_point.x;
                    light_dir.y = light.position.y - hit_point.y;
                    light_dir.z = light.position.z - hit_point.z;
                    
                    /* Normalize light direction */
                    temp_double1 = sqrt(light_dir.x * light_dir.x + light_dir.y * light_dir.y + light_dir.z * light_dir.z);
                    if (temp_double1 > 1e-10) {
                        light_dir.x /= temp_double1;
                        light_dir.y /= temp_double1;
                        light_dir.z /= temp_double1;
                    }
                    
                    /* Shadow test */
                    shadow_ray.origin = hit_point;
                    shadow_ray.direction = light_dir;
                    shadow_t = 0.0;
                    in_shadow = 0;
                    
                    /* Check if we hit any sphere before reaching the light */
                    {
                        int j;
                        for (j = 0; j < 2; j++) {
                            if (j != hit_sphere && ray_sphere_intersect(shadow_ray, scene.spheres[j], &shadow_t) && 
                                shadow_t < sqrt((light.position.x - hit_point.x) * (light.position.x - hit_point.x) +
                                                (light.position.y - hit_point.y) * (light.position.y - hit_point.y) +
                                                (light.position.z - hit_point.z) * (light.position.z - hit_point.z))) {
                                in_shadow = 1;
                                break;
                            }
                        }
                    }
                    
                    if (!in_shadow) {
                        diffuse = fmax(0.0, normal.x * light_dir.x + normal.y * light_dir.y + normal.z * light_dir.z);
                        color.x += albedo.x * diffuse * light.intensity.x;
                        color.y += albedo.y * diffuse * light.intensity.y;
                        color.z += albedo.z * diffuse * light.intensity.z;
                    }
                }
            }
        }
        
        /* Fog effect */
        fog_color = sky_color(ray.direction);
        transmittance = exp(-0.035 * closest_t);
        color.x = transmittance * color.x + (1.0 - transmittance) * fog_color.x;
        color.y = transmittance * color.y + (1.0 - transmittance) * fog_color.y;
        color.z = transmittance * color.z + (1.0 - transmittance) * fog_color.z;
        
        return color;
    }
}

/* Convert RGB to sRGB gamma corrected */
static unsigned char gamma_correct(double c) {
    if (c <= 0.0031308) {
        return (unsigned char)(c * 12.92 * 255.0 + 0.5);
    } else {
        return (unsigned char)((1.055 * pow(c, 1.0/2.4) - 0.055) * 255.0 + 0.5);
    }
}

/* Write TGA file */
static int write_tga(const char *filename, unsigned char *pixels, int width, int height) {
    FILE *fp;
    unsigned char header[18];
    int y, x, idx;
    unsigned char b, g, r;
    
    fp = fopen(filename, "wb");
    if (!fp) {
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
    header[17] = 0x20;  /* 24-bit, top-left origin */
    
    fwrite(header, 1, 18, fp);
    
    /* Write pixel data (BGR order) */
    for (y = height - 1; y >= 0; y--) {
        for (x = 0; x < width; x++) {
            idx = (y * width + x) * 3;
            b = pixels[idx];
            g = pixels[idx + 1];
            r = pixels[idx + 2];
            fwrite(&b, 1, 1, fp);
            fwrite(&g, 1, 1, fp);
            fwrite(&r, 1, 1, fp);
        }
    }
    
    fclose(fp);
    return 1;
}

int main(void) {
    /* Initialize scene */
    Scene scene;
    Vec3 camera_forward, camera_right, camera_up_vec;
    double ndc_x, ndc_y, scale_x, scale_y;
    double px, py;
    Vec3 ray_dir;
    Ray ray;
    Vec3 color;
    int num_samples;
    int dy, dx;
    int y, x;
    unsigned char *pixels;
    int width, height;
    int idx;
    unsigned char b, g, r;
    double temp_double1;
    double temp_double2;
    double temp_double3;
    Vec3 temp_vec1, temp_vec2;
    Vec3 temp_vec3;
    
    /* Camera setup */
    scene.camera_pos.x = 0.0;
    scene.camera_pos.y = 2.6;
    scene.camera_pos.z = 6.5;
    
    scene.camera_target.x = 0.0;
    scene.camera_target.y = 1.0;
    scene.camera_target.z = 0.0;
    
    scene.camera_up.x = 0.0;
    scene.camera_up.y = 1.0;
    scene.camera_up.z = 0.0;
    
    scene.fov_y = 45.0 * M_PI / 180.0;
    scene.aspect_ratio = 1280.0 / 720.0;
    
    /* Create spheres */
    /* Glass sphere */
    scene.spheres[0].center.x = -1.25;
    scene.spheres[0].center.y = 1.0;
    scene.spheres[0].center.z = 0.0;
    scene.spheres[0].radius = 1.0;
    scene.spheres[0].material.albedo.x = 1.0;
    scene.spheres[0].material.albedo.y = 1.0;
    scene.spheres[0].material.albedo.z = 1.0;
    scene.spheres[0].material.roughness = 0.0;
    scene.spheres[0].material.metallic = 0.0;
    scene.spheres[0].material.ior = 1.5;
    scene.spheres[0].material.transmission = 1.0;
    
    /* Chrome sphere */
    scene.spheres[1].center.x = 1.25;
    scene.spheres[1].center.y = 1.0;
    scene.spheres[1].center.z = 0.0;
    scene.spheres[1].radius = 1.0;
    scene.spheres[1].material.albedo.x = 0.8;
    scene.spheres[1].material.albedo.y = 0.8;
    scene.spheres[1].material.albedo.z = 0.8;
    scene.spheres[1].material.roughness = 0.0;
    scene.spheres[1].material.metallic = 1.0;
    scene.spheres[1].material.ior = 0.0;
    scene.spheres[1].material.transmission = 0.0;
    
    /* Ground plane */
    scene.ground.point.x = 0.0;
    scene.ground.point.y = 0.0;
    scene.ground.point.z = 0.0;
    scene.ground.normal.x = 0.0;
    scene.ground.normal.y = 1.0;
    scene.ground.normal.z = 0.0;
    scene.ground.material.albedo.x = 0.0;
    scene.ground.material.albedo.y = 0.0;
    scene.ground.material.albedo.z = 0.0;
    scene.ground.material.roughness = 0.0;
    scene.ground.material.metallic = 0.0;
    scene.ground.material.ior = 0.0;
    scene.ground.material.transmission = 0.0;
    
    /* Lights */
    scene.lights[0].position.x = -3.5;
    scene.lights[0].position.y = 6.0;
    scene.lights[0].position.z = 4.5;
    scene.lights[0].intensity.x = 1.0;
    scene.lights[0].intensity.y = 1.0;
    scene.lights[0].intensity.z = 1.0;
    
    scene.lights[1].position.x = 4.0;
    scene.lights[1].position.y = 3.0;
    scene.lights[1].position.z = 3.0;
    scene.lights[1].intensity.x = 0.35;
    scene.lights[1].intensity.y = 0.35;
    scene.lights[1].intensity.z = 0.35;
    
    scene.lights[2].position.x = 0.5;
    scene.lights[2].position.y = 5.0;
    scene.lights[2].position.z = -4.0;
    scene.lights[2].intensity.x = 0.7;
    scene.lights[2].intensity.y = 0.7;
    scene.lights[2].intensity.z = 0.7;
    
    /* Allocate pixel buffer */
    width = 1280;
    height = 720;
    pixels = malloc(width * height * 3);
    if (!pixels) {
        fprintf(stderr, "Failed to allocate pixel buffer\n");
        return 1;
    }
    
    /* Compute camera vectors */
    camera_forward.x = scene.camera_target.x - scene.camera_pos.x;
    camera_forward.y = scene.camera_target.y - scene.camera_pos.y;
    camera_forward.z = scene.camera_target.z - scene.camera_pos.z;
    
    temp_double1 = sqrt(camera_forward.x * camera_forward.x + camera_forward.y * camera_forward.y + camera_forward.z * camera_forward.z);
    if (temp_double1 > 1e-10) {
        camera_forward.x /= temp_double1;
        camera_forward.y /= temp_double1;
        camera_forward.z /= temp_double1;
    }
    
    camera_right.x = camera_forward.y * scene.camera_up.z - camera_forward.z * scene.camera_up.y;
    camera_right.y = camera_forward.z * scene.camera_up.x - camera_forward.x * scene.camera_up.z;
    camera_right.z = camera_forward.x * scene.camera_up.y - camera_forward.y * scene.camera_up.x;
    
    temp_double1 = sqrt(camera_right.x * camera_right.x + camera_right.y * camera_right.y + camera_right.z * camera_right.z);
    if (temp_double1 > 1e-10) {
        camera_right.x /= temp_double1;
        camera_right.y /= temp_double1;
        camera_right.z /= temp_double1;
    }
    
    camera_up_vec.x = camera_right.y * camera_forward.z - camera_right.z * camera_forward.y;
    camera_up_vec.y = camera_right.z * camera_forward.x - camera_right.x * camera_forward.z;
    camera_up_vec.z = camera_right.x * camera_forward.y - camera_right.y * camera_forward.x;
    
    /* Render image */
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            /* Subpixel sampling (2x2 grid) */
            color.x = 0.0;
            color.y = 0.0;
            color.z = 0.0;
            num_samples = 0;
            
            for (dy = 0; dy < 2; dy++) {
                for (dx = 0; dx < 2; dx++) {
                    /* Calculate pixel position with subpixel offset */
                    px = (x + (dx + 0.5) / 2.0) / width;
                    py = (y + (dy + 0.5) / 2.0) / height;
                    
                    /* Convert to normalized device coordinates */
                    ndc_x = (px - 0.5) * 2.0;
                    ndc_y = (py - 0.5) * 2.0;
                    
                    /* Scale by aspect ratio and FOV */
                    scale_x = tan(scene.fov_y / 2.0) * scene.aspect_ratio;
                    scale_y = tan(scene.fov_y / 2.0);
                    
                    ray_dir.x = ndc_x * scale_x * camera_right.x + ndc_y * scale_y * camera_up_vec.x + camera_forward.x;
                    ray_dir.y = ndc_x * scale_x * camera_right.y + ndc_y * scale_y * camera_up_vec.y + camera_forward.y;
                    ray_dir.z = ndc_x * scale_x * camera_right.z + ndc_y * scale_y * camera_up_vec.z + camera_forward.z;
                    
                    /* Normalize ray direction */
                    temp_double1 = sqrt(ray_dir.x * ray_dir.x + ray_dir.y * ray_dir.y + ray_dir.z * ray_dir.z);
                    if (temp_double1 > 1e-10) {
                        ray_dir.x /= temp_double1;
                        ray_dir.y /= temp_double1;
                        ray_dir.z /= temp_double1;
                    }
                    
                    ray.origin = scene.camera_pos;
                    ray.direction = ray_dir;
                    Vec3 ray_color = trace_ray(ray, scene, 0);
                    
                    /* Accumulate in linear space */
                    color.x += ray_color.x;
                    color.y += ray_color.y;
                    color.z += ray_color.z;
                    num_samples++;
                }
            }
            
            /* Average samples */
            if (num_samples > 0) {
                color.x /= (double)num_samples;
                color.y /= (double)num_samples;
                color.z /= (double)num_samples;
            }
            
            /* Gamma correct and store pixel */
            idx = (y * width + x) * 3;
            b = gamma_correct(color.z);     /* Blue */
            g = gamma_correct(color.y);     /* Green */
            r = gamma_correct(color.x);     /* Red */
            pixels[idx] = b;
            pixels[idx + 1] = g;
            pixels[idx + 2] = r;
        }
    }
    
    /* Write TGA file */
    if (!write_tga("out.tga", pixels, width, height)) {
        fprintf(stderr, "Failed to write TGA file\n");
        free(pixels);
        return 1;
    }
    
    free(pixels);
    return 0;
}
