<div align="center">

# 🧮 Visual Node Calculator

### Build arithmetic operations visually by connecting interactive nodes.

![C++](https://img.shields.io/badge/C++-17-00599C?style=for-the-badge\&logo=cplusplus\&logoColor=white)
![Dear ImGui](https://img.shields.io/badge/Dear_ImGui-UI-1F6FEB?style=for-the-badge)
![DirectX 11](https://img.shields.io/badge/DirectX-11-107C10?style=for-the-badge\&logo=windows\&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Windows-0078D4?style=for-the-badge\&logo=windows\&logoColor=white)

</div>

---

## 📌 About the Project

**Visual Node Calculator** is a node-based arithmetic application developed in C++ using **Dear ImGui**, **imnodes**, **DirectX 11**, and the **Win32 API**.

Instead of entering an entire mathematical expression as text, users create calculations visually by placing number and operation nodes on a canvas and connecting their input and output pins.

The result is calculated and updated through the created node graph.

---

## ✨ Features

* 🔢 Number input nodes
* ➕ Addition nodes
* ➖ Subtraction nodes
* ✖️ Multiplication nodes
* ➗ Division nodes
* 📤 Result output nodes
* 🔗 Interactive pin and link connections
* ⚡ Real-time calculation updates
* 🖱️ Movable nodes
* 🔍 Canvas zoom and navigation
* 💾 Node graph saving and loading
* 🎨 Custom node-editor interface
* 🧩 Support for multiple connected operations

---

## 🛠️ Technologies

| Technology        | Purpose                                   |
| ----------------- | ----------------------------------------- |
| **C++17**         | Application logic and node calculations   |
| **Dear ImGui**    | Immediate-mode graphical user interface   |
| **imnodes**       | Node-editor system and visual connections |
| **DirectX 11**    | Graphics rendering                        |
| **Win32 API**     | Windows application and input handling    |
| **Visual Studio** | Development, building, and debugging      |

---

## 🚀 Getting Started

### Requirements

Before building the project, make sure you have:

* Windows 10 or Windows 11
* Visual Studio 2022
* **Desktop development with C++** workload
* A DirectX 11-compatible graphics device

### Installation

1. Clone the repository:

```bash
git clone https://github.com/ipekdemirel/Visual-Node-Calculator.git
```

2. Open the project folder.

3. Open `VisualNodeCalculator.slnx` with Visual Studio.

4. Select the appropriate configuration:

```text
Debug | x64
```

5. Build the project using:

```text
Build → Build Solution
```

6. Run the application from Visual Studio.

---

## 🧠 How It Works

1. Create one or more **Number** nodes.
2. Enter the numerical values.
3. Add an arithmetic operation node.
4. Connect number outputs to the operation’s input pins.
5. Connect the operation output to another node or a result node.
6. The application evaluates the connected graph and displays the result.

A calculation such as:

```text
(10 + 5) × 2 = 30
```

can be represented as a connected visual node graph rather than a traditional text expression.

---

<div align="center">

Developed with C++ and Dear ImGui.

</div>





