# Expectre

conan install . --build=missing -if build -pr foo   
cmake -B build -G "Visual Studio 16 2019" -T ClangCL
cmake --build build --config Release

PS C:\Expectre> conan profile show foo  
Configuration for profile foo:

[settings]
os=Windows
os_build=Windows
arch=x86_64
arch_build=x86_64
compiler=Visual Studio
compiler.toolset=ClangCL
compiler.version=16
build_type=Release
[options]
[conf]
[build_requires]
[env]



