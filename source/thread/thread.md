# Thread in chrome

## Overview

[chrome thread](http://www.chromium.org/developers/design-documents/threading)

前面说过chrome是一个多进程的架构，这极大提高了chrome的安全和稳定性，每个进程各司其职，那么每个进程内部又是使用了怎样的设计理念呢？

* 高响应
* 避免琐

高响应意味着chrome的UI界面将保持非常高的响应，任何高费时的操作都不会在UI线程发生，包括I/O等，chrome将这些操作打包给其他线程来做，当处理完成以后再通知UI线程。
避免琐，按照上面我们所说，chrome必须实现一套线程模型来规范线程间的通信机制，而多线程并发里最重要的一项工作就是如何使用琐，chrome使用对象生命周期归属限定和回调机制来实现了一套完整的通信框架。

常规thread生命周期：

![thread](https://github.com/llluiop/Chrome-Source-Analyze/raw/master/source/img/chrome-normal-thread)

这是一个普通的线程生命周期，理想情况下，线程做自己的事情，不需要和别的线程打交道，也就不会产生同步的问题。

thread in chrome：

根据thread介绍文档，chrome的browser里使用了下面五类线程，并且开发人员认为此五类线程已经足够使用，不推荐再增加线程数：

* ui_thread: Main thread where the application starts up.
* io_thread: This thread is somewhat mis-named. It is the dispatcher thread that handles communication between the browser process and all the sub-processes. It is also where all resource requests (web page loads) are dispatched from (see Multi-process Architecture). 
* file_thread: A general process thread for file operations. When you want to do blocking filesystem operations (for example, requesting an icon for a file type, or writing downloaded files to disk), dispatch to this thread. 
* db_thread: A thread for database operations. For example, the cookie service does sqlite operations on this thread. Note that the history database doesn't use this thread yet. 
* safe_browsing_thread 


一些对于线程间交互使用的建议：

* UI的耗时操作会分发到IOthread去做，同时IO线程本身也使用 asynchronous/overlapped IO 技术防止自身阻塞在特定操作上
* 线程间一般来说只使用锁来交换特定的共享数据，也就是说一个线程不应该占用一个锁太久使得另一个线程堵塞在等待在这把锁上

![thread](https://github.com/llluiop/Chrome-Source-Analyze/raw/master/source/img/chrome-thread)

## diving into thread

对于thread的交互，现在我们需要讨论的就是：

* 线程对象自身是如果设计的，因为线程有着不同的职责，一个好的可以扩展的设计是非常有必要的
* 线程间是如何交互，对于上面的图来说就是我们是如何将一个请求打包发送到另一个线程的，这其中必然牵扯到了锁，回调通知等

### 线程对象的设计：

对于一个线程对象而言，上面我们介绍过对于常驻线程而言，我们需要维护一个内部的线程循环来不停的执行相关的操作，对于chrome thread自然的抽象了这个循环对象**MessageLoop**：

    thread.h
    
    class BASE_EXPORT Thread : PlatformThread::Delegate { 
        ....
        // The thread's message loop.  Valid only while the thread is alive.  Set
        // by the created thread.
        MessageLoop* message_loop_;
        
        struct BASE_EXPORT Options {
            // Specifies the type of message loop that will be allocated on the thread.
            // This is ignored if message_pump_factory.is_null() is false.
            MessageLoop::Type message_loop_type;
        };
    };
    
    外部代码在创建thread对象的时候，会传入一个Options对象，用于指明创建何种类型的MessageLoop
    
那么**MessageLoop**分为几种，又为何划的呢：

* TYPE_DEFAULT This type of ML only supports tasks and timers.
* TYPE_UI  This type of ML also supports native UI events (e.g., Windows messages).
* TYPE_IO  This type of ML also supports asynchronous IO

可以看到，按照一般的用法，大致有三种类型的线程，用于执行一般的操作，UI操作和IO操作，对于上面我们说到的UI线程，自然使用的TYPE_UI，IO线程使用的是TYPE_IO，在下面的代码中可以看到相关的线程创建：

    browser_main_loop.cc
    
    int BrowserMainLoop::CreateThreads(){
        base::Thread::Options io_message_loop_options;
        io_message_loop_options.message_loop_type = base::MessageLoop::TYPE_IO;
        base::Thread::Options ui_message_loop_options;
        ui_message_loop_options.message_loop_type = base::MessageLoop::TYPE_UI;
        
        ......
    }
    
### 线程间的交互：
    
我们知道，区别线程对象的目的是为了明确线程的职责，比如UI线程在win平台肯定要处理windows消息，IO线程则要使用层叠IO技术进行IO访问等等，至于不同的ML有什么具体的不同，网上文章很多，这里就不再赘述了。
有了ML以后，我们还需要一些其他的类与ML协同工作，用于处理一些平台特定操作，内部交换队列(用于与其他线程通信)，外部通信接口(用于接收外部请求,这就是线程间交互的方式)，这些分别对应了**MessagePump, IncomingTaskQueue and MessageLoopProxyImpl**：

![thread](https://github.com/llluiop/Chrome-Source-Analyze/raw/master/source/img/chrome-messageloop)

在ML的实现文件可以看到相关的依赖：

    class BASE_EXPORT MessageLoop : public MessagePump::Delegate {
        
        scoped_refptr<SingleThreadTaskRunner> task_runner() {
            return message_loop_proxy_;
        }
        
        scoped_ptr<MessagePump> pump_;        
        scoped_refptr<internal::IncomingTaskQueue> incoming_task_queue_;
    }

下面分别介绍下这三种：

* MessagePump
* IncomingTaskQueue
* MessageLoopProxyImpl

MessagePump：
MP一般和ML是成对出现，用于处理一些本地事件，例如windows消息，IO消息，我们知道对于一个MP而言，它应该可以驱动所有的消息的，所以它的设计即要可以驱动自己的业务，也需要驱动ML里的业务，所以它的实现：

    //   for (;;) {
    //     bool did_work = DoInternalWork();  //驱动自己逻辑
    //     if (should_quit_)
    //       break;
    //
    //     did_work |= delegate_->DoWork();   //注意，这里的delegate就是MessageLoop，这样可以将ML的逻辑和MP的逻辑同一起来
    //     if (should_quit_)
    //       break;
    //
    //     TimeTicks next_time;
    //     did_work |= delegate_->DoDelayedWork(&next_time);
    //     if (should_quit_)
    //       break;
    //
    //     if (did_work)
    //       continue;
    //
    //     did_work = delegate_->DoIdleWork();
    //     if (should_quit_)
    //       break;
    //
    //     if (did_work)
    //       continue;
    //
    //     WaitForWork();
    //   }
    
这样，在一次线程循环里，MP先处理自身类型的消息，例如window message，然后再检索对应的ML是否有相关的逻辑需要处理，最后等待新的消息到来。


IncomingTaskQueue：
IncomingTaskQueue内部维护了一个**TaskQueue incoming_queue_**队列用于接收外部的task，当一个线程像另一线程发送task时，将会把task放入到这个队列里，然后接收方在delegate_->DoWork()里会调用**ReloadWorkQueue()**，将**incoming_queue_**里的队列换入到当前线程的任务队列里，注意，这里也是基本线程间交互唯一有锁的地方，可以看到这个锁的粒度非常小。
详细的代码可以参阅：incoming_task_queue.h


MessageLoopProxyImpl:
具体而言，MessageLoopProxyImpl做的事情比较简单，主要是提供了一系列的PostTask接口，用于接收外部的task，然后放入到IncomingTaskQueue里：

![thread](https://github.com/llluiop/Chrome-Source-Analyze/raw/master/source/img/chrome-thread-interact)

至于如何将所有的请求都可以通过一个PostTask实现，这里也是有一些可以说的地方，我们下次再讲，总而言之，到了这里，我们基本上可以看到线程间是如果进程交互，协同工作的了。