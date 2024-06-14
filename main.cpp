#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <memory>
#include "trie completo.h"

// Cabecera tiene un tamaño, dicho tamaño se usa para caminar por el archivo. Se tiene que poner toda la estructura del nodo del arbol b en ella.
// sizeof(cabecera) * offset siendo offset el numero de posicion de lo que quieras encontrar.
// https://aprendeaprogramar.com/cursos/verApartado.php?id=16801
// https://pastebin.com/A450dNmE
// Tabla hash encadenada tal que pueda localizar el offset

struct BinHeader
{
    uint64_t num_registros;
    BinHeader(){};
    BinHeader(long num_registros) : num_registros(num_registros) {}
};

struct RegistroBin
{
    std::string dni;
    std::string datos;
    // Constructor para inicializar los atributos de la estructura
    RegistroBin(const std::string &dni, const std::string &datos) : dni(dni), datos(datos) {}
    RegistroBin() = default;
    ~RegistroBin(){};
};

struct BinPosHeader
{
    uint64_t num_registros;
    BinPosHeader(){};
    BinPosHeader(long num_registros) : num_registros(num_registros) {}
};

struct RegistroPos
{
    long offset;
    std::string dni;
    // Constructor para inicializar los atributos de la estructura
    RegistroPos(std::streampos offset, const std::string &dni) : offset(offset), dni(dni) {}
    RegistroPos() = default;
    ~RegistroPos(){};
};

void escribirArchivosBinario(const std::string &filename)
{
    std::string binname = filename.substr(0, filename.find_last_of('.')) + ".bin";        // Archivo binario con los registros
    std::string posbinname = filename.substr(0, filename.find_last_of('.')) + "_pos.bin"; //  Archivo binario con las posición de los registros
    if (!std::filesystem::exists(binname))
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            std::cerr << "Error: file not found" << std::endl;
            return;
        }

        std::ofstream binFile(binname, std::ios::binary);
        std::ofstream posBinFile(posbinname, std::ios::binary);
        if (!binFile.is_open() || !posBinFile.is_open())
        {
            std::cerr << "Error: could not create binary file" << std::endl;
            return;
        }

        std::string line;
        std::getline(file, line); // Saltamos la primera linea del csv

        BinHeader *cabeceraBin = new BinHeader(0);
        BinPosHeader *cabeceraPosBin = new BinPosHeader(0);
        while (std::getline(file, line))
        {
            std::string dni = line.substr(0, line.find(','));
            long currentPos = binFile.tellp();

            std::unique_ptr<RegistroPos> regPos = std::make_unique<RegistroPos>(currentPos, dni); // Uso de smart pointers para eliminar el RegistroPos al salir del scope
            posBinFile.write(reinterpret_cast<const char *>(&regPos), sizeof(RegistroPos));
            std::unique_ptr<RegistroBin> reg = std::make_unique<RegistroBin>(dni, line); // Uso de smart pointers para eliminar el RegistroBin al salir del scope
            binFile.write(reinterpret_cast<const char *>(&reg), sizeof(RegistroBin));

            cabeceraBin->num_registros++;
            cabeceraPosBin->num_registros++;
        }
        // Volvemos al inicio del archivo para escribir la cabecera
        binFile.seekp(0, binFile.beg);
        binFile.write(reinterpret_cast<const char *>(&cabeceraBin), sizeof(cabeceraBin));
        posBinFile.seekp(0, posBinFile.beg);
        posBinFile.write(reinterpret_cast<const char *>(&cabeceraPosBin), sizeof(cabeceraPosBin));
        binFile.close();
        posBinFile.close();
        delete cabeceraPosBin;
        delete cabeceraBin;
    }
    return;
}

// Estrategia para la busqueda de un registro
// 1. Se lee la cabecera del archivo de posiciones
// 2. Se lee la cabecera del archivo de registros
// 3. Se busca el dni en el archivo de posiciones
// 4. Se lee el registro en la posición encontrada
// 5. Se muestra el registro

void buscarRegistro(const std::string &filename, const std::string &dni)
{
    std::string binname = filename.substr(0, filename.find_last_of('.')) + ".bin";        // Archivo binario con los registros
    std::string posbinname = filename.substr(0, filename.find_last_of('.')) + "_pos.bin"; //  Archivo binario con las posición de los registros
    if (!std::filesystem::exists(binname) || !std::filesystem::exists(posbinname))
    {
        std::cerr << "Error: binary files not found" << std::endl;
        return;
    }

    std::ifstream binFile(binname, std::ios::binary);
    std::ifstream posBinFile(posbinname, std::ios::binary);
    if (!binFile.is_open() || !posBinFile.is_open())
    {
        std::cerr << "Error: could not open binary files" << std::endl;
        return;
    }

    // Lectura de cabeceras
    BinHeader cabeceraBin;
    BinPosHeader cabeceraPosBin;
    posBinFile.read(reinterpret_cast<char *>(&cabeceraPosBin), sizeof(BinPosHeader));
    binFile.read(reinterpret_cast<char *>(&cabeceraBin), sizeof(BinHeader));
    std::cout << "Numero de registros: " << cabeceraBin.num_registros << std::endl;
    std::cout << "Numero de registros de posicion: " << cabeceraPosBin.num_registros << std::endl;

    // Estrategia de busqueda
    // 1. Se lee la cabecera del archivo de posiciones
    // 2. Se lee la cabecera del archivo de registros
    // 3. Se crea un buffer para almacenar una cantidad de registros de posicion
    // 4. Se coloca cada registro de posicion en el buffer
    // 5. Se coloca cada registro de posicion en el buffer en un arbol trie
    // 6. Se busca el dni en el arbol trie
    // 7. Si es que se encuentra, se lee la posicion y muestra el registro en la posición encontrada
    // 8. Si no se encuentra, se vacia el buffer y se vuelve a rellenar con la siguiente cantidad de registros de posicion
    // 9. Se repite el proceso hasta que se encuentre el dni o se haya recorrido todos los registros de posicion, cuyo caso devuelve un mensaje de que no fue encontrado el dni

    // Lectura de registros de posicion
    // Tamaño del buffer en bytes
    size_t buffer_size = 1500;

    // Lectura de registros de posicion
    size_t registrosLeidos = 0;
    Trie *trie = new Trie();
    while (registrosLeidos < cabeceraPosBin.num_registros)
    {
        size_t regsFaltantes = cabeceraPosBin.num_registros - registrosLeidos;
        size_t regsALeer = std::min(buffer_size / sizeof(RegistroPos), regsFaltantes);

        std::vector<RegistroPos> buffer(regsALeer);
        posBinFile.read(reinterpret_cast<char *>(buffer.data()), regsALeer * sizeof(RegistroPos));

        // Se coloca cada registro de posicion en el buffer en un arbol trie
        for (const auto &reg : buffer)
        {
            trie->insert(reg.dni, reg.offset);
        }

        registrosLeidos += regsALeer; // Se actualiza el numero de registros leidos
        // Se busca el dni en el arbol trie solamente cuando se haya llenado el buffer
        if (registrosLeidos % buffer_size == 0)
        {
            std::streampos offset = trie->search(dni);
            if (offset != -1)
            {
                // Se lee el registro en la posición encontrada
                binFile.seekg(offset);
                RegistroBin reg;
                binFile.read(reinterpret_cast<char *>(&reg), sizeof(RegistroBin));
                std::cout << "Registro encontrado: " << reg.dni << " " << reg.datos << std::endl;
                delete trie;
                binFile.close();
                posBinFile.close();
                return;
            }
            delete trie;
        }
    }
    binFile.close();
    posBinFile.close();
    return;
}

int main()
{
    std::cout << "Ingrese nombre y extension del archivo a leer: ";
    std::string filename;
    std::cin >> filename;

    escribirArchivosBinario(filename);

    int n;
    do
    {
        std::cout << "Ingrese una opcion: " << std::endl;
        std::cout << "1. Buscar registro" << std::endl;
        std::cout << "2. Agregar registro" << std::endl;
        std::cout << "3. Eliminar registro" << std::endl;
        std::cout << "0. Salir" << std::endl;
        std::cin >> n;
    } while (n < 0 || n > 3);

    switch (n)
    {
    case 1:
    {
        std::string dni;
        std::cout << "Ingrese el dni a buscar: ";
        std::cin >> dni;
        buscarRegistro(filename, dni);
        break;
    }
    case 2:
    {
        // Agregar registro
        break;
    }
    case 3:
    {
        // Eliminar registro
        break;
    }
    case 0:
    {
        break;
    }
    default:
        break;
    }

    return 0;
}
