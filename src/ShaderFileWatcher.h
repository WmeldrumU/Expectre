#ifndef SHADERFILEWATCHER_H
#define SHADERFILEWATCHER_H

#include <iostream>
#include <fstream>
#include <filesystem> 
#include <shaderc/shaderc.hpp>

namespace Expectre {

	namespace fs = std::filesystem;

	class ShaderFileWatcher {
	public:
		ShaderFileWatcher() = delete;
		ShaderFileWatcher(const std::string& path) : m_path(path) {
			// Check if path exists
			bool exists = std::filesystem::exists(m_path);
			// If not throw exception
			if (!exists)
				throw std::runtime_error("Filewatcher path not found");
			// Use Win/Linux specific code to watch
			// FILETIME 
			m_last_write_time = std::filesystem::last_write_time(m_path);

			if (m_path.ends_with(".vert")) {
				std::cout << "It's a vertex shader.\n";
				m_shader_kind = shaderc_shader_kind::shaderc_vertex_shader;
			}
			else if (m_path.ends_with(".frag")) {
				m_shader_kind = shaderc_shader_kind::shaderc_fragment_shader;
			}

		}

		void check_for_changes() {
			auto write_time = fs::last_write_time(m_path);

			if (write_time != m_last_write_time) {
				compile();
				m_last_write_time = write_time;
			}
		}


	private:

		fs::file_time_type m_last_write_time;
		std::string m_path;
		shaderc_shader_kind m_shader_kind;

		std::string read_file() {
			std::ifstream file(m_path);
			if (!file) throw std::runtime_error("Failed to open shader file: " + m_path);
			return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		}

		void compile() {
			shaderc::Compiler compiler;
			shaderc::CompileOptions options;
			options.SetOptimizationLevel(shaderc_optimization_level::shaderc_optimization_level_performance);

			std::string shader_code = read_file();
			shaderc::CompilationResult result =
				compiler.CompileGlslToSpv(
					shader_code,
					m_shader_kind,
					m_path.c_str(),
					options);

			if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
				throw std::runtime_error("Shader compilation failed: " + result.GetErrorMessage());
			}

			std::string filename = std::filesystem::path(m_path).filename().string();
			std::ofstream out(filename + ".spv", std::ios::binary);
			out.write(reinterpret_cast<const char*>(result.cbegin()),
				std::distance(result.cbegin(), result.cend()) * sizeof(uint32_t));
			out.close();

		}
	};
}


#endif // FILEWATCHER_H