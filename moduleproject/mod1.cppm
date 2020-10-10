
import mod2;

#import <cstdio>
#import <string>

export module mod1;

// clang-format off
// Compile with
// clang++ -std=c++2a -fmodules-ts -fprebuilt-module-path=. -g --precompile mod1.cppm -o mod1.pcm
// clang++ -std=c++2a -fmodules-ts -fprebuilt-module-path=. -g -c mod1.pcm -o mod1.o
// clang-format on

namespace mod1 {

export int getNum() {
    return mod2::getNum2();
}

} // namespace mod1
