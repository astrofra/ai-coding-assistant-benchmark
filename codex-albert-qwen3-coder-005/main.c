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

// Forward declarations
vec3 sky_color(vec3 direction);
vec3 checkerboard_pattern(vec3 point);
int ray_sphere_intersect(ray r, sphere s, double *t);
int ray_plane_intersect(ray r, plane p, double *t);
double fresnel_reflectance(double cos_theta, double ref_idx);
vec3 reflect(vec3 incident, vec3 normal);
vec3 refract(vec3 incident, vec3 normal, double ref_ratio);
int in_shadow(ray r, double max_distance);
vec3 get_material_color(vec3 point, vec3 normal, material mat);
vec3 shade(ray r, vec3 point, vec3 normal, material mat, int depth);
vec3 trace(ray r, int depth);
unsigned char gamma_correct(double c);
int save_tga(const char* filename, unsigned char* pixels, int width, int height);

// Sky color calculation
vec3 sky_color(vec3 direction) {
    // Clamp direction.y to [0, 1] for numerical robustness
    double y;
    double t;
    vec3 dark_red, orange, result;
    
    if (direction.y < 0.0) {
        y = 0.0;
    } else if (direction.y > 1.0) {
        y = 1.0;
    } else {
        y = direction.y;
    }
    
    // Calculate interpolation factor with power function
    t = pow(y, 0.65);
    
    // Define colors
    dark_red.x = 0.18;
    dark_red.y = 0.01;
    dark_red.z = 0.015;
    
    orange.x = 1.0;
    orange.y = 0.25;
    orange.z = 0.03;
    
    // Interpolate between colors
    result.x = (1.0 - t) * orange.x + t * dark_red.x;
    result.y = (1.0 - t) * orange.y + t * dark_red.y;
    result.z = (1.0 - t) * orange.z + t * dark_red.z;
    
    return result;
}

// Checkerboard pattern
vec3 checkerboard_pattern(vec3 point) {
    int x, z;
    vec3 result;
    
    if (point.x < 0.0) {
        x = (int)(point.x - 0.5);
    } else {
        x = (int)(point.x + 0.5);
    }
    
    if (point.z < 0.0) {
        z = (int)(point.z - 0.5);
    } else {
        z = (int)(point.z + 0.5);
    }
    
    // Alternate colors based on sum of coordinates
    if ((x + z) % 2 == 0) {
        // Black
        result.x = 0.0;
        result.y = 0.0;
        result.z = 0.0;
    } else {
        // White
        result.x = 1.0;
        result.y = 1.0;
        result.z = 1.0;
    }
    
    return result;
}

// Ray-sphere intersection
int ray_sphere_intersect(ray r, sphere s, double *t) {
    vec3 oc;
    double a, b, c, discriminant, sqrt_discriminant;
    double t1, t2;
    
    oc = vec3_sub(r.origin, s.center);
    a = vec3_dot(r.direction, r.direction);
    b = 2.0 * vec3_dot(oc, r.direction);
    c = vec3_dot(oc, oc) - s.radius * s.radius;
    discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) {
        return 0; // No intersection
    }
    
    // Calculate both solutions
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
    
    return 0; // No valid intersection
}

// Ray-plane intersection
int ray_plane_intersect(ray r, plane p, double *t) {
    double denom;
    vec3 op;
    
    denom = vec3_dot(p.normal, r.direction);
    
    if (fabs(denom) < 1e-6) {
        return 0; // Ray is parallel to plane
    }
    
    op = vec3_sub(p.point, r.origin);
    *t = vec3_dot(op, p.normal) / denom;
    
    if (*t > 1e-4) {
        return 1; // Valid intersection
    }
    
    return 0; // Intersection behind ray origin
}

// Fresnel reflectance approximation (Schlick's approximation)
double fresnel_reflectance(double cos_theta, double ref_idx) {
    double r0;
    r0 = (1.0 - ref_idx) / (1.0 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow((1.0 - cos_theta), 5.0);
}

// Reflect vector
vec3 reflect(vec3 incident, vec3 normal) {
    vec3 result;
    double dot;
    dot = vec3_dot(incident, normal);
    result.x = incident.x - 2.0 * dot * normal.x;
    result.y = incident.y - 2.0 * dot * normal.y;
    result.z = incident.z - 2.0 * dot * normal.z;
    return result;
}

// Refract vector
vec3 refract(vec3 incident, vec3 normal, double ref_ratio) {
    double cos_i, k;
    vec3 result;
    
    cos_i = vec3_dot(incident, normal);
    k = 1.0 - ref_ratio * ref_ratio * (1.0 - cos_i * cos_i);
    
    if (k < 0.0) {
        // Total internal reflection
        return reflect(incident, normal);
    }
    
    result.x = ref_ratio * (incident.x - normal.x * cos_i) - normal.x * sqrt(k);
    result.y = ref_ratio * (incident.y - normal.y * cos_i) - normal.y * sqrt(k);
    result.z = ref_ratio * (incident.z - normal.z * cos_i) - normal.z * sqrt(k);
    
    return result;
}

// Shadow ray test
int in_shadow(ray r, double max_distance) {
    double t;
    int i;
    
    // Test intersection with spheres
    for (i = 0; i < 2; i++) {
        if (ray_sphere_intersect(r, spheres[i], &t) && t < max_distance) {
            return 1; // Shadowed
        }
    }
    
    // Test intersection with ground plane
    if (ray_plane_intersect(r, ground_plane, &t) && t < max_distance) {
        return 1; // Shadowed
    }
    
    return 0; // Not shadowed
}

// Get material color at intersection point
vec3 get_material_color(vec3 point, vec3 normal, material mat) {
    vec3 pattern_color;
    pattern_color = checkerboard_pattern(point);
    return pattern_color;
}

// Shading function
vec3 shade(ray r, vec3 point, vec3 normal, material mat, int depth) {
    vec3 color, light_dir, light_color, mat_color, view_dir, reflect_dir;
    double diff, spec, light_distance;
    int i;
    
    // Initialize color with ambient component
    color.x = 0.0;
    color.y = 0.0;
    color.z = 0.0;
    
    // Add contribution from each light
    for (i = 0; i < 3; i++) {
        light_dir = vec3_sub(lights[i].position, point);
        light_distance = vec3_length(light_dir);
        light_dir = vec3_normalize(light_dir);
        
        // Check if point is in shadow
        ray shadow_ray;
        shadow_ray.origin = vec3_add(point, vec3_mul(normal, 1e-4));
        shadow_ray.direction = light_dir;
        
        if (!in_shadow(shadow_ray, light_distance)) {
            // Diffuse lighting (Lambertian)
            diff = fmax(0.0, vec3_dot(normal, light_dir));
            
            // Specular lighting (Phong or simple approximation)
            view_dir = vec3_normalize(vec3_mul(r.direction, -1.0));
            reflect_dir = reflect(vec3_mul(light_dir, -1.0), normal);
            spec = pow(fmax(0.0, vec3_dot(view_dir, reflect_dir)), 32.0);
            
            // Combine diffuse and specular
            light_color = lights[i].intensity;
            mat_color = get_material_color(point, normal, mat);
            
            // For glass, we reduce the light contribution
            if (mat.type == MATERIAL_DIELECTRIC) {
                // Glass material - reduce light intensity
                light_color.x *= 0.5;
                light_color.y *= 0.5;
                light_color.z *= 0.5;
            }
            
            // Add contribution from this light
            color.x += (diff * mat_color.x + spec * 0.5) * light_color.x;
            color.y += (diff * mat_color.y + spec * 0.5) * light_color.y;
            color.z += (diff * mat_color.z + spec * 0.5) * light_color.z;
        }
    }
    
    // Handle reflections and refractions for recursive ray tracing
    if (depth < 8) {
        vec3 reflect_color, refract_color;
        ray reflect_ray, refract_ray;
        double reflect_prob, cos_theta, ref_ratio;
        int entering;
        
        if (mat.type == MATERIAL_METAL) {
            // Metal reflection
            reflect_ray.origin = vec3_add(point, vec3_mul(normal, 1e-4));
            reflect_ray.direction = reflect(r.direction, normal);
            
            // Recursive trace
            reflect_color = trace(reflect_ray, depth + 1);
            color = reflect_color;
        } else if (mat.type == MATERIAL_DIELECTRIC) {
            // Dielectric (glass) - handle reflection and refraction
            cos_theta = vec3_dot(r.direction, normal);
            ref_ratio = 1.0 / 1.5; // Air to glass
            
            // Check if ray is entering or exiting the sphere
            entering = (cos_theta < 0);
            
            if (entering) {
                // Entering the sphere
                normal.x = -normal.x;
                normal.y = -normal.y;
                normal.z = -normal.z; // Flip normal
                cos_theta = -cos_theta;
                ref_ratio = 1.0 / 1.5;
            } else {
                // Exiting the sphere
                ref_ratio = 1.5 / 1.0;
            }
            
            // Calculate reflection coefficient
            reflect_prob = fresnel_reflectance(cos_theta, ref_ratio);
            
            // Reflect
            reflect_ray.origin = vec3_add(point, vec3_mul(normal, 1e-4));
            reflect_ray.direction = reflect(r.direction, normal);
            
            // Refract
            refract_ray.origin = vec3_add(point, vec3_mul(normal, -1e-4)); // Opposite side
            refract_ray.direction = refract(r.direction, normal, ref_ratio);
            
            // Recursive traces
            reflect_color = trace(reflect_ray, depth + 1);
            refract_color = trace(refract_ray, depth + 1);
            
            // Blend based on reflectance
            reflect_color.x *= reflect_prob;
            reflect_color.y *= reflect_prob;
            reflect_color.z *= reflect_prob;
            
            refract_color.x *= (1.0 - reflect_prob);
            refract_color.y *= (1.0 - reflect_prob);
            refract_color.z *= (1.0 - reflect_prob);
            
            // Combine reflection and refraction
            color.x = reflect_color.x + refract_color.x;
            color.y = reflect_color.y + refract_color.y;
            color.z = reflect_color.z + refract_color.z;
        }
    }
    
    return color;
}

// Trace a ray through the scene
vec3 trace(ray r, int depth) {
    double closest_t, sphere_t, plane_t;
    int hit_sphere, hit_plane, i;
    vec3 point, normal, color;
    material mat;
    double transmittance;
    
    closest_t = 1e20;
    hit_sphere = -1;
    hit_plane = -1;
    
    // Find closest intersection with spheres
    for (i = 0; i < 2; i++) {
        if (ray_sphere_intersect(r, spheres[i], &sphere_t) && sphere_t < closest_t) {
            closest_t = sphere_t;
            hit_sphere = i;
        }
    }
    
    // Find closest intersection with ground plane
    if (ray_plane_intersect(r, ground_plane, &plane_t) && plane_t < closest_t) {
        closest_t = plane_t;
        hit_plane = 0;
    }
    
    // If we hit something
    if (hit_sphere != -1 || hit_plane != -1) {
        // Calculate intersection point
        point = vec3_add(r.origin, vec3_mul(r.direction, closest_t));
        
        // Calculate normal
        if (hit_sphere != -1) {
            // Hit a sphere
            normal = vec3_sub(point, spheres[hit_sphere].center);
            normal = vec3_normalize(normal);
            mat = spheres[hit_sphere].mat;
        } else {
            // Hit ground plane
            normal = ground_plane.normal;
            mat = ground_plane.mat;
        }
        
        // Shade the point
        color = shade(r, point, normal, mat, depth);
        
        // Apply fog effect
        transmittance = exp(-0.035 * closest_t);
        vec3 fog_color = sky_color(r.direction);
        color.x = transmittance * color.x + (1.0 - transmittance) * fog_color.x;
        color.y = transmittance * color.y + (1.0 - transmittance) * fog_color.y;
        color.z = transmittance * color.z + (1.0 - transmittance) * fog_color.z;
        
        return color;
    }
    
    // No intersection, return sky color
    return sky_color(r.direction);
}

// Convert RGB color to 8-bit with gamma correction
unsigned char gamma_correct(double c) {
    if (c <= 0.0031308) {
        return (unsigned char)(c * 12.92 * 255.0 + 0.5);
    } else {
        return (unsigned char)((1.055 * pow(c, 1.0/2.4) - 0.055) * 255.0 + 0.5);
    }
}

// Save image as TGA file - DEBUG VERSION WITH VERBOSE OUTPUT
int save_tga(const char* filename, unsigned char* pixels, int width, int height) {
    FILE* file;
    unsigned char header[18];
    int y, x, idx;
    unsigned char b, g, r;
    int total_pixels = width * height;
    int expected_size = 18 + total_pixels * 3;
    
    printf("DEBUG: Saving TGA file with width=%d, height=%d\n", width, height);
    printf("DEBUG: Expected size: %d bytes\n", expected_size);
    
    file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open %s for writing\n", filename);
        return 0;
    }
    
    // TGA Header (18 bytes)
    header[0] = 0;     // ID length
    header[1] = 0;     // Color map type
    header[2] = 2;     // Image type: uncompressed true-color
    header[3] = 0;     // Color map first index
    header[4] = 0;     // Color map length
    header[5] = 0;     // Color map entry size
    header[6] = 0;     // X origin
    header[7] = 0;     // Y origin
    
    // Width and height in little-endian format
    header[8] = width & 0xFF;           // Width (low byte)
    header[9] = (width >> 8) & 0xFF;    // Width (high byte)
    header[10] = height & 0xFF;         // Height (low byte)
    header[11] = (height >> 8) & 0xFF;  // Height (high byte)
    header[12] = 24;   // Pixel depth
    header[13] = 0x20; // Descriptor: top-left origin, left-to-right pixel order
    header[14] = 0;    // Reserved byte 1
    header[15] = 0;    // Reserved byte 2
    header[16] = 0;    // Reserved byte 3
    header[17] = 0;    // Reserved byte 4
    
    // Write header
    if (fwrite(header, 1, 18, file) != 18) {
        fclose(file);
        fprintf(stderr, "Error: Failed to write TGA header\n");
        return 0;
    }
    
    printf("DEBUG: Header written successfully\n");
    
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
    printf("DEBUG: File saved successfully\n");
    return 1;
}

// Main function
int main(void) {
    // Set up camera parameters
    vec3 camera_pos, camera_target, camera_up, camera_forward, camera_right;
    double fov, aspect_ratio, half_height, half_width;
    int width, height, x, y, sx, sy, num_samples, idx;
    unsigned char* pixels;
    vec3 color, pixel_color, ray_dir;
    double u, v, ndc_x, ndc_y;
    ray r;
    
    camera_pos.x = 0.0;
    camera_pos.y = 2.6;
    camera_pos.z = 6.5;
    
    camera_target.x = 0.0;
    camera_target.y = 1.0;
    camera_target.z = 0.0;
    
    camera_up.x = 0.0;
    camera_up.y = 1.0;
    camera_up.z = 0.0;
    
    // Create camera coordinate system
    camera_forward = vec3_sub(camera_target, camera_pos);
    camera_forward = vec3_normalize(camera_forward);
    
    camera_right = vec3_cross(camera_forward, camera_up);
    camera_right = vec3_normalize(camera_right);
    
    camera_up = vec3_cross(camera_right, camera_forward);
    
    // Set up projection parameters
    fov = 45.0 * M_PI / 180.0; // 45 degrees in radians
    aspect_ratio = 1280.0 / 720.0;
    half_height = tan(fov / 2.0);
    half_width = half_height * aspect_ratio;
    
    // Initialize spheres
    // Glass sphere (left)
    spheres[0].center.x = -1.25;
    spheres[0].center.y = 1.0;
    spheres[0].center.z = 0.0;
    spheres[0].radius = 1.0;
    spheres[0].mat.type = MATERIAL_DIELECTRIC;
    spheres[0].mat.albedo.x = 1.0;
    spheres[0].mat.albedo.y = 1.0;
    spheres[0].mat.albedo.z = 1.0; // Colorless
    spheres[0].mat.ref_idx = 1.5; // Index of refraction

    // Chrome sphere (right)
    spheres[1].center.x = 1.25;
    spheres[1].center.y = 1.0;
    spheres[1].center.z = 0.0;
    spheres[1].radius = 1.0;
    spheres[1].mat.type = MATERIAL_METAL;
    spheres[1].mat.albedo.x = 0.8;
    spheres[1].mat.albedo.y = 0.8;
    spheres[1].mat.albedo.z = 0.8; // Silver
    spheres[1].mat.fuzz = 0.0; // Perfect mirror

    // Initialize ground plane
    ground_plane.point.x = 0.0;
    ground_plane.point.y = 0.0;
    ground_plane.point.z = 0.0;
    ground_plane.normal.x = 0.0;
    ground_plane.normal.y = 1.0;
    ground_plane.normal.z = 0.0;
    ground_plane.mat.type = MATERIAL_LAMBERTIAN;
    
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
    
    // Allocate memory for image
    width = 1280;
    height = 720;
    pixels = (unsigned char*)malloc(width * height * 3);
    if (!pixels) {
        fprintf(stderr, "Error: Cannot allocate memory for image\n");
        return 1;
    }
    
    printf("DEBUG: Rendering image %dx%d\n", width, height);
    
    // Render image
    for (y = 0; y < height; y++) {
        if (y % 100 == 0) printf("DEBUG: Rendering row %d/%d\n", y, height);
        for (x = 0; x < width; x++) {
            // Subpixel sampling (2x2 grid)
            color.x = 0.0;
            color.y = 0.0;
            color.z = 0.0;
            num_samples = 0;
            
            for (sy = 0; sy < 2; sy++) {
                for (sx = 0; sx < 2; sx++) {
                    // Calculate normalized device coordinates
                    u = (x + (sx + 0.5) / 2.0) / width;
                    v = (y + (sy + 0.5) / 2.0) / height;
                    
                    // Scale to [-1, 1] range
                    ndc_x = (2.0 * u - 1.0) * half_width;
                    ndc_y = (1.0 - 2.0 * v) * half_height;
                    
                    // Calculate ray direction
                    ray_dir.x = ndc_x * camera_right.x + ndc_y * camera_up.x + camera_forward.x;
                    ray_dir.y = ndc_x * camera_right.y + ndc_y * camera_up.y + camera_forward.y;
                    ray_dir.z = ndc_x * camera_right.z + ndc_y * camera_up.z + camera_forward.z;
                    ray_dir = vec3_normalize(ray_dir);
                    
                    // Create ray
                    r.origin = camera_pos;
                    r.direction = ray_dir;
                    
                    // Trace ray
                    pixel_color = trace(r, 0);
                    
                    // Accumulate color (linear RGB)
                    color.x += pixel_color.x;
                    color.y += pixel_color.y;
                    color.z += pixel_color.z;
                    num_samples++;
                }
            }
            
            // Average samples
            if (num_samples > 0) {
                color.x /= num_samples;
                color.y /= num_samples;
                color.z /= num_samples;
            }
            
            // Convert to 8-bit with gamma correction
            idx = (y * width + x) * 3;
            pixels[idx] = gamma_correct(color.z);     // Blue
            pixels[idx + 1] = gamma_correct(color.y); // Green  
            pixels[idx + 2] = gamma_correct(color.x); // Red
        }
    }
    
    printf("DEBUG: Image rendered, saving...\n");
    
    // Save image
    if (!save_tga("out.tga", pixels, width, height)) {
        free(pixels);
        return 1;
    }
    
    free(pixels);
    printf("Raytracer completed. Image saved as out.tga\n");
    return 0;
}
