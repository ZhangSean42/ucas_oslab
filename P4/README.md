### 实现以下功能：

1.页表建立，内核虚地址空间均采用2MiB大页映射，用户态则采用4KiB基本页映射

2.链式物理内存空间管理

3.跟踪物理页框使用情况的倒排页表

4.用户进程虚地址空间管理与按需调页

5.换页

6.基于共享内存方式的IPC实现

7.虚存机制下的线程实现

8.写时复制

### 具体流程与实现细节：

1.解压缩与内核页表建立代码是位置无关代码，即运行时程序计数器PC的值与链接器确定的虚地址不同。这意味着开启分页机制的指令提交后，操作系统正式运行在虚空间前，有一小段代码在运行时，PC仍然指向存放代码的内存物理地址（这段代码的功能就是通过跳转指令，使PC指向代码段虚地址）。但是分页机制开启后，处理器每次访存都需要查询页表。因此需要建立一个临时映射，防止内核缺页异常的发生。

2.换页机制发生时机选择在物理页框全部耗尽的同时出现用户态缺页的情况
