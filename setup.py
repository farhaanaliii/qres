from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext


class BuildExt(build_ext):
    def build_extensions(self):
        compiler_type = self.compiler.compiler_type
        for ext in self.extensions:
            if compiler_type == "msvc":
                ext.extra_compile_args = [
                    "/O2",
                    "/W4",
                    "/std:c11",
                    "/permissive-",
                    "/GS",
                    "/sdl",
                ]
            else:
                ext.extra_compile_args = [
                    "-std=c11",
                    "-O2",
                    "-Wall",
                    "-Wextra",
                    "-Wpedantic",
                    "-Wshadow",
                    "-Wstrict-prototypes",
                    "-Wpointer-arith",
                    "-Wformat=2",
                    "-D_FORTIFY_SOURCE=2",
                    "-fstack-protector-strong",
                ]
        super().build_extensions()


setup(
    cmdclass={"build_ext": BuildExt},
    ext_modules=[
        Extension(
            name="qres._qres",
            sources=[
                "src/qres.c",
                "src/compiler.c",
            ],
            include_dirs=["src"],
        )
    ],
)

