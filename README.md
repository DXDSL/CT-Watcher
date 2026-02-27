| 支持的目标芯片 | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ---------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# _示例项目_

(请查看上级 'examples' 目录中的 README.md 文件，以了解有关示例的更多信息。)

这是最简单的可构建示例。该示例由命令 `idf.py create-project`
该命令将项目复制到用户指定的路径并设置其名称。有关更多信息，请查看[文档页面](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#start-a-new-project)



## 如何使用示例
我们建议用户使用该示例作为新项目的模板。
推荐的方法是按照[文档页面](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#start-a-new-project)上的说明进行操作。

## 示例文件夹内容

项目 **sample_project** 包含一个 C 语言源文件 [main.c](main/main.c)。该文件位于文件夹 [main](main) 中。

ESP-IDF 项目使用 CMake 构建。项目构建配置包含在 `CMakeLists.txt`
文件中，这些文件提供一组指令和说明，描述项目的源文件和目标
(可执行文件、库或两者)。

以下是项目文件夹中剩余文件的简短说明。

```
├── CMakeLists.txt
├── main
│   ├── CMakeLists.txt
│   └── main.c
└── README.md                  这是你当前正在阅读的文件
```
此外，示例项目还包含 Makefile 和 component.mk 文件，用于旧的基于 Make 的构建系统。
在使用 CMake 和 idf.py 构建时不使用或不需要它们。
