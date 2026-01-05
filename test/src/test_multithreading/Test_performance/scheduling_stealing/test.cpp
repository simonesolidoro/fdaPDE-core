#include <fdaPDE/multithreading.h>

int main(int argc, char** argv) {
    int n_thread = std::stoi(argv[1]);
    int runs = 2;

    // StealingStrategy structs:  max_load_stealing, random_stealing, top_half_random_stealing
    // SchedulingStrategy structs: round_robin_scheduling, least_loaded_scheduling

    { //================= Test threadpool scheduling-stealing ==============================
        auto start = std::chrono::high_resolution_clock::now();
        auto end   = std::chrono::high_resolution_clock::now();

        std::vector<std::vector<std::chrono::microseconds>> tempi_send_steal(6); // 2x3
        int n_job = 4096; 
        int n_op_per_job = 15000;

        std::vector<std::future<void>> futs;

        for (int run = 0; run < runs; run++) {

            { // least_loaded + max_load_stealing 0
                fdapde::threadpool<fdapde::least_loaded_scheduling, fdapde::max_load_stealing> tp(4096, n_thread);
                start = std::chrono::high_resolution_clock::now();

                for (int i = 0; i < n_job; i++) {
                    futs.push_back(tp.send([=](int i) mutable {
                        if (i % 2 == 0) { // job pari sono il doppio
                            n_op_per_job *= 4;
                        }
                        int a = 0;
                        for (int j = 0; j < n_op_per_job; j++) { a++; }
                    }, i));
                }

                for (int i = 0; i < n_job; i++) { futs[i].get(); }
                end = std::chrono::high_resolution_clock::now();

                tempi_send_steal[0].push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start));
                futs.clear();
            }

            { // least_loaded + random_stealing 1
                fdapde::threadpool<fdapde::least_loaded_scheduling, fdapde::random_stealing> tp(4096, n_thread);
                start = std::chrono::high_resolution_clock::now();

                for (int i = 0; i < n_job; i++) {
                    futs.push_back(tp.send([=](int i) mutable {
                        if (i % 2 == 0) { // job pari sono il doppio
                            n_op_per_job *= 4;
                        }
                        int a = 0;
                        for (int j = 0; j < n_op_per_job; j++) { a++; }
                    }, i));
                }

                for (int i = 0; i < n_job; i++) { futs[i].get(); }
                end = std::chrono::high_resolution_clock::now();

                tempi_send_steal[1].push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start));
                futs.clear();
            }

            { // least_loaded + top_half_random_stealing 2
                fdapde::threadpool<fdapde::least_loaded_scheduling, fdapde::top_half_random_stealing> tp(4096, n_thread);
                start = std::chrono::high_resolution_clock::now();

                for (int i = 0; i < n_job; i++) {
                    futs.push_back(tp.send([=](int i) mutable {
                        if (i % 2 == 0) { // job pari sono il doppio
                            n_op_per_job *= 4;
                        }
                        int a = 0;
                        for (int j = 0; j < n_op_per_job; j++) { a++; }
                    }, i));
                }

                for (int i = 0; i < n_job; i++) { futs[i].get(); }
                end = std::chrono::high_resolution_clock::now();

                tempi_send_steal[2].push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start));
                futs.clear();
            }

            { // round_robin + max_load_stealing 3
                fdapde::threadpool<fdapde::round_robin_scheduling, fdapde::max_load_stealing> tp(4096, n_thread);
                start = std::chrono::high_resolution_clock::now();

                for (int i = 0; i < n_job; i++) {
                    futs.push_back(tp.send([=](int i) mutable {
                        if (i % 2 == 0) { // job pari sono il doppio
                            n_op_per_job *= 4;
                        }
                        int a = 0;
                        for (int j = 0; j < n_op_per_job; j++) { a++; }
                    }, i));
                }

                for (int i = 0; i < n_job; i++) { futs[i].get(); }
                end = std::chrono::high_resolution_clock::now();

                tempi_send_steal[3].push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start));
                futs.clear();
            }

            { // round_robin + random_stealing 4
                fdapde::threadpool<fdapde::round_robin_scheduling, fdapde::random_stealing> tp(4096, n_thread);
                start = std::chrono::high_resolution_clock::now();

                for (int i = 0; i < n_job; i++) {
                    futs.push_back(tp.send([=](int i) mutable {
                        if (i % 2 == 0) { // job pari sono il doppio
                            n_op_per_job *= 4;
                        }
                        int a = 0;
                        for (int j = 0; j < n_op_per_job; j++) { a++; }
                    }, i));
                }

                for (int i = 0; i < n_job; i++) { futs[i].get(); }
                end = std::chrono::high_resolution_clock::now();

                tempi_send_steal[4].push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start));
                futs.clear();
            }

            { // round_robin + top_half_random_stealing 5
                fdapde::threadpool<fdapde::round_robin_scheduling, fdapde::top_half_random_stealing> tp(4096, n_thread);
                start = std::chrono::high_resolution_clock::now();

                for (int i = 0; i < n_job; i++) {
                    futs.push_back(tp.send([=](int i) mutable {
                        if (i % 2 == 0) { // job pari sono il doppio
                            n_op_per_job *= 4;
                        }
                        int a = 0;
                        for (int j = 0; j < n_op_per_job; j++) { a++; }
                    }, i));
                }

                for (int i = 0; i < n_job; i++) { futs[i].get(); }
                end = std::chrono::high_resolution_clock::now();

                tempi_send_steal[5].push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start));
                futs.clear();
            }
        }

        std::vector<std::string> tag_coppie_send_steal = {
            "least_loaded-max_load",
            "least_loaded-random",
            "least_loaded-top_half_random",
            "round_robin-max_load",
            "round_robin-random",
            "round_robin-top_half_random"
        };

        for (int i = 0; i < 6; i++) {
            std::cout << tag_coppie_send_steal[i] << " thread_" << n_thread << " ";
            for (auto& t : tempi_send_steal[i]) { std::cout << t.count() << " "; }
            std::cout << std::endl;
        }
    }

    return 0;
}
