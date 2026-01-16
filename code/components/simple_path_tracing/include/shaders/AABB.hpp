#pragma once
#ifndef __AABB_HPP__
#define __AABB_HPP__

#include "geometry/vec.hpp"
#include "Ray.hpp"

namespace SimplePathTracer
{
    using namespace NRenderer;

    /**
     * 轴对齐包围盒
     */
    struct AABB {
        Vec3 min;
        Vec3 max;

        AABB() : min(FLT_MAX), max(-FLT_MAX) {}
        AABB(const Vec3& min, const Vec3& max) : min(min), max(max) {}

        /**
         * 扩展包围盒以包含点
         */
        void expand(const Vec3& point) {
            min = glm::min(min, point);
            max = glm::max(max, point);
        }

        /**
         * 扩展包围盒以包含另一个AABB
         */
        void expand(const AABB& other) {
            min = glm::min(min, other.min);
            max = glm::max(max, other.max);
        }

        /**
         * 计算包围盒中心
         */
        Vec3 center() const {
            return (min + max) * 0.5f;
        }

        /**
         * 光线与AABB求交
         */
        bool intersect(const Ray& ray, float tMin, float tMax) const {
            for (int i = 0; i < 3; i++) {
                float invD = 1.0f / ray.direction[i];
                float t0 = (min[i] - ray.origin[i]) * invD;
                float t1 = (max[i] - ray.origin[i]) * invD;

                if (invD < 0.0f) std::swap(t0, t1);

                tMin = t0 > tMin ? t0 : tMin;
                tMax = t1 < tMax ? t1 : tMax;

                if (tMax <= tMin) return false;
            }
            return true;
        }

        /**
         * 计算表面积
         */
        float surfaceArea() const {
            Vec3 d = max - min;
            return 2.0f * (d.x * d.y + d.x * d.z + d.y * d.z);
        }
    };
}

#endif