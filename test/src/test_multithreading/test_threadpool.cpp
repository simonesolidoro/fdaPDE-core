/*test multithreading giocattolo
void conta(){

}

class toy_worker {
    private:
        fdapde::Worker_queue<int> coda;
        std::thread t;
    public:
        toy_worker(int n):coda(n),t(conta,){

        };
};

class toy_threadpool {
    private:
        std::vector<toy_worker> W;
    pubblic:
        toy_threadpool(int n, int n_threads){
            for(int i=0; i<n_threads; i++){
                W.emplace_back(n)
            }
            
        }

};*/


/*
class toy_worker {
    private:
        fdapde::Worker_queue<int> coda;
    public:
        toy_worker(int n):coda(n){
            std::thread t(&toy_worker::run,this);
        };
        void run(){
            while(true){
                if(! coda.empty()){
                    int count= coda.pop_front();
                    std::cout<<"da thread: "<<std::this_thread::get_id()<<" int: "<<count<<std::endl;
                }
            }
        }
};

/*class toy_threadpool {
    private:
        std::vector<toy_worker> W;
    public:
        toy_threadpool(int n_coda, int n_thread){
            for (int i = 0; i< n_thread; i++){
                W.emplace_back(n_coda);
            }
        }

};*/
