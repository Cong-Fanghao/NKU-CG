#pragma once
#ifndef __DIELECTRIC_HPP__
#define __DIELECTRIC_HPP__

#include "Shader.hpp"
#include "samplers/SamplerInstance.hpp"
#include "geometry/vec.hpp"
#include "Onb.hpp"
#include <cmath>

namespace SimplePathTracer
{
    /**
     * 电介质材质（透明/折射）- 修复版
     * 实现正确的玻璃折射效果，支持多重重要性采样
     */
    class Dielectric : public Shader
    {
    private:
        float refractiveIndex;  // 折射率
        Vec3 attenuation;       // 颜色衰减

    public:
        Dielectric(Material& material, vector<Texture>& textures)
            : Shader(material, textures)
        {
            auto refIndexProp = material.getProperty<Property::Wrapper::FloatType>("refractiveIndex");
            if (refIndexProp) {
                refractiveIndex = (*refIndexProp).value;
            }
            else {
                refractiveIndex = 1.5f;
            }

            auto colorProp = material.getProperty<Property::Wrapper::RGBType>("attenuation");
            if (colorProp) {
                attenuation = (*colorProp).value;
            }
            else {
                attenuation = Vec3(1.0f, 1.0f, 1.0f);
            }
        }

        /**
         * 修复的着色方法 - 使用正确的折射重要性采样
         */
        Scattered shade(const Ray& ray, const Vec3& hitPoint, const Vec3& normal) const override
        {
            Vec3 wo = -glm::normalize(ray.direction); // 出射方向（指向外部）
            Vec3 normalCorrected = normal;

            // 判断光线是从外部进入还是内部射出
            bool fromOutside = glm::dot(wo, normal) > 0;
            float etaI = 1.0f; // 入射介质折射率（空气）
            float etaT = refractiveIndex; // 折射介质折射率

            if (!fromOutside) {
                // 光线从内部射出，交换折射率
                std::swap(etaI, etaT);
                normalCorrected = -normal; // 使用内法线
            }

            float cosThetaI = glm::abs(glm::dot(wo, normalCorrected));
            float sinThetaI = std::sqrt(glm::max(0.0f, 1.0f - cosThetaI * cosThetaI));

            // 计算临界角（全内反射）
            float sinThetaT = etaI / etaT * sinThetaI;
            bool canRefract = sinThetaT <= 1.0f;

            // 计算菲涅尔反射率
            float reflectProb = canRefract ? fresnel(cosThetaI, etaI, etaT) : 1.0f;

            // 重要性采样：根据菲涅尔反射率随机选择反射或折射
            float random = defaultSamplerInstance<UniformSampler>().sample1d();
            Vec3 wi;
            float pdf;

            if (random < reflectProb) {
                // 反射采样
                wi = reflect(wo, normalCorrected);
                pdf = reflectProb; // 反射的PDF
            }
            else {
                // 折射采样
                if (canRefract) {
                    wi = refract(wo, normalCorrected, etaI / etaT);
                    pdf = 1.0f - reflectProb; // 折射的PDF
                }
                else {
                    // 全内反射，强制反射
                    wi = reflect(wo, normalCorrected);
                    pdf = 1.0f;
                    reflectProb = 1.0f;
                }
            }

            // 计算BRDF值（介电材质的BRDF包含菲涅尔项）
            Vec3 brdf = evaluateBRDF(wi, wo, normalCorrected, etaI, etaT);
            float cosTheta = glm::max(0.0f, glm::dot(normalCorrected, wi));

            // 正确的衰减计算
            Vec3 finalAttenuation = brdf * cosTheta / (pdf + 1e-6f);

            return {
                Ray{hitPoint + normalCorrected * 0.001f, wi}, // 偏移避免自相交
                finalAttenuation,
                Vec3{0},
                pdf
            };
        }

        /**
         * 改进的直接光照计算 - 包含多重重要性采样
         */
        Vec3 evaluateDirectLighting(const Ray& ray, const Vec3& hitPoint, const Vec3& normal,
            const AreaLight& light, const Vec3& lightDir, float lightDistance) const override
        {
            Vec3 wo = -glm::normalize(ray.direction);
            Vec3 wi = lightDir;
            Vec3 normalCorrected = glm::dot(wi, normal) > 0 ? normal : -normal;

            // 判断入射/折射介质
            bool fromOutside = glm::dot(wo, normalCorrected) > 0;
            float etaI = 1.0f;
            float etaT = refractiveIndex;
            if (!fromOutside) {
                std::swap(etaI, etaT);
            }

            // 计算BRDF
            Vec3 brdf = evaluateBRDF(wi, wo, normalCorrected, etaI, etaT);
            float cosTheta = glm::max(0.0f, glm::dot(normalCorrected, wi));

            // 光源采样贡献
            float lightPdf = 1.0f / (glm::length(light.u) * glm::length(light.v));
            float brdfPdf = calculatePDF(wi, wo, normalCorrected, etaI, etaT);

            // 多重重要性采样权重（平衡启发式）
            float weight = lightPdf / (lightPdf + brdfPdf + 1e-6f);

            float attenuation = 1.0f / (lightDistance * lightDistance);
            return weight * brdf * light.radiance * cosTheta * attenuation;
        }

        Vec3 getBRDF(const Vec3& wi, const Vec3& wo, const Vec3& normal) const override
        {
            Vec3 normalCorrected = glm::dot(wi, normal) > 0 ? normal : -normal;
            bool fromOutside = glm::dot(wo, normalCorrected) > 0;
            float etaI = 1.0f;
            float etaT = refractiveIndex;
            if (!fromOutside) {
                std::swap(etaI, etaT);
            }

            return evaluateBRDF(wi, wo, normalCorrected, etaI, etaT);
        }

    private:
        /**
         * 计算完整的介电BRDF
         */
        Vec3 evaluateBRDF(const Vec3& wi, const Vec3& wo, const Vec3& normal,
            float etaI, float etaT) const
        {
            float cosThetaI = glm::abs(glm::dot(wi, normal));
            float cosThetaO = glm::abs(glm::dot(wo, normal));

            // 菲涅尔反射率
            float F = fresnel(cosThetaI, etaI, etaT);

            // 介电材质的BRDF包含反射和折射贡献
            // 反射部分：F * delta(wi, reflect(wo))
            // 折射部分：(1-F) * (etaT^2/etaI^2) * delta(wi, refract(wo))
            // 这里我们返回一个近似的BRDF值
            return attenuation * F / (cosThetaI + 1e-6f);
        }

        /**
         * 计算采样PDF
         */
        float calculatePDF(const Vec3& wi, const Vec3& wo, const Vec3& normal,
            float etaI, float etaT) const
        {
            float cosThetaI = glm::abs(glm::dot(wi, normal));
            float F = fresnel(cosThetaI, etaI, etaT);

            Vec3 reflected = reflect(wo, normal);
            Vec3 refracted = refract(wo, normal, etaI / etaT);

            // 检查wi是接近反射方向还是折射方向
            float reflectSimilarity = glm::dot(glm::normalize(wi), glm::normalize(reflected));
            float refractSimilarity = refracted != Vec3(0) ?
                glm::dot(glm::normalize(wi), glm::normalize(refracted)) : -1.0f;

            if (reflectSimilarity > 0.99f) {
                return F; // wi接近反射方向
            }
            else if (refractSimilarity > 0.99f) {
                return 1.0f - F; // wi接近折射方向
            }

            return 0.0f; // wi不在主要方向上
        }

        /**
         * 精确的菲涅尔计算
         */
        float fresnel(float cosThetaI, float etaI, float etaT) const
        {
            float sinThetaI = std::sqrt(glm::max(0.0f, 1.0f - cosThetaI * cosThetaI));
            float sinThetaT = etaI / etaT * sinThetaI;

            if (sinThetaT >= 1.0f) {
                return 1.0f; // 全内反射
            }

            float cosThetaT = std::sqrt(glm::max(0.0f, 1.0f - sinThetaT * sinThetaT));

            float rParallel = ((etaT * cosThetaI) - (etaI * cosThetaT)) /
                ((etaT * cosThetaI) + (etaI * cosThetaT));
            float rPerp = ((etaI * cosThetaI) - (etaT * cosThetaT)) /
                ((etaI * cosThetaI) + (etaT * cosThetaT));

            return (rParallel * rParallel + rPerp * rPerp) * 0.5f;
        }

        /**
         * 反射计算
         */
        Vec3 reflect(const Vec3& v, const Vec3& n) const
        {
            return v - 2.0f * glm::dot(v, n) * n;
        }

        /**
         * 折射计算 - 返回折射方向，如果全内反射返回零向量
         */
        Vec3 refract(const Vec3& v, const Vec3& n, float eta) const
        {
            Vec3 uv = glm::normalize(v);
            float dt = glm::dot(uv, n);
            float discriminant = 1.0f - eta * eta * (1.0f - dt * dt);

            if (discriminant > 0) {
                return eta * (uv - n * dt) - n * std::sqrt(discriminant);
            }

            return Vec3(0, 0, 0); // 全内反射
        }
    };
}

#endif