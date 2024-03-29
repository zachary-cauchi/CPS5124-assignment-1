#pragma once
#include <iostream>
#include <memory>

#include "models/primitives/primitive.hpp"
#include "models/materials/material.hpp"
#include "models/lights/area-light.hpp"
#include "utils/mat.hpp"
#include "models/shapes/shape.hpp"

struct EmitterPrimitive : Primitive {
    std::shared_ptr<AreaLight> light;

    EmitterPrimitive(const std::string &id, const std::shared_ptr<Shape> &shape, const std::shared_ptr<Material> &material, const std::shared_ptr<AreaLight> &light)
        : Primitive(id, PrimitiveType::PRIMITIVE_EMISSIVE, shape, material), light(light) {}

    vec3f get_emittance() {
        return light->intensity;
    }
};
