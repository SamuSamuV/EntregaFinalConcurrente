/// @copyright Copyright (c) 2026 Ángel, All rights reserved.
/// angel.rodriguez@udit.es

#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

namespace argb {

    /**
     * Clase ThreadPool: Implementa un pool de hilos de ejecucion para procesar tareas de forma concurrente.
     * Soporta tanto tareas generales (cualquier hilo puede cogerlas) como tareas exclusivas (solo un hilo
     * concreto las puede procesar, vital para evitar condiciones de carrera en la Máquina Virtual de Lua).
     */
    class ThreadPool {
    public:
        ThreadPool(size_t threads) : stop(false) {
            exclusive_tasks.resize(threads);

            // Inicializamos los trabajadores (hilos)
            for (size_t i = 0; i < threads; ++i) {
                workers.emplace_back([this, i] {
                    for (;;) {
                        std::function<void()> task;
                        {
                            // Bloqueamos el mutex de la cola de tareas para evitar lecturas/escrituras simultáneas
                            std::unique_lock<std::mutex> lock(this->queue_mutex);

                            // El hilo se duerme hasta que haya que parar, haya una tarea exclusiva para el, o haya una tarea general
                            this->condition.wait(lock, [this, i] {
                                return this->stop || !this->exclusive_tasks[i].empty() || !this->general_tasks.empty();
                                });

                            if (this->stop && this->exclusive_tasks[i].empty() && this->general_tasks.empty()) return;

                            // Priorizamos las tareas exclusivas de este hilo (Ej: Peticiones asincronas de Lua en el Hilo 0)
                            if (!this->exclusive_tasks[i].empty()) {
                                task = std::move(this->exclusive_tasks[i].front());
                                this->exclusive_tasks[i].pop();
                            }

                            // Si no hay exclusivas para el, ayuda procesando las peticiones generales (Ej: Archivos estáticos nativos)
                            else if (!this->general_tasks.empty()) {
                                task = std::move(this->general_tasks.front());
                                this->general_tasks.pop();
                            }
                        }
                        // Ejecutamos la tarea fuera del lock para no bloquear el pool entero
                        if (task) task();
                    }
                    });
            }
        }

        ~ThreadPool() {
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                stop = true;
            }
            // Despertamos a todos los hilos para que terminen y hacemos join
            condition.notify_all();
            for (std::thread& worker : workers) worker.join();
        }

        /**
         * Encola una tarea general que podra ser procesada por cualquier hilo libre.
         * Devuelve un std::future para poder comprobar el estado de la tarea de forma asíncrona.
         */
        template<class F, class... Args>
        auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>> {
            using return_type = typename std::invoke_result_t<F, Args...>;
            auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
            std::future<return_type> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");
                general_tasks.emplace([task]() { (*task)(); });
            }
            condition.notify_one();
            return res;
        }

        /**
         * Encola una tarea exclusiva para que solamente la procese el hilo con el indice indicado.
         * Ideal para sistemas que no son thread-safe por defecto, como el estado de Lua.
         */
        template<class F, class... Args>
        auto enqueue_exclusive(size_t thread_index, F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>> {
            using return_type = typename std::invoke_result_t<F, Args...>;
            auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
            std::future<return_type> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");
                exclusive_tasks[thread_index].emplace([task]() { (*task)(); });
            }
            condition.notify_all();
            return res;
        }

    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> general_tasks;
        std::vector<std::queue<std::function<void()>>> exclusive_tasks;
        std::mutex queue_mutex;
        std::condition_variable condition;
        bool stop;
    };
}