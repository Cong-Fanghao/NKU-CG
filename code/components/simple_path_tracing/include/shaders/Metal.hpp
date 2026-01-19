#pragma once
#ifndef __METAL_HPP__
#define __METAL_HPP__

#include "Shader.hpp"
#include "samplers/SamplerInstance.hpp"
#include "geometry/vec.hpp"
#include "Onb.hpp"

namespace SimplePathTracer
{
    class Metal : public Shader
    {
    private:
        Vec3 albedo;
        float roughness;

    public:
        Metal(Material& material, vector<Texture>& textures)
            : Shader(material, textures)
        {
            auto metalColor = material.getProperty<Property::Wrapper::RGBType>("albedo");
            if (metalColor) albedo = (*metalColor).value;
            else albedo = Vec3(0.8f, 0.8f, 0.8f);

            auto rough = material.getProperty<Property::Wrapper::FloatType>("roughness");
            if (rough) roughness = glm::clamp((*rough).value, 0.0f, 1.0f);
            else roughness = 0.0f;
        }

        Scattered shade(const Ray& ray, const Vec3& hitPoint, const Vec3& normal) const override
        {
            Vec3 origin = hitPoint;
            Vec3 incident = glm::normalize(ray.direction);

            // 计算完美反射方向
            Vec3 perfectReflectDir = reflect(incident, normal);

            // 根据粗糙度扰动反射方向
            Vec3 finalDirection = perturbDirection(perfectReflectDir, roughness);

            if (glm::dot(finalDirection, normal) < 0) {
                finalDirection = perfectReflectDir;
            }

            Vec3 attenuation = albedo;
            float pdf = 1.0f;

            return {
                Ray{origin, finalDirection},
                attenuation,
                Vec3{0},
                pdf
            };
        }

        Vec3 evaluateDirectLighting(const Ray& ray, const Vec3& hitPoint, const Vec3& normal,
            const AreaLight& light, const Vec3& lightDir, float lightDistance) const {
            Vec3 incident = glm::normalize(ray.direction);
            Vec3 perfectReflectDir = reflect(incident, normal);

            // 计算镜面反射方向与光源方向的一致性
            float reflectionAlignment = glm::max(0.0f, glm::dot(perfectReflectDir, lightDir));

            // 考虑粗糙度的影响
            float specular = std::pow(reflectionAlignment, 1.0f / (roughness + 0.001f));

            // 距离衰减
            float attenuation = 1.0f / (lightDistance * lightDistance);

            return albedo * light.radiance * specular * attenuation;
        }

        Vec3 getBRDF(const Vec3& wi, const Vec3& wo, const Vec3& normal) const {
            // 简化的金属BRDF近似
            Vec3 perfectReflectDir = reflect(wo, normal);
            float alignment = glm::max(0.0f, glm::dot(perfectReflectDir, wi));
            float specular = std::pow(alignment, 1.0f / (roughness + 0.001f));
            return albedo * specular;
        }

    private:
        Vec3 reflect(const Vec3& v, const Vec3& n) const
        {
            return v - 2.0f * glm::dot(v, n) * n;
        }

        Vec3 perturbDirection(const Vec3& perfectReflect, float roughness) const
        {
            if (roughness < 0.001f) {
                return perfectReflect;
            }

            Vec3 random = defaultSamplerInstance<HemiSphere>().sample3d();
            Onb onb{ perfectReflect };
            Vec3 perturbed = glm::normalize(onb.local(random));

            return glm::normalize(glm::mix(perfectReflect, perturbed, roughness));
        }
    };
}

#endif