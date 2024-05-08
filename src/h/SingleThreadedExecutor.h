#ifndef SINGLETHREADEDEXECUTOR_H
#define SINGLETHREADEDEXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

typedef std::function<void()> Task;
typedef std::function<void(int)> FinishedCallback;

class SingleThreadedExecutor {
   public:
      SingleThreadedExecutor(FinishedCallback callback);
      
      ~SingleThreadedExecutor();
      
      /**
       * Provides a task to the scheduler to get executed. As soon as
       * the task finished, the callback will get called with the provided
       * ID.
       */
      void execute(Task task, int id);

   private:
      struct NextTask {
         Task task;
         int  id;
      };
      
      void mainLoop();
      
      std::condition_variable   condition;
      std::mutex                mutex;
      std::thread               thread;
      std::unique_ptr<NextTask> nextTask;
      FinishedCallback          finishedCallback;
      bool                      quit;
};

#endif
