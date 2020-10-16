
// Includes to slow things down a bit
#include <string>
#include <vector>
#include <memory>

using namespace std;

// This is a nonsense function
vector<shared_ptr<string>> randomFunction() {
    vector<shared_ptr<string>> ret;
    
    for (size_t i = 0; i < 10; ++i) {
        ret.push_back(make_shared<string>("hej"));
    }   
    return ret;
}
