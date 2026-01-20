#pragma once
#ifndef __SHADER_HPP__
#define __SHADER_HPP__

#include "geometry/vec.hpp"
#include "common/macros.hpp"
#include "scene/Scene.hpp"
#include "Scattered.hpp"

namespace SimplePathTracer
{
    using namespace NRenderer;
    using namespace std;

    constexpr float PI = 3.1415926535898f;

    /**
     * 着色器基类
     * 添加直接光照计算接口
     */
    class Shader
    {
    protected:
        Material& material;
        vector<Texture>& textureBuffer;

    public:
        Shader(Material& material, vector<Texture>& textures)
            : material(material)
            , textureBuffer(textures)
        {
        }

        /**
         * 间接光照计算
         */
        virtual Scattered shade(const Ray& ray, const Vec3& hitPoint, const Vec3& normal) const = 0;

        /**
         * 直接光照计算 - 新增接口
         * @param ray 入射光线
         * @param hitPoint 交点位置
         * @param normal 法线
         * @param light 光源信息
         * @param lightDir 光源方向
         * @param lightDistance 光源距离
         * @return 直接光照贡献
         */
        virtual Vec3 evaluateDirectLighting(const Ray& ray, const Vec3& hitPoint, const Vec3& normal,
            const AreaLight& light, const Vec3& lightDir, float lightDistance) const = 0;

        /**
         * 获取材质的BRDF值 - 辅助函数
         */
        virtual Vec3 getBRDF(const Vec3& wi, const Vec3& wo, const Vec3& normal) const = 0;
    };
    SHARE(Shader);
}

#endif