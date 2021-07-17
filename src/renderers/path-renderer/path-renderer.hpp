#pragma once

#include "models/cameras/camera.hpp"
#include "models/materials/diffuse.hpp"
#include "renderers/renderer.hpp"
#include "models/primitive.hpp"

class PathRenderer : public Renderer {
public:
    PathRenderer(const std::string &type, const std::string &output, const vec2i &dimensions, int samples, int depth)
        : Renderer(type, output, dimensions, samples, depth) {}

    PathRenderer() : Renderer() {}
    ~PathRenderer() {}

    void prepare(const Scene &scene);
    void render(const std::shared_ptr<Camera> &camera);

    vec3f cast_ray(const PinholeCamera &camera, const vec3f &orig, const vec3f &dir, float &dist, size_t depth);
    bool scene_intersect(const vec3f &orig, const vec3f &dir, vec3f &hit, vec3f &N, float &dist, std::shared_ptr<Material> &material);
    bool ray_intersect(const Sphere &sphere, const vec3f &orig, const vec3f &dir, float &t0) const;

    void compute_diffuse_intensity(const vec3f &hit, const vec3f &N, const vec3f &in_color, vec3f &out);
    void compute_specular_intensity(const vec3f &dir, const vec3f &hit, const vec3f &N, const vec3f &in_color, vec3f &out);
    void compute_glossy(const PinholeCamera &camera, const vec3f &dir, const vec3f &hit, const vec3f &N, const std::shared_ptr<Material> &material, size_t &depth, vec3f &out);
    void compute_fresnel(const PinholeCamera &camera, const vec3f &dir, const vec3f &hit, const vec3f &N, const std::shared_ptr<Material> &material, size_t &depth, vec3f &out);
};

void from_json(const json &j, PathRenderer &r);
void from_json(const json &j, std::shared_ptr<PathRenderer> &r);