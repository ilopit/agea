from enum import Enum


class agea_type_kind(Enum):
  NONE = 0
  CLASS = 1
  STRUCT = 2
  EXTERNAL = 3


class agea_property:

  def __init__(self):
    self.name = ""
    self.name_cut = ""
    self.category = ""
    self.type = ""
    self.access = "no"
    self.owner = ""
    self.hint = ""
    self.invalidates_render = False
    self.invalidates_transform = False
    self.check_not_same = False
    self.serializable = "false"
    self.property_ser_handler = ""
    self.property_des_handler = ""
    self.property_compare_handler = ""
    self.property_load_derive_handler = ""
    self.property_copy_handler = ""
    self.property_instantiate_handler = ""
    self.gpu_data = ""
    self.copyable = "yes"
    self.updatable = "yes"
    self.ref = "false"
    self.has_default = "false"


class agea_type:

  def __init__(self, kind: agea_type_kind):
    self.name = ""
    self.full_name = ""
    self.built_in = False
    self.id = ''
    self.architype = ""
    self.kind = kind
    self.script_support = False
    self.default_handlers = False

    self.properties: list[agea_property] = []
    self.functions = []
    self.ctros = []

    self.parent_name = ""
    self.parent_type: agea_type = None
    self.ctro_line = ""

    self.compare_handler = ""
    self.copy_handler = ""
    self.deserialize_handler = ""
    self.load_derive_handler = ""
    self.serialize_handler = ""
    self.to_string_handler = ""
    self.instantiate_handler = ""
    self.render_constructor = ""
    self.render_destructor = ""

    self.ordered = False

  def get_full_type_name(self):
    if self.built_in:
      return self.name
    else:
      return '::' + self.full_name


class file_context:

  def __init__(self, module_name, module_namespace):
    self.module_name = module_name
    self.root_namespace = module_namespace

    self.full_module_name = ''

    if module_namespace:
      self.full_module_name = module_namespace + '::' + module_name
    else:
      self.full_module_name = module_namespace

    self.includes = set()
    self.custom_types = list()
    self.content = ""
    self.soal = ""
    self.empty_cache = ""
    self.reg_types = ""
    self.methods = ""
    self.lua_binding = ""
    self.lua_ctor = ""

    self.model_has_types_overrides: bool = False
    self.model_has_properties_overrides: bool = False
    self.render_has_types_overrides: bool = False
    self.render_has_custom_resources: bool = False
    self.dependencies: list[str] = []

    self.properies_access_methods: str = ''
    self.output_dir = ''
    self.root_dir = ''
    self.types: list[agea_type] = []
    self.model_overrides: list[str] = []
    self.render_overrides: list[str] = []
    self.model_header_dir = None
    self.package_header_dir = None
    self.model_sources_dir = None
    self.render_header_dir = None
    self.render_sources_dir = None
    self.global_dir = None

  def topo_sort(self, t: agea_type, ordered_types):

    if not t.ordered:
      if t.parent_type:
        self.topo_sort(t.parent_type, ordered_types)
      ordered_types.append(t)
      t.ordered = True

  def order_types_by_parent(self):

    for t in self.types:
      for k in self.types:
        if t.parent_name == k.name:
          t.parent_type = k

    ordered_types = []

    for t in self.types:
      self.topo_sort(t, ordered_types)

    for t in ordered_types:
      print(f"{t.name}")

    self.types = ordered_types


class agea_function:

  def __init__(self):
    self.name = ""


class agea_ctor:

  def __init__(self):
    self.name = ""
