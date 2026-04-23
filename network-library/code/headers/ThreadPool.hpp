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

    class ThreadPool {
    public:
        ThreadPool(size_t threads) : stop(false) {
            exclusive_tasks.resize(threads);
            for (size_t i = 0; i < threads; ++i) {
                workers.emplace_back([this, i] {
                    for (;;) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            // Despierta si hay que parar, si hay tarea exclusiva para MI hilo, o si hay tarea general
                            this->condition.wait(lock, [this, i] {
                                return this->stop || !this->exclusive_tasks[i].empty() || !this->general_tasks.empty();
                                });

                            if (this->stop && this->exclusive_tasks[i].empty() && this->general_tasks.empty()) return;

                            // Priorizamos las tareas exclusivas de este hilo (Ej: Hilo 0 coge peticiones de Lua)
                            if (!this->exclusive_tasks[i].empty()) {
                                task = std::move(this->exclusive_tasks[i].front());
                                this->exclusive_tasks[i].pop();
                            }
                            // Si no hay exclusivas, ayudamos con las peticiones generales (HTTP Nativo)
                            else if (!this->general_tasks.empty()) {
                                task = std::move(this->general_tasks.front());
                                this->general_tasks.pop();
                            }
                        }
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
            condition.notify_all();
            for (std::thread& worker : workers) worker.join();
        }

        // Encola manejadores HTTP Nativos (cualquier hilo puede cogerlo)
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

        // Encola tareas de Lua (solo el hilo especificado lo procesará, evitando condiciones de carrera en la VM)
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