# process in chrome

## overview

[chrome process](https://www.chromium.org/developers/design-documents/multi-process-architecture)

chrome作为一个巨大的工程，每个模块，每个功能都要力求设计的安全，易用，最为一个拥有大量代码的软件，如果在设计时不去考虑它的模块划分，那么最后得到的代码是十分难以维护的，chrome在高层设计上考虑了所有软件都要考虑的问题：

* 使用什么样的进程模型
* 使用什么样的线程模型

首先来看看它采用了什么样的进程模型。

## 进程模型

> Modern operating systems are more robust because they put applications into separate processes that are walled off from one another. A crash in one application generally does not impair other applications or the integrity of the operating system, and each user's access to other users' data is restricted.

我们知道浏览器一般是由展示模块和后台渲染模块两个部分组成，一般而言，这两个模块都是合在一起组成了浏览器的主要功能部分，但是chrome在设计时就意识到，渲染引擎是很难不崩溃的，这就导致了一旦我们访问新的网页造成了渲染崩溃，那么我们之前浏览的所有网页都会受到影响，这也是我们在之前浏览器里经常遇到的问题，所以chrome想到了为什么不降两者分离呢，前台只用来展示，后台渲染自己走自己的，利用操作系统进程隔离的特性即使渲染崩溃也不会影响到我之前的页面。

很多人会说UI层也会崩溃啊，这样的分离也解决不了这个问题，事实确实如此，但是一个侧重点就是渲染崩溃的话UI层是可以继续展示等待渲染的回复，因为“用户”可以继续“浏览”，而UI层崩溃的话，用户失去了交互窗口，我们的程序肯定是要退出的。

这就是chrome现在使用的进程模型：

![process](https://github.com/llluiop/Chrome-Source-Analyze/raw/master/source/img/chrome-mulit-process)

多进程设计自然也有它的坏处，那就是交互，这也体现了软件设计里没有银弹的思想，想象一下oop编程里为什么要强调对象职责的单一，而实际编程中为什么容易出现巨类，说白了就是职责单一会造成通信的复杂，这和多进程或者多线程遇到的问题都是一样的，想想多线程的锁为何要存在。。。

在上述的架构里，浏览器形成了一个browser对应多个renderer的最终模块架构，browser作为老大，管理者所有的后台渲染进程和前台的显示工作，两者之间的交互使用的[Chromium's IPC system](https://www.chromium.org/developers/design-documents/inter-process-communication)，这个我们后面再说。

## 进程职责

可以看到一个renderer只有一个renderprocess对象，但是里面是可以含有多个renderview对象的，这是因为chrome会允许你使用一个renderer来渲染一个以上的网页窗口，每一个renderview都严格对应了了browser里的一个renderviewhost，后者将负责网页真正的显示工作，所有的消息流动都是在renderprocess和renderprocesshost之间进行的，他们两个是各自模块IPC信息的入口，也就是一个facade


## 其它特性

chrome为了保证安全性，使用了一些特性使得render进程运行在一个沙箱里，这就组织了一些潜在的危险行为，同时为了照顾低内存的用户(因为它是多进程的架构啊，当然耗内存。。。)，chrome参考了windows的内存页置换规则，可以释放一些暂时不浏览的页面占用的内存，这就是为什么有时我们切换回一个页面时整个页面内容都不见了的原因，edge浏览器似乎也实现了这个特性，在win10上有明显的感觉。

## 结束

关于进程架构就这么多了，后面我们会继续探索相关的代码实现。

