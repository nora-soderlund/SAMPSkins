#include <string>
#include <fstream>
#include <filesystem>

void writeLog(std::string message) {
	std::ofstream stream;

	stream.open(std::string(std::filesystem::current_path().string() + "\\SAMPSkins.log").c_str(), std::fstream::app);

	stream << message << "\n";

	stream.close();
}

void createLog() {
	std::ofstream stream;

	stream.open(std::string(std::filesystem::current_path().string() + "\\SAMPSkins.log").c_str(), std::ofstream::out | std::ofstream::trunc);

	stream << "";

	stream.close();
}
