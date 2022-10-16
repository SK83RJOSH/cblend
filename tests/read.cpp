#include <catch2/catch_test_macros.hpp>
#include <cblend.hpp>

#include <filesystem>

using namespace cblend;

// NOLINTBEGIN(readability-function-cognitive-complexity)
TEST_CASE("default blend file can be read", "[default]")
// NOLINTEND(readability-function-cognitive-complexity)
{
    auto blend = Blend::Open("default.blend");
    REQUIRE(blend);

    SECTION("file is x64 little endian")
    {
        static constexpr Endian EXPECTED_ENDIAN = Endian::Little;
        REQUIRE(blend->GetEndian() == EXPECTED_ENDIAN);

        static constexpr Pointer EXPECTED_POINTER = Pointer::U64;
        REQUIRE(blend->GetPointer() == EXPECTED_POINTER);

        static constexpr usize EXPECTED_BLOCK_COUNT = 1945;
        REQUIRE(blend->GetBlockCount() == EXPECTED_BLOCK_COUNT);
    }

    SECTION("blocks can be queried successfully")
    {
        const auto endb_block = blend->GetBlock(BLOCK_CODE_ENDB);
        REQUIRE(endb_block != NULL_OPTION);

        const auto dna1_block_count = blend->GetBlockCount(BLOCK_CODE_DNA1);
        REQUIRE(dna1_block_count == 1);

        for (const auto& dna1_block : blend->GetBlocks(BLOCK_CODE_DNA1))
        {
            REQUIRE(dna1_block.header.code == BLOCK_CODE_DNA1);
        }
    }

    SECTION("mesh data can be read successfully")
    {
        const auto mesh_block = blend->GetBlock(BLOCK_CODE_ME);
        REQUIRE(mesh_block != NULL_OPTION);

        const auto mesh_type = blend->GetBlockType(*mesh_block);
        REQUIRE(mesh_type != NULL_OPTION);
    }
}
