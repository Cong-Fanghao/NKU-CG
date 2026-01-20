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
     * 在平面上生成明显的材质展示图案
     */
    class TexturedLambertian : public Shader
    {
    private:
        Vec3 baseColor;
        int textureId;
        bool hasTexture;

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
                baseColor = Vec3(0.8f, 0.8f, 0.8f); // 更亮的默认颜色
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

            // 半球采样
            Vec3 random = defaultSamplerInstance<HemiSphere>().sample3d();
            Onb onb{ normal };
            Vec3 direction = onb.local(random);

            // 获取最终颜色（包含展示图案）
            Vec3 finalColor = generateDisplayPattern(hitPoint, normal);

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
            const AreaLight& light, const Vec3& lightDir, float lightDistance) const override
        {
            // 获取最终颜色（包含展示图案）
            Vec3 finalColor = generateDisplayPattern(hitPoint, normal);

            // Lambertian BRDF
            Vec3 brdf = finalColor / M_PI;

            // 余弦项
            float cosTheta = glm::max(0.0f, glm::dot(normal, lightDir));

            // 距离衰减
            float attenuation = 1.0f / (lightDistance * lightDistance);

            return brdf * light.radiance * cosTheta * attenuation;
        }

        Vec3 getBRDF(const Vec3& wi, const Vec3& wo, const Vec3& normal) const override
        {
            // 使用展示图案的颜色
            return generateDisplayPattern(Vec3(0), normal) / M_PI;
        }

    private:
        /**
         * 生成材质展示图案 - 在平面上绘制明显的图案来展示材质特性
         */
        Vec3 generateDisplayPattern(const Vec3& point, const Vec3& normal) const
        {
            // 根据法向量选择投影平面
            Vec3 absNormal = glm::abs(normal);
            Vec2 uv;

            if (absNormal.x > absNormal.y && absNormal.x > absNormal.z) {
                uv = Vec2(point.y, point.z); // YZ平面
            }
            else if (absNormal.y > absNormal.x && absNormal.y > absNormal.z) {
                uv = Vec2(point.x, point.z); // XZ平面
            }
            else {
                uv = Vec2(point.x, point.y); // XY平面
            }

            // 缩放UV坐标
            uv *= 0.5f;

            // 生成中心圆形区域展示材质
            Vec2 center = Vec2(0.5f, 0.5f);
            float radius = 0.4f;
            float dist = glm::length(uv - center);

            if (dist < radius) {
                // 圆形区域内：展示纹理材质特性
                return generateTexturePattern(uv);
            }
            else {
                // 圆形区域外：边框和标签
                return generateBorderPattern(uv, dist, radius);
            }
        }

        /**
         * 生成纹理材质图案
         */
        Vec3 generateTexturePattern(const Vec2& uv) const
        {
            // 在圆形区域内生成明显的纹理图案
            Vec2 scaledUV = uv * 10.0f; // 放大UV

            // 生成网格纹理
            float gridSize = 1.0f;
            float gridX = glm::fract(scaledUV.x / gridSize);
            float gridY = glm::fract(scaledUV.y / gridSize);

            // 棋盘格效果
            bool isChecker = (int(std::floor(scaledUV.x)) + int(std::floor(scaledUV.y))) % 2 == 0;

            if (isChecker) {
                // 亮色格子
                return baseColor;
            }
            else {
                // 暗色格子 - 展示材质对比
                return baseColor * 0.6f;
            }
        }

        /**
         * 生成边框和标签图案
         */
        Vec3 generateBorderPattern(const Vec2& uv, float dist, float radius) const
        {
            // 边框效果
            float borderWidth = 0.05f;
            if (dist < radius + borderWidth) {
                // 边框
                return Vec3(0.1f, 0.1f, 0.1f); // 黑色边框
            }

            // 背景区域
            Vec2 labelUV = (uv - Vec2(0.5f, 0.5f)) * 2.0f;

            // 在四个角落添加文字标识
            if (std::abs(labelUV.x) > 0.7f && std::abs(labelUV.y) > 0.7f) {
                // 文字区域 - 使用对比色
                return Vec3(0.9f, 0.9f, 0.9f); // 白色文字背景
            }

            // 默认背景
            return Vec3(0.3f, 0.3f, 0.3f); // 灰色背景
        }
    };
}

#endif