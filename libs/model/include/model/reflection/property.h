#pragma once

#include "model/model_minimal.h"

#include "model/reflection/types.h"
#include "model/reflection/property_utils.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace agea
{
namespace reflection
{

class entry
{
public:
    static bool
    set_up();
};

class property
{
public:
    static bool
    save_to_string(property& from_property, blob_ptr ptr, fixed_size_buffer& buf);

    bool
    load_from_string(blob_ptr ptr, const std::string& str);

    bool
    save_to_string_with_hint(blob_ptr ptr, const std::string& hint, fixed_size_buffer& buf);

    bool
    load_from_string_with_hint(blob_ptr ptr, const std::string& hint, const std::string& str);

    // clang-format off
    property_deserialization_handler deserialization_handler = default_deserialize;
    property_serialization_handler   serialization_handler   = default_serialize;
    property_compare_handler         compare_handler         = default_compare;
    property_copy_handler            copy_handler            = default_copy;
    property_prototype_handler       protorype_handler       = default_prototype;


    blob_ptr get_blob(model::smart_object& obj);

    // clang-format on

    static bool
    deserialize_update(reflection::property& p,
                       blob_ptr ptr,
                       const serialization::conteiner& sc,
                       model::object_constructor_context& occ);

private:
    static bool
    default_compare(compare_context& context);

    static bool
    default_deserialize(deserialize_context& context);

    static bool
    default_serialize(serialize_context& context);

    static bool
    default_copy(copy_context& context);

    static bool
    default_prototype(property_prototype_context& context);

    static bool
    deserialize_collection(reflection::property& p,
                           model::smart_object& obj,
                           const serialization::conteiner& sc,
                           model::object_constructor_context& occ);

    static bool
    deserialize_item(reflection::property& p,
                     model::smart_object& obj,
                     const serialization::conteiner& sc,
                     model::object_constructor_context& occ);

    static bool
    serialize_collection(const reflection::property& p,
                         const model::smart_object& obj,
                         serialization::conteiner& sc);

    static bool
    serialize_item(const reflection::property& p,
                   const model::smart_object& obj,
                   serialization::conteiner& sc);

    static bool
    deserialize_update_collection(reflection::property& p,
                                  blob_ptr ptr,
                                  const serialization::conteiner& sc,
                                  model::object_constructor_context& occ);

    static bool
    deserialize_update_item(reflection::property& p,
                            blob_ptr ptr,
                            const serialization::conteiner& sc,
                            model::object_constructor_context& occ);

    static bool
    compare_collection(compare_context& context);

    static bool
    compare_item(compare_context& context);

public:
    // clang-format off
    std::string name;
    size_t offset;
    property_type_description type;

    std::string category;
    std::vector<std::string> hints;

    access_mode access                                              = access_mode::ro;
    bool serializable                                               = false;
    bool visible                                                    = false;



    type_compare_handler              types_compare_handler         = nullptr;
    type_copy_handler                 types_copy_handler            = nullptr;
    type_serialization_handler        types_serialization_handler   = nullptr;
    type_deserialization_handler      types_deserialization_handler = nullptr;
    type_serialization_update_handler types_update_handler          = nullptr;
    // clang-format on
};

struct class_reflection_table
{
    class_reflection_table(class_reflection_table* p)
        : parent(p)
    {
    }
    class_reflection_table* parent;
};

struct property_convertes
{
    using write_to_property = std::function<bool(blob_ptr, const std::string&)>;

    static std::vector<type_read_from_handler>&
    readers()
    {
        static std::vector<type_read_from_handler> s_readers;
        return s_readers;
    }

    static std::vector<write_to_property>&
    writers()
    {
        static std::vector<write_to_property> s_writers;
        return s_writers;
    }

    static bool
    init();

    // STR
    static bool read_t_str(AGEA_read_from_property_args);
    static bool
    write_t_str(blob_ptr ptr, const std::string& str);

    // Bool
    static bool read_t_bool(AGEA_read_from_property_args);
    static bool
    write_t_bool(blob_ptr ptr, const std::string& str);

    // I8
    static bool read_t_i8(AGEA_read_from_property_args);
    static bool
    write_t_i8(blob_ptr ptr, const std::string& str);

    // I16
    static bool read_t_i16(AGEA_read_from_property_args);
    static bool
    write_t_i16(blob_ptr ptr, const std::string& str);

    // I32
    static bool read_t_i32(AGEA_read_from_property_args);
    static bool
    write_t_i32(blob_ptr ptr, const std::string& str);

    // I64
    static bool read_t_i64(AGEA_read_from_property_args);
    static bool
    write_t_i64(blob_ptr ptr, const std::string& str);

    // U8
    static bool read_t_u8(AGEA_read_from_property_args);
    static bool
    write_t_u8(blob_ptr ptr, const std::string& str);

    // U16
    static bool read_t_u16(AGEA_read_from_property_args);
    static bool
    write_t_u16(blob_ptr ptr, const std::string& str);

    // U32
    static bool read_t_u32(AGEA_read_from_property_args);
    static bool
    write_t_u32(blob_ptr ptr, const std::string& str);

    // U64
    static bool read_t_u64(AGEA_read_from_property_args);
    static bool
    write_t_u64(blob_ptr ptr, const std::string& str);

    // Float
    static bool read_t_f(AGEA_read_from_property_args);
    static bool
    write_t_f(blob_ptr ptr, const std::string& str);

    // Double
    static bool read_t_d(AGEA_read_from_property_args);
    static bool
    write_t_d(blob_ptr ptr, const std::string& str);

    // Vec3
    static bool read_t_vec3(AGEA_read_from_property_args);
    static bool
    write_t_vec3(blob_ptr ptr, const std::string& str);

    // Material
    static bool read_t_mat(AGEA_read_from_property_args);
    static bool
    write_t_mat(blob_ptr ptr, const std::string& str);

    // Mesh
    static bool read_t_msh(AGEA_read_from_property_args);
    static bool
    write_t_msh(blob_ptr ptr, const std::string& str);
};

}  // namespace reflection
}  // namespace agea

#define AGEA_property(...)