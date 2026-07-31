#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

char g_install_path[0x504] = {};

void DPLAY_ReceiveMessage(const char* wildcard_path);
void DPLAY_SendMessages();

namespace {
void WriteFile(const std::filesystem::path& path)
{
    std::ofstream stream(path);
    stream << "temporary";
}

void TestPostBagCleanup()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("lego-loco-postbag-" + std::to_string(getpid()));
    std::filesystem::remove_all(root);
    const std::filesystem::path directories[] = {
        root / "PostBag" / "Sort" / "In",
        root / "PostBag" / "Sort" / "Out",
        root / "PostBag" / "Att_Out",
        root / "PostBag" / "Att_In",
    };
    for (const std::filesystem::path& directory : directories) {
        std::filesystem::create_directories(directory / "nested");
        WriteFile(directory / "message.crd");
        WriteFile(directory / ".keep");
    }
    const std::string install = root.string() + "/";
    assert(install.size() < sizeof(g_install_path));
    std::copy(install.begin(), install.end(), g_install_path);
    g_install_path[install.size()] = '\0';

    DPLAY_SendMessages();
    for (const std::filesystem::path& directory : directories) {
        assert(!std::filesystem::exists(directory / "message.crd"));
        assert(std::filesystem::exists(directory / ".keep"));
        assert(std::filesystem::is_directory(directory / "nested"));
    }

    const std::filesystem::path direct = root / "direct";
    std::filesystem::create_directories(direct);
    WriteFile(direct / "payload.dat");
    std::string wildcard = direct.string() + "\\*.*";
    DPLAY_ReceiveMessage(wildcard.c_str());
    assert(!std::filesystem::exists(direct / "payload.dat"));

    std::filesystem::remove_all(root);
}
}  // namespace

int main()
{
    TestPostBagCleanup();
    std::cout << "PASS: PostBag transient files are cleaned without deleting dot files or directories\n";
    return 0;
}
