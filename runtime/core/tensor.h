#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace us4::core
{

    enum class TensorDataType
    {
        kFloat32,
        kInt32,
    };

    class Tensor
    {
      public:
        Tensor() = default;

        explicit Tensor(std::vector<std::size_t> shape,
                        TensorDataType dataType = TensorDataType::kFloat32)
            : shape_(std::move(shape)), dataType_(dataType), storage_(ComputeElementCount(shape_))
        {
        }

        [[nodiscard]] const std::vector<std::size_t>& Shape() const noexcept
        {
            return shape_;
        }

        [[nodiscard]] std::size_t Rank() const noexcept
        {
            return shape_.size();
        }

        [[nodiscard]] std::size_t Dim(std::size_t axis) const
        {
            if (axis >= shape_.size())
            {
                throw std::out_of_range("Tensor axis out of range");
            }
            return shape_[axis];
        }

        [[nodiscard]] std::size_t ElementCount() const noexcept
        {
            return storage_.size();
        }

        [[nodiscard]] TensorDataType DataType() const noexcept
        {
            return dataType_;
        }

        [[nodiscard]] bool Empty() const noexcept
        {
            return storage_.empty();
        }

        [[nodiscard]] float* Data() noexcept
        {
            return storage_.data();
        }

        [[nodiscard]] const float* Data() const noexcept
        {
            return storage_.data();
        }

        void Resize(std::vector<std::size_t> shape)
        {
            shape_ = std::move(shape);
            storage_.assign(ComputeElementCount(shape_), 0.0F);
        }

        void Fill(float value)
        {
            std::fill(storage_.begin(), storage_.end(), value);
        }

        float& operator[](std::size_t index)
        {
            return storage_.at(index);
        }

        const float& operator[](std::size_t index) const
        {
            return storage_.at(index);
        }

        [[nodiscard]] std::size_t Offset(const std::vector<std::size_t>& indices) const
        {
            if (indices.size() != shape_.size())
            {
                throw std::invalid_argument("Tensor index rank mismatch");
            }

            std::size_t offset = 0;
            std::size_t stride = 1;
            for (std::size_t i = shape_.size(); i-- > 0;)
            {
                if (indices[i] >= shape_[i])
                {
                    throw std::out_of_range("Tensor index out of range");
                }
                offset += indices[i] * stride;
                stride *= shape_[i];
            }
            return offset;
        }

        float& At(const std::vector<std::size_t>& indices)
        {
            return storage_.at(Offset(indices));
        }

        const float& At(const std::vector<std::size_t>& indices) const
        {
            return storage_.at(Offset(indices));
        }

        [[nodiscard]] std::string DebugShape() const
        {
            std::string text = "[";
            for (std::size_t i = 0; i < shape_.size(); ++i)
            {
                if (i > 0)
                {
                    text += ", ";
                }
                text += std::to_string(shape_[i]);
            }
            text += "]";
            return text;
        }

        [[nodiscard]] static std::size_t ComputeElementCount(const std::vector<std::size_t>& shape)
        {
            if (shape.empty())
            {
                return 0;
            }

            return std::accumulate(shape.begin(), shape.end(), static_cast<std::size_t>(1),
                                   [](std::size_t left, std::size_t right)
                                   { return left * right; });
        }

      private:
        std::vector<std::size_t> shape_;
        TensorDataType dataType_ = TensorDataType::kFloat32;
        std::vector<float> storage_;
    };

} // namespace us4::core
