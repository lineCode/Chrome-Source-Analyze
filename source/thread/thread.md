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
