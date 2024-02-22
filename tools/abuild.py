import os
import ctypes
import sys
import shutil
import psutil
import time

clean_build = False
open_env = False
build_engine = False


def isAdmin():
    try:
        is_admin = os.getuid() == 0
    except AttributeError:
        is_admin = ctypes.windll.shell32.IsUserAnAdmin() != 0
    return is_admin


def main():
    if not isAdmin():
        print("Admin rights required!!")
        exit(-1)

    should_reopned = False
    agea_root_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
    agea_build_dir = os.path.join(agea_root_dir, "build")

    if clean_build and os.path.exists(agea_build_dir):
        
        for proc in psutil.process_iter():
            # check whether the process name matches
            if proc.name() == "devenv.exe":

                cmdline = proc.cmdline()
                print(agea_build_dir)

                if len(cmdline) > 1 and cmdline[1] == (agea_build_dir + "\\agea.sln"):
                    should_reopned = True
                    proc.kill()
                    time.sleep(2)

        print("Removing " + agea_build_dir)
        shutil.rmtree(agea_build_dir)

    config_cmd = "cmake -A x64 -B{0} -S{1}".format(agea_build_dir, agea_root_dir)
    os.system(config_cmd)

    if build_engine:
        build_cmd = "cmake --build {0} --target engine_app".format(agea_build_dir)
        os.system(build_cmd)

    if open_env:
        os.system("code " + agea_root_dir)
        os.system("start " + agea_build_dir + "/agea.sln")

    if should_reopned:
        os.system("start " + agea_build_dir + "/agea.sln")


if __name__ == "__main__":

    for i in range(1, len(sys.argv)):
        if sys.argv[i] == "--clean":
            clean_build = True
        elif sys.argv[i] == "--openenv":
            open_env = True
        elif sys.argv[i] == "--build":
            build_engine = True
        else:
            print("Unsupported param!!!")
            exit(-1)

    main()
