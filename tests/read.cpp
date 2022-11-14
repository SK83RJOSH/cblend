#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cblend.hpp>

#include <filesystem>

using namespace cblend;

// NOLINTBEGIN
TEST_CASE("queries can be created", "[default]")
// NOLINTEND
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

// NOLINTBEGIN
TEST_CASE("default blend file can be opened via buffer")
// NOLINTEND
{
    std::ifstream file(std::filesystem::path("default.blend").c_str(), std::ios::binary);
    file.unsetf(std::ios::skipws);
    file.seekg(0, std::ios::end);

    const auto file_size = static_cast<usize>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<u8> buffer;
    buffer.reserve(file_size);
    buffer.insert(buffer.begin(), std::istream_iterator<u8>(file), std::istream_iterator<u8>());

    const auto blend = Blend::Read(buffer);
    REQUIRE(blend);
}

struct Vertex
{
    float x;
    float y;
    float z;
    u32 pad;
};

// NOLINTBEGIN
TEST_CASE("default blend file can be read", "[default]")
// NOLINTEND
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

        const auto layer_collection_type = blend->GetType("LayerCollection");
        REQUIRE(layer_collection_type != NULL_OPTION);

        const auto collection_child_type = blend->GetType("CollectionChild");
        REQUIRE(collection_child_type != NULL_OPTION);

        const auto collection_object_type = blend->GetType("CollectionObject");
        REQUIRE(collection_object_type != NULL_OPTION);

        for (const auto& layer_collection_block : blend->GetBlocks(*layer_collection_type))
        {
            const auto flag = layer_collection_type->QueryValue<u16, "flag">(layer_collection_block);
            REQUIRE((flag.has_value() && (*flag & (1U << 4U)) == 0U));

            const auto children_data = layer_collection_type->QueryValue<MemorySpan, "collection[0].children">(layer_collection_block);
            REQUIRE(children_data);

            REQUIRE(collection_child_type->QueryEachValue<MemorySpan, "collection[0].gobject">(
                *children_data,
                [&collection_object_type](MemorySpan gobject_data)
                {
                    REQUIRE(collection_object_type->QueryEachValue<"ob[0]">(
                        gobject_data,
                        [&](const BlendType& object_type, MemorySpan object_data)
                        {
                            const auto type = object_type.QueryValue<u16, "type">(object_data);
                            REQUIRE(type.has_value());
                        }
                    ));
                }
            ));
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
        const auto* vertices_data = reinterpret_cast<const Vertex(*)[8]>(vdata_layers_data_data.data());
        REQUIRE(vertices_data != nullptr);
        const auto& vertices = *vertices_data;
        REQUIRE((vertices[0].x == Catch::Approx(1) && vertices[0].y == Catch::Approx(1) && vertices[0].z == Catch::Approx(1)));
        REQUIRE((vertices[1].x == Catch::Approx(1) && vertices[1].y == Catch::Approx(1) && vertices[1].z == Catch::Approx(-1)));
        REQUIRE((vertices[2].x == Catch::Approx(1) && vertices[2].y == Catch::Approx(-1) && vertices[2].z == Catch::Approx(1)));
        REQUIRE((vertices[3].x == Catch::Approx(1) && vertices[3].y == Catch::Approx(-1) && vertices[3].z == Catch::Approx(-1)));
        REQUIRE((vertices[4].x == Catch::Approx(-1) && vertices[4].y == Catch::Approx(1) && vertices[4].z == Catch::Approx(1)));
        REQUIRE((vertices[5].x == Catch::Approx(-1) && vertices[5].y == Catch::Approx(1) && vertices[5].z == Catch::Approx(-1)));
        REQUIRE((vertices[6].x == Catch::Approx(-1) && vertices[6].y == Catch::Approx(-1) && vertices[6].z == Catch::Approx(1)));
        REQUIRE((vertices[7].x == Catch::Approx(-1) && vertices[7].y == Catch::Approx(-1) && vertices[7].z == Catch::Approx(-1)));
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

        static constexpr float EXPECTED_SIZE = 1.F;
        const auto size_0 = mesh_type->QueryValue<float, "size[0]">(*mesh_block);
        REQUIRE(size_0 == EXPECTED_SIZE);
        const auto size_1 = mesh_type->QueryValue<float, "size[1]">(*mesh_block);
        REQUIRE(size_1 == EXPECTED_SIZE);
        const auto size_2 = mesh_type->QueryValue<float, "size[2]">(*mesh_block);
        REQUIRE(size_2 == EXPECTED_SIZE);

        static constexpr std::array<float, 3> EXPECTED_SIZE_ARRAY = { 1.F, 1.F, 1.F };
        const auto size = mesh_type->QueryValue<std::array<float, 3>, "size">(*mesh_block);
        REQUIRE(size == EXPECTED_SIZE_ARRAY);

        // NOLINTBEGIN
        const auto vertices_data = mesh_type->QueryValue<const Vertex(*)[8], "vdata.layers[0].data[0]">(*mesh_block);
        REQUIRE((vertices_data.has_value() && *vertices_data != nullptr));
        const auto vertices = **vertices_data;
        REQUIRE((vertices[0].x == Catch::Approx(1) && vertices[0].y == Catch::Approx(1) && vertices[0].z == Catch::Approx(1)));
        REQUIRE((vertices[1].x == Catch::Approx(1) && vertices[1].y == Catch::Approx(1) && vertices[1].z == Catch::Approx(-1)));
        REQUIRE((vertices[2].x == Catch::Approx(1) && vertices[2].y == Catch::Approx(-1) && vertices[2].z == Catch::Approx(1)));
        REQUIRE((vertices[3].x == Catch::Approx(1) && vertices[3].y == Catch::Approx(-1) && vertices[3].z == Catch::Approx(-1)));
        REQUIRE((vertices[4].x == Catch::Approx(-1) && vertices[4].y == Catch::Approx(1) && vertices[4].z == Catch::Approx(1)));
        REQUIRE((vertices[5].x == Catch::Approx(-1) && vertices[5].y == Catch::Approx(1) && vertices[5].z == Catch::Approx(-1)));
        REQUIRE((vertices[6].x == Catch::Approx(-1) && vertices[6].y == Catch::Approx(-1) && vertices[6].z == Catch::Approx(1)));
        REQUIRE((vertices[7].x == Catch::Approx(-1) && vertices[7].y == Catch::Approx(-1) && vertices[7].z == Catch::Approx(-1)));
        // NOLINTEND
    }
}
