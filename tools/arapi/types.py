from enum import Enum


class kryga_type_kind(Enum):
  NONE = 0
  CLASS = 1
  STRUCT = 2
  EXTERNAL = 3


class kryga_property:

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
    self.property_save_handler = ""
    self.property_compare_handler = ""
    self.property_load_handler = ""
    self.property_copy_handler = ""
    self.property_snapshot_handler = ""
    self.property_instantiate_handler = ""
    self.instantiate_mode = ""
    self.gpu_data = ""
    self.gpu_texture_slot = -1  # -1 means not a texture slot, >= 0 is slot index
    self.copyable = "yes"
    self.updatable = "yes"
    self.ref = "false"
    self.has_default = "false"
    self.mcp_hint = ""


class kryga_type:

  def __init__(self, kind: kryga_type_kind):
    self.name = ""
    self.full_name = ""
    self.built_in = False
    self.id = ''
    self.architype = ""
    self.kind = kind
    self.script_support = False
    self.default_handlers = False

    self.properties: list[kryga_property] = []
    self.functions = []
    self.ctros = []
    self.construct_params: list[kryga_cparam] = []

    self.parent_name = ""
    self.parent_type: kryga_type = None
    self.ctro_line = ""

    self.compare_handler = ""
    self.copy_handler = ""
    self.load_handler = ""
    self.save_handler = ""
    self.instantiate_handler = ""
    self.render_cmd_builder = ""
    self.render_cmd_destroyer = ""
    self.render_cmd_transform = ""
    self.physics_cmd_builder = ""
    self.physics_cmd_destroyer = ""
    self.physics_cmd_transform = ""
    self.json_save_handler = ""
    self.json_load_handler = ""
    self.mcp_schema = ""
    self.mcp_hint = ""
    self.source_file = ""

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

    if module_namespace and (module_namespace.endswith('::' + module_name) or module_namespace == module_name):
      self.full_module_name = module_namespace
    elif module_namespace:
      self.full_module_name = module_namespace + '::' + module_name
    else:
      self.full_module_name = module_name

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
    self.editor_has_overrides: bool = False
    self.dependencies: list[str] = []

    self.properies_access_methods: str = ''
    self.output_dir = ''
    self.root_dir = ''
    self.types: list[kryga_type] = []
    self.model_overrides: list[str] = []
    self.render_overrides: list[str] = []
    self.editor_overrides: list[str] = []
    self.model_header_dir = None
    self.package_header_dir = None
    self.model_sources_dir = None
    self.render_header_dir = None
    self.render_sources_dir = None
    self.editor_sources_dir = None
    self.global_dir = None
    self.gpu_types_dir = None

  def topo_sort(self, t: kryga_type, ordered_types):

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

    #for t in ordered_types:
      #print(f"{t.name}")

    self.types = ordered_types


class kryga_cparam:

  def __init__(self, type_name: str = "", name: str = "", is_pointer: bool = False, is_optional: bool = False):
    self.type_name = type_name
    self.name = name
    self.is_pointer = is_pointer
    self.is_optional = is_optional


class kryga_function_param:

  def __init__(self, type_name: str = "", name: str = ""):
    self.type_name = type_name
    self.name = name


class kryga_function:

  def __init__(self):
    self.name = ""
    self.category = ""
    self.mcp_hint = ""
    self.return_type = ""
    self.params: list[kryga_function_param] = []
    self.is_const = False


class kryga_ctor:

  def __init__(self):
    self.name = ""
