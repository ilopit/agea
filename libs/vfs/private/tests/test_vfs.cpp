#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vfs/vfs.h>

using namespace kryga;
using namespace kryga::vfs;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgReferee;

namespace
{

class mock_backend : public backend
{
public:
    MOCK_METHOD(std::string_view, name, (), (const, override));
    MOCK_METHOD(file_info, stat, (std::string_view), (const, override));
    MOCK_METHOD(bool, read_all, (std::string_view, std::vector<uint8_t>&), (const, override));
    MOCK_METHOD(bool, write_all, (std::string_view, std::span<const uint8_t>), (override));
    MOCK_METHOD(bool, create_directories, (std::string_view), (override));
    MOCK_METHOD(bool, remove, (std::string_view), (override));
    MOCK_METHOD(bool,
                enumerate,
                (std::string_view, const enumerate_cb&, bool, std::string_view),
                (const, override));
    MOCK_METHOD(std::optional<std::filesystem::path>,
                real_path,
                (std::string_view),
                (const, override));

    explicit mock_backend(bool read_only = false)
        : backend(read_only)
    {
        ON_CALL(*this, name()).WillByDefault(Return("mock"));
    }
};

static file_info
make_exists(uint64_t size = 0)
{
    file_info fi;
    fi.exists = true;
    fi.size = size;
    return fi;
}

static file_info
make_missing()
{
    return {};
}

static file_info
make_dir()
{
    file_info fi;
    fi.exists = true;
    fi.is_directory = true;
    return fi;
}

class VfsTest : public ::testing::Test
{
protected:
    virtual_file_system vfs;

    mock_backend*
    mount_mock(const std::string& mp, int priority = 0, bool read_only = false)
    {
        auto be = std::make_unique<::testing::NiceMock<mock_backend>>(read_only);
        auto* ptr = be.get();
        vfs.mount(mp, std::move(be), priority);
        return ptr;
    }
};
}  // namespace

// --- Read routing ---

TEST_F(VfsTest, read_bytes_delegates_to_backend)
{
    auto* be = mount_mock("data");

    std::vector<uint8_t> expected = {1, 2, 3};
    EXPECT_CALL(*be, stat("file.bin")).WillOnce(Return(make_exists(3)));
    EXPECT_CALL(*be, read_all("file.bin", _))
        .WillOnce(DoAll(SetArgReferee<1>(expected), Return(true)));

    std::vector<uint8_t> out;
    EXPECT_TRUE(vfs.read_bytes(rid("data://file.bin"), out));
    EXPECT_EQ(out, expected);
}

TEST_F(VfsTest, read_string_delegates_to_backend)
{
    auto* be = mount_mock("data");

    std::vector<uint8_t> bytes = {'h', 'i'};
    EXPECT_CALL(*be, stat("msg.txt")).WillOnce(Return(make_exists(2)));
    EXPECT_CALL(*be, read_all("msg.txt", _)).WillOnce(DoAll(SetArgReferee<1>(bytes), Return(true)));

    std::string out;
    EXPECT_TRUE(vfs.read_string(rid("data://msg.txt"), out));
    EXPECT_EQ(out, "hi");
}

TEST_F(VfsTest, read_missing_file_returns_false)
{
    auto* be = mount_mock("data");
    EXPECT_CALL(*be, stat("nope.txt")).WillOnce(Return(make_missing()));

    std::vector<uint8_t> out;
    EXPECT_FALSE(vfs.read_bytes(rid("data://nope.txt"), out));
}

TEST_F(VfsTest, read_unmounted_mount_returns_false)
{
    std::string out;
    EXPECT_FALSE(vfs.read_string(rid("nowhere://file.txt"), out));
}

// --- Write routing ---

TEST_F(VfsTest, write_bytes_delegates_to_backend)
{
    auto* be = mount_mock("data");

    EXPECT_CALL(*be, write_all("out.bin", _)).WillOnce(Return(true));

    std::vector<uint8_t> data = {0xFF};
    EXPECT_TRUE(vfs.write_bytes(rid("data://out.bin"), data));
}

TEST_F(VfsTest, write_string_delegates_to_backend)
{
    auto* be = mount_mock("data");

    EXPECT_CALL(*be, write_all("out.txt", _)).WillOnce(Return(true));

    EXPECT_TRUE(vfs.write_string(rid("data://out.txt"), "hello"));
}

TEST_F(VfsTest, write_no_backend_returns_false)
{
    EXPECT_FALSE(vfs.write_string(rid("nowhere://file.txt"), "data"));
}

TEST_F(VfsTest, write_skips_read_only_backend)
{
    auto* be = mount_mock("data", 0, true);
    EXPECT_CALL(*be, write_all(_, _)).Times(0);

    EXPECT_FALSE(vfs.write_string(rid("data://file.txt"), "x"));
}

// --- exists / stat ---

TEST_F(VfsTest, exists_true)
{
    auto* be = mount_mock("data");
    EXPECT_CALL(*be, stat("file.cfg")).WillOnce(Return(make_exists()));

    EXPECT_TRUE(vfs.exists(rid("data://file.cfg")));
}

TEST_F(VfsTest, exists_false)
{
    auto* be = mount_mock("data");
    EXPECT_CALL(*be, stat("missing.cfg")).WillOnce(Return(make_missing()));

    EXPECT_FALSE(vfs.exists(rid("data://missing.cfg")));
}

TEST_F(VfsTest, stat_returns_file_info)
{
    auto* be = mount_mock("data");
    EXPECT_CALL(*be, stat("info.txt")).WillOnce(Return(make_exists(42)));

    auto info = vfs.stat(rid("data://info.txt"));
    EXPECT_TRUE(info.exists);
    EXPECT_EQ(info.size, 42);
}

TEST_F(VfsTest, stat_missing)
{
    auto* be = mount_mock("data");
    EXPECT_CALL(*be, stat("nope")).WillOnce(Return(make_missing()));

    auto info = vfs.stat(rid("data://nope"));
    EXPECT_FALSE(info.exists);
}

// --- Directory operations ---

TEST_F(VfsTest, create_directories_delegates)
{
    auto* be = mount_mock("data");
    EXPECT_CALL(*be, create_directories("some/path")).WillOnce(Return(true));

    EXPECT_TRUE(vfs.create_directories(rid("data://some/path")));
}

TEST_F(VfsTest, remove_delegates)
{
    auto* be = mount_mock("data");
    EXPECT_CALL(*be, remove("del.txt")).WillOnce(Return(true));

    EXPECT_TRUE(vfs.remove(rid("data://del.txt")));
}

// --- Priority / overlay ---

TEST_F(VfsTest, higher_priority_wins_read)
{
    auto* lo = mount_mock("data", 0);
    auto* hi = mount_mock("data", 10);

    EXPECT_CALL(*hi, stat("f.txt")).WillOnce(Return(make_exists(8)));
    EXPECT_CALL(*lo, stat(_)).Times(0);

    std::vector<uint8_t> override_data = {'H', 'I'};
    EXPECT_CALL(*hi, read_all("f.txt", _))
        .WillOnce(DoAll(SetArgReferee<1>(override_data), Return(true)));

    std::vector<uint8_t> out;
    EXPECT_TRUE(vfs.read_bytes(rid("data://f.txt"), out));
    EXPECT_EQ(out, override_data);
}

TEST_F(VfsTest, fallback_to_lower_priority)
{
    auto* lo = mount_mock("data", 0);
    auto* hi = mount_mock("data", 10);

    EXPECT_CALL(*hi, stat("only_base.txt")).WillOnce(Return(make_missing()));
    EXPECT_CALL(*lo, stat("only_base.txt")).WillOnce(Return(make_exists(4)));

    std::vector<uint8_t> base_data = {'b', 'a', 's', 'e'};
    EXPECT_CALL(*lo, read_all("only_base.txt", _))
        .WillOnce(DoAll(SetArgReferee<1>(base_data), Return(true)));

    std::vector<uint8_t> out;
    EXPECT_TRUE(vfs.read_bytes(rid("data://only_base.txt"), out));
    EXPECT_EQ(out, base_data);
}

TEST_F(VfsTest, exists_checks_all_priorities)
{
    auto* lo = mount_mock("data", 0);
    auto* hi = mount_mock("data", 10);

    EXPECT_CALL(*hi, stat("low.txt")).WillOnce(Return(make_missing()));
    EXPECT_CALL(*lo, stat("low.txt")).WillOnce(Return(make_exists()));

    EXPECT_TRUE(vfs.exists(rid("data://low.txt")));
}

// --- Write target ---

TEST_F(VfsTest, write_goes_to_highest_priority_writable)
{
    auto* lo = mount_mock("data", 0);
    auto* hi = mount_mock("data", 10);

    EXPECT_CALL(*hi, write_all("new.txt", _)).WillOnce(Return(true));
    EXPECT_CALL(*lo, write_all(_, _)).Times(0);

    EXPECT_TRUE(vfs.write_string(rid("data://new.txt"), "hello"));
}

TEST_F(VfsTest, set_write_target_overrides_priority)
{
    auto* lo = mount_mock("data", 0);
    auto* hi = mount_mock("data", 10);

    vfs.set_write_target("data", lo);

    EXPECT_CALL(*lo, write_all("targeted.txt", _)).WillOnce(Return(true));
    EXPECT_CALL(*hi, write_all(_, _)).Times(0);

    EXPECT_TRUE(vfs.write_string(rid("data://targeted.txt"), "here"));
}

// --- Unmount ---

TEST_F(VfsTest, unmount_removes_backend)
{
    auto* be = mount_mock("data");
    EXPECT_CALL(*be, stat("file.txt")).WillOnce(Return(make_exists()));

    EXPECT_TRUE(vfs.exists(rid("data://file.txt")));

    vfs.unmount("data", be);

    EXPECT_FALSE(vfs.exists(rid("data://file.txt")));
}

// --- Enumerate ---

TEST_F(VfsTest, enumerate_delegates_to_all_backends)
{
    auto* lo = mount_mock("data", 0);
    auto* hi = mount_mock("data", 10);

    EXPECT_CALL(*hi, enumerate("pkg", _, true, std::string_view{})).WillOnce(Return(true));
    EXPECT_CALL(*lo, enumerate("pkg", _, true, std::string_view{})).WillOnce(Return(true));

    vfs.enumerate(rid("data://pkg"), [](std::string_view, bool) { return true; }, true);
}

TEST_F(VfsTest, enumerate_stops_if_backend_returns_false)
{
    auto* lo = mount_mock("data", 0);
    auto* hi = mount_mock("data", 10);

    EXPECT_CALL(*hi, enumerate("pkg", _, true, std::string_view{})).WillOnce(Return(false));
    EXPECT_CALL(*lo, enumerate(_, _, _, _)).Times(0);

    auto result =
        vfs.enumerate(rid("data://pkg"), [](std::string_view, bool) { return true; }, true);
    EXPECT_FALSE(result);
}

// --- Invalid paths ---

TEST_F(VfsTest, unmounted_mount_returns_false)
{
    mount_mock("data");

    std::string out;
    EXPECT_FALSE(vfs.read_string(rid("nonexistent"), out));
    EXPECT_FALSE(vfs.write_string(rid("nonexistent"), "x"));
    EXPECT_FALSE(vfs.exists(rid("nonexistent")));
}

// --- real_path ---

TEST_F(VfsTest, real_path_delegates_to_backend)
{
    auto* be = mount_mock("data");
    auto expected = std::filesystem::path("C:/some/path");
    EXPECT_CALL(*be, real_path("file.txt")).WillOnce(Return(expected));

    auto rp = vfs.real_path(rid("data://file.txt"));
    ASSERT_TRUE(rp.has_value());
    EXPECT_EQ(rp.value(), expected);
}

TEST_F(VfsTest, real_path_returns_nullopt_when_backend_does)
{
    auto* be = mount_mock("data");
    EXPECT_CALL(*be, real_path("file.txt")).WillOnce(Return(std::nullopt));

    auto rp = vfs.real_path(rid("data://file.txt"));
    EXPECT_FALSE(rp.has_value());
}

// --- Multiple mount points ---

TEST_F(VfsTest, separate_mount_points_are_isolated)
{
    auto* data_be = mount_mock("data");
    auto* cache_be = mount_mock("cache");

    EXPECT_CALL(*data_be, stat("a.txt")).WillOnce(Return(make_exists()));
    EXPECT_CALL(*cache_be, stat("a.txt")).WillOnce(Return(make_missing()));

    EXPECT_TRUE(vfs.exists(rid("data://a.txt")));
    EXPECT_FALSE(vfs.exists(rid("cache://a.txt")));
}
