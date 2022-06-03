#include <ui/console.h>

#include <gtest/gtest.h>

using namespace agea;

struct commands_tree_test : public testing::Test
{
};

TEST_F(commands_tree_test, happy_run)
{
    ui::commands_tree ct;

    auto in_h = [](ui::editor_console&, const ui::command_context&) {};

    ct.add({"aa", "bb", "cc"}, in_h);
    ct.add({"aa", "bb", "cd"}, in_h);
    ct.add({"aa", "bb", "dd"}, in_h);

    int depth = 0;
    auto out_h = ct.get({"aa", "bb", "cc"}, depth);
    ASSERT_TRUE(out_h);

    out_h = ct.get({"aa", "bb"}, depth);
    ASSERT_FALSE(out_h);

    std::vector<std::string> hints;
    {
        std::vector<std::string> expected;
        ct.hints({"aa", "bc"}, hints);
        ASSERT_EQ(hints, expected);
    }
    {
        hints.clear();
        std::vector<std::string> expected{"cc", "cd", "dd"};
        ct.hints({"aa", "bb"}, hints);
        ASSERT_EQ(hints, expected);
    }
    {
        hints.clear();
        std::vector<std::string> expected{"cc", "cd"};
        ct.hints({"aa", "bb", "c"}, hints);
        ASSERT_EQ(hints, expected);
    }
    {
        hints.clear();
        std::vector<std::string> expected{"dd"};
        ct.hints({"aa", "bb", "d"}, hints);
        ASSERT_EQ(hints, expected);
    }
}