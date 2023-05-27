import os
import ctypes
import sys
import shutil

clean_build = False
open_env = False
build_engine = False



def isAdmin():
    try:
        is_admin = (os.getuid() == 0)
    except AttributeError:
        is_admin = ctypes.windll.shell32.IsUserAnAdmin() != 0
    return is_admin


def main():
    if not isAdmin():
        print("Admin rights required!!")
        exit(-1)

    agea_root_dir = os.path.dirname(
        os.path.dirname(os.path.realpath(__file__)))

    agea_build_dir = os.path.join(agea_root_dir, "build")

    if clean_build:
        print("Removing " + agea_build_dir)
        shutil.rmtree(agea_build_dir)

    config_cmd = "cmake -B{0} -S{1}".format(agea_build_dir, agea_root_dir)
    os.system(config_cmd)

    if build_engine:
        build_cmd = "cmake --build {0} --target engine_app".format(agea_build_dir)
        os.system(build_cmd)

    if open_env:
        os.system("code " + agea_root_dir)
        os.system("start " + agea_build_dir + "/agea.sln")


if __name__ == "__main__":

    for i in range(1, len(sys.argv)):
        if sys.argv[i] == "--clean":
            clean_build = True
        elif sys.argv[i] == "--openevn":
            open_env = True
        elif sys.argv[i] == "--build":
            build_engine = True
        else:
            print("Unsupported param!!!")
            exit(-1)

    main()
