#pragma once
#ifndef __HALTON_SAMPLER_HPP__
#define __HALTON_SAMPLER_HPP__

#include "Sampler.hpp"
#include "geometry/vec.hpp"
#include <vector>
#include <cmath>

namespace SimplePathTracer
{
    /**
     * Halton低差异序列生成器（不继承Sampler）
     * 通过组合方式提供低差异序列功能
     */
    class HaltonSequenceGenerator
    {
    private:
        mutable int currentIndex = 0;

        // 前20个质数，用于高维采样
        static constexpr int PRIMES[20] = {
            2, 3, 5, 7, 11, 13, 17, 19, 23, 29,
            31, 37, 41, 43, 47, 53, 59, 61, 67, 71
        };

    public:
        HaltonSequenceGenerator() = default;

        /**
         * 重置序列状态
         */
        void reset()
        {
            currentIndex = 0;
        }

        /**
         * 生成1D Halton序列
         */
        float generate1d()
        {
            float result = haltonSequence(PRIMES[0], currentIndex);
            currentIndex++;
            return result;
        }

        /**
         * 生成2D Halton序列
         */
        Vec2 generate2d()
        {
            Vec2 result = Vec2(
                haltonSequence(PRIMES[0], currentIndex),  // 基数2
                haltonSequence(PRIMES[1], currentIndex)    // 基数3
            );
            currentIndex++;
            return result;
        }

        /**
         * 生成3D Halton序列
         */
        Vec3 generate3d()
        {
            Vec3 result = Vec3(
                haltonSequence(PRIMES[0], currentIndex),  // 基数2
                haltonSequence(PRIMES[1], currentIndex),  // 基数3
                haltonSequence(PRIMES[2], currentIndex)   // 基数5
            );
            currentIndex++;
            return result;
        }

    private:
        /**
         * 计算Halton序列
         */
        float haltonSequence(int base, int index) const
        {
            float result = 0.0f;
            float f = 1.0f;
            int i = index;

            while (i > 0) {
                f /= base;
                result += f * (i % base);
                i = i / base;
            }

            return result;
        }
    };
}
#endif