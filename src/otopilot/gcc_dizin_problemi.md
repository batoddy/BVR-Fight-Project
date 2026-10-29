Mingw dizininde boşluk var ise:

cmake -S src/otopilot -B src/otopilot/build -G "MinGW Makefiles" `
  "-DCMAKE_CXX_COMPILER=C:/Users/PC_100~3/tools/mingw64/bin/g++.exe" `
  "-DCMAKE_MAKE_PROGRAM=C:/Users/PC_100~3/tools/mingw64/bin/mingw32-make.exe" `
  "-DCMAKE_CXX_FLAGS=-BC:/Users/PC_100~3/tools/mingw64/lib/ -BC:/Users/PC_100~3/tools/mingw64/x86_64-w64-mingw32/lib/"



  cmake --build src/otopilot/build