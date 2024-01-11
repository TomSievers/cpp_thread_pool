#include "thread_pool.hpp"
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

int main()
{
    thread_pool tpool;

    tpool.enqueue_with_cancellation([](std::shared_ptr<cancelation_token> token)
                                    { 
        std::cout << "Hello" << std::endl; 
        token->sleep_for(10s);
        std::cout << "Goodbye" << std::endl; });

    tpool.enqueue_with_cancellation([](std::shared_ptr<cancelation_token> token)
                                    { 
        for(int i = 0; i < 10; ++i)
        {
            std::cout << i << std::endl;
            token->sleep_for(500ms);
        } });

    tpool.enqueue_with_cancellation([](std::shared_ptr<cancelation_token> token)
                                    { 
        for(int i = 0; i < 20; ++i)
        {
            std::cout << i << std::endl;
            token->sleep_for(500ms);
        } });

    std::this_thread::sleep_for(200ms);

    return 0;
}
