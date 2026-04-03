#include <gtest/gtest.h>

#include <vfs/memory_backend.h>

using namespace kryga::vfs;

class MemoryBackendTest : public ::testing::Test
{
protected:
    memory_backend be;
};

TEST_F(MemoryBackendTest, name)
{
    EXPECT_EQ(be.name(), "memory");
}

TEST_F(MemoryBackendTest, not_read_only)
{
    EXPECT_FALSE(be.is_read_only());
}

TEST_F(MemoryBackendTest, stat_missing_file)
{
    auto info = be.stat("no_such_file.txt");
    EXPECT_FALSE(info.exists);
}

TEST_F(MemoryBackendTest, add_file_and_stat)
{
    be.add_file_string("hello.txt", "world");

    auto info = be.stat("hello.txt");
    EXPECT_TRUE(info.exists);
    EXPECT_FALSE(info.is_directory);
    EXPECT_EQ(info.size, 5);
}

TEST_F(MemoryBackendTest, read_all)
{
    be.add_file_string("data.bin", "abc");

    std::vector<uint8_t> out;
    EXPECT_TRUE(be.read_all("data.bin", out));
    EXPECT_EQ(out.size(), 3);
    EXPECT_EQ(out[0], 'a');
    EXPECT_EQ(out[1], 'b');
    EXPECT_EQ(out[2], 'c');
}

TEST_F(MemoryBackendTest, read_missing_returns_false)
{
    std::vector<uint8_t> out;
    EXPECT_FALSE(be.read_all("nope.txt", out));
}

TEST_F(MemoryBackendTest, write_all_creates_file)
{
    std::vector<uint8_t> data = {'x', 'y'};
    EXPECT_TRUE(be.write_all("new.bin", data));

    auto info = be.stat("new.bin");
    EXPECT_TRUE(info.exists);
    EXPECT_EQ(info.size, 2);

    std::vector<uint8_t> out;
    EXPECT_TRUE(be.read_all("new.bin", out));
    EXPECT_EQ(out, data);
}

TEST_F(MemoryBackendTest, write_all_overwrites)
{
    be.add_file_string("f.txt", "old");

    std::vector<uint8_t> data = {'n', 'e', 'w'};
    be.write_all("f.txt", data);

    std::vector<uint8_t> out;
    be.read_all("f.txt", out);
    EXPECT_EQ(std::string(out.begin(), out.end()), "new");
}

TEST_F(MemoryBackendTest, create_directories)
{
    EXPECT_TRUE(be.create_directories("some/dir"));

    auto info = be.stat("some/dir");
    EXPECT_TRUE(info.exists);
    EXPECT_TRUE(info.is_directory);
}

TEST_F(MemoryBackendTest, remove_file)
{
    be.add_file_string("kill_me.txt", "bye");
    EXPECT_TRUE(be.remove("kill_me.txt"));
    EXPECT_FALSE(be.stat("kill_me.txt").exists);
}

TEST_F(MemoryBackendTest, remove_prefix)
{
    be.add_file_string("dir/a.txt", "a");
    be.add_file_string("dir/b.txt", "b");
    be.add_file_string("other.txt", "c");

    EXPECT_TRUE(be.remove("dir"));

    EXPECT_FALSE(be.stat("dir/a.txt").exists);
    EXPECT_FALSE(be.stat("dir/b.txt").exists);
    EXPECT_TRUE(be.stat("other.txt").exists);
}

TEST_F(MemoryBackendTest, real_path_returns_nullopt)
{
    EXPECT_FALSE(be.real_path("anything").has_value());
}

TEST_F(MemoryBackendTest, enumerate_recursive)
{
    be.add_file_string("a/one.txt", "1");
    be.add_file_string("a/b/two.txt", "2");
    be.add_file_string("a/b/three.dat", "3");

    std::vector<std::string> found;
    be.enumerate(
        "a",
        [&](std::string_view path, bool)
        {
            found.emplace_back(path);
            return true;
        },
        true);

    EXPECT_EQ(found.size(), 3);
}

TEST_F(MemoryBackendTest, enumerate_non_recursive)
{
    be.add_file_string("top/shallow.txt", "s");
    be.add_file_string("top/deep/nested.txt", "n");

    std::vector<std::string> found;
    be.enumerate(
        "top",
        [&](std::string_view path, bool)
        {
            found.emplace_back(path);
            return true;
        },
        false);

    EXPECT_EQ(found.size(), 1);
}

TEST_F(MemoryBackendTest, enumerate_ext_filter)
{
    be.add_file_string("dir/a.txt", "a");
    be.add_file_string("dir/b.dat", "b");
    be.add_file_string("dir/c.txt", "c");

    std::vector<std::string> found;
    be.enumerate(
        "dir",
        [&](std::string_view path, bool)
        {
            found.emplace_back(path);
            return true;
        },
        true,
        ".txt");

    EXPECT_EQ(found.size(), 2);
}

TEST_F(MemoryBackendTest, enumerate_early_exit)
{
    be.add_file_string("x/1.txt", "1");
    be.add_file_string("x/2.txt", "2");
    be.add_file_string("x/3.txt", "3");

    int count = 0;
    be.enumerate(
        "x",
        [&](std::string_view, bool)
        {
            ++count;
            return false;
        },
        true);

    EXPECT_EQ(count, 1);
}
