#include <iostream>
#include <fstream>
#include <string>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <vector>
#include <filesystem>
#include <random>

namespace fs = std::filesystem;

const char MZ_ENGINE_STR[16] = { 'M', 'Z', 'E', 'n', 'g', 'i', 'n', 'e', 'B', 'i', 'n', 'a', 'r', 'i', 'r', '\xA7' };

#pragma pack(1)

enum {
    MODE_DECOMPRESS,
    MODE_COMPRESS
} mode;

struct __attribute__ ((packed)) Header {
    char mz_engine_str[16];
    uint16_t count;
    uint32_t size;
} header;

struct __attribute__ ((packed)) File {
    uint32_t field1; // desconocido (siempre cero)
    uint8_t code;
    uint32_t field3; // desconocido
    char name[32]; // xor code xor 6C
    uint8_t field5; // desconocido
    uint16_t field6; // desconocido
    uint32_t size;
    uint32_t field8; // desconocido (siempre cero)
    uint32_t start;
    uint8_t rest[22];
};

std::string sanitize_filename(const std::string &str) {
    std::string result = str;
    for (auto &c : result) {
        if (c <= 0) {
            c = '-';
        }
    }
    return result;
}

std::string right_trim(const std::string &str) {
    int numEndSpaces = 0;
    for (int i = str.length() - 1; i >= 0; i--) {
        if (!isspace(str[i])) break;
        numEndSpaces++;
    }
    return str.substr(0, str.length() - numEndSpaces);
}

std::string encrypt(const std::string &name, uint8_t code) {
    std::string result = name;

    code = code ^ 0x6C;

    for (auto &c : result) {
        c = c ^ code;
    }

    return result;
}

void compress(const std::string &output_path, const std::string &input_path) {
    if (!fs::exists(input_path + "/index.txt")) {
        std::cout << "La carpeta a comprimir no tiene el archivo index.txt.\n";
        return;
    }
    
    std::ifstream index_file(input_path + "/index.txt");

    std::ofstream output(output_path, std::ios::binary);

    if (!output) {
        std::cout << "Acceso denegado al intentar guardar el archivo comprimido.\n";
        return;
    }

    std::cout << "Comprimiendo " << output_path << ":\n";

    Header header = {};
    std::memcpy(header.mz_engine_str, MZ_ENGINE_STR, sizeof(MZ_ENGINE_STR));

    std::vector<File> files;
    std::vector<std::string> input_names;

    std::random_device random;

    std::string line;

    while (std::getline(index_file, line)) {
        if (line == "[:space:]") {
            files.push_back({});
            input_names.push_back(line);
            continue;
        }

        auto separator = line.find(": ");

        std::string file_name = line.substr(0, separator);
        std::string orig_name = line.substr(separator + 2);

        File file = {};
        file.code = random();

        std::string pad_file_name(sizeof(file.name), ' ');
        pad_file_name.replace(0, orig_name.size(), orig_name);
        std::string encrypted_name = encrypt(pad_file_name, file.code);
        std::memcpy(file.name, encrypted_name.c_str(), sizeof(file.name));

        files.push_back(std::move(file));

        input_names.push_back(file_name);
    }

    std::cout << "Procesado el archivo index.txt. Copiando archivos:\n";

    output.seekp(sizeof(header) + files.size() * sizeof(File));

    char buffer[8192];

    for (size_t i = 0; i < files.size(); ++i) {
        const auto &file_name = input_names[i];
        
        if (file_name == "[:space:]") {
            output.seekp(sizeof(File), std::ios_base::cur);
            continue;
        }

        auto &file = files[i];

        file.start = output.tellp() + 1ll;

        std::ifstream input(input_path + '/' + file_name, std::ios::binary);

        if (!input) {
            std::cout << "\rNo se pudo abrir el archivo '" << file_name << "'. Puede causar errores en el juego. Continuando.\n" << std::flush;
            continue;
        }

        do {
            input.read(buffer, sizeof(buffer));

            file.size += input.gcount();

            output.write(buffer, input.gcount());
        }
        while (!input.eof());

        header.size += file.size;

        std::cout << '\r' << i * 100 / files.size() << "% - " << file_name << " (" << file.size << " bytes)\t\t\t\t\t" << std::flush;
    }

    header.count = files.size();
    header.size += 2; // no sé porque, pero siempre es 2 más que el tamaño real

    output.seekp(0);

    output.write(reinterpret_cast<char*>(&header), sizeof(header));

    output.write(reinterpret_cast<char*>(files.data()), sizeof(File) * files.size());

    std::cout << "\r100% - " << header.count << " archivos comprimidos\t\t\t\t\t\n";
}

void decompress(const std::string &input_path, const std::string &output_path) {
    if (!fs::exists(input_path)) {
        std::cout << "El archivo comprimido no existe.\n";
        return;
    }
    else if (fs::exists(output_path + "/index.txt")) {
        std::cout << "Debe borrar todos los archivos de la carpeta de salida.\n";
        return;
    }
    
    std::ifstream input(input_path, std::ios::binary);

    std::cout << "Descomprimiendo " << input_path << ":\n";

    input.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (std::string(header.mz_engine_str, sizeof(MZ_ENGINE_STR)) == MZ_ENGINE_STR) {
        std::cout << "El archivo comprimido no tiene el formato correcto.\n";
        return;
    }

    std::vector<File> files;

    files.reserve(header.count);

    input.read(reinterpret_cast<char*>(files.data()), sizeof(File) * header.count);

    fs::create_directory(output_path);

    std::ofstream index_file(output_path + "/index.txt");

    size_t saved_count = 0;

    for (size_t i = 0; i < header.count; ++i) {
        const auto &file = files[i];

        if (!file.size) {
            index_file << "[:space:]\n";
            continue;
        }

        std::vector<uint8_t> file_data;

        file_data.reserve(file.size);

        input.seekg(file.start - 1);

        input.read(reinterpret_cast<char*>(file_data.data()), file.size);

        std::string name = encrypt(std::string(file.name, sizeof(file.name)), file.code);

        name = right_trim(name);

        std::string output_name = sanitize_filename(name);

        if (output_name.size() == 0) {
            output_name = "no-name";
        }

        fs::path path(output_path + '/' + output_name);

        switch (file_data[0]) {
            case 0x42:
                path.replace_extension(".bmp");
                break;

            case 0x44:
                path.replace_extension(".dds");
                break;

            case 0x47:
                path.replace_extension(".gif");
                break;

            case 0x49:
                path.replace_extension(".mp3");
                break;

            case 0x52:
                path.replace_extension(".wav");
                break;

            case 0x54:
                path.replace_extension(".tga");
                break;

            case 0x89:
                path.replace_extension(".png");
                break;

            case 0xFF:
                path.replace_extension(".jpg");
                break;
            
            default:
                path.replace_extension(".unk");
        }

        size_t n = 1;
        while (fs::exists(path)) {
            path.replace_filename(path.stem().string() + '_' + std::to_string(n) + path.extension().string());
            ++n;
        }

        index_file << path.filename().string() << ": " << name << '\n';

        std::ofstream output(path, std::ios::binary);

        output.write(reinterpret_cast<char*>(file_data.data()), file.size);

        ++saved_count;

        std::cout << '\r' << i * 100 / header.count << "% - " << name << " (" << file.size << " bytes)\t\t\t\t\t" << std::flush;
    }

    std::cout << "\r100% - " << saved_count << " archivos guardados\t\t\t\t\t\n";
}

std::string lowercase(const std::string &str) {
    std::string result = str;

    for (auto &c : result)
        c = std::tolower(c);

    return result;
}

int main(int argc, char *argv[]) {
    mode = MODE_DECOMPRESS;

    if (argc > 1) {
        auto mode_str = lowercase(argv[1]);

        if (mode_str == "comprimir" || mode_str == "compress") {
            mode = MODE_COMPRESS;
        }
        else if(mode_str == "descomprimir" || mode_str == "decompress") {
            mode = MODE_DECOMPRESS;
        }
        else {
            std::cout << "Opción inválida." << std::endl;
            return 1;
        }
    }

    std::string compressed_file = "C:/Program Files (x86)/Tierras del Sur 2/Recursos/Graficos.TDS";
    std::string folder = "./out";

    if (argc > 2) {
        compressed_file = argv[2];
    }

    if (argc > 3) {
        folder = argv[3];
    }

    switch (mode) {
        case MODE_COMPRESS:
            compress(compressed_file, folder);
            break;

        case MODE_DECOMPRESS:
            decompress(compressed_file, folder);
    }
}