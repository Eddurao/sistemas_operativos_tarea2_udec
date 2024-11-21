#include <iostream>             // Para entrada y salida estándar
#include <thread>               // Para manejar hilos
#include <mutex>                // Para sincronización con mutex
#include <condition_variable>   // Para variables de condición
#include <vector>               // Para el uso de vectores dinámicos
#include <fstream>              // Para manejo de archivos
#include <chrono>               // Para manejo de tiempo
#include <queue>                // (No se usa en este código, pero puede ser útil)
#include <cstring>              // Para manejo de cadenas de caracteres C

using namespace std;

// Clase Monitor para manejar la cola circular de tamaño dinámico
class CircularQueueMonitor {
private:
    vector<int> buffer;           // Vector que actúa como la cola circular
    size_t capacity;              // Capacidad actual de la cola
    size_t front;                 // Índice del frente de la cola
    size_t rear;                  // Índice del final de la cola
    size_t count;                 // Número de elementos actuales en la cola

    mutex mtx;                    // Mutex para sincronizar el acceso a la cola
    condition_variable not_full;  // Variable de condición para cuando la cola no está llena
    condition_variable not_empty; // Variable de condición para cuando la cola no está vacía

    ofstream& log_file;           // Referencia al archivo de log para registrar los cambios

    // Función para redimensionar la cola
    void resize(size_t new_capacity) {
        vector<int> new_buffer(new_capacity); // Crear un nuevo buffer con la nueva capacidad

        // Copiar los elementos del buffer antiguo al nuevo
        for (size_t i = 0; i < count; ++i) {
            new_buffer[i] = buffer[(front + i) % capacity];
        }

        buffer = move(new_buffer); // Reemplazar el buffer antiguo por el nuevo
        capacity = new_capacity;   // Actualizar la capacidad
        front = 0;                 // Reiniciar el índice del frente
        rear = count % capacity;   // Actualizar el índice del final

        // Registrar en el log el cambio de tamaño
        log_file << "La cola cambió de tamaño a " << capacity << endl;
    }

public:
    bool producers_done = false; // Indica si los productores han terminado

    // Constructor de la clase
    CircularQueueMonitor(size_t init_capacity, ofstream& log_file_ref)
        : capacity(init_capacity), front(0), rear(0), count(0), buffer(init_capacity), log_file(log_file_ref) {}

    // Función para agregar un elemento a la cola
    void enqueue(int item) {
        unique_lock<mutex> lock(mtx); // Adquirir el mutex

        // Esperar mientras la cola esté llena
        not_full.wait(lock, [this]() { return count < capacity; });

        // Agregar el elemento al final de la cola
        buffer[rear] = item;
        rear = (rear + 1) % capacity; // Actualizar el índice del final
        ++count;                      // Incrementar el contador de elementos

        // Si la cola está llena después de agregar, duplicar su tamaño
        if (count == capacity) {
            resize(capacity * 2);
        }

        not_empty.notify_one(); // Notificar a un hilo que espera por elementos
    }

    // Función para extraer un elemento de la cola
    bool dequeue(int& item) {
        unique_lock<mutex> lock(mtx); // Adquirir el mutex

        // Esperar mientras la cola esté vacía y los productores no hayan terminado
        while (count == 0 && !producers_done) {
            not_empty.wait(lock);
        }

        // Si la cola sigue vacía y los productores han terminado, no hay más elementos
        if (count == 0 && producers_done) {
            return false;
        }

        // Extraer el elemento del frente de la cola
        item = buffer[front];
        front = (front + 1) % capacity; // Actualizar el índice del frente
        --count;                        // Decrementar el contador de elementos

        // Si el uso de la cola llega al 25% o menos, reducir su tamaño a la mitad
        if (capacity > 1 && count <= capacity / 4) {
            resize(capacity / 2);
        }

        not_full.notify_one(); // Notificar a un hilo que espera por espacio
        return true;
    }

    // Función para indicar que los productores han terminado
    void set_producers_done() {
        unique_lock<mutex> lock(mtx); // Adquirir el mutex
        producers_done = true;        // Marcar que los productores han terminado
        not_empty.notify_all();       // Notificar a todos los hilos que esperan por elementos
    }
};

// Función que ejecuta cada hilo productor
void producer_function(CircularQueueMonitor& queue_monitor, int producer_id, int items_to_produce) {
    for (int i = 0; i < items_to_produce; ++i) {
        queue_monitor.enqueue(i); // Agregar un item a la cola
        // Simular trabajo con una pausa
        this_thread::sleep_for(chrono::milliseconds(10));
    }
}

// Función que ejecuta cada hilo consumidor
void consumer_function(CircularQueueMonitor& queue_monitor, int consumer_id, int max_wait_time) {
    auto start_time = chrono::steady_clock::now(); // Tiempo de inicio
    int item;

    while (true) {
        if (queue_monitor.dequeue(item)) {
            // Procesar el item (aquí no hacemos nada específico)
            // Simular trabajo con una pausa
            this_thread::sleep_for(chrono::milliseconds(15));

            // Reiniciar el temporizador si se obtuvo un item
            start_time = chrono::steady_clock::now();
        } else {
            // Si no se pudo extraer un item y los productores han terminado
            if (queue_monitor.producers_done) {
                break; // Terminar el consumidor
            }

            // Verificar si ha excedido el tiempo máximo de espera
            auto current_time = chrono::steady_clock::now();
            auto elapsed_time = chrono::duration_cast<chrono::seconds>(current_time - start_time).count();

            if (elapsed_time >= max_wait_time) {
                break; // Terminar el consumidor por tiempo de espera
            }

            // Esperar un poco antes de intentar nuevamente
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }
}

int main(int argc, char* argv[]) {
    // Parámetros por defecto
    int num_producers = 10;            // Número de productores
    int num_consumers = 5;             // Número de consumidores
    size_t initial_queue_size = 50;    // Tamaño inicial de la cola
    int max_consumer_wait_time = 1;    // Tiempo máximo de espera de los consumidores

    // Parseo de argumentos de línea de comandos
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            num_producers = atoi(argv[++i]); // Número de productores
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            num_consumers = atoi(argv[++i]); // Número de consumidores
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            initial_queue_size = atoi(argv[++i]); // Tamaño inicial de la cola
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            max_consumer_wait_time = atoi(argv[++i]); // Tiempo máximo de espera
        } else {
            cerr << "Parámetro desconocido: " << argv[i] << endl;
            return 1;
        }
    }

    // Crear el archivo de log
    ofstream log_file("log.txt");
    if (!log_file.is_open()) {
        cerr << "No se pudo abrir el archivo log.txt para escribir." << endl;
        return 1;
    }

    // Crear el Monitor de la cola circular
    CircularQueueMonitor queue_monitor(initial_queue_size, log_file);

    // Vectores para almacenar los hilos productores y consumidores
    vector<thread> producers;
    vector<thread> consumers;

    int items_per_producer = 100; // Número de items que produce cada productor

    // Crear los hilos productores
    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back(producer_function, ref(queue_monitor), i, items_per_producer);
    }

    // Crear los hilos consumidores
    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back(consumer_function, ref(queue_monitor), i, max_consumer_wait_time);
    }

    // Esperar a que todos los productores terminen
    for (auto& prod : producers) {
        prod.join();
    }

    // Indicar al monitor que los productores han terminado
    queue_monitor.set_producers_done();

    // Esperar a que todos los consumidores terminen
    for (auto& cons : consumers) {
        cons.join();
    }

    // Cerrar el archivo de log
    log_file.close();

    return 0;
}
