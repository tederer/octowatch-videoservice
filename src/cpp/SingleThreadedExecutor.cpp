#include "SingleThreadedExecutor.h"

using namespace std::chrono_literals;


SingleThreadedExecutor::SingleThreadedExecutor(FinishedCallback callback) 
   : thread(std::bind(&SingleThreadedExecutor::mainLoop, this)), 
     finishedCallback(callback), quit(false) {}
      
SingleThreadedExecutor::~SingleThreadedExecutor() {
   quit = true;
   {
      std::unique_lock lk(mutex);
      condition.notify_all();
   }
   thread.join();
}
      
void SingleThreadedExecutor::execute(Task task, int id) {
   std::unique_lock lk(mutex);
   nextTask.reset(new NextTask{task, id});
   condition.notify_all();
}    

void SingleThreadedExecutor::mainLoop() {
   while(!quit) {
      std::unique_ptr<NextTask> task;
      {
         std::unique_lock lock(mutex);
         if (!nextTask) {
            condition.wait_for(lock, 500ms);
         }
         if (nextTask) {
            task = std::move(nextTask);
         }
      }
      if (task) {
         task->task();
         finishedCallback(task->id);
         task.reset();
      }
   }
}
