// A program of the row: the C++ library over openkal-musl over openkal-svr4. What it prints is
// what the checks of this repository read.
import std;

namespace fs = std::filesystem;

[[noreturn]] void refuse(const char* what) { throw std::runtime_error(what); }

int main(int argc, char** argv) {
    std::vector<std::string> arguments(argv + 1, argv + argc);
    std::ranges::sort(arguments);
    std::println("arguments: {}", arguments);
    try {
        refuse("an exception crossed a frame");
    } catch (const std::exception& e) {
        std::println("caught: {}", e.what());
    }

    const fs::path directory = "oks-hello.dir";
    std::error_code ignored;
    fs::remove_all(directory, ignored);
    fs::create_directory(directory);
    for (const char* name : {"one.txt", "two.txt", "three.txt"}) {
        std::ofstream(directory / name) << name << '\n';
    }
    fs::rename(directory / "two.txt", directory / "second.txt");
    std::vector<std::string> names;
    for (const auto& entry : fs::directory_iterator(directory)) {
        names.push_back(entry.path().filename().string());
    }
    std::ranges::sort(names);
    std::println("listed: {}", names);
    std::string line;
    std::getline(std::ifstream(directory / "second.txt") >> std::ws, line);
    std::println("read: {} ({} bytes)", line, fs::file_size(directory / "second.txt"));
    std::println("removed: {}", fs::remove_all(directory));
    std::println("system: {}", std::getenv("PATH") != nullptr ? "has PATH" : "no PATH");
    return 7;
}
