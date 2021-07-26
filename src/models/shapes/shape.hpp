#pragma once
#include <iostream>
#include "utils/vec.hpp"
#include "models/object.hpp"
#include "models/rays/ray.hpp"

using json = nlohmann::json;

class Shape : public TypedElement {
public:
    vec3f position;

    Shape(const std::string &id, const std::string &type, const vec3f &position) : TypedElement(id, type), position(position) {}
    Shape() {}
    ~Shape() {}

    virtual bool ray_intersect(const RayInfo &, float &t0) const { t0 = 0; return false; }
    virtual void getSurfaceProperties( const vec3f &, vec3f &hitNormal) const { hitNormal = vec3f(0, 0, 0); };
};

void from_json(const json &j, Shape &s);
void from_json(const json &j, std::shared_ptr<Shape> &s);
