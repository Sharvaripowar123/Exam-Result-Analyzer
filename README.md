# 📊 Exam Result Analyzer

A C++ tool to fetch and parse Shivaji University B.Tech 
(CSE & Data Science) NEP 2.0 exam results from PDF 
using libcurl and Poppler, and export analysis to CSV format.

## 👩‍💻 Developer
- Sharvari Powar

## 🔧 Features
- Fetches result PDFs using libcurl
- Parses PDF using Poppler
- Exports data to CSV format

## 🛠️ Requirements
- MinGW (C++ compiler)
- libcurl (Windows x64)
- Poppler for Windows

## ⚙️ How to Compile & Run

```bash
cd C:\ExamAnalyzer
g++ -o exam.exe exam_v3.cpp -I"C:\curl\include" -L"C:\curl\lib" -lcurl -lssl -lcrypto -lws2_32
exam.exe
```

## 📁 Project Structure
```
Exam-Result-Analyzer/
├── exam_v3.cpp   # Main C++ source code
└── README.md     # Documentation
```
