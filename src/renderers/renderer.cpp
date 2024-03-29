#include <iostream>
#include <fstream>
#include <memory>
#include <filesystem>

#include "utils/vec.hpp"
#include "utils/utils.hpp"
#include "models/scene.hpp"
#include "models/lights/light.hpp"
#include "renderers/renderer.hpp"

namespace fs = std::filesystem;

#define SPEC_SHINE 16

static std::vector<std::shared_ptr<Light>> lights;
static std::vector<std::shared_ptr<Primitive>> primitives;

void Renderer::prepare(const Scene &s) {
    lights = s.lights;
    primitives = s.primitives;

    // Initialise frame buffers.
    framebuffer = std::vector<vec3f>(width * height);
    depthbuffer = std::vector<float>(width * height);
}

bool Renderer::scene_intersect(const RayInfo &ray, vec3f &hit, vec3f &N, float &dist, std::shared_ptr<Primitive> &out) {
    dist = std::numeric_limits<float>::max();

    // Iterate over each primitive.
    for (std::shared_ptr<Primitive> &primitive : primitives) {
        float dist_i;

        // If the primitive is hit, record it.
        if (primitive->shape->ray_intersect(ray, dist_i) && dist_i <= dist && dist_i > 1e-6) {
            dist = dist_i;
            hit = ray.orig + ray.dir * dist_i;
            primitive->shape->getSurfaceProperties(hit, N);
            out = primitive;
        }
    }

    // If the sphere is closer than the max draw distance, return true.
    return dist < 1000;
}

void Renderer::compute_diffuse_intensity(const std::shared_ptr<Light> &light, const RayInfo &ray, const vec3f &hit, const vec3f &N, vec3f &out) {
    // Calculate diffuse lighting from the given light source.
    vec3f light_dir, light_intensity;
    float light_distance;

    light->illuminate(ray, hit, N, light_dir, light_intensity, light_distance);

    compute_diffuse_intensity(light_dir, light_intensity, light_distance, ray, hit, N, out);
}

void Renderer::compute_diffuse_intensity(const vec3f &light_dir, const vec3f &light_intensity, const float &light_distance, const RayInfo &, const vec3f &hit, const vec3f &N, vec3f &out) {
    // Calculate shadows.
    // Offset the point to ensure it doesn't accidentally hit the same shape.
    vec3f shadow_orig = dot(light_dir, N) < 0 ? hit - N * 1e-3 : hit + N * 1e-3;
    // Temp variables.
    vec3f shadow_hit, shadow_N;
    float shadow_dist;
    std::shared_ptr<Primitive> tmpprimitive;

    // Check if a ray from this point to the current light is obscured by another object. If so, skip this light.
    if ((scene_intersect(RayInfo(shadow_orig, light_dir), shadow_hit, shadow_N, shadow_dist, tmpprimitive)))
        // If we did hit something, make sure it's not an emissive light object. If it is, we will not be in shadow since it emits light.
        if (tmpprimitive->type != PrimitiveType::PRIMITIVE_EMISSIVE && (shadow_hit - shadow_orig).norm() < light_distance)
            return;

    // Compute the specular intensity for this given light.
    out = out + light_intensity * std::max(0.f, dot(light_dir, N));
}

void Renderer::compute_specular_intensity(const std::shared_ptr<Light> &light, const RayInfo &ray, const vec3f &hit, const vec3f &N, vec3f &out) {
    vec3f light_dir, light_intensity;
    float light_distance;

    light->illuminate(ray, hit, N, light_dir, light_intensity, light_distance);

    // Calculate the reflected ray intensity.
    float ray_intensity = std::max(0.f, dot(reflect(light_dir, N), ray.dir));

    // Calculate the shine intensity for each component color.
    out = out + light_intensity * powf(ray_intensity, SPEC_SHINE);
}

void Renderer::compute_glossy(const Camera &camera, const RayInfo &ray, const vec3f &hit, const vec3f &N, const std::shared_ptr<Material> &material, size_t &depth, vec3f &out) {
    // Cast a reflection off the shape to determine the reflection.
    vec3f reflect_color = compute_reflection(camera, ray, hit, N, material, depth);

    out = out + reflect_color * (1.f - material->get_roughness());
}

void Renderer::compute_fresnel(const Camera &camera, const RayInfo &ray, const vec3f &hit, const vec3f &N, const std::shared_ptr<Material> &material, size_t &depth, vec3f &out) {
    vec3f refract_color, reflect_color;
    float kr, kt;

    // Calculate the ratio of reflection to transmission at this point.
    kt = fresnel(ray.dir, N, material->get_eta(), kr);

    // If the light is not completely reflected, simulate transmission.
    if (kr < 1) {
        // Simulate transmission.
        refract_color = compute_refraction(camera, ray, hit, N, material, depth);
    }

    // Simulate reflection.
    reflect_color = compute_reflection(camera, ray, hit, N, material, depth);

    // Apply the new colours, multiplying by the ratio of light reflected to transmitted.
    out = out + refract_color * kt + reflect_color * kr;
}

vec3f Renderer::compute_reflection(const Camera &camera, const RayInfo &srcRay, const vec3f &hit, const vec3f &N, const std::shared_ptr<Material> &material, size_t &depth) {
    // Simulate reflection.
    float reflect_dist;
    vec3f reflect_dir = reflect(srcRay.dir, N).normalize();
    vec3f reflect_orig = dot(reflect_dir, N) < 0 ? hit - N * 1e-3 : hit + N * 1e-3;

    return material->get_diffuse() * cast_ray(camera, RayInfo(reflect_orig, reflect_dir), reflect_dist, depth + 1);
}

vec3f Renderer::compute_refraction(const Camera &camera, const RayInfo &srcRay, const vec3f &hit, const vec3f &N, const std::shared_ptr<Material> &material, size_t &depth) {
    // Simulate transmission.
    float refract_dist;
    vec3f refract_dir = refract(srcRay.dir, N, material->get_eta()).normalize();
    vec3f refract_orig = dot(refract_dir, N) < 0 ? hit - N * 1e-3 : hit + N * 1e-3;

    return material->get_transmission() * cast_ray(camera, RayInfo(refract_orig, refract_dir), refract_dist, depth + 1);
}

void Renderer::get_paths(const std::string &filePath, fs::path &renderPath, fs::path &depthPath) {
    const fs::path oldCwd = fs::current_path();

    // Check to ensure the output path exists.
    if (!fs::exists("res/output")) fs::create_directory("res/output");

    // Change the current directory to the output path.
    fs::current_path("res/output");

    renderPath = fs::absolute(filePath);

    fs::current_path(oldCwd);

    depthPath = fs::path(renderPath);
    depthPath = depthPath.replace_filename(depthPath.stem().concat("-depth")).replace_extension(renderPath.extension());
}

void Renderer::save() {
    std::cout << "Saving final result..." << std::endl;

    std::ofstream ofs;
    fs::path renderPath, depthPath;

    get_paths(output, renderPath, depthPath);

    save_framebuffer(renderPath);
    save_depth(depthPath);
}

void Renderer::save_framebuffer(const fs::path &path) {
    std::cout << "Saving framebuffer..." << std::endl;

    std::ofstream ofs;

    ofs.open(path);
    ofs << "P6\n" << width << " " << height << "\n255\n";

    for (int i = 0; i < width * height; ++i) {
        ofs << (char)(std::clamp(framebuffer[i].x, 0.f, 255.f));
        ofs << (char)(std::clamp(framebuffer[i].y, 0.f, 255.f));
        ofs << (char)(std::clamp(framebuffer[i].z, 0.f, 255.f));
    }

    ofs.close();

    std::cout << "Done" << std::endl;
}

void Renderer::save_depth(const fs::path &path) {
    std::cout << "Saving depth buffer..." << std::endl;

    std::ofstream ofs;

    ofs.open(path);
    ofs << "P6\n" << width << " " << height << "\n255\n";

    for (int i = 0; i < width * height; ++i) {
        ofs << (char)(255.f * std::clamp(depthbuffer[i], 0.f, 1.f));
        ofs << (char)(255.f * std::clamp(depthbuffer[i], 0.f, 1.f));
        ofs << (char)(255.f * std::clamp(depthbuffer[i], 0.f, 1.f));
    }

    ofs.close();

    std::cout << "Done" << std::endl;
}

void from_json(const json &j, Renderer &r) {
    j.at("type").get_to(r.type);
    j.at("samples").get_to(r.samples);
    j.at("depth").get_to(r.depth);
    j.at("output").get_to(r.output);

    j.at("dimensions")[0].get_to(r.width);
    j.at("dimensions")[1].get_to(r.height);
}

void from_json(const json &j, std::shared_ptr<Renderer> &r) {
    r = std::make_shared<Renderer>();

    j.at("type").get_to(r->type);
    j.at("samples").get_to(r->samples);
    j.at("depth").get_to(r->depth);
    j.at("output").get_to(r->output);
    j.at("dimensions")[0].get_to(r->width);
    j.at("dimensions")[1].get_to(r->height);
}
