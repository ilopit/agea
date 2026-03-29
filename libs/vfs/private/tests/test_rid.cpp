#include <gtest/gtest.h>

#include <vfs/rid.h>

#include <unordered_set>

using namespace kryga::vfs;

TEST(Rid, construct_from_vpath)
{
    rid r("data://configs/kryga.acfg");
    EXPECT_EQ(r.mount_point(), "data");
    EXPECT_EQ(r.relative(), "configs/kryga.acfg");
    EXPECT_FALSE(r.empty());
}

TEST(Rid, construct_from_components)
{
    rid r("cache", "shaders/compiled.spv");
    EXPECT_EQ(r.mount_point(), "cache");
    EXPECT_EQ(r.relative(), "shaders/compiled.spv");
}

TEST(Rid, construct_empty_relative)
{
    rid r("tmp", "");
    EXPECT_EQ(r.mount_point(), "tmp");
    EXPECT_EQ(r.relative(), "");
    EXPECT_FALSE(r.empty());
}

TEST(Rid, default_is_empty)
{
    rid r;
    EXPECT_TRUE(r.empty());
}

TEST(Rid, no_scheme_treated_as_mount_point)
{
    rid r("data");
    EXPECT_EQ(r.mount_point(), "data");
    EXPECT_EQ(r.relative(), "");
    EXPECT_FALSE(r.empty());
}

TEST(Rid, scheme_with_path)
{
    rid r("data://configs/kryga.acfg");
    EXPECT_EQ(r.mount_point(), "data");
    EXPECT_EQ(r.relative(), "configs/kryga.acfg");
}

TEST(Rid, str_roundtrip)
{
    rid r("data", "packages/base.apkg");
    EXPECT_EQ(r.str(), "data://packages/base.apkg");
}

TEST(Rid, str_empty_relative)
{
    rid r("tmp", "");
    EXPECT_EQ(r.str(), "tmp://");
}

TEST(Rid, slash_operator)
{
    rid base("data", "packages");
    auto sub = base / "base.apkg" / "class";
    EXPECT_EQ(sub.mount_point(), "data");
    EXPECT_EQ(sub.relative(), "packages/base.apkg/class");
}

TEST(Rid, slash_assign)
{
    rid r("data", "shaders");
    r /= "includes";
    EXPECT_EQ(r.relative(), "shaders/includes");
}

TEST(Rid, slash_from_empty_relative)
{
    rid r("tmp", "");
    auto sub = r / "dir";
    EXPECT_EQ(sub.relative(), "dir");
}

TEST(Rid, equality)
{
    rid a("data", "file.txt");
    rid b("data", "file.txt");
    rid c("data", "other.txt");
    rid d("cache", "file.txt");

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, d);
}

TEST(Rid, hash_works_in_set)
{
    std::unordered_set<rid> s;
    s.insert(rid("data://a.txt"));
    s.insert(rid("data://b.txt"));
    s.insert(rid("data://a.txt"));

    EXPECT_EQ(s.size(), 2);
}
