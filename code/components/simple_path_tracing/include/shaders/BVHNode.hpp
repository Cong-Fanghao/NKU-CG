#pragma once
#ifndef __BVH_NODE_HPP__
#define __BVH_NODE_HPP__

#include "geometry/vec.hpp"
#include "Ray.hpp"
#include "intersections/HitRecord.hpp"
#include "intersections/intersections.hpp"
#include "scene/Scene.hpp"

#include <memory>
#include <vector>
#include "AABB.hpp"

namespace SimplePathTracer
{
    using namespace NRenderer;

    /**
     * BVH节点基类
     */
    struct BVHNode {
        AABB bbox;  // 包围盒
        virtual ~BVHNode() = default;

        /**
         * 射线相交检测
         */
        virtual HitRecord intersect(const Ray& ray, float tMin, float tMax) const = 0;
    };

    /**
     * 叶子节点 - 包含实际图元的集合
     */
    struct BVHLeaf : public BVHNode {
        std::vector<Triangle> triangles;
        std::vector<Sphere> spheres;
        std::vector<Plane> planes;

        // 修复：添加平面参数的构造函数
        BVHLeaf(const std::vector<Triangle>& tris, const std::vector<Sphere>& sphs, const std::vector<Plane>& pls)
            : triangles(tris), spheres(sphs), planes(pls)
        {
            // 计算包围盒
            bbox = calculateBBox();
        }

        HitRecord intersect(const Ray& ray, float tMin, float tMax) const override
        {
            HitRecord closestHit = getMissRecord();
            float closest = tMax;

            // 三角形相交检测
            for (const auto& tri : triangles) {
                auto hitRecord = Intersection::xTriangle(ray, tri, tMin, closest);
                if (hitRecord && hitRecord->t < closest) {
                    closest = hitRecord->t;
                    closestHit = hitRecord;
                }
            }

            // 球体相交检测
            for (const auto& sph : spheres) {
                auto hitRecord = Intersection::xSphere(ray, sph, tMin, closest);
                if (hitRecord && hitRecord->t < closest) {
                    closest = hitRecord->t;
                    closestHit = hitRecord;
                }
            }

            // 平面相交检测
            for (const auto& pl : planes) {
                auto hitRec = Intersection::xPlane(ray, pl, tMin, closest);
                if (hitRec && hitRec->t < closest) {
                    closest = hitRec->t;
                    closestHit = hitRec;
                }
            }

            return closestHit;
        }

    private:
        AABB calculateBBox() const
        {
            AABB bbox;

            for (const auto& tri : triangles) {
                bbox.expand(tri.v1);
                bbox.expand(tri.v2);
                bbox.expand(tri.v3);
            }

            for (const auto& sph : spheres) {
                bbox.expand(sph.position - Vec3(sph.radius));
                bbox.expand(sph.position + Vec3(sph.radius));
            }

            for (const auto& pl : planes) {
                AABB planeBBox;
                const float largeValue = 1000.0f;
                Vec3 center = pl.position;
                Vec3 normal = glm::normalize(pl.normal);

                for (int i = 0; i < 3; i++) {
                    if (std::abs(normal[i]) > 0.9f) {
                        planeBBox.min[i] = center[i] - 0.1f;
                        planeBBox.max[i] = center[i] + 0.1f;
                    }
                    else {
                        planeBBox.min[i] = center[i] - largeValue;
                        planeBBox.max[i] = center[i] + largeValue;
                    }
                }
                bbox.expand(planeBBox);
            }

            return bbox;
        }
    };

    /**
     * 内部节点 - 包含子节点
     */
    struct BVHInternal : public BVHNode {
        std::shared_ptr<BVHNode> left;
        std::shared_ptr<BVHNode> right;

        BVHInternal(std::shared_ptr<BVHNode> l, std::shared_ptr<BVHNode> r)
            : left(l), right(r)
        {
            // 合并子节点的包围盒
            if (left && right) {
                bbox.min = glm::min(left->bbox.min, right->bbox.min);
                bbox.max = glm::max(left->bbox.max, right->bbox.max);
            }
            else if (left) {
                bbox = left->bbox;
            }
            else if (right) {
                bbox = right->bbox;
            }
        }

        HitRecord intersect(const Ray& ray, float tMin, float tMax) const override {
            // 首先检查与包围盒的交点
            if (!bbox.intersect(ray, tMin, tMax)) {
                return getMissRecord();
            }

            HitRecord leftHit = getMissRecord();
            HitRecord rightHit = getMissRecord();

            // 修复：检查子节点是否存在
            if (left) {
                leftHit = left->intersect(ray, tMin, tMax);
            }
            if (right) {
                rightHit = right->intersect(ray, tMin, tMax);
            }

            // 选择最近的交点
            if (leftHit && rightHit) {
                return leftHit->t < rightHit->t ? leftHit : rightHit;
            }
            else if (leftHit) {
                return leftHit;
            }
            else if (rightHit) {
                return rightHit;
            }
            return getMissRecord();
        }
    };
}

#endif