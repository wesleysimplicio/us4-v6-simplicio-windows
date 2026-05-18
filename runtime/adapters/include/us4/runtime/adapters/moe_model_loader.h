#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace us4::runtime::adapters
{

    struct MappedExpertShard
    {
        std::size_t expertId = 0;
        std::string resolvedPath;
        std::size_t mappedBytes = 0;
    };

    class MoeModelLoader
    {
      public:
        MoeModelLoader();
        ~MoeModelLoader();

        MoeModelLoader(MoeModelLoader&&) noexcept;
        MoeModelLoader& operator=(MoeModelLoader&&) noexcept;

        MoeModelLoader(const MoeModelLoader&) = delete;
        MoeModelLoader& operator=(const MoeModelLoader&) = delete;

        [[nodiscard]] bool Open(std::string_view modelPath);
        [[nodiscard]] std::optional<MappedExpertShard> LoadExpert(std::size_t expertId);
        [[nodiscard]] bool UnloadExpert(std::size_t expertId);
        void Close();

        [[nodiscard]] bool IsMapped(std::size_t expertId) const;
        [[nodiscard]] std::size_t KnownExpertCount() const;
        [[nodiscard]] std::size_t MappedExpertCount() const;
        [[nodiscard]] std::vector<std::size_t> KnownExpertIds() const;
        [[nodiscard]] std::vector<std::size_t> MappedExpertIds() const;

      private:
        struct State;
        std::unique_ptr<State> state_;
    };

} // namespace us4::runtime::adapters
