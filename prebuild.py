import argparse
import shutil
import subprocess
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


def copy_path(src_path, dst_path) -> bool:
    src = Path(src_path)
    dst = Path(dst_path)

    if not src.exists():
        return True

    dst.parent.mkdir(parents=True, exist_ok=True)

    if src.is_dir():
        if dst.exists():
            if dst.is_dir():
                for p in src.iterdir():
                    if not copy_path(p, dst / p.name):
                        return False
                return True
            return False

        shutil.copytree(src, dst)
        return True

    shutil.copy2(src, dst)
    return True


class PrebuildClass:
    __args = None

    __os_name = None
    __os_arch = None
    __dep_dir = None
    __lib_base_dir = None
    __libs = None

    def main(self) -> None:
        self.parse_args()

        rm_path(self.__dep_dir)
        self.__dep_dir.mkdir(parents=True, exist_ok=True)

        libs = self.__libs.split(" ")
        for i in range(1, len(libs), 2):
            lib_name = libs[i - 1].strip()
            lib_version = libs[i].strip()
            if not lib_name:
                continue

            if not self.copy_lib(lib_name, lib_version):
                print(f"{lib_name} {lib_version} not found!")
                return

        self.create_so_link()

    def parse_args(self) -> None:
        parser = argparse.ArgumentParser(description="arg description")

        parser.add_argument("--os_name", required=True)
        parser.add_argument("--os_arch", required=True)
        parser.add_argument("--dep_dir", required=True)
        parser.add_argument("--lib_base_dir", required=True)
        parser.add_argument("--libs", required=True)

        self.__args = parser.parse_args()

        self.__os_name = self.__args.os_name
        self.__os_arch = self.__args.os_arch
        self.__dep_dir = Path(self.__args.dep_dir)
        self.__lib_base_dir = Path(self.__args.lib_base_dir)
        self.__libs = self.__args.libs.strip()

    def copy_lib(self, lib_name: str, lib_version: str) -> bool:
        lib_dir = self.__lib_base_dir / lib_name
        if not lib_dir.is_dir():
            print(f"{lib_name} not found!")
            return False

        sub_dirs = [p for p in lib_dir.iterdir() if p.is_dir()]
        if not sub_dirs:
            print(f"{lib_name} versions not found!")
            return False

        choose_version_dir = max(
            sub_dirs, key=lambda p: tuple(map(int, p.name.split(".")))
        )
        choose_version = choose_version_dir.name
        print(f"{lib_name} {lib_version} => choose {choose_version}")

        choose_os_arch_dir = choose_version_dir / self.__os_name / self.__os_arch
        copy_path(choose_os_arch_dir / "include", self.__dep_dir / "include" / lib_name)
        copy_path(choose_os_arch_dir / "lib", self.__dep_dir / "lib")
        return True

    def create_so_link(self) -> None:
        dep_lib_dir = self.__dep_dir / "lib"
        if not dep_lib_dir.is_dir():
            return

        so_file_names = [
            p.name for p in dep_lib_dir.iterdir() if p.is_file() and ".so." in p.name
        ]

        for so_file_name in so_file_names:
            last_so_index = so_file_name.rfind(".so")
            link_name = so_file_name[:last_so_index] + ".so"

            rm_path(dep_lib_dir / link_name)
            subprocess.run(
                f"cd {dir} && ln -sf {so_file_name} {link_name}",
                check=False,
                shell=True,
            )


if __name__ == "__main__":
    h = PrebuildClass()
    h.main()
