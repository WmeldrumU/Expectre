from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, cmake_layout


class ExpectreConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = ("CMakeDeps",)

    def requirements(self):
        self.requires("sdl/3.2.14")
        self.requires("spdlog/1.16.0")
        self.requires("glm/0.9.5.4")
        self.requires("stb/cci.20230920")
        self.requires("xxhash/0.8.3")
        self.requires("flecs/4.1.1")
        self.requires("fastgltf/0.9.0")
        
    def configure(self):
        self.options["sdl/*"].opengl = False
        self.options["sdl/*"].libusb = False

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def layout(self):
        cmake_layout(self)
