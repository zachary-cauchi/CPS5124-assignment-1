#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>

#include "models/scene.hpp"
#include "utils/vec.hpp"
#include "utils/utils.hpp"
#include "utils/mat.hpp"
#include "models/cameras/camera.hpp"
#include "models/cameras/pinhole.hpp"
#include "models/shapes/sphere.hpp"
#include "models/lights/light.hpp"
#include "models/lights/point-light.hpp"
#include "models/primitive.hpp"
#include "models/rays/ray.hpp"

#include "path-renderer.hpp"

#define MAX_RAY_DEPTH 20
#define SPEC_SHINE 16
#define SPEC_ALBEDO 0.3
#define GLOSS_ALBEDO 0.8
#define FRES_ALBEDO 0.8

static size_t max_depth = MAX_RAY_DEPTH;
static Matrix44f cameraToWorld;
static float scale;
static std::vector<std::shared_ptr<Light>> lights;
static std::vector<std::shared_ptr<Primitive>> primitives;

void PathRenderer::prepare(const Scene &scene) {
    std::cout << "Preparing Path Renderer" << std::endl;

    lights = scene.lights;
    primitives = scene.primitives;
    max_depth = std::min(max_depth, depth);

    // Initialise frame buffers.
    framebuffer = std::vector<vec3f>(width * height);
    depthbuffer = std::vector<float>(width * height);
}

void PathRenderer::render(const std::shared_ptr<Camera> &camera) {
    std::cout << "Rendering..." << std::endl;
    vec3f dir;
    float dir_x, dir_y, dir_z;

    // Get a camera-to-world matrix for the camera's location and orientation.
    cameraToWorld = lookAt(camera->position, camera->target);
    // Calculate the 'spread factor' for the generated rays.
    scale = camera->aspect * tan(DEG_RAD(camera->fov * 0.5));

    for (int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            // Calculate the ray direction of the current pixel in the framebuffer.
            dir_x = -(2. * (x + .5) / width - 1) * scale;
            dir_y = (1 - 2 * (y + .5) / height) * scale;
            dir_z = -1;

            // Convert the ray direction to world space to obtain the true coordinates.
            cameraToWorld.multDirMatrix(vec3f(dir_x, dir_y, dir_z), dir);
            dir = dir.normalize();

            framebuffer[x + y * width] = camera->renderer_cast_ray(*this, RayInfo(camera->position, dir), depthbuffer[x + y * width]);
        }
    }

    std::cout << "Done" << std::endl;
}

void PathRenderer::compute_diffuse_intensity(const vec3f &hit, const vec3f &N, const vec3f &in_color, vec3f &out) {
    vec3f intensity(0, 0, 0);

    // Calculate diffuse lighting from each light source.
    for (size_t i = 0; i < lights.size(); i++) {
        vec3f light_dir, light_intensity;
        float light_distance;

        lights[i]->illuminate(hit, light_dir, light_intensity, light_distance);

        // Calculate shadows.
        // Offset the point to ensure it doesn't accidentally hit the same shape.
        vec3f shadow_orig = dot(light_dir, N) < 0 ? hit - N * 1e-3 : hit + N * 1e-3;
        // Temp variables.
        vec3f shadow_hit, shadow_N;
        float shadow_dist;
        std::shared_ptr<Material> tmpmaterial;

        // Check if a ray from this point to the current light is obscured by another object. If so, skip this light.
        if (scene_intersect(shadow_orig, light_dir, shadow_hit, shadow_N, shadow_dist, tmpmaterial) && (shadow_hit - shadow_orig).norm() < light_distance)
            continue;

        // Compute the specular intensity for this given light.
        intensity = intensity + light_intensity * std::max(0.f, dot(light_dir, N));
    }

    // Apply the intensity to the output vector.
    out = out + in_color * intensity;
}

void PathRenderer::compute_specular_intensity(const vec3f &dir, const vec3f &hit, const vec3f &N, const vec3f &in_color, vec3f &out) {
    vec3f intensity(0, 0, 0);

    for (size_t i = 0; i < lights.size(); i++) {
        vec3f light_dir = (lights[i]->position - hit).normalize();
        // Calculate the reflected ray intensity.
        float ray_intensity = std::max(0.f, dot(reflect(light_dir, N), dir));

        // Calculate the shine intensity for each component color.
        intensity = intensity + lights[i]->intensity * powf(ray_intensity, SPEC_SHINE);
    }

    // Add the new colour to the output.
    out = out + in_color + intensity * SPEC_ALBEDO;
}

void PathRenderer::compute_glossy(const PinholeCamera &camera, const vec3f &dir, const vec3f &hit, const vec3f &N, const std::shared_ptr<Material> &material, size_t &depth, vec3f &out) {
    // If the material is glossy, also calculate the highlights.
    compute_specular_intensity(dir, hit, N, material->get_specular(), out);

    // Cast a reflection off the shape to determine the reflection.
    float reflect_dist;
    vec3f reflect_dir = reflect(dir, N).normalize();
    vec3f reflect_orig = dot(reflect_dir, N) < 0 ? hit - N * 1e-3 : hit + N * 1e-3;
    vec3f reflect_color = cast_ray(camera, reflect_orig, reflect_dir, reflect_dist, depth + 1);

    out = out + reflect_color * (1 - material->get_roughness());
}

void PathRenderer::compute_fresnel(const PinholeCamera &camera, const vec3f &dir, const vec3f &hit, const vec3f &N, const std::shared_ptr<Material> &material, size_t &depth, vec3f &out) {
    vec3f refract_color, reflect_color;
    float kr, kt;

    kt = fresnel(dir, N, material->get_eta(), kr);

    // If the light is not completely reflected, simulate transmission.
    if (kr < 1) {
        // Simulate transmission.
        float refract_dist;
        vec3f refract_dir = refract(dir, N, material->get_eta()).normalize();
        vec3f refract_orig = dot(refract_dir, N) < 0 ? hit - N * 1e-3 : hit + N * 1e-3;
        refract_color = cast_ray(camera, refract_orig, refract_dir, refract_dist, depth + 1);
    }

    // Simulate reflection.
    float reflect_dist;
    vec3f reflect_dir = reflect(dir, N).normalize();
    vec3f reflect_orig = dot(reflect_dir, N) < 0 ? hit - N * 1e-3 : hit + N * 1e-3;
    reflect_color = cast_ray(camera, reflect_orig, reflect_dir, reflect_dist, depth + 1);

    // Apply the new colours, multiplying by the ratio of light reflected to transmitted.
    out = out + refract_color * kt + reflect_color * kr;
}

bool PathRenderer::scene_intersect(const vec3f &orig, const vec3f &dir, vec3f &hit, vec3f &N, float &dist, std::shared_ptr<Material> &material) {
    dist = std::numeric_limits<float>::max();

    // Iterate over each sphere.
    for (size_t i = 0; i < primitives.size(); i++) {
        float dist_i;

        // If the sphere is hit, record it.
        if (primitives[i]->shape->renderer_ray_intersect(*this, RayInfo(orig, dir), dist_i) && dist_i <= dist) {
            dist = dist_i;
            hit = orig + dir * dist_i;
            N = (hit - primitives[i]->shape->position).normalize();
            material = primitives[i]->material;
        }
    }

    // If the sphere is closer than the max draw distance, return true.
    return dist < 1000;
}

vec3f PathRenderer::cast_ray(const PinholeCamera &camera, const vec3f &orig, const vec3f &dir, float &dist, size_t depth = 0) {
    vec3f hit, N;
    std::shared_ptr<Material> material;
    vec3f total_color, diffuse_intensity;

    // Perform the hit-test, saving the hit coordinates, angle, and material that was hit.
    if(depth > max_depth || !scene_intersect(orig, dir, hit, N, dist, material)) {
        // If we didn't hit anything, return only the background colour.
        return vec3f(0.2, 0.7, 0.8);
    }

    // Record the depth value as an inverse reciprocal of the actual distance.
    dist = 1.f - dist / (1.f + dist);

    compute_diffuse_intensity(hit, N, material->get_diffuse(), total_color);

    // If the material is specular, calculate it's specular highlights.
    if (material->type == "specular reflection") compute_specular_intensity(dir, hit, N, material->get_specular(), total_color);
    else if (material->type == "glossy reflection") compute_glossy(camera, dir, hit, N, material, depth, total_color);
    else if (material->type == "fresnel dielectric") compute_fresnel(camera, dir, hit, N, material, depth, total_color);

    return total_color;
}

bool PathRenderer::ray_intersect(const Sphere &sphere, const vec3f &orig, const vec3f &dir, float &t0) const {
    vec3f L = sphere.position - orig;
    float sq_radius = sphere.radius * sphere.radius;
    float tca = dot(L, dir);
    float d2 = dot(L, L) - tca * tca;

    if (d2 > sq_radius) return false;

    float thc = sqrtf(sq_radius - d2);
    t0 = tca - thc;
    float t1 = tca + thc;

    if (t0 < 1e-3) t0 = t1;
    if (t0 < 1e-3) return false;

    return true;
}

void from_json(const json &j, PathRenderer &r) {
    j.at("type").get_to(r.type);
    j.at("samples").get_to(r.samples);
    j.at("depth").get_to(r.depth);
    j.at("output").get_to(r.output);
    j.at("dimensions")[0].get_to(r.width);
    j.at("dimensions")[1].get_to(r.height);
}

void from_json(const json &j, std::shared_ptr<PathRenderer> &r) {
    r = std::make_shared<PathRenderer>();

    j.at("type").get_to(r->type);
    j.at("samples").get_to(r->samples);
    j.at("depth").get_to(r->depth);
    j.at("output").get_to(r->output);
    j.at("dimensions")[0].get_to(r->width);
    j.at("dimensions")[1].get_to(r->height);
}