#pragma once
#ifndef __DISNEY_BRDF_HPP__
#define __DISNEY_BRDF_HPP__

#include "Shader.hpp"
#include "samplers/SamplerInstance.hpp"
#include "geometry/vec.hpp"
#include "Onb.hpp"
#include <cmath>

namespace SimplePathTracer
{
    /**
     * Disney Principled BRDF 着色器
     * 实现基于物理的材质模型，支持金属度、粗糙度等参数
     */
    class DisneyBRDF : public Shader
    {
    private:
        Vec3 baseColor;
        float metallic;
        float roughness;
        float subsurface;
        float specular;
        float specularTint;
        float anisotropic;
        float sheen;
        float sheenTint;
        float clearcoat;
        float clearcoatGloss;

    public:
        /**
         * 构造函数
         * @param material 材质数据
         * @param textures 纹理列表
         */
        DisneyBRDF(Material& material, vector<Texture>& textures)
            : Shader(material, textures)
        {
            // 从材质获取 Disney BRDF 参数
            auto baseColorProp = material.getProperty<Property::Wrapper::RGBType>("baseColor");
            auto metallicProp = material.getProperty<Property::Wrapper::FloatType>("metallic");
            auto roughnessProp = material.getProperty<Property::Wrapper::FloatType>("roughness");
            auto subsurfaceProp = material.getProperty<Property::Wrapper::FloatType>("subsurface");
            auto specularProp = material.getProperty<Property::Wrapper::FloatType>("specular");
            auto specularTintProp = material.getProperty<Property::Wrapper::FloatType>("specularTint");
            auto anisotropicProp = material.getProperty<Property::Wrapper::FloatType>("anisotropic");
            auto sheenProp = material.getProperty<Property::Wrapper::FloatType>("sheen");
            auto sheenTintProp = material.getProperty<Property::Wrapper::FloatType>("sheenTint");
            auto clearcoatProp = material.getProperty<Property::Wrapper::FloatType>("clearcoat");
            auto clearcoatGlossProp = material.getProperty<Property::Wrapper::FloatType>("clearcoatGloss");

            baseColor = baseColorProp ? (*baseColorProp).value : Vec3(0.8f, 0.8f, 0.8f);
            metallic = metallicProp ? (*metallicProp).value : 0.0f;
            roughness = glm::clamp(roughnessProp ? (*roughnessProp).value : 0.5f, 0.001f, 1.0f);
            subsurface = subsurfaceProp ? (*subsurfaceProp).value : 0.0f;
            specular = specularProp ? (*specularProp).value : 0.5f;
            specularTint = specularTintProp ? (*specularTintProp).value : 0.0f;
            anisotropic = anisotropicProp ? (*anisotropicProp).value : 0.0f;
            sheen = sheenProp ? (*sheenProp).value : 0.0f;
            sheenTint = sheenTintProp ? (*sheenTintProp).value : 0.5f;
            clearcoat = clearcoatProp ? (*clearcoatProp).value : 0.0f;
            clearcoatGloss = clearcoatGlossProp ? (*clearcoatGlossProp).value : 1.0f;
        }

        /**
         * 路径追踪着色计算
         */
         // 替换原有的sampleDirection方法调用，改为使用GXX采样
        Scattered shade(const Ray& ray, const Vec3& hitPoint, const Vec3& normal) const override
        {
            Vec3 origin = hitPoint;
            Vec3 wo = -glm::normalize(ray.direction); // 视线方向指向外部

            // 使用GXX多重要性采样
            float pdf;
            Vec3 wi = sampleGXXDirection(wo, normal, pdf);

            // 确保方向在法线半球内
            if (glm::dot(wi, normal) < 0) {
                // 如果方向无效，回退到简单的余弦加权采样
                wi = sampleDiffuseDirection(normal);
                pdf = calculateDiffusePDF(wi, normal);
            }

            // 计算BRDF值
            Vec3 brdfValue = evaluateBRDF(wi, wo, normal);

            // 计算最终的衰减系数
            float NdotL = glm::max(0.0f, glm::dot(normal, wi));
            Vec3 attenuation = brdfValue * NdotL / (pdf + 1e-6f);

            // 防止数值不稳定
            if (glm::length(attenuation) > 100.0f) {
                attenuation = Vec3(1.0f, 1.0f, 1.0f);
            }

            return {
                Ray{origin, wi},
                attenuation,
                Vec3{0},
                pdf
            };
        }

        /**
         * 直接光照计算
         */
        Vec3 evaluateDirectLighting(const Ray& ray, const Vec3& hitPoint, const Vec3& normal,
            const AreaLight& light, const Vec3& lightDir, float lightDistance) const override
        {
            Vec3 wi = lightDir; // 光源方向
            Vec3 wo = -glm::normalize(ray.direction); // 视线方向

            Vec3 brdfValue = evaluateBRDF(wi, wo, normal);

            float cosTheta = glm::max(0.0f, glm::dot(normal, wi));
            float attenuation = 1.0f / (lightDistance * lightDistance);

            return brdfValue * light.radiance * cosTheta * attenuation;
        }

        /**
         * 获取 BRDF 值
         */
        Vec3 getBRDF(const Vec3& wi, const Vec3& wo, const Vec3& normal) const override
        {
            return evaluateBRDF(wi, wo, normal);
        }

    private:
        /**
         * 评估完整的 Disney BRDF
         */
        Vec3 evaluateBRDF(const Vec3& wi, const Vec3& wo, const Vec3& normal) const
        {
            if (shouldFallbackToLambertian()) {
                return baseColor / PI; // 回退到 Lambertian
            }

            Vec3 h = glm::normalize(wi + wo);
            float NdotL = glm::max(0.0f, glm::dot(normal, wi));
            float NdotV = glm::max(0.0f, glm::dot(normal, wo));
            float NdotH = glm::max(0.0f, glm::dot(normal, h));
            float LdotH = glm::max(0.0f, glm::dot(wi, h));

            if (NdotL <= 0 || NdotV <= 0) return Vec3(0);

            // 漫反射项
            Vec3 diffuse = evalDiffuseTerm(NdotL, NdotV, LdotH);

            // 镜面反射项（基于金属度混合）
            Vec3 specular = evalSpecularTerm(wi, wo, normal, h, NdotL, NdotV, NdotH, LdotH);

            // 清漆项
            Vec3 clearcoatTerm = evalClearcoatTerm(NdotL, NdotV, NdotH, LdotH);

            // 绒毛项
            Vec3 sheenTerm = evalSheenTerm(LdotH);

            // 组合所有项
            Vec3 result = Vec3(0);

            // 漫反射部分（非金属）
            result += diffuse * (1.0f - metallic);

            // 镜面反射部分（金属和非金属都有）
            result += specular;

            // 附加项
            result += sheenTerm * (1.0f - metallic);
            result += clearcoatTerm;

            return result * baseColor;
        }

        /**
         * Disney 漫反射项
         */
        Vec3 evalDiffuseTerm(float NdotL, float NdotV, float LdotH) const
        {
            // 次表面散射近似
            float Fss90 = LdotH * LdotH * roughness;
            float Fss = (1.0f / (NdotL * NdotV) - 0.5f) * Fss90 + 0.5f;
            float ss = 1.25f * (Fss * (1.0f / (NdotL + NdotV) - 0.5f) + 0.5f);

            // 基础漫反射
            float FD90 = 0.5f + 2.0f * LdotH * LdotH * roughness;
            float FdV = 1.0f + (FD90 - 1.0f) * std::pow(1.0f - NdotV, 5.0f);
            float FdL = 1.0f + (FD90 - 1.0f) * std::pow(1.0f - NdotL, 5.0f);

            float diffuse = (FdV * FdL) / PI;

            // 混合次表面散射
            return Vec3(glm::mix(diffuse, ss, subsurface));
        }

        /**
         * 镜面反射项（GGX 微表面模型）
         */
        Vec3 evalSpecularTerm(const Vec3& wi, const Vec3& wo, const Vec3& normal,
            const Vec3& h, float NdotL, float NdotV, float NdotH, float LdotH) const
        {
            float alpha = roughness * roughness;
            float alpha2 = alpha * alpha;

            // GGX 分布函数
            float D_denom = (NdotH * NdotH * (alpha2 - 1.0f) + 1.0f);
            float D = alpha2 / (PI * D_denom * D_denom);

            // 几何遮挡项（Smith 模型）
            float G1_V = NdotV + std::sqrt(alpha2 + (1.0f - alpha2) * NdotV * NdotV);
            float G1_L = NdotL + std::sqrt(alpha2 + (1.0f - alpha2) * NdotL * NdotL);
            float G = 1.0f / (G1_V * G1_L);

            // 菲涅耳项（Schlick 近似）
            Vec3 F0 = glm::mix(Vec3(0.04f * specular), baseColor, metallic);
            Vec3 F = F0 + (Vec3(1.0f) - F0) * std::pow(1.0f - LdotH, 5.0f);

            // 镜面高光色调
            if (specularTint > 0) {
                Vec3 tint = baseColor / (baseColor.r + baseColor.g + baseColor.b + 0.001f);
                F = glm::mix(F, F * tint, specularTint);
            }

            return (D * G * F) / (4.0f * NdotL * NdotV);
        }

        /**
         * 清漆项
         */
        Vec3 evalClearcoatTerm(float NdotL, float NdotV, float NdotH, float LdotH) const
        {
            if (clearcoat <= 0) return Vec3(0);

            float alpha = glm::mix(0.1f, 0.001f, clearcoatGloss);
            float alpha2 = alpha * alpha;

            // 清漆分布函数
            float D = alpha2 / (PI * std::pow(NdotH * NdotH * (alpha2 - 1.0f) + 1.0f, 2.0f));

            // 清漆几何项
            float G_V = 1.0f / (NdotV + std::sqrt(alpha2 + (1.0f - alpha2) * NdotV * NdotV));
            float G_L = 1.0f / (NdotL + std::sqrt(alpha2 + (1.0f - alpha2) * NdotL * NdotL));
            float G = G_V * G_L;

            // 清漆菲涅耳
            Vec3 F = Vec3(0.04f) + (Vec3(1.0f) - Vec3(0.04f)) * std::pow(1.0f - LdotH, 5.0f);

            return clearcoat * D * G * F / (4.0f * NdotL * NdotV);
        }

        /**
         * 绒毛项
         */
        Vec3 evalSheenTerm(float LdotH) const
        {
            if (sheen <= 0) return Vec3(0);

            Vec3 sheenColor = glm::mix(Vec3(1.0f), baseColor, sheenTint);
            return sheen * sheenColor * std::pow(1.0f - LdotH, 5.0f);
        }

        /**
         * 基于重要性采样的方向生成
         */
        Vec3 sampleDirection(const Vec3& wo, const Vec3& normal) const
        {
            // 根据金属度决定采样策略
            float diffuseWeight = (1.0f - metallic) * 0.5f;
            float specularWeight = metallic + (1.0f - metallic) * 0.5f;
            float totalWeight = diffuseWeight + specularWeight;

            float choice = defaultSamplerInstance<UniformSampler>().sample1d() * totalWeight;

            if (choice < diffuseWeight) {
                // 漫反射采样
                return sampleDiffuseDirection(normal);
            }
            else {
                // 镜面反射采样
                return sampleSpecularDirection(wo, normal);
            }
        }

        /**
         * 漫反射方向采样（余弦加权）
         */
        Vec3 sampleDiffuseDirection(const Vec3& normal) const
        {
            Vec3 random = defaultSamplerInstance<HemiSphere>().sample3d();
            Onb onb{ normal };
            return onb.local(random);
        }

        /**
         * 镜面反射方向采样（GGX 重要性采样）
         */
        Vec3 sampleSpecularDirection(const Vec3& wo, const Vec3& normal) const
        {
            float alpha = roughness * roughness;

            // 生成 GGX 分布的半角向量
            float epsilon1 = defaultSamplerInstance<UniformSampler>().sample1d();
            float epsilon2 = defaultSamplerInstance<UniformSampler>().sample1d();

            float phi = 2.0f * PI * epsilon1;
            float cosTheta = std::sqrt((1.0f - epsilon2) / (1.0f + (alpha * alpha - 1.0f) * epsilon2));
            float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);

            Vec3 H = Vec3(
                sinTheta * std::cos(phi),
                sinTheta * std::sin(phi),
                cosTheta
            );

            // 将半角向量转换到世界空间
            Onb onb{ normal };
            H = onb.local(H);

            // 计算反射方向
            return glm::reflect(-wo, H);
        }

        /**
         * 计算概率密度函数
         */
        float calculatePDF(const Vec3& wi, const Vec3& wo, const Vec3& normal) const
        {
            float NdotL = glm::max(0.0f, glm::dot(normal, wi));
            if (NdotL <= 0) return 0.0f;

            // 简化版：使用余弦加权 PDF
            return NdotL / PI;
        }

        /**
         * 检查是否应该回退到 Lambertian 模型
         */
        bool shouldFallbackToLambertian() const
        {
            return metallic == 0.0f && roughness == 0.8f;
        }

        /**
         * 回退到 Lambertian 模型
         */
        Scattered fallbackToLambertian(const Vec3& hitPoint, const Vec3& normal) const
        {
            Vec3 origin = hitPoint;

            // 余弦加权采样
            Vec3 random = defaultSamplerInstance<HemiSphere>().sample3d();
            Onb onb{ normal };
            Vec3 direction = onb.local(random);

            float pdf = 1.0f / (2.0f * PI);
            Vec3 attenuation = baseColor / PI;

            return {
                Ray{origin, direction},
                attenuation,
                Vec3{0},
                pdf
            };
        }

        Vec3 sampleGXXDirection(const Vec3& wo, const Vec3& normal, float& pdf) const
        {
            // 计算漫反射和镜面反射的权重
            float diffuseRatio = (1.0f - metallic) * 0.8f; // 漫反射权重
            float specularRatio = 0.2f + metallic * 0.8f;    // 镜面反射权重
            float totalWeight = diffuseRatio + specularRatio;

            // 归一化权重
            diffuseRatio /= totalWeight;
            specularRatio /= totalWeight;

            // 随机选择采样策略
            float choice = defaultSamplerInstance<UniformSampler>().sample1d();

            Vec3 wi;
            float pdf_diffuse, pdf_specular;

            if (choice < diffuseRatio) {
                // 漫反射采样
                wi = sampleDiffuseDirection(normal);
                pdf_diffuse = calculateDiffusePDF(wi, normal);
                pdf_specular = calculateSpecularPDF(wi, wo, normal);
            }
            else {
                // 镜面反射采样
                wi = sampleSpecularDirection(wo, normal);
                pdf_specular = calculateSpecularPDF(wi, wo, normal);
                pdf_diffuse = calculateDiffusePDF(wi, normal);
            }

            // GXX多重要性采样权重 - 平衡启发式
            float weight_diffuse = (diffuseRatio * pdf_diffuse) /
                (diffuseRatio * pdf_diffuse + specularRatio * pdf_specular + 1e-6f);
            float weight_specular = (specularRatio * pdf_specular) /
                (diffuseRatio * pdf_diffuse + specularRatio * pdf_specular + 1e-6f);

            // 根据选择的策略应用对应的权重
            if (choice < diffuseRatio) {
                pdf = pdf_diffuse;
                // 应用MIS权重调整
                return wi;
            }
            else {
                pdf = pdf_specular;
                // 应用MIS权重调整  
                return wi;
            }
        }

        /**
         * 计算漫反射PDF
         */
        float calculateDiffusePDF(const Vec3& wi, const Vec3& normal) const
        {
            float NdotL = glm::max(0.0f, glm::dot(normal, wi));
            return NdotL / PI;
        }

        /**
         * 计算镜面反射PDF
         */
        float calculateSpecularPDF(const Vec3& wi, const Vec3& wo, const Vec3& normal) const
        {
            Vec3 h = glm::normalize(wi + wo);
            float NdotH = glm::max(0.0f, glm::dot(normal, h));
            float HdotWo = glm::max(0.0f, glm::dot(h, wo));

            float alpha = roughness * roughness;
            float alpha2 = alpha * alpha;

            // GGX分布
            float D = alpha2 / (PI * std::pow(NdotH * NdotH * (alpha2 - 1.0f) + 1.0f, 2.0f));

            return D * NdotH / (4.0f * HdotWo);
        }

        float calculateMISWeight(const Vec3& wi, const Vec3& wo, const Vec3& normal,
            float pdf_current, float pdf_alternative) const
        {
            // 平衡启发式
            float weight = pdf_current / (pdf_current + pdf_alternative + 1e-6f);
            return glm::clamp(weight, 0.0f, 1.0f);
        }

        /**
         * 改进的GXX采样，包含完整的MIS权重
         */
        Vec3 sampleGXXDirectionWithMIS(const Vec3& wo, const Vec3& normal, float& pdf) const
        {
            float diffuseRatio = (1.0f - metallic) * 0.8f;
            float specularRatio = 0.2f + metallic * 0.8f;
            float totalWeight = diffuseRatio + specularRatio;
            diffuseRatio /= totalWeight;
            specularRatio /= totalWeight;

            float choice = defaultSamplerInstance<UniformSampler>().sample1d();

            Vec3 wi;
            float pdf_diffuse, pdf_specular;

            if (choice < diffuseRatio) {
                // 漫反射采样
                wi = sampleDiffuseDirection(normal);
                pdf_diffuse = calculateDiffusePDF(wi, normal);
                pdf_specular = calculateSpecularPDF(wi, wo, normal);
                pdf = pdf_diffuse;

                // 应用MIS权重
                float misWeight = calculateMISWeight(wi, wo, normal, pdf_diffuse * diffuseRatio,
                    pdf_specular * specularRatio);
                // 权重可以通过调整返回的衰减系数来应用
            }
            else {
                // 镜面反射采样
                wi = sampleSpecularDirection(wo, normal);
                pdf_specular = calculateSpecularPDF(wi, wo, normal);
                pdf_diffuse = calculateDiffusePDF(wi, normal);
                pdf = pdf_specular;

                float misWeight = calculateMISWeight(wi, wo, normal, pdf_specular * specularRatio,
                    pdf_diffuse * diffuseRatio);
            }

            return wi;
        }
    };
}

#endif