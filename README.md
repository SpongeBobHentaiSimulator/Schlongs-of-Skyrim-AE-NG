# Schlongs of Skyrim - CommonLibSSE-NG Port

C++ SKSE plugin port for Skyrim Special Edition / Anniversary Edition built with CommonLibSSE-NG and xmake.

## Prerequisites

* Visual Studio 2022 (C++ Desktop Development workload)
* [xmake](https://xmake.io/)
* Git

## Building

1. Clone the repository recursively:
	```bash
	git clone --recursive <repository-url>
	cd <repository-folder>
	```

2. Compile the project:
	```bash
	xmake build
	```

The output `.dll` file will be generated inside the `build/` directory.

## Visual Studio & IntelliSense Setup

To fix syntax highlighting, IntelliSense, and header paths in Visual Studio:

1. Generate the project files:
	```bash
	xmake project -k vsxmake2022
	```

2. Open the generated `.sln` file located inside the `vsxmake2022/` folder.