from conans import ConanFile, CMake


class MCSC(ConanFile):
    name = "sbit_mpsc"
    version = "0.0.1"
    homepage = "https://github.com/0x8000-0000/mpsc"
    description = "Signbit libraries: Multiple Producer Single Consumer Queue"
    author = "Florin Iucha <florin@signbit.net>"
    topics = ["lock-free", "queue", "multithreading"]
    settings = "os", "compiler", "build_type", "arch"
    license = "Apache-2.0"

    generators = "cmake"

    def requirements(self):
        self.requires("gtest/1.10.0")
        self.requires("fmt/6.2.1")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
