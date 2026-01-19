#pragma once
#ifndef __TEXTUREDLAMBERTIAN_HPP__
#define __TEXTUREDLAMBERTIAN_HPP__

#include "Shader.hpp"
#include "geometry/vec.hpp"
#include "scene/Scene.hpp"
#include "samplers/SamplerInstance.hpp"
#include "Onb.hpp"
#include <cmath>

#ifndef M_PI
    #define M_PI (float)3.1415926535
#endif

namespace SimplePathTracer
{
    /**
     * 支持纹理的Lambertian材质
     * 在漫反射基础上添加纹理支持
     */
    class TexturedLambertian : public Shader
    {
    private:
        Vec3 baseColor;           // 基础颜色（纹理缺失时使用）
        int textureId;             // 纹理ID
        bool hasTexture;           // 是否有纹理

    public:
        TexturedLambertian(Material& material, vector<Texture>& textures)
            : Shader(material, textures)
            , textureId(-1)
            , hasTexture(false)
        {
            // 获取基础颜色
            auto colorProp = material.getProperty<Property::Wrapper::RGBType>("diffuseColor");
            if (colorProp) {
                baseColor = (*colorProp).value;
            }
            else {
                baseColor = Vec3(0.5f, 0.5f, 0.5f); // 默认灰色
            }

            // 获取纹理ID
            auto texProp = material.getProperty<Property::Wrapper::IntType>("textureId");
            if (texProp) {
                textureId = (*texProp).value;
                if (textureId >= 0 && textureId < textures.size()) {
                    hasTexture = true;
                }
            }
        }

        Scattered shade(const Ray& ray, const Vec3& hitPoint, const Vec3& normal) const override
        {
            Vec3 origin = hitPoint;

            // 使用半球采样器生成随机方向
            Vec3 random = defaultSamplerInstance<HemiSphere>().sample3d();
            Onb onb{ normal };
            Vec3 direction = onb.local(random);

            // 计算纹理颜色
            Vec3 finalColor = baseColor;
            if (hasTexture) {
                // 简化：使用世界坐标的XZ平面作为UV坐标
                float u = fmod(hitPoint.x * 0.01f, 1.0f);
                float v = fmod(hitPoint.z * 0.01f, 1.0f);
                if (u < 0) u += 1.0f;
                if (v < 0) v += 1.0f;

                // 这里需要实现纹理采样
                // finalColor = textureBuffer[textureId].sample(u, v);
                // 暂时使用基础颜色
                finalColor = baseColor * (0.7f + 0.3f * sin(hitPoint.x * 0.1f) * cos(hitPoint.z * 0.1f));
            }

            float pdf = 1.0f / (2 * M_PI);
            Vec3 attenuation = finalColor / M_PI;

            return {
                Ray{origin, direction},
                attenuation,
                Vec3{0},
                pdf
            };
        }

        Vec3 evaluateDirectLighting(const Ray& ray, const Vec3& hitPoint, const Vec3& normal,
            const AreaLight& light, const Vec3& lightDir, float lightDistance) const
        {
            // 获取最终颜色（考虑纹理）
            Vec3 finalColor = getFinalColor(hitPoint, normal);

            // Lambertian BRDF：finalColor / π
            Vec3 brdf = finalColor / M_PI;

            // 计算余弦项
            float cosTheta = glm::max(0.0f, glm::dot(normal, lightDir));

            // 计算距离衰减
            float attenuation = 1.0f / (lightDistance * lightDistance);

            // 直接光照贡献 = BRDF * 光源辐射度 * 余弦项 * 距离衰减
            return brdf * light.radiance * cosTheta * attenuation;
        }

        Vec3 getBRDF(const Vec3& wi, const Vec3& wo, const Vec3& normal) const
        {
            // Lambertian BRDF是常数：finalColor / π
            // 注意：这里我们使用命中点的颜色，但实际应该基于具体的wi和wo
            // 对于Lambertian材质，BRDF是常数，不依赖于方向
            Vec3 finalColor = baseColor; // 简化处理，使用基础颜色
            return finalColor / M_PI;
        }

        Vec2 computeUV(const Vec3& hitPoint, const Vec3& normal) const
        {
            // 基于法线主方向选择投影平面
            Vec3 absNormal = glm::abs(normal);
            Vec2 uv;

            if (absNormal.x > absNormal.y && absNormal.x > absNormal.z) {
                // 使用YZ平面
                uv = Vec2(hitPoint.y, hitPoint.z);
            }
            else if (absNormal.y > absNormal.x && absNormal.y > absNormal.z) {
                // 使用XZ平面
                uv = Vec2(hitPoint.x, hitPoint.z);
            }
            else {
                // 使用XY平面
                uv = Vec2(hitPoint.x, hitPoint.y);
            }

            // 将UV坐标归一化到[0,1]范围
            uv = uv * 0.01f;  // 缩放系数
            uv.x = uv.x - std::floor(uv.x);
            uv.y = uv.y - std::floor(uv.y);

            if (uv.x < 0) uv.x += 1.0f;
            if (uv.y < 0) uv.y += 1.0f;

            return uv;
        }

        Vec3 getFinalColor(const Vec3& hitPoint, const Vec3& normal) const
        {
            Vec3 finalColor = baseColor;

            if (hasTexture && textureId >= 0 && textureId < textureBuffer.size()) {
                // 计算UV坐标
                Vec2 uv = computeUV(hitPoint, normal);

                // 这里需要实现纹理采样
                // 暂时使用程序化纹理作为替代
                finalColor = baseColor * (0.7f + 0.3f * sin(hitPoint.x * 0.1f) * cos(hitPoint.z * 0.1f));

                // 未来实现纹理采样：
                // finalColor = textureBuffer[textureId].sample(uv.x, uv.y);
            }

            return finalColor;
        }
    };
}

#endif