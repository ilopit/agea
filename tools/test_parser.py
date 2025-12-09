import unittest
import tempfile
import os
import arapi.parser
import arapi.types
import arapi.utils


class TestParseAttributes(unittest.TestCase):

    def test_single_line_attributes(self):
        lines = ['AGEA_ar_class(key1=value1, key2=value2)']
        index, properties = arapi.parser.parse_attributes("AGEA_ar_class", lines, 0, 0)
        self.assertEqual(index, 0)
        self.assertEqual(len(properties), 2)
        # Properties may have leading/trailing spaces after parsing
        props_stripped = [p.strip() for p in properties]
        self.assertIn('key1=value1', props_stripped)
        self.assertIn('key2=value2', props_stripped)

    def test_multi_line_attributes(self):
        lines = [
            'AGEA_ar_class(key1=value1,',
            'key2=value2,',
            'key3=value3)'
        ]
        index, properties = arapi.parser.parse_attributes("AGEA_ar_class", lines, 0, 2)
        self.assertEqual(index, 2)
        self.assertEqual(len(properties), 3)

    def test_empty_attributes(self):
        lines = ['AGEA_ar_class()']
        index, properties = arapi.parser.parse_attributes("AGEA_ar_class", lines, 0, 0)
        self.assertEqual(index, 0)
        self.assertEqual(len(properties), 0)

    def test_attributes_with_spaces(self):
        lines = ['AGEA_ar_class( key1 = value1 , key2 = value2 )']
        index, properties = arapi.parser.parse_attributes("AGEA_ar_class", lines, 0, 0)
        self.assertEqual(index, 0)
        self.assertEqual(len(properties), 2)


class TestIsBool(unittest.TestCase):

    def test_true_value(self):
        self.assertTrue(arapi.parser.is_bool('true'))

    def test_false_value(self):
        self.assertFalse(arapi.parser.is_bool('false'))

    def test_invalid_value(self):
        with self.assertRaises(arapi.parser.InvalidBoolValueError) as ctx:
            arapi.parser.is_bool('invalid')
        self.assertIn("invalid", str(ctx.exception))


class TestExtractTypeConfig(unittest.TestCase):

    def setUp(self):
        self.context = arapi.types.file_context("test_module", "test_namespace")
        self.context.model_has_types_overrides = True
        self.context.render_has_types_overrides = True

    def test_copy_handler(self):
        type_obj = arapi.types.agea_type(arapi.types.agea_type_kind.CLASS)
        tokens = ['copy_handler=my_copy_handler']
        arapi.parser.extract_type_config(type_obj, tokens, self.context)
        self.assertEqual(type_obj.copy_handler, 'my_copy_handler')

    def test_multiple_handlers(self):
        type_obj = arapi.types.agea_type(arapi.types.agea_type_kind.CLASS)
        tokens = [
            'copy_handler=my_copy',
            'serialize_handler=my_serialize',
            'compare_handler=my_compare'
        ]
        arapi.parser.extract_type_config(type_obj, tokens, self.context)
        self.assertEqual(type_obj.copy_handler, 'my_copy')
        self.assertEqual(type_obj.serialize_handler, 'my_serialize')
        self.assertEqual(type_obj.compare_handler, 'my_compare')

    def test_render_constructor(self):
        type_obj = arapi.types.agea_type(arapi.types.agea_type_kind.CLASS)
        tokens = ['render_constructor=my_render_ctor']
        arapi.parser.extract_type_config(type_obj, tokens, self.context)
        self.assertEqual(type_obj.render_constructor, 'my_render_ctor')

    def test_built_in_external(self):
        type_obj = arapi.types.agea_type(arapi.types.agea_type_kind.EXTERNAL)
        tokens = ['built_in=true']
        arapi.parser.extract_type_config(type_obj, tokens, self.context)
        self.assertTrue(type_obj.built_in)

    def test_architype(self):
        type_obj = arapi.types.agea_type(arapi.types.agea_type_kind.CLASS)
        tokens = ['architype=my_architype']
        arapi.parser.extract_type_config(type_obj, tokens, self.context)
        self.assertEqual(type_obj.architype, 'my_architype')


class TestParseFile(unittest.TestCase):

    def setUp(self):
        self.context = arapi.types.file_context("test_module", "test_namespace")
        self.temp_dir = tempfile.mkdtemp()

    def tearDown(self):
        import shutil
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_parse_model_overrides(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_model_overrides()\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertIn("test.h", self.context.model_overrides)
        self.assertIn('#include "test.h"', self.context.includes)

    def test_parse_render_overrides(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_render_overrides()\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertIn("test.h", self.context.render_overrides)
        self.assertIn('#include "test.h"', self.context.includes)

    def test_parse_class(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class TestClass {\n')
            f.write('};\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertEqual(len(self.context.types), 1)
        self.assertEqual(self.context.types[0].name, "TestClass")
        self.assertEqual(self.context.types[0].kind, arapi.types.agea_type_kind.CLASS)

    def test_parse_class_with_parent(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class TestClass : public BaseClass {\n')
            f.write('};\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertEqual(len(self.context.types), 1)
        self.assertEqual(self.context.types[0].name, "TestClass")
        self.assertEqual(self.context.types[0].parent_name, "BaseClass")

    def test_parse_struct(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_struct()\n')
            f.write('struct TestStruct {\n')
            f.write('};\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertEqual(len(self.context.types), 1)
        self.assertEqual(self.context.types[0].name, "TestStruct")
        self.assertEqual(self.context.types[0].kind, arapi.types.agea_type_kind.STRUCT)

    def test_parse_property(self):
        # Format is "key=value" (entire pair in quotes), not "key"="value"
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class TestClass {\n')
            f.write('  AGEA_ar_property("category=test", "access=all")\n')
            f.write('  int m_value;\n')
            f.write('};\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertEqual(len(self.context.types), 1)
        self.assertEqual(len(self.context.types[0].properties), 1)
        prop = self.context.types[0].properties[0]
        self.assertEqual(prop.name, "m_value")
        self.assertEqual(prop.name_cut, "value")
        self.assertEqual(prop.type, "int")
        self.assertEqual(prop.category, "test")
        self.assertEqual(prop.access, "all")

    def test_parse_property_with_default(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class TestClass {\n')
            f.write('  AGEA_ar_property("default=true")\n')
            f.write('  int m_value = 42;\n')
            f.write('};\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        prop = self.context.types[0].properties[0]
        self.assertEqual(prop.has_default, "true")

    def test_parse_property_invalidates(self):
        # The parser splits by comma, so we need separate strings for each value
        # Or use a format that works - let's test with a single value first
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class TestClass {\n')
            f.write('  AGEA_ar_property("invalidates=render", "invalidates=transform")\n')
            f.write('  int m_value;\n')
            f.write('};\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        prop = self.context.types[0].properties[0]
        self.assertTrue(prop.invalidates_render)
        self.assertTrue(prop.invalidates_transform)

    def test_parse_property_check(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class TestClass {\n')
            f.write('  AGEA_ar_property("check=not_same")\n')
            f.write('  int m_value;\n')
            f.write('};\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        prop = self.context.types[0].properties[0]
        self.assertTrue(prop.check_not_same)

    def test_parse_property_hint(self):
        # The parser splits by comma, so for hint with multiple values we need separate strings
        # But hint actually processes the comma-separated values, so let's test with a workaround
        # Actually, looking at the parser code, hint processes comma-separated values from a single string
        # But parse_attributes splits by comma first, so we can't have commas in values
        # Let's just test with a single hint value
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class TestClass {\n')
            f.write('  AGEA_ar_property("hint=hint1")\n')
            f.write('  int m_value;\n')
            f.write('};\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        prop = self.context.types[0].properties[0]
        self.assertIn('"hint1"', prop.hint)

    def test_parse_function(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class TestClass {\n')
            f.write('  AGEA_ar_function()\n')
            f.write('  void testFunction();\n')
            f.write('};\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertEqual(len(self.context.types[0].functions), 1)
        self.assertEqual(self.context.types[0].functions[0].name, "testFunction")

    def test_parse_constructor(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_struct()\n')
            f.write('struct TestStruct {\n')
            f.write('  AGEA_ar_ctor()\n')
            f.write('  TestStruct(int value);\n')
            f.write('};\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertEqual(len(self.context.types[0].ctros), 1)
        self.assertIn("TestStruct", self.context.types[0].ctros[0].name)

    def test_parse_external_type(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_external_type()\n')
            f.write('struct ::external::Type;\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertEqual(len(self.context.types), 1)
        self.assertEqual(self.context.types[0].kind, arapi.types.agea_type_kind.EXTERNAL)
        self.assertIn("Type", self.context.types[0].name)

    def test_parse_package(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_package(model.has_types_overrides=true, render.has_overrides=false)\n')
            f.write('// extra line to avoid index error\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertTrue(self.context.model_has_types_overrides)
        self.assertFalse(self.context.render_has_types_overrides)

    def test_parse_package_dependencies(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_package(dependancies=dep1:dep2:dep3)\n')
            f.write('// extra line to avoid index error\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertEqual(len(self.context.dependencies), 3)
        self.assertIn("dep1", self.context.dependencies)
        self.assertIn("dep2", self.context.dependencies)
        self.assertIn("dep3", self.context.dependencies)

    def test_parse_class_with_namespace(self):
        self.context.root_namespace = "agea"
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class TestClass {\n')
            f.write('};\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertIn("agea::test_module::TestClass", self.context.types[0].full_name)

    def test_parse_class_with_full_namespace(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class ns1::ns2::TestClass {\n')
            f.write('};\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertEqual(self.context.types[0].name, "TestClass")

    def test_parse_multiple_properties(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class TestClass {\n')
            f.write('  AGEA_ar_property("category=test1")\n')
            f.write('  int m_value1;\n')
            f.write('  AGEA_ar_property("category=test2")\n')
            f.write('  float m_value2;\n')
            f.write('};\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertEqual(len(self.context.types[0].properties), 2)
        self.assertEqual(self.context.types[0].properties[0].name, "m_value1")
        self.assertEqual(self.context.types[0].properties[1].name, "m_value2")

    def test_parse_property_all_keys(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class TestClass {\n')
            f.write('  AGEA_ar_property("category=cat", "serializable=true", "access=all", "gpu_data=gpu", "copyable=yes", "ref=false")\n')
            f.write('  int m_value;\n')
            f.write('};\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        prop = self.context.types[0].properties[0]
        self.assertEqual(prop.category, "cat")
        self.assertEqual(prop.serializable, "true")
        self.assertEqual(prop.access, "all")
        self.assertEqual(prop.gpu_data, "gpu")
        self.assertEqual(prop.copyable, "yes")
        self.assertEqual(prop.ref, "false")

    def test_parse_property_updatable(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class TestClass {\n')
            f.write('  AGEA_ar_property("updatable=no")\n')
            f.write('  int m_value;\n')
            f.write('};\n')

        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        prop = self.context.types[0].properties[0]
        self.assertEqual(prop.updatable, "no")
        # Verify copyable remains at default value
        self.assertEqual(prop.copyable, "yes")

    def test_parse_property_handlers(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class TestClass {\n')
            f.write('  AGEA_ar_property("property_ser_handler=ser", "property_des_handler=des", "property_compare_handler=cmp", "property_copy_handler=cpy", "property_instantiate_handler=inst", "property_load_derive_handler=load")\n')
            f.write('  int m_value;\n')
            f.write('};\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        prop = self.context.types[0].properties[0]
        self.assertEqual(prop.property_ser_handler, "ser")
        self.assertEqual(prop.property_des_handler, "des")
        self.assertEqual(prop.property_compare_handler, "cmp")
        self.assertEqual(prop.property_copy_handler, "cpy")
        self.assertEqual(prop.property_instantiate_handler, "inst")
        self.assertEqual(prop.property_load_derive_handler, "load")

    def test_parse_class_with_config(self):
        self.context.model_has_types_overrides = True
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class(copy_handler=my_copy, serialize_handler=my_serialize)\n')
            f.write('class TestClass {\n')
            f.write('};\n')

        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        type_obj = self.context.types[0]
        self.assertEqual(type_obj.copy_handler, "my_copy")
        self.assertEqual(type_obj.serialize_handler, "my_serialize")

    def test_parse_property_invalid_access(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class TestClass {\n')
            f.write('  AGEA_ar_property("access=invalid_access")\n')
            f.write('  int m_value;\n')
            f.write('};\n')

        with self.assertRaises(arapi.parser.InvalidPropertyError) as ctx:
            arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertIn("access", str(ctx.exception))
        self.assertIn("invalid_access", str(ctx.exception))

    def test_parse_property_invalid_serializable(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class TestClass {\n')
            f.write('  AGEA_ar_property("serializable=yes")\n')
            f.write('  int m_value;\n')
            f.write('};\n')

        with self.assertRaises(arapi.parser.InvalidPropertyError) as ctx:
            arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertIn("serializable", str(ctx.exception))

    def test_parse_property_invalid_copyable(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class TestClass {\n')
            f.write('  AGEA_ar_property("copyable=true")\n')
            f.write('  int m_value;\n')
            f.write('};\n')

        with self.assertRaises(arapi.parser.InvalidPropertyError) as ctx:
            arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertIn("copyable", str(ctx.exception))
        self.assertIn("yes", str(ctx.exception))

    def test_include_path_generation(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('// empty\n')
        
        arapi.parser.parse_file(test_file, "include/packages/test.h", "test_module", self.context)
        self.assertIn('#include "packages/test.h"', self.context.includes)

    def test_empty_file(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertEqual(len(self.context.types), 0)
        self.assertIn('#include "test.h"', self.context.includes)

    def test_file_with_comments_only(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('// This is a comment\n')
            f.write('/* Another comment */\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertEqual(len(self.context.types), 0)

    def test_multiple_classes_raises_error(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class Class1 {\n')
            f.write('};\n')
            f.write('AGEA_ar_class()\n')
            f.write('class Class2 {\n')
            f.write('};\n')

        with self.assertRaises(arapi.parser.ParserError) as ctx:
            arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertIn("Nested class declaration not allowed", str(ctx.exception))
        self.assertIn("Class1", str(ctx.exception))

    def test_class_and_struct_together(self):
        test_file = os.path.join(self.temp_dir, "test.h")
        with open(test_file, 'w') as f:
            f.write('AGEA_ar_class()\n')
            f.write('class TestClass {\n')
            f.write('};\n')
            f.write('AGEA_ar_struct()\n')
            f.write('struct TestStruct {\n')
            f.write('};\n')
        
        arapi.parser.parse_file(test_file, "include/test.h", "test_module", self.context)
        self.assertEqual(len(self.context.types), 2)
        class_names = [t.name for t in self.context.types]
        self.assertIn("TestClass", class_names)
        self.assertIn("TestStruct", class_names)


if __name__ == '__main__':
    unittest.main()

