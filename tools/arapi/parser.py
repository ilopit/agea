import arapi.types
import arapi.utils
import os


def parse_attributes(name: str, lines, index, max_index):
  property_like = lines[index]
  while index <= max_index and lines[index].find(")") == -1:
    index = index + 1
    property_like += lines[index].strip() + " "

  properties = property_like[property_like.find("(") + 1:property_like.find(")")].split(",")

  properties[:] = [x for x in properties if x]

  return index, properties


def is_bool(value: str):
  if value == 'true':
    return True
  elif value == 'false':
    return False
  else:
    arapi.utils.eprint('UNSUPPORTED!')
    exit(1)


def parse_file(original_file_full_path, original_file_rel_path, module_name,
               context: arapi.types.file_context):
  arapi.utils.eprint("processing : {0} ...".format(original_file_full_path))

  class_name = os.path.basename(original_file_full_path)[:-2]

  cfg = open(original_file_full_path, "r")
  lines = cfg.readlines()
  lines_count = len(lines)

  i = 0

  current_class = None
  current_struct = None

  while i != lines_count:
    line = lines[i].strip()

    if line.startswith("AGEA_ar_class"):
      current_class = arapi.types.agea_type(arapi.types.agea_type_kind.CLASS)
      current_class.has_namespace = True

      i, tokens = arapi.parser.parse_attributes("AGEA_ar_class(", lines, i, lines_count)
      i = i + 1

      for t in tokens:

        pairs = t.strip().split("=")

        if len(pairs) == 1:
          key = arapi.utils.extstrip(pairs[0])
          if key == 'use-default-handlers':
            current_class.default_handlers = True

        if len(pairs) == 2:
          key = arapi.utils.extstrip(pairs[0])
          value = arapi.utils.extstrip(pairs[1])

          if key == 'architype':
            current_class.architype = value
          elif key == 'copy_handler':
            current_class.copy_handler = value
          elif key == 'compare_handler':
            current_class.compare_handler = value
          elif key == 'serialize_handler':
            current_class.serialize_handler = value
          elif key == 'deserialize_handler':
            current_class.deserialize_handler = value
          elif key == 'deserialize_from_proto_handler':
            current_class.deserialize_from_proto_handle = value
          elif key == 'to_string_handler':
            current_class.to_string_handle = value

      final_tokens = []
      class_tokens = (lines[i].replace(" : ", " ").replace("\n", " ").replace(",", " ").split(" "))
      for t in class_tokens:
        if t not in {"class", "public", "private", str()}:
          final_tokens.append(t)

      current_class.name = final_tokens[0].split('::')[-1]
      prefix = ''
      if context.root_namespace:
        prefix += context.root_namespace
        prefix += '::'

      current_class.full_name = f'{prefix}{module_name}::{current_class.name}'

      if len(final_tokens) > 1:
        current_class.parent_name = final_tokens[1]
        current_class.parent_short = final_tokens[1].split("::")[-1]

    if line.startswith("AGEA_ar_struct()"):
      if current_struct is None:
        current_struct = arapi.types.agea_type(arapi.types.agea_type_kind.STRUCT)
        current_struct.has_namespace = True

      i = i + 1
      final_tokens = []
      class_tokens = (lines[i].replace(" : ", " ").replace("\n", " ").replace(",", " ").split(" "))
      for t in class_tokens:
        if t not in {"struct", "public", "private", str()}:
          final_tokens.append(t)

      current_struct.name = final_tokens[0].split('::')[-1]
      prefix = ''
      if context.root_namespace:
        prefix += context.root_namespace
        prefix += '::'

      current_struct.full_name = f'{prefix}{module_name}::{current_struct.name}'

    if line.startswith("AGEA_ar_function"):
      function_header_like = ""
      function_body_like = ""

      current_fucntion = arapi.types.agea_function()
      function_header_like += line + " "

      while i <= lines_count and lines[i].find(")") == -1:
        i = i + 1
        function_header_like += lines[i].strip() + " "

      i = i + 1
      function_body_like += lines[i] + " "

      while i <= lines_count and lines[i].find(")") == -1:
        i = i + 1
        function_body_like += lines[i].strip() + " "

      function_body_like = function_body_like.strip().replace("\n", " ")
      fff = function_body_like.split(" ")

      matches = next(x for x in fff if x.find("(") != -1)
      current_fucntion.name = matches[:matches.find("(")]
      if current_class:
        current_class.functions.append(current_fucntion)

      if current_struct:
        current_struct.functions.append(current_fucntion)

    if line.startswith("AGEA_ar_property"):

      prop = arapi.types.agea_property()

      i, property_meta_tockens = parse_attributes("AGEA_ar_property(", lines, i, lines_count)
      i = i + 1

      property_tokens = lines[i].strip()[:-1].split()
      prop.type = property_tokens[0]
      prop.name = property_tokens[1]
      prop.name_cut = property_tokens[1][2:]
      prop.owner = class_name

      for pf in property_meta_tockens:
        pairs = pf.strip().split("=")
        #eprint("DBG! {0}".format(pairs))
        pairs[0] = pairs[0][1:]
        pairs[1] = pairs[1][:-1]

        if len(pairs) != 2:
          arapi.utils.eprint("Wrong numbers of pairs! {0}".format(pairs))
          exit(-1)

        if pairs[0] == "category":
          prop.category = pairs[1]
        elif pairs[0] == "serializable":
          prop.serializable = pairs[1]
        elif pairs[0] == "property_ser_handler":
          prop.property_ser_handler = pairs[1]
        elif pairs[0] == "property_des_handler":
          prop.property_des_handler = pairs[1]
        elif pairs[0] == "property_prototype_handler":
          prop.property_prototype_handler = pairs[1]
        elif pairs[0] == "property_compare_handler":
          prop.property_compare_handler = pairs[1]
        elif pairs[0] == "property_copy_handler":
          prop.property_copy_handler = pairs[1]
        elif pairs[0] == "access":
          prop.access = pairs[1]
        elif pairs[0] == "default":
          prop.has_default = pairs[1]
          if len(property_tokens) < 3 or property_tokens[2] != "=":
            arapi.utils.eprint("Please provide default arument")
            exit(-1)
        elif pairs[0] == "gpu_data":
          prop.gpu_data = pairs[1]
        elif pairs[0] == "copyable":
          prop.copyable = pairs[1]
        elif pairs[0] == "updatable":
          prop.copyable = pairs[1]
        elif pairs[0] == "ref":
          prop.ref = pairs[1]
        elif pairs[0] == "invalidates":
          tokens = pairs[1].split(",")
          for t in tokens:
            t = arapi.utils.extstrip(t)
            if t == "render":
              prop.invalidates_render = True
            elif t == "transform":
              prop.invalidates_transform = True
        elif pairs[0] == "check":
          tokens = pairs[1].split(",")
          for t in tokens:
            t = arapi.utils.extstrip(t)
            if t == "not_same":
              prop.check_not_same = True
        elif pairs[0] == "hint":
          tokens = pairs[1].split(",")
          prop.hint += ""
          for t in tokens:
            prop.hint += '"'
            prop.hint += t
            prop.hint += '",'
          if len(prop.hint) > 0:
            prop.hint = prop.hint[:-1]
          prop.hint += ""
        else:
          arapi.utils.eprint("Unsupported property = " + pairs[0])
          exit(-1)

      current_class.properties.append(prop)

    if line.startswith("AGEA_ar_ctor"):
      ctor_body_like = ""
      ctor = arapi.types.agea_ctor()
      ctor_header_like = line + " "

      while i <= lines_count and lines[i].find(")") == -1:
        i = i + 1
        ctor_header_like += lines[i].strip() + " "

      i = i + 1
      ctor_body_like += lines[i] + " "

      while i <= lines_count and lines[i].find(")") == -1:
        i = i + 1
        ctor_body_like += lines[i].strip() + " "

      ctor.name = ctor_body_like.strip().replace("\n", " ")
      current_struct.ctros.append(ctor)

    if line.startswith("AGEA_ar_external_type("):
      i, tokens = parse_attributes("AGEA_ar_external_type", lines, i, lines_count)

      extrnal_type = arapi.types.agea_type(arapi.types.agea_type_kind.EXTERNAL)

      for pf in tokens:
        pairs = pf.strip().split("=")
        arapi.utils.eprint("DBG! {0}".format(pairs))
        if len(pairs) == 2:
          key = arapi.utils.extstrip(pairs[0])
          value = arapi.utils.extstrip(pairs[1])

          if key == 'script-support':
            extrnal_type.script_support = is_bool(value)
          elif key == 'architype':
            extrnal_type.architype = value
          elif key == 'copy_handler':
            extrnal_type.copy_handler = value
          elif key == 'compare_handler':
            extrnal_type.compare_handler = value
          elif key == 'serialize_handler':
            extrnal_type.serialize_handler = value
          elif key == 'deserialize_handler':
            extrnal_type.deserialize_handler = value
          elif key == 'deserialize_from_proto_handler':
            extrnal_type.deserialize_from_proto_handle = value
          elif key == 'to_string_handler':
            extrnal_type.to_string_handle = value

        elif len(pairs) == 1:
          key = arapi.utils.extstrip(pairs[0])
          if key == 'built-in':
            extrnal_type.built_in = True
          elif key == 'use-default-handlers':
            extrnal_type.default_handlers = True
          else:
            extrnal_type.full_name = pairs[0].removeprefix('::')
            extrnal_type.name = extrnal_type.full_name.split('::')[-1]

      context.types.append(extrnal_type)

    i = i + 1

  if current_class:
    context.types.append(current_class)

  if current_struct:
    context.types.append(current_struct)

  include = '#include "' + original_file_rel_path[8:] + '"'

  context.includes.add(include)
