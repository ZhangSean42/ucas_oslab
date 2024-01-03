### 实现以下功能：

1.内核压缩

2.镜像紧缩

3.内核启动

### 具体流程：

1.BIOS将bootblock加载至内存后，跳转至bootblock运行

2.bootblock负责将解压程序载入内存，解压程序完成压缩内核的载入与解压

3.跳转至解压后内核的第一条代码处，内核启动成功