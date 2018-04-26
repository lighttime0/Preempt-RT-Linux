# Automatic Patch Generation via Learning 论文阅读记录

这个项目的主页是：http://groups.csail.mit.edu/pac/patchgen/#carousel-example-generic

三篇Paper分别是：[fse15-SPR](http://groups.csail.mit.edu/pac/patchgen/papers/spr-fse15.pdf)、[popl16-Prophet](http://groups.csail.mit.edu/pac/patchgen/papers/prophet-popl16.pdf)、[fse17-Gensis](http://rhino.csail.mit.edu/genesis-rep/genesis-full.pdf)

这些paper的第一作者是Fan Long。

Fan Long的这几篇论文是连着的，15年的SPR是第一份工作，做了一个性能比较好的Genrate and

 Validate的查找bug的工具；接着16年用probabilistic model优化candidate patches的搜索空间，提高了运行效率，做出了prophet；接着17年做的Gensis，可以自动抽取transform，并用线性优化的方法平衡了搜索空间的大小和可行性。



## 1 背景：Generate and Validate

Generate and Validate是一类用于自动修复程序缺陷的方法。

这类方法需要有test cases，每个test case就是一个输入和标准输出，这些test cases都能输出正确的话就认为这个程序没问题了。

然后，会有一些transforms，每个transform表示一种程序的转换方式，比如说，增加一个if条件，就是一个transform。这些transforms是由研究人员根据根据代码的规律手工设计的。

下一步，确定出bug的代码位置，对这个位置以及附近的可能位置套上transform和具体的值，生成一份新代码，如果这份新代码能通过所有的test cases，就认为自动修复了一处bug，这一步就叫做Validate。

当然，transform有多个，肯能要修改的代码位置有多个，具体的值和修改方式有很多个，这些构成了庞大的搜索空间，搜索空间里的每份代码都要通过运行所有的test cases来验证。这是这类方法最大的问题——既要搜索空间足够大，能够找到和修复更多的bug；又要搜索空间不能太大，不然所需的运算时间无限长。



## 2 SPR

SPR是作者在15年的第一份工作，后面的工作都是基于它作提高的。

传统的Generate and Validate系统受限于算法不够好，在搜索空间变大后就不行了，作者提出了三个先进的技术做出了SPR，能比以前的系统多修复5倍的程序缺陷，这三个技术是：

* Parameterized Transformation Schemas：也就是把原本的transforms设计成了类似于C++里的模板类，这样就可以生成更多的transforms，生成更大的搜索空间。
* Target Value Search：上面的技术生成了更大的搜索空间，这个技术是用来剪枝的，也就是用值来判断现在的Transformation Schemas有没有可能生成正确的patch，如果不可能，直接跳过，这样可以减少很多的搜索量。
* Condition Synthesis：Target Value Search会根据原本的程序对变量取值做一些限制，Condition Synthesis就根据这些限制，结合原本的if条件，生成patch。



## 3 Prophet

Prophet通过对已有的人工做的patches做一个offline的机器学习，生成一个概率模型，用它来对搜索空间里的candidate patches做一个排序，这样可以提高搜索的速度。

和SPR做一个对比，19个程序缺陷，12个小时内，Prophet可以解决15个，SPR可以解决11个。



## 4 Gensis

Gensis也是使用机器学习的思路，直接用人工做的patches抽取transforms，在论文的例子中，从356个github repo的963个项目中，自动学习并抽取了108个transform（人工设计的话可能只有几个transforms）。这样一来搜索空间进一步加大，为了解决效率问题，会在搜索之前使用线性规划的方法去找到最可能的几个transforms。



## 5 和目前工作的结合

这三篇paper的价值：

* 可以作为文献综述的内容
* 都是用Clang实现的，代码也许可以借鉴一下

这三篇paper的局限：

* 这类方法只适用于有test cases这样的白盒测试，并且程序的运行时间不能太长。Kernel编译时间和测试时间都比较长，test cases貌似也不好找。

这三篇paper可以借鉴的地方：

* 可以用一些概率模型的方法尝试一下RT bug的查找
* transforms可以有多个，而且可以没有关联