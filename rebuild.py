import argparse
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path


def rm_path(file_name, retries=3) -> bool:
    path = Path(file_name)
    if path.is_symlink() or path.is_file():
        path.unlink()
        return True

    if path.is_dir():
        for i in range(retries):
            try:
                shutil.rmtree(file_name)
                return True
            except OSError:
                if i == retries - 1:
                    return False
                time.sleep(1)
        return False

    return False


class Rebuild:
    __env_dir = None
    __args = None

    __type = None
    __static_lib = None

    __top_dir = None

    def main(self) -> None:
        self.__env_dir = Path(__file__).resolve().parent
        self.parse_args()

        self.__top_dir = self.__env_dir.parent.parent.parent

        self.build(self.__top_dir / "source" / "lib" / "lccl")
        self.build(self.__top_dir / "source" / "program" / "ts_parser")

    def parse_args(self) -> None:
        parser = argparse.ArgumentParser(description="arg description")

        parser.add_argument(
            "-t",
            "--type",
            help="指定编译类型",
            default="debug",
            choices=["debug", "release"],
        )

        parser.add_argument("--static", help="是否编译静态库", action="store_true")

        self.__args = parser.parse_args()

        self.__type = self.__args.type

        if self.__args.static:
            self.__static_lib = "ON"
        else:
            self.__static_lib = "OFF"
        print(f"tpye={self.__type}, static lib={self.__static_lib}")

    def build(self, project_dir: Path) -> None:
        print(f"\n\033[33mmake project {project_dir.name}...\033[0m")

        os.chdir(project_dir)

        build_dir = project_dir / "build"
        rm_path(build_dir)
        build_dir.mkdir(parents=True, exist_ok=True)

        os.chdir("build")
        if "win32" == sys.platform:
            cmake_generator = "Visual Studio 17 2022"
            cmake_arch = "x64"
            subprocess.run(
                [
                    "cmake",
                    "..",
                    f"-G{cmake_generator}",
                    f"-A{cmake_arch}",
                    f"-DSTATIC_LIB={self.__static_lib}",
                ],
                check=True,
                shell=False,
            )
            subprocess.run(
                ["cmake", "--build", ".", "--config", self.__type],
                check=True,
                shell=False,
            )
        else:
            subprocess.run(
                [
                    "cmake",
                    "..",
                    f"-DCMAKE_BUILD_TYPE={self.__type}",
                    f"-DSTATIC_LIB={self.__static_lib}",
                ],
                check=True,
                shell=False,
            )
            subprocess.run(
                ["make", f"-j{os.cpu_count()}"],
                check=True,
                shell=False,
            )


if __name__ == "__main__":
    rebuild = Rebuild()
    rebuild.main()
