#pragma once
#ifndef __BVH_BUILDER_HPP__
#define __BVH_BUILDER_HPP__

#include "BVHNode.hpp"
#include <algorithm>

namespace SimplePathTracer
{
    /**
     * BVH构建器
     */
    class BVHBuilder {
    private:
        /**
         * 构建信息结构体
         */
        struct BuildPrimitive {
            AABB bbox;
            Triangle* triangle = nullptr;
            Sphere* sphere = nullptr;
            Plane* plane = nullptr;
            Vec3 center;
        };

    public:
        /**
         * 构建BVH
         */
        std::shared_ptr<BVHNode> build(Scene& scene) {
            std::vector<BuildPrimitive> primitives;

            // 收集三角形图元
            for (auto& tri : scene.triangleBuffer) {
                BuildPrimitive prim;
                prim.triangle = &tri;
                prim.bbox = calculateTriangleBBox(tri);
                prim.center = prim.bbox.center();
                primitives.push_back(prim);
            }

            // 收集球体图元
            for (auto& sph : scene.sphereBuffer) {
                BuildPrimitive prim;
                prim.sphere = &sph;
                prim.bbox = calculateSphereBBox(sph);
                prim.center = prim.bbox.center();
                primitives.push_back(prim);
            }

            // 收集平面图元
            for (auto& pl : scene.planeBuffer) {
                BuildPrimitive prim;
                prim.plane = &pl;
                prim.bbox = calculatePlaneBBox(pl);
                prim.center = prim.bbox.center();
                primitives.push_back(prim);
            }

            return recursiveBuild(primitives, 0, primitives.size());
        }

    private:
        /**
         * 递归构建BVH
         */
        std::shared_ptr<BVHNode> recursiveBuild(std::vector<BuildPrimitive>& primitives,
            size_t start, size_t end) {
            if (end - start == 0) return nullptr;

            // 当图元数量较少时，创建叶子节点
            if (end - start <= 4) {
                std::vector<Triangle> triangles;
                std::vector<Sphere> spheres;
                std::vector<Plane> planes;

                for (size_t i = start; i < end; i++) {
                    if (primitives[i].triangle) {
                        triangles.push_back(*primitives[i].triangle);
                    }
                    else if (primitives[i].sphere) {
                        spheres.push_back(*primitives[i].sphere);
                    }
                    else if (primitives[i].plane) {
                        planes.push_back(*primitives[i].plane);
                    }
                }
                return std::make_shared<BVHLeaf>(triangles, spheres, planes);
            }

            // 计算图元的质心包围盒
            AABB centroidBounds;
            for (size_t i = start; i < end; i++) {
                centroidBounds.expand(primitives[i].center);
            }

            // 选择最长的轴进行分割
            int axis = centroidBounds.max.x - centroidBounds.min.x >
                centroidBounds.max.y - centroidBounds.min.y ? 0 : 1;
            axis = centroidBounds.max[axis] - centroidBounds.min[axis] >
                centroidBounds.max.z - centroidBounds.min.z ? axis : 2;

            // 按质心坐标排序
            std::sort(primitives.begin() + start, primitives.begin() + end,
                [axis](const BuildPrimitive& a, const BuildPrimitive& b) {
                    return a.center[axis] < b.center[axis];
                });

            // 中间分割
            size_t mid = start + (end - start) / 2;
            auto left = recursiveBuild(primitives, start, mid);
            auto right = recursiveBuild(primitives, mid, end);

            return std::make_shared<BVHInternal>(left, right);
        }

        /**
         * 计算三角形的包围盒
         */
        AABB calculateTriangleBBox(const Triangle& tri) {
            AABB bbox;
            bbox.expand(tri.v1);
            bbox.expand(tri.v2);
            bbox.expand(tri.v3);
            return bbox;
        }

        /**
         * 计算球体的包围盒
         */
        AABB calculateSphereBBox(const Sphere& sph) {
            AABB bbox;
            bbox.min = sph.position - Vec3(sph.radius);
            bbox.max = sph.position + Vec3(sph.radius);
            return bbox;
        }

        /**
         * 计算平面的包围盒（新增）
         */
        AABB calculatePlaneBBox(const Plane& pl) {
            AABB bbox;
            // 平面是无限延伸的，但我们为其创建一个合理的有限包围盒
            // 使用一个较大的范围来包含可见部分
            const float largeValue = 1000.0f;
            Vec3 center = pl.position;

            // 根据法线方向确定包围盒方向
            Vec3 normal = glm::normalize(pl.normal);

            // 创建与法线对齐的包围盒
            for (int i = 0; i < 3; i++) {
                if (std::abs(normal[i]) > 0.9f) {
                    // 法线主要朝向这个轴，在这个轴上使用较小的范围
                    bbox.min[i] = center[i] - 0.1f;
                    bbox.max[i] = center[i] + 0.1f;
                }
                else {
                    // 其他轴使用较大的范围
                    bbox.min[i] = center[i] - largeValue;
                    bbox.max[i] = center[i] + largeValue;
                }
            }
            return bbox;
        }
    };
}

#endif