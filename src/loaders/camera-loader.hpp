#pragma once
#include <iostream>
#include <nlohmann/json.hpp>
#include "utils/vec.hpp"
#include "models/cameras/camera.hpp"
#include "models/cameras/pinhole.hpp"
#include "models/cameras/lens-camera.hpp"

using json = nlohmann::json;

void LoadCameras(const json &j, std::vector<Camera> &cameras) {
    cameras = std::vector<Camera>();

    if (!j["cameras"].empty()) {
        for (json raw_cam : j["cameras"]) {
            if (raw_cam.at("type") == "pinhole") {
                cameras.push_back(raw_cam.get<PinholeCamera>());
            } else if (raw_cam.at("type") == "lens-based") {
                cameras.push_back(raw_cam.get<LensCamera>());
            } else {
                cameras.push_back(raw_cam.get<Camera>());
            }
        }
    }
}

void LoadCameras(const json &j, std::vector<std::shared_ptr<Camera>> &cameras) {
    cameras = std::vector<std::shared_ptr<Camera>>();

    if (!j.at("cameras").empty()) {
        for (json cam : j.at("cameras")) {
            if (cam.at("type") == "pinhole") {
                cameras.push_back(cam.get<std::shared_ptr<PinholeCamera>>());
            } else if (cam.at("type") == "lens-based") {
                cameras.push_back(cam.get<std::shared_ptr<LensCamera>>());
            } else {
                cameras.push_back(cam.get<std::shared_ptr<Camera>>());
            }
        }
    }
}
