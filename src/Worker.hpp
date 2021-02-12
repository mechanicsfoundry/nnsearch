#pragma once

#include <algorithm>
#include <memory>
#include <queue>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

class Worker
{
private:
    std::atomic<bool> running;
    std::atomic<bool> executing;

    std::thread* thread_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::condition_variable condition_join_;
    std::function<void()> job;

public:
    Worker(std::function<void()> job) :
        job(std::move(job)),
        running(true),
        executing(false)
    {
        thread_ = new std::thread([=] {
            thread_loop();
        });
    }

    ~Worker()
    {
        Terminate();
    }

    void Terminate()
    {
        if (running)
        {
            running = false;
            condition_.notify_one();
            thread_->join();
            delete thread_;
        }
    }

    void Notify()
    {
        executing = true;
        condition_.notify_one();
    }

    void Join()
    {
        if (executing)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_join_.wait(lock, [this] {
                return !executing || !running;
            });
            executing = false;
        }
    }

private:
    void thread_loop()
    {
        do
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] {
                return executing || !running;
            });

            if (running && executing)
            {
                job();
            }

            condition_join_.notify_one();
            executing = false;
        }
        while (running);
    }
};

class WorkerGroup
{
private:
    std::vector<std::shared_ptr<Worker>> workers_;

public:
    WorkerGroup()
    {
    }

    ~WorkerGroup()
    {
        Terminate();
    }

    void AddWorker(std::function<void()> job)
    {
        workers_.push_back(std::make_shared<Worker>(job));
    }

    void Notify()
    {
        for (auto& w = workers_.begin(); w != workers_.end(); ++w)
        {
            (*w)->Notify();
        }
    }

    void Join()
    {
        for (auto& w = workers_.begin(); w != workers_.end(); ++w)
        {
            (*w)->Join();
        }
    }

    void Resolve()
    {
        Notify();
        Join();
    }

    void Terminate()
    {
        for (auto& w = workers_.begin(); w != workers_.end(); ++w)
        {
            (*w)->Terminate();
        }
    }
};