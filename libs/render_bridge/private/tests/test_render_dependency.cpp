#include <gtest/gtest.h>

#include "render_bridge/render_dependency.h"

#include <root/assets/mesh.h>
#include <root/assets/material.h>
#include <root/components/mesh_component.h>

#include "core/reflection/reflection_type.h"

using namespace agea;

namespace
{

template <typename T>
std::shared_ptr<T>
make_obj(const utils::id& id)
{
    static auto r = glob::reflection_type_registry::getr().get_type(T::AR_TYPE_id());

    reflection::type_alloc_context ctx{&id};
    return cast_ref<T>(r->alloc(ctx));
}
}  // namespace

TEST(SimpleTest, happy)
{
    render_object_dependency_graph rd;

    auto mc1 = make_obj<root::mesh_component>(AID("mc1"));
    auto mc2 = make_obj<root::mesh_component>(AID("mc2"));

    auto mesh1 = make_obj<root::mesh>(AID("mesh1"));
    auto mesh2 = make_obj<root::mesh>(AID("mesh2"));

    auto mat1 = make_obj<root::material>(AID("mat1"));
    auto mat2 = make_obj<root::material>(AID("mat2"));

    mc1->set_mesh(mesh1.get());
    mc1->set_material(mat1.get());
    rd.build_node(mc1.get());
    rd.print(false);
    std::cout << "===============" << std::endl;

    mc1->set_mesh(mesh2.get());
    mc1->set_material(mat2.get());

    mc1->set_mesh(nullptr);
    mc1->set_material(nullptr);
    rd.build_node(mc1.get());
    rd.print(true);
}