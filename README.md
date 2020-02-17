# QQDuelBot
A bot to make codeforces duels in QQ groups

### Acknowledgement

本项目使用了酷q、nlohmann/json、libcurl等项目，在此一并致谢。传播使用时需遵循对应的license。

本项目受到了 https://github.com/cheran-senthil/TLE 的启发，在此致谢。

目前本项目实现比较dirty，欢迎提出改进。

### Compiling

本项目目前只包含一个文件，你需要使用CoolQ SDK工具链进行构建。以下介绍一种较为简单的构建方法。

在一个空文件夹中运行以下命令：

```bash
git clone https://github.com/cqmoe/cqcppsdk-template.git awesome-bot
cd awesome-bot
git submodule update --init
```

将src文件夹下的demo.cpp使用本项目的代码文件替换，并在src文件夹下放置`nlohamnn\json`项目的文件。

然后文件树应该是这样：

```
awesome-bot\
	...
	src\
		demo.cpp
		nlohmann\
			json.hpp
			...
```

比较复杂的依赖是libcurl。目前我的编译方法是使用Visual Studio 2019，在`CMakeLists.txt`中添加

```CMake
add_definitions("-DCURL_STATICLIB")
set(CURL_STATICLIB true)
include_directories(".../curl-7.68.0/builds/libcurl-vc-x86-release-static-ipv6-sspi-winssl/include")
```

并在代码开头手动添加宏并link动态链接库。

### Deploy

由于懒，本项目的数据均直接使用`json`储存。你需要修改`demo.cpp`中的`path`变量为一个能够写入的空文件夹，并在该文件夹下放置`problemset.problems`文件。该文件需要从 https://codeforces.com/api/problemset.problems 下载（文件体积较大）。

你还需要修改代码中的`admin_list`和`enabled_groups`变量，它们分别为管理员列表和开启的群号列表。

运行编译好的应用可以参见 https://cqcppsdk.cqp.moe/guide/getting-started.html。