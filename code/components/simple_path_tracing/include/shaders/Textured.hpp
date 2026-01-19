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
    };
}

#endif