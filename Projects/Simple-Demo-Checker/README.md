# README

这个文件夹模拟了0089-usb-Use-_nort-in-giveback-function.patch中的hcd.c中出bug的代码部分，仅用于demo测试。

## Usage

* 把my-checker/MyChecker.cpp复制到llvm的代码目录中的`/tools/clang/lib/StaticAnalyzer/Checkers/`中。

* 修改llvm的代码目录中的`/tools/clang/lib/StaticAnalyzer/Checkers/CMakeLists.txt`文件，增加下面一行：

  ```
    MyChecker.cpp
  ```

* 在llvm的代码目录中`/tools/clang/include/clang/StaticAnalyzer/Checkers`下的文件Checkers.td文件中，搜索`alpha.core`，在`alpha.core`的花括号中增加下面的代码：

  ```
  def MyChecker : Checker<"MyChecker">,
    HelpText<"My checker ---- LT">,
    DescFile<"MyChecker.cpp">;
  ```

* 重新编译clang

* 在本demo的目录下，执行：

  ```bash
  $ clang -cc1 -analyze -analyzer-checker=alpha.core hcd.c
  ```

  就会看到如下的结果：

  ```bash
  hcd.c:10:2: warning: sleep while local_irq_save
          __might_sleep();
          ^~~~~~~~~~~~~~~
  1 warning generated.
  ```

  ​