#include <gtest/gtest.h>

#include <vfs/physical_backend.h>

#include <filesystem>
#include <fstream>

using namespace kryga::vfs;

class PhysicalBackendTest : public ::testing::Test
{
protected:
    std::filesystem::path m_root;
    std::unique_ptr<physical_backend> be;

    void
    SetUp() override
    {
        m_root = std::filesystem::temp_directory_path() / "kryga_vfs_test";
        std::filesystem::remove_all(m_root);
        std::filesystem::create_directories(m_root);
        be = std::make_unique<physical_backend>(m_root);
    }

    void
    TearDown() override
    {
        std::filesystem::remove_all(m_root);
    }

    void
    write_file(const std::string& rel, const std::string& content)
    {
        auto full = m_root / rel;
        std::filesystem::create_directories(full.parent_path());
        std::ofstream f(full, std::ios::binary);
        f << content;
    }
};

TEST_F(PhysicalBackendTest, name)
{
    EXPECT_EQ(be->name(), "physical");
}

TEST_F(PhysicalBackendTest, not_read_only)
{
    EXPECT_FALSE(be->is_read_only());
}

TEST_F(PhysicalBackendTest, stat_missing)
{
    auto info = be->stat("no_such.txt");
    EXPECT_FALSE(info.exists);
}

TEST_F(PhysicalBackendTest, stat_file)
{
    write_file("test.txt", "hello");

    auto info = be->stat("test.txt");
    EXPECT_TRUE(info.exists);
    EXPECT_FALSE(info.is_directory);
    EXPECT_EQ(info.size, 5);
}

TEST_F(PhysicalBackendTest, stat_directory)
{
    std::filesystem::create_directories(m_root / "subdir");

    auto info = be->stat("subdir");
    EXPECT_TRUE(info.exists);
    EXPECT_TRUE(info.is_directory);
}

TEST_F(PhysicalBackendTest, read_all)
{
    write_file("data.bin", "abc");

    std::vector<uint8_t> out;
    EXPECT_TRUE(be->read_all("data.bin", out));
    EXPECT_EQ(out.size(), 3);
    EXPECT_EQ(std::string(out.begin(), out.end()), "abc");
}

TEST_F(PhysicalBackendTest, read_missing)
{
    std::vector<uint8_t> out;
    EXPECT_FALSE(be->read_all("nope.bin", out));
}

TEST_F(PhysicalBackendTest, write_all)
{
    std::vector<uint8_t> data = {'x', 'y', 'z'};
    EXPECT_TRUE(be->write_all("new_file.bin", data));

    std::vector<uint8_t> out;
    EXPECT_TRUE(be->read_all("new_file.bin", out));
    EXPECT_EQ(out, data);
}

TEST_F(PhysicalBackendTest, write_creates_parent_dirs)
{
    std::vector<uint8_t> data = {'a'};
    EXPECT_TRUE(be->write_all("deep/nested/file.bin", data));
    EXPECT_TRUE(be->stat("deep/nested/file.bin").exists);
}

TEST_F(PhysicalBackendTest, create_directories)
{
    EXPECT_TRUE(be->create_directories("a/b/c"));
    EXPECT_TRUE(std::filesystem::is_directory(m_root / "a/b/c"));
}

TEST_F(PhysicalBackendTest, remove_file)
{
    write_file("del.txt", "gone");
    EXPECT_TRUE(be->remove("del.txt"));
    EXPECT_FALSE(be->stat("del.txt").exists);
}

TEST_F(PhysicalBackendTest, remove_directory_recursive)
{
    write_file("dir/a.txt", "a");
    write_file("dir/sub/b.txt", "b");

    EXPECT_TRUE(be->remove("dir"));
    EXPECT_FALSE(std::filesystem::exists(m_root / "dir"));
}

TEST_F(PhysicalBackendTest, real_path)
{
    write_file("f.txt", "x");

    auto rp = be->real_path("f.txt");
    ASSERT_TRUE(rp.has_value());
    EXPECT_EQ(rp.value(), m_root / "f.txt");
}

TEST_F(PhysicalBackendTest, real_path_empty_returns_root)
{
    auto rp = be->real_path("");
    ASSERT_TRUE(rp.has_value());
    EXPECT_EQ(rp.value(), m_root);
}

TEST_F(PhysicalBackendTest, enumerate_recursive)
{
    write_file("e/one.txt", "1");
    write_file("e/sub/two.txt", "2");

    std::vector<std::string> found;
    be->enumerate(
        "e",
        [&](std::string_view path, bool)
        {
            found.emplace_back(path);
            return true;
        },
        true);

    EXPECT_EQ(found.size(), 2);
}

TEST_F(PhysicalBackendTest, enumerate_non_recursive)
{
    write_file("flat/a.txt", "a");
    write_file("flat/deep/b.txt", "b");

    std::vector<std::string> found;
    be->enumerate(
        "flat",
        [&](std::string_view path, bool)
        {
            found.emplace_back(path);
            return true;
        },
        false);

    EXPECT_EQ(found.size(), 1);
}

TEST_F(PhysicalBackendTest, enumerate_ext_filter)
{
    write_file("filt/a.txt", "a");
    write_file("filt/b.dat", "b");

    std::vector<std::string> found;
    be->enumerate(
        "filt",
        [&](std::string_view path, bool)
        {
            found.emplace_back(path);
            return true;
        },
        true,
        ".txt");

    EXPECT_EQ(found.size(), 1);
}
