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
     * 电介质材质（透明/折射）- 完整修复版
     * 实现正确的玻璃折射效果，支持完整的菲涅尔反射和透射
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
                refractiveIndex = 1.5f; // 默认玻璃折射率
            }

            auto colorProp = material.getProperty<Property::Wrapper::RGBType>("attenuation");
            if (colorProp) {
                attenuation = (*colorProp).value;
            }
            else {
                attenuation = Vec3(1.0f, 1.0f, 1.0f); // 完全透明
            }
        }

        /**
         * 着色方法 - 使用菲涅尔重要性采样
         */
        Scattered shade(const Ray& ray, const Vec3& hitPoint, const Vec3& normal) const override
        {
            Vec3 wo = -glm::normalize(ray.direction); // 出射方向（指向外部）
            bool fromOutside = glm::dot(wo, normal) > 0;
            float etaI = 1.0f; // 入射介质折射率（空气）
            float etaT = refractiveIndex; // 折射介质折射率
            Vec3 normalCorrected = normal;

            if (!fromOutside) {
                // 光线从内部射出，交换折射率
                std::swap(etaI, etaT);
                normalCorrected = -normal; // 使用内法线
            }

            // 计算菲涅尔反射率
            float cosThetaI = glm::abs(glm::dot(wo, normalCorrected));
            float reflectProb = fresnel(cosThetaI, etaI, etaT);

            // 重要性采样：根据菲涅尔反射率随机选择反射或折射
            float random = defaultSamplerInstance<UniformSampler>().sample1d();
            Vec3 wi;
            float pdf;

            if (random < reflectProb) {
                // 反射
                wi = reflect(wo, normalCorrected);
                pdf = reflectProb;
            }
            else {
                // 尝试折射
                float eta = etaI / etaT;
                wi = refract(wo, normalCorrected, eta);

                if (wi == Vec3(0, 0, 0)) {
                    // 全内反射，强制反射
                    wi = reflect(wo, normalCorrected);
                    pdf = 1.0f;
                    reflectProb = 1.0f;
                }
                else {
                    pdf = 1.0f - reflectProb;
                }
            }

            // 计算衰减
            Vec3 albedo = attenuation;

            if (random < reflectProb) {
                // 反射贡献
                albedo *= reflectProb;
            }
            else {
                // 折射贡献，考虑辐射传输的变换
                float eta = etaI / etaT;
                albedo *= (1.0f - reflectProb) * (eta * eta);
            }

            // 添加小偏移避免自相交
            Vec3 offset = normalCorrected * 0.001f;
            if (glm::dot(wi, normalCorrected) < 0) {
                offset = -offset; // 折射进入物体，偏移方向向内
            }

            return {
                Ray{hitPoint + offset, glm::normalize(wi)},
                albedo,
                Vec3{0}, // 自发光为0
                pdf
            };
        }

        /**
         * 直接光照计算 - 包括对光源的显式采样
         * 这是产生高亮点的关键
         */
        Vec3 evaluateDirectLighting(const Ray& ray, const Vec3& hitPoint, const Vec3& normal,
            const AreaLight& light, const Vec3& lightDir, float lightDistance) const override
        {
            Vec3 wo = -glm::normalize(ray.direction);
            Vec3 wi = lightDir;

            // 判断是反射还是折射
            bool fromOutside = glm::dot(wo, normal) > 0;
            float etaI = 1.0f;
            float etaT = refractiveIndex;
            Vec3 normalCorrected = normal;

            if (!fromOutside) {
                std::swap(etaI, etaT);
                normalCorrected = -normal;
            }

            // 计算BRDF
            Vec3 brdf = evaluateBRDF(wi, wo, normalCorrected, etaI, etaT);
            float cosTheta = glm::max(0.0f, glm::dot(normalCorrected, wi));

            // 计算光源面积
            float lightArea = glm::length(light.u) * glm::length(light.v);
            float lightPdf = 1.0f / lightArea;

            // 计算材质采样PDF
            float materialPdf = calculatePDF(wi, wo, normalCorrected, etaI, etaT);

            // 多重重要性采样权重（平衡启发式）
            float weight = 0.0f;
            if (materialPdf > 0.0f && lightPdf > 0.0f) {
                weight = (lightPdf * lightPdf) / (lightPdf * lightPdf + materialPdf * materialPdf);
            }
            else if (lightPdf > 0.0f) {
                weight = 1.0f;
            }

            // 距离衰减
            float distance2 = lightDistance * lightDistance;
            float attenuation = 1.0f / distance2;

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
         * 计算完整的介电BRDF - 修复版
         * 现在正确包含反射和折射的狄拉克delta函数
         */
        Vec3 evaluateBRDF(const Vec3& wi, const Vec3& wo, const Vec3& normal,
            float etaI, float etaT) const
        {
            // 计算精确的反射和折射方向
            Vec3 reflected = reflect(wo, normal);
            Vec3 refracted = refract(wo, normal, etaI / etaT);

            // 判断入射方向
            float reflectSimilarity = glm::dot(glm::normalize(wi), glm::normalize(reflected));
            float refractSimilarity = (refracted != Vec3(0)) ?
                glm::dot(glm::normalize(wi), glm::normalize(refracted)) : -1.0f;

            // 计算菲涅尔系数
            float cosThetaI = glm::abs(glm::dot(wo, normal));
            float F = fresnel(cosThetaI, etaI, etaT);

            // 狄拉克delta函数的BRDF表示
            // 在实际采样中，我们只需要在特定方向返回非零值
            if (reflectSimilarity > 0.9999f) {
                // wi是精确的反射方向
                return attenuation * F / (glm::abs(glm::dot(wi, normal)) + 1e-6f);
            }
            else if (refracted != Vec3(0) && refractSimilarity > 0.9999f) {
                // wi是精确的折射方向
                float eta = etaI / etaT;
                return attenuation * (1.0f - F) * (eta * eta) / (glm::abs(glm::dot(wi, normal)) + 1e-6f);
            }

            return Vec3(0.0f); // 不在精确方向上
        }

        /**
         * 计算采样PDF
         */
        float calculatePDF(const Vec3& wi, const Vec3& wo, const Vec3& normal,
            float etaI, float etaT) const
        {
            // 计算精确的反射和折射方向
            Vec3 reflected = reflect(wo, normal);
            Vec3 refracted = refract(wo, normal, etaI / etaT);

            // 判断方向
            float reflectSimilarity = glm::dot(glm::normalize(wi), glm::normalize(reflected));
            float refractSimilarity = (refracted != Vec3(0)) ?
                glm::dot(glm::normalize(wi), glm::normalize(refracted)) : -1.0f;

            // 计算菲涅尔系数
            float cosThetaI = glm::abs(glm::dot(wo, normal));
            float F = fresnel(cosThetaI, etaI, etaT);

            if (reflectSimilarity > 0.9999f) {
                return F; // 反射的PDF
            }
            else if (refracted != Vec3(0) && refractSimilarity > 0.9999f) {
                return 1.0f - F; // 折射的PDF
            }

            return 0.0f; // 不在精确方向上
        }

        /**
         * 精确的菲涅尔计算 - 施利克近似
         * 更高效且视觉上准确
         */
        float fresnel(float cosThetaI, float etaI, float etaT) const
        {
            // 处理全内反射
            float sinThetaI = std::sqrt(glm::max(0.0f, 1.0f - cosThetaI * cosThetaI));
            float sinThetaT = etaI / etaT * sinThetaI;

            if (sinThetaT >= 1.0f) {
                return 1.0f; // 全内反射
            }

            float cosThetaT = std::sqrt(glm::max(0.0f, 1.0f - sinThetaT * sinThetaT));

            // 施利克近似
            float R0 = ((etaI - etaT) / (etaI + etaT)) * ((etaI - etaT) / (etaI + etaT));
            return R0 + (1.0f - R0) * std::pow(1.0f - cosThetaI, 5.0f);
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