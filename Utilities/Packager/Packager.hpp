#ifndef ATOM_PACKAGER_HPP
#define ATOM_PACKAGER_HPP

// Standard Library
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

namespace atom::tools {
	class Packager {
		public:
			struct Config {
				bool compress;
				bool verbose;
				bool preserveStructure;
				bool overwrite;

				Config();
			};

			struct FileEntry {
				std::string filename;
				uint64_t offset;
				uint64_t size;
				std::string type;
				std::string original_path;
			};

			enum class Result {
				SUCCESS,
				ERROR_OPEN_OUTPUT,
				ERROR_READ_FILE,
				ERROR_INVALID_PATH,
				ERROR_EMPTY_PACKAGE,
				ERROR_WRITE_FAILED,
				ERROR_ENCODING
			};

			Packager() = default;
			~Packager() = default;

			Packager(const Packager&) = delete;
			Packager& operator=(const Packager&) = delete;

			auto Pack(const std::vector<std::string>& resourcePaths, const std::string& outputFile, const Config& config = Config()) -> Result;
			[[nodiscard]] auto GetPackedFiles() const -> std::vector<std::string>;
			auto PrintPackageInfo() const -> void;

		private:
			std::vector<FileEntry> file_table_;
			std::unordered_map<std::string, size_t> file_index_;

			static constexpr char MAGIC[4] = {'A', 'P', 'K', 'G'};
			static constexpr uint16_t VERSION = 1;

			auto NormalizePath(const std::string& path) -> std::string;
			auto GenerateInternalFilename(const fs::path& filePath, const Config& config) -> std::string;
			auto ToUTF8(const std::string& str) -> std::string;
			auto SafePathToString(const fs::path& path) -> std::string;
			auto SafeRelativePath(const fs::path& path) -> std::string;

			auto CollectFiles(const std::vector<std::string>& resourcePaths, std::vector<fs::path>& allFiles, const Config& config) -> bool;
			auto IsValidUTF8(const std::string& str) -> bool;
	};
}


#endif // ATOM_PACKAGER_HPP
