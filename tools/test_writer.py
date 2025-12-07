"""Tests for arapi.writer module."""
import unittest
import tempfile
import os
import arapi.types
import arapi.writer


class TestKindToString(unittest.TestCase):
    """Test kind_to_string function."""

    def test_class_kind(self):
        result = arapi.writer.kind_to_string(arapi.types.agea_type_kind.CLASS)
        self.assertEqual(result, "agea_class")

    def test_struct_kind(self):
        result = arapi.writer.kind_to_string(arapi.types.agea_type_kind.STRUCT)
        self.assertEqual(result, "agea_struct")

    def test_external_kind(self):
        result = arapi.writer.kind_to_string(arapi.types.agea_type_kind.EXTERNAL)
        self.assertEqual(result, "agea_external")

    def test_invalid_kind(self):
        with self.assertRaises(SystemExit):
            arapi.writer.kind_to_string(arapi.types.agea_type_kind.NONE)


class TestGenerateBuilder(unittest.TestCase):
    """Test generate_builder function."""

    def test_generate_builder_true(self):
        result = arapi.writer.generate_builder(True, "test_name")
        self.assertIn("package_test_name_builder", result)
        self.assertIn("struct package_test_name_builder", result)
        self.assertIn("build", result)
        self.assertIn("destroy", result)

    def test_generate_builder_false(self):
        result = arapi.writer.generate_builder(False, "test_name")
        self.assertEqual(result, "")


class TestWriteArClassIncludeFile(unittest.TestCase):
    """Test write_ar_class_include_file function."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.context = arapi.types.file_context("test_module", "test_namespace")
        self.context.model_header_dir = self.temp_dir

    def tearDown(self):
        import shutil
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_write_class_include_basic(self):
        class_type = arapi.types.agea_type(arapi.types.agea_type_kind.CLASS)
        class_type.name = "TestClass"

        arapi.writer.write_ar_class_include_file(class_type, self.context, self.temp_dir)

        output_file = os.path.join(self.temp_dir, "TestClass.ar.h")
        self.assertTrue(os.path.exists(output_file))

        with open(output_file, "r") as f:
            content = f.read()
            self.assertIn("#pragma once", content)
            self.assertIn("AGEA_gen_meta__TestClass", content)
            self.assertIn("friend class package", content)

    def test_write_class_include_with_properties(self):
        class_type = arapi.types.agea_type(arapi.types.agea_type_kind.CLASS)
        class_type.name = "TestClass"

        prop1 = arapi.types.agea_property()
        prop1.name = "m_value1"
        prop1.name_cut = "value1"
        prop1.type = "int"
        prop1.access = "all"
        class_type.properties.append(prop1)

        prop2 = arapi.types.agea_property()
        prop2.name = "m_value2"
        prop2.name_cut = "value2"
        prop2.type = "float"
        prop2.access = "read_only"
        class_type.properties.append(prop2)

        arapi.writer.write_ar_class_include_file(class_type, self.context, self.temp_dir)

        output_file = os.path.join(self.temp_dir, "TestClass.ar.h")
        with open(output_file, "r") as f:
            content = f.read()
            self.assertIn("get_value1", content)
            self.assertIn("set_value1", content)
            self.assertIn("get_value2", content)
            self.assertNotIn("set_value2", content)

    def test_write_class_include_invalid_kind(self):
        struct_type = arapi.types.agea_type(arapi.types.agea_type_kind.STRUCT)
        struct_type.name = "TestStruct"

        with self.assertRaises(SystemExit):
            arapi.writer.write_ar_class_include_file(struct_type, self.context, self.temp_dir)


class TestWritePropertyAccessMethods(unittest.TestCase):
    """Test write_property_access_methods function."""

    def setUp(self):
        self.context = arapi.types.file_context("test_module", "test_namespace")
        self.context.properies_access_methods = ""

    def test_getter_all_access(self):
        prop = arapi.types.agea_property()
        prop.name = "m_value"
        prop.name_cut = "value"
        prop.type = "int"
        prop.owner = "TestClass"
        prop.access = "all"

        arapi.writer.write_property_access_methods(self.context, prop)

        self.assertIn("get_value", self.context.properies_access_methods)
        self.assertIn("set_value", self.context.properies_access_methods)
        self.assertIn("TestClass::get_value", self.context.properies_access_methods)
        self.assertIn("TestClass::set_value", self.context.properies_access_methods)
        self.assertIn("return m_value", self.context.properies_access_methods)

    def test_getter_read_only(self):
        prop = arapi.types.agea_property()
        prop.name = "m_value"
        prop.name_cut = "value"
        prop.type = "int"
        prop.owner = "TestClass"
        prop.access = "read_only"

        arapi.writer.write_property_access_methods(self.context, prop)

        self.assertIn("get_value", self.context.properies_access_methods)
        self.assertNotIn("set_value", self.context.properies_access_methods)

    def test_setter_write_only(self):
        prop = arapi.types.agea_property()
        prop.name = "m_value"
        prop.name_cut = "value"
        prop.type = "int"
        prop.owner = "TestClass"
        prop.access = "write_only"

        arapi.writer.write_property_access_methods(self.context, prop)

        self.assertNotIn("get_value", self.context.properies_access_methods)
        self.assertIn("set_value", self.context.properies_access_methods)

    def test_setter_with_check_not_same(self):
        prop = arapi.types.agea_property()
        prop.name = "m_value"
        prop.name_cut = "value"
        prop.type = "int"
        prop.owner = "TestClass"
        prop.access = "all"
        prop.check_not_same = True

        arapi.writer.write_property_access_methods(self.context, prop)

        self.assertIn("if(m_value == v)", self.context.properies_access_methods)
        self.assertIn("return;", self.context.properies_access_methods)

    def test_setter_with_invalidates_transform(self):
        prop = arapi.types.agea_property()
        prop.name = "m_value"
        prop.name_cut = "value"
        prop.type = "int"
        prop.owner = "TestClass"
        prop.access = "all"
        prop.invalidates_transform = True

        arapi.writer.write_property_access_methods(self.context, prop)

        self.assertIn("mark_transform_dirty", self.context.properies_access_methods)
        self.assertIn("update_children_matrixes", self.context.properies_access_methods)

    def test_setter_with_invalidates_render(self):
        prop = arapi.types.agea_property()
        prop.name = "m_value"
        prop.name_cut = "value"
        prop.type = "int"
        prop.owner = "TestClass"
        prop.access = "all"
        prop.invalidates_render = True

        arapi.writer.write_property_access_methods(self.context, prop)

        self.assertIn("mark_render_dirty", self.context.properies_access_methods)


class TestModelGenerateOverridesHeaders(unittest.TestCase):
    """Test model_generate_overrides_headers function."""

    def setUp(self):
        self.context = arapi.types.file_context("test_module", "test_namespace")

    def test_no_overrides(self):
        result = arapi.writer.model_generate_overrides_headers(self.context)
        self.assertEqual(result, "")

    def test_single_override(self):
        self.context.model_overrides.append("test/header.h")
        result = arapi.writer.model_generate_overrides_headers(self.context)
        self.assertIn("#include <test/header.h>", result)
        self.assertIn("\n", result)

    def test_multiple_overrides(self):
        self.context.model_overrides.append("test/header1.h")
        self.context.model_overrides.append("test/header2.h")
        result = arapi.writer.model_generate_overrides_headers(self.context)
        self.assertIn("#include <test/header1.h>", result)
        self.assertIn("#include <test/header2.h>", result)


class TestWriteArPackageIncludeFile(unittest.TestCase):
    """Test write_ar_package_include_file function."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.context = arapi.types.file_context("test_module", "test_namespace")
        self.context.package_header_dir = self.temp_dir

    def tearDown(self):
        import shutil
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_write_package_include_basic(self):
        arapi.writer.write_ar_package_include_file(self.context, self.temp_dir)

        output_file = os.path.join(self.temp_dir, "package.ar.h")
        self.assertTrue(os.path.exists(output_file))

        with open(output_file, "r") as f:
            content = f.read()
            self.assertIn("#pragma once", content)
            self.assertIn("test_module", content)
            self.assertIn("AGEA_gen_meta__test_module_package", content)

    def test_write_package_include_with_render_overrides(self):
        self.context.render_has_types_overrides = True
        self.context.render_has_custom_resources = True

        arapi.writer.write_ar_package_include_file(self.context, self.temp_dir)

        output_file = os.path.join(self.temp_dir, "package.ar.h")
        with open(output_file, "r") as f:
            content = f.read()
            self.assertIn("render_types", content)
            self.assertIn("render_custom_resource", content)


class TestWriteTypesResolvers(unittest.TestCase):
    """Test write_types_resolvers function."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.context = arapi.types.file_context("test_module", "test_namespace")
        self.context.package_header_dir = self.temp_dir
        self.context.includes.add('#include "test1.h"')
        self.context.includes.add('#include "test2.h"')

    def tearDown(self):
        import shutil
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_write_types_resolvers_basic(self):
        class_type = arapi.types.agea_type(arapi.types.agea_type_kind.CLASS)
        class_type.name = "TestClass"
        class_type.full_name = "test_module::TestClass"
        class_type.id = "test_id"
        self.context.types.append(class_type)

        arapi.writer.write_types_resolvers(self.context)

        output_file = os.path.join(self.temp_dir, "types_resolvers.ar.h")
        self.assertTrue(os.path.exists(output_file))

        with open(output_file, "r") as f:
            content = f.read()
            self.assertIn("#pragma once", content)
            self.assertIn("type_resolver", content)
            self.assertIn("TestClass", content)
            self.assertIn("#include \"test1.h\"", content)
            self.assertIn("#include \"test2.h\"", content)

    def test_write_types_resolvers_built_in(self):
        external_type = arapi.types.agea_type(arapi.types.agea_type_kind.EXTERNAL)
        external_type.name = "ExternalType"
        external_type.full_name = "ExternalType"
        external_type.built_in = True
        external_type.id = "external_id"
        self.context.types.append(external_type)

        arapi.writer.write_types_resolvers(self.context)

        output_file = os.path.join(self.temp_dir, "types_resolvers.ar.h")
        with open(output_file, "r") as f:
            content = f.read()
            self.assertIn("ExternalType", content)
            # Built-in types should not have '::' prefix
            self.assertNotIn("::ExternalType", content)


class TestWriteLuaStructType(unittest.TestCase):
    """Test write_lua_struct_type function."""

    def setUp(self):
        self.temp_file = tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.h')
        self.temp_file.close()
        self.context = arapi.types.file_context("test_module", "test_namespace")

    def tearDown(self):
        if os.path.exists(self.temp_file.name):
            os.unlink(self.temp_file.name)

    def test_write_lua_struct_type_with_constructors(self):
        struct_type = arapi.types.agea_type(arapi.types.agea_type_kind.STRUCT)
        struct_type.name = "TestStruct"
        struct_type.full_name = "test_module::TestStruct"

        ctor1 = arapi.types.agea_ctor()
        ctor1.name = "TestStruct(int)"
        struct_type.ctros.append(ctor1)

        ctor2 = arapi.types.agea_ctor()
        ctor2.name = "TestStruct(float)"
        struct_type.ctros.append(ctor2)

        with open(self.temp_file.name, 'w') as f:
            arapi.writer.write_lua_struct_type(f, self.context, struct_type)

        with open(self.temp_file.name, 'r') as f:
            content = f.read()
            self.assertIn("TestStruct", content)
            self.assertIn("sol::constructors", content)
            self.assertIn("TestStruct(int)", content)
            self.assertIn("TestStruct(float)", content)


class TestWriteLuaClassType(unittest.TestCase):
    """Test write_lua_class_type function."""

    def setUp(self):
        self.temp_file = tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.h')
        self.temp_file.close()
        self.context = arapi.types.file_context("test_module", "test_namespace")

    def tearDown(self):
        if os.path.exists(self.temp_file.name):
            os.unlink(self.temp_file.name)

    def test_write_lua_class_type_no_parent(self):
        class_type = arapi.types.agea_type(arapi.types.agea_type_kind.CLASS)
        class_type.name = "TestClass"
        class_type.full_name = "test_module::TestClass"

        with open(self.temp_file.name, 'w') as f:
            arapi.writer.write_lua_class_type(f, self.context, class_type)

        with open(self.temp_file.name, 'r') as f:
            content = f.read()
            self.assertIn("TestClass", content)
            self.assertIn("sol::no_constructor", content)
            self.assertIn("get_item", content)
            self.assertNotIn("sol::base_classes", content)

    def test_write_lua_class_type_with_parent(self):
        parent_type = arapi.types.agea_type(arapi.types.agea_type_kind.CLASS)
        parent_type.name = "BaseClass"
        parent_type.full_name = "test_module::BaseClass"

        class_type = arapi.types.agea_type(arapi.types.agea_type_kind.CLASS)
        class_type.name = "TestClass"
        class_type.full_name = "test_module::TestClass"
        class_type.parent_type = parent_type

        with open(self.temp_file.name, 'w') as f:
            arapi.writer.write_lua_class_type(f, self.context, class_type)

        with open(self.temp_file.name, 'r') as f:
            content = f.read()
            self.assertIn("sol::base_classes", content)
            self.assertIn("BaseClass", content)
            self.assertIn("lua_script_extention", content)


class TestWriteLuaUsertypeExtention(unittest.TestCase):
    """Test write_lua_usertype_extention function."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.context = arapi.types.file_context("test_module", "test_namespace")
        self.context.package_header_dir = self.temp_dir

    def tearDown(self):
        import shutil
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_write_lua_usertype_extention_basic(self):
        class_type = arapi.types.agea_type(arapi.types.agea_type_kind.CLASS)
        class_type.name = "TestClass"
        class_type.full_name = "test_module::TestClass"

        prop = arapi.types.agea_property()
        prop.name = "m_value"
        prop.name_cut = "value"
        prop.access = "all"
        class_type.properties.append(prop)

        func = arapi.types.agea_function()
        func.name = "testFunction"
        class_type.functions.append(func)

        self.context.types.append(class_type)

        arapi.writer.write_lua_usertype_extention(self.context)

        output_file = os.path.join(self.temp_dir, "types_script_importer.ar.h")
        self.assertTrue(os.path.exists(output_file))

        with open(output_file, "r") as f:
            content = f.read()
            self.assertIn("#pragma once", content)
            self.assertIn("TestClass__lua_script_extention", content)
            self.assertIn("get_value", content)
            self.assertIn("set_value", content)
            self.assertIn("testFunction", content)

    def test_write_lua_usertype_extention_with_parent(self):
        parent_type = arapi.types.agea_type(arapi.types.agea_type_kind.CLASS)
        parent_type.name = "BaseClass"
        self.context.types.append(parent_type)

        class_type = arapi.types.agea_type(arapi.types.agea_type_kind.CLASS)
        class_type.name = "TestClass"
        class_type.parent_type = parent_type
        self.context.types.append(class_type)

        arapi.writer.write_lua_usertype_extention(self.context)

        output_file = os.path.join(self.temp_dir, "types_script_importer.ar.h")
        with open(output_file, "r") as f:
            content = f.read()
            self.assertIn("BaseClass__lua_script_extention", content)


if __name__ == '__main__':
    unittest.main()

