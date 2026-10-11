#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Vector operations
typedef struct {
    double x, y, z;
} Vec3;

static Vec3 vec3_add(Vec3 a, Vec3 b) {
    Vec3 r;
    r.x = a.x + b.x;
    r.y = a.y + b.y;
    r.z = a.z + b.z;
    return r;
}

static Vec3 vec3_sub(Vec3 a, Vec3 b) {
    Vec3 r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    return r;
}

static Vec3 vec3_mul(Vec3 a, double s) {
    Vec3 r;
    r.x = a.x * s;
    r.y = a.y * s;
    r.z = a.z * s;
    return r;
}

static Vec3 vec3_div(Vec3 a, double s) {
    Vec3 r;
    r.x = a.x / s;
    r.y = a.y / s;
    r.z = a.z / s;
    return r;
}

static double vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3 vec3_cross(Vec3 a, Vec3 b) {
    Vec3 r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return r;
}

static double vec3_length(Vec3 v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static double vec3_length_squared(Vec3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static Vec3 vec3_normalize(Vec3 v) {
    double len;
    Vec3 r;
    len = vec3_length(v);
    if (len > 1e-10) {
        r = vec3_div(v, len);
        return r;
    }
    r.x = 0.0;
    r.y = 0.0;
    r.z = 0.0;
    return r;
}

// Ray structure
typedef struct {
    Vec3 origin;
    Vec3 direction;
} Ray;

// Material types
typedef enum {
    MATERIAL_LAMBERTIAN,
    MATERIAL_METAL,
    MATERIAL_DIELECTRIC
} MaterialType;

// Material structure
typedef struct {
    MaterialType type;
    Vec3 albedo;
    double fuzz;
    double refraction_index;
} Material;

// Sphere structure
typedef struct {
    Vec3 center;
    double radius;
    Material material;
} Sphere;

// Plane structure
typedef struct {
    Vec3 point;
    Vec3 normal;
    Material material;
} Plane;

// Light structure
typedef struct {
    Vec3 position;
    Vec3 intensity;
} Light;

// Scene structure
typedef struct {
    Sphere spheres[2];
    Plane ground;
    Light lights[3];
    Vec3 camera_pos;
    Vec3 camera_target;
    Vec3 camera_up;
    double fov;
    int width;
    int height;
} Scene;

// Sky color computation
static Vec3 sky_color(Vec3 ray_dir) {
    Vec3 dark_red;
    Vec3 orange;
    Vec3 result;
    double t;
    double y;
    
    dark_red.x = 0.18;
    dark_red.y = 0.01;
    dark_red.z = 0.015;
    
    orange.x = 1.0;
    orange.y = 0.25;
    orange.z = 0.03;
    
    // Clamp ray direction y component to [0, 1]
    y = ray_dir.y;
    if (y < 0.0) y = -y;
    if (y > 1.0) y = 1.0;
    
    t = pow(y, 0.65);
    result.x = (1.0 - t) * orange.x + t * dark_red.x;
    result.y = (1.0 - t) * orange.y + t * dark_red.y;
    result.z = (1.0 - t) * orange.z + t * dark_red.z;
    
    return result;
}

// Checkerboard pattern
static Vec3 checkerboard_color(Vec3 point) {
    int x, z;
    Vec3 result;
    
    x = (int)floor(point.x);
    z = (int)floor(point.z);
    
    // Ensure proper alternating pattern for negative coordinates
    if ((x + z) % 2 == 0) {
        result.x = 1.0;  // White
        result.y = 1.0;
        result.z = 1.0;
    } else {
        result.x = 0.0;  // Black
        result.y = 0.0;
        result.z = 0.0;
    }
    return result;
}

// Ray-sphere intersection
static int intersect_sphere(Ray ray, Sphere sphere, double* t) {
    Vec3 oc;
    double a, b, c, discriminant, sqrt_discriminant;
    double t1, t2;
    
    oc = vec3_sub(ray.origin, sphere.center);
    a = vec3_dot(ray.direction, ray.direction);
    b = 2.0 * vec3_dot(oc, ray.direction);
    c = vec3_dot(oc, oc) - sphere.radius * sphere.radius;
    discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) {
        return 0;  // No intersection
    }
    
    // Find the nearest positive root
    sqrt_discriminant = sqrt(discriminant);
    t1 = (-b - sqrt_discriminant) / (2.0 * a);
    t2 = (-b + sqrt_discriminant) / (2.0 * a);
    
    if (t1 > 1e-4) {
        *t = t1;
        return 1;  // Hit front face
    } else if (t2 > 1e-4) {
        *t = t2;
        return 1;  // Hit back face
    }
    
    return 0;  // Intersection behind ray origin
}

// Ray-plane intersection
static int intersect_plane(Ray ray, Plane plane, double* t) {
    double denom;
    Vec3 op;
    
    denom = vec3_dot(plane.normal, ray.direction);
    
    if (fabs(denom) < 1e-6) {
        return 0;  // Ray is parallel to plane
    }
    
    op = vec3_sub(plane.point, ray.origin);
    *t = vec3_dot(op, plane.normal) / denom;
    
    if (*t > 1e-4) {
        return 1;  // Valid intersection
    }
    
    return 0;  // Intersection behind ray origin
}

// Fresnel reflectance using Schlick's approximation
static double fresnel(double cos_theta, double refraction_index) {
    double r0;
    r0 = (1.0 - refraction_index) / (1.0 + refraction_index);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow((1.0 - cos_theta), 5.0);
}

// Refract ray
static Vec3 refract(Vec3 incident, Vec3 normal, double refraction_ratio) {
    double cos_i, k;
    Vec3 refracted;
    
    cos_i = vec3_dot(normal, incident);
    if (cos_i > 0.0) {
        // Ray entering material
        normal = vec3_mul(normal, -1.0);
        cos_i = -cos_i;
    }
    
    k = 1.0 - refraction_ratio * refraction_ratio * (1.0 - cos_i * cos_i);
    
    if (k < 0.0) {
        // Total internal reflection
        refracted = vec3_sub(incident, vec3_mul(normal, 2.0 * cos_i));
        return vec3_normalize(refracted);
    }
    
    refracted = vec3_sub(
        vec3_mul(incident, refraction_ratio),
        vec3_mul(normal, refraction_ratio * cos_i + sqrt(k))
    );
    
    return vec3_normalize(refracted);
}

// Reflect ray
static Vec3 reflect(Vec3 incident, Vec3 normal) {
    Vec3 result;
    result = vec3_sub(incident, vec3_mul(normal, 2.0 * vec3_dot(incident, normal)));
    return result;
}

// Forward declaration for trace_ray
static Vec3 trace_ray(Ray ray, Scene* scene, int depth);

// Shade a point with lighting
static Vec3 shade_point(Vec3 point, Vec3 normal, Material material, Scene* scene, int depth) {
    Vec3 color;
    Vec3 ambient;
    Vec3 diffuse_color;
    Vec3 specular_color;
    Vec3 light_contribution;
    Vec3 light_dir;
    Vec3 view_dir;
    Vec3 reflect_dir;
    double light_distance, diffuse, spec;
    int i, j;
    int in_shadow;
    double t_shadow;
    Ray shadow_ray;
    double tmp;
    
    color.x = 0.0;
    color.y = 0.0;
    color.z = 0.0;
    
    // Ambient lighting
    ambient.x = 0.1;
    ambient.y = 0.1;
    ambient.z = 0.1;
    tmp = ambient.x;
    color = vec3_add(color, vec3_mul(material.albedo, tmp));
    
    // Light contributions
    for (i = 0; i < 3; i++) {
        light_dir = vec3_normalize(vec3_sub(scene->lights[i].position, point));
        light_distance = vec3_length(vec3_sub(scene->lights[i].position, point));
        
        // Shadow ray
        shadow_ray.origin = vec3_add(point, vec3_mul(normal, 1e-4));  // Offset from surface
        shadow_ray.direction = light_dir;
        
        in_shadow = 0;
        
        // Check if shadow ray hits any object before reaching light
        for (j = 0; j < 2; j++) {
            if (intersect_sphere(shadow_ray, scene->spheres[j], &t_shadow)) {
                if (t_shadow < light_distance) {
                    in_shadow = 1;
                    break;
                }
            }
        }
        
        if (!in_shadow) {
            // Diffuse lighting
            diffuse = fmax(0.0, vec3_dot(normal, light_dir));
            diffuse_color = vec3_mul(material.albedo, diffuse);
            
            // Specular lighting (Phong)
            view_dir = vec3_normalize(vec3_sub(scene->camera_pos, point));
            reflect_dir = reflect(vec3_mul(light_dir, -1.0), normal);
            spec = pow(fmax(0.0, vec3_dot(view_dir, reflect_dir)), 32.0);
            specular_color.x = 1.0;
            specular_color.y = 1.0;
            specular_color.z = 1.0;
            tmp = spec;
            specular_color = vec3_mul(specular_color, tmp);
            
            // Combine lighting
            light_contribution = vec3_add(diffuse_color, specular_color);
            tmp = scene->lights[i].intensity.x;
            light_contribution = vec3_mul(light_contribution, tmp);
            color = vec3_add(color, light_contribution);
        }
    }
    
    // Reflection for metal and dielectric materials
    if (depth < 8) {
        if (material.type == MATERIAL_METAL || material.type == MATERIAL_DIELECTRIC) {
            Vec3 view_dir;
            Vec3 reflect_dir;
            Ray reflect_ray;
            Vec3 reflect_color;
            double reflect_prob;
            Vec3 refract_color;
            Ray refract_ray;
            
            view_dir = vec3_normalize(vec3_sub(scene->camera_pos, point));
            reflect_dir = reflect(view_dir, normal);
            
            // Reflection ray
            reflect_ray.origin = vec3_add(point, vec3_mul(normal, 1e-4));  // Offset from surface
            reflect_ray.direction = reflect_dir;
            
            // Trace reflection ray
            reflect_color = trace_ray(reflect_ray, scene, depth + 1);
            
            if (material.type == MATERIAL_METAL) {
                // Metal reflection
                double reflect_weight = 0.95;
                color = vec3_add(vec3_mul(color, 1.0 - reflect_weight), vec3_mul(reflect_color, reflect_weight));
            } else if (material.type == MATERIAL_DIELECTRIC) {
                // Dielectric (glass) reflection
                double cos_theta;
                cos_theta = vec3_dot(normal, view_dir);
                reflect_prob = fresnel(cos_theta, material.refraction_index);
                
                // Refraction ray
                refract_ray.origin = vec3_add(point, vec3_mul(normal, -1e-4));  // Offset from surface
                refract_ray.direction = refract(view_dir, normal, 1.0 / material.refraction_index);
                
                // Trace refraction ray
                refract_color = trace_ray(refract_ray, scene, depth + 1);
                
                // Mix reflection and refraction
                color = vec3_add(vec3_mul(color, 1.0 - reflect_prob), vec3_mul(reflect_color, reflect_prob));
                color = vec3_add(vec3_mul(color, 1.0 - reflect_prob), vec3_mul(refract_color, reflect_prob));
            }
        }
    }
    
    return color;
}

// Trace a ray through the scene
static Vec3 trace_ray(Ray ray, Scene* scene, int depth) {
    double closest_t;
    int hit_object;
    Vec3 hit_point, hit_normal;
    Material hit_material;
    double t;
    Vec3 color;
    Vec3 base_color;
    Material ground_mat;
    double transmittance;
    Vec3 sky;
    Vec3 result;
    
    closest_t = 1e20;
    hit_object = -1;
    
    // Check intersection with spheres
    {
        int i;
        for (i = 0; i < 2; i++) {
            if (intersect_sphere(ray, scene->spheres[i], &t) && t < closest_t) {
                closest_t = t;
                hit_object = i;
                hit_point = vec3_add(ray.origin, vec3_mul(ray.direction, t));
                hit_normal = vec3_normalize(vec3_sub(hit_point, scene->spheres[i].center));
                hit_material = scene->spheres[i].material;
            }
        }
    }
    
    // Check intersection with ground plane
    if (intersect_plane(ray, scene->ground, &t) && t < closest_t) {
        closest_t = t;
        hit_object = 2;  // Ground plane
        hit_point = vec3_add(ray.origin, vec3_mul(ray.direction, t));
        hit_normal = scene->ground.normal;
        hit_material = scene->ground.material;
    }
    
    // If we hit something
    if (hit_object >= 0) {
        // Handle different material types
        if (hit_object < 2) {
            // Sphere hit
            color = shade_point(hit_point, hit_normal, hit_material, scene, depth);
        } else {
            // Ground plane hit - apply checkerboard pattern
            base_color = checkerboard_color(hit_point);
            ground_mat = scene->ground.material;
            ground_mat.albedo = base_color;
            color = shade_point(hit_point, hit_normal, ground_mat, scene, depth);
        }
        
        // Apply fog effect
        transmittance = exp(-0.035 * closest_t);
        sky = sky_color(ray.direction);
        result.x = transmittance * color.x + (1.0 - transmittance) * sky.x;
        result.y = transmittance * color.y + (1.0 - transmittance) * sky.y;
        result.z = transmittance * color.z + (1.0 - transmittance) * sky.z;
        
        return result;
    }
    
    // No hit - return sky color
    return sky_color(ray.direction);
}

// Convert RGB color to 8-bit values with gamma correction
static void rgb_to_8bit(Vec3 color, unsigned char* r, unsigned char* g, unsigned char* b) {
    // Gamma correction (sRGB transfer function)
    double gamma;
    gamma = 2.2;
    *r = (unsigned char)(255.0 * pow(fmax(0.0, fmin(1.0, color.x)), 1.0/gamma) + 0.5);
    *g = (unsigned char)(255.0 * pow(fmax(0.0, fmin(1.0, color.y)), 1.0/gamma) + 0.5);
    *b = (unsigned char)(255.0 * pow(fmax(0.0, fmin(1.0, color.z)), 1.0/gamma) + 0.5);
}

// Save image as TGA file
static int save_tga(const char* filename, unsigned char* pixels, int width, int height) {
    FILE* file;
    unsigned char header[18];
    int y, x, idx;
    unsigned char b, g, r;
    
    file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file %s for writing\n", filename);
        return 0;
    }
    
    // TGA Header (18 bytes)
    header[0] = 0;  // ID length
    header[1] = 0;  // Color map type
    header[2] = 2;  // Image type: uncompressed truecolor
    header[3] = 0;  // Color map first index
    header[4] = 0;  // Color map length
    header[5] = 0;  // Color map entry size
    header[6] = 0;  // X origin
    header[7] = 0;  // Y origin
    header[8] = width & 0xFF;          // Width (low byte)
    header[9] = (width >> 8) & 0xFF;   // Width (high byte)
    header[10] = height & 0xFF;        // Height (low byte)
    header[11] = (height >> 8) & 0xFF; // Height (high byte)
    header[12] = 24;  // Pixel depth
    header[13] = 0x20;  // Image descriptor: top-left origin
    
    if (fwrite(header, 1, 18, file) != 18) {
        fclose(file);
        fprintf(stderr, "Error: Failed to write TGA header\n");
        return 0;
    }
    
    // Write pixel data (BGR format, top to bottom)
    for (y = height - 1; y >= 0; y--) {
        for (x = 0; x < width; x++) {
            idx = (y * width + x) * 3;
            b = pixels[idx];
            g = pixels[idx + 1];
            r = pixels[idx + 2];
            
            if (fwrite(&b, 1, 1, file) != 1 ||
                fwrite(&g, 1, 1, file) != 1 ||
                fwrite(&r, 1, 1, file) != 1) {
                fclose(file);
                fprintf(stderr, "Error: Failed to write pixel data\n");
                return 0;
            }
        }
    }
    
    fclose(file);
    return 1;
}

// Main function
int main(void) {
    // Initialize scene
    Scene scene;
    Vec3 camera_forward;
    Vec3 camera_right;
    Vec3 camera_up_vec;
    Vec3 pixel_color;
    Vec3 color;
    unsigned char r, g, b;
    int x, y, sx, sy;
    double u, v, ndc_x, ndc_y;
    Vec3 ray_dir;
    Ray ray;
    unsigned char* pixels;
    
    // Camera setup
    scene.camera_pos.x = 0.0;
    scene.camera_pos.y = 2.6;
    scene.camera_pos.z = 6.5;
    scene.camera_target.x = 0.0;
    scene.camera_target.y = 1.0;
    scene.camera_target.z = 0.0;
    scene.camera_up.x = 0.0;
    scene.camera_up.y = 1.0;
    scene.camera_up.z = 0.0;
    scene.fov = 45.0;
    scene.width = 1280;
    scene.height = 720;
    
    // Create spheres
    // Glass sphere
    scene.spheres[0].center.x = -1.25;
    scene.spheres[0].center.y = 1.0;
    scene.spheres[0].center.z = 0.0;
    scene.spheres[0].radius = 1.0;
    scene.spheres[0].material.type = MATERIAL_DIELECTRIC;
    scene.spheres[0].material.albedo.x = 1.0;
    scene.spheres[0].material.albedo.y = 1.0;
    scene.spheres[0].material.albedo.z = 1.0;
    scene.spheres[0].material.refraction_index = 1.5;
    
    // Chrome sphere
    scene.spheres[1].center.x = 1.25;
    scene.spheres[1].center.y = 1.0;
    scene.spheres[1].center.z = 0.0;
    scene.spheres[1].radius = 1.0;
    scene.spheres[1].material.type = MATERIAL_METAL;
    scene.spheres[1].material.albedo.x = 0.8;
    scene.spheres[1].material.albedo.y = 0.8;
    scene.spheres[1].material.albedo.z = 0.8;
    scene.spheres[1].material.fuzz = 0.05;
    
    // Ground plane
    scene.ground.point.x = 0.0;
    scene.ground.point.y = 0.0;
    scene.ground.point.z = 0.0;
    scene.ground.normal.x = 0.0;
    scene.ground.normal.y = 1.0;
    scene.ground.normal.z = 0.0;
    scene.ground.material.type = MATERIAL_LAMBERTIAN;
    scene.ground.material.albedo.x = 0.5;
    scene.ground.material.albedo.y = 0.5;
    scene.ground.material.albedo.z = 0.5;
    
    // Lights
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
    
    // Allocate pixel buffer
    pixels = (unsigned char*)malloc(scene.width * scene.height * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }
    
    // Calculate camera vectors
    camera_forward = vec3_normalize(vec3_sub(scene.camera_target, scene.camera_pos));
    camera_right = vec3_normalize(vec3_cross(camera_forward, scene.camera_up));
    camera_up_vec = vec3_cross(camera_right, camera_forward);
    
    // Render image
    for (y = 0; y < scene.height; y++) {
        for (x = 0; x < scene.width; x++) {
            pixel_color.x = 0.0;
            pixel_color.y = 0.0;
            pixel_color.z = 0.0;
            
            // 2x2 subpixel sampling
            for (sy = 0; sy < 2; sy++) {
                for (sx = 0; sx < 2; sx++) {
                    // Calculate subpixel position
                    u = (x + (sx + 0.5) / 2.0) / scene.width;
                    v = (y + (sy + 0.5) / 2.0) / scene.height;
                    
                    // Convert to normalized device coordinates (-1 to 1)
                    ndc_x = (2.0 * u - 1.0) * tan(scene.fov * M_PI / 180.0 / 2.0) * (double)scene.width / (double)scene.height;
                    ndc_y = (1.0 - 2.0 * v) * tan(scene.fov * M_PI / 180.0 / 2.0);
                    
                    // Ray direction
                    ray_dir = vec3_normalize(vec3_add(
                        vec3_add(vec3_mul(camera_right, ndc_x), vec3_mul(camera_up_vec, ndc_y)),
                        camera_forward
                    ));
                    
                    // Create ray
                    ray.origin = scene.camera_pos;
                    ray.direction = ray_dir;
                    
                    // Trace ray
                    color = trace_ray(ray, &scene, 0);
                    
                    // Accumulate color
                    pixel_color.x += color.x;
                    pixel_color.y += color.y;
                    pixel_color.z += color.z;
                }
            }
            
            // Average the subpixels
            pixel_color.x /= 4.0;
            pixel_color.y /= 4.0;
            pixel_color.z /= 4.0;
            
            // Convert to 8-bit and store
            rgb_to_8bit(pixel_color, &r, &g, &b);
            
            {
                int idx = (y * scene.width + x) * 3;
                pixels[idx] = b;     // B
                pixels[idx + 1] = g; // G  
                pixels[idx + 2] = r; // R
            }
        }
    }
    
    // Save image
    if (!save_tga("out.tga", pixels, scene.width, scene.height)) {
        free(pixels);
        return 1;
    }
    
    free(pixels);
    printf("Raytracer completed successfully. Image saved as out.tga\n");
    return 0;
}
