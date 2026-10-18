#include "mt2mp/net/steam_avatars.h"
#include "mt2mp/net/avatar_png.h"
#include "mt2mp/core/hash.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <utility>
#ifdef NDEBUG
#error Avatar tests require assertions
#endif

using namespace mt2mp::net::avatars;
namespace mt2mp::net { HMODULE steam_module() { assert(false && "Fake API tests must never contact Steam"); return nullptr; } }
struct Fake {
    int avatar = -1, calls = 0, size_calls = 0, rgba_calls = 0;
    uint32_t width = 128, height = 128;
    bool size_ok = true, rgba_ok = true;
    static int large(void* self, uint64_t) { auto& f = *static_cast<Fake*>(self); ++f.calls; return f.avatar; }
    static bool size(void* self, int, uint32_t* width, uint32_t* height) {
        auto& f = *static_cast<Fake*>(self); ++f.size_calls; *width = f.width; *height = f.height; return f.size_ok;
    }
    static bool rgba(void* self, int, unsigned char* out, int bytes) {
        auto& f = *static_cast<Fake*>(self); ++f.rgba_calls;
        assert(bytes == int(f.width*f.height*4));
        for (uint32_t y = 0; y < f.height; ++y) for (uint32_t x = 0; x < f.width; ++x) {
            const auto i = (y*f.width+x)*4; out[i] = static_cast<unsigned char>(x);
            out[i+1] = static_cast<unsigned char>(y); out[i+2] = 127; out[i+3] = static_cast<unsigned char>(x+y);
        }
        return f.rgba_ok;
    }
    Api api() { return {this, this, large, size, rgba}; }
};
void write(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream stream(path, std::ios::binary); stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    stream.close(); assert(stream);
}
std::filesystem::path directory(const std::filesystem::path& root, const char* scenario) {
    const auto path = root/scenario/"data"/"doodesch.multiplayer";
    std::filesystem::create_directories(path); return path;
}
int main(int argc, char** argv) {
    assert(argc == 2);
    const auto root = std::filesystem::absolute(argv[1]);
    std::filesystem::create_directories(root);
    const auto t = std::chrono::steady_clock::time_point{};
    std::string error;
    Fake fake;
    Provider provider(fake.api());
    assert(provider.lookup(1).state == State::Error);
    assert(!provider.configure(root/"other", error));
    error = "previous unrelated error";
    const auto data = directory(root, "shared");
    assert(provider.configure(data, error) && error.empty());
    assert(!provider.configure(data, error));
    assert(provider.set_members({1,2}, error));
    assert(!provider.set_members({1,1}, error));
    assert(!provider.set_members({0}, error));
    std::vector<uint64_t> many; for (uint64_t i = 1; i <= 65; ++i) many.push_back(i);
    assert(!provider.set_members(many, error));
    assert(provider.poll(3,t).state == State::Error && fake.calls == 0);
    assert(provider.poll(1,t).state == State::Pending && fake.calls == 1);
    assert(provider.poll(1,t+std::chrono::milliseconds(999)).state == State::Pending && fake.calls == 1);
    fake.avatar = 7;
    const auto ready = provider.poll(1,t+std::chrono::seconds(1));
    assert(ready.state == State::Ready && ready.width == 128 && ready.height == 128);
    assert(ready.relative_path.size() == 74 && ready.relative_path.substr(0,6) == "cache/");
    assert(ready.relative_path.find("..") == std::string::npos);
    assert(fake.calls == 2 && fake.size_calls == 1 && fake.rgba_calls == 1);
    assert(provider.lookup(1).relative_path == ready.relative_path && fake.calls == 2);
    assert(provider.poll(1,t+std::chrono::seconds(60)).state == State::Ready && fake.calls == 2);
    assert(provider.poll(2,t).relative_path == ready.relative_path && fake.rgba_calls == 1);
    const auto size = std::filesystem::file_size(data/ready.relative_path);
    assert(size > 65536 && size < 67000);
    Provider duplicate(fake.api()); assert(duplicate.configure(data,error)); assert(duplicate.set_members({1},error));
    assert(duplicate.poll(1,t).relative_path == ready.relative_path);
    assert(std::filesystem::file_size(data/ready.relative_path) == size);
    assert(provider.set_members({},error)); assert(provider.lookup(1).state == State::Error);
    fake.avatar = 0; assert(provider.set_members({1},error));
    assert(provider.poll(1,t).state == State::Missing);
    const auto missing_calls = fake.calls;
    assert(provider.poll(1,t+std::chrono::seconds(60)).state == State::Missing && fake.calls == missing_calls);
    Fake timeout; Provider pending(timeout.api());
    assert(pending.configure(directory(root,"pending"),error)); assert(pending.set_members({1},error));
    for (int i = 0; i < 29; ++i) assert(pending.poll(1,t+std::chrono::seconds(i)).state == State::Pending);
    assert(pending.poll(1,t+std::chrono::seconds(29)).state == State::Error && timeout.calls == 30);
    assert(pending.poll(1,t+std::chrono::seconds(100)).state == State::Error && timeout.calls == 30);
    Fake oversized; oversized.avatar = 8; oversized.width = 513;
    Provider invalid(oversized.api()); assert(invalid.configure(directory(root,"invalid"),error)); assert(invalid.set_members({1,2},error));
    assert(invalid.poll(1,t).cause == "Steam avatar dimensions actual=513x128 required=1..512 pixels per axis" && oversized.rgba_calls == 0);
    assert(invalid.poll(2,t).state == State::Error && oversized.size_calls == 1);
    for (const auto shape : std::vector<std::pair<uint32_t,uint32_t>>{{184,184},{184,92},{92,184},{512,512}}) {
        Fake large; large.avatar = 11; large.width = shape.first; large.height = shape.second;
        Provider resized(large.api());
        const auto name = "resized-"+std::to_string(shape.first)+"x"+std::to_string(shape.second);
        const auto large_data = directory(root,name.c_str());
        assert(resized.configure(large_data,error)); assert(resized.set_members({1,2},error));
        const auto image = resized.poll(1,t);
        assert(image.state == State::Ready);
        const auto largest = (std::max)(shape.first,shape.second);
        assert(image.width == shape.first*128/largest && image.height == shape.second*128/largest);
        assert(resized.poll(2,t).relative_path == image.relative_path);
        assert(resized.poll(1,t+std::chrono::seconds(60)).relative_path == image.relative_path);
        assert(large.rgba_calls == 1 && large.size_calls == 1 && large.calls == 2);
        std::ifstream encoded(large_data/image.relative_path,std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(encoded)),std::istreambuf_iterator<char>());
        assert(!encoded.bad() && !bytes.empty()); write(root/(name+".png"),bytes);
    }
    Fake rejected; rejected.avatar = 9; rejected.rgba_ok = false;
    Provider failed(rejected.api()); assert(failed.configure(directory(root,"failed"),error)); assert(failed.set_members({1,2},error));
    assert(failed.poll(1,t).cause == "Steam GetImageRGBA rejected avatar image");
    assert(failed.poll(2,t).state == State::Error && rejected.rgba_calls == 1);
    Fake capacity; capacity.width = capacity.height = 1;
    Provider bounded(capacity.api()); assert(bounded.configure(directory(root,"bounded"),error));
    for (int i = 0; i < 128; ++i) {
        assert(bounded.set_members({},error)); assert(bounded.set_members({1},error)); capacity.avatar = 1000+i;
        assert(bounded.poll(1,t).state == State::Ready);
    }
    assert(bounded.set_members({},error)); assert(bounded.set_members({1},error)); capacity.avatar = 2000;
    assert(bounded.poll(1,t).cause == "Avatar image cache reached 128 handles");
    assert(capacity.rgba_calls == 128);
    std::vector<unsigned char> rgba(128*128*4), png;
    Fake fixture; fixture.rgba(&fixture,7,rgba.data(),static_cast<int>(rgba.size()));
    assert(encode_png(128,128,rgba,png,error)); write(root/"png128.png",png);
    assert(ready.relative_path == "cache/"+mt2mp::sha256(png)+".png");
    const auto expected = png;
    assert(!encode_png(129,128,rgba,png,error) && png.empty());
    assert(!encode_png(0,128,rgba,png,error));
    rgba = {0,0,127,0}; assert(encode_png(1,1,rgba,png,error)); write(root/"png1.png",png);
    const auto corrupt_data = directory(root,"corrupt"); std::filesystem::create_directories(corrupt_data/"cache");
    auto corrupt = expected; corrupt[20] ^= 1; write(corrupt_data/ready.relative_path,corrupt);
    Fake verify; verify.avatar = 10; Provider corrupt_cache(verify.api());
    assert(corrupt_cache.configure(corrupt_data,error)); assert(corrupt_cache.set_members({1},error));
    assert(corrupt_cache.poll(1,t).cause == "Avatar cache existing content differs");
    assert(std::filesystem::file_size(corrupt_data/ready.relative_path) == corrupt.size());
    std::cout << "Avatar provider and PNG assertions passed\n";
}
