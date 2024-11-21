#include <iostream>         // Para entrada y salida estándar
#include <fstream>          // Para manejo de archivos
#include <vector>           // Para el uso de vectores dinámicos
#include <list>             // Para implementar listas enlazadas (chaining en la tabla hash)
#include <unordered_map>    // Para implementar la tabla hash
#include <unordered_set>    // Para utilizar std::unordered_set
#include <queue>            // Para implementar colas (FIFO)
#include <algorithm>        // Para funciones como find()
#include <cstring>          // Para manejo de cadenas de caracteres C
#include <cstdlib>          // Para funciones estándar como atoi()
#include <map>              // Para el algoritmo Óptimo

using namespace std;

// Estructura para representar una entrada en la tabla de páginas
struct PageTableEntry {
    int page_number;    // Número de página virtual
    bool valid;         // Bit de validez
    // Se pueden agregar más campos si es necesario (por ejemplo, bits de uso para LRU Reloj)
};

// Clase para implementar la tabla de páginas como una tabla hash con chaining
class PageTable {
private:
    int table_size;    // Tamaño de la tabla hash
    vector<list<PageTableEntry>> table;  // Vector de listas para implementar chaining

public:
    // Constructor
    PageTable(int size) : table_size(size), table(size) {}

    // Función hash simple
    int hashFunction(int page_number) {
        return page_number % table_size;
    }

    // Insertar una entrada en la tabla de páginas
    void insert(int page_number) {
        int index = hashFunction(page_number);
        // Verificar si ya existe
        for (auto& entry : table[index]) {
            if (entry.page_number == page_number) {
                entry.valid = true;
                return;
            }
        }
        // Si no existe, agregar nueva entrada
        PageTableEntry new_entry = {page_number, true};
        table[index].push_back(new_entry);
    }

    // Eliminar una entrada de la tabla de páginas
    void remove(int page_number) {
        int index = hashFunction(page_number);
        table[index].remove_if([page_number](const PageTableEntry& entry) {
            return entry.page_number == page_number;
        });
    }

    // Verificar si una página está en la tabla y es válida
    bool isValid(int page_number) {
        int index = hashFunction(page_number);
        for (auto& entry : table[index]) {
            if (entry.page_number == page_number && entry.valid) {
                return true;
            }
        }
        return false;
    }
};

// Función para leer las referencias de página desde un archivo
vector<int> readReferences(const string& filename) {
    vector<int> references;
    ifstream infile(filename);
    if (!infile.is_open()) {
        cerr << "No se pudo abrir el archivo de referencias." << endl;
        exit(1);
    }

    int page_number;
    while (infile >> page_number) {
        references.push_back(page_number);
    }
    infile.close();
    return references;
}

// Función para simular el algoritmo Óptimo
int simulateOptimal(const vector<int>& references, int num_frames) {
    int page_faults = 0;
    vector<int> frames;

    for (size_t i = 0; i < references.size(); ++i) {
        int page = references[i];
        // Si la página ya está en los marcos, no hay fallo
        if (find(frames.begin(), frames.end(), page) != frames.end()) {
            continue;
        }

        // Si hay espacio libre, agregar la página
        if (frames.size() < num_frames) {
            frames.push_back(page);
        } else {
            // Reemplazar la página que no se usará por más tiempo
            int farthest = i;
            int index_to_replace = -1;
            for (int j = 0; j < frames.size(); ++j) {
                int k;
                for (k = i + 1; k < references.size(); ++k) {
                    if (frames[j] == references[k]) {
                        if (k > farthest) {
                            farthest = k;
                            index_to_replace = j;
                        }
                        break;
                    }
                }
                // Si la página no se usará de nuevo
                if (k == references.size()) {
                    index_to_replace = j;
                    break;
                }
            }
            if (index_to_replace == -1) {
                index_to_replace = 0;
            }
            frames[index_to_replace] = page;
        }
        page_faults++;
    }
    return page_faults;
}

// Función para simular el algoritmo FIFO
int simulateFIFO(const vector<int>& references, int num_frames) {
    int page_faults = 0;
    queue<int> frames;
    std::unordered_set<int> pages_in_memory;  // Usar std::unordered_set

    for (int page : references) {
        // Si la página ya está en los marcos, no hay fallo
        if (pages_in_memory.find(page) != pages_in_memory.end()) {
            continue;
        }

        // Si hay espacio libre, agregar la página
        if (frames.size() < num_frames) {
            frames.push(page);
            pages_in_memory.insert(page);
        } else {
            // Reemplazar la página más antigua
            int page_to_remove = frames.front();
            frames.pop();
            pages_in_memory.erase(page_to_remove);

            frames.push(page);
            pages_in_memory.insert(page);
        }
        page_faults++;
    }
    return page_faults;
}

// Función para simular el algoritmo LRU
int simulateLRU(const vector<int>& references, int num_frames) {
    int page_faults = 0;
    list<int> frames;
    std::unordered_set<int> pages_in_memory;  // Usar std::unordered_set

    for (int page : references) {
        // Si la página ya está en los marcos
        if (pages_in_memory.find(page) != pages_in_memory.end()) {
            // Mover la página al frente (más recientemente usada)
            frames.remove(page);
            frames.push_front(page);
            continue;
        }

        // Si hay espacio libre
        if (frames.size() < num_frames) {
            frames.push_front(page);
            pages_in_memory.insert(page);
        } else {
            // Reemplazar la página menos recientemente usada (al final de la lista)
            int lru_page = frames.back();
            frames.pop_back();
            pages_in_memory.erase(lru_page);

            frames.push_front(page);
            pages_in_memory.insert(page);
        }
        page_faults++;
    }
    return page_faults;
}

// Estructura para el algoritmo Reloj (Clock)
struct ClockEntry {
    int page_number;
    bool use_bit;
};

// Función para simular el algoritmo LRU Reloj simple
int simulateClock(const vector<int>& references, int num_frames) {
    int page_faults = 0;
    vector<ClockEntry> frames;
    int hand = 0;  // Apuntador del reloj

    // Inicializar los marcos vacíos
    for (int i = 0; i < num_frames; ++i) {
        frames.push_back({-1, false});  // -1 indica marco vacío
    }

    for (int page : references) {
        bool page_in_memory = false;
        // Verificar si la página ya está en los marcos
        for (auto& entry : frames) {
            if (entry.page_number == page) {
                entry.use_bit = true;  // Marcar como usada
                page_in_memory = true;
                break;
            }
        }
        if (page_in_memory) {
            continue;  // No hay fallo de página
        }

        // Reemplazar páginas usando el algoritmo del reloj
        while (true) {
            if (frames[hand].use_bit == false) {
                // Reemplazar esta página
                frames[hand].page_number = page;
                frames[hand].use_bit = true;
                hand = (hand + 1) % num_frames;
                break;
            } else {
                // Dar una segunda oportunidad
                frames[hand].use_bit = false;
                hand = (hand + 1) % num_frames;
            }
        }
        page_faults++;
    }
    return page_faults;
}

int main(int argc, char* argv[]) {
    // Parámetros por defecto
    int num_frames = 3;
    string algorithm = "FIFO";
    string filename;

    // Parseo de argumentos
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            num_frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            algorithm = argv[++i];
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            filename = argv[++i];
        } else {
            cerr << "Parámetro desconocido o faltante: " << argv[i] << endl;
            return 1;
        }
    }

    // Verificar que se proporcionó el archivo de referencias
    if (filename.empty()) {
        cerr << "Debe proporcionar un archivo de referencias con el parámetro -f" << endl;
        return 1;
    }

    // Leer las referencias desde el archivo
    vector<int> references = readReferences(filename);

    // Crear la tabla de páginas (el tamaño de la tabla hash puede ser arbitrario)
    PageTable page_table(10);

    int page_faults = 0;

    // Simular según el algoritmo elegido
    if (algorithm == "FIFO") {
        page_faults = simulateFIFO(references, num_frames);
    } else if (algorithm == "LRU") {
        page_faults = simulateLRU(references, num_frames);
    } else if (algorithm == "OPTIMO" || algorithm == "OPT") {
        page_faults = simulateOptimal(references, num_frames);
    } else if (algorithm == "RELOJ" || algorithm == "CLOCK") {
        page_faults = simulateClock(references, num_frames);
    } else {
        cerr << "Algoritmo desconocido: " << algorithm << endl;
        return 1;
    }

    // Imprimir el número de fallos de página
    cout << "Número de fallos de página: " << page_faults << endl;

    return 0;
}
