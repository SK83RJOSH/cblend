#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cblend.hpp>

using namespace cblend;

// NOLINTBEGIN(readability-function-cognitive-complexity)
TEST_CASE("queries can be created", "[default]")
// NOLINTEND(readability-function-cognitive-complexity)
{
    constexpr Option<QueryToken> EXPECTED_NAME_TOKEN("m_test");
    constexpr Option<QueryToken> EXPECTED_INDEX_TOKEN(0U);

    const auto invalid_query = Query::Create<"">();
    REQUIRE(!invalid_query);

    const auto name_query = Query::Create<"m_test">();
    REQUIRE(name_query);
    REQUIRE(name_query->GetTokenCount() == 1U);
    REQUIRE(name_query->GetToken(0) == EXPECTED_NAME_TOKEN);
    REQUIRE(name_query->GetToken(1) == NULL_OPTION);

    const auto index_query = Query::Create<"[0]">();
    REQUIRE(index_query);
    REQUIRE(index_query->GetTokenCount() == 1U);
    REQUIRE(index_query->GetToken(0) == EXPECTED_INDEX_TOKEN);

    const auto composite_query = Query::Create<"m_test.m_test[0]">();
    REQUIRE(composite_query);
    REQUIRE(composite_query->GetTokenCount() == 3U);
    REQUIRE(composite_query->GetToken(0) == EXPECTED_NAME_TOKEN);
    REQUIRE(composite_query->GetToken(1) == EXPECTED_NAME_TOKEN);
    REQUIRE(composite_query->GetToken(2) == EXPECTED_INDEX_TOKEN);
}

struct Vertex
{
    float x;
    float y;
    float z;
    u32 pad;
};

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

    const auto mesh_block = blend->GetBlock(BLOCK_CODE_ME);
    REQUIRE(mesh_block != NULL_OPTION);

    const auto mesh_type = blend->GetBlockType(*mesh_block);
    REQUIRE(mesh_type != NULL_OPTION);

    SECTION("mesh data can be read via reflection")
    {
        const auto fields = mesh_type->GetFields();
        REQUIRE(fields.size() == 54);

        const auto totvert_field = mesh_type->GetField("totvert");
        REQUIRE(totvert_field != NULL_OPTION);

        const auto totvert = totvert_field->GetValue<int>(*mesh_block);
        REQUIRE(totvert == 8);

        const auto vdata_field = mesh_type->GetField("vdata");
        REQUIRE(vdata_field != NULL_OPTION);

        const auto vdata_data = vdata_field->GetData(*mesh_block);
        REQUIRE(vdata_data.size() == 248);

        const auto& vdata_field_type = vdata_field->GetFieldType();

        const auto vdata_totlayer_field = vdata_field_type.GetField("totlayer");
        REQUIRE(vdata_totlayer_field != NULL_OPTION);

        const auto vdata_totlayer = vdata_totlayer_field->GetValue<int>(vdata_data);
        REQUIRE(vdata_totlayer == 1);

        const auto vdata_layers_field = vdata_field_type.GetField("layers");
        REQUIRE(vdata_layers_field != NULL_OPTION);

        const auto vdata_layers_data = vdata_layers_field->GetPointerData(vdata_data);
        REQUIRE(vdata_layers_data.size() == 112);

        const auto vdata_layers_element_type = vdata_layers_field->GetFieldType().GetElementType();
        REQUIRE(vdata_layers_element_type != NULL_OPTION);

        const auto vdata_layers_type_field = vdata_layers_element_type->GetField("type");
        REQUIRE(vdata_layers_type_field != NULL_OPTION);

        const auto vdata_layers_type_data = vdata_layers_type_field->GetValue<int>(vdata_layers_data);
        REQUIRE(vdata_layers_type_data == 0);

        const auto vdata_layers_data_field = vdata_layers_element_type->GetField("data");
        REQUIRE(vdata_layers_data_field != NULL_OPTION);

        const auto vdata_layers_data_data = vdata_layers_data_field->GetPointerData(vdata_layers_data);
        REQUIRE(vdata_layers_data_data.data() != nullptr);

        // NOLINTBEGIN
        const auto* vertices = reinterpret_cast<const Vertex(*)[8]>(vdata_layers_data_data.data());
        REQUIRE(vertices != nullptr);
        REQUIRE(((*vertices)[0].x == Catch::Approx(1) && (*vertices)[0].y == Catch::Approx(1) && (*vertices)[0].z == Catch::Approx(1)));
        REQUIRE(((*vertices)[1].x == Catch::Approx(1) && (*vertices)[1].y == Catch::Approx(1) && (*vertices)[1].z == Catch::Approx(-1)));
        REQUIRE(((*vertices)[2].x == Catch::Approx(1) && (*vertices)[2].y == Catch::Approx(-1) && (*vertices)[2].z == Catch::Approx(1)));
        REQUIRE(((*vertices)[3].x == Catch::Approx(1) && (*vertices)[3].y == Catch::Approx(-1) && (*vertices)[3].z == Catch::Approx(-1)));
        REQUIRE(((*vertices)[4].x == Catch::Approx(-1) && (*vertices)[4].y == Catch::Approx(1) && (*vertices)[4].z == Catch::Approx(1)));
        REQUIRE(((*vertices)[5].x == Catch::Approx(-1) && (*vertices)[5].y == Catch::Approx(1) && (*vertices)[5].z == Catch::Approx(-1)));
        REQUIRE(((*vertices)[6].x == Catch::Approx(-1) && (*vertices)[6].y == Catch::Approx(-1) && (*vertices)[6].z == Catch::Approx(1)));
        REQUIRE(((*vertices)[7].x == Catch::Approx(-1) && (*vertices)[7].y == Catch::Approx(-1) && (*vertices)[7].z == Catch::Approx(-1)));
        // NOLINTEND
    }

    SECTION("mesh data can be read via reflection queries")
    {
        const auto totvert = mesh_type->QueryValue<int, "totvert">(*mesh_block);
        REQUIRE(totvert == 8);

        const auto totlayer = mesh_type->QueryValue<int, "vdata.totlayer">(*mesh_block);
        REQUIRE(totlayer == 1);

        const auto layer_type = mesh_type->QueryValue<int, "vdata.layers[0].type">(*mesh_block);
        REQUIRE(layer_type == 0);

        static constexpr float EXPECTED_SIZE = 1.f;
        const auto size_0 = mesh_type->QueryValue<float, "size[0]">(*mesh_block);
        REQUIRE(size_0 == EXPECTED_SIZE);
        const auto size_1 = mesh_type->QueryValue<float, "size[1]">(*mesh_block);
        REQUIRE(size_1 == EXPECTED_SIZE);
        const auto size_2 = mesh_type->QueryValue<float, "size[2]">(*mesh_block);
        REQUIRE(size_2 == EXPECTED_SIZE);

        static constexpr std::array<float, 3> EXPECTED_SIZE_ARRAY = { 1.f, 1.f, 1.f };
        const auto size = mesh_type->QueryValue<std::array<float, 3>, "size">(*mesh_block);
        REQUIRE(size == EXPECTED_SIZE_ARRAY);

        // NOLINTBEGIN
        const auto vertices = mesh_type->QueryValue<const Vertex(*)[8], "vdata.layers[0].data[0]">(*mesh_block);
        REQUIRE((vertices.has_value() && *vertices != nullptr));
        REQUIRE(((**vertices)[0].x == Catch::Approx(1) && (**vertices)[0].y == Catch::Approx(1) && (**vertices)[0].z == Catch::Approx(1)));
        REQUIRE(((**vertices)[1].x == Catch::Approx(1) && (**vertices)[1].y == Catch::Approx(1) && (**vertices)[1].z == Catch::Approx(-1)));
        REQUIRE(((**vertices)[2].x == Catch::Approx(1) && (**vertices)[2].y == Catch::Approx(-1) && (**vertices)[2].z == Catch::Approx(1)));
        REQUIRE(((**vertices)[3].x == Catch::Approx(1) && (**vertices)[3].y == Catch::Approx(-1) && (**vertices)[3].z == Catch::Approx(-1)));
        REQUIRE(((**vertices)[4].x == Catch::Approx(-1) && (**vertices)[4].y == Catch::Approx(1) && (**vertices)[4].z == Catch::Approx(1)));
        REQUIRE(((**vertices)[5].x == Catch::Approx(-1) && (**vertices)[5].y == Catch::Approx(1) && (**vertices)[5].z == Catch::Approx(-1)));
        REQUIRE(((**vertices)[6].x == Catch::Approx(-1) && (**vertices)[6].y == Catch::Approx(-1) && (**vertices)[6].z == Catch::Approx(1)));
        REQUIRE(((**vertices)[7].x == Catch::Approx(-1) && (**vertices)[7].y == Catch::Approx(-1) && (**vertices)[7].z == Catch::Approx(-1)));
        // NOLINTEND
    }
}
