#pragma once
#ifndef __MARBLE_HPP__
#define __MARBLE_HPP__

#include "Shader.hpp"
#include "geometry/vec.hpp"
#include "samplers/SamplerInstance.hpp"
#include "Onb.hpp"
#include <cmath>
#include <algorithm>

namespace SimplePathTracer
{
    /**
     * 大理石材质
     * 在平面上生成明显的大理石纹理展示图案
     */
    class Marble : public Shader
    {
    private:
        Vec3 baseColor1 = Vec3(0.95f, 0.95f, 0.95f);  // 更亮的浅色
        Vec3 baseColor2 = Vec3(0.4f, 0.4f, 0.6f);    // 更亮的深色
        Vec3 veinColor = Vec3(0.2f, 0.2f, 0.3f);      // 脉络颜色

    public:
        Marble(Material& material, vector<Texture>& textures)
            : Shader(material, textures)
        {
            // 可以在这里从材质属性读取参数
        }

        Scattered shade(const Ray& ray, const Vec3& hitPoint, const Vec3& normal) const override
        {
            Vec3 origin = hitPoint;

            // 半球采样
            Vec3 random = defaultSamplerInstance<HemiSphere>().sample3d();
            Onb onb{ normal };
            Vec3 direction = onb.local(random);

            // 大理石展示图案
            Vec3 marbleColor = generateMarbleDisplay(hitPoint, normal);

            float pdf = 1.0f / (2 * PI);
            Vec3 attenuation = marbleColor / PI;

            return {
                Ray{origin, direction},
                attenuation,
                Vec3{0},
                pdf
            };
        }

        Vec3 evaluateDirectLighting(const Ray& ray, const Vec3& hitPoint, const Vec3& normal,
            const AreaLight& light, const Vec3& lightDir, float lightDistance) const override
        {
            // 大理石展示图案
            Vec3 marbleColor = generateMarbleDisplay(hitPoint, normal);

            // Lambertian BRDF
            Vec3 brdf = marbleColor / PI;

            // 余弦项
            float cosTheta = glm::max(0.0f, glm::dot(normal, lightDir));

            // 距离衰减
            float distanceAttenuation = 1.0f / (lightDistance * lightDistance);

            return brdf * light.radiance * cosTheta * distanceAttenuation;
        }

        Vec3 getBRDF(const Vec3& wi, const Vec3& wo, const Vec3& normal) const override
        {
            // 使用展示图案的颜色
            return generateMarbleDisplay(Vec3(0), normal) / PI;
        }

    private:
        /**
         * 生成大理石展示图案 - 在平面上绘制明显的大理石纹理
         */
        Vec3 generateMarbleDisplay(const Vec3& point, const Vec3& normal) const
        {
            // 根据法向量选择投影平面
            Vec3 absNormal = glm::abs(normal);
            Vec2 uv;

            if (absNormal.x > absNormal.y && absNormal.x > absNormal.z) {
                uv = Vec2(point.y, point.z);
            }
            else if (absNormal.y > absNormal.x && absNormal.y > absNormal.z) {
                uv = Vec2(point.x, point.z);
            }
            else {
                uv = Vec2(point.x, point.y);
            }

            // 缩放和平移UV
            uv = uv * 2.0f + Vec2(0.5f, 0.5f);

            // 生成展示区域
            Vec2 center = Vec2(0.5f, 0.5f);
            float radius = 0.4f;
            float dist = glm::length(uv - center);

            if (dist < radius) {
                // 主展示区域：明显的大理石纹理
                return generateProminentMarble(uv);
            }
            else if (dist < radius + 0.1f) {
                // 边框
                return Vec3(0.0f, 0.0f, 0.0f); // 黑色边框
            }
            else {
                // 背景区域：简化的大理石纹理
                return generateSubtleMarble(uv);
            }
        }

        /**
         * 生成明显的大理石纹理（用于主展示区域）
         */
        Vec3 generateProminentMarble(const Vec2& uv) const
        {
            // 放大UV以增强细节
            Vec2 scaledUV = uv * 15.0f;

            // 生成多层大理石噪声
            float noise1 = generateMarbleNoise(scaledUV, 1.0f);
            float noise2 = generateMarbleNoise(scaledUV * 2.0f, 0.5f);
            float noise3 = generateMarbleNoise(scaledUV * 4.0f, 0.25f);

            float combinedNoise = (noise1 + noise2 + noise3) / 3.0f;

            // 强烈的大理石脉络效果
            float veinPattern = generateVeinPattern(scaledUV);
            combinedNoise = glm::mix(combinedNoise, veinPattern, 0.7f);

            // 高对比度颜色混合
            if (combinedNoise > 0.6f) {
                return baseColor1; // 亮色区域
            }
            else if (combinedNoise > 0.3f) {
                return baseColor2; // 中间色
            }
            else {
                return veinColor; // 暗色脉络
            }
        }

        /**
         * 生成简化的大理石纹理（用于背景区域）
         */
        Vec3 generateSubtleMarble(const Vec2& uv) const
        {
            Vec2 scaledUV = uv * 5.0f;
            float noise = generateMarbleNoise(scaledUV, 1.0f);

            // 低对比度混合
            return glm::mix(baseColor1 * 0.8f, baseColor2 * 0.8f, noise);
        }

        /**
         * 生成大理石噪声
         */
        float generateMarbleNoise(const Vec2& point, float scale) const
        {
            // 使用正弦函数创建大理石纹理
            float x = point.x * scale;
            float y = point.y * scale;

            // 基础噪声
            float noise = std::sin(x * 0.1f + y * 0.05f);
            noise += 0.5f * std::sin(x * 0.2f + y * 0.1f);
            noise += 0.25f * std::sin(x * 0.4f + y * 0.2f);

            // 添加随机扰动
            float random = std::sin(x * 13.0f + y * 7.0f) * 0.1f;
            noise += random;

            // 归一化到[0,1]
            return (noise + 2.0f) / 4.0f;
        }

        /**
         * 生成明显的脉络图案
         */
        float generateVeinPattern(const Vec2& point) const
        {
            // 创建明显的脉络效果
            float vein1 = std::sin(point.x * 3.0f) * 0.5f + 0.5f;
            float vein2 = std::sin(point.y * 2.0f + point.x * 1.0f) * 0.3f + 0.3f;
            float vein3 = std::sin(point.x * 5.0f + point.y * 3.0f) * 0.2f + 0.2f;

            // 合并脉络
            float combined = (vein1 + vein2 + vein3) / 3.0f;

            // 阈值处理以增强对比度
            if (combined > 0.7f) return 0.0f;
            if (combined > 0.4f) return 0.5f;
            return 1.0f;
        }
    };
}

#endif