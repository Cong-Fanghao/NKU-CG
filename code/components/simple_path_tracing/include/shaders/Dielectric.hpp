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
     * 电介质材质（透明/折射）
     * 实现玻璃、水、钻石等透明材质效果
     */
    class Dielectric : public Shader
    {
    private:
        float refractiveIndex;  // 折射率（例如玻璃约为1.5）
        Vec3 attenuation;       // 颜色衰减（用于有色玻璃）

    public:
        /**
         * 构造函数
         * @param material 材质数据
         * @param textures 纹理缓冲区
         */
        Dielectric(Material& material, vector<Texture>& textures)
            : Shader(material, textures)
        {
            // 从材质属性中获取折射率
            auto refIndexProp = material.getProperty<Property::Wrapper::FloatType>("refractiveIndex");
            if (refIndexProp) {
                refractiveIndex = (*refIndexProp).value;
            }
            else {
                refractiveIndex = 1.5f; // 默认玻璃折射率
            }

            // 获取颜色衰减（用于有色玻璃）
            auto colorProp = material.getProperty<Property::Wrapper::RGBType>("attenuation");
            if (colorProp) {
                attenuation = (*colorProp).value;
            }
            else {
                attenuation = Vec3(1.0f, 1.0f, 1.0f); // 默认无色
            }
        }

        /**
         * 电介质材质着色计算
         * @param ray 入射光线
         * @param hitPoint 交点位置
         * @param normal 法线
         * @return 散射信息
         */
        Scattered shade(const Ray& ray, const Vec3& hitPoint, const Vec3& normal) const override
        {
            Vec3 outwardNormal;  // 指向外的法线
            Vec3 reflected = reflect(ray.direction, normal);
            float niOverNt;      // 入射折射率/折射折射率
            Vec3 refracted;

            // 菲涅尔反射率
            float reflectProb;
            float cosine;

            // 判断光线是从外部进入还是内部射出
            if (glm::dot(ray.direction, normal) > 0) {
                // 光线从内部射出（从介质到空气）
                outwardNormal = -normal;
                niOverNt = refractiveIndex; // 从介质到空气，折射率为 refractiveIndex / 1.0
                cosine = refractiveIndex * glm::dot(ray.direction, normal) / glm::length(ray.direction);
            }
            else {
                // 光线从外部进入（从空气到介质）
                outwardNormal = normal;
                niOverNt = 1.0f / refractiveIndex; // 从空气到介质，折射率为 1.0 / refractiveIndex
                cosine = -glm::dot(ray.direction, normal) / glm::length(ray.direction);
            }

            // 计算折射
            if (refract(ray.direction, outwardNormal, niOverNt, refracted)) {
                // 如果发生折射，计算菲涅尔反射率
                reflectProb = schlick(cosine, refractiveIndex);
            }
            else {
                // 全内反射，只反射
                reflectProb = 1.0f;
            }

            // 根据反射概率选择反射或折射
            Ray scatteredRay;
            if (defaultSamplerInstance<UniformSampler>().sample1d() < reflectProb) {
                scatteredRay = Ray{ hitPoint, reflected };
            }
            else {
                scatteredRay = Ray{ hitPoint, refracted };
            }

            // 电介质不吸收光，但可能有颜色衰减（有色玻璃）
            Vec3 finalAttenuation = attenuation;

            // 注意：电介质材质通常不发光，但可以设置发光属性（如霓虹灯）这里设为0
            Vec3 emitted(0, 0, 0);

            // PDF计算：这里反射和折射各占一定比例，但我们的散射光线只选择一条，所以PDF为1
            float pdf = 1.0f;

            return Scattered{ scatteredRay, finalAttenuation, emitted, pdf };
        }

    private:
        /**
         * 计算反射方向
         * @param v 入射方向
         * @param n 法线方向
         * @return 反射方向
         */
        Vec3 reflect(const Vec3& v, const Vec3& n) const
        {
            return v - 2 * glm::dot(v, n) * n;
        }

        /**
         * 计算折射方向（使用斯涅尔定律）
         * @param v 入射方向
         * @param n 法线方向
         * @param niOverNt 入射折射率/折射折射率
         * @param refracted 存储折射方向
         * @return 是否发生折射（全内反射时返回false）
         */
        bool refract(const Vec3& v, const Vec3& n, float niOverNt, Vec3& refracted) const
        {
            Vec3 uv = glm::normalize(v);
            float dt = glm::dot(uv, n);
            float discriminant = 1.0f - niOverNt * niOverNt * (1.0f - dt * dt);
            if (discriminant > 0) {
                refracted = niOverNt * (uv - n * dt) - n * std::sqrt(discriminant);
                return true;
            }
            else {
                return false; // 全内反射
            }
        }

        /**
         * 计算菲涅尔反射率（Schlick近似）
         * @param cosine 入射角余弦
         * @param refIdx 折射率
         * @return 反射率
         */
        float schlick(float cosine, float refIdx) const
        {
            float r0 = (1 - refIdx) / (1 + refIdx);
            r0 = r0 * r0;
            return r0 + (1 - r0) * std::pow((1 - cosine), 5);
        }
    };
}

#endif