
export module mod2;

// clang-format off
// Compile with
// clang++ -std=c++2a -fmodules-ts -fprebuilt-module-path=. -g --precompile mod2.cppm -o mod2.pcm
// clang++ -std=c++2a -fmodules-ts -fprebuilt-module-path=. -g -c mod2.pcm -o mod2.o
// clang-format on

export namespace mod2 {

int getNum2() {
    return 20;
}

} // namespace mod2
