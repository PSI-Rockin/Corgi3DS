#include <fstream>
#include "../core/emulator.hpp"
#include "../core/common/exceptions.hpp"

using namespace std;

ifstream::pos_type filesize(const char* filename)
{
    ifstream in(filename, ifstream::ate | ifstream::binary);
    return in.tellg();
}

int main(int argc, char** argv)
{
    if (argc < 2)
        return 1;

    ifstream test_elf_file(argv[1]);
    if (!test_elf_file.is_open())
        return 1;

    printf("Opened %s successfully, loading into memory\n", argv[1]);

    uint64_t size = filesize(argv[1]);
    uint8_t* mem = new uint8_t[size];

    test_elf_file.read((char*)mem, size);
    test_elf_file.close();

    Emulator e;

    e.load_and_run_elf(mem, size);

    delete[] mem;

    return 0;
}
